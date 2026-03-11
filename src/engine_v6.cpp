#include "engine_v6.h"
#include <fstream>
#include <iostream>
#include <cmath>
#include <algorithm>

using namespace std::chrono;

EngineV6::EngineV6() : nodes_visited(0) {
    load_weights("weights.bin");
    flat_buffer.resize(32 * 144);
    pi_out_buffer.resize(7056);
    v1_buffer.resize(64);
    v_out_buffer.resize(1);
}

long long EngineV6::get_nodes_visited() const { return nodes_visited; }
int EngineV6::get_max_depth() const { return static_cast<int>(nodes_visited); }

bool EngineV6::is_time_up() {
    auto now = steady_clock::now();
    duration<double> elapsed = now - start_time;
    return elapsed.count() >= time_limit_sec;
}

bool EngineV6::load_weights(const std::string& filename) {
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

void EngineV6::conv2d_3x3(const Tensor& in, Tensor& out, const std::vector<float>& w, const std::vector<float>& b, int out_c) {
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

void EngineV6::conv2d_1x1(const Tensor& in, Tensor& out, const std::vector<float>& w, const std::vector<float>& b, int out_c) {
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

void EngineV6::relu(Tensor& t) {
    int size = t.c * t.h * t.w;
    for (int i = 0; i < size; ++i) {
        if (t.data[i] < 0.0f) t.data[i] = 0.0f;
    }
}

void EngineV6::add(Tensor& a, const Tensor& b_t) {
    int size = a.c * a.h * a.w;
    for (int i = 0; i < size; ++i) {
        a.data[i] += b_t.data[i];
    }
}

void EngineV6::flatten(const Tensor& in, std::vector<float>& out) {
    int size = in.c * in.h * in.w;
    out.resize(size);
    std::copy(in.data.begin(), in.data.begin() + size, out.begin());
}

void EngineV6::linear(const std::vector<float>& in, std::vector<float>& out, const std::vector<float>& w, const std::vector<float>& b, int out_features) {
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

void EngineV6::relu(std::vector<float>& v) {
    for (float& val : v) {
        if (val < 0.0f) val = 0.0f;
    }
}

std::pair<std::vector<float>, float> EngineV6::evaluate(const Board& b) {
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
    
    // v
    conv2d_1x1(buffer2, buffer1, v_conv_w, v_conv_b, 1); relu(buffer1);
    flatten(buffer1, flat_buffer);
    linear(flat_buffer, v1_buffer, fc_v1_w, fc_v1_b, 64); relu(v1_buffer);
    linear(v1_buffer, v_out_buffer, fc_v2_w, fc_v2_b, 1);
    
    float max_p = -1e9f;
    for (float p : pi_out_buffer) if (p > max_p) max_p = p;
    float sum_exp = 0.0f;
    for (float& p : pi_out_buffer) {
        p = std::exp(p - max_p);
        sum_exp += p;
    }
    for (float& p : pi_out_buffer) p /= sum_exp;
    
    float v_val = std::tanh(v_out_buffer[0]);
    return {pi_out_buffer, v_val};
}

int EngineV6::move_to_action(const Move& m, Color side_to_move) const {
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

Move EngineV6::search(Board& b, double time_limit) {
    if (conv_in_w.empty()) return Move(); // Not loaded
    
    start_time = steady_clock::now();
    time_limit_sec = time_limit;
    nodes_visited = 0;
    
    if (b.is_game_over() || b.is_draw()) return Move();
    std::vector<Move> initial_moves = b.generate_moves();
    if (initial_moves.empty()) return Move();
    if (initial_moves.size() == 1) return initial_moves[0];
    
    MCTSNodeV6 root(b, nullptr, Move());
    
    auto eval = evaluate(b);
    // std::cout << "Board evaluation: " << eval.second << " (from " << (b.side_to_move() == WHITE ? "White" : "Black") << "'s perspective)" << std::endl;
    root.value_sum = 0;
    root.visits = 1; 
    
    float sum_prior = 0.0f;
    for (const Move& m : initial_moves) {
        int action = move_to_action(m, b.side_to_move());
        if (action >= 0 && action < 7056) {
            float p = eval.first[action];
            MCTSNodeV6::Edge e;
            e.m = m; e.prior = p; e.expanded = false; e.child = nullptr;
            root.edges.push_back(e);
            sum_prior += p;
        }
    }
    
    if (sum_prior > 0) {
        for (auto& e : root.edges) e.prior /= sum_prior;
    } else {
        for (auto& e : root.edges) e.prior = 1.0f / root.edges.size();
    }
    
    while (true) {
        if ((nodes_visited & 127) == 0 && is_time_up()) break;
        MCTSNodeV6* node = &root;
        
        while (node->is_fully_expanded() && !node->is_terminal()) {
            float best_score = -1e9f;
            MCTSNodeV6::Edge* best_edge = nullptr;
            
            float sqrt_N = std::sqrt((float)node->visits);
            for (auto& e : node->edges) {
                float Q = (e.child->visits == 0) ? 0.0f : -(e.child->value_sum / e.child->visits);
                float U = 1.5f * e.prior * sqrt_N / (1.0f + e.child->visits);
                if (Q + U > best_score) {
                    best_score = Q + U;
                    best_edge = &e;
                }
            }
            node = best_edge->child;
        }
        
        float v_for_leaf = 0.0f;
        if (!node->is_terminal()) {
            MCTSNodeV6::Edge* expand_edge = nullptr;
            for (auto& e : node->edges) {
                if (!e.expanded) {
                    expand_edge = &e;
                    break;
                }
            }
            
            Board next_state = node->state;
            next_state.make_move(expand_edge->m);
            
            auto new_child = std::make_unique<MCTSNodeV6>(next_state, node, expand_edge->m);
            MCTSNodeV6* child_ptr = new_child.get();
            node->children.push_back(std::move(new_child));
            
            expand_edge->child = child_ptr;
            expand_edge->expanded = true;
            
            bool is_leaf_terminal = child_ptr->state.is_game_over() || child_ptr->state.is_draw();
            std::vector<Move> child_moves;
            if (!is_leaf_terminal) {
                child_moves = child_ptr->state.generate_moves();
                if (child_moves.empty()) is_leaf_terminal = true;
            }

            if (is_leaf_terminal) {
                if (child_ptr->state.is_game_over()) {
                    v_for_leaf = -1.0f;
                } else {
                    v_for_leaf = 0.0f;
                }
            } else {
                auto child_eval = evaluate(child_ptr->state);
                v_for_leaf = child_eval.second;
                
                float child_sum_p = 0.0f;
                for (const Move& m : child_moves) {
                    int action = move_to_action(m, child_ptr->color_to_move);
                    float p = (action >= 0 && action < 7056) ? child_eval.first[action] : 0.0f;
                    MCTSNodeV6::Edge e; e.m = m; e.prior = p; e.expanded = false; e.child = nullptr;
                    child_ptr->edges.push_back(e);
                    child_sum_p += p;
                }
                
                if (child_sum_p > 0) {
                    for (auto& e : child_ptr->edges) e.prior /= child_sum_p;
                } else {
                    for (auto& e : child_ptr->edges) e.prior = 1.0f / child_ptr->edges.size();
                }
            }
            node = child_ptr; 
        } else {
            if (node->state.is_game_over()) v_for_leaf = -1.0f;
            else v_for_leaf = 0.0f;
        }
        
        float current_v = v_for_leaf;
        MCTSNodeV6* curr = node;
        while (curr != nullptr) {
            curr->visits++;
            curr->value_sum += current_v;
            current_v = -current_v;
            curr = curr->parent;
        }
        
        nodes_visited++;
    }
    
    Move best_move;
    int max_visits = -1;
    for (const auto& e : root.edges) {
        if (e.child && e.child->visits > max_visits) {
            max_visits = e.child->visits;
            best_move = e.m;
        }
    }
    
    return best_move;
}
