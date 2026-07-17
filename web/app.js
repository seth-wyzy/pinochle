const $ = (s) => document.querySelector(s);

let roomCode = '';
let seatToken = '';
let timer;
let selectedMeld = new Set();

const names = ['north', 'east', 'south', 'west'];

const label = (rank) => {
    const labels = {
        11: 'J',
        12: 'Q',
        13: 'K',
        14: '10',
        15: 'A',
    };
    return labels[rank] || rank;
};

const suitSymbol = (suit) => {
    const symbols = ['♣', '♦', '♥', '♠'];
    return symbols[suit] || '';
};

const suitOrder = (suit) => {
    return [0, 1, 3, 2].indexOf(suit);
};

const legalCards = (hand, trick, trump) => {
    if (!trick || trick.length === 0) {
        return hand;
    }
    const leadSuit = trick[0].card.suit;
    const follow = hand.filter(c => c.suit === leadSuit);
    
    let bestPlay = trick[0];
    for (let i = 1; i < trick.length; i++) {
        const play = trick[i];
        const card = play.card;
        const best = bestPlay.card;
        if (
            (card.suit === best.suit && card.rank > best.rank) ||
            (card.suit === trump && best.suit !== trump)
        ) {
            bestPlay = play;
        }
    }
    const bestCard = bestPlay.card;
    
    if (follow.length > 0) {
        if (bestCard.suit === leadSuit) {
            const beaters = follow.filter(c => c.rank > bestCard.rank);
            if (beaters.length > 0) {
                return beaters;
            }
        }
        return follow;
    }
    
    const trumpCards = hand.filter(c => c.suit === trump);
    if (trumpCards.length > 0) {
        if (bestCard.suit === trump) {
            const beaters = trumpCards.filter(c => c.rank > bestCard.rank);
            if (beaters.length > 0) {
                return beaters;
            }
        }
        return trumpCards;
    }
    
    return hand;
};

async function request(url, options) {
    const r = await fetch(url, {
        ...options,
        headers: {
            'content-type': 'application/json',
        },
    });
    const b = await r.json();
    if (!r.ok) {
        const error = new Error(b.error || 'Request failed');
        error.status = r.status;
        throw error;
    }
    return b;
}

function enter(room, token) {
    roomCode = room.code;
    seatToken = token;
    localStorage.setItem(`pinochle:${roomCode}`, seatToken);
    $('#room-code').textContent = roomCode;
    $('#lobby').classList.add('hidden');
    $('#game').classList.remove('hidden');
    render(room);
    clearInterval(timer);
    timer = setInterval(refresh, 900);
}

