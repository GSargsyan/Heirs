from Game import Game
from .HeirsLogic import Board
import numpy as np

class HeirsGame(Game):
    def __init__(self, n=12):
        self.n = n

    def getInitBoard(self):
        b = Board(self.n)
        # 1: Baby, 2: Prince, 3: Princess, 4: Pony, 5: Guard, 6: Tutor, 7: Scout, 8: Sibling
        # White (1)
        white_back = [5, 4, 6, 7, 8, 3, 2, 8, 7, 6, 4, 5]
        white_babies = [1] * 12
        # Black (-1)
        black_back = [-x for x in white_back]
        black_babies = [-1] * 12
        
        b.pieces[11] = white_back
        b.pieces[10] = white_babies
        b.pieces[1] = black_babies
        b.pieces[0] = black_back
        
        # We add a 13th row to track the 50-move rule clock. 
        # Filling the entire row avoids MCTS flipping asymmetry.
        state = np.zeros((13, self.n), dtype=int)
        state[:12, :] = b.pieces
        return state

    def getBoardSize(self):
        # We return 13x12 so the Neural Network knows there's a metadata row
        return (13, self.n)

    def getActionSize(self):
        return self.n * self.n * 49

    def getNextState(self, board, player, action):
        canonical_pieces = np.copy(board[:12, :])
        if player == -1:
            canonical_pieces = np.flipud(canonical_pieces * -1)
            
        b = Board(self.n)
        b.pieces = canonical_pieces
        
        # Check if the move is a capture or a baby move for the 50-move logic
        r = (action // 49) // self.n
        c = (action // 49) % self.n
        piece = b.pieces[r, c]
        is_baby = (abs(piece) == 1)
        
        dr = (action % 49) // 7 - 3
        dc = (action % 49) % 7 - 3
        nr, nc = r + dr, c + dc
        is_capture = (b.pieces[nr, nc] != 0)

        b.execute_move(action, 1)

        next_pieces = b.pieces if player == 1 else np.flipud(b.pieces) * -1
        
        next_state = np.zeros((13, self.n), dtype=int)
        next_state[:12, :] = next_pieces
        
        # Update 50-move clock
        if is_baby or is_capture:
            clock = 0
        else:
            clock = board[12, 0] + 1
            
        next_state[12, :] = clock
        return (next_state, -player)

    def getValidMoves(self, board, player):
        valids = [0]*self.getActionSize()
        canonical_pieces = np.copy(board[:12, :])
        if player == -1:
            canonical_pieces = np.flipud(canonical_pieces * -1)
            
        b = Board(self.n)
        b.pieces = canonical_pieces
        legalMoves = b.get_legal_moves(1)
        for move in legalMoves:
            valids[move] = 1
        return np.array(valids)

    def getGameEnded(self, board, player):
        b = Board(self.n)
        b.pieces = np.copy(board[:12, :])
        
        # Check princes
        player_prince = np.any(b.pieces == 2 * player)
        opponent_prince = np.any(b.pieces == -2 * player)
        
        if not opponent_prince:
            return 1
        if not player_prince:
            return -1
            
        canonical_pieces = np.copy(board[:12, :])
        if player == -1:
            canonical_pieces = np.flipud(canonical_pieces * -1)
            
        b.pieces = canonical_pieces
        if not b.has_legal_moves(1):
            return 1e-4  # Draw by stalemate

        if board[12, 0] >= 100:
            return 1e-4  # Draw by 50-move rule (100 half-steps)

        return 0

    def getCanonicalForm(self, board, player):
        canonical = np.copy(board)
        if player == -1:
            # We ONLY flip and negate the piece positions, NOT the metadata row
            canonical[:12, :] = np.flipud(board[:12, :] * -1)
        return canonical

    def getSymmetries(self, board, pi):
        pi_board = np.reshape(pi, (self.n, self.n, 49))
        pi_board_flipped = np.fliplr(pi_board).copy()
        
        new_pi = np.zeros_like(pi_board_flipped)
        for dr_idx in range(7):
            for dc_idx in range(7):
                flipped_dc_idx = 6 - dc_idx
                new_pi[:, :, dr_idx * 7 + flipped_dc_idx] = pi_board_flipped[:, :, dr_idx * 7 + dc_idx]
                
        return [(board, pi), (np.fliplr(board), list(new_pi.ravel()))]

    def stringRepresentation(self, board):
        return board.tobytes()

    @staticmethod
    def display(board):
        n = 12  # strict 12x12 rendering layout, ignoring the metadata row
        pieces = {
            1: 'B', 2: 'P', 3: 'X', 4: 'Y', 5: 'G', 6: 'T', 7: 'S', 8: 'N',
            -1: 'b', -2: 'p', -3: 'x', -4: 'y', -5: 'g', -6: 't', -7: 's', -8: 'n',
            0: '.'
        }
        
        cols = "abcdefghjkmn"
        print("    ", end="")
        for c in range(n):
            print(f"{cols[c]:2} ", end="")
        print("")
        print("   " + "---"*n)
        for r in range(n):
            print(f"{12-r:2} |", end="")
            for c in range(n):
                val = board[r][c]
                print(f"{pieces.get(val, '.') :2} ", end="")
            print("|")
        print("   " + "---"*n)
