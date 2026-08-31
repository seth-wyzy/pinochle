"""Benchmark the pybind simulator and evaluate a trained Pinochle policy.

Run, for example:
    python ml/benchmark.py --mode all --steps 100000 --repeats 5 --games 5000

The pure-Python game below deliberately mirrors the small non-interactive
training rules in ``Pin::reset_training`` and ``Pin::step_training``. It is a
reference baseline only; production training continues to use ``pinochle_cpp``.
"""

from __future__ import annotations

import argparse
import json
import platform
import sys
import time
from collections.abc import Callable
from dataclasses import dataclass, field
from pathlib import Path
from statistics import mean, stdev
from typing import Any

import numpy as np
from sb3_contrib import MaskablePPO

from env.pinochle_env import PinochleEnv, RandomPolicy


@dataclass(frozen=True)
class Card:
    rank: int
    suit: int
    copy: int = field(compare=False)


def winning_card(trick: list[Card], trump: int) -> Card:
    best = trick[0]
    for candidate in trick[1:]:
        if candidate.suit == best.suit:
            if candidate.rank > best.rank:
                best = candidate
        elif candidate.suit == trump:
            best = candidate
    return best


def legal_cards(hand: list[Card], trick: list[Card], trump: int) -> list[Card]:
    """Match getLegalCards in src/card.cpp, including its must-win rule."""
    if not trick:
        return hand
    lead_suit = trick[0].suit
    follow_suit = [card for card in hand if card.suit == lead_suit]
    trumps = [card for card in hand if card.suit == trump]
    best = winning_card(trick, trump)
    if follow_suit:
        beaters = [card for card in follow_suit if best.suit == lead_suit and card.rank > best.rank]
        return beaters or follow_suit
    if trumps:
        beaters = [card for card in trumps if best.suit == trump and card.rank > best.rank]
        return beaters or trumps
    return hand


class PurePythonGame:
    """Reference implementation of the pybind training-game interface."""

    def __init__(self) -> None:
        self.hands: list[list[Card]] = [[], [], [], []]
        self.trick: list[Card] = []
        self.trick_players: list[int] = []
        self.trump = -1
        self.phase = 0
        self.current_player = 2
        self.us_points = 0
        self.them_points = 0

    @property
    def hand(self) -> list[Card]:
        return self.hands[self.current_player]

    def player_hand(self, player: int) -> list[Card]:
        return self.hands[player]

    def reset(self, seed: int = 0) -> None:
        generator = np.random.default_rng(seed)
        deck = [Card(rank, suit, copy) for copy in range(2) for rank in (9, 11, 12, 13, 14, 15) for suit in range(4)]
        generator.shuffle(deck)
        self.hands = [sorted(deck[player::4], key=lambda card: (card.suit, card.rank)) for player in range(4)]
        self.trick = []
        self.trick_players = []
        self.trump = -1
        self.phase = 0
        self.current_player = 2
        self.us_points = 0
        self.them_points = 0

    def legal_actions(self) -> list[int]:
        if self.phase == 0:
            return [12, 13, 14, 15, 16]
        if self.phase != 1:
            return []
        eligible = legal_cards(self.hand, self.trick, self.trump)
        return [index for index, card in enumerate(self.hand) if card in eligible]

    def step(self, action: int) -> tuple[float, bool]:
        if action not in self.legal_actions():
            return -1.0, self.phase == 2
        if self.phase == 0:
            if action == 12:
                self.trump = self._choose_trump(self.hands[3])
                self.current_player = 3
            else:
                self.trump = action - 13
                self.current_player = 2
            self.phase = 1
            return 0.0, False
        self.trick.append(self.hand.pop(action))
        self.trick_players.append(self.current_player)
        self.current_player = (self.current_player + 1) % 4
        reward = self._resolve_trick() if len(self.trick) == 4 else 0.0
        return reward, self.phase == 2

    def _resolve_trick(self) -> float:
        winner_card = winning_card(self.trick, self.trump)
        winner = self.trick_players[self.trick.index(winner_card)]
        points = sum(card.rank >= 13 for card in self.trick)
        if not self.hands[2]:
            points += 1
        reward = float(points if winner in (0, 2) else -points)
        if winner in (0, 2):
            self.us_points += points
        else:
            self.them_points += points
        self.trick = []
        self.trick_players = []
        self.current_player = winner
        if not self.hands[2]:
            self.phase = 2
        return reward

    @staticmethod
    def _choose_trump(cards: list[Card]) -> int:
        melds = [PurePythonGame._count_meld(cards, suit) for suit in range(4)]
        return max(range(4), key=lambda suit: (melds[suit], -suit))


    @staticmethod
    def _count_meld(cards: list[Card], trump: int) -> int:
        ranks = (9, 11, 12, 13, 14, 15)
        counts = {(suit, rank): 0 for suit in range(4) for rank in ranks}
        for card in cards:
            counts[(card.suit, card.rank)] += 1
        score = 15 if all(counts[(trump, rank)] for rank in (15, 14, 13, 12, 11)) else 0
        for suit in range(4):
            if counts[(suit, 13)] and counts[(suit, 12)]:
                score += (4 if suit == trump else 2) * (2 if counts[(suit, 13)] > 1 and counts[(suit, 12)] > 1 else 1)
        for rank, points in ((15, 10), (13, 8), (12, 6), (11, 4)):
            if all(counts[(suit, rank)] for suit in range(4)):
                score += points
        pinochles = min(counts[(3, 12)], counts[(1, 11)])
        if pinochles:
            score += 30 if pinochles == 2 else 4 * pinochles
        return score + counts[(trump, 9)]


