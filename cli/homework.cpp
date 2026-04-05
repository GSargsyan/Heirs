// --- FILE: board.h ---
#ifndef BOARD_H
#define BOARD_H

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
    
    void make_null_move() {
        turn = (Color)(turn ^ 1);
        if (zobrist_initialized) zobrist_key ^= side_key;
    }
    void unmake_null_move() {
        turn = (Color)(turn ^ 1);
        if (zobrist_initialized) zobrist_key ^= side_key;
    }
    
    // Game state
    Color side_to_move() const { return turn; }
    bool is_game_over() const;
    int get_half_move_clock() const { return half_move_clock; }
    int get_history_count() const { return history_count; }
    
    // Piece lists access for fast evaluation
    const int* get_white_pieces() const { return whitePieces; }
    int get_white_piece_count() const { return whitePieceCount; }
    const int* get_black_pieces() const { return blackPieces; }
    int get_black_piece_count() const { return blackPieceCount; }
    
    // Draw detection
    bool is_repetition(int required_count = 2) const;
    bool is_fifty_moves() const;
    bool is_draw(int required_count = 2) const; // Combines repetition and 50-moves
    uint64_t get_hash() const { return zobrist_key; }
    
    void print() const;
    
    // Incremental Evaluation Tracking
    int piece_counts[2][10];
    int pst_mg[2];
    int pst_eg[2];
    int game_phase;
    int prince_sq[2];

private:
    PieceType pieces[256];
    Color colors[256];
    Color turn;
    
    // Zobrist Hashing
    uint64_t zobrist_key;
    uint64_t history[2048];
    
    // 50-move rule
    int half_move_clock;
    int half_move_history[2048];
    int history_count;
    
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

// --- FILE: eval_constants.h ---
#ifndef EVAL_CONSTANTS_H
#define EVAL_CONSTANTS_H

// strpped include // Needed for PieceType

namespace EvalConstants {
    const int DEFAULT_VAL_BABY = 100;
    const int DEFAULT_VAL_PRINCE = 20000;
    const int DEFAULT_VAL_PRINCESS = 1000;
    const int DEFAULT_VAL_PONY = 100;
    const int DEFAULT_VAL_GUARD = 420;
    const int DEFAULT_VAL_TUTOR = 335;
    const int DEFAULT_VAL_SCOUT = 700;
    const int DEFAULT_VAL_SIBLING = 300;

    const int PIECE_VALUES[] = {
        0, DEFAULT_VAL_BABY, DEFAULT_VAL_PRINCE, DEFAULT_VAL_PRINCESS, DEFAULT_VAL_PONY, 
        DEFAULT_VAL_GUARD, DEFAULT_VAL_TUTOR, DEFAULT_VAL_SCOUT, DEFAULT_VAL_SIBLING
    };

    // Phase Constants
    inline int get_piece_phase(PieceType p) {
        switch(p) {
            case BABY: return 1;
            case PRINCE: return 0;
            case PONY: return 5;
            case SIBLING: return 8;
            case TUTOR: return 10;
            case GUARD: return 12;
            case SCOUT: return 20;
            case PRINCESS: return 40;
            default: return 0;
        }
    }

    const int MAX_PHASE = 324;

    // Piece-Square Tables (White perspective)
    const int PST_BABY[144] = {
         0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
         5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
        15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15, 15,
        20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20, 20,
        25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
        25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25, 25,
        10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10, 10,
         5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,  5,
         0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,
         0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0,  0 
    };

    const int PST_PRINCE[144] = {
        10,   10,  10,  20,  20,  20,  20,  20,  20,  20,  10,  10,
         5,    5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,
        -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10,
        -20, -20, -10, -10, -10, -10, -10, -10, -10, -20, -20, -20,
        -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30, -30,
        -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40, -40,
        -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
        -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
        -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
        -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
        -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50,
        -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50, -50
    };

    const int PST_SCOUT[144] = {
          0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
          0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
          0,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   0,
          0,   10,  10,  10,  10,  10,  10,  10,  10,  10,  10,  0,
          0,   10,  10,  10,  10,  10,  10,  10,  10,  10,  10,   0,
          0,   10,  10,  10,  10,  10,  10,  10,  10,  10,  10,   0,
          0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
          0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
          0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
          0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
          0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
          0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0,
    };

    const int PST_CENTER[144] = {
        -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10,
        -10,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, -10,
        -10,   0,   5,   5,   5,   5,   5,   5,   5,   5,   0, -10,
        -10,   0,   5,  10,  10,  10,  10,  10,  10,   5,   0, -10,
        -10,   0,   5,  10,  20,  20,  20,  20,  10,   5,   0, -10,
        -10,   0,   5,  10,  20,  30,  30,  20,  10,   5,   0, -10,
        -10,   0,   5,  10,  20,  30,  30,  20,  10,   5,   0, -10,
        -10,   0,   5,  10,  20,  20,  20,  20,  10,   5,   0, -10,
        -10,   0,   5,  10,  10,  10,  10,  10,  10,   5,   0, -10,
        -10,   0,   5,   5,   5,   5,   5,   5,   5,   5,   0, -10,
        -10,   0,   0,   0,   0,   0,   0,   0,   0,   0,   0, -10,
        -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10, -10
    };

    const int PST_BABY_EG[144] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    const int PST_PRINCE_EG[144] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    const int PST_CENTER_EG[144] = {
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
    };

    inline int get_pst_value(PieceType p, int pst_idx, bool is_eg) {
        if (!is_eg) {
            switch (p) {
                case BABY: return PST_BABY[pst_idx];
                case PRINCE: return PST_PRINCE[pst_idx];
                case SCOUT: return PST_CENTER[pst_idx]; 
                default: return PST_CENTER[pst_idx];
            }
        } else {
            switch (p) {
                case BABY: return PST_BABY_EG[pst_idx];
                case PRINCE: return PST_PRINCE_EG[pst_idx];
                case SCOUT: return PST_CENTER_EG[pst_idx];
                default: return PST_CENTER_EG[pst_idx];
            }
        }
    }
}

#endif // EVAL_CONSTANTS_H

// --- FILE: board.cpp ---
// strpped include
// strpped include
#include <iomanip>
#include <cstring>

void Board::add_piece_to_list(int sq, Color c) {
    if (c == WHITE) {
        whitePieces[whitePieceCount] = sq;
        pieceIndex[sq] = whitePieceCount;
        whitePieceCount++;
    } else if (c == BLACK) {
        blackPieces[blackPieceCount] = sq;
        pieceIndex[sq] = blackPieceCount;
        blackPieceCount++;
    }
}

