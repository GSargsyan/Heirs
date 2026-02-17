#include "board.h"
#include <iomanip>
#include <cstring> /* for memset */
#include <vector>

void Board::set_piece(int sq, PieceType p, Color c) {
    if (sq < 0 || sq >= 144) return;
    
    // If overwriting a Prince, update count
    if (pieces[sq] == PRINCE) {
        prince_count[colors[sq]]--;
    }
    
    pieces[sq] = p;
    colors[sq] = c;
    
    if (p == PRINCE) {
        prince_count[c]++;
    }
}

void Board::clear_piece(int sq) {
    if (sq < 0 || sq >= 144) return;
    
    if (pieces[sq] == PRINCE) {
        prince_count[colors[sq]]--;
    }
    
    pieces[sq] = NO_PIECE;       
    colors[sq] = COLOR_NB;
}

PieceType Board::piece_at(int sq) const {
    if (sq < 0 || sq >= 144) return NO_PIECE;
    return pieces[sq];
}

Color Board::color_at(int sq) const {
    if (sq < 0 || sq >= 144) return COLOR_NB;
    return colors[sq];
}

Board::Board() {
    reset();
}

void Board::reset() {
    for(int i=0; i<144; ++i) {
        pieces[i] = NO_PIECE;
        colors[i] = COLOR_NB;
    }
    prince_count[WHITE] = 0;
    prince_count[BLACK] = 0;
    
    half_move_clock = 0;
    half_move_history.reserve(100);
    half_move_history.clear();
    
    turn = WHITE;
    
    PieceType placement[] = {
        GUARD, PONY, TUTOR, SCOUT, SIBLING, PRINCESS, PRINCE, SIBLING, SCOUT, TUTOR, PONY, GUARD
    };
    
    init_zobrist();
    zobrist_key = 0;
    
    // White Row 1 (index 0-11)
    for(int i=0; i<12; ++i) {
        set_piece(i, placement[i], WHITE);
        zobrist_key ^= piece_keys[i][WHITE][placement[i]];
    }
    // White Row 2 (index 12-23): Babies
    for(int i=0; i<12; ++i) {
        set_piece(12 + i, BABY, WHITE);
        zobrist_key ^= piece_keys[12+i][WHITE][BABY];
    }
    
    // Black Row 12 (index 132-143)
    for(int i=0; i<12; ++i) {
        set_piece(132 + i, placement[i], BLACK);
        zobrist_key ^= piece_keys[132+i][BLACK][placement[i]];
    }
    // Black Row 11 (index 120-131): Babies
    for(int i=0; i<12; ++i) {
        set_piece(120 + i, BABY, BLACK);
        zobrist_key ^= piece_keys[120+i][BLACK][BABY];
    }
    
    // Turn is WHITE (0), so no side_key XOR needed if White is start and side_key represents Black to move?
    // Or side_key represents "Side to move is Black"?
    // Let's say side_key is XORed when it's Black's turn.
    // Start is White, so key is clean.
    
    history.clear();
    half_move_history.clear();
}

