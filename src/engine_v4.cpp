#include "engine_v4.h"
#include <algorithm>
#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>

using namespace std::chrono;

// VERSION 4 Engine is V2 + improved evaluation function
EngineV4::EngineV4() : nodes_visited(0), last_depth(0) {}

long long EngineV4::get_nodes_visited() const { return nodes_visited; }
int EngineV4::get_max_depth() const { return last_depth; }

// Heirs Constants for Evaluation
namespace EvalConstants {
    // Adjusted values based on 12x12 board dynamics
    const int VAL_BABY = 100;
    const int VAL_PRINCE = 20000;
    const int VAL_PRINCESS = 950;
    const int VAL_PONY = 180;    // Slightly higher: they are weak, but useful for shielding
    const int VAL_GUARD = 350;   // Good defenders
    const int VAL_TUTOR = 325;
    const int VAL_SCOUT = 600;   // The most dangerous piece after Princess due to range
    const int VAL_SIBLING = 280; // Needs to stick together

    const int PIECE_VALUES[] = {
        0, VAL_BABY, VAL_PRINCE, VAL_PRINCESS, VAL_PONY, 
        VAL_GUARD, VAL_TUTOR, VAL_SCOUT, VAL_SIBLING
    };
}

// Helper to flip the board index for Black (mirror vertically)
// Assuming board is 0-143, row 0 is bottom.
inline int mirror_sq(int sq) {
    int r = sq / 12;
    int c = sq % 12;
    return (11 - r) * 12 + c;
}

// PIECE SQUARE TABLES (Linearized 12x12)
// Defined from WHITE's perspective (bottom to top)
// High values = Good square.

// Baby: Wants to advance to ranks 4-8. Rank 10/11 is useless (0 bonus).
const int PST_BABY[144] = {
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // Rank 0 (Invalid for babies usually)
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5, // Rank 1
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, // Rank 2
    15, 15, 20, 25, 25, 25, 25, 25, 20, 15, 15, 15, // Rank 3
    20, 20, 25, 30, 35, 35, 35, 35, 30, 25, 20, 20, // Rank 4
    25, 25, 30, 40, 45, 45, 45, 45, 40, 30, 25, 25, // Rank 5
    25, 25, 30, 40, 45, 45, 45, 45, 40, 30, 25, 25, // Rank 6
    20, 20, 25, 30, 35, 35, 35, 35, 30, 25, 20, 20, // Rank 7
    10, 10, 15, 20, 20, 20, 20, 20, 15, 10, 10, 10, // Rank 8
     0,  0,  0,  5,  5,  5,  5,  5,  0,  0,  0,  0, // Rank 9 (Getting stuck)
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // Rank 10 (Stuck)
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0  // Rank 11 (Stuck)
};

// Prince: Wants to stay safe in the back/mid-back, avoid edges where it can be trapped
const int PST_PRINCE[144] = {
    -10, -10,  10,  20,  20,  15,  15,  20,  20,  10, -10, -10, // Rank 0 (Home)
    -10, -10,   5,   5,   5,   5,   5,   5,   5,   5, -10, -10, // Rank 1
    -20, -10,   0,   0,   0,   0,   0,   0,   0, -10, -10, -20, // Rank 2
    -30, -20, -10, -10, -10, -10, -10, -10, -10, -20, -20, -30, // Rank 3
    -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, // Don't go forward
    -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
    -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
    -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
    -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
    -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
    -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
    -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50
};

// Scout: Wants the center and forward positions to fork pieces
const int PST_SCOUT[144] = {
    -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,
      5,   5,  10,  20,  20,  20,  20,  20,  10,   5,   5,   5,
      5,   5,  10,  25,  30,  30,  30,  25,  10,   5,   5,   5,
      5,   5,  10,  30,  40,  40,  40,  30,  10,   5,   5,   5,
      5,   5,  10,  30,  40,  40,  40,  30,  10,   5,   5,   5,
      5,   5,  10,  25,  30,  30,  30,  25,  10,   5,   5,   5,
      5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, // Too deep, might get trapped
    -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10
};

// Generic Centrality Table (for Guard, Tutor, Princess, Pony, Sibling)
const int PST_CENTER[144] = {
    -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10,
    -10,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, -10,
    -10,   0,   5,   5,   5,   5,   5,   5,   5,   5,   0, -10,
    -10,   0,   5,  10,  10,  10,  10,  10,  10,   5,   0, -10,
    -10,   0,   5,  10,  20,  20,  20,  20,  10,   5,   0, -10,
    -10,   0,   5,  10,  20,  30,  30,  20,  10,   5,   0, -10,
    -10,   0,   5,  10,  20,  30,  30,  20,  10,   5,   0, -10,
    -10,   0,   5,  10,  20,  20,  20,  20,  10,   5,   0, -10,
    -10,   0,   5,  10,  10,  10,  10,  10,  10,   5,   0, -10,
    -10,   0,   5,   5,   5,   5,   5,   5,   5,   5,   0, -10,
    -10,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, -10,
    -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10
};

