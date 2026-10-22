"""Gymnasium wrapper around the non-interactive C++ Pinochle simulator."""

from __future__ import annotations

import sys
import warnings
from pathlib import Path
from collections.abc import Callable, Sequence
from typing import Any, Protocol

import gymnasium as gym
import numpy as np

moduleDirectory = Path(__file__).resolve().parents[2] / 'build' / 'python'
if moduleDirectory.exists():
    sys.path.insert(0, str(moduleDirectory))

try:
    import pinochle_cpp
except ImportError as error:
    raise ImportError(
        'pinochle_cpp is unavailable. Install ml/requirements.txt, then '
        'run: cmake -S . -B build && cmake --build build -j2',
    ) from error


def count_meld(cards: Sequence[Any], trump: int) -> int:
    """Compute Pinochle meld points for a hand given a trump suit.

    Accepts card objects (with rank/suit attributes) or (rank, suit) tuples.
    Rank mapping: 9=9, Jack=11, Queen=12, King=13, 10=14, Ace=15.
    Suits: 0=Clubs, 1=Diamonds, 2=Hearts, 3=Spades.
    """
    counts: dict[tuple[int, int], int] = {}
    for c in cards:
        suit = getattr(c, 'suit', c[1] if isinstance(c, (tuple, list)) else 0)
        rank = getattr(c, 'rank', c[0] if isinstance(c, (tuple, list)) else 0)
        counts[(suit, rank)] = counts.get((suit, rank), 0) + 1

    score = 0
    # Run in trump: A, 10, K, Q, J (15, 14, 13, 12, 11)
    if all(counts.get((trump, r), 0) >= 1 for r in (15, 14, 13, 12, 11)):
        score += 15

    # Marriage: King (13) + Queen (12) in same suit (4 in trump, 2 in others)
    for s in range(4):
        if counts.get((s, 13), 0) > 0 and counts.get((s, 12), 0) > 0:
            pts = 4 if s == trump else 2
            if counts.get((s, 13), 0) > 1 and counts.get((s, 12), 0) > 1:
                pts *= 2
            score += pts

    # Arounds: Aces (10), Kings (8), Queens (6), Jacks (4)
    for r, pts in ((15, 10), (13, 8), (12, 6), (11, 4)):
        if all(counts.get((s, r), 0) > 0 for s in range(4)):
            score += pts

    # Pinochle: Queen of Spades (suit 3, rank 12) + Jack of Diamonds (suit 1, rank 11)
    pinochles = min(counts.get((3, 12), 0), counts.get((1, 11), 0))
    if pinochles >= 1:
        score += 30 if pinochles == 2 else 4 * pinochles

    # 9 of trump
    score += counts.get((trump, 9), 0)
    return score


def decode_hand_from_observation(observation: np.ndarray) -> list[tuple[int, int]]:
    """Decode hand cards (rank, suit) from 288-dim observation prefix."""
    cards: list[tuple[int, int]] = []
    ranks = (9, 11, 12, 13, 14, 15)
    for slot in range(12):
        slotSlice = observation[slot * 24 : (slot + 1) * 24]
        if np.any(slotSlice > 0.5):
            idx = int(np.argmax(slotSlice))
            suit = idx // 6
            rank = ranks[idx % 6]
            cards.append((rank, suit))
    return cards


def decode_trick_from_observation(observation: np.ndarray) -> list[tuple[int, int]]:
    """Decode current trick cards (rank, suit) from observation slots 288..383."""
    trick: list[tuple[int, int]] = []
    ranks = (9, 11, 12, 13, 14, 15)
    trickOffset = 288
    for slot in range(4):
        slotSlice = observation[trickOffset + slot * 24 : trickOffset + (slot + 1) * 24]
        if np.any(slotSlice > 0.5):
            idx = int(np.argmax(slotSlice))
            suit = idx // 6
            rank = ranks[idx % 6]
            trick.append((rank, suit))
    return trick


def decode_trump_from_observation(observation: np.ndarray) -> int:
    """Decode trump suit (0..3) or -1 from observation slots 432..435."""
    trumpSlice = observation[432:436]
    if np.any(trumpSlice > 0.5):
        return int(np.argmax(trumpSlice))
    return -1


