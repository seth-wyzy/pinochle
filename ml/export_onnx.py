"""Export a trained MaskablePPO policy network as ONNX action logits."""

from __future__ import annotations

import argparse
from pathlib import Path

import torch
from sb3_contrib import MaskablePPO


class ExportablePolicy(torch.nn.Module):
    """The policy trunk and action head, without SB3 sampling/state logic."""

    def __init__(self, policy: torch.nn.Module) -> None:
        super().__init__()
        self.policy = policy

    def forward(self, observation: torch.Tensor) -> torch.Tensor:
        features = self.policy.extract_features(observation)
        latentPolicy, _ = self.policy.mlp_extractor(features)
        return self.policy.action_net(latentPolicy)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('checkpoint', type=Path)
    parser.add_argument(
        '--output',
        type=Path,
        default=Path('web/models/pinochle_policy.onnx'),
    )
    parser.add_argument('--opset', type=int, default=17)
    args = parser.parse_args()

    model = MaskablePPO.load(args.checkpoint, device='cpu')
    policy = ExportablePolicy(model.policy).eval()
    observationSize = model.observation_space.shape[0]
    sample = torch.zeros((1, observationSize), dtype=torch.float32)
    args.output.parent.mkdir(parents=True, exist_ok=True)
    torch.onnx.export(
        policy,
        sample,
        args.output,
        input_names=['observation'],
        output_names=['action_logits'],
        dynamic_axes={
            'observation': {0: 'batch'},
            'action_logits': {0: 'batch'},
        },
        opset_version=args.opset,
    )
    print(f'Exported {args.checkpoint} to {args.output}')


if __name__ == '__main__':
    main()
