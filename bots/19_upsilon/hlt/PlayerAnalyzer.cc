#include "PlayerAnalyzer.h"

#include "Game.h"
#include "Log.h"

vector<PosSet> PlayerAnalyzer::pid_to_unsafe_map;
vector<vector<int>> PlayerAnalyzer::pid_to_at_risk_sids;
vector<PlayerAnalyzer::PlayerStats> PlayerAnalyzer::pid_to_stats;

void PlayerAnalyzer::analyze()
{
    if (pid_to_unsafe_map.empty()) {
        pid_to_unsafe_map.resize(Game::num_players, PosSet(false));
        pid_to_at_risk_sids.resize(Game::num_players);
        pid_to_stats.resize(Game::num_players);
    }

    for (int pid = 0; pid < Game::num_players; ++pid) {
        // Do some analysis of the at-risk ships and unsafe squares we computed
        // last turn. Specifically, we are interested in how often the player
        // moved at-risk ships onto an unsafe square.
        vector<int> &at_risk_sids = pid_to_at_risk_sids[pid];
        PosSet &unsafe_map = pid_to_unsafe_map[pid];
        PlayerStats &stats = pid_to_stats[pid];
        const Player &player = Game::players[pid];

        for (int sid : at_risk_sids) {
            auto it = player.id_to_ship.find(sid);
            // TODO: could just count moves, not STILLs, to get a smaller denominator?
            if (it != player.id_to_ship.end()) {
                const Vec pos = it->second.pos;
                if (unsafe_map(pos)) {
                    stats.moved_to_unsafe_count += 1;
                    //Log::flog_color(pos, 255, 0, 0, "MOVED ONTO UNSAFE SQUARE!");
                    //Log::flog(player.shipyard, "ship %d moved unsafely!", sid);
                }
                stats.at_risk_count += 1;
            } else {  // ship no longer exists!
                // For now, let's assume that if an at-risk ship disappears, it
                // must have collided, so it must have moved unsafely. Some drawbacks:
                // - it might have actually made a dropoff. Could check this! But it's annoying and unlikely
                // - it might have made a sensible collision that even a cautious bot would do, like colliding
                //   on its own dropoff.
                stats.moved_to_unsafe_count += 1;
                stats.at_risk_count += 1;
                //Log::flog(player.shipyard, "ship %d disappeared, so probably moved unsafely!", sid);
            }
        }

        Log::flog(player.shipyard, "At risk moves: %d/%d unsafe", stats.moved_to_unsafe_count, stats.at_risk_count);

        // decide what squares are unsafe for this player to move onto this turn
        unsafe_map.fill(false);
        for (int enemy_pid = 0; enemy_pid < Game::num_players; ++enemy_pid) {
            if (pid == enemy_pid) continue;
            for (Ship enemy_ship : Game::players[enemy_pid].ships) {
                // set all adjacent squares unsafe
                for (int d = 0; d < 5; ++d) {
                    const Vec adj = grid.add(enemy_ship.pos, (Direction)d);
                    unsafe_map(adj) = true;
                }
            }
        }
        // Let's mark square adjacent to this player's structures as safe for them
        for (Vec s : player.structures) {
            for (int d = 0; d < 5; ++d) {
                const Vec adj = grid.add(s, (Direction)d);
                unsafe_map(adj) = false;
            }
        }
        /*if (pid == 1) {
            for (Vec pos : grid.positions) {
                if (unsafe_map(pos)) Log::flog_color(pos, 255, 0, 255, "UNSAFE FOR 1");
            } 
        }*/

        // Find the at-risk ships for this player on this turn.
        // We'll say an "at-risk" ship is one that's adjacent to an unsafe square,
        // but isn't currently on an unsafe square.
        at_risk_sids.clear();
        for (Ship s : player.ships) {
            // Who knows what a currently-unsafe ship might do!
            // Let's restrict our analysis to ships that are currently safe
            if (unsafe_map(s.pos)) continue;

            // let's say a ship adjacent to its own structure isn't at risk
            if (grid.smallest_dist(s.pos, player.structures) <= 1) continue;

            for (int d = 0; d < 4; ++d) {
                const Vec adj = grid.add(s.pos, (Direction)d);
                if (unsafe_map(adj)) {
                    at_risk_sids.push_back(s.id);
                    //Log::flog_color(s.pos, 255, 255, 0, "AT RISK!");
                    break;
                }
            }
        }
    }    
}


bool PlayerAnalyzer::player_is_cautious(int player_id)
{
    // in 2p, strict caution is a terrible strategy, so let's give people
    // the benefit of the doubt and assume they are not cautious
    if (Game::num_players == 2) return false;

    const PlayerStats &stats = pid_to_stats.at(player_id);
    return (stats.at_risk_count > 30) && (stats.moved_to_unsafe_count < 0.02 * stats.at_risk_count);
}