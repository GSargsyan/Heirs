#include "engine_v7.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>

using namespace std::chrono;

EngineV7::EngineV7() : nodes_visited(0) {
    load_weights("weights.bin");
    flat_buffer.resize(32 * 144);
    pi_out_buffer.resize(7056);

}

long long EngineV7::get_nodes_visited() const { return nodes_visited; }
int EngineV7::get_max_depth() const { return last_depth; }

bool EngineV7::is_time_up() {
    auto now = steady_clock::now();
    duration<double> elapsed = now - start_time;
    return elapsed.count() >= time_limit_sec;
}

bool EngineV7::load_weights(const std::string& filename) {
    std::ifstream f(filename, std::ios::binary);
    if (!f.is_open()) return false;
    
    auto read_tensor = [&](std::vector<float>& t, int size) {
        t.resize(size);
        f.read(reinterpret_cast<char*>(t.data()), size * sizeof(float));
    };

    read_tensor(conv_in_w, 32 * 17 * 3 * 3); read_tensor(conv_in_b, 32);
    
    read_tensor(res1_c1_w, 32 * 32 * 3 * 3); read_tensor(res1_c1_b, 32);
    read_tensor(res1_c2_w, 32 * 32 * 3 * 3); read_tensor(res1_c2_b, 32);
    
    read_tensor(res2_c1_w, 32 * 32 * 3 * 3); read_tensor(res2_c1_b, 32);
    read_tensor(res2_c2_w, 32 * 32 * 3 * 3); read_tensor(res2_c2_b, 32);
    
    read_tensor(res3_c1_w, 32 * 32 * 3 * 3); read_tensor(res3_c1_b, 32);
    read_tensor(res3_c2_w, 32 * 32 * 3 * 3); read_tensor(res3_c2_b, 32);
    
    read_tensor(res4_c1_w, 32 * 32 * 3 * 3); read_tensor(res4_c1_b, 32);
    read_tensor(res4_c2_w, 32 * 32 * 3 * 3); read_tensor(res4_c2_b, 32);
    
    read_tensor(res5_c1_w, 32 * 32 * 3 * 3); read_tensor(res5_c1_b, 32);
    read_tensor(res5_c2_w, 32 * 32 * 3 * 3); read_tensor(res5_c2_b, 32);
    
    read_tensor(res6_c1_w, 32 * 32 * 3 * 3); read_tensor(res6_c1_b, 32);
    read_tensor(res6_c2_w, 32 * 32 * 3 * 3); read_tensor(res6_c2_b, 32);
    
    read_tensor(pi_conv_w, 2 * 32 * 1 * 1); read_tensor(pi_conv_b, 2);
    read_tensor(v_conv_w, 1 * 32 * 1 * 1); read_tensor(v_conv_b, 1);
    
    read_tensor(fc_pi_w, 7056 * 288); read_tensor(fc_pi_b, 7056);
    read_tensor(fc_v1_w, 64 * 144); read_tensor(fc_v1_b, 64);
    read_tensor(fc_v2_w, 1 * 64); read_tensor(fc_v2_b, 1);

    return f.good();
}

void EngineV7::conv2d_3x3(const TensorV7& in, TensorV7& out, const std::vector<float>& w, const std::vector<float>& b, int out_c) {
    int in_c = in.c;
    int h = in.h, w_dim = in.w;
    out.c = out_c; out.h = h; out.w = w_dim;
    
    // Initialize out with bias
    for (int oc = 0; oc < out_c; ++oc) {
        float bias = b[oc];
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w_dim; ++x) {
                out.set(oc, y, x, bias);
            }
        }
    }

    for (int oc = 0; oc < out_c; ++oc) {
        for (int ic = 0; ic < in_c; ++ic) {
            int w_off = (oc * in_c + ic) * 9;
            for (int ky = -1; ky <= 1; ++ky) {
                int y_start = std::max(0, -ky);
                int y_end = std::min(h, h - ky);
                for (int kx = -1; kx <= 1; ++kx) {
                    float weight = w[w_off + (ky + 1) * 3 + (kx + 1)];
                    int x_start = std::max(0, -kx);
                    int x_end = std::min(w_dim, w_dim - kx);
                    
                    for (int y = y_start; y < y_end; ++y) {
                        int iy = y + ky;
                        for (int x = x_start; x < x_end; ++x) {
                            int ix = x + kx;
                            out.data[(oc * h + y) * w_dim + x] += in.data[(ic * h + iy) * w_dim + ix] * weight;
                        }
                    }
                }
            }
        }
    }
}

