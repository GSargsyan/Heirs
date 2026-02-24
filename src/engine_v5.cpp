#include "engine_v5.h"
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <algorithm>

using namespace std::chrono;

EngineV5::EngineV5() : nodes_visited(0), mt(rd()) {}

long long EngineV5::get_nodes_visited() const { return nodes_visited; }
int EngineV5::get_max_depth() const { return static_cast<int>(nodes_visited); } // Use nodes as depth gauge

bool EngineV5::is_time_up() {
    auto now = steady_clock::now();
    duration<double> elapsed = now - start_time;
    return elapsed.count() >= time_limit_sec;
}

double EngineV5::evaluate_material(const Board& b) const {
    // Basic material heuristic based on V4 values
    static const int VAL_BABY = 100;
    static const int VAL_PRINCE = 20000;
    static const int VAL_PRINCESS = 950;
    static const int VAL_PONY = 180;
    static const int VAL_GUARD = 350;
    static const int VAL_TUTOR = 325;
    static const int VAL_SCOUT = 600;
    static const int VAL_SIBLING = 280;

    static const int PIECE_VALUES[] = {
        0, VAL_BABY, VAL_PRINCE, VAL_PRINCESS, VAL_PONY, 
        VAL_GUARD, VAL_TUTOR, VAL_SCOUT, VAL_SIBLING
    };

    int white_score = 0;
    int black_score = 0;

    for (int sq = 0; sq < 144; ++sq) {
        PieceType p = b.piece_at(sq);
        if (p == NO_PIECE) continue;

        Color c = b.color_at(sq);
        int val = PIECE_VALUES[p];

        if (c == WHITE) {
            white_score += val;
        } else {
            black_score += val;
        }
    }
    
    // Normalize to roughly [-1.0, 1.0] from perspective of winning probability
    // Max difference approx 30000 (Prince capture)
    int diff = white_score - black_score;
    double normalized = static_cast<double>(diff) / 20000.0;
    
    // Clamp
    if (normalized > 1.0) normalized = 1.0;
    if (normalized < -1.0) normalized = -1.0;

    return normalized;
}

MCTSNode* EngineV5::best_child(MCTSNode* node, double c_param) {
    MCTSNode* best = nullptr;
    double best_value = -1e9;
    
    for (auto& child : node->children) {
        if (child->visits == 0) continue; // Should not happen if fully expanded
        
        // UCT formula
        // value_sum is from the perspective of the node's *parent* color who made the move.
        // Wait: value_sum should always store win rate from the node.parent's mover's perspective.
        // Or simply: node.color_to_move is the side that will move NOW. The move leading here was made by the OPPONENT.
        // Therefore, we maximize the average reward for the player who made the move leading to `child`.
        
        double exploit = child->value_sum / child->visits;
        double explore = c_param * std::sqrt(std::log(node->visits) / child->visits);
        double uct = exploit + explore;
        
        if (uct > best_value) {
            best_value = uct;
            best = child.get();
        }
    }
    return best;
}

MCTSNode* EngineV5::expand(MCTSNode* node) {
    if (node->untried_moves.empty()) return node;
    
    // Pick random untried move
    std::uniform_int_distribution<size_t> dist(0, node->untried_moves.size() - 1);
    size_t idx = dist(mt);
    Move move = node->untried_moves[idx];
    
    // Remove it from untried
    node->untried_moves[idx] = node->untried_moves.back();
    node->untried_moves.pop_back();
    
    // Create new state
    Board next_state = node->state;
    next_state.make_move(move);
    
    // Add child
    auto new_child = std::make_unique<MCTSNode>(next_state, node, move);
    MCTSNode* child_ptr = new_child.get();
    node->children.push_back(std::move(new_child));
    
    return child_ptr;
}

MCTSNode* EngineV5::tree_policy(MCTSNode* node) {
    while (!node->is_terminal()) {
        if (!node->is_fully_expanded()) {
            return expand(node);
        } else {
            // C = sqrt(2) is standard
            node = best_child(node, 1.414);
        }
    }
    return node;
}

double EngineV5::default_policy(Board state) {
    // Truncated random playout
    int max_depth = 50; // Random walks in long games wander too much. Cut off early.
    int current_depth = 0;
    
    while (!state.is_game_over() && !state.is_draw() && current_depth < max_depth) {
        std::vector<Move> moves = state.generate_moves();
        if (moves.empty()) break; // No legal moves (should be handled by is_game_over/draw but safe)
        
        std::uniform_int_distribution<size_t> dist(0, moves.size() - 1);
        state.make_move(moves[dist(mt)]);
        current_depth++;
    }
    
    // If not terminal, evaluate using heuristic.
    // If terminal, evaluate normally.
    double eval = evaluate_material(state);
    return eval; // Evaluates board: +1 for White win/advantage, -1 for Black win/advantage.
}

void EngineV5::backpropagate(MCTSNode* node, double final_eval) {
    // final_eval is +1 for White win, -1 for Black win
    while (node != nullptr) {
        node->visits++;
        
        // If node->parent exists, the action from parent to node was made by parent's color.
        // We want to store the reward from the perspective of parent's color.
        // node->color_to_move is the current color. So parent's color is opposite.
        if (node->parent != nullptr) {
            Color parent_color = node->parent->color_to_move;
            double reward = (parent_color == WHITE) ? final_eval : -final_eval;
            // Reward is [0, 1] usually. Let's shift [-1, 1] to [0, 1]
            double shifted_reward = (reward + 1.0) / 2.0; 
            node->value_sum += shifted_reward;
        } else {
            // Root node. Doesn't matter as much, just track visits.
            // But we can store it from root's perspective.
            double reward = (node->color_to_move == WHITE) ? final_eval : -final_eval;
            node->value_sum += (reward + 1.0) / 2.0;
        }
        
        node = node->parent;
    }
}

Move EngineV5::search(Board& b, double time_limit) {
    start_time = steady_clock::now();
    time_limit_sec = time_limit;
    nodes_visited = 0;
    
    // Handle trivial cases
    if (b.is_game_over() || b.is_draw()) return Move();
    std::vector<Move> initial_moves = b.generate_moves();
    if (initial_moves.empty()) return Move();
    if (initial_moves.size() == 1) return initial_moves[0]; // Only 1 move available
    
    // Initialize root
    MCTSNode root(b, nullptr, Move());
    
    // Main MCTS loop
    while (!is_time_up()) {
        MCTSNode* leaf = tree_policy(&root);
        double reward = default_policy(leaf->state); // Always returns White perspective [-1, 1]
        backpropagate(leaf, reward);
        
        nodes_visited++;
        if ((nodes_visited & 127) == 0 && is_time_up()) {
            break;
        }
    }
    
    // Best move is the one from the most visited child
    if (root.children.empty()) return initial_moves[0]; // Failsafe
    
    MCTSNode* best = nullptr;
    int most_visits = -1;
    for (auto& child : root.children) {
        if (child->visits > most_visits) {
            most_visits = child->visits;
            best = child.get();
        }
    }
    
    if (best) return best->move;
    return initial_moves[0];
}
