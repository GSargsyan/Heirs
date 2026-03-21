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

void play_single_game(int game_id, int seed, ofstream& csv_out) {
    srand(seed + (int)time(NULL));
    Board b;
    EngineV1 engine_white;
    EngineV1 engine_black;
    
    int max_moves = 150;
    int random_ply = 6 + (rand() % 5); 
    int moves = 0;
    
    vector<vector<int>> game_states;
    vector<float> game_evals;
    
    while (!b.is_game_over() && moves < max_moves) {
        if (b.is_draw(1)) break;

        Color side = b.side_to_move();
        Move m;
        MoveList valid_moves;
        b.generate_moves(valid_moves);
        if (valid_moves.count == 0) break;
        
        if (moves < random_ply) {
            m = valid_moves.moves[rand() % valid_moves.count];
        } else {
            if (side == WHITE) m = engine_white.search(b, -1.0, 4);
            else m = engine_black.search(b, -1.0, 4);
            
            vector<int> state_features;
            extract_board_state(b, state_features);
            game_states.push_back(state_features);
            game_evals.push_back(0.0f);
        }
        
        if (m.from == 0 && m.to == 0) break;
        b.make_move(m);
        moves++;
    }
    
    float result = 0.5f; 
    if (b.is_game_over()) {
        Color winner = (Color)(b.side_to_move() ^ 1);
        result = (winner == WHITE) ? 1.0f : 0.0f;
    }
    
    for (size_t i = 0; i < game_states.size(); ++i) {
        csv_out << result << "," << game_evals[i];
        for (int f : game_states[i]) csv_out << "," << f;
        csv_out << "\n";
    }
}

int main(int argc, char** argv) {
    if (argc < 3) {
        cerr << "Usage: " << argv[0] << " <games_to_play> <output_filename>" << endl;
        return 1;
    }
    
    int games_to_play = stoi(argv[1]);
    string output_filename = argv[2];
    
    cout << "Worker starting: " << games_to_play << " games -> " << output_filename << endl;
    
    ofstream csv_out(output_filename, ios::app);
    if (!csv_out.is_open()) {
        cerr << "Failed to open " << output_filename << endl;
        return 1;
    }
    
    for (int i = 0; i < games_to_play; ++i) {
        play_single_game(i, i, csv_out);
        if ((i + 1) % 10 == 0) {
            cout << "Worker finished " << (i + 1) << "/" << games_to_play << " games." << endl;
        }
    }
    
    return 0;
}
