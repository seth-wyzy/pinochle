import sys
from pathlib import Path

# Add ml directory to sys.path
sys.path.insert(0, str(Path(__file__).resolve().parent))

import numpy as np
from env.pinochle_env import PinochleEnv, HeuristicPolicy, RandomPolicy, MaskableModelPolicy
from sb3_contrib import MaskablePPO

def evaluate_matchup(name, policies, seat2_policy, games=500, seed=42):
    env = PinochleEnv(policies=policies)
    wins = losses = ties = 0
    point_margins = []
    round_margins = []

    for game_num in range(games):
        obs, info = env.reset(seed=seed + game_num)
        term = False
        while not term:
            action = seat2_policy.predict(obs, info['action_mask'])
            obs, rew, term, trunc, info = env.step(action)
        # Margin from seat 2's team perspective
        # info has us_points, them_points
        # in PinochleGame: us_points is team Us (seats 0 & 2), them_points is team Them (seats 1 & 3)
        margin = float(info['us_points'] - info['them_points'])
        point_margins.append(margin)
        if margin > 0:
            wins += 1
        elif margin < 0:
            losses += 1
        else:
            ties += 1

    env.close()
    win_rate = wins / games
    mean_margin = np.mean(point_margins)
    print(f"=== {name} ({games} games) ===")
    print(f"Wins: {wins} ({win_rate:.1%}) | Losses: {losses} ({losses/games:.1%}) | Ties: {ties} ({ties/games:.1%})")
    print(f"Mean Score Margin: {mean_margin:+.2f}")
    return win_rate, mean_margin

if __name__ == "__main__":
    h = HeuristicPolicy()
    r = RandomPolicy()
    
    # Matchup 1: All 4 players are Heuristic (Team Us: 0 & 2 vs Team Them: 1 & 3)
    evaluate_matchup("4x Heuristic AI (Self-Play Symmetry)", [h, h, None, h], h, games=500)

    # Matchup 2: Team Us (Heuristic) vs Team Them (Random)
    evaluate_matchup("Team Heuristic vs Team Random", [h, r, None, r], h, games=500)

    # Matchup 3: If a trained model checkpoint exists, test against Heuristics
    model_path = Path("ml/models/ppo_pinochle_final.zip")
    if model_path.exists():
        try:
            model = MaskablePPO.load(model_path)
            model_policy = MaskableModelPolicy(model, deterministic=True)
            evaluate_matchup("Team RL Model (Seat 2) + Heuristic Partner vs Team Heuristic", [h, h, None, h], model_policy, games=500)
        except Exception as e:
            print("Could not evaluate trained model:", e)
