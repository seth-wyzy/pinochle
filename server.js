const http = require('node:http');
const fs = require('node:fs');
const path = require('node:path');
const crypto = require('node:crypto');
const { Pool } = require('pg');

let onnxruntime = null;
try {
    onnxruntime = require('onnxruntime-node');
} catch (error) {
    // Keep the web game usable with the existing rule-based AI until the
    // optional native ONNX runtime is installed.
}

const PORT = Number(process.env.PORT || 8787);
const DATABASE_URL = process.env.DATABASE_URL;
if (!DATABASE_URL && require.main === module) {
    throw new Error('DATABASE_URL is required. Set it to a PostgreSQL connection string.');
}
const db = DATABASE_URL ? new Pool({
    connectionString: DATABASE_URL,
    ssl: process.env.NODE_ENV === 'production' ? { rejectUnauthorized: false } : undefined,
}) : null;
const RANKS = [9, 11, 12, 13, 14, 15];
const SUITS = [0, 1, 2, 3];
const SUIT_SYMBOLS = ['♣', '♦', '♥', '♠'];
const ROOM_TTL_DAYS = 30;
const ONNX_MODEL_PATH = path.join(__dirname, 'web', 'models', 'pinochle_policy.onnx');
let onnxSessionPromise;

const makeCard = (rank, suit) => ({
    id: crypto.randomUUID(),
    rank,
    suit,
});
const makeDeck = () => {
    return SUITS.flatMap(suit =>
        RANKS.flatMap(rank => [
            makeCard(rank, suit),
            makeCard(rank, suit),
        ])
    );
};
const cardPoints = (rank) => {
    return rank >= 13 ? 1 : 0;
};
const shuffle = (cards) => {
    for (let i = cards.length - 1; i > 0; i--) {
        const j = Math.floor(Math.random() * (i + 1));
        [cards[i], cards[j]] = [cards[j], cards[i]];
    }
    return cards;
};
const sortHand = (cards) => {
    const order = [0, 1, 3, 2];
    return cards.sort((a, b) => {
        return order.indexOf(a.suit) - order.indexOf(b.suit) || a.rank - b.rank;
    });
};
const tokenHash = (token) => {
    return crypto.createHash('sha256').update(token).digest('hex');
};
const newToken = () => {
    return crypto.randomBytes(32).toString('base64url');
};

function countMeld(hand, trump) {
    const count = (suit, rank) => {
        return hand.filter(c => c.suit === suit && c.rank === rank).length;
    };
    
    const hasRun = [15, 14, 13, 12, 11].every(rank => count(trump, rank));
    let total = hasRun ? 15 : 0;
    
    for (const suit of SUITS) {
        if (count(suit, 13) && count(suit, 12)) {
            const base = suit === trump ? 4 : 2;
            const doubleMeld = count(suit, 13) > 1 && count(suit, 12) > 1;
            total += base * (doubleMeld ? 2 : 1);
        }
    }
    
    const rankValues = [
        [15, 10],
        [13, 8],
        [12, 6],
        [11, 4],
    ];
    for (const [rank, value] of rankValues) {
        if (SUITS.every(suit => count(suit, rank))) {
            total += value;
        }
    }
    
    const pinochles = Math.min(count(3, 12), count(1, 11));
    let pinochleMeld = 0;
    if (pinochles === 2) {
        pinochleMeld = 30;
    } else if (pinochles === 1) {
        pinochleMeld = 4;
    }
    
    return total + pinochleMeld + count(trump, 9);
}