void EngineV7::conv2d_1x1(const TensorV7& in, TensorV7& out, const std::vector<float>& w, const std::vector<float>& b, int out_c) {
    int in_c = in.c, h = in.h, w_dim = in.w;
    out.c = out_c; out.h = h; out.w = w_dim;
    for (int oc = 0; oc < out_c; ++oc) {
        float bias = b[oc];
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w_dim; ++x) {
                float sum = bias;
                for (int ic = 0; ic < in_c; ++ic) {
                    sum += in.data[(ic * h + y) * w_dim + x] * w[oc * in_c + ic];
                }
                out.data[(oc * h + y) * w_dim + x] = sum;
            }
        }
    }
}

void EngineV7::relu(TensorV7& t) {
    int size = t.c * t.h * t.w;
    for (int i = 0; i < size; ++i) {
        if (t.data[i] < 0.0f) t.data[i] = 0.0f;
    }
}

void EngineV7::add(TensorV7& a, const TensorV7& b_t) {
    int size = a.c * a.h * a.w;
    for (int i = 0; i < size; ++i) {
        a.data[i] += b_t.data[i];
    }
}

void EngineV7::flatten(const TensorV7& in, std::vector<float>& out) {
    int size = in.c * in.h * in.w;
    out.resize(size);
    std::copy(in.data.begin(), in.data.begin() + size, out.begin());
}

void EngineV7::linear(const std::vector<float>& in, std::vector<float>& out, const std::vector<float>& w, const std::vector<float>& b, int out_features) {
    int in_features = in.size();
    for (int o = 0; o < out_features; ++o) {
        float sum = b[o];
        int w_off = o * in_features;
        for (int i = 0; i < in_features; ++i) {
            sum += in[i] * w[w_off + i];
        }
        out[o] = sum;
    }
}

void EngineV7::relu(std::vector<float>& v) {
    for (float& val : v) {
        if (val < 0.0f) val = 0.0f;
    }
}

const std::vector<float>& EngineV7::evaluate_policy(const Board& b) {
    buffer1.c = 17; buffer1.h = 12; buffer1.w = 12;
    std::fill(buffer1.data.begin(), buffer1.data.end(), 0.0f);
    
    Color turn = b.side_to_move();
    
    for (int r = 0; r < 12; ++r) {
        for (int c = 0; c < 12; ++c) {
            int board_r = (turn == WHITE) ? 11 - r : r;
            int board_sq = board_r * 12 + c;
            
            PieceType p = b.piece_at(board_sq);
            if (p != NO_PIECE) {
                Color c_piece = b.color_at(board_sq);
                int canon_type = p; 
                if (c_piece != turn) {
                    canon_type = -p;
                }
                
                int channel = -1;
                if (canon_type > 0) {
                    channel = canon_type - 1; // 0 to 7
                } else if (canon_type < 0) {
                    channel = canon_type + 16; // -1 -> 15, -8 -> 8
                }
                if (channel >= 0) {
                    buffer1.set(channel, r, c, 1.0f);
                }
            }
            buffer1.set(16, r, c, b.get_half_move_clock() / 100.0f);
        }
    }
    
    // s is buffer1
    conv2d_3x3(buffer1, buffer2, conv_in_w, conv_in_b, 32); relu(buffer2); // x is buffer2
    
    // r1
    conv2d_3x3(buffer2, buffer1, res1_c1_w, res1_c1_b, 32); relu(buffer1);
    conv2d_3x3(buffer1, buffer3, res1_c2_w, res1_c2_b, 32); add(buffer2, buffer3); relu(buffer2);
    
    // r2
    conv2d_3x3(buffer2, buffer1, res2_c1_w, res2_c1_b, 32); relu(buffer1);
    conv2d_3x3(buffer1, buffer3, res2_c2_w, res2_c2_b, 32); add(buffer2, buffer3); relu(buffer2);

    // r3
    conv2d_3x3(buffer2, buffer1, res3_c1_w, res3_c1_b, 32); relu(buffer1);
    conv2d_3x3(buffer1, buffer3, res3_c2_w, res3_c2_b, 32); add(buffer2, buffer3); relu(buffer2);

    // r4
    conv2d_3x3(buffer2, buffer1, res4_c1_w, res4_c1_b, 32); relu(buffer1);
    conv2d_3x3(buffer1, buffer3, res4_c2_w, res4_c2_b, 32); add(buffer2, buffer3); relu(buffer2);
    
    // r5
    conv2d_3x3(buffer2, buffer1, res5_c1_w, res5_c1_b, 32); relu(buffer1);
    conv2d_3x3(buffer1, buffer3, res5_c2_w, res5_c2_b, 32); add(buffer2, buffer3); relu(buffer2);
    
    // r6
    conv2d_3x3(buffer2, buffer1, res6_c1_w, res6_c1_b, 32); relu(buffer1);
    conv2d_3x3(buffer1, buffer3, res6_c2_w, res6_c2_b, 32); add(buffer2, buffer3); relu(buffer2);
    
    // pi
    conv2d_1x1(buffer2, buffer1, pi_conv_w, pi_conv_b, 2); relu(buffer1);
    flatten(buffer1, flat_buffer);
    linear(flat_buffer, pi_out_buffer, fc_pi_w, fc_pi_b, 7056);
    
        
    float max_p = -1e9f;
    for (float p : pi_out_buffer) if (p > max_p) max_p = p;
    float sum_exp = 0.0f;
    for (float& p : pi_out_buffer) {
        p = std::exp(p - max_p);
        sum_exp += p;
    }
    for (float& p : pi_out_buffer) p /= sum_exp;

    return pi_out_buffer;
}

