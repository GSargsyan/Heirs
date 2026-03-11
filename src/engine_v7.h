#ifndef ENGINE_V7_H
#define ENGINE_V7_H

#include "board.h"
#include <chrono>
#include <vector>
#include <memory>
#include <random>
#include <string>

// Simple TensorV7 class reused from V6
struct TensorV7 {
    int c, h, w;
    std::vector<float> data;

    TensorV7(int c, int h, int w) : c(c), h(h), w(w), data(c * h * w, 0.0f) {}
    
    inline float get(int cc, int hh, int ww) const {
        return data[(cc * h + hh) * w + ww];
    }
    
    inline void set(int cc, int hh, int ww, float val) {
        data[(cc * h + hh) * w + ww] = val;
    }
};

class EngineV7 {
public:
    EngineV7();
    bool load_weights(const std::string& filename);
    Move search(Board& b, double time_limit);

    long long get_nodes_visited() const;
    int get_max_depth() const;

private:
    std::chrono::time_point<std::chrono::steady_clock> start_time;
    double time_limit_sec;
    long long nodes_visited;
    int last_depth;
    
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
    TensorV7 buffer1{32, 12, 12};
    TensorV7 buffer2{32, 12, 12};
    TensorV7 buffer3{32, 12, 12};
    std::vector<float> flat_buffer;
    std::vector<float> pi_out_buffer;

    bool is_time_up();

    // Neural Net Operations
    void conv2d_3x3(const TensorV7& in, TensorV7& out, const std::vector<float>& w, const std::vector<float>& b, int out_c);
    void conv2d_1x1(const TensorV7& in, TensorV7& out, const std::vector<float>& w, const std::vector<float>& b, int out_c);
    void relu(TensorV7& t);
    void add(TensorV7& a, const TensorV7& b);
    void flatten(const TensorV7& in, std::vector<float>& out);
    void linear(const std::vector<float>& in, std::vector<float>& out, const std::vector<float>& w, const std::vector<float>& b, int out_features);
    void relu(std::vector<float>& v);
    
    // Neural Net Policy evaluation
    const std::vector<float>& evaluate_policy(const Board& b);
    int move_to_action(const Move& m, Color side_to_move) const;

    // Alpha-Beta search (V4 logic)
    int alphabeta(Board& b, int depth, int alpha, int beta);
    
    // Evaluation function (V4 logic)
    int evaluate(const Board& b);
};

#endif // ENGINE_V7_H
