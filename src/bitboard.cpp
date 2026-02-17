#include "board.h"
#include <iomanip>
#include <cstring> /* for memset */
#include <vector>

// ... existing Bitboard methods ...
// I will append the new methods to the end of the file or replace the file content if I am rewriting it.
// Since I have the full file content from previous view, I can replace.
// But to be safe and precise, I'll use replace_file_content to append/insert.
// Wait, I need to add many methods. It's better to read the file again to be sure of line numbers? 
// No, I just wrote it.
// I will rewrite the file to include all methods, including the new ones.

// ... Bitboard methods (same as before) ...
// ... Board methods (same as before) ...

// I'll use a clean rewrite since it's cleaner than multiple replaces.

void Bitboard::set(int sq) {
    int part = sq >> 6; // sq / 64
    int bit = sq & 63;  // sq % 64
    parts[part] |= (1ULL << bit);
}

void Bitboard::clear(int sq) {
    int part = sq >> 6;
    int bit = sq & 63;
    parts[part] &= ~(1ULL << bit);
}

bool Bitboard::test(int sq) const {
    int part = sq >> 6;
    int bit = sq & 63;
    return (parts[part] >> bit) & 1ULL;
}

void Bitboard::toggle(int sq) {
    int part = sq >> 6;
    int bit = sq & 63;
    parts[part] ^= (1ULL << bit);
}

int Bitboard::popcount() const {
    return __builtin_popcountll(parts[0]) + 
           __builtin_popcountll(parts[1]) + 
           __builtin_popcountll(parts[2]);
}

int Bitboard::lsb() const {
    if (parts[0]) return __builtin_ctzll(parts[0]);
    if (parts[1]) return 64 + __builtin_ctzll(parts[1]);
    if (parts[2]) return 128 + __builtin_ctzll(parts[2]);
    return -1; // Empty
}

Bitboard& Bitboard::operator&=(const Bitboard& other) {
    parts[0] &= other.parts[0];
    parts[1] &= other.parts[1];
    parts[2] &= other.parts[2];
    return *this;
}

Bitboard& Bitboard::operator|=(const Bitboard& other) {
    parts[0] |= other.parts[0];
    parts[1] |= other.parts[1];
    parts[2] |= other.parts[2];
    return *this;
}

Bitboard& Bitboard::operator^=(const Bitboard& other) {
    parts[0] ^= other.parts[0];
    parts[1] ^= other.parts[1];
    parts[2] ^= other.parts[2];
    return *this;
}

Bitboard Bitboard::operator~() const {
    Bitboard res;
    res.parts[0] = ~parts[0];
    res.parts[1] = ~parts[1];
    res.parts[2] = ~parts[2] & 0xFFFF; 
    return res;
}

Bitboard Bitboard::operator<<(int shift) const {
    if (shift == 0) return *this;
    // Placeholder
    Bitboard res;
    return res; 
}

Bitboard Bitboard::operator>>(int shift) const {
   Bitboard res;
   return res;
}


// Board methods

Board::Board() {
    reset();
}

void Board::reset() {
    // Clear everything
    for(int i=0; i<COLOR_NB; ++i) {
        for(int j=0; j<PIECE_NB; ++j) {
            pieces[i][j] = Bitboard();
        }
        occupied[i] = Bitboard();
    }
    all_occupied = Bitboard();
    
    for(int i=0; i<144; ++i) {
        mailbox[i] = NO_PIECE;
        color_mailbox[i] = COLOR_NB;
    }
    
    turn = WHITE;
    
    PieceType placement[] = {
        GUARD, PONY, TUTOR, SCOUT, SIBLING, PRINCESS, PRINCE, SIBLING, SCOUT, TUTOR, PONY, GUARD
    };
    
    // White Row 1 (index 0-11)
    for(int i=0; i<12; ++i) {
        set_piece(i, placement[i], WHITE);
    }
    // White Row 2 (index 12-23): Babies
    for(int i=0; i<12; ++i) {
        set_piece(12 + i, BABY, WHITE);
    }
    
    // Black Row 12 (index 132-143)
    for(int i=0; i<12; ++i) {
        set_piece(132 + i, placement[i], BLACK);
    }
    // Black Row 11 (index 120-131): Babies
    for(int i=0; i<12; ++i) {
        set_piece(120 + i, BABY, BLACK);
    }
}

