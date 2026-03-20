#include "../src/engine_v3.h"
#include <iostream>
#include <fstream>
#include <string>
#include <random>
#include <cmath>
#include <algorithm>
#include <future>
#include <vector>
#include <mutex>

#ifdef _WIN32
#include <windows.h>
#endif

const double TIME_TO_THINK = 0.2; // Think time per move (short for fast tuning)
const int MATCHES_PER_ITER = 20; // 20 matches total
const int MAX_PLY_LIMIT = 500;   // Threshold to end game
const double ACCEPT_THRESHOLD = 0.55; // Threshold win rate to accept new values (e.g. 0.55 = 55% score required)

bool load_values(const std::string& filename, int values[9]) {
    std::ifstream in(filename);
    if (!in) return false;
    for (int i = 0; i < 9; ++i) {
        if (!(in >> values[i])) return false;
    }
    return true;
}

void save_values(const std::string& filename, const int values[9]) {
    std::ofstream out(filename);
    for (int i = 0; i < 9; ++i) {
        out << values[i] << (i == 8 ? "" : " ");
    }
    out << std::endl;
}

void randomize_values(const int base[9], int tweaked[9]) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::normal_distribution<double> d(0.0, 15.0); 
    
    for (int i = 0; i < 9; ++i) tweaked[i] = base[i];
    
    // Only tweak non-0 indices, and don't touch Baby (benchmark) or Prince
    for (int i = 1; i < 9; ++i) {
        if (i == 1 || i == 2) continue; // Skip Baby (1) and Prince (2)
        int delta = static_cast<int>(std::round(d(gen)));
        tweaked[i] = std::max(10, base[i] + delta);
    }
}

// Returns pair<int, int> representing <winner, moves_played>
// Winner: 1 if p1 wins, -1 if p2 wins, 0 if draw
std::pair<int, int> play_game(EngineV3& p1, EngineV3& p2) {
    Board b;
    int moves = 0;
    while (!b.is_game_over() && moves < MAX_PLY_LIMIT) {
        if (b.is_draw()) break;
        
        Color side = b.side_to_move();
        Move m;
        if (side == WHITE) m = p1.search(b, TIME_TO_THINK);
        else m = p2.search(b, TIME_TO_THINK);
        
        if (m.from == 0 && m.to == 0) break; // No moves / Resign
        b.make_move(m);
        moves++;
    }
    
    // Material evaluation to decide winner
    int w_score = 0;
    int b_score = 0;
    int std_val[] = {0, 100, 20000, 950, 180, 350, 325, 600, 280};
    
    for (int i = 0; i < b.get_white_piece_count(); ++i) {
        w_score += std_val[b.piece_at(b.get_white_pieces()[i])];
    }
    for (int i = 0; i < b.get_black_piece_count(); ++i) {
        b_score += std_val[b.piece_at(b.get_black_pieces()[i])];
    }
    
    if (w_score > b_score + 100) return {1, moves};
    if (b_score > w_score + 100) return {-1, moves};
    return {0, moves}; // Draw
}

int main() {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif
    std::string val_file = "../piece_values.txt"; // Search root directory first
    int iteration = 1;

    // INFINITE LOOP: Run until manually stopped (Ctrl+C)
    while (true) {
        std::cout << "\n======================================\n";
        std::cout << "===  STARTING TUNING ITERATION " << iteration << "   ===\n";
        std::cout << "======================================\n";

        int base_values[9];
        if (!load_values(val_file, base_values)) {
            val_file = "piece_values.txt";
            if (!load_values(val_file, base_values)) {
                std::cerr << "Could not load piece_values.txt. Missing?" << std::endl;
                return 1;
            }
        }
        
        int tweaked_values[9];
        randomize_values(base_values, tweaked_values);
        
        std::cout << "Base:    ";
        for (int i=1; i<9; i++) std::cout << base_values[i] << " ";
        std::cout << "\nTweaked: ";
        for (int i=1; i<9; i++) std::cout << tweaked_values[i] << " ";
        std::cout << "\n\nPlaying " << MATCHES_PER_ITER << " games...\n";
        
        int score_tweaked = 0;
        int score_base = 0;
        int draws = 0;
        
        std::vector<std::future<void>> futures;
        std::mutex mtx; // Protects scores and std::cout

        for (int i = 0; i < MATCHES_PER_ITER; ++i) {
            // Spawn each game securely onto its own thread
            futures.push_back(std::async(std::launch::async, [i, &base_values, &tweaked_values, &score_tweaked, &score_base, &draws, &mtx]() {
                EngineV3 base_engine;
                base_engine.set_piece_values(base_values);
                
                EngineV3 tweaked_engine;
                tweaked_engine.set_piece_values(tweaked_values);
                
                int res = 0;
                int moves = 0;
                std::string log_msg;

                if (i % 2 == 0) {
                    // Tweaked as White (p1), Base as Black (p2)
                    auto result = play_game(tweaked_engine, base_engine);
                    res = result.first;
                    moves = result.second;
                    log_msg = "Game " + std::to_string(i+1) + ": Tweaked (White) vs Base (Black) | Result: " +
                              (res == 1 ? "Tweaked Win" : (res == -1 ? "Base Win" : "Draw")) +
                              " | Moves: " + std::to_string(moves) + "\n";
                } else {
                    // Base as White (p1), Tweaked as Black (p2)
                    auto result = play_game(base_engine, tweaked_engine);
                    res = result.first;
                    moves = result.second;
                    log_msg = "Game " + std::to_string(i+1) + ": Base (White) vs Tweaked (Black) | Result: " +
                              (res == -1 ? "Tweaked Win" : (res == 1 ? "Base Win" : "Draw")) +
                              " | Moves: " + std::to_string(moves) + "\n";
                }

                // Lock the mutex to securely update numbers and print
                std::lock_guard<std::mutex> lock(mtx);
                std::cout << log_msg;
                if (i % 2 == 0) {
                    if (res == 1) score_tweaked++;
                    else if (res == -1) score_base++;
                    else draws++;
                } else {
                    if (res == -1) score_tweaked++;
                    else if (res == 1) score_base++;
                    else draws++;
                }
            }));
        }

        // Wait for all 20 threads to finish parallel processing
        for (auto& f : futures) {
            f.wait();
        }
        
        std::cout << "\nResults: Tweaked won " << score_tweaked << " | Base won " << score_base << " | Draws " << draws << "\n";
        
        // Calculate win rate for Tweaked: (wins + 0.5 * draws) / total matches
        double win_rate = static_cast<double>(score_tweaked + 0.5 * draws) / MATCHES_PER_ITER;
        std::cout << "Tweaked Win Rate: " << win_rate * 100.0 << "% (Required: " << ACCEPT_THRESHOLD * 100.0 << "%)\n";

        // Threshold: Tweaked score must meet the acceptance threshold
        if (win_rate >= ACCEPT_THRESHOLD) {
            std::cout << "Tweaked values accepted! Updating " << val_file << " for next iteration!\n";
            save_values(val_file, tweaked_values);
        } else {
            std::cout << "Tweaked values rejected.\n";
        }
        
        iteration++;
    } // End of infinite while loop
    
    return 0;
}
