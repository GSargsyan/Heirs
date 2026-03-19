#include "engine_v3.h"
#include <algorithm>
#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>

using namespace std::chrono;

extern bool DEBUG_EVAL_V3;
bool DEBUG_EVAL_V3 = false;

// VERSION 3 Engine
EngineV3::EngineV3() : nodes_visited(0), last_depth(0) {}

long long EngineV3::get_nodes_visited() const { return nodes_visited; }
int EngineV3::get_max_depth() const { return last_depth; }

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

inline int to_12x12(int sq) {
    int r = sq >> 4;
    int c = sq & 15;
    return r * 12 + c;
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
    15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, // Rank 3
    20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, // Rank 4
    25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, // Rank 5
    25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, // Rank 6
    10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, // Rank 7
     5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5, // Rank 8 (Getting stuck)
     0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0, // Rank 9 (Stuck)
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
/*
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
*/

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

// Phase Constants and tables
inline int get_piece_phase(PieceType p) {
    switch(p) {
        case BABY: return 1;
        case PRINCE: return 0;
        case PONY: return 5;
        case SIBLING: return 8;
        case TUTOR: return 10;
        case GUARD: return 12;
        case SCOUT: return 20;
        case PRINCESS: return 40;
        default: return 0;
    }
}

const int MAX_PHASE = 324;

const int PST_BABY_EG[144] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

const int PST_PRINCE_EG[144] = {
    -50, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -50,
    -30, -10,   0,   0,   0,   0,   0,   0,   0,   0, -10, -30,
    -30,   0,  10,  10,  10,  10,  10,  10,  10,  10,   0, -30,
    -30,   0,  10,  20,  20,  20,  20,  20,  20,  10,   0, -30,
    -30,   0,  10,  20,  30,  30,  30,  30,  20,  10,   0, -30,
    -30,   0,  10,  20,  30,  40,  40,  30,  20,  10,   0, -30,
    -30,   0,  10,  20,  30,  40,  40,  30,  20,  10,   0, -30,
    -30,   0,  10,  20,  30,  30,  30,  30,  20,  10,   0, -30,
    -30,   0,  10,  20,  20,  20,  20,  20,  20,  10,   0, -30,
    -30,   0,  10,  10,  10,  10,  10,  10,  10,  10,   0, -30,
    -30, -10,   0,   0,   0,   0,   0,   0,   0,   0, -10, -30,
    -50, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -50
};

const int PST_CENTER_EG[144] = {
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

int EngineV3::evaluate(const Board& b, EvalComponents* debug_info) {
    int mg_score = 0; // Middlegame score
    int eg_score = 0; // Endgame score
    int game_phase = 0;

    int white_prince_sq = -1;
    int black_prince_sq = -1;

    // Calculate phase first
    for (int i = 0; i < b.get_white_piece_count(); ++i) {
        game_phase += get_piece_phase(b.piece_at(b.get_white_pieces()[i]));
    }
    for (int i = 0; i < b.get_black_piece_count(); ++i) {
        game_phase += get_piece_phase(b.piece_at(b.get_black_pieces()[i]));
    }
    
    // Find princes first for tropism
    for (int i = 0; i < b.get_white_piece_count(); ++i) {
        if (b.piece_at(b.get_white_pieces()[i]) == PRINCE) white_prince_sq = b.get_white_pieces()[i];
    }
    for (int i = 0; i < b.get_black_piece_count(); ++i) {
        if (b.piece_at(b.get_black_pieces()[i]) == PRINCE) black_prince_sq = b.get_black_pieces()[i];
    }

    int mg_mat = 0, eg_mat = 0;
    int mg_pst_baby = 0, eg_pst_baby = 0;
    int mg_pst_prince = 0, eg_pst_prince = 0;
    int mg_pst_center = 0, eg_pst_center = 0;
    int eg_tropism = 0;

    // 1. MATERIAL & POSITION
    auto eval_list = [&](const int* list, int count, Color c, int enemy_prince_sq) {
        int mg = 0;
        int eg = 0;
        for (int i = 0; i < count; ++i) {
            int sq = list[i];
            PieceType p = b.piece_at(sq);
            int val = EvalConstants::PIECE_VALUES[p];
            int mg_pos_bonus = 0;
            int eg_pos_bonus = 0;

            int pst_sq = to_12x12(sq);
            int pst_idx = (c == WHITE) ? pst_sq : mirror_sq(pst_sq);

            int t_mg_pst_baby = 0, t_eg_pst_baby = 0;
            int t_mg_pst_prince = 0, t_eg_pst_prince = 0;
            int t_mg_pst_center = 0, t_eg_pst_center = 0;

            switch (p) {
                case BABY:
                    mg_pos_bonus = PST_BABY[pst_idx];
                    eg_pos_bonus = PST_BABY_EG[pst_idx];
                    t_mg_pst_baby = mg_pos_bonus;
                    t_eg_pst_baby = eg_pos_bonus;
                    break;
                case PRINCE:    
                    mg_pos_bonus = PST_PRINCE[pst_idx]; 
                    eg_pos_bonus = PST_PRINCE_EG[pst_idx];
                    t_mg_pst_prince = mg_pos_bonus;
                    t_eg_pst_prince = eg_pos_bonus;
                    break;
                default:
                    mg_pos_bonus = PST_CENTER[pst_idx]; 
                    eg_pos_bonus = PST_CENTER_EG[pst_idx];
                    t_mg_pst_center = mg_pos_bonus;
                    t_eg_pst_center = eg_pos_bonus;
                    break; 
            }

            int t_tropism = 0;
            // Tropism: pieces should tend toward enemy prince in endgame
            if (p != PRINCE && enemy_prince_sq != -1) {
                int r1 = (sq >> 4) & 15;
                int c1 = sq & 15;
                int r2 = (enemy_prince_sq >> 4) & 15;
                int c2 = enemy_prince_sq & 15;
                int dist = std::abs(r1 - r2) + std::abs(c1 - c2);
                
                // Reward closeness
                t_tropism = std::max(0, (24 - dist) * 2);
                eg_pos_bonus += t_tropism;
            }

            if (debug_info) {
                if (c == WHITE) {
                    mg_mat += val; eg_mat += val;
                    mg_pst_baby += t_mg_pst_baby; eg_pst_baby += t_eg_pst_baby;
                    mg_pst_prince += t_mg_pst_prince; eg_pst_prince += t_eg_pst_prince;
                    mg_pst_center += t_mg_pst_center; eg_pst_center += t_eg_pst_center;
                    eg_tropism += t_tropism;
                } else {
                    mg_mat -= val; eg_mat -= val;
                    mg_pst_baby -= t_mg_pst_baby; eg_pst_baby -= t_eg_pst_baby;
                    mg_pst_prince -= t_mg_pst_prince; eg_pst_prince -= t_eg_pst_prince;
                    mg_pst_center -= t_mg_pst_center; eg_pst_center -= t_eg_pst_center;
                    eg_tropism -= t_tropism;
                }
            }

            mg += (val + mg_pos_bonus);
            eg += (val + eg_pos_bonus);
        }
        return std::make_pair(mg, eg);
    };

    auto w_scores = eval_list(b.get_white_pieces(), b.get_white_piece_count(), WHITE, black_prince_sq);
    auto b_scores = eval_list(b.get_black_pieces(), b.get_black_piece_count(), BLACK, white_prince_sq);

    mg_score = w_scores.first - b_scores.first;
    eg_score = w_scores.second - b_scores.second;

    // 2. ENEMY PRINCE MOBILITY TROPISM
    // Higher empty sqcount = more mobility
    auto count_empty_neighbors = [&](int sq) {
        if (sq == -1) return 0;
        int empty_count = 0;
        int r = (sq >> 4) & 15;
        int c = sq & 15;
        int dr[] = {-1, -1, -1, 0, 0, 1, 1, 1};
        int dc[] = {-1, 0, 1, -1, 1, -1, 0, 1};
        for (int i = 0; i < 8; ++i) {
            int nr = r + dr[i];
            int nc = c + dc[i];
            if (nr >= 0 && nr < 12 && nc >= 0 && nc < 12) {
                int nsq = (nr << 4) | nc;
                if (b.piece_at(nsq) == NO_PIECE) empty_count++;
            }
        }
        return empty_count;
    };

    int w_prince_moves = count_empty_neighbors(white_prince_sq);
    int b_prince_moves = count_empty_neighbors(black_prince_sq);
    
    // Less mobility for enemy prince is better for us
    // So give us points if our prince has more mobility, and penalize if enemy has more mobility.
    eg_score += w_prince_moves * 10;
    eg_score -= b_prince_moves * 10;

    // 3. TAPERED BLEND
    int phase = std::min(game_phase, MAX_PHASE);
    int final_score = (mg_score * phase + eg_score * (MAX_PHASE - phase)) / MAX_PHASE;

    if (debug_info) {
        debug_info->mg_mat = mg_mat;
        debug_info->eg_mat = eg_mat;
        debug_info->mg_pst_baby = mg_pst_baby;
        debug_info->eg_pst_baby = eg_pst_baby;
        debug_info->mg_pst_prince = mg_pst_prince;
        debug_info->eg_pst_prince = eg_pst_prince;
        debug_info->mg_pst_center = mg_pst_center;
        debug_info->eg_pst_center = eg_pst_center;
        debug_info->eg_tropism = eg_tropism;
        debug_info->w_prince_mobility_score = w_prince_moves * 10;
        debug_info->b_prince_mobility_score = -(b_prince_moves * 10);
        debug_info->phase = phase;
        debug_info->mg_total = mg_score;
        debug_info->eg_total = eg_score;
        debug_info->final_total = final_score;
    }

    return final_score;
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
    MoveList moves;
    b.generate_moves(moves);
    
    if (moves.count == 0) {
        return 0; // Draw
    }
    
    // Move ordering: MVV-LVA (Most Valuable Victim - Least Valuable Attacker)
    std::vector<int> move_scores(moves.count, 0);
    for (int i = 0; i < moves.count; ++i) {
        PieceType captured = moves.moves[i].captured;
        if (captured != NO_PIECE) {
            PieceType attacker = b.piece_at(moves.moves[i].from);
            // Captures get a massive score boost so they are searched first
            move_scores[i] = 1000000 + (EvalConstants::PIECE_VALUES[captured] * 10) - EvalConstants::PIECE_VALUES[attacker];
        } else {
            // Quiet moves (non-captures) get a score of 0 for now
            move_scores[i] = 0;
        }
    }

    // Sort moves based on scores (Descending)
    for (int i = 0; i < moves.count - 1; ++i) {
        int best_idx = i;
        for (int j = i + 1; j < moves.count; ++j) {
            if (move_scores[j] > move_scores[best_idx]) {
                best_idx = j;
            }
        }
        std::swap(move_scores[i], move_scores[best_idx]);
        std::swap(moves.moves[i], moves.moves[best_idx]);
    }
    
    if (side == WHITE) { // Maximize
        int max_eval = -2000000;
        for (int i = 0; i < moves.count; ++i) {
            Move move = moves.moves[i];
            {
                ScopedMoveV3 sm(b, move);
                int eval = alphabeta(b, depth - 1, alpha, beta);
                max_eval = std::max(max_eval, eval);
                alpha = std::max(alpha, eval);
            }
             if (beta <= alpha) break; // Beta Cutoff
        }
        return max_eval;
    } else { // Minimize
        int min_eval = 2000000;
        for (int i = 0; i < moves.count; ++i) {
            Move move = moves.moves[i];
            {
                ScopedMoveV3 sm(b, move);
                int eval = alphabeta(b, depth - 1, alpha, beta);
                min_eval = std::min(min_eval, eval);
                beta = std::min(beta, eval);
            }
             if (beta <= alpha) break; // Alpha Cutoff
        }
        return min_eval;
    }
}

Move EngineV3::search(Board& b, double time_limit) {
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
            MoveList moves;
            b.generate_moves(moves);
            
            if (moves.count == 0) return Move();
            
            // Move ordering: MVV-LVA (Most Valuable Victim - Least Valuable Attacker)
            std::vector<int> move_scores(moves.count, 0);
            for (int i = 0; i < moves.count; ++i) {
                PieceType captured = moves.moves[i].captured;
                if (captured != NO_PIECE) {
                    PieceType attacker = b.piece_at(moves.moves[i].from);
                    move_scores[i] = 1000000 + (EvalConstants::PIECE_VALUES[captured] * 10) - EvalConstants::PIECE_VALUES[attacker];
                } else {
                    move_scores[i] = 0;
                }
            }

            // Sort moves based on scores (Descending)
            for (int i = 0; i < moves.count - 1; ++i) {
                int best_idx = i;
                for (int j = i + 1; j < moves.count; ++j) {
                    if (move_scores[j] > move_scores[best_idx]) {
                        best_idx = j;
                    }
                }
                std::swap(move_scores[i], move_scores[best_idx]);
                std::swap(moves.moves[i], moves.moves[best_idx]);
            }

            Move current_best_move;
            int best_score;
            int alpha = -2000000;
            int beta = 2000000;
            
            if (side == WHITE) {
                best_score = -2000000;
                for (int i = 0; i < moves.count; ++i) {
                    Move move = moves.moves[i];
                    int score;
                    {
                        ScopedMoveV3 sm(b, move);
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
                for (int i = 0; i < moves.count; ++i) {
                    Move move = moves.moves[i];
                    int score;
                    {
                        ScopedMoveV3 sm(b, move);
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
    
    if (DEBUG_EVAL_V3 && !(best_move.from == 0 && best_move.to == 0)) {
        EvalComponents before, after;
        evaluate(b, &before);
        {
            ScopedMoveV3 sm(b, best_move);
            evaluate(b, &after);
        }
        EvalComponents delta = b.side_to_move() == WHITE ? (after - before) : (before - after);
        
        std::cout << "[DEBUG] Move format chosen: from sq " << best_move.from << " to sq " << best_move.to << std::endl;
        std::cout << "[DEBUG] Components gained (+ means good for us): "
                  << "Total=" << delta.final_total << ", "
                  << "Mat=" << delta.mg_mat << ", "
                  << "Baby=" << delta.mg_pst_baby << "/" << delta.eg_pst_baby << ", "
                  << "Center=" << delta.mg_pst_center << ", "
                  << "Prince=" << delta.mg_pst_prince << ", "
                  << "Tropism=" << delta.eg_tropism << ", "
                  << "OurPrinceMob=" << (b.side_to_move() == WHITE ? delta.w_prince_mobility_score : delta.b_prince_mobility_score) << ", "
                  << "EnemyPrinceMob=" << (b.side_to_move() == WHITE ? delta.b_prince_mobility_score : delta.w_prince_mobility_score)
                  << " | Phase=" << after.phase << "/" << MAX_PHASE << std::endl;
    }

    return best_move;
}
