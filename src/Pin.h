#ifndef PIN_H
#define PIN_H

#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include <cstdint>
#include <map>
#include "card.h"
#include "aiPlayer.h"

struct TrainingStep {
    float reward;
    bool terminated;
};

// Class definition for Pin
class Pin {
public:
    // Constructor
    Pin();
    void playGame();

    // Methods
    void initialize_deck();  
    void shuffleDeck();
    void choose_dealer();
    void print_deck();
    void deal_hands();
    void print_hand(std::vector<card> choice);
    void count_cards();
    void initilize_game();
    void clear_hands();
    void meld();
    int bidding();
    void choose_cards(std::vector<card>& hand, std::vector<card>& meld_hand);
    int count_meld(std::vector<card> hand, bool verbose = false);
    int count_meld(std::vector<card> hand, int trump, bool verbose = false);
    void all_count_meld();
    void choose_all_cards();
    std::map<int, card> trick();
    std::vector<card> allTrick();
    int hand_winner();
    void delete_cards();
    void print_person(int per);
    void sortHands();
    void takeTrick();
    void doTrickTaking();
    void tPoints();
    bool checkTricks(std::map<int, card> currTrick, int startPlayer);
    int aiMeld(AIPlayer& ai, bool verbose = false);
    int allAiMeld();
    void suitAi(AIPlayer& ai);
    void allSuitAi();

    // Non-interactive API used by the Python reinforcement-learning bindings.
    // Action space (Discrete 47):
    // Actions 0..11: Play card at hand slot 0..11 (Trick play phase)
    // Action 12: Pass (Bidding phase)
    // Actions 13..42: Bid 21..50 (Bidding phase, bid = 21 + (action - 13))
    // Actions 43..46: Choose Trump (43 Clubs, 44 Diamonds, 45 Hearts, 46 Spades)
    static constexpr int ACTION_SPACE_SIZE = 47;

    // Observation space (Box 459, float32):
    // [0..287]:   Hand slots (12 slots x 24 card types one-hot)
    // [288..383]: Current trick slots (4 slots x 24 card types one-hot)
    // [384..431]: Played cards history (24 card types x 2: count >= 1, count == 2)
    // [432..435]: Trump suit (4 one-hot: Clubs, Diamonds, Hearts, Spades; 0 if none)
    // [436..438]: Phase (3 one-hot: 0 Bidding, 1 Trump Selection, 2 Trick Taking)
    // [439]:      Current bid / 50.0f
    // [440..442]: Bid winner (3 one-hot: Us (0/2), Them (1/3), None (-1))
    // [443..446]: Relative seat to dealer (4 one-hot: (player - dealer + 4) % 4)
    // [447..450]: Position in current trick (4 one-hot: currTrick.size())
    // [451]:      Is my turn (1.0f if currentPlayer == player else 0.0f)
    // [452]:      Us meld / 50.0f
    // [453]:      Them meld / 50.0f
    // [454]:      Us trick points / 25.0f
    // [455]:      Them trick points / 25.0f
    // [456]:      Did anyone bid yet (1.0f if bidWinner != -1 else 0.0f)
    // [457..458]: Active bidders (2 floats: fraction Us active, fraction Them active)
    static constexpr int OBSERVATION_SIZE = 459;

    void reset_training(std::uint32_t seed = 0);
    std::vector<int> legal_training_actions() const;
    TrainingStep step_training(int action);
    const std::vector<card>& training_hand() const;
    const std::vector<card>& training_player_hand(int player) const;
    const std::vector<card>& training_trick() const;
    const std::vector<card>& training_played_cards() const;
    int training_trump() const;
    int training_phase() const;
    int training_current_player() const;
    int training_dealer() const;
    int training_bid() const;
    int training_bid_winner() const;
    int training_us_points() const;
    int training_them_points() const;
    int training_us_meld() const;
    int training_them_meld() const;
    int training_us_trick_points() const;
    int training_them_trick_points() const;
    int training_us_score() const;
    int training_them_score() const;
    std::vector<bool> training_active_bidders() const;

    void write_training_observation(int player, float* buffer) const;
    std::vector<float> training_observation(int player) const;
    


    
    
private:
    // Member variables
    int players = 4;
    
 
    
    std::map<int, std::vector<card>*> allHands;
    std::map<int, std::vector<card>*> meldHands;
    std::vector<card> hand;
    std::vector<card> north;
    std::vector<card> west;
    std::vector<card> east;
    std::vector<card> meld_hand;
    std::vector<card> meld_north;
    std::vector<card> meld_west;
    std::vector<card> meld_east;
    std::vector<card> usCards;
    std::vector<card> themCards;
    std::vector<card> currTrick;

    std::map<int, AIPlayer*> playerArray; // 0 is north, 1 is east, 2 is you (not used), 3 is west
    
    
    int tWinner = 0;
    bool lastTrick = false;
    int dealer = 0;
    int round = 0;
    int usPoints = 0;
    int themPoints = 0;
    int usTotal = 0;
    int themTotal = 0;
    int bet;
    int betWinner;
    char winner; 
    int trumpSuit;
    std::map<int, card> trick_cards;
    // SO i need to do stuff for like how do i keep track of the trick points
    std::vector<card> deck;
    std::vector<std::string> ranks = {"9", "J", "Q", "K", "10", "A"};
    std::vector<std::string> suits = {"Clubs", "Diamonds", "Hearts", "Spades"};

    int trainingDealer = 3;
    int trainingBid = 20;
    int trainingBidWinner = -1;
    int trainingTrump = -1;
    std::vector<bool> trainingActiveBidders = {true, true, true, true};
    int trainingCurrentPlayer = 0;
    int trainingPhase = 0; // 0: bidding, 1: trump selection, 2: trick taking, 3: finished
    std::vector<int> trainingTrickPlayers;
    std::vector<card> trainingPlayedCards;
    int trainingUsMeld = 0;
    int trainingThemMeld = 0;
    int trainingUsTrickPoints = 0;
    int trainingThemTrickPoints = 0;
    int trainingUsScore = 0;
    int trainingThemScore = 0;
    std::mt19937 trainingRandom;

    void play_training_card(int player, int handIndex, float& reward);
    void resolve_training_trick(float& reward);
    int choose_training_trump(const std::vector<card>& cards);
};

#endif // PIN_H
