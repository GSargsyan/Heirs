#include "engine_v1.h"
#include <algorithm>
#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>

using namespace std::chrono;

#include "eval_constants.h"

// VERSION 1 Engine

long long EngineV1::get_nodes_visited() const { return nodes_visited; }
int EngineV1::get_max_depth() const { return last_depth; }

EngineV1::EngineV1() : nodes_visited(0), last_depth(0) {
    for (int i = 0; i < 9; ++i) {
        piece_values[i] = EvalConstants::PIECE_VALUES[i];
    }
}

void EngineV1::set_piece_values(const int values[9]) {
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

int EngineV1::evaluate(const Board& b) {
    int game_phase = b.game_phase;

    int white_prince_sq = b.prince_sq[WHITE];
    int black_prince_sq = b.prince_sq[BLACK];

    // 1. MATERIAL & BASE PST
    int w_mg = b.pst_mg[WHITE];
    int w_eg = b.pst_eg[WHITE];
    int b_mg = b.pst_mg[BLACK];
    int b_eg = b.pst_eg[BLACK];

    for (int p = 1; p < 10; ++p) {
        if (p == OFFBOARD) continue;
        w_mg += b.piece_counts[WHITE][p] * piece_values[p];
        w_eg += b.piece_counts[WHITE][p] * piece_values[p];
        b_mg += b.piece_counts[BLACK][p] * piece_values[p];
        b_eg += b.piece_counts[BLACK][p] * piece_values[p];
    }

    auto eval_list_dyn = [&](const int* list, int count, Color c, int enemy_prince_sq) {
        int dyn_mg = 0;
        int dyn_eg = 0;
        for (int i = 0; i < count; ++i) {
            int sq = list[i];
            PieceType p = b.piece_at(sq);
            int pst_sq = to_12x12(sq);
            int pst_idx = (c == WHITE) ? pst_sq : mirror_sq(pst_sq);

            switch (p) {
                case BABY:
                    {
                        int rank = pst_idx / 12;
                        if (rank == 11) { dyn_mg -= 100; dyn_eg -= 100; }
                        else if (rank == 10) { dyn_mg -= 90; dyn_eg -= 90; }
                        else if (rank == 9) { dyn_mg -= 50; dyn_eg -= 50; }
                        else if (rank == 8) { dyn_mg -= 20; dyn_eg -= 20; }
                        
                        // Penalty if friendly non-baby piece is blocking it
                        int forward_sq = (c == WHITE) ? (sq + 16) : (sq - 16);
                        int fr = forward_sq >> 4;
                        int fc = forward_sq & 15;
                        if (fr >= 0 && fr < 12 && fc >= 0 && fc < 12) {
                            if (b.color_at(forward_sq) == c && b.piece_at(forward_sq) != BABY) {
                                dyn_mg -= 40; 
                                dyn_eg -= 40;
                            }
                        }
                    }
                    break;
                case SCOUT:
                    {
                        int rank = pst_idx / 12;
                        if (rank >= 7) {
                            int penalty = (rank - 6) * 120;
                            dyn_mg -= penalty;
                            dyn_eg -= penalty;
                        }
                    }
                    break;
                default:
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
                dyn_eg += std::max(0, (24 - dist) * 2);
            }
        }
        return std::make_pair(dyn_mg, dyn_eg);
    };

    auto w_dyn = eval_list_dyn(b.get_white_pieces(), b.get_white_piece_count(), WHITE, black_prince_sq);
    auto b_dyn = eval_list_dyn(b.get_black_pieces(), b.get_black_piece_count(), BLACK, white_prince_sq);

    w_mg += w_dyn.first;
    w_eg += w_dyn.second;
    b_mg += b_dyn.first;
    b_eg += b_dyn.second;

    int mg_score = w_mg - b_mg;
    int eg_score = w_eg - b_eg;

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
    int phase = std::min(game_phase, EvalConstants::MAX_PHASE);
    int final_score = (mg_score * phase + eg_score * (EvalConstants::MAX_PHASE - phase)) / EvalConstants::MAX_PHASE;

    return final_score;
}

bool EngineV1::is_time_up() {
    auto now = steady_clock::now();
    duration<double> elapsed = now - start_time;
    return elapsed.count() >= time_limit_sec;
}

// RAII wrapper for move making/unmaking
struct ScopedMoveV1 {
    Board& b;
    Move m;
    ScopedMoveV1(Board& board, const Move& move) : b(board), m(move) {
        b.make_move(m);
    }
    ~ScopedMoveV1() {
        b.unmake_move(m);
    }
};

void EngineV1::score_moves(const MoveList& moves, int* move_scores, const Board& b, int ply) {
    if (moves.count == 0) return;
    
    Color side = b.side_to_move();
    
    for (int i = 0; i < moves.count; ++i) {
        Move m = moves.moves[i];
        PieceType attacker = b.piece_at(m.from);
        
        if (m.captured != NO_PIECE) {
            // Band 1: Captures get a massive score boost so they are searched first
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
        
        // Band 0: Scout moves take absolute priority
        if (attacker == SCOUT) {
            move_scores[i] += 5000000;
        }
    }
}

// Alpha-Beta Search
int EngineV1::alphabeta(Board& b, int depth, int alpha, int beta, int ply) {
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
    
    // Move scoring
    int move_scores[256];
    score_moves(moves, move_scores, b, ply);
    
    if (side == WHITE) { // Maximize
        int max_eval = -2000000;
        for (int i = 0; i < moves.count; ++i) {
            // Lazy sorting inline
            int best_idx = i;
            for (int j = i + 1; j < moves.count; ++j) {
                if (move_scores[j] > move_scores[best_idx]) best_idx = j;
            }
            std::swap(move_scores[i], move_scores[best_idx]);
            std::swap(moves.moves[i], moves.moves[best_idx]);
            
            Move move = moves.moves[i];
            {
                ScopedMoveV1 sm(b, move);
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
            // Lazy sorting inline
            int best_idx = i;
            for (int j = i + 1; j < moves.count; ++j) {
                if (move_scores[j] > move_scores[best_idx]) best_idx = j;
            }
            std::swap(move_scores[i], move_scores[best_idx]);
            std::swap(moves.moves[i], moves.moves[best_idx]);
            
            Move move = moves.moves[i];
            {
                ScopedMoveV1 sm(b, move);
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

Move EngineV1::search(Board& b, double time_limit) {
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
            
            // Move scoring
            int move_scores[256];
            score_moves(moves, move_scores, b, 0); // Root is ply 0

            Move current_best_move;
            int best_score;
            int alpha = -2000000;
            int beta = 2000000;
            
            if (side == WHITE) {
                best_score = -2000000;
                for (int i = 0; i < moves.count; ++i) {
                    // Lazy sorting inline
                    int best_idx = i;
                    for (int j = i + 1; j < moves.count; ++j) {
                        if (move_scores[j] > move_scores[best_idx]) best_idx = j;
                    }
                    std::swap(move_scores[i], move_scores[best_idx]);
                    std::swap(moves.moves[i], moves.moves[best_idx]);
                    
                    Move move = moves.moves[i];
                    int score;
                    {
                        ScopedMoveV1 sm(b, move);
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
                    // Lazy sorting inline
                    int best_idx = i;
                    for (int j = i + 1; j < moves.count; ++j) {
                        if (move_scores[j] > move_scores[best_idx]) best_idx = j;
                    }
                    std::swap(move_scores[i], move_scores[best_idx]);
                    std::swap(moves.moves[i], moves.moves[best_idx]);
                    
                    Move move = moves.moves[i];
                    int score;
                    {
                        ScopedMoveV1 sm(b, move);
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
