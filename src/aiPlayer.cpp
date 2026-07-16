#include "aiPlayer.h"

AIPlayer::AIPlayer(int pos) {
    this->position = pos;
    if (pos == 0) 
        this->team = true; // us
    else
        this->team = false; // them
    hMeld = 0;
    dMeld = 0;
    cMeld = 0;
    sMeld = 0;
}
/*
* This should be used at the start of each round to initilize the hand, meld can be choosen
*/
void AIPlayer::startRound(std::vector<card> deltHand) { 
    this->myHand = deltHand;
    hMeld = 0;
    dMeld = 0;
    cMeld = 0;
    sMeld = 0;
}


/*
* Deal with 4 possible senarios:
* Playing first
* having a card of the same suit
* not having a card of the same suit, and having trump --> check if a trump has already been played
* not having a card of the same suit, and not having trump --> have to know who's leading
*/
card AIPlayer::chooseMove(std::vector<card> currTrick, bool leader, int trump) {
    if (currTrick.empty()) { 
        // Leading: for now, just play the highest card
        card toPlay = myHand[0];
        int index = 0;
        for (int i = 1; i < myHand.size(); i++) {
            if (myHand[i].rank > toPlay.rank) {
                toPlay = myHand[i];
                index = i;
            }
        }
        return toPlay;
    }

    card leadCard = currTrick[0];
    card bestCardInTrick = currTrick[0];
    for (const auto& c : currTrick) {
        if (c.suit == bestCardInTrick.suit) {
            if (c.rank > bestCardInTrick.rank) bestCardInTrick = c;
        } else if (c.suit == trump) {
            bestCardInTrick = c;
        }
    }

    std::vector<card> followSuit;
    std::vector<card> trumps;
    for (const auto& c : myHand) {
        if (c.suit == leadCard.suit) followSuit.push_back(c);
        if (c.suit == trump) trumps.push_back(c);
    }

    if (!followSuit.empty()) {
        // Must follow suit. Try to beat best card if it's the same suit
        if (bestCardInTrick.suit == leadCard.suit) {
            for (const auto& c : followSuit) {
                if (c.rank > bestCardInTrick.rank) return c;
            }
        }
        // Can't beat or teammate is winning? Just play lowest follow suit
        return followSuit.back(); 
    }

    if (!trumps.empty()) {
        // Must trump if no follow suit. Try to beat best card if it's trump
        if (bestCardInTrick.suit == trump) {
            for (const auto& c : trumps) {
                if (c.rank > bestCardInTrick.rank) return c;
            }
        } else {
            // Trump is not played yet, any trump wins
            return trumps.back();
        }
        return trumps.back();
    }
     
    // Can't follow suit or trump, play anything (lowest)
    return myHand.back();
}

int AIPlayer::chooseBid(int currBid) { 
    // AI bids based on its biddingMeld + some anticipated trick points (e.g. 10)
    int maxBid = biddingMeld + 10;
    if (currBid == 0) { // First bid
        if (21 <= maxBid) return 21;
        return -1; // Pass
    }
    if (currBid < maxBid) 
        return currBid + 1;
    return -1; // Pass
}

int AIPlayer::chooseTrump() {
    return personalTrump;
}

void AIPlayer::choosePersonalTrump() {
    biddingMeld = cMeld;
    personalTrump = 0;
    if (dMeld > biddingMeld) {
        biddingMeld = dMeld;
        personalTrump = 1;
    }
    if (hMeld > biddingMeld) {
        biddingMeld = hMeld;
        personalTrump = 2;
    }
    if (sMeld > biddingMeld) {
        biddingMeld = sMeld;
        personalTrump = 3;
    }
}