function scoringCards(hand, trump) {
    const selected = new Map();
    const add = (suit, rank, copies = 1) => {
        hand.filter(c => c.suit === suit && c.rank === rank)
            .slice(0, copies)
            .forEach(c => selected.set(c.id, c));
    };
    
    const count = (suit, rank) => {
        return hand.filter(c => c.suit === suit && c.rank === rank).length;
    };
    
    const hasRun = [15, 14, 13, 12, 11].every(rank => count(trump, rank));
    if (hasRun) {
        [15, 14, 13, 12, 11].forEach(rank => add(trump, rank));
    }
    
    for (const suit of SUITS) {
        const copies = Math.min(count(suit, 13), count(suit, 12));
        if (copies) {
            add(suit, 13, copies);
            add(suit, 12, copies);
        }
    }
    
    for (const rank of [15, 13, 12, 11]) {
        if (SUITS.every(suit => count(suit, rank))) {
            SUITS.forEach(suit => add(suit, rank));
        }
    }
    
    const pinochles = Math.min(count(3, 12), count(1, 11));
    if (pinochles) {
        add(3, 12, pinochles);
        add(1, 11, pinochles);
    }
    
    add(trump, 9, count(trump, 9));
    
    return sortHand([...selected.values()]);
}

function legalCards(hand, trick, trump) {
    if (trick.length === 0) {
        return hand;
    }
    const leadSuit = trick[0].card.suit;
    const follow = hand.filter(c => c.suit === leadSuit);
    
    const bestPlay = trickWinner(trick, trump);
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
}

function trickWinner(trick, trump) {
    let winner = trick[0];
    for (const play of trick.slice(1)) {
        const card = play.card;
        const best = winner.card;
        if (
            (card.suit === best.suit && card.rank > best.rank) ||
            (card.suit === trump && best.suit !== trump)
        ) {
            winner = play;
        }
    }
    return winner;
}

function aiMove(player, trick, trump) {
    const options = legalCards(player.hand, trick, trump);
    if (trick.length === 0) {
        return options.reduce((best, card) => {
            return card.rank > best.rank ? card : best;
        });
    }
    const best = trickWinner(trick, trump).card;
    const winningPlay = options.find(card => {
        return (card.suit === best.suit && card.rank > best.rank) ||
            (card.suit === trump && best.suit !== trump);
    });
    if (winningPlay) {
        return winningPlay;
    }
    return options.slice().sort((a, b) => a.rank - b.rank)[0];
}

const trainingCardIndex = (card) => {
    const rankIndex = RANKS.indexOf(card.rank);
    return card.suit * RANKS.length + rankIndex;
};

const trainingSortHand = (hand) => {
    return hand.slice().sort((a, b) => a.suit - b.suit || a.rank - b.rank);
};

function modelObservation(player, trick, trump, score) {
    const observation = new Float32Array(391);
    const sortedHand = trainingSortHand(player.hand);
    sortedHand.forEach((card, slot) => {
        observation[slot * 24 + trainingCardIndex(card)] = 1;
    });
    trick.forEach((play, slot) => {
        observation[288 + slot * 24 + trainingCardIndex(play.card)] = 1;
    });
    if (trump !== null && trump !== undefined) {
        observation[384 + trump] = 1;
    }
    observation[388] = 0;
    observation[389] = 1;
    const usTeam = player.seat % 2 === 0;
    observation[390] = usTeam
        ? (score.us >= score.them ? 1 : 0)
        : (score.them >= score.us ? 1 : 0);
    return { observation, sortedHand };
}

async function loadOnnxSession() {
    if (!onnxruntime) {
        return null;
    }
    if (!onnxSessionPromise) {
        onnxSessionPromise = onnxruntime.InferenceSession.create(ONNX_MODEL_PATH)
            .catch(error => {
                onnxSessionPromise = null;
                console.error(`Unable to load ONNX policy: ${error.message}`);
                return null;
            });
    }
    return onnxSessionPromise;
}

async function modelAiMove(room, player) {
    const session = await loadOnnxSession();
    if (!session) {
        return aiMove(player, room.trick, room.trump);
    }

    const { observation, sortedHand } = modelObservation(
        player,
        room.trick,
        room.trump,
        room.trickPoints,
    );
    const legal = legalCards(sortedHand, room.trick, room.trump);
    const legalSlots = legal.map(card => sortedHand.findIndex(item => item.id === card.id));
    const tensor = new onnxruntime.Tensor('float32', observation, [1, 391]);
    const output = await session.run({ observation: tensor });
    const logits = output.action_logits.data;
    const selectedSlot = legalSlots.reduce((best, slot) => {
        return logits[slot] > logits[best] ? slot : best;
    }, legalSlots[0]);
    return sortedHand[selectedSlot];
}

