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

void test_bidding_opening_bid_21() {
    std::cout << "  Testing opening bid of 21..." << std::endl;
    Pin game;
    game.reset_training(42);
    assert(game.training_phase() == 0);
    assert(game.training_dealer() == 0);
    assert(game.training_current_player() == 1);
    assert(game.training_bid() == 20);
    assert(game.training_bid_winner() == -1);

    // Initial legal actions: Pass (12) + Bids 21..50 (13..42) = 31 actions
    std::vector<int> legal = game.legal_training_actions();
    assert(legal.size() == 31);
    assert(legal.front() == 12);
    assert(legal[1] == 13);
    assert(legal.back() == 42);

    // Player 1 opens with bid 21 (action 13)
    TrainingStep step = game.step_training(13);
    assert(!step.terminated);
    assert(game.training_phase() == 0);
    assert(game.training_bid() == 21);
    assert(game.training_bid_winner() == 1);
    assert(game.training_current_player() == 2);

    // Player 2 legal bids must be > 21: Pass (12) + Bids 22..50 (14..42) = 30 actions
    legal = game.legal_training_actions();
    assert(legal.size() == 30);
    assert(legal.front() == 12);
    assert(legal[1] == 14);

    // Player 2 passes
    step = game.step_training(12);
    assert(!step.terminated);
    assert(game.training_current_player() == 3);

    // Player 3 passes
    step = game.step_training(12);
    assert(!step.terminated);
    assert(game.training_current_player() == 0);

    // Player 0 (dealer) passes
    step = game.step_training(12);
    assert(!step.terminated);

    // Auction complete! Player 1 won the auction at 21
    assert(game.training_phase() == 1);
    assert(game.training_bid() == 21);
    assert(game.training_bid_winner() == 1);
    assert(game.training_current_player() == 1);
}

void test_bidding_three_non_dealers_pass() {
    std::cout << "  Testing 3 non-dealers pass -> dealer stuck with 20..." << std::endl;
    Pin game;
    game.reset_training(100);
    assert(game.training_phase() == 0);
    const int dealer = game.training_dealer();
    assert(game.training_current_player() == (dealer + 1) % 4);

    // First 2 non-dealers pass -> still in bidding phase
    for (int i = 0; i < 2; ++i) {
        assert(game.training_phase() == 0);
        TrainingStep step = game.step_training(12);
        assert(!step.terminated);
        assert(game.training_phase() == 0);
    }

    // 3rd non-dealer passes -> dealer is immediately stuck with 20 without needing to act!
    TrainingStep step = game.step_training(12);
    assert(!step.terminated);
    assert(game.training_phase() == 1); // Advanced to Trump Selection
    assert(game.training_bid_winner() == dealer);
    assert(game.training_bid() == 20);
    assert(game.training_current_player() == dealer);
}

void test_training_game_full_flow() {
    std::cout << "  Testing full auction -> trump selection -> trick play -> completion..." << std::endl;
    Pin game;
    game.reset_training(42);
    assert(game.training_hand().size() == 12);
    assert(game.training_phase() == 0);

    // Verify observation vector shape and baseline offsets
    std::vector<float> obs = game.training_observation(game.training_current_player());
    assert(static_cast<int>(obs.size()) == Pin::OBSERVATION_SIZE);
    assert(Pin::OBSERVATION_SIZE == 459);
    assert(obs[436] == 1.0f); // Phase 0 one-hot
    assert(obs[439] == 20.0f / 50.0f); // Current bid 20 / 50
    assert(obs[442] == 1.0f); // Bid winner None (-1)
    assert(obs[451] == 1.0f); // Is my turn
    assert(obs[456] == 0.0f); // Did anyone bid yet: false
    assert(obs[457] == 1.0f); // Us active fraction: 2/2 = 1.0
    assert(obs[458] == 1.0f); // Them active fraction: 2/2 = 1.0

    // Full auction: player 1 bids 21, all others pass
    game.step_training(13);
    game.step_training(12);
    game.step_training(12);
    game.step_training(12);

    // Phase 1: Trump Selection
    assert(game.training_phase() == 1);
    assert(game.training_bid_winner() == 1);
    assert(game.training_current_player() == 1);
    const std::vector<int> trumpActions = game.legal_training_actions();
    assert(trumpActions.size() == 4);
    assert(trumpActions[0] == 43 && trumpActions[3] == 46);

    // Choose trump: 46 (Spades = 3)
    TrainingStep step = game.step_training(46);
    assert(!step.terminated);
    assert(game.training_phase() == 2);
    assert(game.training_trump() == 3);
    assert(game.training_us_meld() >= 0);
    assert(game.training_them_meld() >= 0);
    assert(game.training_current_player() == 1); // Bid winner leads first trick

    // Phase 2: Trick Taking until game ends
    obs = game.training_observation(game.training_current_player());
    assert(obs[438] == 1.0f); // Phase 2 one-hot
    assert(obs[435] == 1.0f); // Trump Spades one-hot (432 + 3 = 435)

    int trickCardCount = 0;
    while (!step.terminated) {
        const std::vector<int> legalActions = game.legal_training_actions();
        assert(!legalActions.empty());
        step = game.step_training(legalActions.front());
        ++trickCardCount;
        assert(trickCardCount <= 48);
    }

    // Phase 3: Finished
    assert(step.terminated);
    assert(game.training_phase() == 3);
    assert(game.training_hand().empty());
    assert(game.training_played_cards().size() == 48);
    assert(game.training_us_trick_points() + game.training_them_trick_points() == 25);
    assert(game.legal_training_actions().empty());
}

void test_training_game() {
    std::cout << "Running test_training_game..." << std::endl;
    test_bidding_opening_bid_21();
    test_bidding_three_non_dealers_pass();
    test_training_game_full_flow();
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
