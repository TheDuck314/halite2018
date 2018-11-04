#pragma once

#include <vector>
#include "PosMap.h"
using namespace std;

class PlayerAnalyzer final
{
  public:
    // called at the start of each turn
    static void analyze();

    // returns true if we're pretty confident that this player will
    // not move adjacent to one of our ships.
    static bool player_is_cautious(int player_id);

  private:
    struct PlayerStats
    {
        int at_risk_count = 0;
        int moved_to_unsafe_count = 0;
    };    

    static vector<PosSet> pid_to_unsafe_map;
    static vector<vector<int>> pid_to_at_risk_sids;
    static vector<PlayerStats> pid_to_stats;
};