function addEvent(room, text) {
    room.history.push(text);
    if (room.history.length > 14) {
        room.history.shift();
    }
    room.events.push(text);
}

function due(room, type, milliseconds) {
    room.dueAction = {
        type,
        at: Date.now() + milliseconds,
    };
}

function finishBidding(room) {
    if (room.active.filter(Boolean).length > 1) {
        return;
    }
    if (room.bidWinner === null) {
        room.bidWinner = room.dealer;
        room.bid = 20;
        room.message = `Everyone passed. ${room.players[room.dealer].name} is stuck with 20.`;
        addEvent(room, room.message);
    }
    room.phase = 'trump';
    room.turn = room.bidWinner;
}

function startRound(room, advanceDealer = true) {
    if (advanceDealer) {
        room.dealer = (room.dealer + 1) % 4;
    }
    room.phase = 'bidding';
    room.bid = 20;
    room.bidWinner = null;
    room.active = [true, true, true, true];
    room.trump = null;
    room.turn = (room.dealer + 1) % 4;
    room.trick = [];
    room.lastTrick = [];
    room.tricks = 0;
    room.lastWinner = 0;
    room.trickPoints = {
        us: 0,
        them: 0,
    };
    
    const deck = shuffle(makeDeck());
    room.players.forEach(p => {
        p.hand = sortHand(deck.splice(0, 12));
        p.meld = 0;
        p.meldCards = [];
    });
    
    room.message = `${room.players[room.dealer].name} deals game ${room.gameNumber}, hand ${room.handsInGame + 1}.`;
    addEvent(room, room.message);
}

function startNewGame(room) {
    room.gameNumber++;
    room.handsInGame = 0;
    room.scores = {
        us: 0,
        them: 0,
    };
    room.dealer = (room.dealer + 2) % 4;
    startRound(room, false);
}

function finishGame(room) {
    let winner = 'them';
    if (room.scores.us === room.scores.them) {
        winner = 'tie';
    } else if (room.scores.us > room.scores.them) {
        winner = 'us';
    }
    
    room.gameWinners.push(winner);
    room.phase = 'game-result';
    
    let winText = 'Them win';
    if (winner === 'tie') {
        winText = 'tied';
    } else if (winner === 'us') {
        winText = 'Us win';
    }
    
    room.message = `Game ${room.gameNumber} complete: ${winText} (${room.scores.us}–${room.scores.them}).`;
    addEvent(room, room.message);
    
    if (room.gameNumber === 1 || (room.gameNumber === 2 && room.gameWinners[0] !== winner)) {
        if (room.gameNumber === 2) {
            room.message += ' Tiebreaker game starts next.';
            addEvent(
                room,
                'The first two games had different winners; starting a tiebreaker.'
            );
        }
        return due(room, 'new-game', 3500);
    }
    
    room.phase = 'match-complete';
    let matchText = 'Them win the match.';
    if (winner === 'tie') {
        matchText = 'Match remains tied.';
    } else if (winner === 'us') {
        matchText = 'Us win the match.';
    }
    room.message += ` ${matchText}`;
    addEvent(room, room.message);
}

function finishRound(room) {
    const trickWinnerKey = room.lastWinner % 2 ? 'them' : 'us';
    room.trickPoints[trickWinnerKey]++;
    
    const usMeld = room.players
        .filter(p => p.seat % 2 === 0)
        .reduce((n, p) => n + p.meld, 0);
    const themMeld = room.players
        .filter(p => p.seat % 2 !== 0)
        .reduce((n, p) => n + p.meld, 0);
        
    const usTotal = usMeld + room.trickPoints.us;
    const themTotal = themMeld + room.trickPoints.them;
    
    if (room.bidWinner % 2 === 0) {
        room.scores.us += usTotal >= room.bid ? usTotal : -room.bid;
        room.scores.them += themTotal;
    } else {
        room.scores.them += themTotal >= room.bid ? themTotal : -room.bid;
        room.scores.us += usTotal;
    }
    
    room.handsInGame++;
    room.phase = 'round-result';
    room.message = `Hand ${room.handsInGame}/4 complete. Us ${usTotal}, Them ${themTotal}.`;
    addEvent(room, room.message);
    
    const nextAction = room.handsInGame === 4 ? 'finish-game' : 'next-round';
    due(room, nextAction, 3000);
}