def decode_phase_from_observation(observation: np.ndarray) -> int:
    """Decode phase (0=bidding, 1=trump, 2=trick) from slots 436..438."""
    phaseSlice = observation[436:439]
    if np.any(phaseSlice > 0.5):
        return int(np.argmax(phaseSlice))
    return 0


def decode_bid_from_observation(observation: np.ndarray) -> int:
    """Decode current bid from normalized float at index 439."""
    return max(20, int(round(float(observation[439]) * 50.0)))


class SeatPolicy(Protocol):
    """Policy interface used for non-training seats."""

    def predict(
        self,
        observation: np.ndarray,
        action_mask: np.ndarray,
    ) -> int:
        """Choose a legal action for an observation."""


class RandomPolicy:
    """Baseline policy that samples uniformly from legal actions."""

    def __init__(self, randomGenerator: np.random.Generator | None = None) -> None:
        self.randomGenerator = randomGenerator or np.random.default_rng()

    def predict(
        self,
        observation: np.ndarray,
        action_mask: np.ndarray | None = None,
        action_masks: np.ndarray | None = None,
    ) -> int:
        del observation
        mask = action_masks if action_masks is not None else action_mask
        if mask is None:
            raise ValueError('action_mask must be provided.')
        legal = np.flatnonzero(mask)
        if len(legal) == 0:
            return 0
        return int(self.randomGenerator.choice(legal))


class HeuristicPolicy:
    """Rule-based Pinochle player covering bidding, trump selection, and tricks."""

    def __init__(self, randomGenerator: np.random.Generator | None = None) -> None:
        self.randomGenerator = randomGenerator or np.random.default_rng()

    def predict(
        self,
        observation: np.ndarray,
        action_mask: np.ndarray | None = None,
        action_masks: np.ndarray | None = None,
    ) -> int:
        mask = action_masks if action_masks is not None else action_mask
        if mask is None:
            raise ValueError('action_mask must be provided.')
        legalActions = np.flatnonzero(mask)
        if len(legalActions) == 0:
            return 0

        phase = decode_phase_from_observation(observation)
        hand = decode_hand_from_observation(observation)

        if phase == 0:
            # Phase 0: Bidding
            # Count meld in all 4 suits, determine max meld, max_bid = max_meld + 10
            melds = [count_meld(hand, suit) for suit in range(4)]
            maxMeld = max(melds) if melds else 0
            maxBid = maxMeld + 10
            currentBid = decode_bid_from_observation(observation)
            if currentBid < maxBid and currentBid < 50:
                bidVal = currentBid + 1
                bidAction = 13 + bidVal - 21
                if 0 <= bidAction < len(mask) and mask[bidAction]:
                    return bidAction
            # Pass
            if 12 < len(mask) and mask[12]:
                return 12
            return int(legalActions[0])

        if phase == 1:
            # Phase 1: Trump Selection
            # Pick suit with highest meld: action 43 + best_suit
            melds = [count_meld(hand, suit) for suit in range(4)]
            bestSuit = int(np.argmax(melds)) if melds else 0
            trumpAction = 43 + bestSuit
            if 0 <= trumpAction < len(mask) and mask[trumpAction]:
                return trumpAction
            return int(legalActions[0])

        # Phase 2: Trick Play
        trick = decode_trick_from_observation(observation)
        trump = decode_trump_from_observation(observation)
        legalSlots = [slot for slot in range(len(hand)) if slot < len(mask) and mask[slot]]
        if not legalSlots:
            return int(legalActions[0])

        if not trick:
            # Leading: pick highest card
            return max(legalSlots, key=lambda s: hand[s][0])

        # Trick in progress: find current winning card
        bestCard = trick[0]
        for c in trick[1:]:
            if c[1] == bestCard[1]:
                if c[0] > bestCard[0]:
                    bestCard = c
            elif c[1] == trump:
                bestCard = c

        def beats(candidate: tuple[int, int], target: tuple[int, int]) -> bool:
            if candidate[1] == target[1] and candidate[0] > target[0]:
                return True
            if candidate[1] == trump and target[1] != trump:
                return True
            return False

        winningSlots = [s for s in legalSlots if beats(hand[s], bestCard)]
        if winningSlots:
            # Pick lowest winning card
            return min(winningSlots, key=lambda s: hand[s][0])
        # Else pick lowest card
        return min(legalSlots, key=lambda s: hand[s][0])


