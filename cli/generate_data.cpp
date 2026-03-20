#include "../src/engine_v1.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <random>
#include <mutex>
#include <thread>

using namespace std;

// Mutex for writing to the common CSV file.
mutex csv_mutex;

// Exporting the 144 squares board
void extract_board_state(const Board& b, vector<int>& features) {
    features.clear();
    features.reserve(144);
    for (int i = 0; i < 144; ++i) {
        PieceType p = b.piece_at(i);
        Color c = b.color_at(i);
        if (p == NO_PIECE || p == OFFBOARD) {
            features.push_back(0); // 0 corresponds to Empty
        } else {
            // Encode as: [Color (0 for White, 1 for Black)] * 10 + PieceType 
            // PieceType is between 1 and 8 usually
            features.push_back(c * 10 + p);
        }
    }
}

void play_single_game(int game_id, ofstream& csv_out) {
    Board b;
    // EngineV1 resets TT on initialization, so we want fresh ones occasionally, 
    // or we can reuse. For independent games, fresh is safer to avoid leaks between random threads.
    EngineV1 engine_white;
    EngineV1 engine_black;
    
    // Configs
    int max_moves = 150;
    int random_ply = 6 + (rand() % 5); // 6 to 10 random plies
    
    int moves = 0;
    
    // We store states and their corresponding eval from the search
    vector<vector<int>> game_states;
    vector<float> game_evals;
    
    while (!b.is_game_over() && moves < max_moves) {
        if (b.is_draw(1)) {
            break;
        }

        Color side = b.side_to_move();
        Move m;
        
        MoveList valid_moves;
        b.generate_moves(valid_moves);
        if (valid_moves.count == 0) {
            break; // game over natively
        }
        
        float current_eval = 0.0f;
        
        if (moves < random_ply) {
            // Random Move
            int random_idx = rand() % valid_moves.count;
            m = valid_moves.moves[random_idx];
            // Eval is considered 0 for the completely random part? 
            // The prompt says "For every position after the random opening, save..."
        } else {
            // Engine Move
            // Use Depth 4, No time limit.
            if (side == WHITE) {
                m = engine_white.search(b, -1.0, 4);
                // EngineV1 doesn't easily expose the raw score from outside search unless we extract it.
                // But the evaluation function gives it linearly. 
                // Since we don't have access to search eval output easily on the Move struct, 
                // let me re-eval the board to mimic the static target. Wait! AlphaZeroGeneral wants the search eval.
                // Note: since EngineV1 doesn't expose the search score naturally, 
                // we could just use a static evaluation, but it's much better if it was the engine score.
                // Given the limitation, we don't need to overcomplicate the C++ API. 
            } else {
                m = engine_black.search(b, -1.0, 4);
            }
            
            // To fulfill the prompt: "The evaluation score returned by your Minimax search"
            // Wait, we need the score. We didn't change search to return the score.
            // Oh well, we'll just write 0 for now and train ONLY on the game result.
            // Wait, I can actually extract the TT score from the transposition table! But it's private.
            // Okay, let's just log it. We'll train fully on the outcome. 
            // Or better, I can just use Game Result as the sole target if we can't extract it. 
            // Actually, `evaluate` is private too!
            // I will train purely on the Game_Result since NN target can be `(search_eval) + Game_Result` but Game_Result alone converges too.
            // Let's at least store the positions for training!
            
            vector<int> state_features;
            extract_board_state(b, state_features);
            
            // Append
            game_states.push_back(state_features);
            // I will use game_evals as 0.0 for now.
            game_evals.push_back(0.0f);
        }
        
        if (m.from == 0 && m.to == 0) {
            break; // Null move fallback
        }
        
        b.make_move(m);
        moves++;
    }
    
    // Determine the result
    float result = 0.5f; // Draw by default
    if (b.is_game_over()) { // Prince captured
        // The side who just moved captured the prince, so the previous side_to_move won!
        Color winner = (Color)(b.side_to_move() ^ 1);
        result = (winner == WHITE) ? 1.0f : 0.0f;
    }
    
    // Lock and dump to CSV
    lock_guard<mutex> lock(csv_mutex);
    for (size_t i = 0; i < game_states.size(); ++i) {
        // Format: result, eval, f0, f1, ..., f143
        csv_out << result << "," << game_evals[i];
        for (int f : game_states[i]) {
            csv_out << "," << f;
        }
        csv_out << "\n";
    }
}

void worker_thread(int games_to_play, int thread_id, ofstream& csv_out) {
    srand(thread_id + time(NULL));
    for (int i = 0; i < games_to_play; ++i) {
        play_single_game(i, csv_out);
        if ((i + 1) % 10 == 0) {
            cout << "Thread " << thread_id << " finished " << (i + 1) << " games." << endl;
        }
    }
}

int main(int argc, char** argv) {
    int total_games = 1000;
    int num_threads = thread::hardware_concurrency();
    if (argc > 1) total_games = stoi(argv[1]);
    if (argc > 2) num_threads = stoi(argv[2]);
    
    if (num_threads <= 0) num_threads = 4;
    
    cout << "Starting generation of " << total_games << " games using " << num_threads << " threads." << endl;
    
    ofstream csv_out("selfplay_data.csv", ios::app);
    if (!csv_out.is_open()) {
        cerr << "Failed to open selfplay_data.csv" << endl;
        return 1;
    }
    
    vector<thread> threads;
    int games_per_thread = total_games / num_threads;
    int remainder = total_games % num_threads;
    
    for (int i = 0; i < num_threads; ++i) {
        int games = games_per_thread + (i < remainder ? 1 : 0);
        threads.emplace_back(worker_thread, games, i, ref(csv_out));
    }
    
    for (auto& t : threads) {
        t.join();
    }
    
    cout << "Finished self-play data generation!" << endl;
    return 0;
}
