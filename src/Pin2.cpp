#include "Pin2.h"
#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <map>


// Constructor definition
Pin::Pin() {
    initialize_deck();
    shuffleDeck();
    choose_dealer();
    allHands[0] = northPtr;
    allHands[1] = eastPtr;
    allHands[2] = handPtr;
    allHands[3] = westPtr;
    initilize_game();
}

// Member functions definitions
void Pin::initialize_deck() {
    for (int repeat = 0; repeat < 2; ++repeat) {
        for (const auto& rank : ranks) {
            for (const auto& suit : suits) {
                deck.push_back(rank + " of " + suit);
            }
        }
    }
}

void Pin::shuffleDeck() {
    std::random_device rd;
    std::mt19937 g(rd()); 
    std::shuffle(deck.begin(), deck.end(), g);
}

void Pin::choose_dealer() {
    std::random_device rd;
    std::mt19937 gen(rd()); 
    std::uniform_int_distribution<int> dist(0, players - 1);
    dealer = dist(gen);
}

void Pin::print_deck() {
    for (const auto& card : deck) {
        std::cout << card << " " << std::endl;
    }
}

void Pin::deal_hands() {
    for (int i = dealer; i < 48+dealer; i++) {
        allHands[i % 4]->push_back(deck[i - dealer]);
    }
}

void Pin::print_hand(std::vector<std::string> choice) {
    for (const auto& card:choice) {
        std::cout << card << std::endl;
    }
}

void Pin::count_cards() {
    std::cout << north.size() << " cards in north's hand" << std::endl;
    std::cout << east.size() << " cards in east's hand" << std::endl;
    std::cout << hand.size() << " cards in your hand" << std::endl;
    std::cout << west.size() << " cards in west's hand" << std::endl;    
}

void Pin::clear_hands(){
    north.clear();
    east.clear();
    west.clear();
    hand.clear();
}
void Pin::initilize_game() {
    clear_hands();
    deal_hands();
    print_hand(hand);
}
void Pin::meld() {

}
void Pin::bidding() {
    bool y = true;
    bool n = true;
    bool e = true;
    bool w = true;
    int n_bid = 0;  
    int e_bid = 0;    
    int y_bid = 0;
    int w_bid = 0;
    int winning_bid;
    int temp_bid;

    while (y || n || e || w) {
        switch (dealer) {
            case 0:
                if (n) {
                    std::cout << "North Bid: ";
                    std::cin >> n_bid;
                    std::cout << std::endl;
                    if (n_bid == 0) {
                        n = false;
                    }
                    
                }
            case 1:
                if (e) {
                    std::cout << "East Bid: ";
                    std::cin >> e_bid;
                    std::cout << std::endl;
                    if (e_bid == 0) {
                        e = false;
                    }
                }

            case 2:
                if (y) {
                    std::cout << "Your Bid: ";
                    std::cin >> y_bid;
                    std::cout << std::endl;
                    if (y_bid == 0) {
                        y = false;
                    }  
                }
            case 3:
                if (w) {
                    std::cout << "West Bid: ";
                    std::cin >> w_bid;
                    std::cout << std::endl;
                    if (w_bid == 0) {
                        w = false;
                    }   
                }
                dealer = 0;
        }


    }
    

}