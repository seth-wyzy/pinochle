# Pinochle

This repository contains the C++17 Pinochle game and its optional
reinforcement-learning pipeline. The RL environment trains seat 2 while
Python policies control the other seats. Opponents can be the current model,
saved checkpoints, or a random baseline.

## Build the game and bindings

Build the C++ game and tests with:

```sh
cmake -S . -B build
cmake --build build -j2
ctest --test-dir build --output-on-failure
```

Install the Python dependencies before building the extension:

```sh
python3 -m venv .venv
. .venv/bin/activate
python -m pip install --upgrade pip
python -m pip install -r ml/requirements.txt
```

Reconfigure CMake after installing pybind11. The extension is written to
`build/python/`:

```sh
cmake -S . -B build -DPython3_EXECUTABLE="$(which python)"
cmake --build build -j2
```

The explicit Python path makes CMake use the same virtual environment where
`pybind11` was installed. Verify that `build/python/` contains the generated
`pinochle_cpp` module before starting training.

## Train agents

From the repository root, run the default 250,000-timestep training job:

```sh
python ml/train.py
```

The script creates `ml/models/` and writes periodic files named
`ppo_pinochle_*.zip`, followed by `ppo_pinochle_final.zip`. Each episode
randomly assigns seats 0, 1, and 3 from the live learner, recent checkpoints,
and the random baseline.

Common run-time controls are available as command-line options:

```sh
python ml/train.py \
    --timesteps 1000000 \
    --n-envs 8 \
    --seed 42 \
    --opponent-checkpoints 8
```

## Reset training

To start from a completely fresh model and opponent pool, stop any running
job and remove the generated checkpoints, then run training again:

```sh
rm -f ml/models/ppo_pinochle*.zip
python ml/train.py
```

Copy the model files elsewhere first if they need to be retained. To keep old
checkpoints on disk but exclude them from the new opponent pool, use:

```sh
python ml/train.py --opponent-checkpoints 0
```

The `--seed` option makes deck generation and environment sampling repeatable.

## Adjust hyperparameters

The main MaskablePPO settings are exposed by `ml/train.py`:

| Option | Default | Purpose |
| --- | ---: | --- |
| `--learning-rate` | `0.0003` | Optimizer step size |
| `--n-steps` | `2048` | Rollout steps collected per environment update |
| `--batch-size` | `64` | Minibatch size used for PPO updates |
| `--gamma` | `0.99` | Future-reward discount factor |
| `--ent-coef` | `0.0` | Extra action entropy incentive |
| `--timesteps` | `250000` | Total environment steps |
| `--n-envs` | `4` | Parallel environments |

For example, a longer run with more exploration is:

```sh
python ml/train.py \
    --timesteps 2000000 \
    --learning-rate 0.0001 \
    --n-steps 4096 \
    --batch-size 128 \
    --ent-coef 0.01
```

Keep `--batch-size` no larger than `--n-steps * --n-envs` and preferably choose
values that divide that rollout size evenly.
