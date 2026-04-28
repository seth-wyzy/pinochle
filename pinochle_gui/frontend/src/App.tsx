import React, { useState, useEffect, useRef } from 'react';
import './App.css';

interface CardData {
  rank: number;
  suit: number;
  rank_name: string;
  suit_name: string;
}

interface TrickEntry {
  player: number;
  card: CardData;
}

interface GameState {
  game_mode: string;
  active_players: number[];
  player_totals: number[];
  game_num: number;
  rounds_played: number;
  us_games: number;
  them_games: number;
  round: number;
  dealer: number;
  us_total: number;
  them_total: number;
  phase: string;
  hands: CardData[][];
  seat_assignments: (string | null)[];
  player_names: string[];
  trump: number;
  bid: number;
  bid_winner: number;
  current_bidder: number;
  current_trick: TrickEntry[];
  trick_leader: number;
  us_trick_points: number;
  them_trick_points: number;
  player_trick_points: number[];
  melds: number[];
  meld_details: string[][];
  meld_cards: CardData[][][]; 
  tricks_played: number;
  log: string[];
}

const SUIT_ICONS: { [key: number]: string } = {
  3: '♠', 2: '♥', 1: '♣', 0: '♦'
};

const Card: React.FC<{ card: CardData; onClick?: () => void; selected?: boolean; dimmed?: boolean }> = ({ card, onClick, selected, dimmed }) => {
  const isRed = card.suit === 2 || card.suit === 0;
  return (
    <div 
      className={`card ${isRed ? 'red' : 'black'} ${selected ? 'selected' : ''} ${dimmed ? 'dimmed' : ''}`} 
      onClick={onClick}
      style={selected ? { transform: 'translateY(-20px)', boxShadow: '0 0 15px #f1c40f' } : {}}
    >
      <div className="rank-text">{card.rank_name}</div>
      <div className="suit-icon">{SUIT_ICONS[card.suit]}</div>
    </div>
  );
};

