#ifndef ENGINE_V2_H
#define ENGINE_V2_H

#include "board.h"
#include <chrono>
#include <vector>

enum TTBoundV2 {
    BOUND_NONE_V2,
    BOUND_EXACT_V2,
    BOUND_LOWER_V2,
    BOUND_UPPER_V2
};

struct TTEntryV2 {
    uint64_t key;
    int depth;
    int score;
    TTBoundV2 bound;
    Move best_move;
};

class TranspositionTableV2 {
public:
    TranspositionTableV2(size_t size_mb) {
        size_t num_entries = (size_mb * 1024 * 1024) / sizeof(TTEntryV2);
        table.resize(num_entries);
        clear();
    }

    void clear() {
        for (auto& entry : table) {
            entry.key = 0;
            entry.depth = -1;
            entry.score = 0;
            entry.bound = BOUND_NONE_V2;
            entry.best_move = Move();
        }
    }

    void store(uint64_t key, int depth, int score, TTBoundV2 bound, Move best_move) {
        size_t index = key % table.size();
        if (table[index].key == 0 || depth >= table[index].depth) {
            table[index].key = key;
            table[index].depth = depth;
            table[index].score = score;
            table[index].bound = bound;
            table[index].best_move = best_move;
        }
    }

    bool probe(uint64_t key, TTEntryV2& out_entry) const {
        size_t index = key % table.size();
        if (table[index].key == key) {
            out_entry = table[index];
            return true;
        }
        return false;
    }

private:
    std::vector<TTEntryV2> table;
};

class EngineV2 {
public:
    EngineV2();
    
    // Search for the best move within the time limit (or depth limit if time_limit <= 0)
    Move search(Board& b, double time_limit, int max_depth = 100);

    long long get_nodes_visited() const;
    int get_max_depth() const;
    
    // Set custom piece values
    void set_piece_values(const int values[9]);
    
private:
    int piece_values[9];

    TranspositionTableV2 tt;

    // Alpha-Beta search
    int alphabeta(Board& b, int depth, int alpha, int beta, int ply);
    
    // Helper for move scoring
    void score_moves(const MoveList& moves, int* move_scores, const Board& b, int ply, Move tt_move);
    
    // Evaluation function
    int evaluate(const Board& b);

    // Quiescence Search
    int quiescence(Board& b, int alpha, int beta, int ply);
    

    
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

#endif // ENGINE_V2_H