function render(room) {
    if (room.phase !== 'meld') {
        selectedMeld.clear();
    }
    
    const mine = room.players.find(p => p.seat === room.mySeat) ||
        room.players.find(p => p.seat === 2);
    const displaySeat = (seat) => {
        return (seat - (mine?.seat ?? 2) + 6) % 4;
    };
    const myTurn = mine && room.turn === mine.seat;
    
    const turnControls = $('#turn-controls');
    turnControls.dataset.phase = room.phase;
    turnControls.dataset.waiting = String(!myTurn && room.phase !== 'meld-result');
    
    room.players.forEach(p => {
        const seat = names[displaySeat(p.seat)];
        const target = $(`#${seat}-name`);
        if (target) {
            target.textContent = p.name;
        }
        const meld = $(`#${seat}-meld`);
        if (meld) {
            meld.textContent = `meld ${p.meld}`;
        }
    });
    
    $('#hand').className = 'hand seat-2';
    $('#bid').textContent = room.bid;
    $('#phase').textContent = room.phase[0].toUpperCase() + room.phase.slice(1);
    $('#message').textContent = room.message;
    $('#mobile-message').textContent = room.message;
    $('#us-score').textContent = room.scores.us;
    $('#them-score').textContent = room.scores.them;
    
    const suitsList = ['♣ Clubs', '♦ Diamonds', '♥ Hearts', '♠ Spades'];
    $('#trump').textContent = room.trump === null || room.trump === undefined
        ? 'Not chosen'
        : suitsList[room.trump];
        
    const dealerPlayer = room.players.find(p => p.seat === room.dealer);
    $('#dealer').textContent = dealerPlayer?.name || '—';
    
    if (room.bidWinner === null || room.bidWinner === undefined) {
        $('#bid-winner').textContent = '—';
    } else {
        const bidWinnerPlayer = room.players.find(p => p.seat === room.bidWinner);
        const winnerName = bidWinnerPlayer?.name || '—';
        $('#bid-winner').textContent = `${winnerName} (${room.bid})`;
    }
    
    const teams = [
        [0, 2, 'Us — North + South'],
        [1, 3, 'Them — East + West'],
    ];
    const teamElements = teams.map(([first, second, title]) => {
        const members = room.players.filter(
            p => p.seat === first || p.seat === second
        );
        const item = document.createElement('li');
        const total = members.reduce((sum, p) => sum + p.meld, 0);
        const cards = members
            .map(p => {
                const cardStr = (p.meldCards || [])
                    .map(c => `${label(c.rank)}${suitSymbol(c.suit)}`)
                    .join(' ');
                return `${p.name}: ${cardStr || '—'}`;
            })
            .join(' · ');
        item.textContent = `${title}: ${total} (${cards})`;
        return item;
    });
    $('#meld-list').replaceChildren(...teamElements);
    
    const activityItems = (room.history || [])
        .slice()
        .reverse()
        .map(text => {
            const item = document.createElement('li');
            item.textContent = text;
            return item;
        });
    $('#activity').replaceChildren(...activityItems);
    
    const sortedHand = (room.hand || [])
        .slice()
        .sort((a, b) => {
            return suitOrder(a.suit) - suitOrder(b.suit) || a.rank - b.rank;
        });
        
    const hand = $('#hand');
    const twoRowHand = sortedHand.length > 6;
    hand.dataset.rows = twoRowHand ? 'two' : 'one';
    document.documentElement.style.setProperty(
        '--mobile-hand-height',
        twoRowHand ? '166px' : '108px'
    );
    
    const cardButtons = sortedHand.map(c => {
        const el = document.createElement('button');
        const redSuit = c.suit === 1 || c.suit === 2;
        el.className = `card ${redSuit ? 'red' : 'black'}${
            selectedMeld.has(c.id) ? ' selected' : ''
        }`;
        el.style.color = redSuit ? '#b53b45' : '#202a45';
        el.textContent = `${label(c.rank)}${suitSymbol(c.suit)}`;
        
        const selectingMeld = room.phase === 'meld' && myTurn;
        let isCardLegal = true;
        if (room.phase === 'playing' && myTurn && !selectingMeld) {
            const legal = legalCards(sortedHand, room.trick || [], room.trump);
            isCardLegal = legal.some(lc => lc.id === c.id);
        }
        el.disabled =
            room.phase === 'complete' ||
            (!selectingMeld && (room.phase !== 'playing' || !myTurn || !isCardLegal));
            
        el.onclick = () => {
            if (selectingMeld) {
                if (selectedMeld.has(c.id)) {
                    selectedMeld.delete(c.id);
                } else {
                    selectedMeld.add(c.id);
                }
                render(room);
            } else {
                act({
                    type: 'play',
                    cardId: c.id,
                });
            }
        };
        return el;
    });
    hand.replaceChildren(...cardButtons);
    
    room.players.forEach(p => {
        const reveal = $(`#${names[displaySeat(p.seat)]}-reveal`);
        const visible = room.phase === 'meld-result';
        const revealCards = (visible ? p.meldCards : []).map(c => {
            const el = document.createElement('div');
            const redSuit = c.suit === 1 || c.suit === 2;
            el.className = `mini-card ${redSuit ? 'red' : 'black'}`;
            el.textContent = `${label(c.rank)}${suitSymbol(c.suit)}`;
            return el;
        });
        reveal.replaceChildren(...revealCards);
        reveal.classList.toggle('hidden', !visible);
    });
    
    const visibleTrick = room.trick.length ? room.trick : (room.lastTrick || []);
    const playedCards = visibleTrick.map(x => {
        const el = document.createElement('div');
        const value = document.createElement('span');
        const owner = document.createElement('small');
        const redSuit = x.card.suit === 1 || x.card.suit === 2;
        el.className = `card played-card seat-${displaySeat(x.seat)} ${
            redSuit ? 'red' : 'black'
        }`;
        el.style.color = redSuit ? '#b53b45' : '#202a45';
        value.textContent = `${label(x.card.rank)}${suitSymbol(x.card.suit)}`;
        owner.textContent = x.player;
        el.append(value, owner);
        return el;
    });
    $('#played').replaceChildren(...playedCards);
    
    const bid = $('#bid-button');
    const bidInput = $('#bid-input');
    bidInput.min = room.bid + 1;
    if (Number(bidInput.value) <= room.bid) {
        bidInput.value = room.bid + 1;
    }
    
    bidInput.disabled = !myTurn || room.phase !== 'bidding';
    bid.disabled = !myTurn || !['bidding', 'trump'].includes(room.phase);
    
    let bidText = 'Playing';
    if (room.phase === 'bidding') {
        bidText = `Bid ${bidInput.value}`;
    } else if (room.phase === 'trump') {
        bidText = 'Choose trump';
    }
    bid.textContent = bidText;
    
    bidInput.oninput = () => {
        if (room.phase === 'bidding') {
            bid.textContent = `Bid ${bidInput.value}`;
        }
    };
    
    bid.onclick = () => {
        if (room.phase === 'trump') {
            act({
                type: 'trump',
                value: $('#trump-select').value,
            });
        } else {
            act({
                type: 'bid',
                value: Number(bidInput.value),
            });
        }
    };
    
    $('#pass-button').disabled = !myTurn || room.phase !== 'bidding';
    $('#pass-button').onclick = () => {
        act({
            type: 'pass',
        });
    };
    
    const meldButton = $('#meld-button');
    const showingMeld = room.phase === 'meld' && myTurn;
    const showingResult = room.phase === 'meld-result';
    meldButton.classList.toggle('hidden', !showingMeld && !showingResult);
    meldButton.disabled = !showingMeld && !showingResult;
    meldButton.textContent = showingResult ? 'Begin trick play' : 'Submit meld';
    
    meldButton.onclick = () => {
        if (showingResult) {
            return act({
                type: 'continue',
            });
        }
        selectedMeld = new Set(
            [...selectedMeld].filter(id => sortedHand.some(c => c.id === id))
        );
        act({
            type: 'meld',
            cardIds: [...selectedMeld],
        });
    };
}