function App() {
  const [gameState, setGameState] = useState<GameState | null>(null);
  const [mySeat, setMySeat] = useState<number | null>(() => {
    const saved = localStorage.getItem('pinochle_seat');
    return saved !== null ? parseInt(saved) : null;
  });
  const [myUsername, setMyUsername] = useState<string>(() => {
    return localStorage.getItem('pinochle_username') || "";
  });
  const [customBid, setCustomBid] = useState<number>(21);
  const [message, setMessage] = useState<string | null>(null);
  const [selectedMeldIndices, setSelectedMeldIndices] = useState<number[]>([]);
  
  // Use relative-to-root URLs for everything in Vercel
  const API_BASE = `/api`;
  
  const logEndRef = useRef<HTMLDivElement>(null);

  useEffect(() => {
    localStorage.setItem('pinochle_username', myUsername);
  }, [myUsername]);

  useEffect(() => {
    if (mySeat !== null) {
      localStorage.setItem('pinochle_seat', mySeat.toString());
    } else {
      localStorage.removeItem('pinochle_seat');
    }
  }, [mySeat]);

  const fetchState = async () => {
    try {
      const res = await fetch(`${API_BASE}/game/state`);
      if (res.ok) {
        const data = await res.json();
        setGameState(data);
      }
    } catch (e) {
      console.error("Fetch state error:", e);
    }
  };

  useEffect(() => {
    // Initial fetch
    fetchState();

    // Set up polling for real-time updates (replaces WebSockets for Vercel)
    const pollInterval = setInterval(() => {
      fetchState();
    }, 1000);

    return () => {
      clearInterval(pollInterval);
    };
  }, []);

  // AI play loop trigger
  useEffect(() => {
    if (gameState?.phase === 'trick_taking') {
      const activePlayers = gameState.active_players || [0, 1, 2, 3];
      const currentIndex = (activePlayers.indexOf(gameState.trick_leader) + gameState.current_trick.length) % activePlayers.length;
      const curr_p = activePlayers[currentIndex];
      
      if (gameState.player_names[curr_p]?.startsWith("AI") && gameState.current_trick.length < activePlayers.length) {
        const firstHuman = gameState.seat_assignments.findIndex(s => s !== null && s !== "AI");
        if (mySeat === firstHuman) {
          const timer = setTimeout(async () => {
            const res = await fetch(`${API_BASE}/game/ai_play`, { method: 'POST' });
            if (res.ok) {
              const data = await res.json();
              setGameState(data.state);
            }
          }, 1000);
          return () => clearTimeout(timer);
        }
      }
    }
  }, [gameState, mySeat]);

  useEffect(() => {
    if (logEndRef.current) {
      logEndRef.current.scrollIntoView({ behavior: 'smooth' });
    }
  }, [gameState?.log]);

  useEffect(() => {
    if (gameState?.phase === 'bidding' && gameState.current_bidder === mySeat) {
      const minStartBid = gameState.game_mode === "5-card" ? 5 : 20;
      setCustomBid(gameState.bid === minStartBid ? minStartBid + 1 : gameState.bid + 1);
    }
  }, [gameState?.bid, gameState?.phase, gameState?.current_bidder, mySeat, gameState?.game_mode]);

  const joinGame = async (seat: number) => {
    if (!myUsername) {
      setMessage("Please enter a username first.");
      return;
    }
    const res = await fetch(`${API_BASE}/lobby/join`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ seat_index: seat, name: myUsername })
    });
    if (res.ok) {
      setMySeat(seat);
      setGameState(await res.json());
    } else {
      const err = await res.json();
      setMessage(err.detail);
    }
  };

  const leaveSeat = async () => {
    if (mySeat !== null) {
      await fetch(`${API_BASE}/lobby/vacate`, {
        method: 'POST',
        headers: { 'Content-Type': 'application/json' },
        body: JSON.stringify({ seat_index: mySeat, name: myUsername })
      });
    }
    setMySeat(null);
    setMessage(null);
    localStorage.removeItem('pinochle_seat');
  };

  const resetServer = async () => {
    if (window.confirm("This will reset the game for EVERYONE. Are you sure?")) {
      const res = await fetch(`${API_BASE}/game/new`, { method: 'POST' });
      if (res.ok) {
        setGameState(await res.json());
        setMySeat(null);
        localStorage.removeItem('pinochle_seat');
      }
    }
  };

  const setGameMode = async (mode: string) => {
    const res = await fetch(`${API_BASE}/lobby/mode`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ mode })
    });
    if (res.ok) setGameState(await res.json());
  };

  const toggleAI = async (seat: number, action: 'add' | 'remove') => {
    const res = await fetch(`${API_BASE}/lobby/ai/${action}`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ seat_index: seat })
    });
    if (res.ok) {
      setGameState(await res.json());
    } else {
      const err = await res.json();
      setMessage(err.detail);
    }
  };

  const startMatch = async () => {
    const res = await fetch(`${API_BASE}/game/start`, { method: 'POST' });
    if (res.ok) setGameState(await res.json());
  };

  const startNewGame = async () => {
    const res = await fetch(`${API_BASE}/game/new`, { method: 'POST' });
    if (res.ok) setGameState(await res.json());
  };

  const nextRound = async () => {
    const res = await fetch(`${API_BASE}/game/next_round`, { method: 'POST' });
    if (res.ok) setGameState(await res.json());
  };

  const placeBid = async (amount: number) => {
    setMessage(null);
    const res = await fetch(`${API_BASE}/game/bid`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ seat_index: mySeat, amount })
    });
    if (res.ok) {
      setGameState(await res.json());
    } else {
      const err = await res.json();
      setMessage(err.detail);
      setTimeout(() => setMessage(null), 3000);
    }
  };

  const selectTrump = async (suit: number) => {
    const res = await fetch(`${API_BASE}/game/trump`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ suit })
    });
    if (res.ok) {
      setGameState(await res.json());
      setSelectedMeldIndices([]);
    }
  };

  const toggleMeldCard = (index: number) => {
    if (selectedMeldIndices.includes(index)) {
      setSelectedMeldIndices(selectedMeldIndices.filter(i => i !== index));
    } else {
      setSelectedMeldIndices([...selectedMeldIndices, index]);
    }
  };

  const confirmMeld = async () => {
    const res = await fetch(`${API_BASE}/game/meld/confirm`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ seat_index: mySeat, selected_indices: selectedMeldIndices })
    });
    if (res.ok) {
      setGameState(await res.json());
      setSelectedMeldIndices([]);
    }
  };

  const startTricks = async () => {
    const res = await fetch(`${API_BASE}/game/start_tricks`, { method: 'POST' });
    if (res.ok) setGameState(await res.json());
  };

  const playCard = async (index: number) => {
    setMessage(null);
    const res = await fetch(`${API_BASE}/game/play`, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ seat_index: mySeat, card_index: index })
    });
    if (res.ok) {
      setGameState(await res.json());
    } else {
      const err = await res.json();
      setMessage(err.detail);
      setTimeout(() => setMessage(null), 3000);
    }
  };

  const evaluateTrick = async () => {
    const res = await fetch(`${API_BASE}/game/evaluate`, { method: 'POST' });
    if (res.ok) setGameState(await res.json());
  };

  if (!gameState) return <div className="game-container">Connecting...</div>;

  if (mySeat === null || gameState.phase === 'lobby') {
    return (
      <div className="game-container">
        <h1>Pinochle Lobby</h1>
        <div className="bidding-panel" style={{width: 500}}>
          {gameState.phase === 'lobby' && (
            <div style={{marginBottom: 20}}>
              <h3>Game Mode</h3>
              <select 
                value={gameState.game_mode} 
                onChange={(e) => setGameMode(e.target.value)}
                style={{width: '100%', padding: 10, fontSize: '1.1em'}}
              >
                <option value="standard">Standard (4 Players, Teams, 12 Cards)</option>
                <option value="5-card">5-Card (2-4 Players, Free-for-all, 5 Cards)</option>
              </select>
            </div>
          )}

          <input 
            type="text" 
            placeholder="Your Name" 
            value={myUsername} 
            onChange={(e) => setMyUsername(e.target.value)}
            style={{width: '90%', padding: 10, marginBottom: 20, fontSize: '1.2em'}}
          />
          <h3>Select a Seat</h3>
          <div style={{display: 'grid', gridTemplateColumns: '1fr 1fr', gap: 10}}>
            {[0, 1, 2, 3].map(i => {
              const occupant = gameState.seat_assignments[i];
              const isMe = occupant === myUsername;
              const isAI = occupant === "AI";
              const isEmpty = occupant === null;
              
              return (
                <div key={i} style={{display: 'flex', flexDirection: 'column', gap: 5}}>
                  <button 
                    onClick={() => joinGame(i)} 
                    disabled={!isEmpty && !isMe}
                    style={{
                      padding: '15px 5px', 
                      background: isMe ? '#3498db' : (isEmpty ? '#27ae60' : '#7f8c8d'),
                      opacity: (!isEmpty && !isMe) ? 0.7 : 1
                    }}
                  >
                    {['North', 'East', 'South', 'West'][i]}
                    {occupant ? ` (${occupant})` : ''}
                  </button>
                  {gameState.phase === 'lobby' && (
                    <>
                      {isEmpty && <button onClick={() => toggleAI(i, 'add')} style={{background: '#8e44ad', padding: '5px', fontSize: '0.8em'}}>Add AI</button>}
                      {isAI && <button onClick={() => toggleAI(i, 'remove')} style={{background: '#e67e22', padding: '5px', fontSize: '0.8em'}}>Remove AI</button>}
                    </>
                  )}
                </div>
              );
            })}
          </div>
          {gameState.active_players?.length >= 2 && (
            <button onClick={startMatch} style={{marginTop: 30, width: '100%', padding: 15, background: '#f39c12'}}>
              {gameState.phase === 'lobby' ? 'Start Game' : 'Rejoin Game'}
            </button>
          )}
          <button onClick={resetServer} style={{marginTop: 10, width: '100%', padding: 10, background: '#c0392b', fontSize: '0.8em'}}>
            Reset Server (Clear All)
          </button>
        </div>
        {message && <p style={{color: '#e74c3c'}}>{message}</p>}
      </div>
    );
  }

  const getRelativePos = (idx: number) => {
    if (idx === mySeat) return "south";
    if (idx === (mySeat + 1) % 4) return "west";
    if (idx === (mySeat + 2) % 4) return "north";
    if (idx === (mySeat + 3) % 4) return "east";
    return "";
  };

  const activePlayers = gameState.active_players || [0, 1, 2, 3];
  const isMyTurnToPlay = gameState.phase === 'trick_taking' && 
                         activePlayers[(activePlayers.indexOf(gameState.trick_leader) + gameState.current_trick.length) % activePlayers.length] === mySeat;

  return (
    <div className="game-container">
      <div className="scoreboard">
        <div>Game: {gameState.game_num} {gameState.game_mode === 'standard' && `(${gameState.us_games} - ${gameState.them_games})`}</div>
        <div>Round: {gameState.rounds_played} {gameState.game_mode === 'standard' && "/ 4"}</div>
        {gameState.game_mode === 'standard' ? (
          <>
            <div>Us: {gameState.us_total} (+{(gameState.melds?.[0] || 0) + (gameState.melds?.[2] || 0)})</div>
            <div>Them: {gameState.them_total} (+{(gameState.melds?.[1] || 0) + (gameState.melds?.[3] || 0)})</div>
          </>
        ) : (
          <div style={{display: 'flex', gap: '15px'}}>
            {gameState.active_players?.map(p => (
              <div key={p}>{gameState.player_names?.[p]}: {gameState.player_totals?.[p]} (+{gameState.melds?.[p] || 0})</div>
            ))}
          </div>
        )}
        <div>Dealer: {gameState.player_names?.[gameState.dealer] || "Unknown"}</div>
        {gameState.trump !== -1 && (
          <div style={{fontWeight: 'bold', color: '#f1c40f', borderLeft: '2px solid #555', paddingLeft: 15}}>
            Trump: {SUIT_ICONS[gameState.trump]} {["Diamonds", "Clubs", "Hearts", "Spades"][gameState.trump]}
          </div>
        )}
      </div>

      {message && <div className="error-message" style={{position: 'fixed', top: 80, left: '50%', transform: 'translateX(-50%)', background: '#e74c3c', padding: '10px 20px', borderRadius: 5, zIndex: 200, fontWeight: 'bold'}}>{message}</div>}

      <div className="game-layout">
        <div className="main-area">
          <div className="table-area">
            <div className="phase-indicator">{gameState.phase.replace('_', ' ').toUpperCase()}</div>
            
            {[0, 1, 2, 3].map(idx => {
              if (!gameState.active_players?.includes(idx) && idx !== mySeat) return null;
              return (
                <div key={idx} className={`player-position ${getRelativePos(idx)}`}>
                  {gameState.player_names?.[idx] || ""} {gameState.current_bidder === idx && gameState.phase === 'bidding' ? "💬" : ""}
                  {gameState.phase === 'meld_display' && gameState.meld_cards?.[idx] && (
                    <div className={`meld-on-table ${["east", "west"].includes(getRelativePos(idx)) ? "vertical" : ""}`}>
                      {gameState.meld_cards[idx].flat().map((c, i) => <Card key={i} card={c} dimmed />)}
                    </div>
                  )}
                </div>
              );
            })}
            
            <div className="trick-area">
              {gameState.current_trick.map((entry, i) => (
                <div key={i} className={`trick-card-wrapper ${getRelativePos(entry.player)}-card`}>
                  <Card card={entry.card} />
                </div>
              ))}
            </div>

            <div className="player-position south">
              <strong>{mySeat !== null ? (gameState.player_names?.[mySeat] || "") : ""} (You)</strong>
              {gameState.phase === 'meld_display' && mySeat !== null && gameState.meld_cards?.[mySeat] && (
                <div className="meld-on-table">
                  {gameState.meld_cards[mySeat].flat().map((c, i) => <Card key={i} card={c} dimmed />)}
                </div>
              )}
              <div className="hand">
                {mySeat !== null && gameState.hands?.[mySeat]?.map((c, i) => (
                  <Card 
                    key={i} 
                    card={c} 
                    onClick={
                      gameState.phase === 'meld_selection' ? () => toggleMeldCard(i) :
                      isMyTurnToPlay ? () => playCard(i) : undefined
                    }
                    selected={selectedMeldIndices.includes(i)}
                  />
                ))}
              </div>
            </div>
          </div>
        </div>

        <div className="side-panel">
          <div className="game-log">
            <strong>Game Log</strong>
            {gameState.log.map((entry, i) => (
              <div key={i} className="log-entry">{entry}</div>
            ))}
            <div ref={logEndRef} />
          </div>

          {gameState.phase === 'meld_selection' && (
            <div className="bidding-panel" style={{background: '#8e44ad'}}>
              <h3>Select Cards for Meld</h3>
              <p>Click cards in your hand that you want to meld.</p>
              <button onClick={confirmMeld} style={{width: '100%', padding: '15px', fontSize: '1.1em'}}>Confirm Meld</button>
            </div>
          )}

          {gameState.phase === 'meld_display' && (
            <div className="bidding-panel" style={{background: '#2980b9'}}>
              <h3>Melds Displayed</h3>
              <p>Review everyone's meld on the table.</p>
              <button onClick={startTricks} style={{width: '100%', padding: '15px', fontSize: '1.1em'}}>Start Tricks</button>
            </div>
          )}

          {gameState.current_trick.length === activePlayers.length && (
            <div className="bidding-panel" style={{background: '#27ae60'}}>
              <h3>Trick Complete</h3>
              <button onClick={evaluateTrick} style={{width: '100%', padding: '15px', fontSize: '1.1em'}}>Collect Trick</button>
            </div>
          )}

          {gameState.phase === 'bidding' && gameState.current_bidder === mySeat && (
            <div className="bidding-panel">
              <h3>Your Bid</h3>
              <p>Current High: {gameState.bid === (gameState.game_mode === "5-card" ? 4 : 19) ? "None" : gameState.bid}</p>
              <div className="bid-buttons">
                <button onClick={() => placeBid(0)} style={{background: '#e74c3c'}}>Pass</button>
                <button onClick={() => placeBid(gameState.bid === (gameState.game_mode === "5-card" ? 4 : 19) ? (gameState.game_mode === "5-card" ? 5 : 20) : gameState.bid + 1)}>
                  Bid {gameState.bid === (gameState.game_mode === "5-card" ? 4 : 19) ? (gameState.game_mode === "5-card" ? 5 : 20) : gameState.bid + 1}
                </button>
              </div>
              <div className="custom-bid">
                <input 
                  type="number" 
                  min={gameState.bid === (gameState.game_mode === "5-card" ? 4 : 19) ? (gameState.game_mode === "5-card" ? 5 : 20) : gameState.bid + 1} 
                  value={customBid} 
                  onChange={(e) => setCustomBid(parseInt(e.target.value))}
                />
                <button onClick={() => placeBid(customBid)}>Bid</button>
              </div>
            </div>
          )}

          {gameState.phase === 'trump_selection' && gameState.bid_winner === mySeat && (
            <div className="bidding-panel">
              <h3>Select Trump</h3>
              <div className="trump-buttons">
                {Object.entries(SUIT_ICONS).map(([suit, icon]) => (
                  <button key={suit} onClick={() => selectTrump(parseInt(suit))} style={{fontSize: '1.2em', padding: '10px'}}>
                    {icon}
                  </button>
                ))}
              </div>
            </div>
          )}

          {(gameState.phase === 'round_end' || gameState.phase === 'game_end') && (
             <button onClick={() => nextRound()} style={{width: '100%', padding: 20, fontSize: '1.2em', background: '#f39c12'}}>
               {gameState.phase === 'round_end' ? "Start Next Round" : "Start Next Game"}
             </button>
          )}

          {gameState.phase === 'match_end' && (
            <div className="modal-overlay" style={{position: 'fixed', top: 0, left: 0, right: 0, bottom: 0, background: 'rgba(0,0,0,0.9)', display: 'flex', justifyContent: 'center', alignItems: 'center', zIndex: 300}}>
               <div className="modal-content">
                  <h1>MATCH OVER!</h1>
                  {gameState.game_mode === 'standard' ? (
                    <>
                      <h2>{gameState.us_games > gameState.them_games ? "YOU WON THE MATCH!" : "THEM WON THE MATCH!"}</h2>
                      <p>Final Match Score: {gameState.us_games} - {gameState.them_games}</p>
                    </>
                  ) : (
                    <>
                      <h2>Game Over!</h2>
                      <p>Final Scores:</p>
                      {gameState.active_players.map(p => (
                        <p key={p}>{gameState.player_names[p]}: {gameState.player_totals[p]}</p>
                      ))}
                    </>
                  )}
                  <button onClick={startNewGame} style={{padding: '20px 40px', fontSize: '1.5em'}}>Play Again</button>
               </div>
            </div>
          )}

          <div style={{marginTop: 'auto', paddingTop: 20, display: 'flex', flexDirection: 'column', gap: 10}}>
            <button onClick={leaveSeat} style={{background: '#7f8c8d', width: '100%'}}>Leave Seat / Lobby</button>
            <button onClick={resetServer} style={{background: '#c0392b', width: '100%', fontSize: '0.8em'}}>Reset Server (Clear All)</button>
          </div>
        </div>
      </div>
    </div>
  );
}

export default App;
