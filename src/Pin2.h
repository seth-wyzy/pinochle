#ifndef PIN_H
#define PIN_H

#include <iostream>
#include <vector>
#include <string>
#include <random>
#include <algorithm>
#include <map>

// Class definition for Pin
class Pin {
public:
    // Constructor
    Pin();

    // Methods
    void initialize_deck();
    void shuffleDeck();
    void choose_dealer();
    void print_deck();
    void deal_hands();
    void print_hand(std::vector<std::string> choice);
    void count_cards();
    void initilize_game();
    void clear_hands();
    void meld();
    void bidding();
    std::vector<std::string> hand;
    std::vector<std::string> north;
    std::vector<std::string> west;
    std::vector<std::string> east;
    
private:
    // Member variables
    int players = 4;
    
    std::vector<std::string>* handPtr = &hand;
    std::vector<std::string>* northPtr = &north;
    std::vector<std::string>* westPtr = &west;
    std::vector<std::string>* eastPtr = &east;
    
    std::map<int, std::vector<std::string>*> allHands;
    


    int dealer = 0;
    int round = 0;
    std::vector<std::string> deck;
    std::vector<std::string> ranks = {"9", "J", "Q", "K", "10", "A"};
    std::vector<std::string> suits = {"Clubs", "Diamonds", "Hearts", "Spades"};
};

#endif // PIN_H