void Board::remove_piece_from_list(int sq, Color c) {
    int idx = pieceIndex[sq];
    if (c == WHITE) {
        int last_sq = whitePieces[--whitePieceCount];
        whitePieces[idx] = last_sq;
        pieceIndex[last_sq] = idx;
    } else if (c == BLACK) {
        int last_sq = blackPieces[--blackPieceCount];
        blackPieces[idx] = last_sq;
        pieceIndex[last_sq] = idx;
    }
}

void Board::move_piece_in_list(int from, int to, Color c) {
    int idx = pieceIndex[from];
    if (c == WHITE) {
        whitePieces[idx] = to;
    } else if (c == BLACK) {
        blackPieces[idx] = to;
    }
    pieceIndex[to] = idx;
}

void Board::set_piece(int sq, PieceType p, Color c) {
    if (sq < 0 || sq >= 256) return;
    if (pieces[sq] == PRINCE) {
        prince_count[colors[sq]]--;
    }
    if (pieces[sq] != NO_PIECE && pieces[sq] != OFFBOARD) {
        remove_piece_from_list(sq, colors[sq]);
        piece_counts[colors[sq]][pieces[sq]]--;
        game_phase -= EvalConstants::get_piece_phase(pieces[sq]);
        int pst_idx = (colors[sq] == WHITE) ? ((sq >> 4) * 12 + (sq & 15)) : ((11 - (sq >> 4)) * 12 + (sq & 15));
        pst_mg[colors[sq]] -= EvalConstants::get_pst_value(pieces[sq], pst_idx, false);
        pst_eg[colors[sq]] -= EvalConstants::get_pst_value(pieces[sq], pst_idx, true);
    }
    pieces[sq] = p;
    colors[sq] = c;
    if (p == PRINCE) {
        prince_count[c]++;
        prince_sq[c] = sq;
    }
    if (p != NO_PIECE && p != OFFBOARD) {
        add_piece_to_list(sq, c);
        piece_counts[c][p]++;
        game_phase += EvalConstants::get_piece_phase(p);
        int pst_idx = (c == WHITE) ? ((sq >> 4) * 12 + (sq & 15)) : ((11 - (sq >> 4)) * 12 + (sq & 15));
        pst_mg[c] += EvalConstants::get_pst_value(p, pst_idx, false);
        pst_eg[c] += EvalConstants::get_pst_value(p, pst_idx, true);
    }
}

void Board::clear_piece(int sq) {
    if (sq < 0 || sq >= 256) return;
    if (pieces[sq] == PRINCE) {
        prince_count[colors[sq]]--;
        if (prince_sq[colors[sq]] == sq) prince_sq[colors[sq]] = -1;
    }
    if (pieces[sq] != NO_PIECE && pieces[sq] != OFFBOARD) {
        remove_piece_from_list(sq, colors[sq]);
        piece_counts[colors[sq]][pieces[sq]]--;
        game_phase -= EvalConstants::get_piece_phase(pieces[sq]);
        int pst_idx = (colors[sq] == WHITE) ? ((sq >> 4) * 12 + (sq & 15)) : ((11 - (sq >> 4)) * 12 + (sq & 15));
        pst_mg[colors[sq]] -= EvalConstants::get_pst_value(pieces[sq], pst_idx, false);
        pst_eg[colors[sq]] -= EvalConstants::get_pst_value(pieces[sq], pst_idx, true);
    }
    pieces[sq] = NO_PIECE;       
    colors[sq] = COLOR_NB;
}

PieceType Board::piece_at(int sq) const {
    if (sq < 0 || sq >= 256) return OFFBOARD;
    return pieces[sq];
}

Color Board::color_at(int sq) const {
    if (sq < 0 || sq >= 256) return COLOR_OFFBOARD;
    return colors[sq];
}

Board::Board() {
    reset();
}

void Board::reset() {
    whitePieceCount = 0;
    blackPieceCount = 0;
    for (int i=0; i<10; ++i) { piece_counts[0][i] = 0; piece_counts[1][i] = 0; }
    pst_mg[0] = 0; pst_mg[1] = 0;
    pst_eg[0] = 0; pst_eg[1] = 0;
    game_phase = 0;
    prince_sq[0] = -1; prince_sq[1] = -1;
    
    for(int i=0; i<256; ++i) {
        pieceIndex[i] = -1;
        if ((i & 15) >= 12 || (i >> 4) >= 12) {
            pieces[i] = OFFBOARD;
            colors[i] = COLOR_OFFBOARD;
        } else {
            pieces[i] = NO_PIECE;
            colors[i] = COLOR_NB;
        }
    }
    prince_count[WHITE] = 0;
    prince_count[BLACK] = 0;
    half_move_clock = 0;
    history_count = 0;
    turn = WHITE;
    
    PieceType placement[] = {
        GUARD, PONY, TUTOR, SCOUT, SIBLING, PRINCESS, PRINCE, SIBLING, SCOUT, TUTOR, PONY, GUARD
    };
    init_zobrist();
    zobrist_key = 0;
    
    // White Row 1 (index 0-11 in 16x16 is 0-11)
    for(int i=0; i<12; ++i) {
        set_piece(i, placement[i], WHITE);
        zobrist_key ^= piece_keys[i][WHITE][placement[i]];
    }
    // White Row 2 (index 16-27 in 16x16)
    for(int i=0; i<12; ++i) {
        set_piece(16 + i, BABY, WHITE);
        zobrist_key ^= piece_keys[16+i][WHITE][BABY];
    }
    
    // Black Row 12 (index 176-187 in 16x16)
    for(int i=0; i<12; ++i) {
        set_piece(176 + i, placement[i], BLACK);
        zobrist_key ^= piece_keys[176+i][BLACK][placement[i]];
    }
    // Black Row 11 (index 160-171 in 16x16)
    for(int i=0; i<12; ++i) {
        set_piece(160 + i, BABY, BLACK);
        zobrist_key ^= piece_keys[160+i][BLACK][BABY];
    }
}

