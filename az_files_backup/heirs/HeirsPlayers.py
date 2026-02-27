import numpy as np

class RandomPlayer():
    def __init__(self, game):
        self.game = game

    def play(self, board):
        a = np.random.randint(self.game.getActionSize())
        valids = self.game.getValidMoves(board, 1)
        while valids[a] != 1:
            a = np.random.randint(self.game.getActionSize())
        return a

class HumanHeirsPlayer():
    def __init__(self, game):
        self.game = game

    def play(self, board):
        # We output valid moves occasionally
        valid = self.game.getValidMoves(board, 1)
        for i in range(len(valid)):
            if valid[i]:
                c = (i // 49) % self.game.n
                r = (i // 49) // self.game.n
                dc = (i % 49) % 7 - 3
                dr = (i % 49) // 7 - 3
                # uncomment to print valid moves
                # print(f"{r},{c} -> {r+dr},{c+dc}")
        
        while True:
            # simple input format
            a = input("Enter move [r c nr nc]: ")
            try:
                r, c, nr, nc = [int(x) for x in a.split()]
                dr = nr - r
                dc = nc - c
                action = (r * self.game.n + c) * 49 + (dr + 3) * 7 + (dc + 3)
                if valid[action]:
                    break
                else:
                    print("Invalid move")
            except Exception as e:
                print("Invalid format! Enter four space-separated integers, e.g., '10 1 9 1'")
        return action
