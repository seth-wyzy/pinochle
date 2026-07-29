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
    // Actions 12-16 are bidding actions: pass, or bid 20 with trump 0-3.
    void reset_training(std::uint32_t seed = 0);
    std::vector<int> legal_training_actions() const;
    TrainingStep step_training(int action);
    const std::vector<card>& training_hand() const;
    const std::vector<card>& training_player_hand(int player) const;
    const std::vector<card>& training_trick() const;
    int training_trump() const;
    int training_phase() const;
    int training_current_player() const;
    int training_us_points() const;
    int training_them_points() const;
    


    
    
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

    int trainingPhase = 0; // 0: bidding, 1: trick taking, 2: finished
    int trainingCurrentPlayer = 2;
    std::vector<int> trainingTrickPlayers;
    std::mt19937 trainingRandom;

    void play_training_card(int player, int handIndex, float& reward);
    void resolve_training_trick(float& reward);
    int choose_training_trump(const std::vector<card>& cards);
};

#endif // PIN_H