void Board::print() const {
    const char* white_pieces[] = {".", "●", "♚", "♛", "♞", "♜", "♝", "▲", "◆", " "};
    const char* black_pieces[] = {".", "○", "♔", "♕", "♘", "♖", "♗", "△", "◇", " "};
    
    std::cout << "    a b c d e f g h j k m n\n";
    std::cout << "  -------------------------\n";
    
    for (int row = 11; row >= 0; --row) {
        std::cout << std::setw(2) << (row + 1) << "|";
        for (int col = 0; col < 12; ++col) {
            int sq = row * 16 + col;
            PieceType p = pieces[sq];
            Color c = colors[sq];
            const char* s = ".";
            if (p != NO_PIECE && p != OFFBOARD) {
                if (c == WHITE) {
                    s = white_pieces[p];
                } else {
                    s = black_pieces[p];
                }
            }
            std::cout << " " << s;
        }
        std::cout << " |" << std::setw(2) << (row + 1) << "\n";
    }
    std::cout << "  -------------------------\n";
    std::cout << "   a b c d e f g h j k m n\n";
}

// Move Generation
static const int DIRECTIONS[8] = {
    16, -16, 1, -1, // Orthogonal: N, S, E, W
    17, 15, -15, -17 // Diagonal: NE, NW, SE, SW
};

#define IS_OK(sq) ((sq) >= 0 && (sq) < 256 && pieces[(sq)] != OFFBOARD)

void Board::generate_moves(MoveList& list) const {
    list.count = 0;
    
    const int* piece_list = (turn == WHITE) ? whitePieces : blackPieces;
    int count = (turn == WHITE) ? whitePieceCount : blackPieceCount;
    
    for (int i = 0; i < count; ++i) {
        int sq = piece_list[i];
        PieceType p = pieces[sq];
        
        switch (p) {
            case BABY: {
                int forward = (turn == WHITE) ? 16 : -16;
                int target = sq + forward;
                if (IS_OK(target)) {
                    Color target_color = colors[target];
                    if (target_color != turn) {
                        list.add(sq, target, pieces[target]);
                        if (target_color == COLOR_NB) { 
                            int target2 = target + forward;
                            if (IS_OK(target2)) {
                                if (colors[target2] != turn) {
                                    list.add(sq, target2, pieces[target2]);
                                }
                            }
                        }
                    }
                }
                break;
            }
            case PRINCE: {
                for (int dir : DIRECTIONS) {
                    int target = sq + dir;
                    if (IS_OK(target)) {
                         if (colors[target] != turn) {
                             list.add(sq, target, pieces[target]);
                         }
                    }
                }
                break;
            }
            case PRINCESS: {
                for (int dir : DIRECTIONS) {
                    int target = sq;
                    for (int k = 1; k <= 3; ++k) {
                        target += dir;
                        if (!IS_OK(target)) break;
                        Color target_color = colors[target];
                        if (target_color == turn) break;
                        list.add(sq, target, pieces[target]);
                        if (target_color != COLOR_NB) break;
                    }
                }
                break;
            }
            case PONY: {
                for (int k = 4; k < 8; ++k) {
                    int target = sq + DIRECTIONS[k];
                    if (IS_OK(target)) {
                        if (colors[target] != turn) {
                            list.add(sq, target, pieces[target]);
                        }
                    }
                }
                break;
            }
            case GUARD: {
                for (int k = 0; k < 4; ++k) {
                    int target = sq;
                    for (int step = 1; step <= 2; ++step) {
                        target += DIRECTIONS[k];
                        if (!IS_OK(target)) break;
                        Color target_color = colors[target];
                        if (target_color == turn) break; 
                        list.add(sq, target, pieces[target]);
                        if (target_color != COLOR_NB) break;
                    }
                }
                break;
            }
            case TUTOR: {
                for (int k = 4; k < 8; ++k) {
                    int target = sq;
                    for (int step = 1; step <= 2; ++step) {
                        target += DIRECTIONS[k];
                        if (!IS_OK(target)) break;
                        Color target_color = colors[target];
                        if (target_color == turn) break;
                        list.add(sq, target, pieces[target]);
                        if (target_color != COLOR_NB) break;
                    }
                }
                break;
            }
            case SCOUT: {
                int forward = (turn == WHITE) ? 16 : -16;
                for (int k = 1; k <= 3; ++k) {
                    int center = sq + forward * k;
                    if (center < 0 || center >= 256) break;
                    if ((center >> 4) >= 12 || (center >> 4) < 0) break;
                    for (int dc = -1; dc <= 1; ++dc) {
                        int target = center + dc;
                        if (IS_OK(target)) {
                            if (colors[target] != turn) {
                                list.add(sq, target, pieces[target]);
                            }
                        }
                    }
                }
                break;
            }
            case SIBLING: {
                 for (int dir : DIRECTIONS) {
                    int target = sq + dir;
                    if (IS_OK(target)) {
                         if (colors[target] != turn) {
                             bool has_friend = false;
                             for (int adj_off : DIRECTIONS) {
                                 int adj_sq = target + adj_off;
                                 if (adj_sq == sq) continue;
                                 if (IS_OK(adj_sq)) {
                                     if (colors[adj_sq] == turn) {
                                         has_friend = true;
                                         break;
                                     }
                                 }
                             }
                             if (has_friend) {
                                 list.add(sq, target, pieces[target]);
                             }
                         }
                    }
                }
                break;
            }
            default: break;
        }
    }
}

uint64_t Board::piece_keys[256][2][10];
uint64_t Board::side_key;
bool Board::zobrist_initialized = false;

static uint64_t rand64() {
    static uint64_t seed = 123456789;
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    return seed;
}

void Board::init_zobrist() {
    if (zobrist_initialized) return;
    for(int sq=0; sq<256; ++sq) {
        for(int c=0; c<2; ++c) {
            for(int p=0; p<10; ++p) {
                piece_keys[sq][c][p] = rand64();
            }
        }
    }
    side_key = rand64();
    zobrist_initialized = true;
}

