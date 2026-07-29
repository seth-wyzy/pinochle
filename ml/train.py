"""Train a PPO Pinochle agent and store checkpoints under ml/models/."""

from __future__ import annotations

import argparse
from pathlib import Path

import numpy as np
from stable_baselines3.common.callbacks import CheckpointCallback
from stable_baselines3.common.env_util import make_vec_env
from sb3_contrib import MaskablePPO

from env.pinochle_env import PinochleEnv, RandomPolicy


class MaskableModelPolicy:
    """Adapts a MaskablePPO model to the environment's seat-policy protocol."""

    def __init__(self, model: MaskablePPO) -> None:
        self.model = model

    def predict(self, observation: np.ndarray, action_mask: np.ndarray) -> int:
        action, _ = self.model.predict(
            observation,
            deterministic=False,
            action_masks=action_mask,
        )
        return int(np.asarray(action).item())


def load_checkpoint_policies(models_dir: Path, limit: int) -> list[MaskableModelPolicy]:
    """Load a bounded set of archived opponents, newest checkpoint first."""
    checkpointPaths = sorted(
        models_dir.glob('ppo_pinochle*.zip'),
        key=lambda path: path.stat().st_mtime,
        reverse=True,
    )[:limit]
    return [MaskableModelPolicy(MaskablePPO.load(path)) for path in checkpointPaths]


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--timesteps', type=int, default=250_000)
    parser.add_argument('--n-envs', type=int, default=4)
    parser.add_argument('--seed', type=int, default=7)
    parser.add_argument('--opponent-checkpoints', type=int, default=4)
    parser.add_argument('--learning-rate', type=float, default=3e-4)
    parser.add_argument('--n-steps', type=int, default=2048)
    parser.add_argument('--batch-size', type=int, default=64)
    parser.add_argument('--gamma', type=float, default=0.99)
    parser.add_argument('--ent-coef', type=float, default=0.0)
    args = parser.parse_args()

    modelsDir = Path(__file__).resolve().parent / 'models'
    modelsDir.mkdir(parents=True, exist_ok=True)
    environment = make_vec_env(PinochleEnv, n_envs=args.n_envs, seed=args.seed)
    checkpoint = CheckpointCallback(
        save_freq=max(10_000 // args.n_envs, 1),
        save_path=str(modelsDir),
        name_prefix='ppo_pinochle',
    )
    model = MaskablePPO(
        'MlpPolicy',
        environment,
        verbose=1,
        seed=args.seed,
        learning_rate=args.learning_rate,
        n_steps=args.n_steps,
        batch_size=args.batch_size,
        gamma=args.gamma,
        ent_coef=args.ent_coef,
    )
    opponentPool = [MaskableModelPolicy(model), RandomPolicy()]
    opponentPool.extend(load_checkpoint_policies(modelsDir, args.opponent_checkpoints))
    environment.env_method('set_policy_pool', opponentPool)
    model.learn(total_timesteps=args.timesteps, callback=checkpoint)
    model.save(modelsDir / 'ppo_pinochle_final')
    environment.close()


if __name__ == '__main__':
    main()
