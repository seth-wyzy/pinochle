const assert = require('node:assert');
const {
    countMeld,
    legalCards,
    trickWinner,
    makeCard,
    modelObservation,
} = require('../server.js');
const { buildObservation } = require('../web/app.js');

function runTest(name, fn) {
    try {
        fn();
        console.log(`PASS: ${name}`);
    } catch (e) {
        console.error(`FAIL: ${name}`);
        console.error(e);
        process.exit(1);
    }
}

// Helper to easily construct card arrays for tests
const createHand = (cardSpecs) => {
    return cardSpecs.map(([rank, suit]) => ({
        id: `${rank}-${suit}`,
        rank,
        suit
    }));
};

runTest('countMeld - empty hand', () => {
    const hand = [];
    assert.strictEqual(countMeld(hand, 3), 0);
});

runTest('countMeld - trump run', () => {
    const trump = 3; // Spades
    // A run in trump: A (15), 10 (14), K (13), Q (12), J (11).
    // In server.js countMeld:
    // - hasRun adds 15
    // - Marriage loop checks all suits. Since trump has 13 and 12, it adds 4 points (s === trump ? 4 : 2).
    // Total should be 19 points.
    const hand = createHand([
        [15, 3], // A Spades
        [14, 3], // 10 Spades
        [13, 3], // K Spades
        [12, 3], // Q Spades
        [11, 3]  // J Spades
    ]);
    assert.strictEqual(countMeld(hand, trump), 19);
});

runTest('countMeld - marriages', () => {
    // Trump marriage: 4 points
    const trumpHand = createHand([
        [13, 2], // K Hearts
        [12, 2]  // Q Hearts
    ]);
    assert.strictEqual(countMeld(trumpHand, 2), 4);

    // Non-trump marriage: 2 points
    const nonTrumpHand = createHand([
        [13, 1], // K Diamonds
        [12, 1]  // Q Diamonds
    ]);
    assert.strictEqual(countMeld(nonTrumpHand, 2), 2);

    // Double non-trump marriage: 2 * 2 = 4 points
    const doubleMarriageHand = createHand([
        [13, 1], [13, 1],
        [12, 1], [12, 1]
    ]);
    assert.strictEqual(countMeld(doubleMarriageHand, 2), 4);
});

runTest('countMeld - arounds', () => {
    // Aces around: 10 points
    const aces = createHand([
        [15, 0], [15, 1], [15, 2], [15, 3]
    ]);
    assert.strictEqual(countMeld(aces, 0), 10);

    // Kings around: 8 points
    const kings = createHand([
        [13, 0], [13, 1], [13, 2], [13, 3]
    ]);
    assert.strictEqual(countMeld(kings, 0), 8);

    // Queens around: 6 points
    const queens = createHand([
        [12, 0], [12, 1], [12, 2], [12, 3]
    ]);
    assert.strictEqual(countMeld(queens, 0), 6);

    // Jacks around: 4 points
    const jacks = createHand([
        [11, 0], [11, 1], [11, 2], [11, 3]
    ]);
    assert.strictEqual(countMeld(jacks, 0), 4);
});

runTest('countMeld - pinochle', () => {
    // Single Pinochle: Q Spades (12, 3) + J Diamonds (11, 1) -> 4 points
    const singleP = createHand([
        [12, 3], // Q Spades
        [11, 1]  // J Diamonds
    ]);
    assert.strictEqual(countMeld(singleP, 0), 4);

    // Double Pinochle: 2x Q Spades + 2x J Diamonds -> 30 points
    const doubleP = createHand([
        [12, 3], [12, 3],
        [11, 1], [11, 1]
    ]);
    assert.strictEqual(countMeld(doubleP, 0), 30);
});

runTest('countMeld - nine of trump', () => {
    // 9 of trump -> 1 point each
    const nines = createHand([
        [9, 3],
        [9, 3]
    ]);
    assert.strictEqual(countMeld(nines, 3), 2);
});