void Board::make_move(const Move& m) {
    if (history_count < 2048) {
        history[history_count] = zobrist_key;
        half_move_history[history_count] = half_move_clock;
        history_count++;
    }
    
    PieceType p = pieces[m.from];
    Color c = colors[m.from];
    
    if ((pieces[m.to] != NO_PIECE && pieces[m.to] != OFFBOARD) || p == BABY) {
        half_move_clock = 0;
    } else {
        half_move_clock++;
    }
    
    zobrist_key ^= piece_keys[m.from][c][p];
    
    int from_idx = (c == WHITE) ? ((m.from >> 4) * 12 + (m.from & 15)) : ((11 - (m.from >> 4)) * 12 + (m.from & 15));
    int to_idx = (c == WHITE) ? ((m.to >> 4) * 12 + (m.to & 15)) : ((11 - (m.to >> 4)) * 12 + (m.to & 15));
    
    pst_mg[c] -= EvalConstants::get_pst_value(p, from_idx, false);
    pst_eg[c] -= EvalConstants::get_pst_value(p, from_idx, true);
    pst_mg[c] += EvalConstants::get_pst_value(p, to_idx, false);
    pst_eg[c] += EvalConstants::get_pst_value(p, to_idx, true);
    
    if (p == PRINCE) {
        prince_sq[c] = m.to;
    }
    
    if (pieces[m.to] != NO_PIECE && pieces[m.to] != OFFBOARD) {
        PieceType cap = pieces[m.to];
        Color cap_c = colors[m.to];
        int cap_idx = (cap_c == WHITE) ? ((m.to >> 4) * 12 + (m.to & 15)) : ((11 - (m.to >> 4)) * 12 + (m.to & 15));
        
        piece_counts[cap_c][cap]--;
        game_phase -= EvalConstants::get_piece_phase(cap);
        pst_mg[cap_c] -= EvalConstants::get_pst_value(cap, cap_idx, false);
        pst_eg[cap_c] -= EvalConstants::get_pst_value(cap, cap_idx, true);
        
        zobrist_key ^= piece_keys[m.to][cap_c][cap];
        if (m.captured == PRINCE) {
            prince_count[(Color)(1-c)]--;
        }
        remove_piece_from_list(m.to, cap_c);
    }
    
    zobrist_key ^= piece_keys[m.to][c][p];
    pieces[m.from] = NO_PIECE;
    colors[m.from] = COLOR_NB;
    
    pieces[m.to] = p;
    colors[m.to] = c;
    move_piece_in_list(m.from, m.to, c);
    
    zobrist_key ^= side_key;
    turn = (Color)(1 - turn);
}

void Board::unmake_move(const Move& m) {
    turn = (Color)(1 - turn);
    zobrist_key ^= side_key;
    
    PieceType p = pieces[m.to];
    Color c = colors[m.to];
    
    int from_idx = (c == WHITE) ? ((m.from >> 4) * 12 + (m.from & 15)) : ((11 - (m.from >> 4)) * 12 + (m.from & 15));
    int to_idx = (c == WHITE) ? ((m.to >> 4) * 12 + (m.to & 15)) : ((11 - (m.to >> 4)) * 12 + (m.to & 15));
    
    pst_mg[c] -= EvalConstants::get_pst_value(p, to_idx, false);
    pst_eg[c] -= EvalConstants::get_pst_value(p, to_idx, true);
    pst_mg[c] += EvalConstants::get_pst_value(p, from_idx, false);
    pst_eg[c] += EvalConstants::get_pst_value(p, from_idx, true);
    
    if (p == PRINCE) {
        prince_sq[c] = m.from;
    }
    
    zobrist_key ^= piece_keys[m.to][c][p];
    
    pieces[m.from] = p;
    colors[m.from] = c;
    zobrist_key ^= piece_keys[m.from][c][p];
    
    move_piece_in_list(m.to, m.from, c);
    
    pieces[m.to] = m.captured;
    if (m.captured != NO_PIECE && m.captured != OFFBOARD) {
        Color cap_c = (Color)(1 - c);
        colors[m.to] = cap_c;
        int cap_idx = (cap_c == WHITE) ? ((m.to >> 4) * 12 + (m.to & 15)) : ((11 - (m.to >> 4)) * 12 + (m.to & 15));
        
        piece_counts[cap_c][m.captured]++;
        game_phase += EvalConstants::get_piece_phase(m.captured);
        pst_mg[cap_c] += EvalConstants::get_pst_value(m.captured, cap_idx, false);
        pst_eg[cap_c] += EvalConstants::get_pst_value(m.captured, cap_idx, true);
        
        zobrist_key ^= piece_keys[m.to][cap_c][m.captured];
        if (m.captured == PRINCE) {
            prince_count[cap_c]++;
            prince_sq[cap_c] = m.to; 
        }
        add_piece_to_list(m.to, cap_c);
    } else {
        colors[m.to] = COLOR_NB;
    }
    
    if (history_count > 0) {
        history_count--;
        half_move_clock = half_move_history[history_count];
    }
}

bool Board::is_game_over() const {
    if (prince_count[WHITE] == 0) return true;
    if (prince_count[BLACK] == 0) return true;
    return false;
}

bool Board::is_repetition(int required_count) const {
    int limit = half_move_clock;
    if (limit > history_count) limit = history_count;
    int count = 0;
    for (int i = 1; i <= limit; ++i) {
        if (history[history_count - i] == zobrist_key) {
            count++;
            if (count >= required_count) return true; 
        }
    }
    return false;
}

bool Board::is_fifty_moves() const {
    return half_move_clock >= 100;
}

bool Board::is_draw(int required_count) const {
    if (is_fifty_moves()) return true;
    if (is_repetition(required_count)) return true;
    return false;
}

// --- FILE: engine_v1.h ---
#ifndef ENGINE_V1_H
#define ENGINE_V1_H

// strpped include
#include <chrono>
#include <vector>

enum TTBoundV1 {
    BOUND_NONE_V1,
    BOUND_EXACT_V1,
    BOUND_LOWER_V1,
    BOUND_UPPER_V1
};

struct TTEntryV1 {
    uint64_t key;
    int depth;
    int score;
    TTBoundV1 bound;
    Move best_move;
};

class TranspositionTableV1 {
public:
    TranspositionTableV1(size_t size_mb) {
        size_t num_entries = (size_mb * 1024 * 1024) / sizeof(TTEntryV1);
        table.resize(num_entries);
        clear();
    }

    void clear() {
        for (auto& entry : table) {
            entry.key = 0;
            entry.depth = -1;
            entry.score = 0;
            entry.bound = BOUND_NONE_V1;
            entry.best_move = Move();
        }
    }

    void store(uint64_t key, int depth, int score, TTBoundV1 bound, Move best_move) {
        size_t index = key % table.size();
        if (table[index].key == 0 || depth >= table[index].depth) {
            table[index].key = key;
            table[index].depth = depth;
            table[index].score = score;
            table[index].bound = bound;
            table[index].best_move = best_move;
        }
    }

