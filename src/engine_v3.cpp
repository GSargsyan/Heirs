#include "engine_v3.h"
#include <algorithm>
#include <iostream>
#include <chrono>
#include <vector>

using namespace std::chrono;

// VERSION 3 Engine is V2 + transposition table (with move ordering in it) + different piece approximation costs
EngineV3::EngineV3() : nodes_visited(0), last_depth(0) {
    tt.resize(TT_SIZE);
    clear_tt();
}

long long EngineV3::get_nodes_visited() const { return nodes_visited; }
int EngineV3::get_max_depth() const { return last_depth; }

void EngineV3::clear_tt() {
    std::fill(tt.begin(), tt.end(), TTEntry{0, 0, 0, 0, Move()});
}

bool EngineV3::probe_tt(uint64_t key, int depth, int alpha, int beta, int& score, Move& tt_move) {
    size_t index = key & (TT_SIZE - 1); // Power of 2 mask
    const TTEntry& entry = tt[index];

    if (entry.key == key) {
        tt_move = entry.best_move;
        
        if (entry.depth >= depth) {
            if (entry.flag == TT_EXACT) {
                score = entry.score;
                return true;
            }
            if (entry.flag == TT_LOWER && entry.score >= beta) {
                score = entry.score;
                return true;
            }
            if (entry.flag == TT_UPPER && entry.score <= alpha) {
                score = entry.score;
                return true;
            }
        }
    }
    return false;
}

void EngineV3::store_tt(uint64_t key, int depth, int score, int flag, Move best_move) {
    size_t index = key & (TT_SIZE - 1); // Power of 2 mask
    
    // Replacement strategy: Deepest or Empty
    // (A simple Always Replace or Depth Preferred is common)
    // Here we use Depth Preferred: only overwrite if new depth >= old depth
    // But we should also overwrite if the old entry is from a different position (collision) ??
    // Actually, "always replace" is often surprisingly good.
    // Let's use a hybrid: Replace if new depth >= old depth OR old entry is stale/different.
    // Since we don't track age, we just use depth.
    
    // Simple: Always replace (works well for small TTs or high contention, ensures fresh data)
    // Or Depth-Preferred:
    if (tt[index].key == 0 || depth >= tt[index].depth) {
        tt[index] = {key, depth, score, flag, best_move};
    }
}

