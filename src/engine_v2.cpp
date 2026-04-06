#include "engine_v2.h"
#include <algorithm>
#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>

using namespace std::chrono;

#include "eval_constants.h"

long long EngineV2::get_nodes_visited() const { return nodes_visited; }
int EngineV2::get_max_depth() const { return last_depth; }

EngineV2::EngineV2() : nodes_visited(0), last_depth(0), tt(16) {
    for (int i = 0; i < 9; ++i) {
        piece_values[i] = EvalConstants::PIECE_VALUES[i];
    }
}

void EngineV2::set_piece_values(const int values[9]) {
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
static const int MIRROR_SQ[144] = {
    132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143,
    120, 121, 122, 123, 124, 125, 126, 127, 128, 129, 130, 131,
    108, 109, 110, 111, 112, 113, 114, 115, 116, 117, 118, 119,
     96,  97,  98,  99, 100, 101, 102, 103, 104, 105, 106, 107,
     84,  85,  86,  87,  88,  89,  90,  91,  92,  93,  94,  95,
     72,  73,  74,  75,  76,  77,  78,  79,  80,  81,  82,  83,
     60,  61,  62,  63,  64,  65,  66,  67,  68,  69,  70,  71,
     48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,
     36,  37,  38,  39,  40,  41,  42,  43,  44,  45,  46,  47,
     24,  25,  26,  27,  28,  29,  30,  31,  32,  33,  34,  35,
     12,  13,  14,  15,  16,  17,  18,  19,  20,  21,  22,  23,
      0,   1,   2,   3,   4,   5,   6,   7,   8,   9,  10,  11
};

int EngineV2::evaluate(const Board& b) {
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
            int pst_idx = (c == WHITE) ? pst_sq : MIRROR_SQ[pst_sq];

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
                        int scout_val = piece_values[SCOUT];
                        int penalty = 0;
                        if (rank == 11) penalty = scout_val;
                        else if (rank == 10) penalty = (scout_val * 90) / 100;
                        else if (rank == 9) penalty = (scout_val * 75) / 100;
                        else if (rank == 8) penalty = (scout_val * 50) / 100;
                        else if (rank == 7) penalty = (scout_val * 25) / 100;
                        else if (rank == 6 || rank == 5) penalty = (scout_val * 10) / 100;

                        dyn_mg -= penalty;
                        dyn_eg -= penalty;
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

    // --- EARLY GAME DEVELOPMENT PENALTY ---
    if (game_phase > 200) {
        if (b.piece_at(5) != PRINCESS) {
            int w_undeveloped = 0;
            if (b.piece_at(1) == PONY) w_undeveloped++;
            if (b.piece_at(2) == TUTOR) w_undeveloped++;
            if (b.piece_at(4) == SIBLING) w_undeveloped++;
            if (b.piece_at(7) == SIBLING) w_undeveloped++;
            if (b.piece_at(9) == TUTOR) w_undeveloped++;
            if (b.piece_at(10) == PONY) w_undeveloped++;
            mg_score -= w_undeveloped * 30;
        }
        
        if (b.piece_at(181) != PRINCESS) {
            int b_undeveloped = 0;
            if (b.piece_at(177) == PONY) b_undeveloped++;
            if (b.piece_at(178) == TUTOR) b_undeveloped++;
            if (b.piece_at(180) == SIBLING) b_undeveloped++;
            if (b.piece_at(183) == SIBLING) b_undeveloped++;
            if (b.piece_at(185) == TUTOR) b_undeveloped++;
            if (b.piece_at(186) == PONY) b_undeveloped++;
            mg_score += b_undeveloped * 30; // penalize black
        }
        
        // --- SPECIFIC OPENING BONUSES ---
        /*
        int opening_bonus_w = 0;
        if (b.piece_at(52) == BABY && b.color_at(52) == WHITE) opening_bonus_w += 30;
        if (b.piece_at(36) == SCOUT && b.color_at(36) == WHITE) opening_bonus_w += 30;
        if (b.piece_at(55) == BABY && b.color_at(55) == WHITE) opening_bonus_w += 30;
        if (b.piece_at(39) == SCOUT && b.color_at(39) == WHITE) opening_bonus_w += 30;
        
        if (b.piece_at(52) == BABY && b.color_at(52) == WHITE && b.piece_at(36) == SCOUT && b.color_at(36) == WHITE) opening_bonus_w += 20;
        if (b.piece_at(55) == BABY && b.color_at(55) == WHITE && b.piece_at(39) == SCOUT && b.color_at(39) == WHITE) opening_bonus_w += 20;
        
        mg_score += opening_bonus_w;

        int opening_bonus_b = 0;
        if (b.piece_at(132) == BABY && b.color_at(132) == BLACK) opening_bonus_b += 30;
        if (b.piece_at(148) == SCOUT && b.color_at(148) == BLACK) opening_bonus_b += 30;
        if (b.piece_at(135) == BABY && b.color_at(135) == BLACK) opening_bonus_b += 30;
        if (b.piece_at(151) == SCOUT && b.color_at(151) == BLACK) opening_bonus_b += 30;

        if (b.piece_at(132) == BABY && b.color_at(132) == BLACK && b.piece_at(148) == SCOUT && b.color_at(148) == BLACK) opening_bonus_b += 20;
        if (b.piece_at(135) == BABY && b.color_at(135) == BLACK && b.piece_at(151) == SCOUT && b.color_at(151) == BLACK) opening_bonus_b += 20;

        mg_score -= opening_bonus_b;
        */
    }

    // --- MOP-UP EVALUATION (Force Checkmate) ---
    int winning_threshold = 400;

    if (eg_score > winning_threshold && game_phase < 150) {
        int w_prince_r = (white_prince_sq >> 4) & 15;
        int w_prince_c = white_prince_sq & 15;
        int b_prince_r = (black_prince_sq >> 4) & 15;
        int b_prince_c = black_prince_sq & 15;

        int center_dist = std::abs(b_prince_r - 5) + std::abs(b_prince_c - 5) + 
                          std::abs(b_prince_r - 6) + std::abs(b_prince_c - 6);
        eg_score += center_dist * 20;

        int princes_dist = std::abs(w_prince_r - b_prince_r) + std::abs(w_prince_c - b_prince_c);
        eg_score += (24 - princes_dist) * 10;
    }
    else if (eg_score < -winning_threshold && game_phase < 150) {
        int w_prince_r = (white_prince_sq >> 4) & 15;
        int w_prince_c = white_prince_sq & 15;
        int b_prince_r = (black_prince_sq >> 4) & 15;
        int b_prince_c = black_prince_sq & 15;

        int center_dist = std::abs(w_prince_r - 5) + std::abs(w_prince_c - 5) + 
                          std::abs(w_prince_r - 6) + std::abs(w_prince_c - 6);
        eg_score -= center_dist * 20;

        int princes_dist = std::abs(b_prince_r - w_prince_r) + std::abs(b_prince_c - w_prince_c);
        eg_score -= (24 - princes_dist) * 10;
    }

    // 3. TAPERED BLEND
    int phase = std::min(game_phase, EvalConstants::MAX_PHASE);
    int final_score = (mg_score * phase + eg_score * (EvalConstants::MAX_PHASE - phase)) / EvalConstants::MAX_PHASE;

    return (b.side_to_move() == WHITE) ? final_score : -final_score;
}

bool EngineV2::is_time_up() {
    if (time_limit_sec <= 0.0) return false;
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

struct ScopedNullMoveV2 {
    Board& b;
    ScopedNullMoveV2(Board& board) : b(board) {
        b.make_null_move();
    }
    ~ScopedNullMoveV2() {
        b.unmake_null_move();
    }
};

void EngineV2::score_moves(const MoveList& moves, int* move_scores, const Board& b, int ply, Move tt_move) {
    if (moves.count == 0) return;
    
    Color side = b.side_to_move();
    
    for (int i = 0; i < moves.count; ++i) {
        Move m = moves.moves[i];
        
        if (m == tt_move) {
            move_scores[i] = 10000000;
            continue;
        }

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
    }
}



// Alpha-Beta Search
int EngineV2::alphabeta(Board& b, int depth, int alpha, int beta, int ply) {
    nodes_visited++;
    
    // Check for draw by repetition or 50-move rule
    // We must check this BEFORE the TT probe, because a position reached by 
    // a cyclic path is a draw (score 0), whereas the same position reached 
    // from a standard path might be evaluated and stored in the TT as winning (+x).
    if (ply > 0 && b.is_draw(1)) {
        return 0; // Returning 0 on 1-fold repetition strictly avoids cycles
    }

    int original_alpha = alpha;
    TTEntryV2 tt_entry;
    Move tt_move;
    if (tt.probe(b.get_hash(), tt_entry)) {
        tt_move = tt_entry.best_move;
        if (tt_entry.depth >= depth) {
            if (tt_entry.bound == BOUND_EXACT_V2)
                return tt_entry.score;
            if (tt_entry.bound == BOUND_LOWER_V2)
                alpha = std::max(alpha, tt_entry.score);
            else if (tt_entry.bound == BOUND_UPPER_V2)
                beta = std::min(beta, tt_entry.score);
            if (alpha >= beta)
                return tt_entry.score;
        }
    }
    
    if (depth == 0) {
        return quiescence(b, alpha, beta, ply);
    }

    if (b.is_game_over()) {
        int eval = evaluate(b);
        // Prefer faster mates (higher remaining depth = faster mate)
        // With relative scoring, being materially winning guarantees +score > 10000
        if (eval > 10000) {
            eval += depth * 100;
        } else if (eval < -10000) {
            eval -= depth * 100;
        }
        return eval;
    }

    // Null Move Pruning
    if (depth >= 3 && b.game_phase > 100) {
        int stand_pat = evaluate(b);
        if (stand_pat >= beta) {
            int R = 2; // Fixed depth reduction
            int null_score;
            {
                ScopedNullMoveV2 snm(b);
                null_score = -alphabeta(b, depth - 1 - R, -beta, -beta + 1, ply + 1);
            }
            
            if (null_score >= beta) {
                return beta; // Fail-high, skip move generation
            }
        }
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
    
    // Futility Pruning Setup
    bool futil_prune = false;
    int futil_stand_pat = -2000000;
    if (depth == 1 && !b.is_game_over() && !b.is_draw()) {
        futil_stand_pat = evaluate(b);
        int margin = piece_values[GUARD];
        if (futil_stand_pat + margin < alpha) {
            futil_prune = true;
        }
    }

    // Move scoring
    int move_scores[256];
    score_moves(moves, move_scores, b, ply, tt_move);
    
    // Safe initialization bounding in case all moves are verified futile
    int max_eval = futil_prune ? futil_stand_pat : -2000000;
    Move best_move;
    for (int i = 0; i < moves.count; ++i) {
        // Lazy sorting inline
        int best_idx = i;
        for (int j = i + 1; j < moves.count; ++j) {
            if (move_scores[j] > move_scores[best_idx]) best_idx = j;
        }
        std::swap(move_scores[i], move_scores[best_idx]);
        std::swap(moves.moves[i], moves.moves[best_idx]);
        
        Move move = moves.moves[i];
        
        // Futility Pruning: purely skip unpromising quiet moves
        if (futil_prune && move.captured == NO_PIECE) {
            continue;
        }

        int eval;
        {
            ScopedMoveV2 sm(b, move);
            if (i == 0) {
                // Full window search for first move (PV node)
                eval = -alphabeta(b, depth - 1, -beta, -alpha, ply + 1);
            } else {
                bool is_capture = (move.captured != NO_PIECE);
                bool is_killer = (ply < MAX_PLY && (move == killer_moves[ply][0] || move == killer_moves[ply][1]));
                bool do_lmr = (i >= 4 && depth >= 3 && !is_capture && !is_killer);

                if (do_lmr) {
                    // LMR zero-window search
                    eval = -alphabeta(b, depth - 2, -alpha - 1, -alpha, ply + 1);
                    if (eval > alpha) {
                        // Failed high, needs standard depth zero-window search
                        eval = -alphabeta(b, depth - 1, -alpha - 1, -alpha, ply + 1);
                    }
                } else {
                    // Standard depth zero-window search (null-window)
                    eval = -alphabeta(b, depth - 1, -alpha - 1, -alpha, ply + 1);
                }
                
                if (eval > alpha && eval < beta) {
                    // Full-window re-search if zero-window failed high
                    eval = -alphabeta(b, depth - 1, -beta, -alpha, ply + 1);
                }
            }
        }
        
        if (eval > max_eval) {
            max_eval = eval;
            best_move = move;
        }
        alpha = std::max(alpha, eval);
        if (alpha >= beta) {
            if (move.captured == NO_PIECE) {
                history_table[side][move.from][move.to] += depth * depth;
                if (ply < MAX_PLY && !(killer_moves[ply][0] == move)) {
                    killer_moves[ply][1] = killer_moves[ply][0];
                    killer_moves[ply][0] = move;
                }
            }
            break; // Beta Cutoff
        }
    }
    
    TTBoundV2 bound;
    if (max_eval <= original_alpha) bound = BOUND_UPPER_V2;
    else if (max_eval >= beta) bound = BOUND_LOWER_V2;
    else bound = BOUND_EXACT_V2;
    tt.store(b.get_hash(), depth, max_eval, bound, best_move);
    
    return max_eval;
}

// Quiescence Search
int EngineV2::quiescence(Board& b, int alpha, int beta, int ply) {
    nodes_visited++;
    
    if ((nodes_visited & 1023) == 0) {
        if (is_time_up()) throw std::runtime_error("Time limit exceeded");
    }

    if (b.is_draw(1)) return 0; // 1-fold repetition strictly avoids cycles inside search

    int stand_pat = evaluate(b);

    if (stand_pat >= beta) return beta;
    if (alpha < stand_pat) alpha = stand_pat;

    if (b.is_game_over()) {
        return stand_pat;
    }

    MoveList moves;
    b.generate_moves(moves);
    

    if (moves.count == 0) return stand_pat;

    int capture_count = 0;
    Move captures[256];
    int capture_scores[256];
    
    for (int i = 0; i < moves.count; ++i) {
        Move m = moves.moves[i];
        if (m.captured != NO_PIECE) {
            captures[capture_count] = m;
            PieceType attacker = b.piece_at(m.from);
            // Material difference heuristic for QS move ordering
            capture_scores[capture_count] = 1000000 + (piece_values[m.captured] * 10) - piece_values[attacker];
            capture_count++;
        }
    }

    int max_eval = stand_pat;
    for (int i = 0; i < capture_count; ++i) {
        int best_idx = i;
        for (int j = i + 1; j < capture_count; ++j) {
            if (capture_scores[j] > capture_scores[best_idx]) best_idx = j;
        }
        std::swap(capture_scores[i], capture_scores[best_idx]);
        std::swap(captures[i], captures[best_idx]);
        
        Move move = captures[i];
        


        int eval;
        {
            ScopedMoveV2 sm(b, move);
            eval = -quiescence(b, -beta, -alpha, ply + 1);
        }
        max_eval = std::max(max_eval, eval);
        alpha = std::max(alpha, eval);
        
        if (alpha >= beta) break; // Beta Cutoff
    }
    return max_eval;
}

Move EngineV2::search(Board& b, double time_limit, int max_depth) {
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
    
    for (int depth = 1; depth <= max_depth; ++depth) {
        last_depth = depth;
        try {
            Color side = b.side_to_move();
            MoveList moves;
            b.generate_moves(moves);
            

            
            if (moves.count == 0) return Move();
            
            // Move scoring
            int move_scores[256];
            Move tt_move;
            TTEntryV2 tt_entry;
            if (tt.probe(b.get_hash(), tt_entry)) {
                tt_move = tt_entry.best_move;
            }
            score_moves(moves, move_scores, b, 0, tt_move); // Root is ply 0

            Move current_best_move;
            int best_score;
            int alpha = -2000000;
            int beta = 2000000;
            
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
                    ScopedMoveV2 sm(b, move);
                    if (i == 0) {
                        score = -alphabeta(b, depth - 1, -beta, -alpha, 1);
                    } else {
                        bool is_capture = (move.captured != NO_PIECE);
                        bool is_killer = (move == killer_moves[0][0] || move == killer_moves[0][1]);
                        bool do_lmr = (i >= 4 && depth >= 3 && !is_capture && !is_killer);

                        if (do_lmr) {
                            score = -alphabeta(b, depth - 2, -alpha - 1, -alpha, 1);
                            if (score > alpha) {
                                score = -alphabeta(b, depth - 1, -alpha - 1, -alpha, 1);
                            }
                        } else {
                            score = -alphabeta(b, depth - 1, -alpha - 1, -alpha, 1);
                        }
                        
                        if (score > alpha && score < beta) {
                            score = -alphabeta(b, depth - 1, -beta, -alpha, 1);
                        }
                    }
                }
                
                if (score > best_score) {
                    best_score = score;
                    current_best_move = move;
                }
                alpha = std::max(alpha, best_score); // Update alpha at root
                
                if (is_time_up()) throw std::runtime_error("Time limit");
            }
            
            best_move = current_best_move;
            
            if (is_time_up()) break;
            
        } catch (const std::exception& e) {
            break;
        }
    }
    
    return best_move;
}
