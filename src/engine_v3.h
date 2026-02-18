#ifndef ENGINE_V3_H
#define ENGINE_V3_H

#include "board.h"
#include <chrono>
#include <vector>

struct TTEntry {
    uint64_t key;
    int depth;
    int score;
    int flag; // 0=Exact, 1=Lower (Beta), 2=Upper (Alpha)
    Move best_move;
};

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
    int evaluate(const Board& b);
    
    // Helper to check time
    bool is_time_up();
    
    std::chrono::time_point<std::chrono::steady_clock> start_time;
    double time_limit_sec;

    long long nodes_visited;
    int last_depth;

    // Transposition Table
    std::vector<TTEntry> tt;
    static const size_t TT_SIZE = 1 << 20; // ~1 million entries
    
    // TT Flags
    static const int TT_EXACT = 0;
    static const int TT_LOWER = 1;
    static const int TT_UPPER = 2;

    void clear_tt();
    bool probe_tt(uint64_t key, int depth, int alpha, int beta, int& score, Move& tt_move);
    void store_tt(uint64_t key, int depth, int score, int flag, Move best_move);
};

#endif // ENGINE_V3_H
