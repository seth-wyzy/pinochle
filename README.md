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

To retrain and refresh the model used by the web server in one command:

```sh
sh ml/train_and_export.sh --timesteps 1000000
```

This trains a new checkpoint and overwrites
`web/models/pinochle_policy.onnx`. Restart the Node server after replacing the
file so its ONNX session is reloaded.

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

## Export a model for the web app with ONNX

The web server serves static files from `web/`, so an exported policy can be
deployed as `web/models/pinochle_policy.onnx`. Export the final checkpoint
after training:

```sh
python ml/export_onnx.py \
    ml/models/ppo_pinochle_final.zip \
    --output web/models/pinochle_policy.onnx
```

The exporter writes the policy action head as `action_logits`. The input is a
`float32` observation with 391 features (the same observation produced by
`PinochleEnv`) and the output has 17 logits. Before selecting an action, the
web client must mask illegal actions using the room state and choose the
highest remaining logit. Actions 0–11 are card slots; 12 is pass; and 13–16
are bidding/trump actions.

For browser inference, install ONNX Runtime Web in the web application and
load the static artifact. Because this project serves plain JavaScript rather
than bundling a frontend, the quickest option is the browser distribution:

```sh
mkdir -p web/vendor
curl -L https://cdn.jsdelivr.net/npm/onnxruntime-web@1.20.1/dist/ort.min.js \
    -o web/vendor/ort.min.js
```

Add the runtime before `app.js` in `web/index.html`:

```html
<script src="vendor/ort.min.js"></script>
<script src="app.js"></script>
```

Then load the model from the same origin in browser code:

```js
const policySession = await ort.InferenceSession.create(
    '/models/pinochle_policy.onnx',
);
const input = new ort.Tensor('float32', observation, [1, 391]);
const output = await policySession.run({ observation: input });
const logits = output.action_logits.data;
const action = legalActions.reduce((best, candidate) => {
    return logits[candidate] > logits[best] ? candidate : best;
}, legalActions[0]);
```

`observation` must be the same 391-feature vector used during training, and
`legalActions` must be computed from the current room state before selecting a
logit. The model can provide a browser-side hint or control a browser-owned
seat. For normal web games, the Node server is authoritative and uses the same
ONNX model for server-owned AI seats.

Server-side inference uses `onnxruntime-node`. It is listed in the root
`package.json`, so a normal `npm install` installs it and the server loads the
model automatically. If the package cannot be installed, the server falls
back to the existing rule-based AI:

```sh
npm start
```

The server masks the model output against legal cards before playing, so the
model cannot make an illegal trick play.

Do not commit large `.onnx` artifacts unless the deployment repository is
intended to contain model binaries; store them in release/object storage and
copy or download them into `web/models/` during deployment when appropriate.

### Troubleshooting `ld.so` errors

Errors such as `Inconsistency detected by ld.so: dl-setup_hash.c` come from a
native shared-library loader, not from ONNX model parsing. They usually mean a
Conda library is being mixed with the Node/ONNX Runtime native binary. Run the
browser app with `onnxruntime-web` and do not install or import
`onnxruntime-node` in the browser. For a server-side Node runtime, try a clean
shell with the intended Node installation:

```sh
conda deactivate
env -u LD_LIBRARY_PATH -u LD_PRELOAD npm start
```

If the error persists, reinstall `onnxruntime-node` using that same Node/npm
installation and verify `node --version` and `npm --version` point to the same
environment.

## Deploy the model-backed AI to Render

Render runs the Node server only; it does not run the Python training pipeline.
Train and export locally, then commit and push all of these files before
deploying:

```text
package.json
server.js
web/models/pinochle_policy.onnx
web/models/pinochle_policy.onnx.data   # keep this beside the .onnx file
```

The existing `render.yaml` build command (`npm install`) installs
`onnxruntime-node`, and the start command (`npm start`) loads the committed
model. Render also needs the `DATABASE_URL` environment variable configured in
the service. A model retrained on your workstation is not available to Render
until the refreshed ONNX files are committed and pushed.

After a model update:

```sh
sh ml/train_and_export.sh --timesteps 1000000
git add package.json server.js web/models/pinochle_policy.onnx web/models/pinochle_policy.onnx.data
git commit -m "Update web AI model"
git push
```

Render will rebuild the service, install the native runtime, and restart with
the new model. Keep the `.onnx.data` sidecar; deleting it makes the model
unloadable.

