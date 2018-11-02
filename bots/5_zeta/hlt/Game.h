#pragma once

#include <vector>
#include "Constants.h"
#include "Player.h"
using namespace std;

class Game final {
  public:
    static int num_players;
    static int my_player_id;

    static vector<Player> players;
    static Player *me;
    
    static int turn;

    static int turns_left() { return Constants::MAX_TURNS - turn; }
};