runTest('trickWinner', () => {
    const trump = 3; // Spades

    // Case 1: Highest following lead suit wins
    const trick1 = [
        { player: 'A', seat: 0, card: { rank: 11, suit: 2 } }, // J Hearts (lead)
        { player: 'B', seat: 1, card: { rank: 13, suit: 2 } }  // K Hearts
    ];
    assert.deepStrictEqual(trickWinner(trick1, trump), trick1[1]);

    // Case 2: Lower follow suit does not win
    const trick2 = [
        { player: 'A', seat: 0, card: { rank: 11, suit: 2 } }, // J Hearts (lead)
        { player: 'B', seat: 1, card: { rank: 9, suit: 2 } }   // 9 Hearts
    ];
    assert.deepStrictEqual(trickWinner(trick2, trump), trick2[0]);

    // Case 3: Trump beats lead suit
    const trick3 = [
        { player: 'A', seat: 0, card: { rank: 11, suit: 2 } }, // J Hearts (lead)
        { player: 'B', seat: 1, card: { rank: 9, suit: 3 } }   // 9 Spades (trump)
    ];
    assert.deepStrictEqual(trickWinner(trick3, trump), trick3[1]);

    // Case 4: Higher trump beats lower trump
    const trick4 = [
        { player: 'A', seat: 0, card: { rank: 11, suit: 2 } }, // J Hearts (lead)
        { player: 'B', seat: 1, card: { rank: 9, suit: 3 } },  // 9 Spades (trump)
        { player: 'C', seat: 2, card: { rank: 12, suit: 3 } }  // Q Spades (higher trump)
    ];
    assert.deepStrictEqual(trickWinner(trick4, trump), trick4[2]);
});

runTest('legalCards - leading', () => {
    const hand = createHand([[9, 0], [11, 1], [12, 2]]);
    const trick = [];
    // Leading: any card is legal
    assert.deepStrictEqual(legalCards(hand, trick, 3), hand);
});

runTest('legalCards - follow suit requirements', () => {
    const trump = 3; // Spades
    const trick = [
        { player: 'A', seat: 0, card: { rank: 11, suit: 2 } } // J Hearts (lead)
    ];
    const hand = createHand([
        [9, 2],  // 9 Hearts
        [13, 2], // K Hearts
        [15, 0]  // A Clubs
    ]);

    // Case A: Must beat current highest card (11, 2) if possible
    // Only K Hearts (13, 2) is legal because it can follow suit and beat J Hearts.
    const expectedA = createHand([[13, 2]]);
    assert.deepStrictEqual(legalCards(hand, trick, trump), expectedA);

    // Case B: No card beats the winning card of lead suit
    const trickB = [
        { player: 'A', seat: 0, card: { rank: 15, suit: 2 } } // A Hearts (lead)
    ];
    // Hand has Hearts (9, 2 and 13, 2) but neither beats 15.
    // Both Hearts cards are legal (must follow suit).
    const expectedB = createHand([[9, 2], [13, 2]]);
    assert.deepStrictEqual(legalCards(hand, trickB, trump), expectedB);
});