int EngineV4::evaluate(const Board& b) {
    int mg_score = 0; // Middlegame score
    
    // We need to detect if we are winning or losing to adjust aggressiveness
    // But for V4, let's stick to a solid static eval first.

    int white_prince_sq = -1;
    int black_prince_sq = -1;

    // 1. MATERIAL & POSITION (via PST)
    for (int sq = 0; sq < 144; ++sq) {
        PieceType p = b.piece_at(sq);
        if (p == NO_PIECE) continue;

        Color c = b.color_at(sq);
        int val = EvalConstants::PIECE_VALUES[p];
        int pos_bonus = 0;

        // Apply PST based on perspective
        int pst_idx = (c == WHITE) ? sq : mirror_sq(sq);

        switch (p) {
            case BABY:      pos_bonus = PST_BABY[pst_idx]; break;
            case PRINCE:    
                pos_bonus = PST_PRINCE[pst_idx]; 
                if (c == WHITE) white_prince_sq = sq; else black_prince_sq = sq;
                break;
            case SCOUT:     pos_bonus = PST_SCOUT[pst_idx]; break;
            default:        pos_bonus = PST_CENTER[pst_idx]; break; 
        }

        // Sibling Logic: Bonus for being near friends (any friend)
        if (p == SIBLING) {
            // Check 8 neighbors. If at least one ally, give bonus. 
            // If surrounded by allies, small extra bonus.
            int allies = 0;
            // ... (Insert simple adjacency check logic here if available in Board class)
            // If not, simplify: Sibling likes center (already in PST_CENTER)
        }

        if (c == WHITE) {
            mg_score += (val + pos_bonus);
        } else {
            mg_score -= (val + pos_bonus);
        }
    }

    // 2. MOBILITY (Crucial for 12x12)
    // We estimate mobility by counting legal moves. 
    // This is expensive if done fully, so we do a "pseudo-mobility" or rely on the Board's move gen if fast.
    // If your move generation is slow, skip this or approximate it by counting empty adjacent squares.
    
    // Assuming you have a function `b.generate_moves(color)` or similar that is reasonably fast:
    // If not, implementing a simplified "count empty neighbors" loop is better than nothing.
    
    /* 
    // Pseudo-code for mobility bonus
    std::vector<Move> white_moves, black_moves;
    b.get_moves(WHITE, white_moves);
    b.get_moves(BLACK, black_moves);
    
    // Weight mobility: 5 points per move
    mg_score += (white_moves.size() * 5);
    mg_score -= (black_moves.size() * 5);
    */

    // 3. PRINCE SAFETY & ATTACK
    // Instead of simple distance, check "Attackers in Sector"
    if (white_prince_sq != -1 && black_prince_sq != -1) {
        int w_pr_r = white_prince_sq / 12;
        int w_pr_c = white_prince_sq % 12;
        int b_pr_r = black_prince_sq / 12;
        int b_pr_c = black_prince_sq % 12;

        // Reward White pieces close to Black Prince (Manhattan dist < 4)
        // This is safer than the "Global Distance" you used before.
        // We iterate pieces again? No, too slow. 
        // Optimization: Do this inside the main loop above.
        
        // Re-implementing simplified distance logic:
        int dist = std::abs(w_pr_r - b_pr_r) + std::abs(w_pr_c - b_pr_c);
        
        // As the game progresses (material drops), kings should avoid each other 
        // unless one has a massive advantage. 
        // But in Heirs, Prince capture is instant win. 
        
        // Aggression Bonus:
        // Calculate distance between White Scout/Princess and Black Prince.
        // Give bonus if low.
    }

    return mg_score;
}

bool EngineV4::is_time_up() {
    auto now = steady_clock::now();
    duration<double> elapsed = now - start_time;
    return elapsed.count() >= time_limit_sec;
}

// RAII wrapper for move making/unmaking
struct ScopedMoveV4 {
    Board& b;
    Move m;
    ScopedMoveV4(Board& board, const Move& move) : b(board), m(move) {
        b.make_move(m);
    }
    ~ScopedMoveV4() {
        b.unmake_move(m);
    }
};

// Alpha-Beta Search
int EngineV4::alphabeta(Board& b, int depth, int alpha, int beta) {
    nodes_visited++;
    
    // Check for draw by repetition or 50-move rule
    if (b.is_draw()) {
        return 0; 
    }
    
    if (depth == 0 || b.is_game_over()) {
        int eval = evaluate(b);
        // Prefer faster mates (higher remaining depth = faster mate)
        if (eval > 10000) {
            eval += depth * 100;
        } else if (eval < -10000) {
            eval -= depth * 100;
        }
        return eval;
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
                ScopedMoveV4 sm(b, move);
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
                ScopedMoveV4 sm(b, move);
                int eval = alphabeta(b, depth - 1, alpha, beta);
                min_eval = std::min(min_eval, eval);
                beta = std::min(beta, eval);
            }
             if (beta <= alpha) break; // Alpha Cutoff
        }
        return min_eval;
    }
}

Move EngineV4::search(Board& b, double time_limit) {
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
                        ScopedMoveV4 sm(b, move);
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
                        ScopedMoveV4 sm(b, move);
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