function showMeldResult(room) {
    if (!room.players.every(p => p.meld !== null)) {
        return;
    }
    if (room.bidWinner === null) {
        room.bidWinner = room.dealer;
        room.bid = 20;
    }
    room.phase = 'meld-result';
    room.turn = null;
    room.message = 'Meld revealed. Review the totals, then begin trick play.';
}

function chooseTrump(room, trump) {
    room.trump = trump;
    room.phase = 'meld';
    room.turn = 0;
    room.players.forEach(p => {
        p.meld = p.ai ? countMeld(p.hand, trump) : null;
        p.meldCards = p.ai ? scoringCards(p.hand, trump) : [];
    });
    const chooser = room.players[room.bidWinner ?? room.dealer];
    room.message = `${chooser.name} chooses ${SUIT_SYMBOLS[trump]} as trump.`;
    addEvent(room, room.message);
}

function finishTrick(room) {
    const winner = trickWinner(room.trick, room.trump);
    const teamKey = winner.seat % 2 ? 'them' : 'us';
    const points = room.trick.reduce((n, play) => {
        return n + cardPoints(play.card.rank);
    }, 0);
    room.trickPoints[teamKey] += points;
    room.lastWinner = winner.seat;
    room.lastTrick = room.trick;
    room.trick = [];
    room.tricks++;
    room.turn = winner.seat;
    room.phase = 'trick-result';
    room.message = `${winner.player} wins the trick.`;
    addEvent(room, room.message);
    
    const nextAction = room.tricks === 12 ? 'finish-round' : 'resume-playing';
    due(room, nextAction, 1600);
}

function advanceAi(room) {
    while (room.phase === 'bidding' && room.players[room.turn].ai) {
        const p = room.players[room.turn];
        const maximum = Math.max(...SUITS.map(s => countMeld(p.hand, s))) + 10;
        const bid = room.bid < maximum && room.bid < 30 ? room.bid + 1 : 0;
        if (bid) {
            room.bid = bid;
            room.bidWinner = p.seat;
            room.message = `${p.name} bids ${bid}.`;
        } else {
            room.active[p.seat] = false;
            room.message = `${p.name} passes.`;
        }
        addEvent(room, room.message);
        room.turn = (room.turn + 1) % 4;
        finishBidding(room);
        while (room.phase === 'bidding' && !room.active[room.turn]) {
            room.turn = (room.turn + 1) % 4;
        }
    }
    
    if (room.phase === 'trump' && room.players[room.turn].ai) {
        const hand = room.players[room.turn].hand;
        const bestSuit = SUITS.reduce((best, s) => {
            return countMeld(hand, s) > countMeld(hand, best) ? s : best;
        }, 0);
        chooseTrump(room, bestSuit);
    }
    
    while (room.phase === 'meld' && room.players[room.turn].ai) {
        room.turn = (room.turn + 1) % 4;
    }
    
    if (room.phase === 'meld') {
        showMeldResult(room);
    }
    
    if (room.phase === 'playing' && room.players[room.turn].ai && !room.dueAction) {
        due(room, 'ai-play', 900);
    }
}

