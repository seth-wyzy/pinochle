"""Gymnasium wrapper around the non-interactive C++ Pinochle simulator."""

from __future__ import annotations

import sys
from pathlib import Path
from typing import Any

import gymnasium as gym
import numpy as np

moduleDirectory = Path(__file__).resolve().parents[2] / 'build' / 'python'
if moduleDirectory.exists():
    sys.path.insert(0, str(moduleDirectory))

try:
    import pinochle_cpp
except ImportError as error:
    raise ImportError(
        'Build pinochle_cpp with CMake before importing PinochleEnv.',
    ) from error


class PinochleEnv(gym.Env[np.ndarray, int]):
    """Train player 2 and use deterministic rule-based opponents.

    Actions 0-11 select a card slot. During bidding, 12 passes and 13-16 bid
    20 while selecting Clubs, Diamonds, Hearts, or Spades as trump.
    """

    metadata = {'render_modes': ['human']}

    def __init__(self, render_mode: str | None = None) -> None:
        super().__init__()
        self.render_mode = render_mode
        self.game = pinochle_cpp.PinochleGame()
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
        return self._observation(), self._info()

    def step(self, action: int) -> tuple[np.ndarray, float, bool, bool, dict[str, Any]]:
        reward, terminated = self.game.step(int(action))
        observation = self._observation()
        info = self._info()
        if self.render_mode == 'human':
            self.render()
        return observation, float(reward), bool(terminated), False, info

    def action_masks(self) -> np.ndarray:
        """Return legal actions for MaskablePPO or custom action masking."""
        mask = np.zeros(self.action_space.n, dtype=bool)
        mask[self.game.legal_actions()] = True
        return mask

    def action_mask(self) -> np.ndarray:
        """Backward-compatible singular alias for callers outside sb3-contrib."""
        return self.action_masks()

    def _observation(self) -> np.ndarray:
        observation = np.zeros(self.observation_space.shape, dtype=np.int8)
        for slot, card in enumerate(self.game.hand):
            observation[slot * 24 + self._card_index(card)] = 1

        trickOffset = 12 * 24
        for slot, card in enumerate(self.game.trick):
            observation[trickOffset + slot * 24 + self._card_index(card)] = 1

        contextOffset = trickOffset + 4 * 24
        if self.game.trump >= 0:
            observation[contextOffset + self.game.trump] = 1
        observation[contextOffset + 4] = self.game.phase == 0
        observation[contextOffset + 5] = self.game.current_player == 2
        observation[contextOffset + 6] = self.game.us_points >= self.game.them_points
        return observation

    @staticmethod
    def _card_index(card: pinochle_cpp.Card) -> int:
        rankIndex = {9: 0, 11: 1, 12: 2, 13: 3, 14: 4, 15: 5}[card.rank]
        return card.suit * 6 + rankIndex

    def _info(self) -> dict[str, Any]:
        return {
            'action_mask': self.action_masks(),
            'trump': self.game.trump,
            'us_points': self.game.us_points,
            'them_points': self.game.them_points,
        }

    def render(self) -> None:
        print(
            f'trump={self.game.trump} hand={len(self.game.hand)} '
            f'trick={len(self.game.trick)} score={self.game.us_points}-{self.game.them_points}',
        )