void Board::set_piece(int sq, PieceType p, Color c) {
    pieces[c][p].set(sq);
    occupied[c].set(sq);
    all_occupied.set(sq);
    mailbox[sq] = p;
    color_mailbox[sq] = c;
}

void Board::clear_piece(int sq) {
    PieceType p = mailbox[sq];
    Color c = color_mailbox[sq];
    if (p == NO_PIECE) return;
    
    pieces[c][p].clear(sq);
    occupied[c].clear(sq);
    all_occupied.clear(sq);
    mailbox[sq] = NO_PIECE;
    color_mailbox[sq] = COLOR_NB;
}

PieceType Board::piece_at(int sq) const {
    if (sq < 0 || sq >= 144) return NO_PIECE;
    return mailbox[sq];
}

Color Board::color_at(int sq) const {
    if (sq < 0 || sq >= 144) return COLOR_NB;
    return color_mailbox[sq];
}

void Board::print() const {
    char piece_chars[] = {'.', 'B', 'P', 'X', 'Y', 'G', 'T', 'S', 'N'}; 
    
    std::cout << "   a b c d e f g h j k m n\n";
    std::cout << "  -------------------------\n";
    
    for (int row = 11; row >= 0; --row) {
        std::cout << std::setw(2) << (row + 1) << "|";
        for (int col = 0; col < 12; ++col) {
            int sq = row * 12 + col;
            PieceType p = mailbox[sq];
            Color c = color_mailbox[sq];
            char ch = '.';
            if (p != NO_PIECE) {
                ch = piece_chars[p];
                if (c == BLACK) {
                    ch = tolower(ch);
                }
            }
            std::cout << " " << ch;
        }
        std::cout << " |" << std::setw(2) << (row + 1) << "\n";
    }
    std::cout << "  -------------------------\n";
    std::cout << "   a b c d e f g h j k m n\n";
}

// Move Generation

static const int DIRECTIONS[8][2] = {
    {1, 0}, {-1, 0}, {0, 1}, {0, -1}, // Orthogonal: N, S, E, W
    {1, 1}, {1, -1}, {-1, 1}, {-1, -1} // Diagonal: NE, NW, SE, SW
};

// Helper to check boundaries
inline bool is_valid(int r, int c) {
    return r >= 0 && r < 12 && c >= 0 && c < 12;
}