runTest('legalCards - trump requirements', () => {
    const trump = 3; // Spades
    const trick = [
        { player: 'A', seat: 0, card: { rank: 11, suit: 2 } } // J Hearts (lead)
    ];
    const hand = createHand([
        [9, 3],  // 9 Spades (trump)
        [13, 3], // K Spades (trump)
        [15, 0]  // A Clubs
    ]);

    // Case A: No lead suit, but has trump. Must play trump.
    // Any trump is legal since the trick hasn't been trumped yet.
    const expectedA = createHand([[9, 3], [13, 3]]);
    assert.deepStrictEqual(legalCards(hand, trick, trump), expectedA);

    // Case B: Trick already trumped (with Q Spades, 12, 3).
    // Must beat winning trump if possible (K Spades, 13, 3).
    const trickB = [
        { player: 'A', seat: 0, card: { rank: 11, suit: 2 } }, // J Hearts
        { player: 'B', seat: 1, card: { rank: 12, suit: 3 } }  // Q Spades
    ];
    const expectedB = createHand([[13, 3]]);
    assert.deepStrictEqual(legalCards(hand, trickB, trump), expectedB);

    // Case C: Trick trumped with higher trump (A Spades, 15, 3) than any in hand.
    // Must still play trump (9, 3 and 13, 3 are legal).
    const trickC = [
        { player: 'A', seat: 0, card: { rank: 11, suit: 2 } }, // J Hearts
        { player: 'B', seat: 1, card: { rank: 15, suit: 3 } }  // A Spades
    ];
    const expectedC = createHand([[9, 3], [13, 3]]);
    assert.deepStrictEqual(legalCards(hand, trickC, trump), expectedC);
});

runTest('legalCards - discard', () => {
    const trump = 3; // Spades
    const trick = [
        { player: 'A', seat: 0, card: { rank: 11, suit: 2 } } // J Hearts (lead)
    ];
    // Hand has no Hearts, no Spades (trump). Can play anything.
    const hand = createHand([
        [9, 0],  // 9 Clubs
        [13, 1]  // K Diamonds
    ]);
    assert.deepStrictEqual(legalCards(hand, trick, trump), hand);
});

runTest('modelObservation - observation length is 459', () => {
    const player = {
        seat: 0,
        hand: createHand([[9, 0], [15, 3]]),
    };
    const room = {
        dealer: 0,
        bid: 20,
        bidWinner: null,
        active: [true, true, true, true],
        phase: 'bidding',
        trump: null,
        trick: [],
        playedCards: [],
        turn: 0,
        trickPoints: { us: 0, them: 0 },
        players: [player, { seat: 1, hand: [] }, { seat: 2, hand: [] }, { seat: 3, hand: [] }],
    };

    const result = modelObservation(player, room);
    assert.strictEqual(result.length, 459);
    assert(result instanceof Float32Array);
    assert.strictEqual(result.observation.length, 459);
    assert.strictEqual(result.sortedHand.length, 2);

    const webResult = buildObservation(player, room);
    assert.strictEqual(webResult.length, 459);
    assert(webResult instanceof Float32Array);
});

runTest('modelObservation - hand encoding and sorting', () => {
    // [15, 3] = A Spades: suit 3, rank 15 (idx 5) -> cardType = 3 * 6 + 5 = 23
    // [9, 0] = 9 Clubs: suit 0, rank 9 (idx 0) -> cardType = 0 * 6 + 0 = 0
    // [12, 1] = Q Diamonds: suit 1, rank 12 (idx 2) -> cardType = 1 * 6 + 2 = 8
    const player = {
        seat: 2,
        hand: createHand([[15, 3], [9, 0], [12, 1]]),
    };
    const room = {};
    const obs = modelObservation(player, room);

    // Sorted order: [9, 0] (slot 0), [12, 1] (slot 1), [15, 3] (slot 2)
    // Slot 0 (0..23): cardType 0 is 1.0, others 0
    assert.strictEqual(obs[0 * 24 + 0], 1.0);
    for (let i = 1; i < 24; i++) {
        assert.strictEqual(obs[0 * 24 + i], 0.0);
    }

    // Slot 1 (24..47): cardType 8 is 1.0, others 0
    assert.strictEqual(obs[1 * 24 + 8], 1.0);
    for (let i = 0; i < 24; i++) {
        if (i !== 8) {
            assert.strictEqual(obs[1 * 24 + i], 0.0);
        }
    }

    // Slot 2 (48..71): cardType 23 is 1.0, others 0
    assert.strictEqual(obs[2 * 24 + 23], 1.0);
    for (let i = 0; i < 24; i++) {
        if (i !== 23) {
            assert.strictEqual(obs[2 * 24 + i], 0.0);
        }
    }

    // Remaining slots (3..11) should all be 0
    for (let slot = 3; slot < 12; slot++) {
        for (let i = 0; i < 24; i++) {
            assert.strictEqual(obs[slot * 24 + i], 0.0);
        }
    }
});

