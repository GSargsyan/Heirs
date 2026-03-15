#ifndef ENGINE_V2_H
#define ENGINE_V2_H

#include "board.h"
#include <chrono>

class EngineV2 {
public:
    EngineV2();
    
    // Search for the best move within the time limit (in seconds)
    Move search(Board& b, double time_limit);

    long long get_nodes_visited() const;
    int get_max_depth() const;
    
private:
    // Alpha-Beta search
    int alphabeta(Board& b, int depth, int alpha, int beta);
    
    // Evaluation function
    int evaluate(const Board& b);
    
    // Helper to check time
    bool is_time_up();
    
    std::chrono::time_point<std::chrono::steady_clock> start_time;
    double time_limit_sec;

    long long nodes_visited;
    int last_depth;
};

#endif // ENGINE_V2_H
