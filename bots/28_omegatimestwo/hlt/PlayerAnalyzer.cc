#include "PlayerAnalyzer.h"

#include "Game.h"
#include "Log.h"

vector<PlayerAnalyzer::PlayerData> PlayerAnalyzer::pid_to_data;
vector<Ship> PlayerAnalyzer::my_still_ships;

void PlayerAnalyzer::analyze()
{
    if (pid_to_data.empty()) {
        // first turn initialization
        Log::log("initing pid_to_data, grid.width = %d", grid.width);
        pid_to_data.resize(Game::num_players);
    }

    // check whether any of our stationary ships have disappeared. If so, find any
    // previously-adjacent enemy ships that also disappeared and assume they rammed us
    for (Ship my_still_ship : my_still_ships) {
        if (Game::me->id_to_ship.count(my_still_ship.id)) continue;  // my ship is still alive
        // my ship is gone. assume we got rammed (currently not going to worry about the
        // case where we actually built a structure)
        for (int pid = 0; pid < Game::num_players; ++pid) {
            if (pid == Game::my_player_id) continue;
            const vector<Ship> &prev_enemy_ships = pid_to_data[pid].ships;
            for (Ship prev_enemy_ship : prev_enemy_ships) {
                if (Game::players[pid].id_to_ship.count(prev_enemy_ship.id)) continue;  // enemy ship is still there, so didn't ram us
                if (grid.dist(prev_enemy_ship.pos, my_still_ship.pos) > 1) continue;  // enemy ship was too far away to ram us
                // OK, this enemy ship probably rammed us. 
                pid_to_data[pid].stats.num_times_rammed_my_still_ship += 1;
            }
        }
    }

    for (int pid = 0; pid < Game::num_players; ++pid) {
        // Do some analysis of the at-risk ships and unsafe squares we computed
        // last turn. Specifically, we are interested in how often the player
        // moved at-risk ships onto an unsafe square.
        PlayerData &data = pid_to_data[pid];
        vector<Ship> &player_ships = data.ships;
        vector<int> &at_risk_sids = data.at_risk_sids;
        PosSet &unsafe_map = data.unsafe_map;
        PosSet &enemy_ship_map = data.enemy_ship_map;
        set<int> &rammer_sids = data.rammer_sids;
        map<int, int> &sid_to_consecutive_blocking_turns = data.sid_to_consecutive_blocking_turns;
        PlayerStats &stats = data.stats;
        const Player &player = Game::players[pid];

        /////////////////////////// ANALYZE THINGS THAT HAPPENED LAST TURN //////////////////////////////

        // count the number of ships of this player that moved to an unsafe square
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

        // count the number of ships of this player that attempted to ram, but missed b/c the enemy fled
        // also, remember the ids of those ships cause they'll probably make another attempt this turn
        rammer_sids.clear();
        for (Ship s : player.ships) {
            if (enemy_ship_map(s.pos)) {
                stats.num_missed_rams += 1;
                rammer_sids.insert(s.id);
                //Log::flog_color(s.pos, 0, 0, 255, "missed ram by player %d", pid);
            }
        }

        // count the number of ships of this player that have disappeared. They must have either collided
        // or converted to a dropoff
        set<int> current_sids;
        for (Ship s : player.ships) current_sids.insert(s.id);
        for (Ship prev_ship : player_ships) {
            if (!current_sids.count(prev_ship.id)) {
                stats.num_missing_ships += 1;
            }
        }

        Log::flog(player.shipyard, "At risk moves: %d/%d unsafe", stats.moved_to_unsafe_count, stats.at_risk_count);
        Log::flog(player.shipyard, "num_missed_rams = %d", stats.num_missed_rams);
        Log::flog(player.shipyard, "get_num_collisions() = %d", get_num_collisions(pid));
        Log::flog(player.shipyard, "num_times_rammed_my_still_ship = %d", stats.num_times_rammed_my_still_ship);

        /////////////////////////// PREPARE THE DATA THAT WE WILL USE NEXT TURN //////////////////////////////

        // decide what squares are unsafe for this player to move onto this turn.
        // also, record which squares were occupied by enemy ships this turn
        unsafe_map.fill(false);
        enemy_ship_map.fill(false);
        for (int enemy_pid = 0; enemy_pid < Game::num_players; ++enemy_pid) {
            if (pid == enemy_pid) continue;
            for (Ship enemy_ship : Game::players[enemy_pid].ships) {
                // set all adjacent squares unsafe
                for (int d = 0; d < 5; ++d) {
                    const Vec adj = grid.add(enemy_ship.pos, (Direction)d);
                    unsafe_map(adj) = true;
                }
                enemy_ship_map(enemy_ship.pos) = true;
            }
        }
        // Let's mark squares adjacent to this player's structures as safe for them
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

        // remember all the player's ships as of this turn
        player_ships = player.ships;

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

        /////////////////////////// LOOK FOR ENEMY BLOCKING BEHAVIOR //////////////////////////////
        if (pid != Game::my_player_id) {
            data.structure_blocker_sids.clear();

            map<Vec, int> my_structure_to_num_blockers;
            for (Vec v : Game::me->structures) my_structure_to_num_blockers[v] = 0;

            for (Ship s : player.ships) {
                // First count how many turns this ship has been hanging around one of our dropoffs
                Vec my_structure = grid.closest(s.pos, Game::me->structures);
                const bool blocking = (grid.dist(s.pos, my_structure) <= 3) && 
                                      (grid.smallest_dist(s.pos, player.structures) > 3);
                auto it = sid_to_consecutive_blocking_turns.find(s.id);
                if (it == sid_to_consecutive_blocking_turns.end()) {
                    sid_to_consecutive_blocking_turns[s.id] = (blocking ? 1 : 0);
                } else {
                    if (blocking) it->second += 1;
                    else it->second = 0;
                }
                if (blocking) {
                    Log::flog_color(s.pos, 0, 0, 255, "BLOCKER!");
                }
                Log::flog(s.pos, "sid_to_consecutive_blocking_turns[%d] = %d", s.id, sid_to_consecutive_blocking_turns[s.id]);

                if (sid_to_consecutive_blocking_turns[s.id] > 20) {
                    // this guy is a long-term blocker. add to the blocker count of my_structure
                    my_structure_to_num_blockers[my_structure] += 1;
                }
            }

            for (const pair<Vec, int> &struct_and_num_blockers : my_structure_to_num_blockers) {
                const Vec my_structure = struct_and_num_blockers.first;
                const int num_blockers = struct_and_num_blockers.second;
                if (num_blockers < 2) continue;  // no real blocking problem

                // Player pid is trying to block this structure. Designate all his nearby ships as blockers
                for (Ship s : player.ships) {
                    if (grid.dist(s.pos, my_structure) <= 4) {
                        Log::flog_color(s.pos, 255, 0, 0, "REAL BAD BLOCKER!");
                        data.structure_blocker_sids.insert(s.id);
                    }
                }
            }
        }
    }    
}

