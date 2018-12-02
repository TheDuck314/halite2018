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

    static vector<Vec> enemy_structures;
    static vector<Vec> enemy_shipyards;
    static vector<Ship> enemy_ships;

    // update auxiliary structures after basic info is initialized at the beginning of the turn
    // must be called after Player::post_update() is called on all players
    static void post_update();

    static int turns_left() { return Constants::MAX_TURNS - turn; }
};