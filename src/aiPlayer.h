#ifndef AIPLAYER
#define AIPLAYER

#include "card.h"
#include <vector>


class AIPlayer {
public:
    //constructor
    AIPlayer(int pos);

    void startRound(std::vector<card> deltHand);
    card chooseMove(std::vector<card> currTrick, bool leader, int trump);
    int chooseBid(int currBid);
    std::vector<card> getMyHand() {return myHand;};
    void setMeld(int m) {meld = m;};
    int hypotheticalMeld();
    int chooseTrump();
    void sethMeld(int m) {hMeld = m;};
    void setdMeld(int m) {dMeld = m;};
    void setsMeld(int m) {sMeld = m;};
    void setcMeld(int m) {cMeld = m;};
    void choosePersonalTrump();
private:
    int position;
    
    bool team; // true if us, false if them
    std::vector<card> myHand;
    int meld;
    int biddingMeld;
    int personalTrump;
    int hMeld;
    int dMeld;
    int sMeld;
    int cMeld;

};


#endif