    bool probe(uint64_t key, TTEntryV1& out_entry) const {
        size_t index = key % table.size();
        if (table[index].key == key) {
            out_entry = table[index];
            return true;
        }
        return false;
    }

private:
    std::vector<TTEntryV1> table;
};

class EngineV1 {
public:
    EngineV1();
    
    // Search for the best move within the time limit (or depth limit if time_limit <= 0)
    Move search(Board& b, double time_limit, int max_depth = 100);

    long long get_nodes_visited() const;
    int get_max_depth() const;
    
    // Set custom piece values
    void set_piece_values(const int values[9]);
    
    // Pass time constraints to adapt draw behavior
    void set_times(double my_time, double opp_time) {
        my_time_left = my_time;
        opp_time_left = opp_time;
    }
    
private:
    int piece_values[9];

    TranspositionTableV1 tt;

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
    
    double my_time_left = 0.0;
    double opp_time_left = 0.0;
};

#endif // ENGINE_V1_H

// --- FILE: engine_v1.cpp ---
// strpped include
#include <algorithm>
#include <iostream>
#include <chrono>
#include <vector>
#include <cmath>

using namespace std::chrono;

// strpped include

long long EngineV1::get_nodes_visited() const { return nodes_visited; }
int EngineV1::get_max_depth() const { return last_depth; }

EngineV1::EngineV1() : nodes_visited(0), last_depth(0), tt(16) {
    for (int i = 0; i < 9; ++i) {
        piece_values[i] = EvalConstants::PIECE_VALUES[i];
    }
}

void EngineV1::set_piece_values(const int values[9]) {
    for (int i = 0; i < 9; ++i) {
        piece_values[i] = values[i];
    }
}

inline int to_12x12(int sq) {
    int r = sq >> 4;
    int c = sq & 15;
    return r * 12 + c;
}

// Helper to flip the board index for Black (mirror vertically)
// Assuming board is 0-143, row 0 is bottom.
inline int mirror_sq(int sq) {
    int r = sq / 12;
    int c = sq % 12;
    return (11 - r) * 12 + c;
}

int EngineV1::evaluate(const Board& b) {
    int game_phase = b.game_phase;

    int white_prince_sq = b.prince_sq[WHITE];
    int black_prince_sq = b.prince_sq[BLACK];

    // 1. MATERIAL & BASE PST
    int w_mg = b.pst_mg[WHITE];
    int w_eg = b.pst_eg[WHITE];
    int b_mg = b.pst_mg[BLACK];
    int b_eg = b.pst_eg[BLACK];

    for (int p = 1; p < 10; ++p) {
        if (p == OFFBOARD) continue;
        w_mg += b.piece_counts[WHITE][p] * piece_values[p];
        w_eg += b.piece_counts[WHITE][p] * piece_values[p];
        b_mg += b.piece_counts[BLACK][p] * piece_values[p];
        b_eg += b.piece_counts[BLACK][p] * piece_values[p];
    }

    auto eval_list_dyn = [&](const int* list, int count, Color c, int enemy_prince_sq) {
        int dyn_mg = 0;
        int dyn_eg = 0;
        for (int i = 0; i < count; ++i) {
            int sq = list[i];
            PieceType p = b.piece_at(sq);
            int pst_sq = to_12x12(sq);
            int pst_idx = (c == WHITE) ? pst_sq : mirror_sq(pst_sq);

            switch (p) {
                case BABY:
                    {
                        int rank = pst_idx / 12;
                        if (rank == 11) { dyn_mg -= 100; dyn_eg -= 100; }
                        else if (rank == 10) { dyn_mg -= 90; dyn_eg -= 90; }
                        else if (rank == 9) { dyn_mg -= 50; dyn_eg -= 50; }
                        else if (rank == 8) { dyn_mg -= 20; dyn_eg -= 20; }
                        
                        // Penalty if friendly non-baby piece is blocking it
                        int forward_sq = (c == WHITE) ? (sq + 16) : (sq - 16);
                        int fr = forward_sq >> 4;
                        int fc = forward_sq & 15;
                        if (fr >= 0 && fr < 12 && fc >= 0 && fc < 12) {
                            if (b.color_at(forward_sq) == c && b.piece_at(forward_sq) != BABY) {
                                dyn_mg -= 40; 
                                dyn_eg -= 40;
                            }
                        }
                    }
                    break;
                default:
                    break; 
            }
            // Tropism: pieces should tend toward enemy prince in endgame
            if (p != PRINCE && enemy_prince_sq != -1) {
                int r1 = (sq >> 4) & 15;
                int c1 = sq & 15;
                int r2 = (enemy_prince_sq >> 4) & 15;
                int c2 = enemy_prince_sq & 15;
                int dist = std::abs(r1 - r2) + std::abs(c1 - c2);
                
                // Reward closeness
                dyn_eg += std::max(0, (24 - dist) * 2);
            }
        }
        return std::make_pair(dyn_mg, dyn_eg);
    };

    auto w_dyn = eval_list_dyn(b.get_white_pieces(), b.get_white_piece_count(), WHITE, black_prince_sq);
    auto b_dyn = eval_list_dyn(b.get_black_pieces(), b.get_black_piece_count(), BLACK, white_prince_sq);

    w_mg += w_dyn.first;
    w_eg += w_dyn.second;
    b_mg += b_dyn.first;
    b_eg += b_dyn.second;

    int mg_score = w_mg - b_mg;
    int eg_score = w_eg - b_eg;

    // 2. ENEMY PRINCE MOBILITY TROPISM
    // Higher empty sqcount = more mobility
    auto count_empty_neighbors = [&](int sq) {
        if (sq == -1) return 0;
        int empty_count = 0;
        int r = (sq >> 4) & 15;
        int c = sq & 15;
        int dr[] = {-1, -1, -1, 0, 0, 1, 1, 1};
        int dc[] = {-1, 0, 1, -1, 1, -1, 0, 1};
        for (int i = 0; i < 8; ++i) {
            int nr = r + dr[i];
            int nc = c + dc[i];
            if (nr >= 0 && nr < 12 && nc >= 0 && nc < 12) {
                int nsq = (nr << 4) | nc;
                if (b.piece_at(nsq) == NO_PIECE) empty_count++;
            }
        }
        return empty_count;
    };

    int w_prince_moves = count_empty_neighbors(white_prince_sq);
    int b_prince_moves = count_empty_neighbors(black_prince_sq);
    
    // Less mobility for enemy prince is better for us
    // So give us points if our prince has more mobility, and penalize if enemy has more mobility.
    eg_score += w_prince_moves * 10;
    eg_score -= b_prince_moves * 10;

    // --- EARLY GAME DEVELOPMENT PENALTY ---
    if (game_phase > 200) {
        if (b.piece_at(5) != PRINCESS) {
            int w_undeveloped = 0;
            if (b.piece_at(1) == PONY) w_undeveloped++;
            if (b.piece_at(2) == TUTOR) w_undeveloped++;
            if (b.piece_at(4) == SIBLING) w_undeveloped++;
            if (b.piece_at(7) == SIBLING) w_undeveloped++;
            if (b.piece_at(9) == TUTOR) w_undeveloped++;
            if (b.piece_at(10) == PONY) w_undeveloped++;
            mg_score -= w_undeveloped * 30;
        }
        
        if (b.piece_at(181) != PRINCESS) {
            int b_undeveloped = 0;
            if (b.piece_at(177) == PONY) b_undeveloped++;
            if (b.piece_at(178) == TUTOR) b_undeveloped++;
            if (b.piece_at(180) == SIBLING) b_undeveloped++;
            if (b.piece_at(183) == SIBLING) b_undeveloped++;
            if (b.piece_at(185) == TUTOR) b_undeveloped++;
            if (b.piece_at(186) == PONY) b_undeveloped++;
            mg_score += b_undeveloped * 30; // penalize black
        }
    }

    // --- MOP-UP EVALUATION (Force Checkmate) ---
    int winning_threshold = 400;

    if (eg_score > winning_threshold && game_phase < 150) {
        int w_prince_r = (white_prince_sq >> 4) & 15;
        int w_prince_c = white_prince_sq & 15;
        int b_prince_r = (black_prince_sq >> 4) & 15;
        int b_prince_c = black_prince_sq & 15;

        int center_dist = std::abs(b_prince_r - 5) + std::abs(b_prince_c - 5) + 
                          std::abs(b_prince_r - 6) + std::abs(b_prince_c - 6);
        eg_score += center_dist * 20;

        int princes_dist = std::abs(w_prince_r - b_prince_r) + std::abs(w_prince_c - b_prince_c);
        eg_score += (24 - princes_dist) * 10;
    }
    else if (eg_score < -winning_threshold && game_phase < 150) {
        int w_prince_r = (white_prince_sq >> 4) & 15;
        int w_prince_c = white_prince_sq & 15;
        int b_prince_r = (black_prince_sq >> 4) & 15;
        int b_prince_c = black_prince_sq & 15;

        int center_dist = std::abs(w_prince_r - 5) + std::abs(w_prince_c - 5) + 
                          std::abs(w_prince_r - 6) + std::abs(w_prince_c - 6);
        eg_score -= center_dist * 20;

        int princes_dist = std::abs(b_prince_r - w_prince_r) + std::abs(b_prince_c - w_prince_c);
        eg_score -= (24 - princes_dist) * 10;
    }

    // 3. TAPERED BLEND
    int phase = std::min(game_phase, EvalConstants::MAX_PHASE);
    int final_score = (mg_score * phase + eg_score * (EvalConstants::MAX_PHASE - phase)) / EvalConstants::MAX_PHASE;

    return (b.side_to_move() == WHITE) ? final_score : -final_score;
}