class PurePythonPinochleEnv(PinochleEnv):
    """PinochleEnv with only its game engine swapped for the Python baseline."""

    def __init__(self, *args: Any, **kwargs: Any) -> None:
        super().__init__(*args, **kwargs)
        self.game = PurePythonGame()


class ModelPolicy:
    def __init__(self, model: MaskablePPO) -> None:
        self.model = model

    def predict(self, observation: np.ndarray, action_mask: np.ndarray) -> int:
        action, _ = self.model.predict(observation, deterministic=True, action_masks=action_mask)
        return int(np.asarray(action).item())


def play_random_steps(environment: PinochleEnv, steps: int, seed: int) -> None:
    _, info = environment.reset(seed=seed)
    for _ in range(steps):
        action = int(np.flatnonzero(info['action_mask'])[0])
        _, _, terminated, truncated, info = environment.step(action)
        if terminated or truncated:
            _, info = environment.reset()


def timed_runs(run: Callable[[int], None], repeats: int) -> list[float]:
    durations = []
    for repeat in range(repeats):
        started = time.perf_counter()
        run(repeat)
        durations.append(time.perf_counter() - started)
    return durations


def summary(values: list[float], unit: str) -> dict[str, Any]:
    return {
        'unit': unit,
        'runs': len(values),
        'values': values,
        'mean': mean(values),
        'sample_standard_deviation': stdev(values) if len(values) > 1 else 0.0,
    }


def benchmark_rollout(environment_type: type[PinochleEnv], steps: int, repeats: int, seed: int) -> dict[str, Any]:
    def run(repeat: int) -> None:
        environment = environment_type()
        try:
            play_random_steps(environment, steps, seed + repeat)
        finally:
            environment.close()

    durations = timed_runs(run, repeats)
    return summary([steps / duration for duration in durations], 'environment decisions/second')


def benchmark_learning(environment_type: type[PinochleEnv], steps: int, repeats: int, seed: int) -> dict[str, Any]:
    def run(repeat: int) -> None:
        environment = environment_type()
        try:
            model = MaskablePPO('MlpPolicy', environment, seed=seed + repeat, verbose=0, n_steps=256, batch_size=64)
            model.learn(total_timesteps=steps)
        finally:
            environment.close()

    durations = timed_runs(run, repeats)
    return summary([steps / duration for duration in durations], 'PPO timesteps/second')


