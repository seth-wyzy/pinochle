#include "Pin2.h"
#include <iostream>


int main() {
    Pin game;
    std::cout << "Your hand " << std::endl;
    game.shuffleDeck();
    // game.deal_hands();
    game.print_hand(game.hand);
    game.count_cards();
    game.print_hand(game.north);
    game.bidding();
    
}