bool EngineV1::is_time_up() {
    if (time_limit_sec <= 0.0) return false;
    auto now = steady_clock::now();
    duration<double> elapsed = now - start_time;
    return elapsed.count() >= time_limit_sec;
}

// RAII wrapper for move making/unmaking
struct ScopedMoveV1 {
    Board& b;
    Move m;
    ScopedMoveV1(Board& board, const Move& move) : b(board), m(move) {
        b.make_move(m);
    }
    ~ScopedMoveV1() {
        b.unmake_move(m);
    }
};

struct ScopedNullMoveV1 {
    Board& b;
    ScopedNullMoveV1(Board& board) : b(board) {
        b.make_null_move();
    }
    ~ScopedNullMoveV1() {
        b.unmake_null_move();
    }
};

void EngineV1::score_moves(const MoveList& moves, int* move_scores, const Board& b, int ply, Move tt_move) {
    if (moves.count == 0) return;
    
    Color side = b.side_to_move();
    
    for (int i = 0; i < moves.count; ++i) {
        Move m = moves.moves[i];
        
        if (m == tt_move) {
            move_scores[i] = 10000000;
            continue;
        }

        PieceType attacker = b.piece_at(m.from);
        
        if (m.captured != NO_PIECE) {
            // Band 1: Captures get a massive score boost so they are searched first
            move_scores[i] = 1000000 + (piece_values[m.captured] * 10) - piece_values[attacker];
        } else {
            // Quiet moves
            if (ply < MAX_PLY && m == killer_moves[ply][0]) {
                move_scores[i] = 900000;
            } else if (ply < MAX_PLY && m == killer_moves[ply][1]) {
                move_scores[i] = 800000;
            } else {
                int hist = history_table[side][m.from][m.to];
                if (hist > 690000) hist = 690000; // Cap safely
                move_scores[i] = hist;
            }
        }
        
        // Band 0: Scout moves take absolute priority
        if (attacker == SCOUT) {
            move_scores[i] += 5000000;
        }
    }
}



