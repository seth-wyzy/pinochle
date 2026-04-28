import random
from typing import List, Dict, Optional, Tuple

class Card:
    RANK_MAP = {9: "9", 11: "J", 12: "Q", 13: "K", 14: "10", 15: "A"}
    # New Mapping for Spades, Hearts, Clubs, Diamonds order with reverse sorting
    SUIT_MAP = {0: "Diamonds", 1: "Clubs", 2: "Hearts", 3: "Spades"}
    
    def __init__(self, rank: int, suit: int):
        self.rank = rank
        self.suit = suit

    def to_dict(self):
        return {
            "rank": self.rank,
            "suit": self.suit,
            "rank_name": self.RANK_MAP[self.rank],
            "suit_name": self.SUIT_MAP[self.suit]
        }

    def __repr__(self):
        return f"{self.RANK_MAP[self.rank]} of {self.SUIT_MAP[self.suit]}"

    def __eq__(self, other):
        return self.rank == other.rank and self.suit == other.suit

class MeldCounter:
    @staticmethod
    def count_meld(hand: List[Card], trump: int) -> Tuple[int, List[str], List[List[Card]]]:
        suit_rank = {s: {r: 0 for r in [9, 11, 12, 13, 14, 15]} for s in range(4)}
        for card in hand:
            suit_rank[card.suit][card.rank] += 1
        
        meld_points = 0
        details = []
        meld_card_groups = []

        # Run in trump: A 10 K Q J (15 14 13 12 11)
        run_ranks = [15, 14, 13, 12, 11]
        has_run = False
        if all(suit_rank[trump][r] > 0 for r in run_ranks):
            meld_points += 15
            details.append("Run in Trump (+15)")
            meld_card_groups.append([Card(r, trump) for r in run_ranks])
            has_run = True

        # Marriage
        for s in range(4):
            if suit_rank[s][13] > 0 and suit_rank[s][12] > 0:
                pts = 4 if s == trump else 2
                
                # If it's trump and we have a run, one marriage is already in the run
                effective_kings = suit_rank[s][13]
                effective_queens = suit_rank[s][12]
                
                if s == trump and has_run:
                    effective_kings -= 1
                    effective_queens -= 1
                
                if effective_kings > 0 and effective_queens > 0:
                    if effective_kings > 1 and effective_queens > 1:
                        meld_points += pts * 2
                        details.append(f"Double Marriage in {Card.SUIT_MAP[s]} (+{pts*2})")
                        meld_card_groups.append([Card(13, s), Card(13, s), Card(12, s), Card(12, s)])
                    else:
                        meld_points += pts
                        details.append(f"Marriage in {Card.SUIT_MAP[s]} (+{pts})")
                        meld_card_groups.append([Card(13, s), Card(12, s)])

        # Arounds
        arounds = {15: (10, "Aces"), 13: (8, "Kings"), 12: (6, "Queens"), 11: (4, "Jacks")}
        for rank, (pts, name) in arounds.items():
            count = min(suit_rank[s][rank] for s in range(4))
            if count >= 1:
                actual_pts = pts * (10 if count == 2 else 1) # Double around is 10x
                meld_points += actual_pts
                details.append(f"{'Double ' if count == 2 else ''}{name} Around (+{actual_pts})")
                for _ in range(count):
                    meld_card_groups.append([Card(rank, s) for s in range(4)])

        # Pinochle: Q Spades (12, 3) + J Diamonds (11, 0)
        q_spades = suit_rank[3][12]
        j_diamonds = suit_rank[0][11]
        pinochles = min(q_spades, j_diamonds)
        if pinochles >= 1:
            pts = 30 if pinochles == 2 else 4 * pinochles
            meld_points += pts
            details.append(f"{'Double ' if pinochles == 2 else ''}Pinochle (+{pts})")
            for _ in range(pinochles):
                meld_card_groups.append([Card(12, 3), Card(11, 0)])

        # 9 of trump
        nines = suit_rank[trump][9]
        if nines > 0:
            meld_points += nines
            details.append(f"{nines} Nine(s) of Trump (+{nines})")
            for _ in range(nines):
                meld_card_groups.append([Card(9, trump)])

        return meld_points, details, meld_card_groups

