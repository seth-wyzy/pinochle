#include "../src/card.h"
#include <iostream>
#include <vector>
#include <cassert>
#include <algorithm>

// Simple assertion helper
void assert_equal(const card& actual, const card& expected, const std::string& testName) {
    if (!(actual == expected)) {
        std::cerr << "Assertion failed in " << testName << std::endl;
        std::cerr << "Expected: Rank " << expected.rank << ", Suit " << expected.suit << std::endl;
        std::cerr << "Actual:   Rank " << actual.rank << ", Suit " << actual.suit << std::endl;
        std::exit(1);
    }
}

void assert_hands_equal(const std::vector<card>& actual, const std::vector<card>& expected, const std::string& testName) {
    if (actual.size() != expected.size()) {
        std::cerr << "Assertion failed in " << testName << ": size mismatch." << std::endl;
        std::cerr << "Expected size: " << expected.size() << ", Actual size: " << actual.size() << std::endl;
        std::exit(1);
    }
    for (size_t i = 0; i < expected.size(); ++i) {
        if (!(actual[i] == expected[i])) {
            std::cerr << "Assertion failed in " << testName << " at index " << i << std::endl;
            std::cerr << "Expected: Rank " << expected[i].rank << ", Suit " << expected[i].suit << std::endl;
            std::cerr << "Actual:   Rank " << actual[i].rank << ", Suit " << actual[i].suit << std::endl;
            std::exit(1);
        }
    }
}

void test_getWinningCard() {
    std::cout << "Running test_getWinningCard..." << std::endl;

    int trump = 3; // Spades

    // Scenario 1: Empty trick
    std::vector<card> trick1 = {};
    assert_equal(getWinningCard(trick1, trump), card(), "test_getWinningCard - Empty");

    // Scenario 2: Single card
    card c1(11, 2, 0); // J of Hearts
    std::vector<card> trick2 = {c1};
    assert_equal(getWinningCard(trick2, trump), c1, "test_getWinningCard - Single Card");

    // Scenario 3: Higher rank follows lead suit
    card c2(13, 2, 0); // K of Hearts
    std::vector<card> trick3 = {c1, c2};
    assert_equal(getWinningCard(trick3, trump), c2, "test_getWinningCard - Higher Follow");

    // Scenario 4: Lower rank follows lead suit
    card c3(9, 2, 0); // 9 of Hearts
    std::vector<card> trick4 = {c1, c3};
    assert_equal(getWinningCard(trick4, trump), c1, "test_getWinningCard - Lower Follow");

    // Scenario 5: Playing trump
    card c4(9, 3, 0); // 9 of Spades (trump)
    std::vector<card> trick5 = {c1, c4};
    assert_equal(getWinningCard(trick5, trump), c4, "test_getWinningCard - Trump Played");

    // Scenario 6: Higher trump beats lower trump
    card c5(12, 3, 0); // Q of Spades (higher trump)
    std::vector<card> trick6 = {c1, c4, c5};
    assert_equal(getWinningCard(trick6, trump), c5, "test_getWinningCard - Higher Trump Played");

    // Scenario 7: Off-suit (non-trump) does not beat lead suit
    card c6(15, 0, 0); // A of Clubs (non-lead, non-trump)
    std::vector<card> trick7 = {c1, c6};
    assert_equal(getWinningCard(trick7, trump), c1, "test_getWinningCard - Off-suit Off-trump");
}

void test_getLegalCards_leading() {
    std::cout << "Running test_getLegalCards_leading..." << std::endl;
    int trump = 3; // Spades
    std::vector<card> hand = {
        card(9, 0, 0), card(11, 1, 0), card(12, 2, 0)
    };
    std::vector<card> trick = {};
    
    // When leading, any card is legal
    assert_hands_equal(getLegalCards(hand, trick, trump), hand, "test_getLegalCards_leading");
}

