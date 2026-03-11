#include <iostream>
#include <vector>
#include <string>
#include <algorithm>
#include <iomanip>
#include <windows.h>

#define private public
#include "../src/engine_v6.h"
#undef private

std::string move_to_string(const Move& m) {
    if (m.from == 0 && m.to == 0) return "NullMove";
    const char* cols = "abcdefghjkmn";
    int r1 = m.from / 12;
    int c1 = m.from % 12;
    int r2 = m.to / 12;
    int c2 = m.to % 12;
    return std::string(1, cols[c1]) + std::to_string(r1 + 1) + "-" + std::string(1, cols[c2]) + std::to_string(r2 + 1);
}

void print_eval(EngineV6& engine, const Board& b, const std::string& case_name) {
    std::cout << "========================================" << std::endl;
    std::cout << "Test Case: " << case_name << std::endl;
    std::cout << "========================================" << std::endl;
    b.print();
    
    // Evaluate the board
    auto eval = engine.evaluate(b);
    float value = eval.second;
    std::cout << "\n[Value Network Score] (from " << (b.side_to_move() == WHITE ? "White" : "Black") << "'s perspective): " << value << std::endl;
    
    std::cout << "\n[Policy Network Top 5 Legal Moves]:" << std::endl;
    auto moves = b.generate_moves();
    
    struct MoveProb {
        Move m;
        float prob;
    };
    std::vector<MoveProb> probs;
    
    float sum_p = 0.0f;
    for (const Move& m : moves) {
        int action = engine.move_to_action(m, b.side_to_move());
        float p = (action >= 0 && action < 7056) ? eval.first[action] : 0.0f;
        probs.push_back({m, p});
        sum_p += p;
    }
    
    // Normalize probabilities among legal moves just like MCTS does
    if (sum_p > 0) {
        for (auto& mp : probs) mp.prob /= sum_p;
    } else {
        if (!probs.empty()) {
            for (auto& mp : probs) mp.prob = 1.0f / probs.size();
        }
    }
    
    std::sort(probs.begin(), probs.end(), [](const MoveProb& a, const MoveProb& b) {
        return a.prob > b.prob;
    });
    
    for (size_t i = 0; i < std::min((size_t)5, probs.size()); ++i) {
        std::cout << "  " << i+1 << ". " << move_to_string(probs[i].m) 
                  << " - Raw Probability (Legal Normalized): " << std::fixed << std::setprecision(4) << probs[i].prob * 100.0f << "%" << std::endl;
    }
    std::cout << "\n\n";
}

int main() {
    // Set console output to UTF-8 for displaying the board characters correctly
    SetConsoleOutputCP(CP_UTF8);

    EngineV6 engine;
    // Attempt to load weights from the parent directory
    if (!engine.load_weights("../weights.bin")) {
        std::cerr << "Failed to load ../weights.bin. Attempting to load from current directory..." << std::endl;
        if (!engine.load_weights("weights.bin")) {
            std::cerr << "Failed to load weights.bin completely." << std::endl;
            return 1;
        }
    }
    std::cout << "Loaded weights successfully." << std::endl;
    std::cout << std::endl;

    // Case 1: Black without babies (pawns)
    {
        Board b; 
        for (int i=0; i<144; ++i) {
            if (b.piece_at(i) == BABY && b.color_at(i) == BLACK) {
                b.clear_piece(i);
            }
        }
        print_eval(engine, b, "Black without Babies (Pawns)");
    }

    // Case 2: White left with only Prince
    {
        Board b;
        for (int i=0; i<144; ++i) {
            if (b.color_at(i) == WHITE && b.piece_at(i) != PRINCE) {
                b.clear_piece(i);
            }
        }
        print_eval(engine, b, "White left with only Prince");
    }

    // Case 3: White has very developed pieces, Black is in initial position
    {
        Board b;
        // Move White pieces to developed positions (clear row 0 and 1, except Prince)
        for (int i = 0; i < 24; ++i) {
            PieceType p = b.piece_at(i);
            if (p != NO_PIECE && p != PRINCE && b.color_at(i) == WHITE) {
                b.clear_piece(i);
            }
        }
        // Place White's pieces in developed, attacking positions
        b.set_piece(4 * 12 + 5, PRINCESS, WHITE); // f5
        b.set_piece(5 * 12 + 4, PONY, WHITE);     // e6
        b.set_piece(5 * 12 + 7, PONY, WHITE);     // h6
        b.set_piece(6 * 12 + 2, TUTOR, WHITE);    // c7
        b.set_piece(6 * 12 + 9, TUTOR, WHITE);    // k7
        b.set_piece(7 * 12 + 5, SCOUT, WHITE);    // f8
        b.set_piece(7 * 12 + 6, SCOUT, WHITE);    // g8
        b.set_piece(3 * 12 + 4, SIBLING, WHITE);  // e4
        b.set_piece(3 * 12 + 7, SIBLING, WHITE);  // h4
        b.set_piece(2 * 12 + 0, GUARD, WHITE);    // a3
        b.set_piece(2 * 12 + 11, GUARD, WHITE);   // n3
        
        // Advanced pawns (babies)
        for (int c = 2; c < 10; ++c) {
            b.set_piece(6 * 12 + c, BABY, WHITE); // Advanced babies on row 7
        }
        
        print_eval(engine, b, "White very developed, Black in initial position");
    }

    // Case 4: Black has very developed pieces, White is in initial position
    {
        Board b;
        // Move Black pieces to developed positions (clear row 10 and 11, except Prince)
        for (int i = 120; i < 144; ++i) {
            PieceType p = b.piece_at(i);
            if (p != NO_PIECE && p != PRINCE && b.color_at(i) == BLACK) {
                b.clear_piece(i);
            }
        }
        // Place Black's pieces in developed, attacking positions
        b.set_piece(7 * 12 + 5, PRINCESS, BLACK); // f8
        b.set_piece(6 * 12 + 4, PONY, BLACK);     // e7
        b.set_piece(6 * 12 + 7, PONY, BLACK);     // h7
        b.set_piece(5 * 12 + 2, TUTOR, BLACK);    // c6
        b.set_piece(5 * 12 + 9, TUTOR, BLACK);    // k6
        b.set_piece(4 * 12 + 5, SCOUT, BLACK);    // f5
        b.set_piece(4 * 12 + 6, SCOUT, BLACK);    // g5
        b.set_piece(8 * 12 + 4, SIBLING, BLACK);  // e9
        b.set_piece(8 * 12 + 7, SIBLING, BLACK);  // h9
        b.set_piece(9 * 12 + 0, GUARD, BLACK);    // a10
        b.set_piece(9 * 12 + 11, GUARD, BLACK);   // n10
        
        // Advanced pawns (babies)
        for (int c = 2; c < 10; ++c) {
            b.set_piece(5 * 12 + c, BABY, BLACK); // Advanced babies on row 6
        }
        
        print_eval(engine, b, "Black very developed, White in initial position");
    }

    return 0;
}