class AIPlayer:
    def __init__(self, position: int):
        self.position = position
        self.hand: List[Card] = []
        self.bidding_meld = 0
        self.personal_trump = 0

    def start_round(self, hand: List[Card]):
        self.hand = hand
        # Calculate best potential trump for bidding
        best_meld = -1
        for t in range(4):
            m, _, _ = MeldCounter.count_meld(self.hand, t)
            if m > best_meld:
                best_meld = m
                self.personal_trump = t
        self.bidding_meld = best_meld

    def choose_bid(self, current_high: int, game_mode: str = "standard") -> int:
        max_bid = self.bidding_meld + 10
        if game_mode == "5-card":
            max_bid = self.bidding_meld + 2 # Be conservative since less tricks
            start_bid = 5
        else:
            start_bid = 20

        if current_high < start_bid:
            if start_bid <= max_bid: return start_bid
            return 0 # Pass
        if current_high < max_bid:
            return current_high + 1
        return 0 # Pass

    def choose_move(self, trick: List[Dict], is_leader: bool, trump: int) -> Card:
        # trick is now list of {"player": int, "card": Card}
        trick_cards = [t["card"] for t in trick]
        
        if not trick_cards:
            # Lead highest card
            self.hand.sort(key=lambda c: c.rank, reverse=True)
            return self.hand[0]

        lead_card = trick_cards[0]
        # Determine best card currently in trick
        best_in_trick = trick_cards[0]
        for c in trick_cards[1:]:
            if c.suit == best_in_trick.suit:
                if c.rank > best_in_trick.rank: best_in_trick = c
            elif c.suit == trump:
                best_in_trick = c
        
        follow_suit = [c for c in self.hand if c.suit == lead_card.suit]
        trumps = [c for c in self.hand if c.suit == trump]

        if follow_suit:
            # Must follow suit, try to win
            winning = [c for c in follow_suit if (best_in_trick.suit == lead_card.suit and c.rank > best_in_trick.rank)]
            if winning:
                winning.sort(key=lambda c: c.rank)
                return winning[0]
            else:
                follow_suit.sort(key=lambda c: c.rank)
                return follow_suit[0]
        
        if trumps:
            # Must trump if no follow suit
            winning_trump = [c for c in trumps if (best_in_trick.suit != trump or c.rank > best_in_trick.rank)]
            if winning_trump:
                winning_trump.sort(key=lambda c: c.rank)
                return winning_trump[0]
            else:
                trumps.sort(key=lambda c: c.rank)
                return trumps[0]

        # Discard lowest
        self.hand.sort(key=lambda c: c.rank)
        return self.hand[0]