int EngineV7::move_to_action(const Move& m, Color side_to_move) const {
    int from_r = m.from / 12;
    int from_c = m.from % 12;
    int to_r = m.to / 12;
    int to_c = m.to % 12;

    int dr = to_r - from_r;
    int dc = to_c - from_c;

    int mapped_from_r = (side_to_move == WHITE) ? 11 - from_r : from_r;
    int mapped_from_c = from_c;
    int mapped_dr = (side_to_move == WHITE) ? -dr : dr;
    int mapped_dc = dc;

    return (mapped_from_r * 12 + mapped_from_c) * 49 + (mapped_dr + 3) * 7 + (mapped_dc + 3);
}

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
} // end namespace EvalConstants

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

int EngineV7::evaluate(const Board& b) {
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

struct ScopedMoveV7 {
    Board& b;
    Move m;
    ScopedMoveV7(Board& board, const Move& move) : b(board), m(move) {
        b.make_move(m);
    }
    ~ScopedMoveV7() {
        b.unmake_move(m);
    }
};

int EngineV7::alphabeta(Board& b, int depth, int alpha, int beta) {
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
                ScopedMoveV7 sm(b, move);
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
                ScopedMoveV7 sm(b, move);
                int eval = alphabeta(b, depth - 1, alpha, beta);
                min_eval = std::min(min_eval, eval);
                beta = std::min(beta, eval);
            }
             if (beta <= alpha) break; // Alpha Cutoff
        }
        return min_eval;
    }
}

Move EngineV7::search(Board& b, double time_limit) {
    if (conv_in_w.empty()) return Move(); // Not loaded
    
    start_time = steady_clock::now();
    time_limit_sec = time_limit;
    nodes_visited = 0;
    last_depth = 0;
    
    Move best_move;
    int max_depth = 100; 
    
    Color side = b.side_to_move();
    std::vector<Move> initial_moves = b.generate_moves();
    if (initial_moves.empty()) return Move();
    if (initial_moves.size() == 1) return initial_moves[0];

    // ROOT MOVE ORDERING USING NEURAL NETWORK POLICY
    const std::vector<float>& policy = evaluate_policy(b);
    
    struct MoveWithProb {
        Move m;
        float prob;
    };
    std::vector<MoveWithProb> sorted_moves;
    for (const Move& m : initial_moves) {
        int action = move_to_action(m, side);
        float p = (action >= 0 && action < 7056) ? policy[action] : 0.0f;
        sorted_moves.push_back({m, p});
    }
    
    // Sort descending by probability
    std::sort(sorted_moves.begin(), sorted_moves.end(), [](const MoveWithProb& a, const MoveWithProb& b) {
        return a.prob > b.prob;
    });
    
    for (int depth = 1; depth <= max_depth; ++depth) {
        last_depth = depth;
        try {
            Move current_best_move;
            int best_score;
            int alpha = -2000000;
            int beta = 2000000;
            
            if (side == WHITE) {
                best_score = -2000000;
                for (auto& mp : sorted_moves) {
                    int score;
                    {
                        ScopedMoveV7 sm(b, mp.m);
                        score = alphabeta(b, depth - 1, alpha, beta);
                    }
                    
                    if (score > best_score) {
                        best_score = score;
                        current_best_move = mp.m;
                    }
                    alpha = std::max(alpha, best_score);
                    
                    if (is_time_up()) throw std::runtime_error("Time limit");
                }
            } else {
                best_score = 2000000;
                for (auto& mp : sorted_moves) {
                    int score;
                    {
                        ScopedMoveV7 sm(b, mp.m);
                        score = alphabeta(b, depth - 1, alpha, beta);
                    }
                    
                    if (score < best_score) {
                        best_score = score;
                        current_best_move = mp.m;
                    }
                    beta = std::min(beta, best_score);
                    
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
