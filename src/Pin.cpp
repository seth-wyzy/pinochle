#include "Pin.h"
#include "aiPlayer.h"
#include "card.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <exception>
#include <iostream>
#include <map>
#include <ostream>
#include <random>
#include <vector>



// Constructor definition
Pin::Pin() {
    initialize_deck();
    shuffleDeck();
    choose_dealer();

    allHands[0] = &north;
    allHands[1] = &east;
    allHands[2] = &hand;
    allHands[3] = &west;

    meldHands[0] = &meld_north;
    meldHands[1] = &meld_east;
    meldHands[2] = &meld_hand;
    meldHands[3] = &meld_west;

    playerArray[0] = new AIPlayer(0);
    playerArray[1] = new AIPlayer(1);
    playerArray[3] = new AIPlayer(3);
}

void Pin::playGame() {
    for (int i = 0; i < 4; i++) { // Let's play 4 rounds for testing or one full game
        std::cout << "\n--- Round " << i+1 << " ---\n";
        std::cout << "Dealer: ";
        print_person(dealer);
        shuffleDeck();
        clear_hands();
        deal_hands();
        sortHands();
        
        usCards.clear();
        themCards.clear();
        usPoints = 0;
        themPoints = 0;

        // Initialize AI players with their hands
        for (int p = 0; p < 4; p++) {
            if (p != 2) {
                playerArray[p]->startRound(*allHands[p]);
                suitAi(*playerArray[p]);
            }
        }

        int winningBid = bidding();
        
        // Count meld for everyone
        std::cout << "--- Meld ---" << std::endl;
        int roundUsMeld = 0;
        int roundThemMeld = 0;
        for (int p = 0; p < 4; p++) {
            int m;
            std::cout << "\n--- ";
            print_person(p);
            std::cout << " Meld ---" << std::endl;
            if (p == 2) {
                m = count_meld(hand, trumpSuit, true); 
            } else {
                m = aiMeld(*playerArray[p], true); 
            }
            
            if (p == 0 || p == 2) roundUsMeld += m;
            else roundThemMeld += m;
        }
        std::cout << "\nUs Meld Total: " << roundUsMeld << " Them Meld Total: " << roundThemMeld << std::endl;

        doTrickTaking();
        tPoints();
        
        int usRoundTotal = roundUsMeld + usPoints;
        int themRoundTotal = roundThemMeld + themPoints;

        if (betWinner == 0 || betWinner == 2) { // Us bid
            if (usRoundTotal >= bet) {
                usTotal += usRoundTotal;
                std::cout << "Us MADE bid! +" << usRoundTotal << " pts" << std::endl;
            } else {
                usTotal -= bet;
                std::cout << "Us SET! -" << bet << " pts" << std::endl;
            }
            themTotal += themRoundTotal;
        } else { // Them bid
            if (themRoundTotal >= bet) {
                themTotal += themRoundTotal;
                std::cout << "Them MADE bid! +" << themRoundTotal << " pts" << std::endl;
            } else {
                themTotal -= bet;
                std::cout << "Them SET! -" << bet << " pts" << std::endl;
            }
            usTotal += usRoundTotal;
        }
        
        std::cout << "Us Trick Points: " << usPoints << " Them Trick Points: " << themPoints << std::endl;
        std::cout << "Current Score - Us: " << usTotal << " Them: " << themTotal << std::endl;
        
        dealer = (dealer + 1) % 4;
    }
}

