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
    COLOR_NB = 2,
    COLOR_OFFBOARD = 3
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
    OFFBOARD = 9,
    PIECE_NB = 10
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

struct MoveList {
    Move moves[256];
    int count = 0;
    
    void add(int from, int to, PieceType captured) {
        moves[count].from = from;
        moves[count].to = to;
        moves[count].captured = captured;
        count++;
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
    void generate_moves(MoveList& list) const;
    void make_move(const Move& m);
    void unmake_move(const Move& m);
    
    // Game state
    Color side_to_move() const { return turn; }
    bool is_game_over() const;
    int get_half_move_clock() const { return half_move_clock; }
    
    // Piece lists access for fast evaluation
    const int* get_white_pieces() const { return whitePieces; }
    int get_white_piece_count() const { return whitePieceCount; }
    const int* get_black_pieces() const { return blackPieces; }
    int get_black_piece_count() const { return blackPieceCount; }
    
    // Draw detection
    bool is_repetition() const;
    bool is_fifty_moves() const;
    bool is_draw() const; // Combines repetition and 50-moves
    uint64_t get_hash() const { return zobrist_key; }
    
    void print() const;
    
private:
    PieceType pieces[256];
    Color colors[256];
    Color turn;
    
    // Zobrist Hashing
    uint64_t zobrist_key;
    std::vector<uint64_t> history;
    
    // 50-move rule
    int half_move_clock;
    std::vector<int> half_move_history;
    
    static uint64_t piece_keys[256][2][10];
    static uint64_t side_key;
    static bool zobrist_initialized;
    static void init_zobrist();
    
    // Helper counts for fast check
    int prince_count[2]; 
    
    // Piece lists for fast generation
    int whitePieces[36];
    int whitePieceCount;
    int blackPieces[36];
    int blackPieceCount;
    int pieceIndex[256];
    
    // Helper methods
    void add_piece_to_list(int sq, Color c);
    void remove_piece_from_list(int sq, Color c);
    void move_piece_in_list(int from, int to, Color c);
};

#endif // BOARD_H
