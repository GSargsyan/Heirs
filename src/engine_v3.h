#ifndef ENGINE_V3_H
#define ENGINE_V3_H

#include "board.h"
#include <chrono>

struct EvalComponents {
    int mg_mat = 0, eg_mat = 0;
    int mg_pst_baby = 0, eg_pst_baby = 0;
    int mg_pst_prince = 0, eg_pst_prince = 0;
    int mg_pst_center = 0, eg_pst_center = 0;
    int eg_tropism = 0;
    int w_prince_mobility_score = 0;
    int b_prince_mobility_score = 0;
    int phase = 0;
    int mg_total = 0;
    int eg_total = 0;
    int final_total = 0;

    EvalComponents operator-(const EvalComponents& other) const {
        EvalComponents d;
        d.mg_mat = mg_mat - other.mg_mat;
        d.eg_mat = eg_mat - other.eg_mat;
        d.mg_pst_baby = mg_pst_baby - other.mg_pst_baby;
        d.eg_pst_baby = eg_pst_baby - other.eg_pst_baby;
        d.mg_pst_prince = mg_pst_prince - other.mg_pst_prince;
        d.eg_pst_prince = eg_pst_prince - other.eg_pst_prince;
        d.mg_pst_center = mg_pst_center - other.mg_pst_center;
        d.eg_pst_center = eg_pst_center - other.eg_pst_center;
        d.eg_tropism = eg_tropism - other.eg_tropism;
        d.w_prince_mobility_score = w_prince_mobility_score - other.w_prince_mobility_score;
        d.b_prince_mobility_score = b_prince_mobility_score - other.b_prince_mobility_score;
        d.phase = phase;
        d.mg_total = mg_total - other.mg_total;
        d.eg_total = eg_total - other.eg_total;
        d.final_total = final_total - other.final_total;
        return d;
    }
};

extern bool DEBUG_EVAL_V3;

class EngineV3 {
public:
    EngineV3();
    
    // Search for the best move within the time limit (in seconds)
    Move search(Board& b, double time_limit);

    long long get_nodes_visited() const;
    int get_max_depth() const;
    
private:
    // Alpha-Beta search
    int alphabeta(Board& b, int depth, int alpha, int beta);
    
    // Evaluation function
    int evaluate(const Board& b, EvalComponents* debug_info = nullptr);
    
    // Helper to check time
    bool is_time_up();
    
    std::chrono::time_point<std::chrono::steady_clock> start_time;
    double time_limit_sec;

    long long nodes_visited;
    int last_depth;
};

#endif // ENGINE_V3_H