class Game:
    def __init__(self):
        self.deck = []
        self.hands = [[] for _ in range(4)]
        self.players = [AIPlayer(0), AIPlayer(1), AIPlayer(2), AIPlayer(3)] 
        self.seat_assignments = [None] * 4 # None = Empty, "AI" = AI, other string = Human
        self.game_mode = "standard" # "standard" or "5-card"
        self.us_total = 0
        self.them_total = 0
        self.player_totals = [0, 0, 0, 0] # Individual scoring for 5-card
        self.us_games = 0
        self.them_games = 0
        self.game_num = 1
        self.round_num = 0
        self.rounds_played_in_game = 0
        self.dealer = 0
        self.melds = [0] * 4
        self.meld_details = [[] for _ in range(4)]
        self.meld_cards = [[] for _ in range(4)]
        self.user_selected_meld_indices = []
        self.humans_melded = set()
        self.phase = "lobby"
        self.log = ["Waiting for players to join..."]
        self.current_trick = [] # Safety
        self.bid = 20 # Safety
        self.trump = -1 # Safety
        self.current_bidder = -1 # Safety

    def set_game_mode(self, mode: str):
        if self.phase == "lobby" and mode in ["standard", "5-card"]:
            self.game_mode = mode
            self.add_log(f"Game mode set to {mode}.")

    def add_ai(self, seat_index: int):
        if 0 <= seat_index < 4 and self.seat_assignments[seat_index] is None:
            self.seat_assignments[seat_index] = "AI"
            self.add_log(f"AI added to seat {['North', 'East', 'South', 'West'][seat_index]}.")
            return True
        return False

    def remove_ai(self, seat_index: int):
        if 0 <= seat_index < 4 and self.seat_assignments[seat_index] == "AI":
            self.seat_assignments[seat_index] = None
            self.add_log(f"AI removed from seat {['North', 'East', 'South', 'West'][seat_index]}.")
            return True
        return False

    def get_active_players(self):
        # In standard mode, always 4 players (empty seats treated as AI).
        # In 5-card mode, only seats that are not None are active.
        if self.game_mode == "standard":
            return [0, 1, 2, 3]
        return [i for i, s in enumerate(self.seat_assignments) if s is not None]

    def get_player_name(self, idx: int):
        if self.seat_assignments[idx] is not None:
            if self.seat_assignments[idx] == "AI":
                return f"AI ({['North', 'East', 'South', 'West'][idx]})"
            return self.seat_assignments[idx]
        if self.game_mode == "standard":
            return f"AI ({['North', 'East', 'South', 'West'][idx]})"
        return "Empty"

    def is_human(self, seat_index: int):
        return self.seat_assignments[seat_index] is not None and self.seat_assignments[seat_index] != "AI"

    def is_ai(self, seat_index: int):
        if self.game_mode == "standard":
            return not self.is_human(seat_index)
        return self.seat_assignments[seat_index] == "AI"

    def is_active(self, seat_index: int):
        return seat_index in self.get_active_players()

    def add_log(self, msg: str):
        self.log.append(msg)
        if len(self.log) > 15:
            self.log.pop(0)

    def join_seat(self, seat_index: int, name: str):
        if 0 <= seat_index < 4:
            if self.seat_assignments[seat_index] is None:
                self.seat_assignments[seat_index] = name
                self.add_log(f"{name} joined at seat {['North', 'East', 'South', 'West'][seat_index]}.")
                return True
            elif self.seat_assignments[seat_index] == name:
                # Rejoining own seat
                return True
        return False

    def vacate_seat(self, seat_index: int):
        if 0 <= seat_index < 4:
            name = self.seat_assignments[seat_index]
            if name and name != "AI":
                self.seat_assignments[seat_index] = None
                self.add_log(f"{name} left their seat.")
                if self.game_mode == "standard":
                    self.add_log("AI is taking over.")
                # If it's the vacated player's turn to bid, trigger AI (if standard mode)
                if self.phase == "bidding" and self.current_bidder == seat_index:
                    self.ai_bid_loop()
                return True
        return False

    def start_game(self):
        if self.phase == "lobby":
            active = self.get_active_players()
            if len(active) < 2:
                self.add_log("Need at least 2 players to start.")
                return
            self.reset_round()

    def reset_round(self):
        self.round_num += 1
        self.rounds_played_in_game += 1
        self.deck = [Card(r, s) for r in [9, 11, 12, 13, 14, 15] for s in range(4)] * 2
        random.shuffle(self.deck)
        
        active_players = self.get_active_players()
        cards_per_player = 5 if self.game_mode == "5-card" else 12
        
        for i in range(4):
            self.hands[i] = []
            
        for idx, p in enumerate(active_players):
            self.hands[p] = sorted(self.deck[idx*cards_per_player : (idx+1)*cards_per_player], key=lambda c: (c.suit, c.rank), reverse=True)
            self.players[p].start_round(self.hands[p])
        
        self.trump = -1
        self.bid = 4 if self.game_mode == "5-card" else 19
        self.bid_winner = -1
        self.current_trick = []
        
        # Ensure dealer is an active player
        if self.dealer not in active_players:
            self.dealer = active_players[0]
            
        self.trick_leader = active_players[(active_players.index(self.dealer) + 1) % len(active_players)]
        self.tricks_played = 0
        self.us_trick_points = 0
        self.them_trick_points = 0
        self.player_trick_points = [0] * 4
        self.meld_details = [[] for _ in range(4)]
        self.meld_cards = [[] for _ in range(4)]
        self.user_selected_meld_indices = []
        self.humans_melded = set()
        self.phase = "bidding" 
        
        self.bidding_active = [False] * 4
        for p in active_players:
            self.bidding_active[p] = True
            
        self.current_bidder = self.trick_leader
        self.log = []
        self.log.append(f"Game {self.game_num}, Round {self.rounds_played_in_game}. Dealer: {self.get_player_name(self.dealer)}")
        
        if self.is_ai(self.current_bidder):
            self.ai_bid_loop()

    def ai_bid_loop(self):
        while self.phase == "bidding" and self.is_ai(self.current_bidder):
            ai_bid = self.players[self.current_bidder].choose_bid(self.bid, self.game_mode)
            self.handle_bid(self.current_bidder, ai_bid)

    def handle_bid(self, player_idx: int, bid_amount: int):
        if self.phase != "bidding" or player_idx != self.current_bidder:
            return
        
        name = self.get_player_name(player_idx)

        if bid_amount == 0:
            self.bidding_active[player_idx] = False
            self.add_log(f"{name} passed.")
        elif bid_amount > self.bid:
            self.bid = bid_amount
            self.bid_winner = player_idx
            self.add_log(f"{name} bid {bid_amount}.")
        else:
            return
        
        active_count = sum(self.bidding_active)
        if active_count == 1:
            if self.bid_winner == -1: 
                self.bid_winner = self.dealer
                # Stuck bid
                stuck_bid = 4 if self.game_mode == "5-card" else 20
                self.bid = stuck_bid
                self.add_log(f"Everyone passed. {self.get_player_name(self.dealer)} stuck with {stuck_bid}.")
            
            winner_name = self.get_player_name(self.bid_winner)
            self.add_log(f"{winner_name} wins bid with {self.bid}.")

            if self.is_human(self.bid_winner):
                self.phase = "trump_selection"
            else:
                self.trump = self.players[self.bid_winner].personal_trump
                self.add_log(f"Trump chosen: {Card.SUIT_MAP[self.trump]}")
                # All AI melds calculated automatically, but humans will pick manually in next phase
                for i in self.get_active_players():
                    if self.is_ai(i):
                        pts, details, card_groups = MeldCounter.count_meld(self.hands[i], self.trump)
                        self.melds[i] = pts
                        self.meld_details[i] = details
                        self.meld_cards[i] = card_groups
                
                human_active = any(self.is_human(j) for j in self.get_active_players())
                if not human_active:
                    self.phase = "meld_display"
                    self.trick_leader = self.bid_winner
                else:
                    self.phase = "meld_selection"
            return

        active_players = self.get_active_players()
        current_idx = active_players.index(self.current_bidder)
        
        self.current_bidder = active_players[(current_idx + 1) % len(active_players)]
        while not self.bidding_active[self.current_bidder]:
            current_idx = active_players.index(self.current_bidder)
            self.current_bidder = active_players[(current_idx + 1) % len(active_players)]
        
        if self.is_ai(self.current_bidder):
            self.ai_bid_loop()

    def select_trump(self, trump: int):
        if self.phase == "trump_selection":
            self.trump = trump
            self.add_log(f"Trump chosen: {Card.SUIT_MAP[self.trump]}")
            
            for i in self.get_active_players():
                if self.is_ai(i):
                    pts, details, card_groups = MeldCounter.count_meld(self.hands[i], self.trump)
                    self.melds[i] = pts
                    self.meld_details[i] = details
                    self.meld_cards[i] = card_groups
            
            human_active = any(self.is_human(i) for i in self.get_active_players())
            if not human_active:
                self.phase = "meld_display"
                self.trick_leader = self.bid_winner
            else:
                self.phase = "meld_selection"

    def confirm_user_meld(self, seat_index: int, selected_indices: List[int]):
        if self.phase == "meld_selection" and self.is_human(seat_index):
            # Ensure index is within hand range
            valid_indices = [i for i in selected_indices if 0 <= i < len(self.hands[seat_index])]
            selected_cards = [self.hands[seat_index][i] for i in valid_indices]
            
            pts, details, card_groups = MeldCounter.count_meld(selected_cards, self.trump)
            self.melds[seat_index] = pts
            self.meld_details[seat_index] = details
            self.meld_cards[seat_index] = card_groups
            self.add_log(f"{self.get_player_name(seat_index)} melded {pts} points.")
            
            self.humans_melded.add(seat_index)
            
            active_human_seats = {i for i in self.get_active_players() if self.is_human(i)}
            
            # Use >= to handle cases where a human might have left during the phase
            if self.humans_melded >= active_human_seats:
                self.phase = "meld_display"
                self.add_log("All humans confirmed melds.")
                self.trick_leader = self.bid_winner if self.bid_winner != -1 else self.dealer
                self.humans_melded = set()

    def start_tricks(self):
        if self.phase == "meld_display":
            self.phase = "trick_taking"

    def play_card(self, player_idx: int, card_idx: int):
        active_players = self.get_active_players()
        current_idx = (active_players.index(self.trick_leader) + len(self.current_trick)) % len(active_players)
        curr_p = active_players[current_idx]

        if self.phase != "trick_taking" or player_idx != curr_p:
            return
        
        card = self.hands[player_idx][card_idx]
        is_valid, msg = self.validate_move(player_idx, card)
        if not is_valid:
            raise ValueError(msg)

        self.hands[player_idx].pop(card_idx)
        self.current_trick.append({"player": player_idx, "card": card})

    def validate_move(self, player_idx: int, card: Card) -> Tuple[bool, str]:
        if not self.current_trick: return True, ""
        
        lead_card = self.current_trick[0]["card"]
        hand = self.hands[player_idx]
        
        # Determine current winner and best card to beat
        best_card = self.current_trick[0]["card"]
        for t in self.current_trick[1:]:
            c = t["card"]
            if c.suit == best_card.suit:
                if c.rank > best_card.rank: best_card = c
            elif c.suit == self.trump:
                if best_card.suit != self.trump or c.rank > best_card.rank:
                    best_card = c

        # Must follow suit
        follow_suit = [c for c in hand if c.suit == lead_card.suit]
        if follow_suit:
            if card.suit != lead_card.suit:
                return False, f"Must follow suit ({Card.SUIT_MAP[lead_card.suit]})"
            
            # If lead was trump, must try to beat best trump
            if lead_card.suit == self.trump:
                can_beat = [c for c in follow_suit if c.rank > best_card.rank]
                if can_beat and card.rank <= best_card.rank:
                    return False, "Must beat the current high trump"
            
            return True, ""
            
        # If cannot follow suit, must trump
        trumps = [c for c in hand if c.suit == self.trump]
        if trumps:
            if card.suit != self.trump:
                return False, "Must play trump if you cannot follow suit"
            
            # Must beat current high trump if possible
            if best_card.suit == self.trump:
                can_beat = [c for c in trumps if c.rank > best_card.rank]
                if can_beat and card.rank <= best_card.rank:
                    return False, "Must beat the current high trump"
            
            return True, ""
            
        return True, ""

    def evaluate_trick(self):
        active_players = self.get_active_players()
        if len(self.current_trick) != len(active_players): return
        
        winner_idx = 0
        best_card = self.current_trick[0]["card"]
        
        for i in range(1, len(self.current_trick)):
            c = self.current_trick[i]["card"]
            if c.suit == best_card.suit:
                if c.rank > best_card.rank:
                    best_card = c
                    winner_idx = i
            elif c.suit == self.trump:
                if best_card.suit != self.trump or c.rank > best_card.rank:
                    best_card = c
                    winner_idx = i
        
        actual_winner = self.current_trick[winner_idx]["player"]
        # Points: A, 10, K are points
        pts = sum(1 for t in self.current_trick if t["card"].rank in [14, 15, 13])
        
        self.player_trick_points[actual_winner] += pts

        if actual_winner in [0, 2]: self.us_trick_points += pts
        else: self.them_trick_points += pts
        
        self.trick_leader = actual_winner
        self.tricks_played += 1
        self.add_log(f"{self.get_player_name(actual_winner)} won trick with {best_card} ({pts} pts).")
        self.current_trick = []
        
        target_tricks = 5 if self.game_mode == "5-card" else 12
        if self.tricks_played == target_tricks:
            # Last trick bonus
            if actual_winner in [0, 2]: self.us_trick_points += 1
            else: self.them_trick_points += 1
            self.player_trick_points[actual_winner] += 1
            self.finalize_round()

    def finalize_round(self):
        if self.game_mode == "5-card":
            for p in self.get_active_players():
                round_score = self.melds[p] + self.player_trick_points[p]
                if p == self.bid_winner:
                    if round_score >= self.bid:
                        self.player_totals[p] += round_score
                    else:
                        self.player_totals[p] -= self.bid
                else:
                    self.player_totals[p] += round_score
            
            # Check win condition
            winners = [p for p in self.get_active_players() if self.player_totals[p] >= 25]
            if winners:
                self.phase = "match_end"
                for w in winners:
                    self.add_log(f"{self.get_player_name(w)} reached 25 and won!")
            else:
                self.phase = "round_end"
        else:
            us_round = self.melds[0] + self.melds[2] + self.us_trick_points
            them_round = self.melds[1] + self.melds[3] + self.them_trick_points
            
            if self.bid_winner in [0, 2]:
                if us_round >= self.bid: self.us_total += us_round
                else: self.us_total -= self.bid
                self.them_total += them_round
            else:
                if them_round >= self.bid: self.them_total += them_round
                else: self.them_total -= self.bid
                self.us_total += us_round
            
            self.phase = "round_end"
            
            if self.rounds_played_in_game == 4:
                if self.us_total > self.them_total:
                    self.us_games += 1
                    self.add_log("US won the game!")
                elif self.them_total > self.us_total:
                    self.them_games += 1
                    self.add_log("THEM won the game!")
                else:
                    self.add_log("Game was a TIE!")
                
                if self.us_games == 2 or self.them_games == 2:
                    self.phase = "match_end"
                else:
                    self.phase = "game_end"

    def start_next_round(self):
        if self.phase == "round_end":
            active_players = self.get_active_players()
            idx = active_players.index(self.dealer) if self.dealer in active_players else -1
            self.dealer = active_players[(idx + 1) % len(active_players)]
            self.reset_round()
        elif self.phase == "game_end":
            self.game_num += 1
            self.rounds_played_in_game = 0
            # Next dealer Logic (depends on mode)
            if self.game_mode == "standard":
                self.dealer = (self.dealer + 2) % 4
            self.us_total = 0
            self.them_total = 0
            if self.game_mode == "5-card":
                self.player_totals = [0, 0, 0, 0]
            self.reset_round()

    def ai_play_one(self) -> bool:
        active_players = self.get_active_players()
        if self.phase == "trick_taking" and len(self.current_trick) < len(active_players):
            current_idx = (active_players.index(self.trick_leader) + len(self.current_trick)) % len(active_players)
            curr_p = active_players[current_idx]

            if self.is_ai(curr_p):
                card = self.players[curr_p].choose_move(self.current_trick, len(self.current_trick) == 0, self.trump)
                try:
                    card_idx = self.hands[curr_p].index(card)
                    self.hands[curr_p].pop(card_idx)
                    self.current_trick.append({"player": curr_p, "card": card})
                    return True
                except ValueError:
                    return False
        return False

    def get_state(self):
        # Ensure meld_cards is initialized correctly for all 4 players
        display_meld_cards = [[] for _ in range(4)]
        if hasattr(self, 'meld_cards'):
            for i in range(min(4, len(self.meld_cards))):
                display_meld_cards[i] = [[c.to_dict() for c in group] for group in self.meld_cards[i]]

        return {
            "game_mode": self.game_mode,
            "active_players": self.get_active_players(),
            "player_totals": self.player_totals,
            "game_num": self.game_num,
            "rounds_played": self.rounds_played_in_game,
            "us_games": self.us_games,
            "them_games": self.them_games,
            "round": self.round_num,
            "dealer": self.dealer,
            "us_total": self.us_total,
            "them_total": self.them_total,
            "phase": self.phase,
            "hands": [[c.to_dict() for c in h] for h in self.hands],
            "seat_assignments": self.seat_assignments,
            "player_names": [self.get_player_name(i) for i in range(4)],
            "trump": self.trump,
            "bid": self.bid,
            "bid_winner": getattr(self, 'bid_winner', -1),
            "current_bidder": self.current_bidder,
            "current_trick": [{"player": t["player"], "card": t["card"].to_dict()} for t in self.current_trick],
            "trick_leader": getattr(self, 'trick_leader', -1),
            "us_trick_points": getattr(self, 'us_trick_points', 0),
            "them_trick_points": getattr(self, 'them_trick_points', 0),
            "player_trick_points": getattr(self, 'player_trick_points', [0] * 4),
            "melds": self.melds,
            "meld_details": self.meld_details,
            "meld_cards": display_meld_cards,
            "tricks_played": getattr(self, 'tricks_played', 0),
            "log": self.log
        }
