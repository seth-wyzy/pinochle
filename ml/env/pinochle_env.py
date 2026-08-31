"""Gymnasium wrapper around the non-interactive C++ Pinochle simulator."""

from __future__ import annotations

import sys
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


class SeatPolicy(Protocol):
    """Policy interface used for non-training seats."""

    def predict(self, observation: np.ndarray, action_mask: np.ndarray) -> int:
        """Choose a legal action for an observation."""


class RandomPolicy:
    """Baseline policy that samples uniformly from legal actions."""

    def __init__(self, randomGenerator: np.random.Generator | None = None) -> None:
        self.randomGenerator = randomGenerator or np.random.default_rng()

    def predict(self, observation: np.ndarray, action_mask: np.ndarray) -> int:
        del observation
        return int(self.randomGenerator.choice(np.flatnonzero(action_mask)))


class PinochleEnv(gym.Env[np.ndarray, int]):
    """Train seat 2 while Python policies control the other three seats.

    Actions 0-11 select a card slot. During bidding, 12 passes and 13-16 bid
    20 while selecting Clubs, Diamonds, Hearts, or Spades as trump.
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
        self._nativeState: tuple[np.ndarray, np.ndarray, int, int, int, int, int] | None = None
        self._nativeStatePlayer: int | None = None
        self.policies = list(policies) if policies is not None else [None] * 4
        self.policyPool = list(policy_pool) if policy_pool is not None else None
        self.activePolicies = list(self.policies)
        self.action_space = gym.spaces.Discrete(17)
        # 12 hand slots x (6 ranks x 4 suits), 4 trick slots, and game context.
        self.observation_space = gym.spaces.Box(
            low=0,
            high=1,
            shape=(12 * 24 + 4 * 24 + 4 + 3,),
            dtype=np.int8,
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
        observation = np.zeros(self.observation_space.shape, dtype=np.int8)
        for slot, card in enumerate(self.game.player_hand(player_id)):
            observation[slot * 24 + self._card_index(card)] = 1

        trickOffset = 12 * 24
        for slot, card in enumerate(self.game.trick):
            observation[trickOffset + slot * 24 + self._card_index(card)] = 1

        contextOffset = trickOffset + 4 * 24
        if self.game.trump >= 0:
            observation[contextOffset + self.game.trump] = 1
        observation[contextOffset + 4] = self.game.phase == 0
        observation[contextOffset + 5] = self.game.current_player == player_id
        ourTeam = player_id in (0, 2)
        observation[contextOffset + 6] = (
            self.game.us_points >= self.game.them_points if ourTeam
            else self.game.them_points >= self.game.us_points
        )
        return observation

    def _select_episode_policies(self) -> None:
        self.activePolicies = list(self.policies)
        if self.policyPool is None:
            return
        for player_id in (0, 1, 3):
            index = int(self.np_random.integers(len(self.policyPool)))
            self.activePolicies[player_id] = self.policyPool[index]

    def _advance_to_training_turn(self) -> tuple[float, bool]:
        reward = 0.0
        terminated = self.game.phase == 2
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
        if int(action) not in legalActions:
            return int(self.np_random.choice(legalActions))
        return int(action)

    @staticmethod
    def _card_index(card: pinochle_cpp.Card) -> int:
        rankIndex = {9: 0, 11: 1, 12: 2, 13: 3, 14: 4, 15: 5}[card.rank]
        return card.suit * 6 + rankIndex

    def _info(self, player_id: int, terminated: bool = False) -> dict[str, Any]:
        state = self._native_state(player_id)
        if state is not None:
            _, actionMask, _, _, trump, usPoints, themPoints = state
            return {
                'action_mask': actionMask,
                'player_id': player_id,
                'terminated': terminated,
                'trump': trump,
                'us_points': usPoints,
                'them_points': themPoints,
            }
        return {
            'action_mask': self.action_masks(),
            'player_id': player_id,
            'terminated': terminated,
            'trump': self.game.trump,
            'us_points': self.game.us_points,
            'them_points': self.game.them_points,
        }

    def _native_state(
        self,
        player_id: int,
    ) -> tuple[np.ndarray, np.ndarray, int, int, int, int, int] | None:
        """Fetch and cache one C++-built state per game turn."""
        if not hasattr(self.game, 'training_state'):
            return None
        if self._nativeState is None or self._nativeStatePlayer != player_id:
            state = self.game.training_state(player_id)
            self._nativeState = (
                np.asarray(state[0], dtype=np.int8),
                np.asarray(state[1], dtype=bool),
                int(state[2]),
                int(state[3]),
                int(state[4]),
                int(state[5]),
                int(state[6]),
            )
            self._nativeStatePlayer = player_id
        return self._nativeState

    def _clear_native_state(self) -> None:
        self._nativeState = None
        self._nativeStatePlayer = None

    def render(self) -> None:
        print(
            f'trump={self.game.trump} hand={len(self.game.hand)} '
            f'trick={len(self.game.trick)} score={self.game.us_points}-{self.game.them_points}',
        )