// Alpha-Beta Search
int EngineV1::alphabeta(Board& b, int depth, int alpha, int beta, int ply) {
    nodes_visited++;
    
    // Check for draw by repetition or 50-move rule
    // We must check this BEFORE the TT probe, because a position reached by 
    // a cyclic path is a draw (score 0), whereas the same position reached 
    // from a standard path might be evaluated and stored in the TT as winning (+x).
    if (ply > 0 && b.is_draw(1)) {
        if (my_time_left > opp_time_left + 2.0) return 20000 - ply; // Draw is win on time
        if (opp_time_left > my_time_left + 2.0) return -20000 + ply; // Draw is loss on time
        return 0; // Returning 0 on 1-fold repetition strictly avoids cycles
    }

    int original_alpha = alpha;
    TTEntryV1 tt_entry;
    Move tt_move;
    if (tt.probe(b.get_hash(), tt_entry)) {
        tt_move = tt_entry.best_move;
        if (tt_entry.depth >= depth) {
            if (tt_entry.bound == BOUND_EXACT_V1)
                return tt_entry.score;
            if (tt_entry.bound == BOUND_LOWER_V1)
                alpha = std::max(alpha, tt_entry.score);
            else if (tt_entry.bound == BOUND_UPPER_V1)
                beta = std::min(beta, tt_entry.score);
            if (alpha >= beta)
                return tt_entry.score;
        }
    }
    
    if (depth == 0) {
        return quiescence(b, alpha, beta, ply);
    }

    if (b.is_game_over()) {
        int eval = evaluate(b);
        // Prefer faster mates (higher remaining depth = faster mate)
        // With relative scoring, being materially winning guarantees +score > 10000
        if (eval > 10000) {
            eval += depth * 100;
        } else if (eval < -10000) {
            eval -= depth * 100;
        }
        return eval;
    }

    // Null Move Pruning
    if (depth >= 3 && b.game_phase > 100) {
        int stand_pat = evaluate(b);
        if (stand_pat >= beta) {
            int R = 2; // Fixed depth reduction
            int null_score;
            {
                ScopedNullMoveV1 snm(b);
                null_score = -alphabeta(b, depth - 1 - R, -beta, -beta + 1, ply + 1);
            }
            
            if (null_score >= beta) {
                return beta; // Fail-high, skip move generation
            }
        }
    }
    
    if ((nodes_visited & 1023) == 0) {
        if (is_time_up()) throw std::runtime_error("Time limit exceeded");
    }
    
    Color side = b.side_to_move();
    MoveList moves;
    b.generate_moves(moves);
    
    
    if (moves.count == 0) {
        return 0; // Draw
    }
    
    // Futility Pruning Setup
    bool futil_prune = false;
    int futil_stand_pat = -2000000;
    if (depth == 1 && !b.is_game_over() && !b.is_draw()) {
        futil_stand_pat = evaluate(b);
        int margin = piece_values[GUARD];
        if (futil_stand_pat + margin < alpha) {
            futil_prune = true;
        }
    }

    // Move scoring
    int move_scores[256];
    score_moves(moves, move_scores, b, ply, tt_move);
    
    // Safe initialization bounding in case all moves are verified futile
    int max_eval = futil_prune ? futil_stand_pat : -2000000;
    Move best_move;
    for (int i = 0; i < moves.count; ++i) {
        // Lazy sorting inline
        int best_idx = i;
        for (int j = i + 1; j < moves.count; ++j) {
            if (move_scores[j] > move_scores[best_idx]) best_idx = j;
        }
        std::swap(move_scores[i], move_scores[best_idx]);
        std::swap(moves.moves[i], moves.moves[best_idx]);
        
        Move move = moves.moves[i];
        
        // Futility Pruning: purely skip unpromising quiet moves
        if (futil_prune && move.captured == NO_PIECE) {
            continue;
        }

        int eval;
        {
            ScopedMoveV1 sm(b, move);
            if (i == 0) {
                // Full window search for first move (PV node)
                eval = -alphabeta(b, depth - 1, -beta, -alpha, ply + 1);
            } else {
                bool is_capture = (move.captured != NO_PIECE);
                bool is_killer = (ply < MAX_PLY && (move == killer_moves[ply][0] || move == killer_moves[ply][1]));
                bool do_lmr = (i >= 4 && depth >= 3 && !is_capture && !is_killer);

                if (do_lmr) {
                    // LMR zero-window search
                    eval = -alphabeta(b, depth - 2, -alpha - 1, -alpha, ply + 1);
                    if (eval > alpha) {
                        // Failed high, needs standard depth zero-window search
                        eval = -alphabeta(b, depth - 1, -alpha - 1, -alpha, ply + 1);
                    }
                } else {
                    // Standard depth zero-window search (null-window)
                    eval = -alphabeta(b, depth - 1, -alpha - 1, -alpha, ply + 1);
                }
                
                if (eval > alpha && eval < beta) {
                    // Full-window re-search if zero-window failed high
                    eval = -alphabeta(b, depth - 1, -beta, -alpha, ply + 1);
                }
            }
        }
        
        if (eval > max_eval) {
            max_eval = eval;
            best_move = move;
        }
        alpha = std::max(alpha, eval);
        if (alpha >= beta) {
            if (move.captured == NO_PIECE) {
                history_table[side][move.from][move.to] += depth * depth;
                if (ply < MAX_PLY && !(killer_moves[ply][0] == move)) {
                    killer_moves[ply][1] = killer_moves[ply][0];
                    killer_moves[ply][0] = move;
                }
            }
            break; // Beta Cutoff
        }
    }
    
    TTBoundV1 bound;
    if (max_eval <= original_alpha) bound = BOUND_UPPER_V1;
    else if (max_eval >= beta) bound = BOUND_LOWER_V1;
    else bound = BOUND_EXACT_V1;
    tt.store(b.get_hash(), depth, max_eval, bound, best_move);
    
    return max_eval;
}

// Quiescence Search
int EngineV1::quiescence(Board& b, int alpha, int beta, int ply) {
    nodes_visited++;
    
    if ((nodes_visited & 1023) == 0) {
        if (is_time_up()) throw std::runtime_error("Time limit exceeded");
    }

    if (b.is_draw(1)) {
        if (my_time_left > opp_time_left + 2.0) return 20000 - ply; // Draw is win on time
        if (opp_time_left > my_time_left + 2.0) return -20000 + ply; // Draw is loss on time
        return 0; // 1-fold repetition strictly avoids cycles inside search
    }

    int stand_pat = evaluate(b);

    if (stand_pat >= beta) return beta;
    if (alpha < stand_pat) alpha = stand_pat;

    if (b.is_game_over()) {
        return stand_pat;
    }

    MoveList moves;
    b.generate_moves(moves);
    

    if (moves.count == 0) return stand_pat;

    int capture_count = 0;
    Move captures[256];
    int capture_scores[256];
    
    for (int i = 0; i < moves.count; ++i) {
        Move m = moves.moves[i];
        if (m.captured != NO_PIECE) {
            captures[capture_count] = m;
            PieceType attacker = b.piece_at(m.from);
            // Material difference heuristic for QS move ordering
            capture_scores[capture_count] = 1000000 + (piece_values[m.captured] * 10) - piece_values[attacker];
            capture_count++;
        }
    }

    int max_eval = stand_pat;
    for (int i = 0; i < capture_count; ++i) {
        int best_idx = i;
        for (int j = i + 1; j < capture_count; ++j) {
            if (capture_scores[j] > capture_scores[best_idx]) best_idx = j;
        }
        std::swap(capture_scores[i], capture_scores[best_idx]);
        std::swap(captures[i], captures[best_idx]);
        
        Move move = captures[i];
        


        int eval;
        {
            ScopedMoveV1 sm(b, move);
            eval = -quiescence(b, -beta, -alpha, ply + 1);
        }
        max_eval = std::max(max_eval, eval);
        alpha = std::max(alpha, eval);
        
        if (alpha >= beta) break; // Beta Cutoff
    }
    return max_eval;
}