std::vector<Move> Board::generate_moves() const {
    std::vector<Move> moves;
    moves.reserve(100);
    
    // Iterate over all squares to find pieces of current turn
    // This is slow for sparse boards, but for 12x12 with ~24 pieces it's OK.
    // Iterating over bitboard set bits would be faster but more complex to implement 'lsb' correctly loop.
    // For "speed advantage" requested by user, I should probably iterate pieces?
    // But 'lsb' implementation for 3-part bitboard is already there. Let's iterate squares for simplicity first, 
    // or iterate bitboard if piece count is low. 24 pieces is low density (24/144 = 16%).
    // Iterating occupied bitboard is better.
    
    Bitboard my_pieces = occupied[turn];
    // Copy because we pop bits
    
    // We need a 'next_bit' iterator or just loop parts.
    // Let's just loop all squares for V1 to be safe and correct.
    
    for (int sq = 0; sq < 144; ++sq) {
        if (color_mailbox[sq] != turn) continue;
        
        PieceType p = mailbox[sq];
        int r = sq / 12;
        int c = sq % 12;
        
        switch (p) {
            case BABY: {
                int forward = (turn == WHITE) ? 1 : -1;
                // 1 step
                int nr = r + forward;
                int nc = c;
                if (is_valid(nr, nc)) {
                    int target = nr * 12 + nc;
                    // Move or Capture logic: "Captures straight forward"
                    // If empty or enemy -> Valid.
                    // Wait, usually pawns move to empty and capture diagonally.
                    // "Captures straight forward." "Cannot jump."
                    // Checks project description: "Captures straight forward."
                    // "Cannot jump over pieces."
                    // So if occupied by enemy, it's a capture.
                    // If occupied by friend, blocked.
                    
                    Color target_color = color_mailbox[target];
                    if (target_color != turn) {
                        add_move(moves, sq, target);
                        
                        // 2 steps
                        // Only if 1st step was not blocked/captured?
                        // "Cannot jump over pieces" implies if 1st square occupied, cannot go to 2nd?
                        // "Moves 1 or 2 squares straight forward... May choose either distance... Cannot jump"
                        // Yes, path must be clear.
                        // If 1st square was empty, we can check 2nd.
                        // If 1st square was capture (enemy), can we go to 2nd? No, that would be jump over captured piece?
                        // Or does it capture on 2nd square?
                        // "Moves 1 or 2... Captures straight forward."
                        // If I move 2 squares and capture on 2nd, the 1st must be empty.
                        
                        if (target_color == COLOR_NB) { // 1st square was empty
                             int nr2 = r + forward * 2;
                             if (is_valid(nr2, nc)) {
                                 int target2 = nr2 * 12 + nc;
                                 if (color_mailbox[target2] != turn) {
                                     add_move(moves, sq, target2);
                                 }
                             }
                        }
                    }
                }
                break;
            }
            case PRINCE: { // 1 step any dir
                for (auto& dir : DIRECTIONS) {
                    int nr = r + dir[0];
                    int nc = c + dir[1];
                    if (is_valid(nr, nc)) {
                         int target = nr * 12 + nc;
                         if (color_mailbox[target] != turn) {
                             add_move(moves, sq, target);
                         }
                    }
                }
                break;
            }
            case PRINCESS: { // 1-3 steps any dir, slide
                for (auto& dir : DIRECTIONS) {
                    for (int k = 1; k <= 3; ++k) {
                        int nr = r + dir[0] * k;
                        int nc = c + dir[1] * k;
                        if (!is_valid(nr, nc)) break;
                        int target = nr * 12 + nc;
                        Color target_color = color_mailbox[target];
                        if (target_color == turn) break; // Blocked by friend
                        add_move(moves, sq, target);
                        if (target_color != COLOR_NB) break; // Capture, stop
                    }
                }
                break;
            }
            case PONY: { // 1 step diagonal
                for (int i = 4; i < 8; ++i) { // Diagonals in DIRECTIONS
                    int nr = r + DIRECTIONS[i][0];
                    int nc = c + DIRECTIONS[i][1];
                    if (is_valid(nr, nc)) {
                        int target = nr * 12 + nc;
                        if (color_mailbox[target] != turn) {
                            add_move(moves, sq, target);
                        }
                    }
                }
                break;
            }
            case GUARD: { // 1-2 orthogonal
                for (int i = 0; i < 4; ++i) { // Orthogonals
                    for (int k = 1; k <= 2; ++k) {
                        int nr = r + DIRECTIONS[i][0] * k;
                        int nc = c + DIRECTIONS[i][1] * k;
                        if (!is_valid(nr, nc)) break;
                        int target = nr * 12 + nc;
                        Color target_color = color_mailbox[target];
                        if (target_color == turn) break;
                        add_move(moves, sq, target);
                        if (target_color != COLOR_NB) break;
                    }
                }
                break;
            }
            case TUTOR: { // 1-2 diagonal
                for (int i = 4; i < 8; ++i) { // Diagonals
                    for (int k = 1; k <= 2; ++k) {
                        int nr = r + DIRECTIONS[i][0] * k;
                        int nc = c + DIRECTIONS[i][1] * k;
                        if (!is_valid(nr, nc)) break;
                        int target = nr * 12 + nc;
                        Color target_color = color_mailbox[target];
                        if (target_color == turn) break;
                        add_move(moves, sq, target);
                        if (target_color != COLOR_NB) break;
                    }
                }
                break;
            }
            case SCOUT: {
                // 1-3 forward, plus optional 1 sideways shift.
                // "Jumps over pieces allowed."
                // "At most one capture may occur, on the landing point".
                // So intermediate squares don't matter, except for boundaries.
                // Can shift sideways by 1 "while advancing".
                // Does this mean total displacement?
                // "f5, f6, f7 (straight), g5, g6, g7 (right shift), h5?? (h is right of g)"
                // Example: White Scout at g2 (r1, c6).
                // Target set: f5, f6, f7, g5, g6, g7, h5, h6, h7.
                // Wait. g2 is row 1 (0-indexed 1).
                // Forward 3 to row 4: g5 is correct?
                // g2 -> g5 is +3 rows.
                // g2 -> f5 is +3 rows, -1 col.
                // g2 -> h5 is +3 rows, +1 col.
                // So for each forward step k \in {1,2,3}, we can have col shift \in {-1, 0, 1}.
                // Total 3 * 3 = 9 possible targets.
                // Jumps allowed -> ignore occupied intermediate.
                // Only check landing.
                
                int forward = (turn == WHITE) ? 1 : -1;
                for (int k = 1; k <= 3; ++k) {
                    int nr = r + forward * k;
                    if (nr < 0 || nr >= 12) continue;
                    
                    // 3 columns: c-1, c, c+1
                    for (int dc = -1; dc <= 1; ++dc) {
                        int nc = c + dc;
                        if (nc >= 0 && nc < 12) {
                            int target = nr * 12 + nc;
                            if (color_mailbox[target] != turn) {
                                add_move(moves, sq, target);
                            }
                        }
                    }
                }
                break;
            }
            case SIBLING: { // 1 step any dir, adj friend
                 for (auto& dir : DIRECTIONS) {
                    int nr = r + dir[0];
                    int nc = c + dir[1];
                    if (is_valid(nr, nc)) {
                         int target = nr * 12 + nc;
                         if (color_mailbox[target] != turn) {
                             // Check adjacency to friend (excluding self at sq)
                             // We need to check neighbors of 'target'
                             bool has_friend = false;
                             for (auto& adj : DIRECTIONS) {
                                 int ar = nr + adj[0];
                                 int ac = nc + adj[1];
                                 if (is_valid(ar, ac)) {
                                     int adj_sq = ar * 12 + ac;
                                     if (adj_sq == sq) continue; // Ignore self current pos
                                     if (color_mailbox[adj_sq] == turn) {
                                         has_friend = true;
                                         break;
                                     }
                                 }
                             }
                             if (has_friend) {
                                 add_move(moves, sq, target);
                             }
                         }
                    }
                }
                break;
            }
            default: break;
        }
    }
    
    return moves;
}

