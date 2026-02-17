#ifndef BOARD_H
#define BOARD_H

#include <array>
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

// 12x12 board = 144 squares.
// We use 3 uint64_t to represent the board.
// 0-63: first uint64_t
// 64-127: second uint64_t
// 128-143: third uint64_t (16 bits used)

struct Bitboard {
    std::array<uint64_t, 3> parts;

    Bitboard() : parts{0, 0, 0} {}

    void set(int sq);
    void clear(int sq);
    bool test(int sq) const;
    void toggle(int sq);
    int popcount() const;
    int lsb() const; // Returns index of least significant bit set
    
    // Operators
    Bitboard& operator&=(const Bitboard& other);
    Bitboard& operator|=(const Bitboard& other);
    Bitboard& operator^=(const Bitboard& other);
    Bitboard operator~() const;
    
    friend Bitboard operator&(Bitboard lhs, const Bitboard& rhs) {
        lhs &= rhs;
        return lhs;
    }
    friend Bitboard operator|(Bitboard lhs, const Bitboard& rhs) {
        lhs |= rhs;
        return lhs;
    }
    friend Bitboard operator^(Bitboard lhs, const Bitboard& rhs) {
        lhs ^= rhs;
        return lhs;
    }
    
    // Shift operators (might be tricky with 3 parts, but essential for patterns)
    // For now, simple shifts. Optimized implementations can be added.
    // Note: Shifting across uint64_t boundaries requires care.
    Bitboard operator<<(int shift) const;
    Bitboard operator>>(int shift) const;

     bool operator==(const Bitboard& other) const {
        return parts[0] == other.parts[0] && parts[1] == other.parts[1] && parts[2] == other.parts[2];
    }

    bool operator!=(const Bitboard& other) const {
        return !(*this == other);
    }
};

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
    // No promotion in Heirs
    // We might want to store captured piece type for unmake move
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
    
    void reset(); // Sets up the initial position
    
    void set_piece(int sq, PieceType p, Color c);
    void clear_piece(int sq);
    
    PieceType piece_at(int sq) const;
    Color color_at(int sq) const;
    
    void print() const;
    
    // Move generation and handling
    std::vector<Move> generate_moves() const;
    void make_move(const Move& m);
    void unmake_move(const Move& m);
    
    // Game state
    Color side_to_move() const { return turn; }
    bool is_game_over() const; // Basic check (Prince captured)
    
private:
    Bitboard pieces[COLOR_NB][PIECE_NB];
    Bitboard occupied[COLOR_NB];
    Bitboard all_occupied;
    PieceType mailbox[144]; // For fast lookup of piece type
    Color color_mailbox[144]; // For fast lookup of piece color
    Color turn;
    
    // Helper for move generation
    void add_move(std::vector<Move>& moves, int from, int to) const;
};

#endif // BOARD_H