async function applyDue(room) {
    if (!room.dueAction || room.dueAction.at > Date.now()) {
        return false;
    }
    const action = room.dueAction.type;
    room.dueAction = null;
    
    if (action === 'ai-play' && room.phase === 'playing' && room.players[room.turn].ai) {
        const p = room.players[room.turn];
        const card = await modelAiMove(room, p);
        p.hand.splice(p.hand.indexOf(card), 1);
        room.trick.push({
            player: p.name,
            seat: p.seat,
            card,
        });
        room.message = `${p.name} plays ${card.rank}${SUIT_SYMBOLS[card.suit]}.`;
        addEvent(room, room.message);
        room.turn = (room.turn + 1) % 4;
        if (room.trick.length === 4) {
            finishTrick(room);
        }
    } else if (action === 'resume-playing' && room.phase === 'trick-result') {
        room.phase = 'playing';
        room.lastTrick = [];
    } else if (action === 'finish-round' && room.phase === 'trick-result') {
        finishRound(room);
    } else if (action === 'next-round' && room.phase === 'round-result') {
        startRound(room);
    } else if (action === 'finish-game' && room.phase === 'round-result') {
        finishGame(room);
    } else if (action === 'new-game' && room.phase === 'game-result') {
        startNewGame(room);
    }
    
    advanceAi(room);
    return true;
}

function newRoom(name) {
    const aiNames = shuffle(['Bruce', 'Dick', 'Tim']);
    const players = [0, 1, 2, 3].map(seat => {
        const isUser = seat === 2;
        const defaultName = isUser ? (name || 'You') : `${aiNames.pop()} (AI)`;
        return {
            name: defaultName,
            ai: !isUser,
            seat,
            hand: [],
            meld: 0,
            meldCards: [],
        };
    });
    const room = {
        dealer: Math.floor(Math.random() * 4),
        gameNumber: 1,
        handsInGame: 0,
        gameWinners: [],
        phase: 'bidding',
        bid: 20,
        bidWinner: null,
        active: [true, true, true, true],
        turn: 0,
        trump: null,
        trick: [],
        lastTrick: [],
        tricks: 0,
        lastWinner: 0,
        trickPoints: {
            us: 0,
            them: 0,
        },
        scores: {
            us: 0,
            them: 0,
        },
        message: 'Your table is ready.',
        history: [],
        events: [],
        dueAction: null,
        players,
    };
    
    startRound(room, false);
    advanceAi(room);
    return room;
}

function view(room, seat) {
    const player = room.players.find(p => p.seat === seat);
    const revealPhases = ['meld-result', 'playing', 'trick-result', 'round-result'];
    const reveal = revealPhases.includes(room.phase);
    
    const mappedPlayers = room.players.map(p => {
        return {
            name: p.name,
            ai: p.ai,
            seat: p.seat,
            meld: reveal ? p.meld : 0,
            meldCards: reveal ? p.meldCards : [],
        };
    });
    
    return {
        ...room,
        mySeat: seat,
        events: undefined,
        dueAction: undefined,
        players: mappedPlayers,
        hand: player?.hand || [],
    };
}

async function migrate() {
    const sql = fs.readFileSync(
        path.join(__dirname, 'migrations', '001_rooms.sql'),
        'utf8'
    );
    await db.query(sql);
}

async function cleanExpired() {
    await db.query('DELETE FROM rooms WHERE expires_at < now()');
}

async function processDueRoom(code) {
    const client = await db.connect();
    try {
        await client.query('BEGIN');
        const found = await client.query(
            'SELECT snapshot FROM rooms WHERE code = $1 AND expires_at > now() FOR UPDATE',
            [code]
        );
        if (!found.rowCount) {
            await client.query('COMMIT');
            return;
        }
        const room = found.rows[0].snapshot;
        room.code = code;
        room.events = [];
        if (!await applyDue(room)) {
            await client.query('COMMIT');
            return;
        }
        if (room.events.length) {
            await client.query(
                'INSERT INTO room_events (room_code, text) SELECT $1, unnest($2::text[])',
                [code, room.events]
            );
        }
        await client.query(
            'UPDATE rooms SET snapshot = $2, version = version + 1, updated_at = now() WHERE code = $1',
            [code, JSON.stringify(room)]
        );
        await client.query('COMMIT');
    } catch (error) {
        await client.query('ROLLBACK');
        throw error;
    } finally {
        client.release();
    }
}

