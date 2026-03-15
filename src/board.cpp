#include "board.h"
#include <iomanip>
#include <cstring>
#include <vector>

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
    }
    pieces[sq] = p;
    colors[sq] = c;
    if (p == PRINCE) {
        prince_count[c]++;
    }
    if (p != NO_PIECE && p != OFFBOARD) {
        add_piece_to_list(sq, c);
    }
}

void Board::clear_piece(int sq) {
    if (sq < 0 || sq >= 256) return;
    if (pieces[sq] == PRINCE) {
        prince_count[colors[sq]]--;
    }
    if (pieces[sq] != NO_PIECE && pieces[sq] != OFFBOARD) {
        remove_piece_from_list(sq, colors[sq]);
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
    half_move_history.reserve(2000);
    half_move_history.clear();
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
    
    history.reserve(2000);
    history.clear();
    half_move_history.clear();
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
    history.push_back(zobrist_key);
    half_move_history.push_back(half_move_clock);
    
    PieceType p = pieces[m.from];
    Color c = colors[m.from];
    
    if ((pieces[m.to] != NO_PIECE && pieces[m.to] != OFFBOARD) || p == BABY) {
        half_move_clock = 0;
    } else {
        half_move_clock++;
    }
    
    zobrist_key ^= piece_keys[m.from][c][p];
    
    if (pieces[m.to] != NO_PIECE && pieces[m.to] != OFFBOARD) {
        zobrist_key ^= piece_keys[m.to][colors[m.to]][pieces[m.to]];
        if (m.captured == PRINCE) {
            prince_count[(Color)(1-c)]--;
        }
        remove_piece_from_list(m.to, colors[m.to]);
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
    zobrist_key ^= piece_keys[m.to][c][p];
    
    pieces[m.from] = p;
    colors[m.from] = c;
    zobrist_key ^= piece_keys[m.from][c][p];
    
    move_piece_in_list(m.to, m.from, c);
    
    pieces[m.to] = m.captured;
    if (m.captured != NO_PIECE && m.captured != OFFBOARD) {
        colors[m.to] = (Color)(1 - c);
        zobrist_key ^= piece_keys[m.to][colors[m.to]][m.captured];
        if (m.captured == PRINCE) {
            prince_count[(Color)(1-c)]++;
        }
        add_piece_to_list(m.to, colors[m.to]);
    } else {
        colors[m.to] = COLOR_NB;
    }
    
    if (!history.empty()) history.pop_back();
    if (!half_move_history.empty()) {
        half_move_clock = half_move_history.back();
        half_move_history.pop_back();
    }
}

bool Board::is_game_over() const {
    if (prince_count[WHITE] == 0) return true;
    if (prince_count[BLACK] == 0) return true;
    return false;
}

bool Board::is_repetition() const {
    int count = 0;
    for (auto it = history.rbegin(); it != history.rend(); ++it) {
        if (*it == zobrist_key) {
            count++;
            if (count >= 2) return true; 
        }
    }
    return false;
}

bool Board::is_fifty_moves() const {
    return half_move_clock >= 100;
}

bool Board::is_draw() const {
    if (is_repetition()) return true;
    return false;
}
