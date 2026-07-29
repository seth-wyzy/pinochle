#!/usr/bin/env sh
set -eu

python ml/train.py "$@"
python ml/export_onnx.py \
    ml/models/ppo_pinochle_final.zip \
    --output web/models/pinochle_policy.onnx

echo 'Updated web/models/pinochle_policy.onnx'