def wilson_interval(wins: int, games: int) -> tuple[float, float]:
    z = 1.959963984540054
    proportion = wins / games
    denominator = 1 + z ** 2 / games
    centre = (proportion + z ** 2 / (2 * games)) / denominator
    spread = z * ((proportion * (1 - proportion) / games + z ** 2 / (4 * games ** 2)) ** 0.5) / denominator
    return centre - spread, centre + spread


def evaluate(model_path: Path, games: int, seed: int) -> dict[str, Any]:
    model = MaskablePPO.load(model_path)
    agent = ModelPolicy(model)
    environment = PinochleEnv(policies=[RandomPolicy(), RandomPolicy(), agent, RandomPolicy()])
    wins = losses = ties = 0
    point_margins = []
    try:
        for game_number in range(games):
            _, info = environment.reset(seed=seed + game_number)
            terminated = False
            while not terminated:
                action = agent.predict(environment._observation(2), info['action_mask'])
                _, _, terminated, _, info = environment.step(action)
            margin = info['us_points'] - info['them_points']
            point_margins.append(margin)
            if margin > 0:
                wins += 1
            elif margin < 0:
                losses += 1
            else:
                ties += 1
    finally:
        environment.close()
    lower, upper = wilson_interval(wins, games)
    return {
        'model': str(model_path),
        'opponents': 'three independent uniform-random legal-action policies',
        'games': games,
        'wins': wins,
        'losses': losses,
        'ties': ties,
        'win_rate': wins / games,
        'win_rate_95_percent_wilson_interval': [lower, upper],
        'mean_point_margin': mean(point_margins),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--mode', choices=('rollout', 'learn', 'evaluate', 'all'), default='all')
    parser.add_argument('--steps', type=int, default=50_000, help='Steps per timing repetition.')
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--games', type=int, default=1_000)
    parser.add_argument('--seed', type=int, default=20260819)
    parser.add_argument('--model', type=Path, default=Path('ml/models/ppo_pinochle_final.zip'))
    parser.add_argument('--output', type=Path, default=Path('ml/benchmark_results.json'))
    args = parser.parse_args()
    if args.steps <= 0 or args.repeats <= 0 or args.games <= 0:
        parser.error('--steps, --repeats, and --games must be positive.')
    if args.mode in ('evaluate', 'all') and not args.model.exists():
        parser.error(f'Model does not exist: {args.model}')

    results: dict[str, Any] = {
        'command': ' '.join(sys.argv),
        'python': sys.version,
        'platform': platform.platform(),
        'seed': args.seed,
    }
    if args.mode in ('rollout', 'all'):
        results['rollout'] = {
            'cpp_pybind': benchmark_rollout(PinochleEnv, args.steps, args.repeats, args.seed),
            'pure_python': benchmark_rollout(PurePythonPinochleEnv, args.steps, args.repeats, args.seed),
        }
        results['rollout']['speedup'] = results['rollout']['cpp_pybind']['mean'] / results['rollout']['pure_python']['mean']
    if args.mode in ('learn', 'all'):
        results['learn'] = {
            'cpp_pybind': benchmark_learning(PinochleEnv, args.steps, args.repeats, args.seed),
            'pure_python': benchmark_learning(PurePythonPinochleEnv, args.steps, args.repeats, args.seed),
        }
        results['learn']['speedup'] = results['learn']['cpp_pybind']['mean'] / results['learn']['pure_python']['mean']
    if args.mode in ('evaluate', 'all'):
        results['evaluation'] = evaluate(args.model, args.games, args.seed)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(results, indent=2))
    print(f'\nWrote reproducible results to {args.output}')


if __name__ == '__main__':
    main()
