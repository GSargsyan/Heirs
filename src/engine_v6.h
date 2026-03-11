#ifndef ENGINE_V6_H
#define ENGINE_V6_H

#include "board.h"
#include <chrono>
#include <vector>
#include <memory>
#include <random>
#include <string>

// Simple Tensor class
struct Tensor {
    int c, h, w;
    std::vector<float> data;

    Tensor(int c, int h, int w) : c(c), h(h), w(w), data(c * h * w, 0.0f) {}
    
    inline float get(int cc, int hh, int ww) const {
        return data[(cc * h + hh) * w + ww];
    }
    
    inline void set(int cc, int hh, int ww, float val) {
        data[(cc * h + hh) * w + ww] = val;
    }
};

class EngineV6 {
public:
    EngineV6();
    bool load_weights(const std::string& filename);
    Move search(Board& b, double time_limit);

    long long get_nodes_visited() const;
    int get_max_depth() const;

private:
    std::chrono::time_point<std::chrono::steady_clock> start_time;
    double time_limit_sec;
    long long nodes_visited;
    
    // Model Weights
    std::vector<float> conv_in_w, conv_in_b;
    std::vector<float> res1_c1_w, res1_c1_b, res1_c2_w, res1_c2_b;
    std::vector<float> res2_c1_w, res2_c1_b, res2_c2_w, res2_c2_b;
    std::vector<float> res3_c1_w, res3_c1_b, res3_c2_w, res3_c2_b;
    std::vector<float> res4_c1_w, res4_c1_b, res4_c2_w, res4_c2_b;
    std::vector<float> res5_c1_w, res5_c1_b, res5_c2_w, res5_c2_b;
    std::vector<float> res6_c1_w, res6_c1_b, res6_c2_w, res6_c2_b;
    std::vector<float> pi_conv_w, pi_conv_b;
    std::vector<float> v_conv_w, v_conv_b;
    std::vector<float> fc_pi_w, fc_pi_b;
    std::vector<float> fc_v1_w, fc_v1_b;
    std::vector<float> fc_v2_w, fc_v2_b;

    // Pre-allocated buffers
    Tensor buffer1{32, 12, 12};
    Tensor buffer2{32, 12, 12};
    Tensor buffer3{32, 12, 12};
    std::vector<float> flat_buffer;
    std::vector<float> pi_out_buffer;
    std::vector<float> v1_buffer;
    std::vector<float> v_out_buffer;

    bool is_time_up();

    // Neural Net Operations
    void conv2d_3x3(const Tensor& in, Tensor& out, const std::vector<float>& w, const std::vector<float>& b, int out_c);
    void conv2d_1x1(const Tensor& in, Tensor& out, const std::vector<float>& w, const std::vector<float>& b, int out_c);
    void relu(Tensor& t);
    void add(Tensor& a, const Tensor& b);
    void flatten(const Tensor& in, std::vector<float>& out);
    void linear(const std::vector<float>& in, std::vector<float>& out, const std::vector<float>& w, const std::vector<float>& b, int out_features);
    void relu(std::vector<float>& v);
    
    // Evaluation returns (Policy, Value)
    std::pair<std::vector<float>, float> evaluate(const Board& b);
    int move_to_action(const Move& m, Color side_to_move) const;
};

// MCTS Node for PUCT
struct MCTSNodeV6 {
    Board state;
    Move move;
    MCTSNodeV6* parent;
    std::vector<std::unique_ptr<MCTSNodeV6>> children;
    
    // To keep track of untried moves, we can just spawn all children at once 
    // or keep a list of valid moves and their priors.
    struct Edge {
        Move m;
        float prior;
        bool expanded;
        MCTSNodeV6* child; // points to unique_ptr in children vector or null
    };
    std::vector<Edge> edges;
    
    double value_sum;
    int visits;
    Color color_to_move;
    
    MCTSNodeV6(const Board& b, MCTSNodeV6* p, const Move& m)
        : state(b), move(m), parent(p), value_sum(0), visits(0) {
        color_to_move = b.side_to_move();
    }
    
    bool is_fully_expanded() const {
        for (const auto& e : edges) {
            if (!e.expanded) return false;
        }
        return true;
    }
    
    bool is_terminal() const {
        // We handle stalemates manually in engine_v6.cpp, so here we only check state game over.
        return state.is_game_over() || state.is_draw();
    }
};

#endif // ENGINE_V6_H
