from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from pydantic import BaseModel
from typing import List, Optional, Dict
import os
import db
from game_logic import Game

app = FastAPI()

app.add_middleware(
    CORSMiddleware,
    allow_origins=["*"],
    allow_methods=["*"],
    allow_headers=["*"],
)

# Initialize DB table on startup if needed
@app.on_event("startup")
def on_startup():
    db.init_db()

class JoinRequest(BaseModel):
    seat_index: int
    name: str

class BidRequest(BaseModel):
    seat_index: int
    amount: int

class TrumpRequest(BaseModel):
    suit: int

class PlayRequest(BaseModel):
    seat_index: int
    card_index: int

class MeldConfirmRequest(BaseModel):
    seat_index: int
    selected_indices: List[int]

class ModeRequest(BaseModel):
    mode: str

class AIRequest(BaseModel):
    seat_index: int

@app.post("/api/lobby/mode")
async def set_game_mode(req: ModeRequest):
    game = db.load_game()
    game.set_game_mode(req.mode)
    db.save_game(game)
    return game.get_state()

@app.post("/api/lobby/ai/add")
async def add_ai(req: AIRequest):
    game = db.load_game()
    success = game.add_ai(req.seat_index)
    if not success:
        raise HTTPException(status_code=400, detail="Cannot add AI to this seat")
    db.save_game(game)
    return game.get_state()

@app.post("/api/lobby/ai/remove")
async def remove_ai(req: AIRequest):
    game = db.load_game()
    success = game.remove_ai(req.seat_index)
    if not success:
        raise HTTPException(status_code=400, detail="Cannot remove AI from this seat")
    db.save_game(game)
    return game.get_state()

@app.post("/api/lobby/join")
async def join_seat(req: JoinRequest):
    game = db.load_game()
    success = game.join_seat(req.seat_index, req.name)
    if not success:
        raise HTTPException(status_code=400, detail="Seat occupied or invalid")
    db.save_game(game)
    return game.get_state()

@app.post("/api/lobby/vacate")
async def vacate_seat(req: JoinRequest):
    game = db.load_game()
    success = game.vacate_seat(req.seat_index)
    if not success:
         raise HTTPException(status_code=400, detail="Seat already empty or invalid")
    db.save_game(game)
    return game.get_state()

@app.post("/api/game/start")
async def start_game():
    game = db.load_game()
    game.start_game()
    db.save_game(game)
    return game.get_state()

@app.post("/api/game/new")
async def new_game():
    game = Game() # Reset everything
    db.save_game(game)
    return game.get_state()

@app.post("/api/game/next_round")
async def next_round():
    game = db.load_game()
    game.start_next_round()
    db.save_game(game)
    return game.get_state()

@app.get("/api/game/state")
def get_state():
    game = db.load_game()
    return game.get_state()

@app.post("/api/game/bid")
async def place_bid(req: BidRequest):
    game = db.load_game()
    if game.phase != "bidding" or game.current_bidder != req.seat_index:
        raise HTTPException(status_code=400, detail="Not your turn to bid or wrong phase")
    game.handle_bid(req.seat_index, req.amount)
    db.save_game(game)
    return game.get_state()

@app.post("/api/game/trump")
async def select_trump(req: TrumpRequest):
    game = db.load_game()
    if game.phase != "trump_selection":
        raise HTTPException(status_code=400, detail="Wrong phase for trump selection")
    game.select_trump(req.suit)
    db.save_game(game)
    return game.get_state()

@app.post("/api/game/meld/confirm")
async def confirm_meld(req: MeldConfirmRequest):
    game = db.load_game()
    game.confirm_user_meld(req.seat_index, req.selected_indices)
    db.save_game(game)
    return game.get_state()

@app.post("/api/game/start_tricks")
async def start_tricks():
    game = db.load_game()
    if game.phase != "meld_display":
         raise HTTPException(status_code=400, detail="Wrong phase to start tricks")
    game.start_tricks()
    db.save_game(game)
    return game.get_state()

@app.post("/api/game/ai_play")
async def ai_play():
    game = db.load_game()
    played = game.ai_play_one()
    if played:
        db.save_game(game)
    return {"played": played, "state": game.get_state()}

@app.post("/api/game/play")
async def play_card(req: PlayRequest):
    game = db.load_game()
    active_players = game.get_active_players()
    current_idx = (active_players.index(game.trick_leader) + len(game.current_trick)) % len(active_players)
    curr_p = active_players[current_idx]
    
    if game.phase != "trick_taking" or curr_p != req.seat_index:
        raise HTTPException(status_code=400, detail="Not your turn to play or wrong phase")
    
    if req.card_index < 0 or req.card_index >= len(game.hands[req.seat_index]):
        raise HTTPException(status_code=400, detail="Invalid card index")
        
    try:
        game.play_card(req.seat_index, req.card_index)
    except ValueError as e:
        raise HTTPException(status_code=400, detail=str(e))
        
    db.save_game(game)
    return game.get_state()

@app.post("/api/game/evaluate")
async def evaluate():
    game = db.load_game()
    active_players = game.get_active_players()
    if game.phase != "trick_taking" or len(game.current_trick) != len(active_players):
         raise HTTPException(status_code=400, detail="Trick not finished")
    game.evaluate_trick()
    db.save_game(game)
    return game.get_state()