function returnToLobby(message) {
    clearInterval(timer);
    selectedMeld.clear();
    roomCode = '';
    seatToken = '';
    $('#game').classList.add('hidden');
    $('#lobby').classList.remove('hidden');
    $('#room-hint').textContent = message;
}

async function refresh() {
    try {
        const res = await request(
            `/api/rooms/${roomCode}?seatToken=${encodeURIComponent(seatToken)}`
        );
        render(res.room);
    } catch (e) {
        if (e.status === 404) {
            return returnToLobby(
                'That room is no longer available. Create or join a new table.'
            );
        }
        $('#message').textContent = e.message;
    }
}

async function act(action) {
    try {
        const res = await request(`/api/rooms/${roomCode}/actions`, {
            method: 'POST',
            body: JSON.stringify({
                ...action,
                seatToken,
            }),
        });
        render(res.room);
    } catch (e) {
        $('#message').textContent = e.message;
    }
}

$('#create').onclick = async () => {
    try {
        const d = await request('/api/rooms', {
            method: 'POST',
            body: JSON.stringify({
                name: $('#name').value,
                players: 4,
            }),
        });
        enter(d.room, d.seatToken);
    } catch (e) {
        $('#room-hint').textContent = e.message;
    }
};

$('#join').onclick = async () => {
    try {
        const code = $('#room-input').value.trim().toUpperCase();
        const saved = localStorage.getItem(`pinochle:${code}`);
        if (saved) {
            const d = await request(
                `/api/rooms/${code}?seatToken=${encodeURIComponent(saved)}`
            );
            return enter(d.room, saved);
        }
        const d = await request(`/api/rooms/${code}/join`, {
            method: 'POST',
            body: JSON.stringify({
                name: $('#name').value,
            }),
        });
        enter(d.room, d.seatToken);
    } catch (e) {
        $('#room-hint').textContent = e.message;
    }
};

$('#leave').onclick = () => {
    returnToLobby('');
};
