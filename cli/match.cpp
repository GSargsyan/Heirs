#include "../src/engine_v1.h"
#include "../src/engine_v2.h"
#include "../src/eval_constants.h"
#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#ifdef _WIN32
#include <windows.h>
#endif

std::string move_to_string(const Move& m) {
    if (m.from == 0 && m.to == 0) return "NullMove";
    
    // 0-143 -> a1..n12
    // col 0 = a .. 
    // row 0 = 1 ..
    // Columns: a b c d e f g h j k m n
    // Index:   0 1 2 3 4 5 6 7 8 9 10 11
    
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

// Time limits in seconds
const double TIME_TO_THINK_WHITE = 0.2;
const double TIME_TO_THINK_BLACK = 0.01;

// Use tuned piece values from piece_values.txt
const bool USE_TUNED_VALUES_WHITE = false;
const bool USE_TUNED_VALUES_BLACK = false;

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
    // Set console code page to UTF-8
    SetConsoleOutputCP(CP_UTF8);
#endif

    Board b;
    // b.reset(); // Constructor calls reset
    
    // EngineV2 vs EngineV1
    EngineV2 engine_white;
    EngineV2 engine_black;
    
    if (USE_TUNED_VALUES_WHITE) {
        int vals[9];
        if (load_values("piece_values.txt", vals) || load_values("../piece_values.txt", vals)) {
            engine_white.set_piece_values(vals);
            std::cout << "White is using tuned piece values from file.\n";
        } else {
            std::cout << "Failed to load piece_values.txt for White.\n";
        }
    }
    if (USE_TUNED_VALUES_BLACK) {
        int vals[9];
        if (load_values("piece_values.txt", vals) || load_values("../piece_values.txt", vals)) {
            engine_black.set_piece_values(vals);
            std::cout << "Black is using tuned piece values from file.\n";
        } else {
            std::cout << "Failed to load piece_values.txt for Black.\n";
        }
    }
    
    std::cout << "Starting match with time limits: White=" << TIME_TO_THINK_WHITE << "s, Black=" << TIME_TO_THINK_BLACK << "s" << std::endl;
    b.print();
    
    int moves = 0;
    while (!b.is_game_over() && moves < 600) { // Limit game length to avoid infinite
        if (b.is_draw()) {
            std::cout << "Draw detected (Repetition or 50-move rule)." << std::endl;
            return 0;
        }

        Color side = b.side_to_move();
        std::cout << "Move " << (moves/2 + 1) << ". " << (side == WHITE ? "White" : "Black") << " to move." << std::endl;
        
        Move m;
        if (side == WHITE) {
            m = engine_white.search(b, TIME_TO_THINK_WHITE);
            std::cout << "  Stats: Depth=" << engine_white.get_max_depth() 
                      << ", Nodes=" << engine_white.get_nodes_visited() << std::endl;
        } else {
            m = engine_black.search(b, TIME_TO_THINK_BLACK);
            std::cout << "  Stats: Depth=" << engine_black.get_max_depth() 
                      << ", Nodes=" << engine_black.get_nodes_visited() << std::endl;
        }
        
        if (m.from == 0 && m.to == 0) {
            std::cout << "Engine returned null move (Resign/No moves?)" << std::endl;
            MoveList m_list;
            b.generate_moves(m_list);
            if (m_list.count == 0) {
                std::cout << "Stalemate detected." << std::endl;
            }
            break;
        }
        
        b.make_move(m);
        b.print();
        
        int white_mat = 0;
        int black_mat = 0;
        for (int p = 1; p < 9; ++p) {
            white_mat += b.piece_counts[WHITE][p] * EvalConstants::PIECE_VALUES[p];
            black_mat += b.piece_counts[BLACK][p] * EvalConstants::PIECE_VALUES[p];
        }
        std::cout << "Material Diff (White - Black): " << (white_mat - black_mat) << std::endl;
        
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
