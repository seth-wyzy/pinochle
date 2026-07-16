// card class
#ifndef CARD_H
#define CARD_H

#include <bitset>
#include <iostream>
#include <string>

class card {

public:
    card() : rank(0), suit(0), mult(0) {}
    card(int rank, int suit, int m);
    void print_card() const;
    std::string p_rank() const;
    std::string p_suit() const;
    int rank; // 9, J, Q, K, 10, A 
    int suit; // Clubs, Diamons, Hearts, Spades
    int mult; // 0 or 1
    friend bool operator==(const card& lhs, const card& rhs);
    friend bool operator<(const card& lhs, const card& rhs);
    friend bool operator>(const card& lhs, const card& rhs);
};

#endif