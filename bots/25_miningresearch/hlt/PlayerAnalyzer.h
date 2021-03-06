#pragma once

#include <set>
#include <vector>
#include "Command.h"
#include "PosMap.h"
using namespace std;

class PlayerAnalyzer final
{
  public:
    // called at the start of each turn
    static void analyze();

    // called at the end of each turn
    static void remember_my_commands(const vector<Command> &commands);

    // returns true if we're pretty confident that this player will
    // not move adjacent to one of our ships.
    static bool player_is_cautious(int player_id);

    // Make a guess about whether we should be scared of this player ramming
    // their_ship into my_ship.
    static bool player_might_ram(int player_id, Ship my_ship, Ship their_ship);

  private:
    static int get_num_collisions(int player_id);

    struct PlayerStats
    {
        int at_risk_count = 0;  // each turn for each ship of this player that could have moved to an unsafe square
        int moved_to_unsafe_count = 0;  // number of times one of this player's ships moved to an unsafe square

        int num_missed_rams = 0;  // number of times one of this player's ships tried to ram an enemy, but missed b/c the enemy fled

        int num_missing_ships = 0;  // number of ships of this player that have disappeared. should equal collision - dropoffs

        // when two adjacent enemy ships disappear, it's hard to know which one rammed the other one. But in
        // the case of our own ships, we do know whether our ship stayed still or moved. When one of our own ships
        // disappears, and it didn't make a dropoff, we know that an adjacent enemy ship must have rammed it. 
        // so here we'll count the number of times that one of our ships which was commanded to stay still disappeared,
        // and simultaneously an adjacent ship of this player disappeared
        int num_times_rammed_my_still_ship = 0;
    };

    struct PlayerData
    {
        PosSet unsafe_map;  // which squares were adjacent to enemies of this player on the previous turn
        PosSet enemy_ship_map;  // which squares were occupied by enemies of this player on the previous turn
        vector<Ship> ships;  // the player's ships on the previousu turn
        vector<int> at_risk_sids;  // sids of this player that were adjacent to an "unsafe" square last turn
        set<int> rammer_sids;  // sids of this player that tried to ram last turn, but missed
        PlayerStats stats;

        PlayerData() : unsafe_map(false), enemy_ship_map(false) {}
    };

    static vector<PlayerData> pid_to_data;
    static vector<Ship> my_still_ships;
};