runTest('modelObservation - trick encoding and trick position', () => {
    // Current trick slots: 4 slots x 24 card types (indices 288..383)
    // Slot 0: J Clubs [11, 0] -> suit 0, rank 11 (idx 1) -> cardType 1
    // Slot 1: 10 Hearts [14, 2] -> suit 2, rank 14 (idx 4) -> cardType 2 * 6 + 4 = 16
    const player = { seat: 0, hand: [] };
    const room = {
        trick: [
            { player: 'A', seat: 0, card: { rank: 11, suit: 0 } },
            { player: 'B', seat: 1, card: { rank: 14, suit: 2 } },
        ],
    };
    const obs = modelObservation(player, room);

    // Slot 0 (288..311): cardType 1 is 1.0
    assert.strictEqual(obs[288 + 0 * 24 + 1], 1.0);
    for (let i = 0; i < 24; i++) {
        if (i !== 1) {
            assert.strictEqual(obs[288 + 0 * 24 + i], 0.0);
        }
    }

    // Slot 1 (312..335): cardType 16 is 1.0
    assert.strictEqual(obs[288 + 1 * 24 + 16], 1.0);
    for (let i = 0; i < 24; i++) {
        if (i !== 16) {
            assert.strictEqual(obs[288 + 1 * 24 + i], 0.0);
        }
    }

    // Slots 2 and 3 should be all 0s
    for (let slot = 2; slot < 4; slot++) {
        for (let i = 0; i < 24; i++) {
            assert.strictEqual(obs[288 + slot * 24 + i], 0.0);
        }
    }

    // Position in current trick (indices 447..450): trick length is 2 -> index 449 is 1.0
    assert.strictEqual(obs[447], 0.0);
    assert.strictEqual(obs[448], 0.0);
    assert.strictEqual(obs[449], 1.0);
    assert.strictEqual(obs[450], 0.0);
});

runTest('modelObservation - played history encoding (card counting)', () => {
    // Played cards history: 24 card types x 2 (indices 384..431)
    // Card type 3 (K Clubs, [13, 0]): played 1 time -> [384 + 3*2] = 1, [384 + 3*2 + 1] = 0
    // Card type 17 (A Hearts, [15, 2]): played 2 times -> [384 + 17*2] = 1, [384 + 17*2 + 1] = 1
    const player = { seat: 0, hand: [] };
    const room = {
        playedCards: [
            { rank: 13, suit: 0 },
            { rank: 15, suit: 2 },
            { rank: 15, suit: 2 },
        ],
    };
    const obs = modelObservation(player, room);

    // Card type 3
    assert.strictEqual(obs[384 + 3 * 2], 1.0);
    assert.strictEqual(obs[384 + 3 * 2 + 1], 0.0);

    // Card type 17
    assert.strictEqual(obs[384 + 17 * 2], 1.0);
    assert.strictEqual(obs[384 + 17 * 2 + 1], 1.0);

    // Card type 0 (unplayed)
    assert.strictEqual(obs[384 + 0 * 2], 0.0);
    assert.strictEqual(obs[384 + 0 * 2 + 1], 0.0);
});

