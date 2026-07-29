"""Train a PPO Pinochle agent and store checkpoints under ml/models/."""

from __future__ import annotations

import argparse
from pathlib import Path

from stable_baselines3.common.callbacks import CheckpointCallback
from stable_baselines3.common.env_util import make_vec_env
from sb3_contrib import MaskablePPO

from env.pinochle_env import PinochleEnv


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--timesteps', type=int, default=250_000)
    parser.add_argument('--n-envs', type=int, default=4)
    parser.add_argument('--seed', type=int, default=7)
    args = parser.parse_args()

    modelsDir = Path(__file__).resolve().parent / 'models'
    modelsDir.mkdir(parents=True, exist_ok=True)
    environment = make_vec_env(PinochleEnv, n_envs=args.n_envs, seed=args.seed)
    checkpoint = CheckpointCallback(
        save_freq=max(10_000 // args.n_envs, 1),
        save_path=str(modelsDir),
        name_prefix='ppo_pinochle',
    )
    model = MaskablePPO('MlpPolicy', environment, verbose=1, seed=args.seed)
    model.learn(total_timesteps=args.timesteps, callback=checkpoint)
    model.save(modelsDir / 'ppo_pinochle_final')
    environment.close()


if __name__ == '__main__':
    main()
