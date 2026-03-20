#include "engine_v3.h"
#include <algorithm>
#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>

using namespace std::chrono;

// VERSION 3 Engine


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

EngineV3::EngineV3() : nodes_visited(0), last_depth(0) {
    for (int i = 0; i < 9; ++i) {
        piece_values[i] = EvalConstants::PIECE_VALUES[i];
    }
}

void EngineV3::set_piece_values(const int values[9]) {
    for (int i = 0; i < 9; ++i) {
        piece_values[i] = values[i];
    }
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
    10,   10,  10,  20,  20,  20,  20,  20,  20,  20,  10,  10, // Rank 0 (Home)
     5,    5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5, // Rank 1
    -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, // Rank 2
    -20, -20, -10, -10, -10, -10, -10, -10, -10, -20, -20, -20, // Rank 3
    -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, // Don't go forward
    -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40,
    -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
    -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
    -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
    -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
    -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
    -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50
};

// Scout: Wants the center and forward positions to fork pieces
const int PST_SCOUT[144] = {
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   0,
      0,   10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  0,
      0,   10,  10,  10,  10,  10,  10,  10,  10,  10,  10,   0,
      0,   10,  10,  10,  10,  10,  10,  10,  10,  10,  10,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
      0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
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

int EngineV3::evaluate(const Board& b) {
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

    // 1. MATERIAL & POSITION
    auto eval_list = [&](const int* list, int count, Color c, int enemy_prince_sq) {
        int mg = 0;
        int eg = 0;
        for (int i = 0; i < count; ++i) {
            int sq = list[i];
            PieceType p = b.piece_at(sq);
            int val = piece_values[p];
            int mg_pos_bonus = 0;
            int eg_pos_bonus = 0;

            int pst_sq = to_12x12(sq);
            int pst_idx = (c == WHITE) ? pst_sq : mirror_sq(pst_sq);

            switch (p) {
                case BABY:
                    mg_pos_bonus = PST_BABY[pst_idx];
                    eg_pos_bonus = PST_BABY_EG[pst_idx];
                    {
                        int rank = pst_idx / 12;
                        if (rank == 11) { mg_pos_bonus -= 100; eg_pos_bonus -= 100; }
                        else if (rank == 10) { mg_pos_bonus -= 90; eg_pos_bonus -= 90; }
                        else if (rank == 9) { mg_pos_bonus -= 50; eg_pos_bonus -= 50; }
                        else if (rank == 8) { mg_pos_bonus -= 20; eg_pos_bonus -= 20; }
                        
                        // Penalty if friendly non-baby piece is blocking it
                        int forward_sq = (c == WHITE) ? (sq + 16) : (sq - 16);
                        int fr = forward_sq >> 4;
                        int fc = forward_sq & 15;
                        if (fr >= 0 && fr < 12 && fc >= 0 && fc < 12) {
                            if (b.color_at(forward_sq) == c && b.piece_at(forward_sq) != BABY) {
                                mg_pos_bonus -= 40; // Penalty for blocked baby
                                eg_pos_bonus -= 40;
                            }
                        }
                    }
                    break;
                case SCOUT:
                    mg_pos_bonus = PST_CENTER[pst_idx]; 
                    eg_pos_bonus = PST_CENTER_EG[pst_idx];
                    {
                        int rank = pst_idx / 12;
                        if (rank >= 7) {
                            int penalty = (rank - 6) * 120; // rank 7: -120 -> rank 11: -600 (Scout value is 600)
                            mg_pos_bonus -= penalty;
                            eg_pos_bonus -= penalty;
                        }
                    }
                    break;
                case PRINCE:    
                    mg_pos_bonus = PST_PRINCE[pst_idx]; 
                    eg_pos_bonus = PST_PRINCE_EG[pst_idx];
                    break;
                default:
                    mg_pos_bonus = PST_CENTER[pst_idx]; 
                    eg_pos_bonus = PST_CENTER_EG[pst_idx];
                    break; 
            }
            // Tropism: pieces should tend toward enemy prince in endgame
            if (p != PRINCE && enemy_prince_sq != -1) {
                int r1 = (sq >> 4) & 15;
                int c1 = sq & 15;
                int r2 = (enemy_prince_sq >> 4) & 15;
                int c2 = enemy_prince_sq & 15;
                int dist = std::abs(r1 - r2) + std::abs(c1 - c2);
                
                // Reward closeness
                eg_pos_bonus += std::max(0, (24 - dist) * 2);
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

void EngineV3::score_and_sort_moves(MoveList& moves, const Board& b, int ply) {
    if (moves.count == 0) return;
    
    std::vector<int> move_scores(moves.count, 0);
    Color side = b.side_to_move();
    
    for (int i = 0; i < moves.count; ++i) {
        Move m = moves.moves[i];
        if (m.captured != NO_PIECE) {
            // Band 1: Captures get a massive score boost so they are searched first
            PieceType attacker = b.piece_at(m.from);
            move_scores[i] = 1000000 + (piece_values[m.captured] * 10) - piece_values[attacker];
        } else {
            // Quiet moves
            if (ply < MAX_PLY && m == killer_moves[ply][0]) {
                move_scores[i] = 900000;
            } else if (ply < MAX_PLY && m == killer_moves[ply][1]) {
                move_scores[i] = 800000;
            } else {
                int hist = history_table[side][m.from][m.to];
                if (hist > 690000) hist = 690000; // Cap safely
                move_scores[i] = hist;
            }
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
}

// Alpha-Beta Search
int EngineV3::alphabeta(Board& b, int depth, int alpha, int beta, int ply) {
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
    
    // Move ordering
    score_and_sort_moves(moves, b, ply);
    
    if (side == WHITE) { // Maximize
        int max_eval = -2000000;
        for (int i = 0; i < moves.count; ++i) {
            Move move = moves.moves[i];
            {
                ScopedMoveV3 sm(b, move);
                int eval = alphabeta(b, depth - 1, alpha, beta, ply + 1);
                max_eval = std::max(max_eval, eval);
                alpha = std::max(alpha, eval);
            }
             if (beta <= alpha) {
                 if (move.captured == NO_PIECE) {
                     history_table[WHITE][move.from][move.to] += depth * depth;
                     if (ply < MAX_PLY && !(killer_moves[ply][0] == move)) {
                         killer_moves[ply][1] = killer_moves[ply][0];
                         killer_moves[ply][0] = move;
                     }
                 }
                 break; // Beta Cutoff
             }
        }
        return max_eval;
    } else { // Minimize
        int min_eval = 2000000;
        for (int i = 0; i < moves.count; ++i) {
            Move move = moves.moves[i];
            {
                ScopedMoveV3 sm(b, move);
                int eval = alphabeta(b, depth - 1, alpha, beta, ply + 1);
                min_eval = std::min(min_eval, eval);
                beta = std::min(beta, eval);
            }
             if (beta <= alpha) {
                 if (move.captured == NO_PIECE) {
                     history_table[BLACK][move.from][move.to] += depth * depth;
                     if (ply < MAX_PLY && !(killer_moves[ply][0] == move)) {
                         killer_moves[ply][1] = killer_moves[ply][0];
                         killer_moves[ply][0] = move;
                     }
                 }
                 break; // Alpha Cutoff
             }
        }
        return min_eval;
    }
}

Move EngineV3::search(Board& b, double time_limit) {
    start_time = steady_clock::now();
    time_limit_sec = time_limit;
    nodes_visited = 0;
    last_depth = 0;
    
    // Clear killers array for this search
    for (int p = 0; p < MAX_PLY; ++p) {
        killer_moves[p][0] = Move();
        killer_moves[p][1] = Move();
    }
    
    // Decay history roughly by half over time
    for (int c = 0; c < 2; ++c) {
        for (int f = 0; f < 256; ++f) {
            for (int t = 0; t < 256; ++t) {
                history_table[c][f][t] /= 2;
            }
        }
    }
    
    Move best_move;
    int max_depth = 100; 
    
    for (int depth = 1; depth <= max_depth; ++depth) {
        last_depth = depth;
        try {
            Color side = b.side_to_move();
            MoveList moves;
            b.generate_moves(moves);
            
            if (moves.count == 0) return Move();
            
            // Move ordering
            score_and_sort_moves(moves, b, 0); // Root is ply 0

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
                        score = alphabeta(b, depth - 1, alpha, beta, 1);
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
                        score = alphabeta(b, depth - 1, alpha, beta, 1);
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
