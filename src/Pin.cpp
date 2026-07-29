#include "Pin.h"
#include <exception>
#include <iostream>
#include <ostream>
#include <pstl/glue_algorithm_defs.h>
#include <vector>
#include <random>
#include <algorithm>
#include <map>



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

    currTrick.clear();
    trainingTrickPlayers.clear();
    usCards.clear();
    themCards.clear();
    usPoints = 0;
    themPoints = 0;
    trumpSuit = -1;
    bet = 20;
    betWinner = -1;
    tWinner = 2;
    trainingCurrentPlayer = 2;
    trainingPhase = 0;
}

std::vector<int> Pin::legal_training_actions() const {
    if (trainingPhase == 0) {
        return {12, 13, 14, 15, 16};
    }
    if (trainingPhase != 1) {
        return {};
    }

    std::vector<int> actions;
    const std::vector<card>& currentHand = *allHands.at(trainingCurrentPlayer);
    const std::vector<card> legal = getLegalCards(currentHand, currTrick, trumpSuit);
    for (int i = 0; i < static_cast<int>(currentHand.size()); ++i) {
        if (std::find(legal.begin(), legal.end(), currentHand[i]) != legal.end()) {
            actions.push_back(i);
        }
    }
    return actions;
}

TrainingStep Pin::step_training(int action) {
    TrainingStep result{0.0F, trainingPhase == 2};
    const std::vector<int> legalActions = legal_training_actions();
    if (std::find(legalActions.begin(), legalActions.end(), action) == legalActions.end()) {
        result.reward = -1.0F;
        return result;
    }

    if (trainingPhase == 0) {
        if (action == 12) {
            betWinner = (trainingCurrentPlayer + 1) % 4;
            trumpSuit = choose_training_trump(*allHands.at(betWinner));
        } else {
            betWinner = trainingCurrentPlayer;
            trumpSuit = action - 13;
        }
        tWinner = betWinner;
        trainingCurrentPlayer = tWinner;
        trainingPhase = 1;
    } else {
        play_training_card(trainingCurrentPlayer, action, result.reward);
    }

    result.terminated = trainingPhase == 2;
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

int Pin::training_trump() const {
    return trumpSuit;
}

int Pin::training_phase() const {
    return trainingPhase;
}

int Pin::training_current_player() const {
    return trainingCurrentPlayer;
}

int Pin::training_us_points() const {
    return usPoints;
}

int Pin::training_them_points() const {
    return themPoints;
}

void Pin::play_training_card(int player, int handIndex, float& reward) {
    std::vector<card>& playerHand = *allHands[player];
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
    const card winningCard = getWinningCard(currTrick, trumpSuit);
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
    if (hand.empty()) {
        ++trickPoints;
    }

    const bool ourTeam = winner == 0 || winner == 2;
    if (ourTeam) {
        usPoints += trickPoints;
        reward += static_cast<float>(trickPoints);
    } else {
        themPoints += trickPoints;
        reward -= static_cast<float>(trickPoints);
    }

    currTrick.clear();
    trainingTrickPlayers.clear();
    tWinner = winner;
    trainingCurrentPlayer = winner;
    if (hand.empty()) {
        trainingPhase = 2;
    }
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