async function processOverdueRooms() {
    const queryText =
        'SELECT code FROM rooms WHERE expires_at > now() AND ' +
        "snapshot #>> '{dueAction,at}' IS NOT NULL AND " +
        '(snapshot #>> \'{dueAction,at}\')::bigint <= $1';
    const result = await db.query(queryText, [Date.now()]);
    for (const row of result.rows) {
        await processDueRoom(row.code);
    }
}

async function withRoom(code, token, mutate) {
    const client = await db.connect();
    try {
        await client.query('BEGIN');
        const found = await client.query(
            'SELECT snapshot FROM rooms WHERE code = $1 AND expires_at > now() FOR UPDATE',
            [code]
        );
        if (!found.rowCount) {
            const error = new Error('room not found');
            error.status = 404;
            throw error;
        }
        
        const seatResult = await client.query(
            'SELECT seat, ai FROM room_seats WHERE room_code = $1 AND token_hash = $2',
            [code, tokenHash(token || '')]
        );
        const seat = seatResult.rows[0];
        if (!seat) {
            const error = new Error('invalid room token');
            error.status = 403;
            throw error;
        }
        
        const room = found.rows[0].snapshot;
        room.code = code;
        room.events = [];
        await applyDue(room);
        
        const result = await mutate(room, seat);
        
        if (room.events.length) {
            await client.query(
                'INSERT INTO room_events (room_code, text) SELECT $1, unnest($2::text[])',
                [code, room.events]
            );
        }
        
        await client.query(
            "UPDATE rooms SET snapshot = $2, version = version + 1, updated_at = now(), " +
                "expires_at = now() + interval '30 days' WHERE code = $1",
            [code, JSON.stringify(room)]
        );
        await client.query('COMMIT');
        return {
            room,
            seat: seat.seat,
            result,
        };
    } catch (error) {
        await client.query('ROLLBACK');
        throw error;
    } finally {
        client.release();
    }
}

async function makeCode(client) {
    for (;;) {
        const code = crypto.randomBytes(4).toString('hex').toUpperCase();
        const check = await client.query('SELECT 1 FROM rooms WHERE code = $1', [code]);
        if (!check.rowCount) {
            return code;
        }
    }
}

async function createRoom(name) {
    const client = await db.connect();
    try {
        await client.query('BEGIN');
        const code = await makeCode(client);
        const token = newToken();
        const room = newRoom(name);
        room.code = code;
        
        await client.query(
            "INSERT INTO rooms (code, snapshot, expires_at) VALUES ($1, $2, now() + " +
                "interval '30 days')",
            [code, JSON.stringify(room)]
        );
        for (const p of room.players) {
            const hash = p.seat === 2 ? tokenHash(token) : null;
            await client.query(
                'INSERT INTO room_seats (room_code, seat, name, ai, token_hash) ' +
                    'VALUES ($1, $2, $3, $4, $5)',
                [code, p.seat, p.name, p.ai, hash]
            );
        }
        await client.query(
            'INSERT INTO room_events (room_code, text) SELECT $1, unnest($2::text[])',
            [code, room.events]
        );
        await client.query('COMMIT');
        return {
            code,
            token,
            room,
            seat: 2,
        };
    } catch (error) {
        await client.query('ROLLBACK');
        throw error;
    } finally {
        client.release();
    }
}

