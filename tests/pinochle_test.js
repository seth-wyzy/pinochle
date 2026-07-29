const assert = require('node:assert');
const {
    countMeld,
    legalCards,
    trickWinner,
    makeCard
} = require('../server.js');

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

console.log('All JavaScript tests passed!');
