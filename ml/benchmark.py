"""Benchmark the pybind simulator and evaluate a trained Pinochle policy.

Run, for example:
    python ml/benchmark.py --mode all --steps 100000 --repeats 5 --games 5000

The pure-Python game below deliberately mirrors the non-interactive
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

from env.pinochle_env import (
    HeuristicPolicy,
    MaskableModelPolicy,
    PinochleEnv,
    RandomPolicy,
)


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
    leadSuit = trick[0].suit
    followSuit = [card for card in hand if card.suit == leadSuit]
    trumps = [card for card in hand if card.suit == trump]
    best = winning_card(trick, trump)
    if followSuit:
        beaters = [
            card
            for card in followSuit
            if best.suit == leadSuit and card.rank > best.rank
        ]
        return beaters or followSuit
    if trumps:
        beaters = [
            card
            for card in trumps
            if best.suit == trump and card.rank > best.rank
        ]
        return beaters or trumps
    return hand


class PurePythonGame:
    """Reference implementation of the pybind training-game interface."""

    def __init__(self) -> None:
        self.hands: list[list[Card]] = [[], [], [], []]
        self.trick: list[Card] = []
        self.trick_players: list[int] = []
        self.played_cards: list[Card] = []
        self.trump = -1
        self.phase = 0
        self.dealer = 3
        self.bid = 20
        self.bid_winner = -1
        self.current_player = 0
        self.active_bidders = [True, True, True, True]
        self.us_points = 0
        self.them_points = 0
        self.us_meld = 0
        self.them_meld = 0
        self.us_trick_points = 0
        self.them_trick_points = 0
        self.us_score = 0
        self.them_score = 0

    @property
    def hand(self) -> list[Card]:
        return self.hands[self.current_player]

    def player_hand(self, player: int) -> list[Card]:
        return self.hands[player]

    def reset(self, seed: int = 0) -> None:
        generator = np.random.default_rng(seed)
        deck = [
            Card(rank, suit, copy)
            for copy in range(2)
            for rank in (9, 11, 12, 13, 14, 15)
            for suit in range(4)
        ]
        generator.shuffle(deck)
        self.hands = [
            sorted(deck[player::4], key=lambda card: (card.suit, card.rank))
            for player in range(4)
        ]
        self.trick = []
        self.trick_players = []
        self.played_cards = []
        self.dealer = (self.dealer + 1) % 4
        self.bid = 20
        self.bid_winner = -1
        self.trump = -1
        self.phase = 0
        self.active_bidders = [True, True, True, True]
        self.current_player = (self.dealer + 1) % 4
        self.us_points = 0
        self.them_points = 0
        self.us_meld = 0
        self.them_meld = 0
        self.us_trick_points = 0
        self.them_trick_points = 0
        self.us_score = 0
        self.them_score = 0

    def legal_actions(self) -> list[int]:
        if self.phase == 0:
            actions = [12]  # Pass
            for a in range(13, 43):
                bidAmount = 21 + (a - 13)
                if bidAmount > self.bid:
                    actions.append(a)
            return actions
        if self.phase == 1:
            return [43, 44, 45, 46]
        if self.phase == 2:
            eligible = legal_cards(self.hand, self.trick, self.trump)
            return [
                index
                for index, card in enumerate(self.hand)
                if card in eligible
            ]
        return []

    def step(self, action: int) -> tuple[float, bool]:
        if action not in self.legal_actions():
            return -1.0, self.phase == 3

        if self.phase == 0:
            if action == 12:
                self.active_bidders[self.current_player] = False
            elif 13 <= action <= 42:
                self.bid = 21 + (action - 13)
                self.bid_winner = self.current_player

            activeCount = sum(self.active_bidders)
            if activeCount <= 1:
                if self.bid_winner == -1:
                    # 3 non-dealers passed without bidding: dealer stuck with 20
                    self.bid_winner = self.dealer
                    self.bid = 20
                self.phase = 1
                self.current_player = self.bid_winner
            else:
                nextP = (self.current_player + 1) % 4
                while not self.active_bidders[nextP]:
                    nextP = (nextP + 1) % 4
                self.current_player = nextP
            return 0.0, False

        if self.phase == 1:
            self.trump = action - 43
            self.us_meld = (
                self._count_meld(self.hands[0], self.trump)
                + self._count_meld(self.hands[2], self.trump)
            )
            self.them_meld = (
                self._count_meld(self.hands[1], self.trump)
                + self._count_meld(self.hands[3], self.trump)
            )
            self.phase = 2
            self.current_player = self.bid_winner
            return 0.0, False

        # Phase 2: Trick Play
        self.trick.append(self.hand.pop(action))
        self.trick_players.append(self.current_player)
        if len(self.trick) == 4:
            reward = self._resolve_trick()
        else:
            self.current_player = (self.current_player + 1) % 4
            reward = 0.0
        return reward, self.phase == 3

    def _resolve_trick(self) -> float:
        winnerCard = winning_card(self.trick, self.trump)
        winner = self.trick_players[self.trick.index(winnerCard)]
        points = sum(card.rank >= 13 for card in self.trick)
        isLastTrick = len(self.hands[0]) == 0
        if isLastTrick:
            points += 1

        if winner in (0, 2):
            self.us_trick_points += points
            self.us_points += points
            reward = float(points) / 25.0
        else:
            self.them_trick_points += points
            self.them_points += points
            reward = -float(points) / 25.0

        self.played_cards.extend(self.trick)
        self.trick = []
        self.trick_players = []
        self.current_player = winner

        if isLastTrick:
            self.phase = 3
            finalUs = self.us_meld + self.us_trick_points
            finalThem = self.them_meld + self.them_trick_points
            if self.bid_winner in (0, 2):
                usScore = finalUs if finalUs >= self.bid else -self.bid
                themScore = finalThem
            else:
                themScore = finalThem if finalThem >= self.bid else -self.bid
                usScore = finalUs
            self.us_score = usScore
            self.them_score = themScore
            totalTargetReward = float(usScore - themScore) / 25.0
            trickRewardsGiven = (
                float(self.us_trick_points - self.them_trick_points) / 25.0
            )
            reward += totalTargetReward - trickRewardsGiven

        return reward

    @staticmethod
    def _count_meld(cards: list[Card], trump: int) -> int:
        ranks = (9, 11, 12, 13, 14, 15)
        counts = {(suit, rank): 0 for suit in range(4) for rank in ranks}
        for card in cards:
            counts[(card.suit, card.rank)] += 1
        score = (
            15
            if all(counts[(trump, rank)] for rank in (15, 14, 13, 12, 11))
            else 0
        )
        for suit in range(4):
            if counts[(suit, 13)] and counts[(suit, 12)]:
                score += (4 if suit == trump else 2) * (
                    2
                    if counts[(suit, 13)] > 1 and counts[(suit, 12)] > 1
                    else 1
                )
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


def benchmark_rollout(
    environment_type: type[PinochleEnv],
    steps: int,
    repeats: int,
    seed: int,
) -> dict[str, Any]:
    def run(repeat: int) -> None:
        environment = environment_type()
        try:
            play_random_steps(environment, steps, seed + repeat)
        finally:
            environment.close()

    durations = timed_runs(run, repeats)
    return summary(
        [steps / duration for duration in durations],
        'environment decisions/second',
    )


def benchmark_learning(
    environment_type: type[PinochleEnv],
    steps: int,
    repeats: int,
    seed: int,
) -> dict[str, Any]:
    def run(repeat: int) -> None:
        environment = environment_type()
        try:
            model = MaskablePPO(
                'MlpPolicy',
                environment,
                seed=seed + repeat,
                verbose=0,
                n_steps=256,
                batch_size=64,
            )
            model.learn(total_timesteps=steps)
        finally:
            environment.close()

    durations = timed_runs(run, repeats)
    return summary(
        [steps / duration for duration in durations],
        'PPO timesteps/second',
    )


def wilson_interval(wins: int, games: int) -> tuple[float, float]:
    z = 1.959963984540054
    proportion = wins / games
    denominator = 1 + z**2 / games
    centre = (proportion + z**2 / (2 * games)) / denominator
    spread = (
        z
        * ((proportion * (1 - proportion) / games + z**2 / (4 * games**2)) ** 0.5)
        / denominator
    )
    return centre - spread, centre + spread


def evaluate(model_path: Path, games: int, seed: int) -> dict[str, Any]:
    model = MaskablePPO.load(model_path)
    agent = MaskableModelPolicy(model, deterministic=True)
    environment = PinochleEnv(
        policies=[RandomPolicy(), RandomPolicy(), agent, RandomPolicy()]
    )
    wins = losses = ties = 0
    point_margins: list[float] = []
    try:
        for game_number in range(games):
            _, info = environment.reset(seed=seed + game_number)
            terminated = False
            while not terminated:
                action = agent.predict(
                    environment._observation(2),
                    info['action_mask'],
                )
                _, _, terminated, _, info = environment.step(action)
            margin = float(info['us_points'] - info['them_points'])
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
    parser.add_argument(
        '--mode',
        choices=('rollout', 'learn', 'evaluate', 'all'),
        default='all',
    )
    parser.add_argument(
        '--steps',
        type=int,
        default=50_000,
        help='Steps per timing repetition.',
    )
    parser.add_argument('--repeats', type=int, default=3)
    parser.add_argument('--games', type=int, default=1_000)
    parser.add_argument('--seed', type=int, default=20260819)
    parser.add_argument(
        '--model',
        type=Path,
        default=Path('ml/models/ppo_pinochle_final.zip'),
    )
    parser.add_argument(
        '--output',
        type=Path,
        default=Path('ml/benchmark_results.json'),
    )
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
            'cpp_pybind': benchmark_rollout(
                PinochleEnv,
                args.steps,
                args.repeats,
                args.seed,
            ),
            'pure_python': benchmark_rollout(
                PurePythonPinochleEnv,
                args.steps,
                args.repeats,
                args.seed,
            ),
        }
        results['rollout']['speedup'] = (
            results['rollout']['cpp_pybind']['mean']
            / results['rollout']['pure_python']['mean']
        )
    if args.mode in ('learn', 'all'):
        results['learn'] = {
            'cpp_pybind': benchmark_learning(
                PinochleEnv,
                args.steps,
                args.repeats,
                args.seed,
            ),
            'pure_python': benchmark_learning(
                PurePythonPinochleEnv,
                args.steps,
                args.repeats,
                args.seed,
            ),
        }
        results['learn']['speedup'] = (
            results['learn']['cpp_pybind']['mean']
            / results['learn']['pure_python']['mean']
        )
    if args.mode in ('evaluate', 'all'):
        results['evaluation'] = evaluate(args.model, args.games, args.seed)

    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_text(json.dumps(results, indent=2) + '\n', encoding='utf-8')
    print(json.dumps(results, indent=2))
    print(f'\nWrote reproducible results to {args.output}')


if __name__ == '__main__':
    main()