void Board::print() const {
    char piece_chars[] = {'.', 'B', 'P', 'X', 'Y', 'G', 'T', 'S', 'N'}; 
    
    std::cout << "    a b c d e f g h j k m n\n";
    std::cout << "  -------------------------\n";
    
    for (int row = 11; row >= 0; --row) {
        std::cout << std::setw(2) << (row + 1) << "|";
        for (int col = 0; col < 12; ++col) {
            int sq = row * 12 + col;
            PieceType p = pieces[sq];
            Color c = colors[sq];
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

inline bool is_valid(int r, int c) {
    return r >= 0 && r < 12 && c >= 0 && c < 12;
}

void Board::add_move(std::vector<Move>& moves, int from, int to) const {
    moves.emplace_back(from, to, pieces[to]);
}

std::vector<Move> Board::generate_moves() const {
    std::vector<Move> moves;
    moves.reserve(100);
    
    for (int sq = 0; sq < 144; ++sq) {
        if (colors[sq] != turn) continue;
        
        PieceType p = pieces[sq];
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
                    // If occupied by enemy -> Valid capture.
                     // If occupied by friend -> Blocked.
                     // If empty -> Valid move.
                    
                    Color target_color = colors[target];
                    if (target_color != turn) {
                        add_move(moves, sq, target);
                        
                        // 2 steps
                         if (target_color == COLOR_NB) { // 1st square was empty
                             int nr2 = r + forward * 2;
                             if (is_valid(nr2, nc)) {
                                 int target2 = nr2 * 12 + nc;
                                 if (colors[target2] != turn) {
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
                         if (colors[target] != turn) {
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
                        Color target_color = colors[target];
                        if (target_color == turn) break; // Blocked by friend
                        add_move(moves, sq, target);
                        if (target_color != COLOR_NB) break; // Capture, stop
                    }
                }
                break;
            }
            case PONY: { // 1 step diagonal
                for (int i = 4; i < 8; ++i) { // Diagonals
                    int nr = r + DIRECTIONS[i][0];
                    int nc = c + DIRECTIONS[i][1];
                    if (is_valid(nr, nc)) {
                        int target = nr * 12 + nc;
                        if (colors[target] != turn) {
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
                        Color target_color = colors[target];
                        if (target_color == turn) break; 
                        add_move(moves, sq, target);
                        if (target_color != COLOR_NB) break;
                    }
                }
                break;
            }
            case TUTOR: { // 1-2 diagonal
                for (int i = 4; i < 8; ++i) {
                    for (int k = 1; k <= 2; ++k) {
                        int nr = r + DIRECTIONS[i][0] * k;
                        int nc = c + DIRECTIONS[i][1] * k;
                        if (!is_valid(nr, nc)) break;
                        int target = nr * 12 + nc;
                        Color target_color = colors[target];
                        if (target_color == turn) break;
                        add_move(moves, sq, target);
                        if (target_color != COLOR_NB) break;
                    }
                }
                break;
            }
            case SCOUT: {
                int forward = (turn == WHITE) ? 1 : -1;
                for (int k = 1; k <= 3; ++k) {
                    int nr = r + forward * k;
                    if (nr < 0 || nr >= 12) continue;
                    
                    for (int dc = -1; dc <= 1; ++dc) { // -1, 0, +1 col shift
                        int nc = c + dc;
                        if (nc >= 0 && nc < 12) {
                            int target = nr * 12 + nc;
                            if (colors[target] != turn) {
                                // Jump allowed, so just check landing
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
                         if (colors[target] != turn) {
                             bool has_friend = false;
                             for (auto& adj : DIRECTIONS) {
                                 int ar = nr + adj[0];
                                 int ac = nc + adj[1];
                                 if (is_valid(ar, ac)) {
                                     int adj_sq = ar * 12 + ac;
                                     // "Adjacent to at least one friendly piece"
                                     // Does the piece itself count? No, it moved.
                                     // "Must end adjacent to friendly piece".
                                     if (adj_sq == sq) continue; // Original position might be empty now? 
                                     // Wait, adjacency is checked AFTER move? Or BEFORE?
                                     // Usually "must end adjacent" implies the state AFTER the move.
                                     // If I move, I leave 'sq'.
                                     // So I check neighbors of 'target' for friendly pieces.
                                     // NOTE: 'sq' is now empty (or will be).
                                     // But iterating current board, 'sq' HAS a friend (me).
                                     // So we must exclude 'sq' from adjacency check of 'target' if 'target' is adjacent to 'sq'.
                                     // Yes.
                                     
                                     if (adj_sq == sq) continue; 
                                     if (colors[adj_sq] == turn) {
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

// Zobrist Statics
uint64_t Board::piece_keys[144][2][10];
uint64_t Board::side_key;
bool Board::zobrist_initialized = false;

// Simple LCG for random numbers
static uint64_t rand64() {
    static uint64_t seed = 123456789;
    seed ^= seed << 13;
    seed ^= seed >> 7;
    seed ^= seed << 17;
    return seed;
}

void Board::init_zobrist() {
    if (zobrist_initialized) return;
    for(int sq=0; sq<144; ++sq) {
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
    // Push current hash to history before changing
    history.push_back(zobrist_key);
    half_move_history.push_back(half_move_clock);
    
    PieceType p = pieces[m.from];
    Color c = colors[m.from];
    
    // Update half_move_clock
    // Reset if capture or Baby move
    if (pieces[m.to] != NO_PIECE || p == BABY) {
        half_move_clock = 0;
    } else {
        half_move_clock++;
    }
    
    // Update Hash: Remove piece from 'from'
    zobrist_key ^= piece_keys[m.from][c][p];
    
    // Capture
    if (pieces[m.to] != NO_PIECE) {
        // Remove captured piece from hash
        zobrist_key ^= piece_keys[m.to][colors[m.to]][pieces[m.to]];
        
        if (m.captured == PRINCE) { // Should match pieces[m.to]
            prince_count[(Color)(1-c)]--;
        }
    }
    
    // Update Hash: Add piece to 'to'
    zobrist_key ^= piece_keys[m.to][c][p];
    
    // Clear from
    pieces[m.from] = NO_PIECE;
    colors[m.from] = COLOR_NB;
    
    // Set to
    pieces[m.to] = p;
    colors[m.to] = c;
    
    // Update side to move hash
    zobrist_key ^= side_key;
    turn = (Color)(1 - turn);
}

void Board::unmake_move(const Move& m) {
    turn = (Color)(1 - turn);
    zobrist_key ^= side_key; // Restore side hash
    
    PieceType p = pieces[m.to];
    Color c = colors[m.to]; // == turn
    
    // Reverse Hash: Remove piece from 'to'
    zobrist_key ^= piece_keys[m.to][c][p];
    
    // Move back
    pieces[m.from] = p;
    colors[m.from] = c;
    
    // Restore Hash: Add piece to 'from'
    zobrist_key ^= piece_keys[m.from][c][p];
    
    // Restore captured
    pieces[m.to] = m.captured;
    if (m.captured != NO_PIECE) {
        colors[m.to] = (Color)(1 - c);
        // Restore Hash: Add captured piece back
        zobrist_key ^= piece_keys[m.to][colors[m.to]][m.captured];
        
        if (m.captured == PRINCE) {
            prince_count[(Color)(1-c)]++;
        }
    } else {
        colors[m.to] = COLOR_NB;
    }
    
    // Restore history
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
    // Check if current zobrist_key appears in history
    // 3-fold repetition: appears 2 times in history + 1 current = 3.
    // Usually engines check for 1 repetition (2 occurrences) to avoid drawish lines early?
    // But legal draw is 3-fold.
    // Let's return true if we see it at least once in history for now,
    // effectively avoiding ANY repetition to be safe, or check count.
    
    int count = 0;
    // Iterate backwards for efficiency?
    for (auto it = history.rbegin(); it != history.rend(); ++it) {
        if (*it == zobrist_key) {
            count++;
            if (count >= 2) return true; // 2 in history + current = 3
        }
    }
    return false;
}

bool Board::is_fifty_moves() const {
    return half_move_clock >= 100; // 50 full moves = 100 half moves
}

bool Board::is_draw() const {
    if (is_fifty_moves()) return true;
    if (is_repetition()) return true;
    return false;
}
