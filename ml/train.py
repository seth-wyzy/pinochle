"""Train a PPO Pinochle agent and store checkpoints under ml/models/."""

from __future__ import annotations

import argparse
from pathlib import Path

from sb3_contrib import MaskablePPO
from stable_baselines3.common.callbacks import BaseCallback
from stable_baselines3.common.env_util import make_vec_env

from env.pinochle_env import (
    HeuristicPolicy,
    MaskableModelPolicy,
    PinochleEnv,
    RandomPolicy,
)


def load_checkpoint_policies(
    models_dir: Path,
    limit: int,
    expected_obs_shape: tuple[int, ...] = (459,),
    expected_action_dim: int = 47,
) -> list[MaskableModelPolicy]:
    """Load a bounded set of archived opponents matching current dimensions."""
    checkpointPaths = sorted(
        models_dir.glob('ppo_pinochle*.zip'),
        key=lambda path: path.stat().st_mtime,
        reverse=True,
    )
    policies: list[MaskableModelPolicy] = []
    for path in checkpointPaths:
        if len(policies) >= limit:
            break
        try:
            loadedModel = MaskablePPO.load(path, device='cpu')
            if (
                hasattr(loadedModel, 'observation_space')
                and loadedModel.observation_space.shape != expected_obs_shape
            ):
                continue
            if (
                hasattr(loadedModel, 'action_space')
                and getattr(loadedModel.action_space, 'n', None)
                != expected_action_dim
            ):
                continue
            policies.append(MaskableModelPolicy(loadedModel))
        except Exception as error:
            print(f'Warning: Could not load opponent checkpoint {path}: {error}')
    return policies


class SelfPlayCallback(BaseCallback):
    """Periodically save checkpoints and update the vectorized environment's policy pool."""

    def __init__(
        self,
        save_freq: int,
        models_dir: Path,
        checkpoint_limit: int = 4,
        verbose: int = 0,
    ) -> None:
        super().__init__(verbose)
        self.save_freq = max(1, save_freq)
        self.models_dir = models_dir
        self.checkpoint_limit = checkpoint_limit

    def _on_step(self) -> bool:
        if self.n_calls % self.save_freq == 0:
            checkpointPath = (
                self.models_dir / f'ppo_pinochle_{self.num_timesteps}_steps'
            )
            self.model.save(checkpointPath)
            if self.verbose > 0:
                print(f'Saved self-play checkpoint: {checkpointPath}')
            updatedPool = [HeuristicPolicy(), RandomPolicy()]
            updatedPool.extend(
                load_checkpoint_policies(self.models_dir, self.checkpoint_limit)
            )
            self.training_env.env_method('set_policy_pool', updatedPool)
            if self.verbose > 0:
                print(
                    f'Refreshed policy pool with {len(updatedPool)} opponents.'
                )
        return True


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
    parser.add_argument('--ent-coef', type=float, default=0.01)
    parser.add_argument('--checkpoint-freq', type=int, default=10_000)
    args = parser.parse_args()

    modelsDir = Path(__file__).resolve().parent / 'models'
    modelsDir.mkdir(parents=True, exist_ok=True)
    environment = make_vec_env(PinochleEnv, n_envs=args.n_envs, seed=args.seed)

    saveFreq = max(1, min(args.checkpoint_freq, args.timesteps // 2) // args.n_envs)
    selfPlayCallback = SelfPlayCallback(
        save_freq=saveFreq,
        models_dir=modelsDir,
        checkpoint_limit=args.opponent_checkpoints,
        verbose=1,
    )

    policyKwargs = {
        'net_arch': [256, 256],
    }

    model = MaskablePPO(
        'MlpPolicy',
        environment,
        policy_kwargs=policyKwargs,
        verbose=1,
        seed=args.seed,
        learning_rate=args.learning_rate,
        n_steps=args.n_steps,
        batch_size=args.batch_size,
        gamma=args.gamma,
        ent_coef=args.ent_coef,
    )

    opponentPool = [HeuristicPolicy(), RandomPolicy()]
    opponentPool.extend(
        load_checkpoint_policies(modelsDir, args.opponent_checkpoints)
    )
    environment.env_method('set_policy_pool', opponentPool)

    model.learn(total_timesteps=args.timesteps, callback=selfPlayCallback)
    model.save(modelsDir / 'ppo_pinochle_final')
    environment.close()


if __name__ == '__main__':
    main()