void PlayerAnalyzer::remember_my_commands(const vector<Command> &commands)
{
    my_still_ships.clear();
    for (Command c : commands) {
        if (c.type == Command::Type::Move && c.move_dir == Direction::Still) {
            const Ship ship = Game::me->id_to_ship.at(c.ship_id);
            my_still_ships.push_back(ship);
            //Log::flog_color(ship.pos, 0, 255, 0, "Commanding this ship to stay still");
        }
    }
}

int PlayerAnalyzer::get_num_collisions(int player_id)
{
    return pid_to_data.at(player_id).stats.num_missing_ships - Game::players.at(player_id).dropoffs.size();
}

bool PlayerAnalyzer::player_is_cautious(int player_id)
{
    // in 2p, strict caution is a terrible strategy, so let's give people
    // the benefit of the doubt and assume they are not cautious
    if (Game::num_players == 2) return false;

    const PlayerStats &stats = pid_to_data.at(player_id).stats;
    return (stats.at_risk_count > 30) && (stats.moved_to_unsafe_count < 0.02 * stats.at_risk_count);
}

bool PlayerAnalyzer::player_might_ram(int player_id, Ship my_ship, Ship their_ship)
{
    const PlayerData &data = pid_to_data.at(player_id);

    // first, if this ship tried to ram last turn it seems very likely that it will try again
    // this turn
    if (data.rammer_sids.count(their_ship.id)) return true;

    // check if they're the ramming sort of person
    if (data.stats.num_times_rammed_my_still_ship < 3) return false;

    // they totally are. let's impose some plausible conditions for when they might ram
    return (their_ship.halite < 500) && (my_ship.halite > their_ship.halite);
}

bool PlayerAnalyzer::ship_is_structure_blocker(int player_id, Ship enemy_ship)
{
    return pid_to_data.at(player_id).structure_blocker_sids.count(enemy_ship.id) > 0;
}
