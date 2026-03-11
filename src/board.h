#ifndef BOARD_H
#define BOARD_H

#include <vector>
#include <cstdint>
#include <iostream>
#include <string>

// Enum definitions (same as before to maintain compatibility)
enum Color {
    WHITE = 0,
    BLACK = 1,
    COLOR_NB = 2
};

enum PieceType {
    NO_PIECE = 0,
    BABY = 1,
    PRINCE = 2,
    PRINCESS = 3,
    PONY = 4,
    GUARD = 5,
    TUTOR = 6,
    SCOUT = 7,
    SIBLING = 8,
    PIECE_NB = 9
};

struct Move {
    uint16_t from;
    uint16_t to;
    PieceType captured; 
    
    Move() : from(0), to(0), captured(NO_PIECE) {}
    Move(int f, int t, PieceType c = NO_PIECE) : from(f), to(t), captured(c) {}
    
    bool operator==(const Move& other) const {
        return from == other.from && to == other.to; 
    }
};

class Board {
public:
    Board();
    
    void reset();
    
    // Core accessors
    PieceType piece_at(int sq) const;
    Color color_at(int sq) const;
    
    // Modifiers (internal use mostly, but useful for setup)
    void set_piece(int sq, PieceType p, Color c);
    void clear_piece(int sq);
    
    // Move generation and handling
    std::vector<Move> generate_moves() const;
    void make_move(const Move& m);
    void unmake_move(const Move& m);
    
    // Game state
    Color side_to_move() const { return turn; }
    bool is_game_over() const;
    int get_half_move_clock() const { return half_move_clock; }
    
    // Draw detection
    bool is_repetition() const;
    bool is_fifty_moves() const;
    bool is_draw() const; // Combines repetition and 50-moves
    uint64_t get_hash() const { return zobrist_key; }
    
    void print() const;
    
private:
    PieceType pieces[144];
    Color colors[144];
    Color turn;
    
    // Zobrist Hashing
    uint64_t zobrist_key;
    std::vector<uint64_t> history;
    
    // 50-move rule
    int half_move_clock;
    std::vector<int> half_move_history;
    
    static uint64_t piece_keys[144][2][10];
    static uint64_t side_key;
    static bool zobrist_initialized;
    static void init_zobrist();
    
    // Helper counts for fast check
    int prince_count[2]; 
    
    // Helper methods
    void add_move(std::vector<Move>& moves, int from, int to) const;
};

#endif // BOARD_H
