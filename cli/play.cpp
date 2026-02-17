#include "../src/engine_v2.h"
#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <limits>
#include <iomanip>

// Helper to convert move to string (copied from match.cpp for consistency)
std::string move_to_string(const Move& m) {
    if (m.from == 0 && m.to == 0) return "NullMove";
    
    // Columns: a b c d e f g h j k m n
    const char* cols = "abcdefghjkmn";
    
    int r1 = m.from / 12;
    int c1 = m.from % 12;
    int r2 = m.to / 12;
    int c2 = m.to % 12;
    
    std::string s = "";
    s += cols[c1];
    s += std::to_string(r1 + 1);
    s += "-";
    s += cols[c2];
    s += std::to_string(r2 + 1);
    return s;
}

int get_col_index(char c) {
    c = std::tolower(c);
    const char* cols = "abcdefghjkmn";
    for (int i = 0; i < 12; ++i) {
        if (cols[i] == c) return i;
    }
    return -1;
}

int parse_square(std::string s) {
    if (s.empty()) return -1;
    int col = get_col_index(s[0]);
    if (col == -1) return -1;
    try {
        std::string row_part = s.substr(1);
        if (row_part.empty()) return -1;
        int row = std::stoi(row_part) - 1;
        if (row < 0 || row >= 12) return -1;
        return row * 12 + col;
    } catch (...) {
        return -1;
    }
}

Move parse_move(std::string input, const Board& b) {
    // Try to split by '-'
    size_t sep = input.find('-');
    
    std::string from_str, to_str;
    if (sep != std::string::npos) {
        from_str = input.substr(0, sep);
        to_str = input.substr(sep + 1);
    } else {
        return Move(); // Invalid format
    }
    
    int from = parse_square(from_str);
    int to = parse_square(to_str);
    
    if (from == -1 || to == -1) return Move();

    std::vector<Move> moves = b.generate_moves();
    for (const auto& m : moves) {
        if (m.from == from && m.to == to) return m;
    }
    return Move();
}

int main(int argc, char* argv[]) {
    Board b;
    // b.reset() is called in constructor
    
    EngineV2 engine;
    
    std::cout << "========================================\n";
    std::cout << "      Play vs Heirs Engine V2          \n";
    std::cout << "========================================\n";
    
    char choice = ' ';
    while (choice != 'w' && choice != 'b') {
        std::cout << "Choose your color (w for White, b for Black): ";
        std::cin >> choice;
        choice = std::tolower(choice);
    }
    Color player_color = (choice == 'w') ? WHITE : BLACK;
    
    double time_limit = 1.0;
    std::cout << "Enter engine time limit in seconds (default 1.0): ";
    // Use a temp string to invalid input handling
    std::string time_str;
    std::cin >> time_str;
    try {
        time_limit = std::stod(time_str);
    } catch (...) {
        std::cout << "Invalid time, defaulting to 1.0s\n";
        time_limit = 1.0;
    }
    
    std::cout << "\nGame Starting! You are " << (player_color == WHITE ? "White" : "Black") << ".\n";
    std::cout << "Enter moves in format 'e2-e4'.\n\n";
    
    b.print();
    
    int moves_count = 0;
    while (!b.is_game_over()) {
        if (b.is_draw()) {
            std::cout << "Game Over. Draw detected (Repetition or 50-move rule).\n";
            return 0;
        }

        Color side = b.side_to_move();
        std::cout << "\nMove " << (moves_count/2 + 1) << ". " << (side == WHITE ? "White" : "Black") << " to move." << std::endl;
        
        Move m;
        if (side == player_color) {
            bool valid = false;
            while (!valid) {
                std::cout << "Your move: ";
                std::string input;
                std::cin >> input;
                
                if (input == "quit" || input == "exit") {
                    std::cout << "Game aborted.\n";
                    return 0;
                }
                
                m = parse_move(input, b);
                if (m.from == 0 && m.to == 0) {
                     std::cout << "Invalid move or format. Use 'e2-e4'. Type 'quit' to exit.\n";
                } else {
                    valid = true;
                }
            }
        } else {
            std::cout << "Engine is thinking...\n";
            m = engine.search(b, time_limit);
            std::cout << "Engine plays: " << move_to_string(m) << "\n";
            
            if (m.from == 0 && m.to == 0) {
                std::cout << "Engine resigned or no moves left.\n";
                break;
            }
        }
        
        b.make_move(m);
        b.print();
        moves_count++;
    }
    
    std::cout << "\nGame Over!" << std::endl;
    
    if (b.is_game_over()) {
        Color current_turn = b.side_to_move();
        Color winner = (Color)(1 - current_turn);
        std::cout << (winner == WHITE ? "White" : "Black") << " wins!" << std::endl;
    } else {
        // Stalemate via no moves (if logic above didn't catch it?)
        // Or draw detected
        if (b.is_draw()) {
             std::cout << "Draw detected." << std::endl;
        } else {
             // Should not happen if loop terminates on game_over or break
             std::cout << "Game ended unexpectedly." << std::endl;
        }
    }
    
    return 0;
}
