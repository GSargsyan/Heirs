#include "../src/engine_v1.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <sstream>

#ifdef _WIN32
#include <windows.h>
#endif

std::string move_to_string(const Move& m) {
    if (m.from == 0 && m.to == 0) return "NullMove";
    const char* cols = "abcdefghjkmn";
    int r1 = m.from >> 4;
    int c1 = m.from & 15;
    int r2 = m.to >> 4;
    int c2 = m.to & 15;
    
    std::string s = "";
    s += cols[c1];
    s += std::to_string(r1 + 1);
    s += "-";
    s += cols[c2];
    s += std::to_string(r2 + 1);
    return s;
}

int parse_col(char c) {
    const char* cols = "abcdefghjkmn";
    for(int i = 0; i < 12; ++i) {
        if(cols[i] == c) return i;
    }
    return -1;
}

bool parse_square(const std::string& s, int& sq) {
    if (s.length() < 2) return false;
    int c = parse_col(s[0]);
    if (c == -1) return false;
    try {
        int r = std::stoi(s.substr(1)) - 1;
        if (r < 0 || r > 11) return false;
        sq = (r << 4) | c;
        return true;
    } catch (...) {
        return false;
    }
}

bool parse_move(const std::string& input, Move& out_move, const Board& b) {
    std::stringstream ss(input);
    std::string q1, q2;
    if (!(ss >> q1 >> q2)) return false;
    int from, to;
    if (!parse_square(q1, from)) return false;
    if (!parse_square(q2, to)) return false;
    
    MoveList ml;
    b.generate_moves(ml);
    for(int i = 0; i < ml.count; ++i) {
        if (ml.moves[i].from == from && ml.moves[i].to == to) {
            out_move = ml.moves[i];
            return true;
        }
    }
    return false;
}

bool load_values(const std::string& filename, int values[9]) {
    std::ifstream in(filename);
    if (!in) return false;
    for (int i = 0; i < 9; ++i) {
        if (!(in >> values[i])) return false;
    }
    return true;
}

int main(int argc, char* argv[]) {
#ifdef _WIN32
    SetConsoleOutputCP(CP_UTF8);
#endif

    Board b;
    EngineV1 engine_bot;
    
    // Use tuned piece values if available
    int vals[9];
    if (load_values("piece_values.txt", vals) || load_values("../piece_values.txt", vals)) {
        engine_bot.set_piece_values(vals);
        std::cout << "Bot is using tuned piece values from file.\n";
    }
    
    double bot_time = 1.0;
    
    std::cout << "Starting game. You are White, bot is Black. Time to think: " << bot_time << "s\n";
    std::cout << "Enter your moves in format: a2 a4\n";
    
    b.print();
    
    int moves = 0;
    while (!b.is_game_over() && moves < 600) {
        if (b.is_draw()) {
            std::cout << "Draw detected (Repetition or 50-move rule)." << std::endl;
            break;
        }

        Color side = b.side_to_move();
        
        if (side == WHITE) {
            std::string input;
            Move m;
            while (true) {
                std::cout << "White to move: ";
                if (!std::getline(std::cin, input)) return 0; // EOF
                
                // Allow user to exit
                if (input == "quit" || input == "exit") return 0;
                
                if (parse_move(input, m, b)) {
                    break;
                } else {
                    std::cout << "Invalid move. Try again.\n";
                }
            }
            b.make_move(m);
            b.print();
        } else {
            std::cout << "\nBot is thinking..." << std::endl;
            Move m = engine_bot.search(b, bot_time);
            std::cout << "  Stats: Depth=" << engine_bot.get_max_depth() 
                      << ", Nodes=" << engine_bot.get_nodes_visited() << std::endl;
                      
            if (m.from == 0 && m.to == 0) {
                std::cout << "Engine returned null move (Resign/No moves?)" << std::endl;
                MoveList m_list;
                b.generate_moves(m_list);
                if (m_list.count == 0) {
                    std::cout << "Stalemate detected." << std::endl;
                }
                break;
            }
            
            std::cout << "Bot plays: " << move_to_string(m) << std::endl;
            b.make_move(m);
            b.print();
        }
        
        moves++;
    }
    
    if (b.is_game_over()) {
        std::cout << "Game Over. Prince Captured." << std::endl;
    } else if (moves >= 600) {
        std::cout << "Game Over. Move limit reached (Draw)." << std::endl;
    } else if (b.is_draw()) {
        std::cout << "Game Over. Draw detected." << std::endl;
    }

    return 0;
}
