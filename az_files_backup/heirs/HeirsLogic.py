import numpy as np

class Board():
    # 1: Baby, 2: Prince, 3: Princess, 4: Pony, 5: Guard, 6: Tutor, 7: Scout, 8: Sibling
    def __init__(self, n=12):
        self.n = n
        self.pieces = np.zeros((n, n), dtype=int)
        
    def __getitem__(self, index): 
        return self.pieces[index]

    def get_legal_moves(self, player):
        moves = []
        for r in range(self.n):
            for c in range(self.n):
                if self.pieces[r, c] * player > 0:
                    piece_type = abs(self.pieces[r, c])
                    moves.extend(self.get_piece_moves(r, c, piece_type, player))
        return moves

    def add_move(self, moves, r, c, nr, nc):
        if 0 <= nr < self.n and 0 <= nc < self.n:
            dr = nr - r
            dc = nc - c
            # Only up to 3 distance
            if abs(dr) <= 3 and abs(dc) <= 3:
                action = (r * self.n + c) * 49 + (dr + 3) * 7 + (dc + 3)
                moves.append(action)

    def is_empty_or_enemy(self, r, c, player):
        return self.pieces[r, c] * player <= 0

    def is_enemy(self, r, c, player):
        return self.pieces[r, c] * player < 0

    def is_empty(self, r, c):
        return self.pieces[r, c] == 0

    def get_piece_moves(self, r, c, piece_type, player):
        moves = []
        forward = -1 if player == 1 else 1

        if piece_type == 1: # Baby
            nr = r + forward
            if 0 <= nr < self.n:
                if self.is_empty_or_enemy(nr, c, player):
                    self.add_move(moves, r, c, nr, c)
                    nr2 = r + 2 * forward
                    if self.is_empty(nr, c) and 0 <= nr2 < self.n:
                        if self.is_empty_or_enemy(nr2, c, player):
                            self.add_move(moves, r, c, nr2, c)

        elif piece_type == 2: # Prince
            for dr in [-1, 0, 1]:
                for dc in [-1, 0, 1]:
                    if dr == 0 and dc == 0: continue
                    nr, nc = r + dr, c + dc
                    if 0 <= nr < self.n and 0 <= nc < self.n:
                        if self.is_empty_or_enemy(nr, nc, player):
                            self.add_move(moves, r, c, nr, nc)

        elif piece_type == 3: # Princess
            dirs = [(1,0),(-1,0),(0,1),(0,-1),(1,1),(1,-1),(-1,1),(-1,-1)]
            for dr, dc in dirs:
                for dist in range(1, 4):
                    nr, nc = r + dr * dist, c + dc * dist
                    if 0 <= nr < self.n and 0 <= nc < self.n:
                        if self.is_empty_or_enemy(nr, nc, player):
                            self.add_move(moves, r, c, nr, nc)
                            if self.is_enemy(nr, nc, player):
                                break
                        else:
                            break
                    else:
                        break

        elif piece_type == 4: # Pony
            dirs = [(1,1),(1,-1),(-1,1),(-1,-1)]
            for dr, dc in dirs:
                nr, nc = r + dr, c + dc
                if 0 <= nr < self.n and 0 <= nc < self.n:
                    if self.is_empty_or_enemy(nr, nc, player):
                        self.add_move(moves, r, c, nr, nc)

        elif piece_type == 5: # Guard
            dirs = [(1,0),(-1,0),(0,1),(0,-1)]
            for dr, dc in dirs:
                for dist in range(1, 3):
                    nr, nc = r + dr * dist, c + dc * dist
                    if 0 <= nr < self.n and 0 <= nc < self.n:
                        if self.is_empty_or_enemy(nr, nc, player):
                            self.add_move(moves, r, c, nr, nc)
                            if self.is_enemy(nr, nc, player):
                                break
                        else:
                            break
                    else:
                        break

        elif piece_type == 6: # Tutor
            dirs = [(1,1),(1,-1),(-1,1),(-1,-1)]
            for dr, dc in dirs:
                for dist in range(1, 3):
                    nr, nc = r + dr * dist, c + dc * dist
                    if 0 <= nr < self.n and 0 <= nc < self.n:
                        if self.is_empty_or_enemy(nr, nc, player):
                            self.add_move(moves, r, c, nr, nc)
                            if self.is_enemy(nr, nc, player):
                                break
                        else:
                            break
                    else:
                        break

        elif piece_type == 7: # Scout
            for f_dist in range(1, 4):
                nr = r + f_dist * forward
                for s_shift in [-1, 0, 1]:
                    nc = c + s_shift
                    if 0 <= nr < self.n and 0 <= nc < self.n:
                        if self.is_empty_or_enemy(nr, nc, player):
                            self.add_move(moves, r, c, nr, nc)

        elif piece_type == 8: # Sibling
            for dr in [-1, 0, 1]:
                for dc in [-1, 0, 1]:
                    if dr == 0 and dc == 0: continue
                    nr, nc = r + dr, c + dc
                    if 0 <= nr < self.n and 0 <= nc < self.n:
                        if self.is_empty_or_enemy(nr, nc, player):
                            has_friendly_adj = False
                            for adr in [-1, 0, 1]:
                                for adc in [-1, 0, 1]:
                                    if adr == 0 and adc == 0: continue
                                    anr, anc = nr + adr, nc + adc
                                    if 0 <= anr < self.n and 0 <= anc < self.n:
                                        if anr == r and anc == c:
                                            continue
                                        if self.pieces[anr, anc] * player > 0:
                                            has_friendly_adj = True
                                            break
                                if has_friendly_adj: break
                            if has_friendly_adj:
                                self.add_move(moves, r, c, nr, nc)

        return moves

    def has_legal_moves(self, player):
        return len(self.get_legal_moves(player)) > 0
        
    def execute_move(self, action, player):
        c = (action // 49) % self.n
        r = (action // 49) % (self.n * self.n) // self.n
        # action = (r * 12 + c) * 49 + (dr + 3) * 7 + (dc + 3)
        # So action // 49 = r * 12 + c
        # (action // 49) % 12 = c
        # (action // 49) // 12 = r
        
        c = (action // 49) % self.n
        r = (action // 49) // self.n
        dc = (action % 49) % 7 - 3
        dr = (action % 49) // 7 - 3
        nr, nc = r + dr, c + dc
        
        self.pieces[nr, nc] = self.pieces[r, c]
        self.pieces[r, c] = 0
