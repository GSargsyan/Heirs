#include "../src/engine_v6.h"
#include "../src/engine_v4.h"
#include "../src/engine_v3.h"
#include "../src/engine_v2.h"
#include "../src/engine_v1.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <windows.h>

// Adjustable Settings
int GAMES_PER_PAIR = 10;
int MAX_MOVES = 30; // 30 full moves = 60 ply
double TIME_TO_THINK = 0.1; // Seconds per move

// Piece Values
const int VAL_BABY = 100;
const int VAL_PRINCE = 10000;
const int VAL_PRINCESS = 950;
const int VAL_PONY = 180;
const int VAL_GUARD = 350;
const int VAL_TUTOR = 325;
const int VAL_SCOUT = 600;
const int VAL_SIBLING = 280;

// Evaluates material score based on custom piece values
int get_material_score(const Board& b, Color side) {
    int score = 0;
    for (int i = 0; i < 144; ++i) {
        if (b.piece_at(i) != NO_PIECE && b.color_at(i) == side) {
            PieceType p = b.piece_at(i);
            switch (p) {
                case BABY:     score += VAL_BABY; break;
                case PRINCE:   score += VAL_PRINCE; break;
                case PRINCESS: score += VAL_PRINCESS; break;
                case PONY:     score += VAL_PONY; break;
                case GUARD:    score += VAL_GUARD; break;
                case TUTOR:    score += VAL_TUTOR; break;
                case SCOUT:    score += VAL_SCOUT; break;
                case SIBLING:  score += VAL_SIBLING; break;
                default: break;
            }
        }
    }
    return score;
}

struct MatchResult {
    int winner; // 0 for White, 1 for Black, 2 for Draw
    int white_score;
    int black_score;
    int reason; // 0: Prince capture, 1: Move Limit, 2: Draw/Stalemate
};

template <typename EngineWhite, typename EngineBlack>
MatchResult play_match(EngineWhite& w_eng, EngineBlack& b_eng) {
    Board b; // Constructor should call reset() internally, if not, it starts initialized.
    int moves = 0;
    int max_ply = MAX_MOVES * 2;
    
    while (!b.is_game_over() && moves < max_ply) {
        if (b.is_draw()) {
            break;
        }

        Color side = b.side_to_move();
        Move m;
        if (side == WHITE) {
            m = w_eng.search(b, TIME_TO_THINK);
        } else {
            m = b_eng.search(b, TIME_TO_THINK);
        }
        
        if (m.from == 0 && m.to == 0) {
            // Null move usually implies stalemate/checkmate/no valid moves
            break; 
        }
        
        b.make_move(m);
        moves++;
    }
    
    MatchResult res;
    res.white_score = get_material_score(b, WHITE);
    res.black_score = get_material_score(b, BLACK);
    
    if (b.is_game_over()) {
        res.reason = 0; // Prince captured
    } else if (moves >= max_ply) {
        res.reason = 1; // Move limit
    } else {
        res.reason = 2; // Draw (repetition/50 moves/no moves)
    }

    if (res.white_score > res.black_score) {
        res.winner = WHITE;
    } else if (res.black_score > res.white_score) {
        res.winner = BLACK;
    } else {
        res.winner = 2; // Draw
    }
    
    return res;
}

template <typename Engine1, typename Engine2>
void run_pair(Engine1& eng1, Engine2& eng2, const std::string& name1, const std::string& name2, std::ofstream& log) {
    int v6_wins = 0;
    int v6_losses = 0;
    int draws = 0;
    long long total_diff = 0;
    
    log << "========== " << name1 << " vs " << name2 << " ==========\n";
    std::cout << "========== " << name1 << " vs " << name2 << " ==========\n";
    
    for (int i = 0; i < GAMES_PER_PAIR; ++i) {
        bool v6_is_white = (i % 2 == 0);
        MatchResult res;
        
        std::cout << "  Starting Game " << (i+1) << "/" << GAMES_PER_PAIR << ": " 
                  << (v6_is_white ? name1 : name2) << " (W) vs " 
                  << (v6_is_white ? name2 : name1) << " (B)... " << std::flush;
                  
        if (v6_is_white) {
            res = play_match(eng1, eng2);
        } else {
            res = play_match(eng2, eng1);
        }
        
        int v6_score = v6_is_white ? res.white_score : res.black_score;
        int opp_score = v6_is_white ? res.black_score : res.white_score;
        int diff = v6_score - opp_score;
        
        std::string outcome;
        if (diff > 0) {
            outcome = "WON ";
            v6_wins++;
        } else if (diff < 0) {
            outcome = "LOST";
            v6_losses++;
        } else {
            outcome = "DRAW";
            draws++;
        }
        
        total_diff += diff;
        
        std::string reason_str;
        if (res.reason == 0) reason_str = "Prince Captured";
        else if (res.reason == 1) reason_str = "Move Limit Reached";
        else reason_str = "Draw/Stalemate";
        
        log << "Game " << (i+1) << ": V6 as " << (v6_is_white ? "White" : "Black") 
            << " | " << outcome << " | Diff: " << diff 
            << " | Score: " << v6_score << " to " << opp_score 
            << " | Reason: " << reason_str << "\n";
            
        std::cout << outcome << " (Diff: " << diff << ") [" << reason_str << "]\n";
    }
    
    log << "--- Summary " << name1 << " vs " << name2 << " ---\n";
    log << name1 << " Wins: " << v6_wins << ", Losses: " << v6_losses << ", Draws: " << draws 
        << " | Average Diff: " << (double)total_diff / GAMES_PER_PAIR << "\n\n";
        
    std::cout << "  -> Summary: " << v6_wins << " Wins, " << v6_losses << " Losses, " 
              << draws << " Draws (Avg Diff: " << (double)total_diff / GAMES_PER_PAIR << ")\n\n";
}

int main(int argc, char* argv[]) {
    // Set console code page to UTF-8
    SetConsoleOutputCP(CP_UTF8);
    
    std::string log_filename = "tournament_results.txt";
    std::ofstream log(log_filename, std::ios_base::app);
    if (!log.is_open()) {
        std::cerr << "Failed to open log file." << std::endl;
        return 1;
    }
    
    log << "\n\n======================================================\n";
    log << "                NEW TOURNAMENT RUN\n";
    log << "======================================================\n\n";
    
    log << "Tournament settings:\n";
    log << "Games per pair: " << GAMES_PER_PAIR << "\n";
    log << "Max moves per game: " << MAX_MOVES << " (" << MAX_MOVES * 2 << " ply)\n";
    log << "Time per move: " << TIME_TO_THINK << "s\n\n";
    
    std::cout << "Loading engines..." << std::endl;
    // Instantiate all engines. If constructors load weights or initialize TTs, it happens here once.
    EngineV6 eng6;
    EngineV4 eng4;
    EngineV3 eng3;
    EngineV2 eng2;
    EngineV1 eng1;
    
    std::cout << "Starting tournament! Total matches: " << (GAMES_PER_PAIR * 4) << "\n\n";
    
    run_pair(eng6, eng1, "EngineV6", "EngineV1", log);
    run_pair(eng6, eng2, "EngineV6", "EngineV2", log);
    run_pair(eng6, eng3, "EngineV6", "EngineV3", log);
    run_pair(eng6, eng4, "EngineV6", "EngineV4", log);
    
    log.close();
    std::cout << "Tournament complete! Results saved to " << log_filename << std::endl;
    
    return 0;
}