runTest('modelObservation - trump encoding', () => {
    const player = { seat: 0, hand: [] };

    // Trump = 0 (Clubs) -> 432 is 1.0
    const obs0 = modelObservation(player, { trump: 0 });
    assert.strictEqual(obs0[432], 1.0);
    assert.strictEqual(obs0[433], 0.0);
    assert.strictEqual(obs0[434], 0.0);
    assert.strictEqual(obs0[435], 0.0);

    // Trump = 3 (Spades) -> 435 is 1.0
    const obs3 = modelObservation(player, { trump: 3 });
    assert.strictEqual(obs3[432], 0.0);
    assert.strictEqual(obs3[433], 0.0);
    assert.strictEqual(obs3[434], 0.0);
    assert.strictEqual(obs3[435], 1.0);

    // Trump = null -> all 0
    const obsNull = modelObservation(player, { trump: null });
    assert.strictEqual(obsNull[432], 0.0);
    assert.strictEqual(obsNull[433], 0.0);
    assert.strictEqual(obsNull[434], 0.0);
    assert.strictEqual(obsNull[435], 0.0);
});

runTest('modelObservation - game phase encoding', () => {
    const player = { seat: 0, hand: [] };

    // Bidding phase (0) -> 436 is 1.0
    const obsBidding = modelObservation(player, { phase: 'bidding' });
    assert.strictEqual(obsBidding[436], 1.0);
    assert.strictEqual(obsBidding[437], 0.0);
    assert.strictEqual(obsBidding[438], 0.0);

    // Trump phase (1) -> 437 is 1.0
    const obsTrump = modelObservation(player, { phase: 'trump' });
    assert.strictEqual(obsTrump[436], 0.0);
    assert.strictEqual(obsTrump[437], 1.0);
    assert.strictEqual(obsTrump[438], 0.0);

    // Playing phase (2) -> 438 is 1.0
    const obsPlaying = modelObservation(player, { phase: 'playing' });
    assert.strictEqual(obsPlaying[436], 0.0);
    assert.strictEqual(obsPlaying[437], 0.0);
    assert.strictEqual(obsPlaying[438], 1.0);
});

runTest('modelObservation - bid encoding and winner', () => {
    const player = { seat: 0, hand: [] };

    // Bid 25 -> 25 / 50.0 = 0.5
    const obsBid = modelObservation(player, { bid: 25, bidWinner: 0 });
    assert.strictEqual(obsBid[439], 0.5);
    // Us won bid (seat 0) -> 440 is 1.0
    assert.strictEqual(obsBid[440], 1.0);
    assert.strictEqual(obsBid[441], 0.0);
    assert.strictEqual(obsBid[442], 0.0);
    // Did anyone bid yet -> 456 is 1.0
    assert.strictEqual(obsBid[456], 1.0);

    // Them won bid (seat 1) -> 441 is 1.0
    const obsThem = modelObservation(player, { bid: 30, bidWinner: 1 });
    assert.strictEqual(obsThem[440], 0.0);
    assert.strictEqual(obsThem[441], 1.0);
    assert.strictEqual(obsThem[442], 0.0);
    assert.strictEqual(obsThem[456], 1.0);

    // Us won bid (seat 2) -> 440 is 1.0
    const obsUs2 = modelObservation(player, { bid: 22, bidWinner: 2 });
    assert.strictEqual(obsUs2[440], 1.0);
    assert.strictEqual(obsUs2[441], 0.0);
    assert.strictEqual(obsUs2[442], 0.0);

    // Them won bid (seat 3) -> 441 is 1.0
    const obsThem3 = modelObservation(player, { bid: 24, bidWinner: 3 });
    assert.strictEqual(obsThem3[440], 0.0);
    assert.strictEqual(obsThem3[441], 1.0);
    assert.strictEqual(obsThem3[442], 0.0);

    // No bid winner (null) -> 442 is 1.0, 456 is 0.0
    const obsNone = modelObservation(player, { bid: 20, bidWinner: null });
    assert.strictEqual(obsNone[440], 0.0);
    assert.strictEqual(obsNone[441], 0.0);
    assert.strictEqual(obsNone[442], 1.0);
    assert.strictEqual(obsNone[456], 0.0);
});