void Board::add_move(std::vector<Move>& moves, int from, int to) const {
    moves.emplace_back(from, to, mailbox[to]);
}

void Board::make_move(const Move& m) {
    // Capture
    if (m.captured != NO_PIECE) {
        // Clear captured piece from opponent
        Color opp = (Color)(1 - turn);
        pieces[opp][m.captured].clear(m.to);
        occupied[opp].clear(m.to);
         // all_occupied update later
    }
    
    PieceType p = mailbox[m.from];
    
    // Remove from 'from'
    pieces[turn][p].clear(m.from);
    occupied[turn].clear(m.from);
    
    // Add to 'to'
    pieces[turn][p].set(m.to);
    occupied[turn].set(m.to);
    
    // Update mailbox
    mailbox[m.from] = NO_PIECE;
    color_mailbox[m.from] = COLOR_NB;
    
    mailbox[m.to] = p;
    color_mailbox[m.to] = turn;
    
    // Update all_occupied
    all_occupied.clear(m.from);
    all_occupied.set(m.to);
    
    turn = (Color)(1 - turn);
}

void Board::unmake_move(const Move& m) {
    turn = (Color)(1 - turn); // Swap back
    
    PieceType p = mailbox[m.to];
    
    // Remove from 'to'
    pieces[turn][p].clear(m.to);
    occupied[turn].clear(m.to);
    
    // Add back to 'from'
    pieces[turn][p].set(m.from);
    occupied[turn].set(m.from); // Oops, fixed bug here
    
    mailbox[m.to] = NO_PIECE; // Temporarily clear
    color_mailbox[m.to] = COLOR_NB;
    
    mailbox[m.from] = p;
    color_mailbox[m.from] = turn;
    
    // Restore captured
    if (m.captured != NO_PIECE) {
        Color opp = (Color)(1 - turn);
        pieces[opp][m.captured].set(m.to);
        occupied[opp].set(m.to);
        
        mailbox[m.to] = m.captured;
        color_mailbox[m.to] = opp;
    }
    
    // Rebuild all_occupied just to be safe or update incrementally
    all_occupied = occupied[WHITE] | occupied[BLACK];
}

bool Board::is_game_over() const {
    // Game over if Prince captured.
    // Check if Prince exists for both sides.
    // White Prince
    if (pieces[WHITE][PRINCE].popcount() == 0) return true;
    if (pieces[BLACK][PRINCE].popcount() == 0) return true;
    return false;
}
