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
    std::vector<card> legal = getLegalCards(myHand, currTrick, trump);
    
    if (currTrick.empty()) {
        // Leading: play the highest rank card in legal
        card toPlay = legal[0];
        for (const auto& c : legal) {
            if (c.rank > toPlay.rank) {
                toPlay = c;
            }
        }
        return toPlay;
    }
    
    card bestCard = getWinningCard(currTrick, trump);
    std::vector<card> winningPlays;
    for (const auto& c : legal) {
        if ((c.suit == bestCard.suit && c.rank > bestCard.rank) ||
            (c.suit == trump && bestCard.suit != trump)) {
            winningPlays.push_back(c);
        }
    }
    
    if (!winningPlays.empty()) {
        // Play the lowest winning card (the first one, since it is sorted ascending by rank)
        return winningPlays[0];
    }
    
    // Cannot win, play the lowest card in legal
    return legal[0];
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


