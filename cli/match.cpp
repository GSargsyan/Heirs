#include "../src/engine_v1.h"
#include "../src/engine_v2.h"
#include "../src/engine_v3.h"
#include <iostream>
#include <vector>
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
const double TIME_TO_THINK_WHITE = 5;
const double TIME_TO_THINK_BLACK = 1;

int main(int argc, char* argv[]) {
#ifdef _WIN32
    // Set console code page to UTF-8
    SetConsoleOutputCP(CP_UTF8);
#endif

    Board b;
    // b.reset(); // Constructor calls reset
    
    EngineV3 engine_white;
    EngineV1 engine_black;
    
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