runTest('modelObservation - relative seat and turn', () => {
    // Relative seat: (seat - dealer + 4) % 4
    // dealer = 1, player seat = 2 -> (2 - 1 + 4) % 4 = 1 -> 443 + 1 = 444 is 1.0
    const obsSeat = modelObservation({ seat: 2, hand: [] }, { dealer: 1, turn: 2 });
    assert.strictEqual(obsSeat[443], 0.0);
    assert.strictEqual(obsSeat[444], 1.0);
    assert.strictEqual(obsSeat[445], 0.0);
    assert.strictEqual(obsSeat[446], 0.0);
    // Turn is 2 and player seat is 2 -> is my turn = 1.0
    assert.strictEqual(obsSeat[451], 1.0);

    // Turn is 1 and player seat is 2 -> is my turn = 0.0
    const obsNotTurn = modelObservation({ seat: 2, hand: [] }, { dealer: 1, turn: 1 });
    assert.strictEqual(obsNotTurn[451], 0.0);
});

runTest('modelObservation - meld and trick points', () => {
    const player = { seat: 0, hand: [] };
    const room = {
        players: [
            { seat: 0, meld: 15 },
            { seat: 1, meld: 10 },
            { seat: 2, meld: 5 },
            { seat: 3, meld: 5 },
        ],
        trickPoints: {
            us: 10,
            them: 15,
        },
    };
    const obs = modelObservation(player, room);
    // Us meld = 15 + 5 = 20 -> 20 / 50.0 = 0.4 (index 452)
    assert.strictEqual(Math.round(obs[452] * 100) / 100, 0.4);
    // Them meld = 10 + 5 = 15 -> 15 / 50.0 = 0.3 (index 453)
    assert.strictEqual(Math.round(obs[453] * 100) / 100, 0.3);
    // Us trick points = 10 / 25.0 = 0.4 (index 454)
    assert.strictEqual(Math.round(obs[454] * 100) / 100, 0.4);
    // Them trick points = 15 / 25.0 = 0.6 (index 455)
    assert.strictEqual(Math.round(obs[455] * 100) / 100, 0.6);
});

runTest('modelObservation - active bidders', () => {
    const player = { seat: 0, hand: [] };
    // Us: 0 (true) + 2 (true) = 2/2 = 1.0 (index 457)
    // Them: 1 (false) + 3 (true) = 1/2 = 0.5 (index 458)
    const obs = modelObservation(player, { active: [true, false, true, true] });
    assert.strictEqual(obs[457], 1.0);
    assert.strictEqual(obs[458], 0.5);

    // All inactive
    const obsAllFalse = modelObservation(player, { active: [false, false, false, false] });
    assert.strictEqual(obsAllFalse[457], 0.0);
    assert.strictEqual(obsAllFalse[458], 0.0);
});

runTest('buildObservation and modelObservation consistency', () => {
    const player = {
        seat: 2,
        hand: createHand([[9, 0], [13, 2], [15, 3]]),
    };
    const room = {
        dealer: 3,
        turn: 2,
        phase: 'playing',
        trump: 2,
        bid: 28,
        bidWinner: 2,
        active: [true, false, true, true],
        trick: [
            { player: 'A', seat: 3, card: { rank: 11, suit: 2 } },
        ],
        playedCards: [
            { rank: 9, suit: 1 },
            { rank: 9, suit: 1 },
            { rank: 14, suit: 0 },
        ],
        players: [
            { seat: 0, meld: 4 },
            { seat: 1, meld: 2 },
            { seat: 2, meld: 14, hand: player.hand },
            { seat: 3, meld: 0 },
        ],
        trickPoints: { us: 3, them: 2 },
    };

    const serverObs = modelObservation(player, room);
    const webObs = buildObservation(player, room);

    assert.strictEqual(serverObs.length, 459);
    assert.strictEqual(webObs.length, 459);

    for (let i = 0; i < 459; i++) {
        assert.strictEqual(
            webObs[i],
            serverObs[i],
            `Mismatch at observation index ${i}: web=${webObs[i]}, server=${serverObs[i]}`,
        );
    }
});

console.log('All JavaScript tests passed!');