// Member function definitions
void Pin::initialize_deck() {
    for (int repeat = 0; repeat < 2; ++repeat) {
        for (int r = 9; r <= 15; r++) {
            if (r != 10) {
                for (int s = 0; s <= 3; s++) {
                    deck.push_back(card(r,s,repeat));
                }
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
        card.print_card();
    }
}

void Pin::deal_hands() {
    for (int i = dealer; i < 48+dealer; i++) {
        allHands[i % 4]->push_back(deck[i - dealer]);
    }
}

void Pin::print_hand(std::vector<card> choice) {
    for (const auto& card: choice) {
        card.print_card();
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
    bidding();
     
}
int Pin::bidding() {
    std::map<int, bool> active;
    for (int i = 0; i < 4; i++) active[i] = true;
    int currHigh = 20; 
    int bidderWinner = -1;
    int turn = (dealer + 1) % 4;
    int activeCount = 4;

    while (activeCount > 1) {
        if (active[turn]) {
            int bid = 0;
            if (turn == 2) { // Human
                std::cout << "Your Hand: \n";
                print_hand(hand);
                std::cout << "Current high bid: " << (currHigh == 20 ? "None" : std::to_string(currHigh)) << std::endl;
                std::cout << "Enter your bid (0 to pass): ";
                std::cin >> bid;
                if (bid <= currHigh) bid = 0;
            } else { // AI
                bid = playerArray[turn]->chooseBid(currHigh);
                if (bid != -1) {
                    std::cout << "Player ";
                    print_person(turn);
                    std::cout << " bids " << bid << std::endl;
                } else {
                    std::cout << "Player ";
                    print_person(turn);
                    std::cout << " passes." << std::endl;
                    bid = 0;
                }
            }

            if (bid == 0) {
                active[turn] = false;
                activeCount--;
            } else {
                currHigh = bid;
                bidderWinner = turn;
            }
        }
        turn = (turn + 1) % 4;
    }

    if (bidderWinner == -1) {
        bidderWinner = dealer;
        currHigh = 20;
        std::cout << "Everyone passed. Dealer (";
        print_person(dealer);
        std::cout << ") is stuck with 20." << std::endl;
    }

    std::cout << "Winner of bidding: ";
    print_person(bidderWinner);
    std::cout << " with bid " << currHigh << std::endl;
    bet = currHigh;
    betWinner = bidderWinner;

    if (bidderWinner == 2) {
        std::cout << "Clubs: 0, Diamonds: 1, Hearts: 2, Spades: 3" << std::endl;
        std::cout << "Please input your trump suit: ";
        std::cin >> trumpSuit;
    } else {
        trumpSuit = playerArray[bidderWinner]->chooseTrump();
        std::cout << "Trump suit chosen: " << trumpSuit << " (";
        card temp(9, trumpSuit, 0);
        std::cout << temp.p_suit() << ")" << std::endl;
    }

    tWinner = bidderWinner; // Bid winner starts the first trick
    return currHigh;
}

void Pin::choose_cards(std::vector<card>& hand, std::vector<card>& meld_hand){
    char ch;
    print_hand(hand);
    std::cout << "type y to include or n to not include" << std::endl;
    for (const auto& card : hand){
        card.print_card();
        std::cin >> ch;
        if (ch == 'y') {
            meld_hand.push_back(card);
        } 
    }
    print_hand(meld_hand);
}
void Pin::choose_all_cards(){
    for (int i = 0; i < 4; i++){
        std::cout << i << ":" << std::endl;
        choose_cards(*allHands[i],*meldHands[i]);
    }
}

int Pin::count_meld(std::vector<card> hand, bool verbose){ // should only pass meld hand in for actual playing becuase we want that element
/*
    Pinochle 4
    Double 30
    Around:
        Ace 10
        King 8
        Queen 6
        Jack 4
    Marriage 2/4
    Run 15
    9_t 1
*/
    // rank of suit
    // Clubs, Dia, Hearts, Spades
    std::map<int, std::map<int, int>> suitRank;
    std::map<int, int> suits;
    std::map<int, int> ranks;

    for (const auto& card : hand) {
        suitRank[card.suit][card.rank]++;
        suits[card.suit]++;
        ranks[card.rank]++;
    }
    int meldPoints = 0;

    
    // Run in trump: A 10 K Q J (15 14 13 12 11)
    std::vector<int> run = {15, 14, 13, 12, 11};
    bool hasRun = true;
    for (int r : run) {
        if (suitRank[trumpSuit][r] < 1) {
            hasRun = false;
            break;
        }
    }
    if (hasRun) {
        if (verbose) std::cout << "Run in Trump: +15" << std::endl;
        meldPoints += 15;
    }
    // Marriage
    for (int s = 0; s <= 3; ++s) {
        if (suitRank[s][13] > 0 && suitRank[s][12] > 0) {
            int pts = (s == trumpSuit) ? 4 : 2;
            if (suitRank[s][13] > 1 && suitRank[s][12] > 1) pts *= 2;
            if (verbose) {
                card che = {0, s,0};
                std::cout << "Marriage in " << che.p_suit() << ": +" << pts << std::endl;
            }
            meldPoints += pts;
        }
    }
    std::map<int, int> aroundPoints = {
        {15, 10}, // Aces
        {13, 8},  // Kings
        {12, 6},  // Queens
        {11, 4}   // Jacks
    };
    for (auto [r, points] : aroundPoints) {
        bool hasAll = true;
        for (int s = 0; s < 4; ++s) {
            if (suitRank[s][r] == 0) {
                hasAll = false;
                break;
            }
        }
        if (hasAll) {
            if (verbose) {
                card check = {r,0,0};
                std::cout << check.p_rank() <<"s around: +" << points << "\n";
            }
            meldPoints += points;
        }
    }

        // Pinochle: Q diamonds (12,3) + J spades (11,1)
        int q_spades = suitRank[3][12];
        int j_diamonds = suitRank[1][11];
        int pinochles = std::min(q_spades, j_diamonds);
        if (pinochles >= 1) {
            int pts = (pinochles == 2) ? 30 : 4 * pinochles;
            if (verbose) std::cout << (pinochles == 2 ? "Double " : "") << "Pinochle: +" << pts << "\n";
            meldPoints += pts;
        }
    
        // 9 of trump
        if (suitRank[trumpSuit][9] > 0) {
            int p = suitRank[trumpSuit][9];
            if (verbose) std::cout << "9 of trump x" << p << std::endl;
            meldPoints += p;
        }
    
        if (verbose) std::cout << "Total Meld Points: " << meldPoints << std::endl;
    

        return meldPoints;
}
void Pin::all_count_meld(){
    for (int i = 0; i < 4; i++){
        count_meld(*allHands[i]);
    }
}

std::map<int, card> Pin::trick() {
    std::map<int, card> current_trick;
    std::vector<card> trick_vec;
    
    for (int i = 0; i < 4; i++) {
        int h = (tWinner + i) % 4;
        card chosen_card;
        
        if (h == 2) { // Human
            std::cout << "--- Your turn ---" << std::endl;
            if (i > 0) {
                std::cout << "Current trick:" << std::endl;
                for (const auto& c : trick_vec) c.print_card();
            }
            std::cout << ":\n ";
            print_hand(hand);
            std::cout << "Enter rank and suit (e.g. 15 3 for Ace of Spades): ";
            
            bool valid = false;
            while (!valid) {
                int r, s;
                if (!(std::cin >> r >> s)) {
                    std::cin.clear();
                    std::cin.ignore(10000, '\n');
                    continue;
                }
                chosen_card = card(r, s, 0);
                std::vector<card> legal = getLegalCards(*allHands[h], trick_vec, trumpSuit);
                auto legal_it = std::find(legal.begin(), legal.end(), chosen_card);
                if (legal_it != legal.end()) {
                    auto hand_it = std::find(allHands[h]->begin(), allHands[h]->end(), chosen_card);
                    if (hand_it != allHands[h]->end()) {
                        chosen_card = *hand_it;
                        allHands[h]->erase(hand_it);
                        valid = true;
                    }
                } else {
                    auto hand_it = std::find(allHands[h]->begin(), allHands[h]->end(), chosen_card);
                    if (hand_it != allHands[h]->end()) {
                        std::cout << "Illegal card play. You must follow Pinochle rules (follow suit, trump, must-win). Try again: ";
                    } else {
                        std::cout << "Card not in hand. Try again: ";
                    }
                }
            }
        } else { // AI
            chosen_card = playerArray[h]->chooseMove(trick_vec, (i == 0), trumpSuit);
            auto it = std::find(allHands[h]->begin(), allHands[h]->end(), chosen_card);
            if (it != allHands[h]->end()) {
                allHands[h]->erase(it);
            }
            std::cout << "Player ";
            print_person(h);
            std::cout << " plays ";
            chosen_card.print_card();
        }
        current_trick[h] = chosen_card;
        trick_vec.push_back(chosen_card);
    }
    return current_trick;
}


int Pin::hand_winner() {
    if (trick_cards.empty()) return -1;
    
    int winner = tWinner;
    card winningCard = trick_cards[tWinner];
    
    for (int i = 1; i < 4; i++) {
        int h = (tWinner + i) % 4;
        card c = trick_cards[h];
        if (c.suit == winningCard.suit) {
            if (c.rank > winningCard.rank) {
                winner = h;
                winningCard = c;
            }
        } else if (c.suit == trumpSuit) {
            winner = h;
            winningCard = c;
        }
    }
    return winner;
}

void Pin::takeTrick(){
    int winner = hand_winner();
    switch (winner) {
        case 0: case 2:
            for (const auto& it: trick_cards){
                usCards.push_back(it.second);
            }
            lastTrick = true;
            break;
        case 1: case 3:
            for (const auto& it: trick_cards) {
                themCards.push_back(it.second);
            }
            lastTrick = false;
            break;
    }
    tWinner = winner; // Winner of this trick starts the next one
    trick_cards.clear();
}

void Pin::doTrickTaking() {
    usCards.clear();
    themCards.clear();
    for (int i = 0; i < 12; i++) {
        std::cout << "\n--- Trick " << i+1 << " ---\n";
        trick_cards = trick();
        takeTrick();
        std::cout << "Trick winner: ";
        print_person(tWinner);
        std::cout << std::endl;
    }
}

void Pin::tPoints() {
    for (const auto& it: usCards) {
        if (it.rank >= 13) {
            usPoints++;
        }
    }
    for (const auto& it: themCards) {
        if (it.rank >= 13) {
            themPoints++;
        }
    }
    if (lastTrick) {
        usPoints++;
    } else {
        themPoints++;
    }
}


void Pin::print_person(int per) { // prints whos turn it is based on an integer [0-3]
    switch (per) {
        case 0:
            std::cout << "North\n";
            break;
        case 1:
            std::cout << "East\n";
            break;
        case 2:
            std::cout << "You\n";
            break;
        case 3:
            std::cout << "West\n";
            break;
        default:
            std::cout << "Invalid Person\n";
    }   
}

void Pin::sortHands() {
    // lambda to compare two cards: suit first, then rank
    auto cardLess = [](const card &a, const card &b) {
        if (a.suit != b.suit) 
            return a.suit < b.suit;
        return a.rank < b.rank;
    };

    // sort each hand in allHands[0-3]
    for (int i = 0; i < 4; ++i) {
        std::vector<card>* handPtr = allHands[i];
        std::sort(handPtr->begin(), handPtr->end(), cardLess);
    }
}

bool Pin::checkTricks(std::map<int, card> currTrick, int startPlayer) {
    std::vector<card> trickCards;
    for (int i = 0; i < 4; ++i) {
        int h = (startPlayer + i) % 4;
        std::vector<card> handBeforePlay = *allHands[h];
        handBeforePlay.push_back(currTrick[h]);
        
        std::vector<card> legal = getLegalCards(handBeforePlay, trickCards, trumpSuit);
        bool isLegal = false;
        for (const auto& c : legal) {
            if (c == currTrick[h]) {
                isLegal = true;
                break;
            }
        }
        if (!isLegal) {
            return false;
        }
        trickCards.push_back(currTrick[h]);
    }
    return true;
}

int Pin::aiMeld(AIPlayer& ai, bool verbose) {
    int meld =  count_meld(ai.getMyHand(), trumpSuit, verbose);
    ai.setMeld(meld);
    return meld;
}

int Pin::allAiMeld() {
    int total = 0;
    for (int i = 0; i < 4; i++) {
        if (i == 2) continue;
        total += aiMeld(*playerArray[i]);
    }
    return total;
}

void Pin::suitAi(AIPlayer& ai) {
    std::vector<card> hand = ai.getMyHand();
    ai.setcMeld(count_meld(hand, 0, false)); // 0: Clubs
    ai.setdMeld(count_meld(hand, 1, false)); // 1: Diamonds
    ai.sethMeld(count_meld(hand, 2, false)); // 2: Hearts
    ai.setsMeld(count_meld(hand, 3, false)); // 3: Spades
    ai.choosePersonalTrump(); // picks the largest one for bidding purposes 
}

void Pin::allSuitAi() {
    for (int i = 0; i < 4; i++) {
        if (i == 2) continue;
        suitAi(*playerArray[i]);
    }
}

void Pin::reset_training(std::uint32_t seed) {
    trainingRandom.seed(seed);
    deck.clear();
    initialize_deck();
    std::shuffle(deck.begin(), deck.end(), trainingRandom);
    clear_hands();
    deal_hands();
    sortHands();

    trainingDealer = (trainingDealer + 1) % 4;
    dealer = trainingDealer;
    trainingBid = 20;
    trainingBidWinner = -1;
    bet = 20;
    betWinner = -1;
    trainingActiveBidders.assign(4, true);
    trainingCurrentPlayer = (trainingDealer + 1) % 4;
    trainingPhase = 0;

    currTrick.clear();
    trainingTrickPlayers.clear();
    trainingPlayedCards.clear();
    usCards.clear();
    themCards.clear();

    trainingTrump = -1;
    trumpSuit = -1;
    tWinner = trainingCurrentPlayer;

    trainingUsMeld = 0;
    trainingThemMeld = 0;
    trainingUsTrickPoints = 0;
    trainingThemTrickPoints = 0;
    trainingUsScore = 0;
    trainingThemScore = 0;
    usPoints = 0;
    themPoints = 0;
    this->usTotal = 0;
    this->themTotal = 0;
}

std::vector<int> Pin::legal_training_actions() const {
    std::vector<int> actions;
    if (trainingPhase == 0) {
        // Bidding phase
        // Action 12: Pass
        actions.push_back(12);
        // Actions 13..42: Bid 21..50 (bid amount = 21 + (action - 13))
        for (int a = 13; a <= 42; ++a) {
            int bidAmount = 21 + (a - 13);
            if (bidAmount > trainingBid) {
                actions.push_back(a);
            }
        }
        return actions;
    }
    if (trainingPhase == 1) {
        // Trump selection phase: 43 Clubs, 44 Diamonds, 45 Hearts, 46 Spades
        return {43, 44, 45, 46};
    }
    if (trainingPhase == 2) {
        // Trick taking phase
        const std::vector<card>& currentHand = *allHands.at(trainingCurrentPlayer);
        const std::vector<card> legal = getLegalCards(currentHand, currTrick, trainingTrump);
        for (int i = 0; i < static_cast<int>(currentHand.size()); ++i) {
            if (std::find(legal.begin(), legal.end(), currentHand[i]) != legal.end()) {
                actions.push_back(i);
            }
        }
        return actions;
    }
    // Phase 3 (Finished) or invalid
    return {};
}

TrainingStep Pin::step_training(int action) {
    TrainingStep result{0.0f, trainingPhase == 3};
    const std::vector<int> legalActions = legal_training_actions();
    if (std::find(legalActions.begin(), legalActions.end(), action) == legalActions.end()) {
        result.reward = -1.0f;
        result.terminated = (trainingPhase == 3);
        return result;
    }

    if (trainingPhase == 0) {
        if (action == 12) {
            trainingActiveBidders[trainingCurrentPlayer] = false;
        } else if (action >= 13 && action <= 42) {
            trainingBid = 21 + (action - 13);
            trainingBidWinner = trainingCurrentPlayer;
            bet = trainingBid;
            betWinner = trainingBidWinner;
        }

        int activeCount = 0;
        for (int i = 0; i < 4; ++i) {
            if (trainingActiveBidders[i]) {
                ++activeCount;
            }
        }

        if (activeCount <= 1) {
            if (trainingBidWinner == -1) {
                // The 3 players besides dealer passed without bidding: dealer stuck with 20
                trainingBidWinner = trainingDealer;
                trainingBid = 20;
            }
            bet = trainingBid;
            betWinner = trainingBidWinner;
            trainingPhase = 1;
            trainingCurrentPlayer = trainingBidWinner;
        } else {
            // Advance trainingCurrentPlayer = (trainingCurrentPlayer + 1) % 4 until active bidder
            do {
                trainingCurrentPlayer = (trainingCurrentPlayer + 1) % 4;
            } while (!trainingActiveBidders[trainingCurrentPlayer]);
        }
    } else if (trainingPhase == 1) {
        // Trump selection phase
        trainingTrump = action - 43;
        trumpSuit = trainingTrump;

        // Calculate meld for all 4 players with trainingTrump
        int meld0 = count_meld(*allHands.at(0), trainingTrump, false);
        int meld1 = count_meld(*allHands.at(1), trainingTrump, false);
        int meld2 = count_meld(*allHands.at(2), trainingTrump, false);
        int meld3 = count_meld(*allHands.at(3), trainingTrump, false);
        trainingUsMeld = meld0 + meld2;
        trainingThemMeld = meld1 + meld3;

        trainingPhase = 2;
        trainingCurrentPlayer = trainingBidWinner;
        tWinner = trainingBidWinner;
    } else if (trainingPhase == 2) {
        // Trick taking phase
        play_training_card(trainingCurrentPlayer, action, result.reward);
    }

    result.terminated = (trainingPhase == 3);
    return result;
}

const std::vector<card>& Pin::training_hand() const {
    return training_player_hand(trainingCurrentPlayer);
}

const std::vector<card>& Pin::training_player_hand(int player) const {
    return *allHands.at(player);
}

const std::vector<card>& Pin::training_trick() const {
    return currTrick;
}

const std::vector<card>& Pin::training_played_cards() const {
    return trainingPlayedCards;
}

int Pin::training_trump() const {
    return trainingTrump;
}

int Pin::training_phase() const {
    return trainingPhase;
}

int Pin::training_current_player() const {
    return trainingCurrentPlayer;
}

int Pin::training_dealer() const {
    return trainingDealer;
}

int Pin::training_bid() const {
    return trainingBid;
}

int Pin::training_bid_winner() const {
    return trainingBidWinner;
}

int Pin::training_us_points() const {
    return trainingUsTrickPoints;
}

int Pin::training_them_points() const {
    return trainingThemTrickPoints;
}

int Pin::training_us_meld() const {
    return trainingUsMeld;
}

int Pin::training_them_meld() const {
    return trainingThemMeld;
}

int Pin::training_us_trick_points() const {
    return trainingUsTrickPoints;
}

int Pin::training_them_trick_points() const {
    return trainingThemTrickPoints;
}

int Pin::training_us_score() const {
    return trainingUsScore;
}

int Pin::training_them_score() const {
    return trainingThemScore;
}

std::vector<bool> Pin::training_active_bidders() const {
    return trainingActiveBidders;
}

void Pin::play_training_card(int player, int handIndex, float& reward) {
    std::vector<card>& playerHand = *allHands.at(player);
    const card played = playerHand[handIndex];
    playerHand.erase(playerHand.begin() + handIndex);
    currTrick.push_back(played);
    trainingTrickPlayers.push_back(player);
    trainingCurrentPlayer = (player + 1) % 4;

    if (currTrick.size() == 4) {
        resolve_training_trick(reward);
    }
}

void Pin::resolve_training_trick(float& reward) {
    const card winningCard = getWinningCard(currTrick, trainingTrump);
    int winner = trainingTrickPlayers.front();
    for (std::size_t i = 0; i < currTrick.size(); ++i) {
        if (currTrick[i] == winningCard) {
            winner = trainingTrickPlayers[i];
            break;
        }
    }

    int trickPoints = 0;
    for (const card& played : currTrick) {
        if (played.rank >= 13) {
            ++trickPoints;
        }
    }

    bool isLastTrick = allHands.at(0)->empty();
    if (isLastTrick) {
        ++trickPoints;
    }

    const bool ourTeam = (winner == 0 || winner == 2);
    if (ourTeam) {
        trainingUsTrickPoints += trickPoints;
        reward += static_cast<float>(trickPoints) / 25.0f;
    } else {
        trainingThemTrickPoints += trickPoints;
        reward -= static_cast<float>(trickPoints) / 25.0f;
    }

    for (const card& played : currTrick) {
        trainingPlayedCards.push_back(played);
    }

    currTrick.clear();
    trainingTrickPlayers.clear();
    tWinner = winner;
    trainingCurrentPlayer = winner;

    if (isLastTrick) {
        trainingPhase = 3;
        int finalUsTotal = trainingUsMeld + trainingUsTrickPoints;
        int finalThemTotal = trainingThemMeld + trainingThemTrickPoints;
        int usScore = 0;
        int themScore = 0;

        if (trainingBidWinner == 0 || trainingBidWinner == 2) {
            // Us bid
            usScore = (finalUsTotal >= trainingBid) ? finalUsTotal : -trainingBid;
            themScore = finalThemTotal;
        } else {
            // Them bid
            themScore = (finalThemTotal >= trainingBid) ? finalThemTotal : -trainingBid;
            usScore = finalUsTotal;
        }
        trainingUsScore = usScore;
        trainingThemScore = themScore;

        float totalTargetReward = static_cast<float>(usScore - themScore) / 25.0f;
        float trickRewardsGiven = static_cast<float>(trainingUsTrickPoints - trainingThemTrickPoints) / 25.0f;
        float settlementReward = totalTargetReward - trickRewardsGiven;
        reward += settlementReward;

        usPoints = trainingUsTrickPoints;
        themPoints = trainingThemTrickPoints;
        this->usTotal = trainingUsScore;
        this->themTotal = trainingThemScore;
    }
}

void Pin::write_training_observation(int player, float* buffer) const {
    std::fill_n(buffer, OBSERVATION_SIZE, 0.0f);

    // 1. Hand slots: 12 slots x 24 one-hot = 288 (0..287)
    const std::vector<card>& playerHand = *allHands.at(player);
    for (std::size_t slot = 0; slot < playerHand.size() && slot < 12; ++slot) {
        int idx = card_index(playerHand[slot]);
        if (idx >= 0 && idx < 24) {
            buffer[slot * 24 + idx] = 1.0f;
        }
    }

    // 2. Current trick slots: 4 slots x 24 one-hot = 96 (288..383)
    constexpr int trickOffset = 288;
    for (std::size_t slot = 0; slot < currTrick.size() && slot < 4; ++slot) {
        int idx = card_index(currTrick[slot]);
        if (idx >= 0 && idx < 24) {
            buffer[trickOffset + slot * 24 + idx] = 1.0f;
        }
    }

    // 3. Played cards history: 24 card types x 2 = 48 (384..431)
    // 24 card types x 2 (count >= 1, count == 2)
    constexpr int playedOffset = 384;
    int cardCounts[24] = {0};
    for (const card& c : trainingPlayedCards) {
        int idx = card_index(c);
        if (idx >= 0 && idx < 24) {
            cardCounts[idx]++;
        }
    }
    for (int c = 0; c < 24; ++c) {
        if (cardCounts[c] >= 1) {
            buffer[playedOffset + c * 2] = 1.0f;
        }
        if (cardCounts[c] == 2) {
            buffer[playedOffset + c * 2 + 1] = 1.0f;
        }
    }

    // 4. Trump suit: 4 one-hot (432..435) (all 0 if not yet chosen)
    constexpr int trumpOffset = 432;
    if (trainingTrump >= 0 && trainingTrump < 4) {
        buffer[trumpOffset + trainingTrump] = 1.0f;
    }

    // 5. Phase: 3 one-hot for phases 0, 1, 2 (436..438)
    constexpr int phaseOffset = 436;
    if (trainingPhase >= 0 && trainingPhase <= 2) {
        buffer[phaseOffset + trainingPhase] = 1.0f;
    }

    // 6. Current bid: trainingBid / 50.0f (439)
    buffer[439] = static_cast<float>(trainingBid) / 50.0f;

    // 7. Bid winner: 3 one-hot: Us (0/2), Them (1/3), None (-1) (440..442)
    constexpr int bidWinnerOffset = 440;
    if (trainingBidWinner == 0 || trainingBidWinner == 2) {
        buffer[bidWinnerOffset + 0] = 1.0f;
    } else if (trainingBidWinner == 1 || trainingBidWinner == 3) {
        buffer[bidWinnerOffset + 1] = 1.0f;
    } else {
        buffer[bidWinnerOffset + 2] = 1.0f;
    }

    // 8. Relative seat to dealer: 4 one-hot (player - trainingDealer + 4) % 4 (443..446)
    constexpr int relSeatOffset = 443;
    int relSeat = (player - trainingDealer + 4) % 4;
    buffer[relSeatOffset + relSeat] = 1.0f;

    // 9. Position in current trick: 4 one-hot currTrick.size() (447..450)
    constexpr int trickPosOffset = 447;
    int trickPos = static_cast<int>(currTrick.size());
    if (trickPos >= 0 && trickPos < 4) {
        buffer[trickPosOffset + trickPos] = 1.0f;
    }

    // 10. Is my turn: 1 float (451)
    buffer[451] = (trainingCurrentPlayer == player) ? 1.0f : 0.0f;

    // 11. Us meld: trainingUsMeld / 50.0f (452)
    buffer[452] = static_cast<float>(trainingUsMeld) / 50.0f;

    // 12. Them meld: trainingThemMeld / 50.0f (453)
    buffer[453] = static_cast<float>(trainingThemMeld) / 50.0f;

    // 13. Us trick points: trainingUsTrickPoints / 25.0f (454)
    buffer[454] = static_cast<float>(trainingUsTrickPoints) / 25.0f;

    // 14. Them trick points: trainingThemTrickPoints / 25.0f (455)
    buffer[455] = static_cast<float>(trainingThemTrickPoints) / 25.0f;

    // 15. Did anyone bid yet: trainingBidWinner != -1 ? 1.0f : 0.0f (456)
    buffer[456] = (trainingBidWinner != -1) ? 1.0f : 0.0f;

    // 16. Active bidders: 2 floats: fraction Us active, fraction Them active (457..458)
    int usActive = (trainingActiveBidders[0] ? 1 : 0) + (trainingActiveBidders[2] ? 1 : 0);
    int themActive = (trainingActiveBidders[1] ? 1 : 0) + (trainingActiveBidders[3] ? 1 : 0);
    buffer[457] = static_cast<float>(usActive) / 2.0f;
    buffer[458] = static_cast<float>(themActive) / 2.0f;
}

std::vector<float> Pin::training_observation(int player) const {
    std::vector<float> obs(OBSERVATION_SIZE);
    write_training_observation(player, obs.data());
    return obs;
}

int Pin::choose_training_trump(const std::vector<card>& cards) {
    int bestSuit = 0;
    int bestMeld = count_meld(cards, bestSuit, false);
    for (int suit = 1; suit < 4; ++suit) {
        const int meld = count_meld(cards, suit, false);
        if (meld > bestMeld) {
            bestMeld = meld;
            bestSuit = suit;
        }
    }
    return bestSuit;
}



int Pin::count_meld(std::vector<card> hand, int trump, bool verbose){ // should only pass meld hand in for actual playing becuase we want that element
/*
    Pinochle 4
    Double 30
    Around:
        Ace 10
        King 8
        Queen 6
        Jack 4
    Marriage 2/4
    Run 15
    9_t 1
*/
    // rank of suit
    // Clubs, Dia, Hearts, Spades
    std::map<int, std::map<int, int>> suitRank;
    std::map<int, int> suits;
    std::map<int, int> ranks;

    for (const auto& card : hand) {
        suitRank[card.suit][card.rank]++;
        suits[card.suit]++;
        ranks[card.rank]++;
    }
    int meldPoints = 0;


    // Run in trump: A 10 K Q J (15 14 13 12 11)
    std::vector<int> run = {15, 14, 13, 12, 11};
    bool hasRun = true;
    for (int r : run) {
        if (suitRank[trump][r] < 1) {
            hasRun = false;
            break;
        }
    }
    if (hasRun) {
        if (verbose) std::cout << "Run in Trump: +15" << std::endl;
        meldPoints += 15;
    }
    // Marriage
    for (int s = 0; s <= 3; ++s) {
        if (suitRank[s][13] > 0 && suitRank[s][12] > 0) {
            int pts = (s == trump) ? 4 : 2;
            if (suitRank[s][13] > 1 && suitRank[s][12] > 1) pts *= 2;
            if (verbose) {
                card che = {0, s,0};
                std::cout << "Marriage in " << che.p_suit() << ": +" << pts << std::endl;
            }
            meldPoints += pts;
        }
    }
    std::map<int, int> aroundPoints = {
        {15, 10}, // Aces
        {13, 8},  // Kings
        {12, 6},  // Queens
        {11, 4}   // Jacks
    };
    for (auto [r, points] : aroundPoints) {
        bool hasAll = true;
        for (int s = 0; s < 4; ++s) {
            if (suitRank[s][r] == 0) {
                hasAll = false;
                break;
            }
        }
        if (hasAll) {
            if (verbose) {
                card check = {r,0,0};
                std::cout << check.p_rank() <<"s around: +" << points << "\n";
            }
            meldPoints += points;
        }
    }

        // Pinochle: Q spades (12,3) + J diamonds (11,1)
        int q_spades = suitRank[3][12];
        int j_diamonds = suitRank[1][11];
        int pinochles = std::min(q_spades, j_diamonds);
        if (pinochles >= 1) {
            int pts = (pinochles == 2) ? 30 : 4 * pinochles;
            if (verbose) std::cout << (pinochles == 2 ? "Double " : "") << "Pinochle: +" << pts << "\n";
            meldPoints += pts;
        }

        // 9 of trump
        if (suitRank[trump][9] > 0) {
            int p = suitRank[trump][9];
            if (verbose) std::cout << "9 of trump x" << p << std::endl;
            meldPoints += p;
        }

        if (verbose) std::cout << "Total Meld Points: " << meldPoints << std::endl;


        return meldPoints;
}
