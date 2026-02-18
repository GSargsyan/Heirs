#include "engine_v1.h"
#include <algorithm>
#include <iostream>
#include <chrono>

using namespace std::chrono;

// VERSION 1 Engine does basic minimax search with piece values evaluation
EngineV1::EngineV1() : nodes_visited(0), last_depth(0) {}

long long EngineV1::get_nodes_visited() const { return nodes_visited; }
int EngineV1::get_max_depth() const { return last_depth; }

int EngineV1::evaluate(const Board& b) {
    // Basic material evaluation
    // Values:
    // Prince = 10000 (game end)
    // Princess = 500
    // Pony = 300
    // Guard = 300
    // Tutor = 300
    // Scout = 400 (flexible)
    // Sibling = 200 ??? (needs friend)
    // Baby = 100
    
    // Let's adopt simple chess-like values to start.
    // P=10000, X=900, Y=300, G=300, T=300, S=400, N=300, B=100
    
    static const int PIECE_VALUES[] = {
        0,      // NO_PIECE
        100,    // BABY
        10000,  // PRINCE
        900,    // PRINCESS
        300,    // PONY
        300,    // GUARD
        300,    // TUTOR
        400,    // SCOUT
        300     // SIBLING
    };
    
    int score = 0;
    for (int sq = 0; sq < 144; ++sq) {
        PieceType p = b.piece_at(sq);
        Color c = b.color_at(sq);
        if (p != NO_PIECE) {
            int val = PIECE_VALUES[p];
            if (c == WHITE) score += val;
            else score -= val;
        }
    }
    
    // Perspective: Always return score from White's perspective? or side to move?
    // Minimax usually maximizes for current player in recursive calls?
    // But evaluating board statically: positive for White advantage.
    
    // If side to move is Black, we want to maximize negative score?
    // Standard Minimax with Max/Min players:
    // Max (White) tries to maximize score.
    // Min (Black) tries to minimize score.
    
    return score;
}

bool EngineV1::is_time_up() {
    auto now = steady_clock::now();
    duration<double> elapsed = now - start_time;
    return elapsed.count() >= time_limit_sec;
}

// RAII wrapper for move making/unmaking
struct ScopedMove {
    Board& b;
    Move m;
    ScopedMove(Board& board, const Move& move) : b(board), m(move) {
        b.make_move(m);
    }
    ~ScopedMove() {
        b.unmake_move(m);
    }
};

// Minimax (No Alpha-Beta)
int EngineV1::minimax(Board& b, int depth) {
    nodes_visited++;
    
    if (depth == 0 || b.is_game_over()) {
        return evaluate(b);
    }
    
    // Check time every 1024 nodes
    if ((nodes_visited & 1023) == 0) {
        if (is_time_up()) throw std::runtime_error("Time limit exceeded");
    }
    
    Color side = b.side_to_move();
    std::vector<Move> moves = b.generate_moves();
    
    if (moves.empty()) {
        return 0; // Draw
    }
    
    if (side == WHITE) { // Maximize
        int max_eval = -2000000;
        for (const auto& move : moves) {
            {
                ScopedMove sm(b, move);
                int eval = minimax(b, depth - 1);
                max_eval = std::max(max_eval, eval);
            }
        }
        return max_eval;
    } else { // Minimize
        int min_eval = 2000000;
        for (const auto& move : moves) {
            {
                ScopedMove sm(b, move);
                int eval = minimax(b, depth - 1);
                min_eval = std::min(min_eval, eval);
            }
        }
        return min_eval;
    }
}

Move EngineV1::search(Board& b, double time_limit) {
    start_time = steady_clock::now();
    time_limit_sec = time_limit;
    nodes_visited = 0;
    last_depth = 0;
    
    Move best_move;
    int max_depth = 100; 
    
    // Iterative Deepening
    for (int depth = 1; depth <= max_depth; ++depth) {
        last_depth = depth;
        try {
            Color side = b.side_to_move();
            std::vector<Move> moves = b.generate_moves();
            
            if (moves.empty()) return Move();
            
            Move current_best_move;
            int best_score;
            
            if (side == WHITE) {
                best_score = -2000000;
                for (const auto& move : moves) {
                    int score;
                    {
                        ScopedMove sm(b, move);
                        score = minimax(b, depth - 1);
                    }
                    
                    if (score > best_score) {
                        best_score = score;
                        current_best_move = move;
                    }
                    
                    if (is_time_up()) throw std::runtime_error("Time limit");
                }
            } else {
                best_score = 2000000;
                for (const auto& move : moves) {
                    int score;
                    {
                        ScopedMove sm(b, move);
                        score = minimax(b, depth - 1);
                    }
                    
                    if (score < best_score) {
                        best_score = score;
                        current_best_move = move;
                    }
                    
                    if (is_time_up()) throw std::runtime_error("Time limit");
                }
            }
            
            best_move = current_best_move;
            
            if (is_time_up()) break;
            
        } catch (const std::exception& e) {
            // Stack unwinding in ScopedMove will unmake the moves correctly.
            break;
        }
    }
    
    return best_move;
}