async function joinRoom(code, name) {
    const client = await db.connect();
    try {
        await client.query('BEGIN');
        const found = await client.query(
            'SELECT snapshot FROM rooms WHERE code = $1 AND expires_at > now() FOR UPDATE',
            [code]
        );
        if (!found.rowCount) {
            const error = new Error('room not found');
            error.status = 404;
            throw error;
        }
        
        const room = found.rows[0].snapshot;
        const open = room.players.find(p => p.ai);
        if (!open) {
            const error = new Error('table is full');
            error.status = 409;
            throw error;
        }
        
        const token = newToken();
        room.code = code;
        open.ai = false;
        open.name = name || 'Player';
        room.events = [];
        
        addEvent(room, `${open.name} joined the table.`);
        advanceAi(room);
        
        await client.query(
            "UPDATE rooms SET snapshot = $2, version = version + 1, updated_at = now(), " +
                "expires_at = now() + interval '30 days' WHERE code = $1",
            [code, JSON.stringify(room)]
        );
        await client.query(
            'UPDATE room_seats SET name = $3, ai = false, token_hash = $4 ' +
                'WHERE room_code = $1 AND seat = $2',
            [code, open.seat, open.name, tokenHash(token)]
        );
        await client.query(
            'INSERT INTO room_events (room_code, text) SELECT $1, unnest($2::text[])',
            [code, room.events]
        );
        await client.query('COMMIT');
        return {
            token,
            room,
            seat: open.seat,
        };
    } catch (error) {
        await client.query('ROLLBACK');
        throw error;
    } finally {
        client.release();
    }
}

function readBody(req) {
    return new Promise(resolve => {
        let text = '';
        req.on('data', c => {
            text += c;
        });
        req.on('end', () => {
            try {
                const body = JSON.parse(text || '{}');
                resolve({
                    ...body,
                    cardIds: Array.isArray(body.cardIds) ? body.cardIds : [],
                });
            } catch {
                resolve({
                    cardIds: [],
                });
            }
        });
    });
}

function send(res, status, body) {
    res.writeHead(status, {
        'content-type': 'application/json',
    });
    res.end(JSON.stringify(body));
}

function serveFile(req, res) {
    const url = new URL(req.url, 'http://localhost');
    const requested = url.pathname === '/' ? '/index.html' : url.pathname;
    const file = path.join(
        __dirname,
        'web',
        path.normalize(requested).replace(/^\/+/, '')
    );
    
    if (!file.startsWith(path.join(__dirname, 'web'))) {
        return send(res, 403, {
            error: 'forbidden',
        });
    }
    
    fs.readFile(file, (error, data) => {
        if (error) {
            return send(res, 404, {
                error: 'not found',
            });
        }
        
        let contentType = 'text/html';
        if (file.endsWith('.css')) {
            contentType = 'text/css';
        } else if (file.endsWith('.js')) {
            contentType = 'text/javascript';
        } else if (file.endsWith('.onnx') || file.endsWith('.onnx.data')) {
            contentType = 'application/octet-stream';
        }
        
        res.writeHead(200, {
            'content-type': contentType,
            'cache-control': 'no-store',
        });
        res.end(data);
    });
}

function action(room, seat, input) {
    const player = room.players[seat];
    if (player.ai || (room.phase !== 'meld-result' && seat !== room.turn)) {
        const error = new Error('not your turn');
        throw Object.assign(error, {
            status: 400,
        });
    }
    
    if (room.phase === 'bidding' && ['bid', 'pass'].includes(input.type)) {
        const bid = input.type === 'pass' ? 0 : Number(input.value);
        if (bid && bid <= room.bid) {
            const error = new Error('bid must exceed current bid');
            throw Object.assign(error, {
                status: 400,
            });
        }
        if (bid) {
            room.bid = bid;
            room.bidWinner = seat;
            room.message = `${player.name} bids ${bid}.`;
        } else {
            room.active[seat] = false;
            room.message = `${player.name} passes.`;
        }
        addEvent(room, room.message);
        room.turn = (room.turn + 1) % 4;
        finishBidding(room);
        while (room.phase === 'bidding' && !room.active[room.turn]) {
            room.turn = (room.turn + 1) % 4;
        }
    } else if (room.phase === 'trump' && input.type === 'trump') {
        chooseTrump(room, Math.max(0, Math.min(3, Number(input.value))));
    } else if (room.phase === 'meld' && input.type === 'meld') {
        const submitted = player.hand.filter(c => input.cardIds.includes(c.id));
        player.meld = countMeld(submitted, room.trump);
        player.meldCards = scoringCards(submitted, room.trump);
        room.message = `${player.name} exposed ${player.meld} meld points.`;
        addEvent(room, room.message);
        room.turn = (room.turn + 1) % 4;
    } else if (room.phase === 'meld-result' && input.type === 'continue') {
        room.phase = 'playing';
        room.turn = room.bidWinner;
        room.message = 'Meld counted. The bid winner leads.';
        addEvent(room, room.message);
    } else if (room.phase === 'playing' && input.type === 'play') {
        const card = player.hand.find(c => c.id === input.cardId);
        const legal = legalCards(player.hand, room.trick, room.trump);
        const isLegal = card && legal.some(c => c.id === card.id);
        if (!isLegal) {
            const error = new Error('card is not legal for this trick');
            throw Object.assign(error, {
                status: 400,
            });
        }
        player.hand.splice(player.hand.indexOf(card), 1);
        room.trick.push({
            player: player.name,
            seat,
            card,
        });
        room.message = `${player.name} plays ${card.rank}${SUIT_SYMBOLS[card.suit]}.`;
        addEvent(room, room.message);
        room.turn = (room.turn + 1) % 4;
        if (room.trick.length === 4) {
            finishTrick(room);
        }
    } else {
        const error = new Error('action is not valid in this phase');
        throw Object.assign(error, {
            status: 400,
        });
    }
    
    advanceAi(room);
}

