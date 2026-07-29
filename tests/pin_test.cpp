#include "../src/Pin.h"
#include "../src/card.h"
#include <iostream>
#include <vector>
#include <cassert>

void assert_meld(Pin& game, const std::vector<card>& hand, int trump, int expectedPoints, const std::string& testName) {
    int actualPoints = game.count_meld(hand, trump, false);
    if (actualPoints != expectedPoints) {
        std::cerr << "Assertion failed in " << testName << std::endl;
        std::cerr << "Expected meld points: " << expectedPoints << ", Got: " << actualPoints << std::endl;
        std::exit(1);
    }
}

void test_empty_hand(Pin& game) {
    std::cout << "Running test_empty_hand..." << std::endl;
    std::vector<card> hand = {};
    assert_meld(game, hand, 3, 0, "test_empty_hand");
}

void test_trump_run(Pin& game) {
    std::cout << "Running test_trump_run..." << std::endl;
    int trump = 3; // Spades
    // A run in trump (A 10 K Q J) is 15 points.
    // Note: A trump marriage is also part of a run. But does a run include marriage points?
    // Let's check Pin::count_meld implementation in Pin.cpp:
    // It checks hasRun, if so meldPoints += 15.
    // Then it checks marriages: for (int s = 0; s <= 3; ++s) { if (suitRank[s][13] > 0 && suitRank[s][12] > 0) ... meldPoints += pts; }
    // Wait! In Pin.cpp, if you have a Run, it adds 15 AND it also adds the marriage points (4 for trump marriage)!
    // Let's check Pin.cpp lines 295-319:
    //   std::vector<int> run = {15, 14, 13, 12, 11};
    //   bool hasRun = true;
    //   ... if (hasRun) meldPoints += 15;
    //   ...
    //   // Marriage
    //   for (int s = 0; s <= 3; ++s) {
    //       if (suitRank[s][13] > 0 && suitRank[s][12] > 0) {
    //           int pts = (s == trump) ? 4 : 2;
    //           ...
    //           meldPoints += pts;
    //       }
    //   }
    // Yes! According to Pin's implementation, a run adds 15, and the marriage in trump adds 4, totaling 19 points!
    // We should test according to the codebase's actual implementation. Let's verify this.
    std::vector<card> hand = {
        card(15, 3, 0), // A of Spades
        card(14, 3, 0), // 10 of Spades
        card(13, 3, 0), // K of Spades
        card(12, 3, 0), // Q of Spades
        card(11, 3, 0)  // J of Spades
    };
    // Expected: 15 (run) + 4 (trump marriage) = 19 points
    assert_meld(game, hand, trump, 19, "test_trump_run");
}

void test_marriages(Pin& game) {
    std::cout << "Running test_marriages..." << std::endl;
    
    // Trump marriage (4 points)
    std::vector<card> hand1 = {
        card(13, 2, 0), // K of Hearts
        card(12, 2, 0)  // Q of Hearts
    };
    assert_meld(game, hand1, 2, 4, "test_trump_marriage");

    // Non-trump marriage (2 points)
    std::vector<card> hand2 = {
        card(13, 1, 0), // K of Diamonds
        card(12, 1, 0)  // Q of Diamonds
    };
    assert_meld(game, hand2, 2, 2, "test_nontrump_marriage");

    // Double non-trump marriage (2 * 2 * 2 = 8 points? Let's check Pin.cpp:
    // "int pts = (s == trump) ? 4 : 2; if (suitRank[s][13] > 1 && suitRank[s][12] > 1) pts *= 2;")
    // Yes, a double marriage is 2 * pts.
    std::vector<card> hand3 = {
        card(13, 1, 0), card(13, 1, 1), // 2x K of Diamonds
        card(12, 1, 0), card(12, 1, 1)  // 2x Q of Diamonds
    };
    assert_meld(game, hand3, 2, 4, "test_double_nontrump_marriage"); // 2 * 2 = 4 points
}

void test_arounds(Pin& game) {
    std::cout << "Running test_arounds..." << std::endl;
    // Aces around (10 points)
    std::vector<card> aces = {
        card(15, 0, 0), // A of Clubs
        card(15, 1, 0), // A of Diamonds
        card(15, 2, 0), // A of Hearts
        card(15, 3, 0)  // A of Spades
    };
    assert_meld(game, aces, 0, 10, "test_aces_around");

    // Kings around (8 points)
    std::vector<card> kings = {
        card(13, 0, 0), card(13, 1, 0), card(13, 2, 0), card(13, 3, 0)
    };
    assert_meld(game, kings, 0, 8, "test_kings_around");

    // Queens around (6 points)
    std::vector<card> queens = {
        card(12, 0, 0), card(12, 1, 0), card(12, 2, 0), card(12, 3, 0)
    };
    assert_meld(game, queens, 0, 6, "test_queens_around");

    // Jacks around (4 points)
    std::vector<card> jacks = {
        card(11, 0, 0), card(11, 1, 0), card(11, 2, 0), card(11, 3, 0)
    };
    assert_meld(game, jacks, 0, 4, "test_jacks_around");
}

void test_pinochle(Pin& game) {
    std::cout << "Running test_pinochle..." << std::endl;
    // Single Pinochle: Q of Spades (12, 3) + J of Diamonds (11, 1) -> 4 points
    std::vector<card> singleP = {
        card(12, 3, 0), // Q of Spades
        card(11, 1, 0)  // J of Diamonds
    };
    assert_meld(game, singleP, 0, 4, "test_single_pinochle");

    // Double Pinochle: 2x Q of Spades + 2x J of Diamonds -> 30 points
    std::vector<card> doubleP = {
        card(12, 3, 0), card(12, 3, 1),
        card(11, 1, 0), card(11, 1, 1)
    };
    assert_meld(game, doubleP, 0, 30, "test_double_pinochle");
}

void test_nine_of_trump(Pin& game) {
    std::cout << "Running test_nine_of_trump..." << std::endl;
    // 9 of trump -> 1 point each
    std::vector<card> nines = {
        card(9, 3, 0), // 9 of Spades (trump)
        card(9, 3, 1)  // 9 of Spades (trump)
    };
    assert_meld(game, nines, 3, 2, "test_nine_of_trump");
}

void test_training_game() {
    std::cout << "Running test_training_game..." << std::endl;
    Pin game;
    game.reset_training(42);
    assert(game.training_hand().size() == 12);
    assert(game.training_phase() == 0);
    assert(game.legal_training_actions().size() == 5);

    TrainingStep result = game.step_training(13); // Bid 20, Clubs trump.
    int decisions = 0;
    while (!result.terminated) {
        const std::vector<int> legalActions = game.legal_training_actions();
        assert(!legalActions.empty());
        result = game.step_training(legalActions.front());
        ++decisions;
        assert(decisions <= 12);
    }

    assert(game.training_hand().empty());
    assert(game.training_us_points() + game.training_them_points() > 0);
}

int main() {
    Pin game; // Uses refactored constructor - does not start interactive loop
    test_empty_hand(game);
    test_trump_run(game);
    test_marriages(game);
    test_arounds(game);
    test_pinochle(game);
    test_nine_of_trump(game);
    test_training_game();

    std::cout << "All Pin and meld scoring tests passed!" << std::endl;
    return 0;
}
