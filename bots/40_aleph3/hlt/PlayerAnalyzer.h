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

    // Returns true if we think this enemy ship is deliberately blocking the way to
    // one of our strucutres.
    static bool ship_is_structure_blocker(int player_id, Ship enemy_ship);

    struct PlayerStats
    {
        int at_risk_count = 0;  // each turn for each ship of this player that could have moved to an unsafe square
        int moved_to_unsafe_count = 0;  // number of times one of this player's ships moved to an unsafe square
        // number of times one of this player's ships moved to a square adjacent to an enemy with less halite than that ship
        int moved_next_to_lower_halite_enemy_count = 0;  

        int num_missed_rams = 0;  // number of times one of this player's ships tried to ram an enemy, but missed b/c the enemy fled

        int num_missing_ships = 0;  // number of ships of this player that have disappeared. should equal collision - dropoffs

        // when two adjacent enemy ships disappear, it's hard to know which one rammed the other one. But in
        // the case of our own ships, we do know whether our ship stayed still or moved. When one of our own ships
        // disappears, and it didn't make a dropoff, we know that an adjacent enemy ship must have rammed it. 
        // so here we'll count the number of times that one of our ships which was commanded to stay still disappeared,
        // and simultaneously an adjacent ship of this player disappeared
        int num_times_rammed_my_still_ship = 0;
    };

    static const PlayerStats& getStats(int pid) { return pid_to_data.at(pid).stats; }

  private:
    static int get_num_collisions(int player_id);

    struct PlayerData
    {
        PosSet unsafe_map;  // which squares were adjacent to enemies of this player on the previous turn
        PosSet enemy_ship_map;  // which squares were occupied by enemies of this player on the previous turn
        vector<Ship> ships;  // the player's ships on the previousu turn
        vector<int> at_risk_sids;  // sids of this player that were adjacent to an "unsafe" square last turn
        set<int> rammer_sids;  // sids of this player that tried to ram last turn, but missed
        // for each ship of this player, how many turns in a row it's been blocking one of our dropoffs
        map<int, int> sid_to_consecutive_blocking_turns;
        set<int> structure_blocker_sids;
        // for each square, the minimum halite of all adjacent enemy ships on the previous turn, or 9999 if there
        // were no adjacent enemy ships
        PosMap<int> min_adjacent_enemy_ship_halite;
        
        PlayerStats stats;

        PlayerData() : unsafe_map(false), enemy_ship_map(false), min_adjacent_enemy_ship_halite(-1) {}
    };

    static vector<PlayerData> pid_to_data;
    static vector<Ship> my_still_ships;
};