Move EngineV1::search(Board& b, double time_limit, int max_depth) {
    start_time = steady_clock::now();
    time_limit_sec = time_limit;
    nodes_visited = 0;
    last_depth = 0;
    
    // Clear killers array for this search
    for (int p = 0; p < MAX_PLY; ++p) {
        killer_moves[p][0] = Move();
        killer_moves[p][1] = Move();
    }
    
    // Decay history roughly by half over time
    for (int c = 0; c < 2; ++c) {
        for (int f = 0; f < 256; ++f) {
            for (int t = 0; t < 256; ++t) {
                history_table[c][f][t] /= 2;
            }
        }
    }
    
    Move best_move;
    
    for (int depth = 1; depth <= max_depth; ++depth) {
        last_depth = depth;
        try {
            Color side = b.side_to_move();
            MoveList moves;
            b.generate_moves(moves);
            

            
            if (moves.count == 0) return Move();
            
            // Move scoring
            int move_scores[256];
            Move tt_move;
            TTEntryV1 tt_entry;
            if (tt.probe(b.get_hash(), tt_entry)) {
                tt_move = tt_entry.best_move;
            }
            score_moves(moves, move_scores, b, 0, tt_move); // Root is ply 0

            Move current_best_move;
            int best_score;
            int alpha = -2000000;
            int beta = 2000000;
            
            best_score = -2000000;
            for (int i = 0; i < moves.count; ++i) {
                // Lazy sorting inline
                int best_idx = i;
                for (int j = i + 1; j < moves.count; ++j) {
                    if (move_scores[j] > move_scores[best_idx]) best_idx = j;
                }
                std::swap(move_scores[i], move_scores[best_idx]);
                std::swap(moves.moves[i], moves.moves[best_idx]);
                
                Move move = moves.moves[i];
                int score;
                {
                    ScopedMoveV1 sm(b, move);
                    if (i == 0) {
                        score = -alphabeta(b, depth - 1, -beta, -alpha, 1);
                    } else {
                        bool is_capture = (move.captured != NO_PIECE);
                        bool is_killer = (move == killer_moves[0][0] || move == killer_moves[0][1]);
                        bool do_lmr = (i >= 4 && depth >= 3 && !is_capture && !is_killer);

                        if (do_lmr) {
                            score = -alphabeta(b, depth - 2, -alpha - 1, -alpha, 1);
                            if (score > alpha) {
                                score = -alphabeta(b, depth - 1, -alpha - 1, -alpha, 1);
                            }
                        } else {
                            score = -alphabeta(b, depth - 1, -alpha - 1, -alpha, 1);
                        }
                        
                        if (score > alpha && score < beta) {
                            score = -alphabeta(b, depth - 1, -beta, -alpha, 1);
                        }
                    }
                }
                
                if (score > best_score) {
                    best_score = score;
                    current_best_move = move;
                }
                alpha = std::max(alpha, best_score); // Update alpha at root
                
                if (is_time_up()) throw std::runtime_error("Time limit");
            }
            
            best_move = current_best_move;
            
            if (is_time_up()) break;
            
        } catch (const std::exception& e) {
            break;
        }
    }
    
    return best_move;
}


#include <iostream>
#include <fstream>
#include <string>

using namespace std;

string move_to_string_hw(const Move& m) {
    if (m.from == 0 && m.to == 0) return "NullMove";
    const char* cols = "abcdefghjkmn";
    int r1 = m.from >> 4;
    int c1 = m.from & 15;
    int r2 = m.to >> 4;
    int c2 = m.to & 15;

    string s = "";
    s += cols[c1];
    s += std::to_string(r1 + 1);
    s += " ";
    s += cols[c2];
    s += std::to_string(r2 + 1);
    return s;
}

int main() {
    ifstream in("input.txt");
    if (!in) {
        return 0;
    }

    string color_str;
    in >> color_str;

    Color my_color = (color_str == "WHITE" || color_str == "White" || color_str == "white") ? WHITE : BLACK;

    double my_time = 0.0;
    double opp_time = 0.0;
    in >> my_time >> opp_time;

    Board b;
    for(int r = 0; r < 12; ++r) {
        for(int c = 0; c < 12; ++c) {
            b.clear_piece(r * 16 + c);
        }
    }
    
    string line;
    getline(in, line);
    for (int r = 11; r >= 0; --r) {
        getline(in, line);
        while(!line.empty() && (line.back() == '\r' || line.back() == '\n')) {
            line.pop_back();
        }
        for (int c = 0; c < 12 && c < line.size(); ++c) {
            char ch = line[c];
            if (ch == '.') continue;
            
            PieceType pt = NO_PIECE;
            Color col = WHITE;
            if (ch >= 'a' && ch <= 'z') { col = BLACK; }
            
            char lower = tolower(ch);
            if (lower == 'b') pt = BABY;
            else if (lower == 'p') pt = PRINCE;
            else if (lower == 'x') pt = PRINCESS;
            else if (lower == 'y') pt = PONY;
            else if (lower == 'g') pt = GUARD;
            else if (lower == 't') pt = TUTOR;
            else if (lower == 's') pt = SCOUT;
            else if (lower == 'n') pt = SIBLING;
            
            if (pt != NO_PIECE) {
                b.set_piece(r * 16 + c, pt, col);
            }
        }
    }

    if (b.side_to_move() != my_color) {
        b.make_null_move();
    }

    EngineV1 engine;
    engine.set_times(my_time, opp_time);
    
    double think_time = 0.95;
    // if 50 seconds remaining, think for 0.1 second
    if (my_time < 50) {
        think_time = 0.1;
    }

    Move m = engine.search(b, think_time);

    if (m.from == 0 && m.to == 0) {
        MoveList ml;
        b.generate_moves(ml);
        if (ml.count > 0) m = ml.moves[0];
    }

    ofstream out("output.txt");
    out << move_to_string_hw(m) << "\n";
    out.close();

    return 0;
}
