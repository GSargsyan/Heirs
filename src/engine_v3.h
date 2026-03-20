#ifndef ENGINE_V3_H
#define ENGINE_V3_H

#include "board.h"
#include <chrono>


class EngineV3 {
public:
    EngineV3();
    
    // Search for the best move within the time limit (in seconds)
    Move search(Board& b, double time_limit);

    long long get_nodes_visited() const;
    int get_max_depth() const;
    
    // Set custom piece values
    void set_piece_values(const int values[9]);
    
private:
    int piece_values[9];

    // Alpha-Beta search
    int alphabeta(Board& b, int depth, int alpha, int beta, int ply);
    
    // Helper for move sorting
    void score_and_sort_moves(MoveList& moves, const Board& b, int ply);
    
    // Evaluation function
    int evaluate(const Board& b);
    
    // Helper to check time
    bool is_time_up();
    
    static const int MAX_PLY = 128; // slightly over 100 for safety
    Move killer_moves[MAX_PLY][2];
    int history_table[2][256][256]; // 256 size correctly maps to 16x16 coordinates directly

    std::chrono::time_point<std::chrono::steady_clock> start_time;
    double time_limit_sec;

    long long nodes_visited;
    int last_depth;
};

#endif // ENGINE_V3_H
