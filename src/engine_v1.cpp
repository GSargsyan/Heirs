#include "engine_v1.h"
#include <algorithm>
#include <iostream>
#include <chrono>
#include <random>

using namespace std::chrono;

// VERSION 1 Engine does basic minimax search with piece values evaluation
EngineV1::EngineV1() : nodes_visited(0), last_depth(0) {}

long long EngineV1::get_nodes_visited() const { return nodes_visited; }
int EngineV1::get_max_depth() const { return last_depth; }

int EngineV1::evaluate(const Board& b) {
    return 0;
}

bool EngineV1::is_time_up() {
    return false;
}

int EngineV1::minimax(Board& b, int depth) {
    return 0;
}

Move EngineV1::search(Board& b, double time_limit) {
    nodes_visited = 1;
    last_depth = 1;
    
    std::vector<Move> moves = b.generate_moves();
    if (moves.empty()) {
        return Move(); 
    }
    
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> distrib(0, moves.size() - 1);
    
    return moves[distrib(gen)];
}