class MaskableModelPolicy:
    """Adapts a MaskablePPO model to the environment's seat-policy protocol."""

    def __init__(self, model: Any, deterministic: bool = False) -> None:
        self.model = model
        self.deterministic = deterministic

    def predict(
        self,
        observation: np.ndarray,
        action_mask: np.ndarray | None = None,
        action_masks: np.ndarray | None = None,
    ) -> int:
        mask = action_masks if action_masks is not None else action_mask
        action, _ = self.model.predict(
            observation,
            deterministic=self.deterministic,
            action_masks=mask,
        )
        return int(np.asarray(action).item())


class PinochleEnv(gym.Env[np.ndarray, int]):
    """Train seat 2 while Python policies control the other three seats.

    Action space: Discrete(47)
      0..11:  Play card slot in current hand
      12:     Pass (bidding)
      13..42: Bid 21..50
      43..46: Choose trump (43 Clubs, 44 Diamonds, 45 Hearts, 46 Spades)

    Observation space: Box(0.0, 1.0, shape=(459,), dtype=np.float32)
    """

    metadata = {'render_modes': ['human']}

    def __init__(
        self,
        policies: Sequence[SeatPolicy | Callable[..., int] | None] | None = None,
        policy_pool: Sequence[SeatPolicy | Callable[..., int] | None] | None = None,
        render_mode: str | None = None,
    ) -> None:
        super().__init__()
        if policies is not None and len(policies) != 4:
            raise ValueError('policies must contain exactly one entry per seat.')
        self.render_mode = render_mode
        self.game = pinochle_cpp.PinochleGame()
        self._nativeState: tuple[
            np.ndarray, np.ndarray, int, int, int, int, int, int, int
        ] | None = None
        self._nativeStatePlayer: int | None = None
        self.policies = list(policies) if policies is not None else [None] * 4
        self.policyPool = (
            list(policy_pool)
            if policy_pool is not None
            else [HeuristicPolicy(), RandomPolicy()]
        )
        self.activePolicies = list(self.policies)
        self.action_space = gym.spaces.Discrete(47)
        self.observation_space = gym.spaces.Box(
            low=0.0,
            high=1.0,
            shape=(459,),
            dtype=np.float32,
        )

    def reset(
        self,
        *,
        seed: int | None = None,
        options: dict[str, Any] | None = None,
    ) -> tuple[np.ndarray, dict[str, Any]]:
        super().reset(seed=seed)
        self.game.reset(int(self.np_random.integers(0, 2**32 - 1)))
        self._clear_native_state()
        self._select_episode_policies()
        _, terminated = self._advance_to_training_turn()
        return self._observation(2), self._info(2, terminated)

    def step(self, action: int) -> tuple[np.ndarray, float, bool, bool, dict[str, Any]]:
        self._clear_native_state()
        reward, terminated = self.game.step(int(action))
        if not terminated:
            opponentReward, opponentTerminated = self._advance_to_training_turn()
            reward += opponentReward
            terminated = terminated or opponentTerminated
        observation = self._observation(2)
        info = self._info(2, terminated)
        if self.render_mode == 'human':
            self.render()
        return observation, float(reward), bool(terminated), False, info

    def set_policy_pool(
        self,
        policy_pool: Sequence[SeatPolicy | Callable[..., int] | None],
    ) -> None:
        """Set the policies sampled independently for seats 0, 1, and 3."""
        if not policy_pool:
            raise ValueError('policy_pool must contain at least one policy.')
        self.policyPool = list(policy_pool)

    def action_masks(self) -> np.ndarray:
        """Return legal actions for MaskablePPO or custom action masking."""
        state = self._native_state(self.game.current_player)
        if state is not None:
            return state[1]
        mask = np.zeros(self.action_space.n, dtype=bool)
        mask[self.game.legal_actions()] = True
        return mask

    def action_mask(self) -> np.ndarray:
        """Backward-compatible singular alias for callers outside sb3-contrib."""
        return self.action_masks()

    def _observation(self, player_id: int) -> np.ndarray:
        state = self._native_state(player_id)
        if state is not None:
            return state[0]
        obs = np.zeros(self.observation_space.shape, dtype=np.float32)

        # 1. Hand slots: 12 slots x 24 one-hot = 288 (0..287)
        hand = (
            self.game.player_hand(player_id)
            if hasattr(self.game, 'player_hand')
            else getattr(self.game, 'hand', [])
        )
        for slot, card in enumerate(hand[:12]):
            idx = self._card_index(card)
            if 0 <= idx < 24:
                obs[slot * 24 + idx] = 1.0

        # 2. Trick slots: 4 slots x 24 one-hot = 96 (288..383)
        trick = getattr(self.game, 'trick', [])
        for slot, card in enumerate(trick[:4]):
            idx = self._card_index(card)
            if 0 <= idx < 24:
                obs[288 + slot * 24 + idx] = 1.0

        # 3. Played cards history: 24 card types x 2 = 48 (384..431)
        played = getattr(self.game, 'played_cards', [])
        cardCounts = [0] * 24
        for card in played:
            idx = self._card_index(card)
            if 0 <= idx < 24:
                cardCounts[idx] += 1
        for c in range(24):
            if cardCounts[c] >= 1:
                obs[384 + c * 2] = 1.0
            if cardCounts[c] == 2:
                obs[384 + c * 2 + 1] = 1.0

        # 4. Trump suit: 4 one-hot (432..435)
        trump = getattr(self.game, 'trump', -1)
        if 0 <= trump < 4:
            obs[432 + trump] = 1.0

        # 5. Phase: 3 one-hot (436..438)
        phase = getattr(self.game, 'phase', 0)
        if 0 <= phase <= 2:
            obs[436 + phase] = 1.0

        # 6. Current bid: bid / 50.0 (439)
        bid = getattr(self.game, 'bid', 20)
        obs[439] = float(bid) / 50.0

        # 7. Bid winner: Us (440), Them (441), None (442)
        bidWinner = getattr(self.game, 'bid_winner', -1)
        if bidWinner in (0, 2):
            obs[440] = 1.0
        elif bidWinner in (1, 3):
            obs[441] = 1.0
        else:
            obs[442] = 1.0

        # 8. Relative seat to dealer: (player - dealer + 4) % 4 (443..446)
        dealer = getattr(self.game, 'dealer', 3)
        relSeat = (player_id - dealer + 4) % 4
        obs[443 + relSeat] = 1.0

        # 9. Trick position: len(trick) (447..450)
        trickPos = len(trick)
        if 0 <= trickPos < 4:
            obs[447 + trickPos] = 1.0

        # 10. Is my turn (451)
        currPlayer = getattr(self.game, 'current_player', 0)
        obs[451] = 1.0 if currPlayer == player_id else 0.0

        # 11. Us meld: us_meld / 50.0 (452)
        usMeld = getattr(self.game, 'us_meld', 0)
        obs[452] = float(usMeld) / 50.0

        # 12. Them meld: them_meld / 50.0 (453)
        themMeld = getattr(self.game, 'them_meld', 0)
        obs[453] = float(themMeld) / 50.0

        # 13. Us trick points: us_trick_points / 25.0 (454)
        usTrickPts = getattr(self.game, 'us_trick_points', getattr(self.game, 'us_points', 0))
        obs[454] = float(usTrickPts) / 25.0

        # 14. Them trick points: them_trick_points / 25.0 (455)
        themTrickPts = getattr(self.game, 'them_trick_points', getattr(self.game, 'them_points', 0))
        obs[455] = float(themTrickPts) / 25.0

        # 15. Did anyone bid yet (456)
        obs[456] = 1.0 if bidWinner != -1 else 0.0

        # 16. Active bidders (457..458)
        active = getattr(self.game, 'active_bidders', [True, True, True, True])
        usActive = (1 if active[0] else 0) + (1 if active[2] else 0)
        themActive = (1 if active[1] else 0) + (1 if active[3] else 0)
        obs[457] = float(usActive) / 2.0
        obs[458] = float(themActive) / 2.0

        return obs

    def _select_episode_policies(self) -> None:
        self.activePolicies = list(self.policies)
        if not self.policyPool:
            return
        for player_id in (0, 1, 3):
            if self.policies[player_id] is None:
                index = int(self.np_random.integers(len(self.policyPool)))
                self.activePolicies[player_id] = self.policyPool[index]

    def _advance_to_training_turn(self) -> tuple[float, bool]:
        reward = 0.0
        terminated = (self.game.phase == 3)
        while not terminated and self.game.current_player != 2:
            player_id = self.game.current_player
            action_mask = self.action_masks()
            policy = self.activePolicies[player_id]
            action = self._policy_action(policy, self._observation(player_id), action_mask)
            self._clear_native_state()
            stepReward, terminated = self.game.step(action)
            reward += float(stepReward)
        return reward, bool(terminated)

    def _policy_action(
        self,
        policy: SeatPolicy | Callable[..., int] | None,
        observation: np.ndarray,
        action_mask: np.ndarray,
    ) -> int:
        legalActions = np.flatnonzero(action_mask)
        if len(legalActions) == 0:
            return 0
        if policy is None:
            return int(self.np_random.choice(legalActions))
        if hasattr(policy, 'predict'):
            try:
                action = policy.predict(observation, action_masks=action_mask)
            except TypeError:
                action = policy.predict(observation, action_mask)
        else:
            action = policy(observation, action_mask)
        if isinstance(action, tuple):
            action = action[0]
        actionInt = int(action)
        if actionInt not in legalActions:
            warnings.warn(
                f'Policy returned illegal action {actionInt}. Legal actions: {legalActions.tolist()}',
                RuntimeWarning,
                stacklevel=2,
            )
            return int(self.np_random.choice(legalActions))
        return actionInt

    @staticmethod
    def _card_index(card: Any) -> int:
        rankToOffset = {9: 0, 11: 1, 12: 2, 13: 3, 14: 4, 15: 5}
        rankOffset = rankToOffset.get(card.rank, -1)
        if rankOffset == -1 or card.suit < 0 or card.suit > 3:
            return -1
        return card.suit * 6 + rankOffset

    def _info(self, player_id: int, terminated: bool = False) -> dict[str, Any]:
        state = self._native_state(player_id)
        if state is not None:
            _, actionMask, phase, currentPlayer, trump, usPoints, themPoints, bid, bidWinner = state
            return {
                'action_mask': actionMask,
                'player_id': player_id,
                'terminated': terminated,
                'phase': phase,
                'current_player': currentPlayer,
                'trump': trump,
                'us_points': usPoints,
                'them_points': themPoints,
                'bid': bid,
                'bid_winner': bidWinner,
            }
        return {
            'action_mask': self.action_masks(),
            'player_id': player_id,
            'terminated': terminated,
            'phase': getattr(self.game, 'phase', 0),
            'current_player': getattr(self.game, 'current_player', 0),
            'trump': getattr(self.game, 'trump', -1),
            'us_points': getattr(self.game, 'us_points', 0),
            'them_points': getattr(self.game, 'them_points', 0),
            'bid': getattr(self.game, 'bid', 20),
            'bid_winner': getattr(self.game, 'bid_winner', -1),
        }

    def _native_state(
        self,
        player_id: int,
    ) -> tuple[np.ndarray, np.ndarray, int, int, int, int, int, int, int] | None:
        """Fetch and cache one C++-built state per game turn."""
        if not hasattr(self.game, 'training_state'):
            return None
        if self._nativeState is None or self._nativeStatePlayer != player_id:
            state = self.game.training_state(player_id)
            self._nativeState = (
                np.asarray(state[0], dtype=np.float32),
                np.asarray(state[1], dtype=bool),
                int(state[2]),
                int(state[3]),
                int(state[4]),
                int(state[5]),
                int(state[6]),
                int(state[7]),
                int(state[8]),
            )
            self._nativeStatePlayer = player_id
        return self._nativeState

    def _clear_native_state(self) -> None:
        self._nativeState = None
        self._nativeStatePlayer = None

    def render(self) -> None:
        print(
            f'phase={self.game.phase} trump={self.game.trump} hand={len(self.game.hand)} '
            f'trick={len(self.game.trick)} score={self.game.us_points}-{self.game.them_points}',
        )
