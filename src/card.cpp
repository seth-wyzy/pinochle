#include "card.h"
#include <iostream>


card::card(int r, int s, int m) {
    rank = r;
    suit = s;
    mult = m;
    
}



void card::print_card() const{
    switch (rank){
        case 9:
            std::cout << "9";
            break;
        case 11:
            std::cout << "Jack";
            break;
        case 12:
            std::cout << "Queen";
            break;
        case 13:
            std::cout << "King";
            break;
        case 14:
            std::cout << "10";
            break;
        case 15:
            std::cout << "Ace";
            break;

    }

    std::cout << " of ";

    switch (suit) {
        case 0:
            std::cout << "Clubs" << std::endl;
            break;
        case 1: 
            std::cout << "Diamonds" << std::endl;
            break;
        case 2: 
            std::cout << "Hearts" << std::endl;
            break;
        case 3:
            std::cout << "Spades" << std::endl;
            break;
    }
    

}

std::string card::p_rank() const {
    switch (rank){
        case 9:
            return "9";
            break;
        case 11:
            return "Jack";
            break;
        case 12:
            return "Queen";
            break;
        case 13:
            return "King";
            break;
        case 14:
            return "10";
            break;
        case 15:
            return "Ace";
            break;
        default:
            return "ERROR";
            break;
    }
    
}



std::string card::p_suit() const {
    switch (suit) {
        case 0:
            return "Clubs";
            break;
        case 1: 
            return "Diamonds";
            break;
        case 2: 
            return "Hearts";
            break;
        case 3:
            return "Spades";
            break;
        default:
            return "ERROR";
            break;
    }
}
bool operator==(const card& lhs, const card& rhs) {
    return lhs.rank == rhs.rank && lhs.suit == rhs.suit;
}

bool operator<(const card& lhs, const card& rhs){
    return lhs.rank < rhs.rank;
}
    
bool operator>(const card& lhs, const card& rhs){
    return lhs.rank > rhs.rank;
}