int EngineV3::evaluate(const Board& b) {
    // Improved Evaluation
    // 1. Material - improved
    // 2. Positional Heuristics
    //    - Advance towards center (Files D-K, Ranks 4-9)
    //    - Baby advancement
    // 3. Mobility (Number of legal moves)
    
    static const int PIECE_VALUES[] = {
        0,      // NO_PIECE
        100,    // BABY (Base value)
        20000,  // PRINCE (Infinite/Game Over)
        950,    // PRINCESS (Queen equivalent, high mobility)
        150,    // PONY (Very weak, limited range, colorbound)
        325,    // GUARD (Solid, controls lines, though limited range)
        300,    // TUTOR (Solid, controls diagonals, though limited range)
        550,    // SCOUT (High value: Jumps, forks, fast attack)
        250     // SIBLING (Weak attacker, but strong defender)
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
    
    return eval;
}

bool EngineV3::is_time_up() {
    auto now = steady_clock::now();
    duration<double> elapsed = now - start_time;
    return elapsed.count() >= time_limit_sec;
}

// RAII wrapper for move making/unmaking
struct ScopedMoveV3 {
    Board& b;
    Move m;
    ScopedMoveV3(Board& board, const Move& move) : b(board), m(move) {
        b.make_move(m);
    }
    ~ScopedMoveV3() {
        b.unmake_move(m);
    }
};

// Alpha-Beta Search
int EngineV3::alphabeta(Board& b, int depth, int alpha, int beta) {
    // Probe TT
    uint64_t key = b.get_hash();
    int tt_score;
    Move tt_move;
    
    if (probe_tt(key, depth, alpha, beta, tt_score, tt_move)) {
        return tt_score;
    }
    
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
    
    // Move Ordering: TT move first
    if (tt_move.from != 0 || tt_move.to != 0) {
        for (size_t i = 0; i < moves.size(); ++i) {
            if (moves[i] == tt_move) {
                std::swap(moves[0], moves[i]);
                break;
            }
        }
    }
    
    int original_alpha = alpha;
    Move best_move_in_node;
    int best_score;
    
    if (side == WHITE) { // Maximize
        best_score = -2000000;
        for (const auto& move : moves) {
            {
                ScopedMoveV3 sm(b, move);
                int eval = alphabeta(b, depth - 1, alpha, beta);
                if (eval > best_score) {
                    best_score = eval;
                    best_move_in_node = move;
                }
            }
            alpha = std::max(alpha, best_score);
            if (beta <= alpha) break; // Beta Cutoff
        }
    } else { // Minimize
        best_score = 2000000;
        for (const auto& move : moves) {
            {
                ScopedMoveV3 sm(b, move);
                int eval = alphabeta(b, depth - 1, alpha, beta);
                if (eval < best_score) {
                    best_score = eval;
                    best_move_in_node = move;
                }
            }
            beta = std::min(beta, best_score);
            if (beta <= alpha) break; // Alpha Cutoff
        }
    }
    
    // Store in TT
    int flag;
    if (best_score <= original_alpha) {
        flag = TT_UPPER;
    } else if (best_score >= beta) {
        flag = TT_LOWER;
    } else {
        flag = TT_EXACT;
    }
    
    store_tt(key, depth, best_score, flag, best_move_in_node);
    
    return best_score;
}

Move EngineV3::search(Board& b, double time_limit) {
    start_time = steady_clock::now();
    time_limit_sec = time_limit;
    nodes_visited = 0;
    last_depth = 0;
    
    // Clear TT at start of search to ensure no stale data from previous games/moves interferes?
    // User wants "skipping already evaluated positions".
    // Usually standard engines KEEP the TT.
    // "It will be for skipping already evaluated positions... improve the iterative deepening"
    // I will NOT clear TT here, allowing it to persist across calls if the instance persists.
    // But I should ensure the same engine instance is used in match.cpp.
    // Wait, in previous match.cpp, engines are instantiated once in main.
    // So if I don't clear, it persists across moves! This is good.
    // So remove `clear_tt()` from search.
    
    Move best_move;
    int max_depth = 100; 
    
    for (int depth = 1; depth <= max_depth; ++depth) {
        last_depth = depth;
        try {
            Color side = b.side_to_move();
            std::vector<Move> moves = b.generate_moves();
            
            if (moves.empty()) return Move();
            
            // Should prompt TT for root move ordering too?
            // Existing logic uses `best_move` from previous iteration or TT.
            // TT already helps alphabeta calls.
            // But let's keep the user's structure which just calls alphabeta.

            Move current_best_move;
            int best_score;
            int alpha = -2000000;
            int beta = 2000000;
            
            // We can also use TT at the root to improve ordering if we wanted, 
            // but alphabeta calls recursively use it.
            // The root search itself is not recursively calling alphabeta for the root, 
            // it iterates moves and calls alphabeta(depth-1).
            
            // To get TT benefit at root for ordering, we'd need to peek. 
            // But since we iterate depth, previous iteration already populated TT.
            // `alphabeta` inside the loop will benefit from TT hits for subtrees.
            // What about ordering ROOT moves?
            // The loop below iterates moves. If we gathered stats or used TT move first, it would be improved.
            // Let's implement basic root move ordering using TT if available.
            
            uint64_t root_key = b.get_hash();
            int tt_score_dummy;
            Move tt_root_move;
            probe_tt(root_key, depth, alpha, beta, tt_score_dummy, tt_root_move);
            
            if (tt_root_move.from != 0 || tt_root_move.to != 0) {
                 for (size_t i = 0; i < moves.size(); ++i) {
                     if (moves[i] == tt_root_move) {
                         std::swap(moves[0], moves[i]);
                         break;
                     }
                 }
            }
            
            if (side == WHITE) {
                best_score = -2000000;
                for (const auto& move : moves) {
                    int score;
                    {
                        ScopedMoveV3 sm(b, move);
                        score = alphabeta(b, depth - 1, alpha, beta);
                    }
                    
                    if (score > best_score) {
                        best_score = score;
                        current_best_move = move;
                    }
                    alpha = std::max(alpha, best_score); 
                    
                    if (is_time_up()) throw std::runtime_error("Time limit");
                }
            } else {
                best_score = 2000000;
                for (const auto& move : moves) {
                    int score;
                    {
                        ScopedMoveV3 sm(b, move);
                        score = alphabeta(b, depth - 1, alpha, beta);
                    }
                    
                    if (score < best_score) {
                        best_score = score;
                        current_best_move = move;
                    }
                    beta = std::min(beta, best_score); 
                    
                    if (is_time_up()) throw std::runtime_error("Time limit");
                }
            }
            
            best_move = current_best_move;
            // Store root result in TT
            store_tt(root_key, depth, best_score, TT_EXACT, best_move);
            
            if (is_time_up()) break;
            
        } catch (const std::exception& e) {
            break;
        }
    }
    
    return best_move;
}