const server = http.createServer(async (req, res) => {
    try {
        const url = new URL(req.url, `http://${req.headers.host}`);
        const parts = url.pathname.split('/').filter(Boolean);
        
        if (req.method === 'GET' && url.pathname === '/health') {
            await db.query('SELECT 1');
            return send(res, 200, {
                ok: true,
            });
        }
        
        if (req.method === 'POST' && url.pathname === '/api/rooms') {
            const input = await readBody(req);
            const created = await createRoom(input.name);
            return send(res, 201, {
                seatToken: created.token,
                room: view(created.room, created.seat),
            });
        }
        
        const isApiRooms = parts[0] === 'api' && parts[1] === 'rooms' && parts[2];
        if (!isApiRooms) {
            return serveFile(req, res);
        }
        
        const code = parts[2].toUpperCase();
        
        if (req.method === 'POST' && parts[3] === 'join') {
            const input = await readBody(req);
            const joined = await joinRoom(code, input.name);
            return send(res, 200, {
                seatToken: joined.token,
                room: view(joined.room, joined.seat),
            });
        }
        
        const input = req.method === 'POST' ? await readBody(req) : null;
        const token = req.method === 'GET' ? url.searchParams.get('seatToken') : input.seatToken;
        
        if (req.method === 'GET') {
            const loaded = await withRoom(code, token, async () => {});
            return send(res, 200, {
                room: view(loaded.room, loaded.seat),
            });
        }
        
        if (req.method === 'POST' && parts[3] === 'actions') {
            const updated = await withRoom(code, input.seatToken, async (room, seat) => {
                return action(room, seat.seat, input);
            });
            return send(res, 200, {
                room: view(updated.room, updated.seat),
            });
        }
        
        return send(res, 404, {
            error: 'not found',
        });
    } catch (error) {
        console.error(error);
        const status = error.status || 500;
        const message = error.status ? error.message : 'internal server error';
        return send(res, status, {
            error: message,
        });
    }
});

async function start() {
    await migrate();
    await cleanExpired();
    await processOverdueRooms();
    
    setInterval(() => {
        cleanExpired().catch(console.error);
    }, 6 * 60 * 60 * 1000).unref();
    
    server.listen(PORT, () => {
        console.log(`Pinochle web server listening on http://localhost:${PORT}`);
    });
}

if (require.main === module) {
    start().catch(error => {
        console.error('Unable to start Pinochle:', error);
        process.exit(1);
    });
} else {
    module.exports = {
        countMeld,
        scoringCards,
        legalCards,
        trickWinner,
        makeCard,
        makeDeck,
        cardPoints
    };
}
