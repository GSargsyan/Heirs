#ifndef ENGINE_V5_H
#define ENGINE_V5_H

#include "board.h"
#include <chrono>
#include <vector>
#include <memory>
#include <random>

// MCTS Node structure
struct MCTSNode {
    Board state;
    Move move; // Move that led to this state
    MCTSNode* parent;
    std::vector<std::unique_ptr<MCTSNode>> children;
    std::vector<Move> untried_moves;
    
    double value_sum;
    int visits;
    Color color_to_move; // Whose turn it is to move from this node
    
    MCTSNode(const Board& b, MCTSNode* p, const Move& m)
        : state(b), move(m), parent(p), value_sum(0), visits(0) {
        color_to_move = b.side_to_move();
        if (!state.is_game_over() && !state.is_draw()) {
            untried_moves = state.generate_moves();
        }
    }
    
    bool is_fully_expanded() const {
        return untried_moves.empty();
    }
    
    bool is_terminal() const {
        return state.is_game_over() || state.is_draw() || (untried_moves.empty() && children.empty());
    }
};

class EngineV5 {
public:
    EngineV5();
    
    // Search for the best move within the time limit (in seconds)
    Move search(Board& b, double time_limit);

    long long get_nodes_visited() const;
    int get_max_depth() const; // Max simulation depth reached or simply iteration count
    
private:
    std::chrono::time_point<std::chrono::steady_clock> start_time;
    double time_limit_sec;
    long long nodes_visited;
    
    std::random_device rd;
    std::mt19937 mt;

    bool is_time_up();
    
    // MCTS steps
    MCTSNode* tree_policy(MCTSNode* node);
    MCTSNode* expand(MCTSNode* node);
    MCTSNode* best_child(MCTSNode* node, double c_param);
    double default_policy(Board state); // Random simulation with depth truncation
    void backpropagate(MCTSNode* node, double reward);
    
    // Heuristic for truncated simulation
    double evaluate_material(const Board& b) const;
};

#endif // ENGINE_V5_H