void test_getLegalCards_followSuit() {
    std::cout << "Running test_getLegalCards_followSuit..." << std::endl;
    int trump = 3; // Spades

    // Trick lead: Jack of Hearts (suit 2)
    std::vector<card> trick = { card(11, 2, 0) };

    // Hand: 9 of Hearts (suit 2), King of Hearts (suit 2), Ace of Clubs (suit 0)
    std::vector<card> hand = {
        card(9, 2, 0), card(13, 2, 0), card(15, 0, 0)
    };

    // Case A: Must beat current highest card of lead suit in trick (which is 11, 2).
    // The King of Hearts (13, 2) can beat it. Since we can follow suit and beat it, we must.
    std::vector<card> expectedA = { card(13, 2, 0) };
    assert_hands_equal(getLegalCards(hand, trick, trump), expectedA, "test_getLegalCards_followSuit - Must Beat");

    // Case B: No card in hand can beat current highest card of lead suit.
    // Trick lead: Ace of Hearts (15, 2).
    std::vector<card> trickB = { card(15, 2, 0) };
    // Hand has Hearts (9, 2 and 13, 2), but neither beats 15.
    // We must still follow suit, so both Hearts cards are legal.
    std::vector<card> expectedB = { card(9, 2, 0), card(13, 2, 0) };
    assert_hands_equal(getLegalCards(hand, trickB, trump), expectedB, "test_getLegalCards_followSuit - Cannot Beat But Must Follow");
}

void test_getLegalCards_trumpRequirements() {
    std::cout << "Running test_getLegalCards_trumpRequirements..." << std::endl;
    int trump = 3; // Spades

    // Trick lead: Jack of Hearts (suit 2)
    std::vector<card> trick = { card(11, 2, 0) };

    // Hand: 9 of Spades (suit 3 - trump), King of Spades (suit 3 - trump), Ace of Clubs (suit 0)
    // Hand has no Hearts, so cannot follow suit.
    std::vector<card> hand = {
        card(9, 3, 0), card(13, 3, 0), card(15, 0, 0)
    };

    // Case A: Best card on trick is lead suit. Hand has trump. Must play trump.
    // Since we have trumps, we must play them. Any trump is legal since the lead card is not trump.
    std::vector<card> expectedA = { card(9, 3, 0), card(13, 3, 0) };
    assert_hands_equal(getLegalCards(hand, trick, trump), expectedA, "test_getLegalCards_trumpRequirements - Must Trump");

    // Case B: Best card on trick is already a trump (e.g. Queen of Spades, 12, 3).
    // Trick: Hearts lead, trumped with Q of Spades.
    std::vector<card> trickB = { card(11, 2, 0), card(12, 3, 0) };
    // Hand has trumps: 9 of Spades (9, 3) and King of Spades (13, 3).
    // We must beat the winning trump (12, 3) if possible. King of Spades (13, 3) beats it.
    std::vector<card> expectedB = { card(13, 3, 0) };
    assert_hands_equal(getLegalCards(hand, trickB, trump), expectedB, "test_getLegalCards_trumpRequirements - Must Overtrump");

    // Case C: Best card on trick is already a higher trump than any we have.
    // Trick: Hearts lead, trumped with Ace of Spades (15, 3).
    std::vector<card> trickC = { card(11, 2, 0), card(15, 3, 0) };
    // Hand trumps: 9, 3 and 13, 3. Neither can beat 15.
    // We must still play trump because we have no cards of the led suit, and we have trump.
    std::vector<card> expectedC = { card(9, 3, 0), card(13, 3, 0) };
    assert_hands_equal(getLegalCards(hand, trickC, trump), expectedC, "test_getLegalCards_trumpRequirements - Cannot Overtrump But Must Trump");
}

void test_getLegalCards_discard() {
    std::cout << "Running test_getLegalCards_discard..." << std::endl;
    int trump = 3; // Spades

    // Trick lead: Jack of Hearts (suit 2)
    std::vector<card> trick = { card(11, 2, 0) };

    // Hand: 9 of Clubs (0), King of Diamonds (1)
    // No Hearts (cannot follow suit), no Spades (no trump).
    std::vector<card> hand = {
        card(9, 0, 0), card(13, 1, 0)
    };

    // Any card in hand is legal (discard)
    assert_hands_equal(getLegalCards(hand, trick, trump), hand, "test_getLegalCards_discard");
}

int main() {
    test_getWinningCard();
    test_getLegalCards_leading();
    test_getLegalCards_followSuit();
    test_getLegalCards_trumpRequirements();
    test_getLegalCards_discard();

    std::cout << "All card and trick rules tests passed!" << std::endl;
    return 0;
}
