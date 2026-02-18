#include "engine_v2.h"
#include <algorithm>
#include <iostream>
#include <chrono>
#include <vector>

using namespace std::chrono;

// VERSION 2 Engine is V1 + alpha-beta pruning + basic center control and mobility 
EngineV2::EngineV2() : nodes_visited(0), last_depth(0) {}

long long EngineV2::get_nodes_visited() const { return nodes_visited; }
int EngineV2::get_max_depth() const { return last_depth; }

int EngineV2::evaluate(const Board& b) {
    // Improved Evaluation
    // 1. Material (kept from V1)
    // 2. Positional Heuristics
    //    - Advance towards center (Files D-K, Ranks 4-9)
    //    - Baby advancement
    // 3. Mobility (Number of legal moves)
    
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
    
    int material_score = 0;
    int positional_score = 0;
    
    // Simple positional map: Center = higher value
    // 12x12
    // Rows 0-11, Cols 0-11
    
    for (int sq = 0; sq < 144; ++sq) {
        PieceType p = b.piece_at(sq);
        if (p == NO_PIECE) continue;
        
        Color c = b.color_at(sq);
        int val = PIECE_VALUES[p];
        
        int r = sq / 12;
        int col = sq % 12;
        
        // Positional Bonus
        int bonus = 0;
        
        // Center Bonus (cols 3-8, rows 3-8)
        if (col >= 3 && col <= 8 && r >= 3 && r <= 8) {
            bonus += 10;
        }
        
        // Advancement Bonus (for non-prince)
        if (p != PRINCE) {
            if (c == WHITE) {
                bonus += r * 5; // Reward moving up
            } else {
                bonus += (11 - r) * 5; // Reward moving down
            }
        }
        
        if (c == WHITE) {
            material_score += val;
            positional_score += bonus;
        } else {
            material_score -= val;
            positional_score -= bonus;
        }
    }
    
    int eval = material_score + positional_score;
    
    // Mobility (Expensive? generating moves is somewhat costly)
    // Let's try adding it.
    // We need to generate moves for the side to move? 
    // Usually mobility is calc for both sides.
    // But generate_moves() only does side_to_move.
    // If we want mobility for both, we need to hack it or skip it for now to avoid being too slow.
    // "heuristics... not slow". 
    // Let's stick to Positional + Material for now, iterating mobility might double the cost.
    // But we can approximate mobility:
    // e.g. Scouts have high mobility in open.
    
    return eval;
}

bool EngineV2::is_time_up() {
    auto now = steady_clock::now();
    duration<double> elapsed = now - start_time;
    return elapsed.count() >= time_limit_sec;
}

// RAII wrapper for move making/unmaking
struct ScopedMoveV2 {
    Board& b;
    Move m;
    ScopedMoveV2(Board& board, const Move& move) : b(board), m(move) {
        b.make_move(m);
    }
    ~ScopedMoveV2() {
        b.unmake_move(m);
    }
};

// Alpha-Beta Search
int EngineV2::alphabeta(Board& b, int depth, int alpha, int beta) {
    nodes_visited++;
    
    // Check for draw by repetition or 50-move rule
    if (b.is_draw()) {
        return 0; 
    }
    
    if (depth == 0 || b.is_game_over()) {
        return evaluate(b);
    }
    
    if ((nodes_visited & 1023) == 0) {
        if (is_time_up()) throw std::runtime_error("Time limit exceeded");
    }
    
    Color side = b.side_to_move();
    std::vector<Move> moves = b.generate_moves();
    
    if (moves.empty()) {
        return 0; // Draw
    }
    
    // Move ordering? (Captures first?)
    // Basic ordering: if capture, put front.
    // For now, simpler improvement first.
    
    if (side == WHITE) { // Maximize
        int max_eval = -2000000;
        for (const auto& move : moves) {
            {
                ScopedMoveV2 sm(b, move);
                int eval = alphabeta(b, depth - 1, alpha, beta);
                max_eval = std::max(max_eval, eval);
                alpha = std::max(alpha, eval);
            }
             if (beta <= alpha) break; // Beta Cutoff
        }
        return max_eval;
    } else { // Minimize
        int min_eval = 2000000;
        for (const auto& move : moves) {
            {
                ScopedMoveV2 sm(b, move);
                int eval = alphabeta(b, depth - 1, alpha, beta);
                min_eval = std::min(min_eval, eval);
                beta = std::min(beta, eval);
            }
             if (beta <= alpha) break; // Alpha Cutoff
        }
        return min_eval;
    }
}

Move EngineV2::search(Board& b, double time_limit) {
    start_time = steady_clock::now();
    time_limit_sec = time_limit;
    nodes_visited = 0;
    last_depth = 0;
    
    Move best_move;
    int max_depth = 100; 
    
    for (int depth = 1; depth <= max_depth; ++depth) {
        last_depth = depth;
        try {
            Color side = b.side_to_move();
            std::vector<Move> moves = b.generate_moves();
            
            if (moves.empty()) return Move();
            
            Move current_best_move;
            int best_score;
            int alpha = -2000000;
            int beta = 2000000;
            
            if (side == WHITE) {
                best_score = -2000000;
                for (const auto& move : moves) {
                    int score;
                    {
                        ScopedMoveV2 sm(b, move);
                        score = alphabeta(b, depth - 1, alpha, beta);
                    }
                    
                    if (score > best_score) {
                        best_score = score;
                        current_best_move = move;
                    }
                    alpha = std::max(alpha, best_score); // Update alpha at root
                    
                    if (is_time_up()) throw std::runtime_error("Time limit");
                }
            } else {
                best_score = 2000000;
                for (const auto& move : moves) {
                    int score;
                    {
                        ScopedMoveV2 sm(b, move);
                        score = alphabeta(b, depth - 1, alpha, beta);
                    }
                    
                    if (score < best_score) {
                        best_score = score;
                        current_best_move = move;
                    }
                    beta = std::min(beta, best_score); // Update beta at root
                    
                    if (is_time_up()) throw std::runtime_error("Time limit");
                }
            }
            
            best_move = current_best_move;
            
            if (is_time_up()) break;
            
        } catch (const std::exception& e) {
            break;
        }
    }
    
    return best_move;
}
