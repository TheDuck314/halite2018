#include <algorithm>
#include <cmath>
#include <deque>
#include <queue>
#include <vector>
#include <set>
#include <unordered_set>
#include <iostream>
#include "hlt/Constants.h"
#include "hlt/Game.h"
#include "hlt/Network.h"
#include "hlt/Grid.h"
#include "hlt/Log.h"
#include "hlt/Parameter.h"
#include "hlt/Path.h"
#include "hlt/Plan.h"
#include "hlt/PlayerAnalyzer.h"
#include "hlt/PosMap.h"
#include "hlt/Util.h"
#include "hlt/PredictorDataCollector.h"
#include "hlt/Predictor.h"
using namespace std;

PARAM(double, MINE_MOVE_ON_MULT);
PARAM(double, MINE_RATE_MULT);
PARAM(double, LOW_HALITE_EFFICIENCY_PENALTY);
PARAM(double, MINING_DIST_FROM_ME_MULT);
PARAM(double, MIN_HALITE_PER_SHIP_TO_SPAWN_4P);
PARAM(double, DROPOFF_MIN_NEW_HALITE_IN_RADIUS);
PARAM(double, ENEMY_SHIP_HALITE_FRAC);
PARAM(double, DROPOFF_PENALTY_FRAC_PER_DIST);
PARAM(double, MINING_INSPIRATION_MULT_2P);
PARAM(double, MINING_INSPIRATION_MULT_4P);
PARAM(double, DROPOFF_ENEMY_BASE_DIST_BONUS_MAX);
PARAM(double, DROPOFF_ENEMY_BASE_DIST_BONUS_FALLOFF_PER_DIST);
PARAM(double, RETURN_TARGET_DIST_PENALTY);
PARAM(double, MIN_ENEMY_HALITE_TO_RAM_4P);
PARAM(double, DROPOFF_SHIP_DIST_PENALTY);
PARAM(double, DROPOFF_ENEMY_DIST_PENALTY);
PARAM(double, MINING_RETURN_RICHNESS_BONUS_MULT);
PARAM(double, PASSABLE_SAFETY_THRESH_4P);
PARAM(int, MIN_DROPOFF_SEPARATION);
PARAM(int, MAX_TURNS_LEFT_TO_RAM_4P);
PARAM(int, FIGHT_COUNT_RADIUS_2P);
PARAM(int, MAX_HALITE_TO_HELP_IN_FIGHT_2P);
PARAM(int, MAX_HALITE_TO_HELP_IN_FIGHT_4P);
PARAM(int, MAX_HALITE_TO_RAM_2P);
PARAM(int, HALITE_DIFF_CAUTION_THRESH_2P);
PARAM(int, DEBUG_MAX_SHIPS);
PARAM(int, RETURN_HALITE_THRESH);
PARAM(int, SHIPS_FOR_FIRST_DROPOFF);
PARAM(int, SHIPS_PER_LATER_DROPOFF);
PARAM(bool, CONSIDER_RAMMING_IN_4P);
PARAM(bool, DONT_MINE_RAM_TARGETS);
PARAM(bool, DONT_BUILD_NEAR_ENEMY_STRUCTURES);
PARAM(bool, DONT_BUILD_NEAR_ENEMY_SHIPYARDS);
PARAM(bool, LOW_HALITE_PATHFINDING_FOR_NON_RETURNERS);
PARAM(bool, DO_DROPOFF_BLOCKING);

PARAM(string, TRAINING_DATA_SAVE_FILENAME);
PARAM(bool, JUST_MAKE_TRAINING_DATA);
PARAM(bool, DONT_PREDICT);
PARAM(string, PREDICTOR_MODEL);

struct Bot {
    // GLOBAL STATE
    vector<Plan> plans;  // cleared each turn
    set<int> busy_ship_ids;   // ships that shouldn't be assigned new plans. cleared each turn
    unordered_set<int> returning_ship_ids; // maintained across turned
    bool want_dropoff = false;  // recomputed by consider_making_dropoff() each turn
    Vec planned_dropoff_pos;
    map<int, PosSet> sid_to_impassable;  // recomputed by set_impassable each turn
    map<Vec, double> safety_map;
    PosSet ram_targets; // squares we are ramming this turn
    bool still_spawning_ships = true;

    PredictorDataCollector predictor_data_collector;
    Predictor predictor;

    // GLOBAL CONSTANTS
    const int TURNS_LEFT_TO_RETURN = 15;  // TUNE
    const int return_halite_thresh;

    Bot(int argc, char **argv)
      : ram_targets(false),
        predictor(predictor_data_collector, PREDICTOR_MODEL.get("./safety_model.pt")),
        return_halite_thresh(RETURN_HALITE_THRESH.get(950))        
    {
    }

    void plan(const Ship &ship, Vec tgt, Purpose purpose)
    {
        for (Plan p : plans) {
            if (p.ship.id == ship.id) {
                Log::die("attempt to add duplicate plan ship=%s tgt=%s purpose=%s but that ship already has plan %s",
                    +ship.toString(), +tgt.toString(), +desc(purpose), +p.toString());
            }
        }
        plans.push_back({ship, tgt, purpose});
    }

    // whether a ship has enough halite to move
    bool can_move(Ship ship)
    {
        return grid(ship.pos).halite / Constants::MOVE_COST_RATIO <= ship.halite;
    }

    // how much halite a ship will mine from a square with the given amount of
    // halite, assuming the ship has room and is not inspired
    int mined_halite(int square_halite)
    {
        return (square_halite + Constants::EXTRACT_RATIO - 1) / Constants::EXTRACT_RATIO;  // divide and round up
    }

    bool want_more_ships()
    {
        if ((int)Game::me->ships.size() >= DEBUG_MAX_SHIPS.get(99999)) return false;

        int halite_left = 0;
        for (Vec pos : grid.positions) halite_left += grid(pos).halite;

        // check if we already have plenty of ships
        if (Game::num_players != 2) {
            if (Game::turns_left() < 200) return false;  // TUNE
            if (halite_left < MIN_HALITE_PER_SHIP_TO_SPAWN_4P.get(2000) * (int)Game::me->ships.size()) return false;  // TUNE
        } else { // two players
            if (Game::turns_left() < 140) return false;  // TUNE
            if (halite_left < 2500 * (int)Game::me->ships.size()) return false;  // TUNE
        }

        return true;
    }

    bool can_afford_a_ship()
    {
        int min_halite_to_spawn = Constants::SHIP_COST;

        // if we want to build a dropoff but we didn't manage to do it this turn, then
        // only build a ship if we'll have enough halite left after building the ship
        // to also build a dropoff
        if (want_dropoff) min_halite_to_spawn += Constants::DROPOFF_COST;
        return Game::me->halite >= min_halite_to_spawn;
    }

    void predict_safety()
    {
        if (DONT_PREDICT.get(false)) return;
        predictor.predict(safety_map);
    }

    bool we_win_fight_2p(Vec pos, bool higher_caution)
    {
        constexpr int max_R = 8;  // FIXME
        vector<int> num_enemies_within(max_R+1, 0);
        for (Ship e : Game::enemy_ships) {
            int dist = grid.dist(e.pos, pos);
            while (dist <= max_R) num_enemies_within[dist++] += 1;
        }

        bool exclude_returners = Game::me->dist_from_structure(pos) > 8;  // TUNE
        vector<int> num_allies_within(max_R+1, 0);
        for (Ship ally : Game::me->ships) {
            if (exclude_returners && returning_ship_ids.count(ally.id)) continue;
            if (ally.halite > MAX_HALITE_TO_HELP_IN_FIGHT_2P.get(1000)) continue;

            int dist = grid.dist(ally.pos, pos);
            while (dist <= max_R) num_allies_within[dist++] += 1;
        }

        // this doesn't matter in local play but I have seen too many live replays
        // where I get into a ramming war within 2 squares of an enemy structure:
        int enemy_factor = 1;
        if (grid.smallest_dist(pos, Game::enemy_structures) <= 2) enemy_factor = 2;

        if (higher_caution) {
            return num_allies_within[8] > enemy_factor * num_enemies_within[8];
        } else {
            return num_allies_within[8] >= enemy_factor * num_enemies_within[8];
        }
    }


    bool we_win_fight_4p(Vec pos)
    {
        if (Game::turns_left() > MAX_TURNS_LEFT_TO_RAM_4P.get(200)) return false;

        const int R = 4;  // TUNE
        const int num_enemies = grid.num_within_dist(pos, Game::enemy_ships, R);

        bool exclude_returners = true; // grid.smallest_dist(pos, Game::me->structures) > R;

        int num_allies = 0;
        for (Ship ally : Game::me->ships) {
            if (grid.dist(ally.pos, pos) > R) continue;
            if (ally.halite > MAX_HALITE_TO_HELP_IN_FIGHT_4P.get(600)) continue;  // TUNE
            if (exclude_returners && returning_ship_ids.count(ally.id)) continue;
            num_allies += 1;
        }

        // TUNE
        return  (num_enemies <= 1 && num_allies >= 2) ||
                (num_enemies == 2 && num_allies >= 4) ||
                (num_enemies == 3 && num_allies >= 6);
    }

    void set_impassable_around_structure_blocker(PosMap<bool> &impassable, const Player &enemy, Ship enemy_ship)
    {
        // current strategy for dealing with structure blockers:
        // 1. Never consider adjacent squares impassable; that way we'll try to squirm around them
        // 2. Consider the enemy ship itself passable if we have enough nearby ships
        const int R = 10;
        const int num_nearby_allies = grid.num_within_dist(enemy_ship.pos, Game::me->ships, R);
        const int num_nearby_enemies = grid.num_within_dist(enemy_ship.pos, enemy.ships, R);
        impassable(enemy_ship.pos) = (num_nearby_enemies > num_nearby_allies);
    }
    
    template<typename T>
    struct Memo
    {
        bool computed = false;
        T value;

        void set(T _value)
        {
            value = _value;
            computed = true;
        }
    };

    void set_impassable()
    {
        if (Game::num_players != 2) predict_safety();

        sid_to_impassable.clear();

        PosMap<Memo<bool>> we_win_fight_4p_cache{Memo<bool>()};

        PosMap<int> allied_ship_id_at(-1);
        for (Ship s : Game::me->ships) allied_ship_id_at(s.pos) = s.id;

        for (Ship my_ship : Game::me->ships) {
            PosSet &impassable = sid_to_impassable.emplace(my_ship.id, PosSet(false)).first->second;

            for (const Player &enemy : Game::players) {
                if (enemy.id == Game::my_player_id) continue;
                const bool enemy_is_cautious = PlayerAnalyzer::player_is_cautious(enemy.id);
                for (Ship enemy_ship : enemy.ships) {
                    // we need to ignore enemy ships that are on top of our structures
                    // or they'll block us from delivering. it's fine to ram them.
                    if (Game::me->has_structure_at(enemy_ship.pos)) continue;

                    if (enemy_is_cautious) {  // currently only ever true in 4p
                        // This player generally doesn't move adjacent to opponents' ships.
                        // Let's only rely on this if they are currently not adjacent to one
                        // of their structures and also not adjacent to any of our ships (because
                        // in those special situations who knows what crazy stuff they might do).
                        const int dist_from_enemy_base = enemy.dist_from_structure(enemy_ship.pos);
                        const int dist_from_me = grid.smallest_dist(enemy_ship.pos, Game::me->ships);
                        if (dist_from_enemy_base > 2 && dist_from_me > 1) {
                            // set the enemy ship itself as impassable, but not the adjacent squares
                            impassable(enemy_ship.pos) = true;
                            continue;
                        }
                    }
                    // if we get here that enemy_is_cautious stuff didn't apply.

                    // handle SiestaGuru's 4p structure blockers.
                    if (Game::num_players != 2 && PlayerAnalyzer::ship_is_structure_blocker(enemy.id, enemy_ship)) {
                        set_impassable_around_structure_blocker(impassable, enemy, enemy_ship);
                        continue;
                    }

                    const bool enemy_can_move = can_move(enemy_ship);
                    for (int d = 0; d < 5; ++d) {
                        // if the enemy is unable to move we won't consider adjacent squares impassable
                        // effect of adding this leans positive, but probably noise: 2p:  488-429    4p:  481-474-422-432
                        if (!enemy_can_move && (Direction)d != Direction::Still) continue;

                        const Vec pos = grid.add(enemy_ship.pos, (Direction)d);

                        // If this is our structure don't avoid it just because there's an
                        // adjacent enemy ship
                        if (Game::me->has_structure_at(pos)) continue;

                        if (Game::num_players == 2) {
                            // high positive HALITE_DIFF_CAUTION_THRESH_2P means we will be less cautious, e.g. only when we have 900 and they have 100
                            // high negative HALITE_DIFF_CAUTION_THRESH_2P means we will be more cautious, e.g. even when we have 100 and they have 900
                            const bool higher_caution = my_ship.halite - enemy_ship.halite > HALITE_DIFF_CAUTION_THRESH_2P.get(0);
                            impassable(pos) |= !we_win_fight_2p(pos, higher_caution);
    //                        if (impassable(pos)) Log::flog_color(pos, 125, 0, 0, "impassable");
    //                        else Log::flog_color(pos, 0, 125, 0, "passable");
                        } else {  // 4-player games
                            // TEST!!
                            // If the safety predictor is pretty confident that this square is OK, leave it passable.
                            // TODO: this obviously ought to depend on my halite
                            // TODO: maybe it should depend on opponent's halite
                            // TODO: probably it should depend on # of turns left (since it's ok to lose a ship late)
                            // TODO: maybe it should depend on halite of adjacent enemies?
                            // TODO: for miners, this shouldn't necessarily be a hard passability thing, but a penalty on the square
                            // TODO: possibly this should take over from PlayerAnalyzer::player_might_ram
                            auto safety_it = safety_map.find(pos);
                            if (safety_it != safety_map.end()) {
                                const double predicted_safety = safety_it->second;
                                if (predicted_safety >= PASSABLE_SAFETY_THRESH_4P.get(0.98)) {
                                    // leave this square passable
                                    continue;
                                }
                            }

                            const int allied_ship_id = allied_ship_id_at(pos);
                            if (allied_ship_id != -1) {
                                // There's an allied ship here.
                                // In most cases, we don't expect an enemy to ram our ships in 4p
                                // Let's just check with the PlayerAnalyzer to see if it agrees
                                const Ship allied_ship = Game::me->id_to_ship.at(allied_ship_id);
                                if (!PlayerAnalyzer::player_might_ram(enemy.id, allied_ship, enemy_ship)) {
                                    // The PlayerAnalyzer doesn't expect the enemy to ram, so we won't set
                                    // this location impassable
                                    //if (my_ship.id == 2) Log::flog_color(pos, 255, 0, 0, "maybe safe");
                                    continue;
                                }
                            }

                            // messy memoization for speed :(
                            Memo<bool> &memo = we_win_fight_4p_cache(pos);
                            if (!memo.computed) memo.set(we_win_fight_4p(pos));
                            impassable(pos) |= !memo.value;  // == !we_win_fight_4p(pos)
                        }
                    }

                    // in 4p, if we think someone is going to ram us, don't go near them. also, we set
                    // adjacent squares impassable so consider_fleeing will flee. This isn't obviously 
                    // correct: if we get to this point and these squares are still passable it's because
                    // we_win_fight_4p is true, so maybe it's fine to be rammed.
                    // NOTE: commenting this out for now, not sure if it's a good idea or not. something 
                    // more restrictive might be good: if this guy tried to ram last turn, and has little
                    // halite, and is next to us, then fleeing seems good.
                    /*if (Game::num_players != 2 && PlayerAnalyzer::player_might_ram(enemy.id, my_ship, enemy_ship)) {
                        for (int d = 0; d < 5; ++d) {
                            const Vec pos = grid.add(enemy_ship.pos, (Direction)d);
                            impassable(pos) = true;
                        }
                    }*/

                    // don't ram when we have high halite
                    if (Game::num_players == 2) {
                        // don't ram if either
                        // - we have more than 500 halite or
                        // - we have more halite than the guy we would ram -- but add on half the halite on the square
                        //   he's standing on so we don't let low halite enemies freely mine high-halite squares
                        const int max_halite_to_ram = min(MAX_HALITE_TO_RAM_2P.get(500),
                                                          enemy_ship.halite + (grid(enemy_ship.pos).halite / 2));  // TUNE?
                        if (my_ship.halite > max_halite_to_ram) {
                            impassable(enemy_ship.pos) = true;
                        }
                    }

                    // in 4p, don't ram enemies with too little halite.
                    // there's no evidence that this is actually good
                    if (Game::num_players != 2 && enemy_ship.halite < MIN_ENEMY_HALITE_TO_RAM_4P.get(300)) {
                        impassable(enemy_ship.pos) = true;
                    }
                } // end loop over enemy ships

                // Consider enemy structures impassable if there is an adjacent ship of that player, 
                // because there's a good chance that ship will hit us if we move onto the structure.
                for (Vec s : enemy.structures) {
                    if (grid.smallest_dist(s, enemy.ships) <= 1) {
                        impassable(s) = true;
                    }
                }
            }
        }
    }

    // the normal system for resolving plans into commands assumes that we will
    // move at most one ship onto a given square this turn. This isn't true at the
    // end when we're collapsing all our ships down onto structures. So let's just
    // generate the commands for those guys now, and remove them from the list of
    // regular plans. That way resolve_plans_into_commands() doesn't have to worry
    // about them.
    void make_special_final_collapse_commands(vector<Command> &commands)
    {
        if (Game::turns_left() > TURNS_LEFT_TO_RETURN + 1) return;

        vector<Plan> remaining_plans;
        for (Plan p : plans) {
            Vec structure = grid.closest(p.ship.pos, Game::me->structures);
            if (p.ship.pos == structure && !Game::enemy_ships.empty()) {
                // We're on top of a structure! Let's look for any adjacent enemy ships
                // and try to ram them. This might help deal with end-game blockers.
                const Ship closest_enemy = grid.closest(p.ship.pos, Game::enemy_ships);
                if (grid.dist(p.ship.pos, closest_enemy.pos) == 1) {
                    // there's an adjacent enemy. ram!!!
                    commands.push_back(Command::move(p.ship, grid.adj_dir(p.ship.pos, closest_enemy.pos)));
                } else {
                    // no adjacent enemy. stay put
                    commands.push_back(Command::move(p.ship, Direction::Still));
                }
            } else if (grid.dist(p.ship.pos, structure) <= 1 && can_move(p.ship)) {
                // We're adjacent to a structure (or we're on top of a structure and there are no enemy ships).
                // Move onto the structure, heedless of any possible collision.
                Direction d = grid.adj_dir(p.ship.pos, structure);
                commands.push_back(Command::move(p.ship, d));
            } else {
                // We're not on or adjacent to a structure. Stick to whatever the original plan was.
                remaining_plans.push_back(p);
            }
        }
        plans = remaining_plans;
    }


    void resolve_plans_into_commands(vector<Command> &commands)
    {
        deque<Plan> q;
        std::copy(plans.begin(), plans.end(), std::back_inserter(q));

        map<Vec, Plan> dest_to_plan;

        while (!q.empty()) {
            Plan p = q.front();
            q.pop_front();
//            Log::flog(p.ship.pos, "popped %s", +p.toString());

            // decide where this ship should move to act on its plan.
            // can only pick a move that doesn't interfere with an existing move
            // of an earlier-priority plan. If there's no such move, we'll pick
            // a new earlier-priority purpose.
            Vec move_dest;
            if (p.tgt == p.ship.pos) {  // simplest case: plan calls for staying still
                move_dest = p.tgt;
            } else {  // plan calls for moving to some target
                // Plan a path that avoids any plans of equal or earlier priority.
                PosSet blocked_for_pathing(sid_to_impassable.at(p.ship.id));
                PosSet prefer_to_avoid(false);

                // consider_fleeing() may decide to flee onto a nominally "impassable" square.
                // Here we should just trust it and not consider any square impassable for Fleeing.
                if (p.purpose == Purpose::Flee) {
                    blocked_for_pathing.fill(false);
                }
                
                // Returners should consider enemy ships blocked, so they don't just casually ram them.
                if (p.purpose == Purpose::Return) {
                    for (Ship e : Game::enemy_ships) {
                        if (Game::me->has_structure_at(e.pos)) continue;  // never consider our structures blocked
                        blocked_for_pathing(e.pos) = true;
                    }
                }

                // If we're evading a returner (which currently means just swapping with them)
                // then don't consider their square blocked, even if it's, say, adjacent to an
                // enemy. It's fine to risk an enemy collision in this case
                if (p.purpose == Purpose::EvadeReturner) blocked_for_pathing(p.tgt) = false;

                // don't path over the dests of earlier priority plans
                for (const pair<Vec, Plan> &vp : dest_to_plan) {
                    if (vp.second.purpose <= p.purpose) {
                        // if you're a Returner, don't consider Stuck guys as obstacles
                        if (p.purpose == Purpose::Return && vp.second.purpose == Purpose::Stuck) {
                            continue;
                        }

                        // if you're a Miner or Returner, don't consider returners' paths blocked except for adjacent squares.
                        // For Miners, if you run into them you'll just switch places; there's no point
                        // trying to path around them. For Returners you'll probably never run into them. 
                        // might want to expand this to other cases? like, no point pathing around any very distant ships?
                        if ((p.purpose == Purpose::Mine || p.purpose == Purpose::Return) && 
                            vp.second.purpose == Purpose::Return && 
                            grid.dist(vp.first, p.ship.pos) > 1) {
                            continue;
                        }

                        // otherwise, don't path over this square
                        blocked_for_pathing(vp.first) = true;
                    }
                }

                Direction path_dir;
                if (p.purpose == Purpose::Return) {
                    // Returners use special pathing that finds the lowest halite minimum distance
                    // path so they burn less halite
                    path_dir = Path::find_low_halite(p.ship.pos, p.tgt, blocked_for_pathing);
                } else {
                    if (LOW_HALITE_PATHFINDING_FOR_NON_RETURNERS.get(false)) { // this looks bad, especially in 4p
                        path_dir = Path::find_low_halite(p.ship.pos, p.tgt, blocked_for_pathing);
                    } else {
                        path_dir = Path::find(p.ship.pos, p.tgt, blocked_for_pathing, prefer_to_avoid);
                    }
                }
                move_dest = grid.add(p.ship.pos, path_dir);
            }
    //        Log::flog(p.ship, "move_dest = %s", +move_dest.toString());
            
            // Check if anyone else is trying to move to the same square
            auto conflict_it = dest_to_plan.find(move_dest);
            if (conflict_it != dest_to_plan.end()) {  // someone is
                Plan conflict = conflict_it->second;

                // Returners don't bother to pathfind around Stuck ships. If our path tells us to 
                // move onto a Stuck ship, we should instead stay still and say that we are Stuck too.
                if (conflict.purpose == Purpose::Stuck) {
                    Log::flog_color(p.ship.pos, 0, 255, 0, "piling up behind a Stuck");
                    Log::flog_color(move_dest, 0, 0, 255, "I am the Stuck");
                    p.purpose = Purpose::Stuck;
                    p.tgt = p.ship.pos;
                    q.push_back(p);
                    continue;
                }

                // If we came up with a non-Still move, it's guaranteed to avoid intefering
                // with any plans of equal or earlier priority. But it's possible that we're
                // staying still and that will interfere with a plan of earlier priority.
                if (move_dest == p.ship.pos && conflict.purpose < p.purpose) {
                    // It happened. We want to try to get out of that plan's way. Do this by promoting
                    // ourselves to an evasion purpose with an earlier priority.
                    // NOTE: I think currently this should never happen with conflict.purpose == Purpose::Flee,
                    // because we currently never flee onto a square occupied by an ally
                    if (!(p.purpose == Purpose::Mine && conflict.purpose == Purpose::Return)) {
                        Log::die("case AAA I thought shouldn't happen id1=%d id2=%d", p.ship.id, conflict.ship.id);
                    }
                    // for now, let's just swap places with the guy who's coming onto our square.
                    // this should be guaranteed to succeed.
                    // TODO: It won't be guaranteed if, in the future, we have a priority that's earlier than
                    // Return and can move onto a Returner's square. For example we should make Flee do this.
                    p.purpose = Purpose::EvadeReturner;
                    p.tgt = conflict.ship.pos;
                    q.push_back(p);
    //                Log::flog(p.ship, "can't stay still b/c of ship id %d. going to evade.", conflict.ship.id);
                    continue;
                }

                // The other possibilities are:
                // - we are staying still and have equal or earlier priority to the conflicting plan
                // - we are moving, in which case we made sure that any conflict is with a later-priority plan
                // So if there's a conflict, we win and the other guy has to re-plan
    //            Log::flog(p.ship, "I win the conflict with ship id %d", conflict.ship.id);
    //            Log::flog(conflict.ship, "Forced to replan by ship id %d", p.ship.id);
                q.push_back(conflict);
            }

            // If we get here, we get to go to move_dest, at least for now
    //        Log::flog(p.ship, "putting myself down for move_dest = %s", +move_dest.toString());
            dest_to_plan[move_dest] = p;
        }

    //    Log::log("converting dest_to_plan to commands:");
        for (const pair<Vec, Plan> &vp : dest_to_plan) {
            Vec move_dest = vp.first;
            Plan p = vp.second;
            Direction d = grid.adj_dir(p.ship.pos, move_dest);
            //Log::flog(p.ship, "move_dest %s: %s is moving %s", +move_dest.toString(), +p.toString(), +desc(d));
            commands.push_back(Command::move(p.ship, d));

            /*if (grid.dist(move_dest, p.tgt) > grid.dist(p.ship.pos, p.tgt)) {
                const int r = rand() % 255;
                const int g = rand() % 255;
                const int b = 0; //rand() % 255;
                Log::flog_color(p.ship.pos, r, g, b, "Moving away from my tgt of %s", +p.tgt.toString());
                Log::flog_color(p.tgt, r, g, b, "Target of ship %d", +p.ship.id);
            }*/
        }
    }


    bool shipyard_will_be_clear(const vector<Command> &commands)
    {
        // returns true iff no ship is moving onto the shipyard
        for (Command c : commands) {
            if (c.type != Command::Type::Move) continue;
            const Vec src = Game::me->id_to_ship[c.ship_id].pos;
            const Vec dest = grid.add(src, c.move_dir);
            if (dest == Game::me->shipyard) {
                return false;
            }
        }
        return true;
    }

    void plan_returning()
    {
        vector<Ship> returners;
        for (Ship s : Game::me->ships) {
            if (busy_ship_ids.count(s.id)) continue;
            if (returning_ship_ids.count(s.id)) returners.push_back(s);
        }

        for (Ship s : returners) Log::log("returners: %s", +s.toString());

        // compute the average halite richness near each possible destination
        vector<Vec> return_targets = Game::me->structures;
        if (want_dropoff) {
            Log::flog(Game::me->shipyard, "ALLOWING PLANNED RETURN TARGET %s", +planned_dropoff_pos.toString());
            return_targets.push_back(planned_dropoff_pos);
        }

        const int R = 7;
        vector<pair<Vec, double>> target_scores;
        for (Vec t : return_targets) {
            double score = grid.halite_in_square(t, R);
            score /= (2*R + 1) * (2*R + 1);
            Log::flog(t, "RETURN TARGET: avg halite in radius %d is %f", R, score);
            target_scores.push_back({t, score});
        }

        set<int> sids_that_must_go_to_real_structure;
        if (want_dropoff) {
            double halite_needed_for_dropoff = Constants::DROPOFF_COST - Game::me->halite - grid(planned_dropoff_pos).halite;
            Log::flog(Game::me->shipyard, "initial halite_needed_for_dropoff = %f", halite_needed_for_dropoff);
            vector<pair<int, Ship>> dist_and_ships;
            for (Ship r : returners) {
                dist_and_ships.push_back({Game::me->dist_from_structure(r.pos), r});
            }
            sort(dist_and_ships.begin(), dist_and_ships.end(), [](auto a, auto b) { return a.first < b.first; });
            for (auto dist_and_ship : dist_and_ships) {
                if (halite_needed_for_dropoff <= 0) break;
                const Ship s = dist_and_ship.second;
                Log::flog(s.pos, "ALAS, I must go to a real dropoff,<br/>for halite_needed_for_dropoff=%f", halite_needed_for_dropoff);
                sids_that_must_go_to_real_structure.insert(s.id);
                halite_needed_for_dropoff -= 0.8 * s.halite;
            }
        }

        for (Ship ship : returners) {
            bool must_go_to_real_structure = sids_that_must_go_to_real_structure.count(ship.id) > 0;

            Log::flog(ship.pos, "return dests:");
            double best_score = -1e99;
            Vec best_tgt;
            for (pair<Vec, double> ts : target_scores) {
                const Vec target = ts.first;
                if (want_dropoff && must_go_to_real_structure && target == planned_dropoff_pos) continue;
                const double target_score = ts.second;
                double score = target_score - RETURN_TARGET_DIST_PENALTY.get(30.0) * grid.dist(ship.pos, target); // TUNE
                Log::flog(ship.pos, "%s: %f", +target.toString(), score);
                if (score > best_score) {
                    best_score = score;
                    best_tgt = target;
                }
            }
            Log::flog(ship.pos, "best return dest is %s", +best_tgt.toString());

            const int halite_room = Constants::MAX_HALITE - ship.halite;
            const int halite_here = grid(ship.pos).halite;
            const int halite_from_staying = min(halite_room, mined_halite(halite_here));       
            if (halite_from_staying >= 50) {
                Log::flog_color(ship.pos, 255, 0, 255, "staying during return!");
                plan(ship, ship.pos, Purpose::Return);
                continue;
            }
            
            Log::flog(ship.pos, "%s returning to %s", +ship.toString(), +best_tgt.toString());
            plan(ship, best_tgt, Purpose::Return);
        }
    }

    int compute_turns_to_fill_up(int ship_halite, int square_halite)
    {
        if (ship_halite + square_halite < Constants::MAX_HALITE) Log::die("%d %d will never fill up", ship_halite, square_halite);
        int turns = 0;
        while (ship_halite < Constants::MAX_HALITE) {
            const int amt_mined = mined_halite(square_halite);
            ship_halite += amt_mined;
            square_halite -= amt_mined;
            turns += 1;
        }
        return turns;
    }


    void plan_mining(const vector<Ship> &miners)
    {
        // Build a map of halite adjusted for inspiration and various incentives
        PosMap<double> effective_halite(0.0);
        for (Vec pos : grid.positions) effective_halite(pos) = grid(pos).halite;

        // adjust effective_halite for inspiration
        PosMap<int> nearby_enemy_ship_count(0);
        const int R = Constants::INSPIRATION_RADIUS;
        for (const Player &player : Game::players) {
            if (player.id == Game::my_player_id) continue;
            for (Ship enemy_ship : player.ships) {
                for (int dx = -R; dx <= R; ++dx) {
                    for (int dy = -R; dy <= R; ++dy) {
                        const Vec pos = grid.add(enemy_ship.pos, dx, dy);
                        if (grid.dist(pos, enemy_ship.pos) > R) continue;
                        nearby_enemy_ship_count(pos) += 1;
                    }
                }
            }
        }
        // Inspiration multiplies your mining rate by a factor of (1 + Constants::INSPIRED_BONUS_MULTIPLIER) = 3.
        // But we might want to use a smaller mult since there's a chance that the enemies will move away,
        // or something.
        // - 2.0 is bad, like 30% winrate in self-play
        // - 4.0 has 50% winrate in self-play
        // - let's stick with 3.0 for now
        const double inspiration_mult = (Game::num_players == 2 ? MINING_INSPIRATION_MULT_2P.get(3.0)  // TUNE
                                                                : MINING_INSPIRATION_MULT_4P.get(3.0));
        for (Vec pos : grid.positions) {
            if (nearby_enemy_ship_count(pos) >= Constants::INSPIRATION_SHIP_COUNT) {
                effective_halite(pos) *= inspiration_mult;
            }
        }
        
        // Halite in enemy ships will partially count toward increasing the effective halite on
        // the square. This will only have an effect if we consider the square passable, in which
        // case it will be an incentive to ram loaded enemy ships
        for (Ship enemy_ship : Game::enemy_ships) {
            effective_halite(enemy_ship.pos) += ENEMY_SHIP_HALITE_FRAC.get(0.5) * enemy_ship.halite;  // TUNE
        }

        // We're going to fudge the score of a square upward by an amount proportional to the average halite
        // richness of the closest return point. The return point can either be an existing structure or the
        // planned dropoff position
        // This whole thing is only very slightly better at best
        //     2p:  376-341    4p:  397-369-349-325
        // so maybe just get rid of it
        vector<Vec> return_points = Game::me->structures;
        if (want_dropoff) return_points.push_back(planned_dropoff_pos);
        vector<pair<Vec, double>> return_points_and_bonuses;
        for (Vec return_point : return_points) {
            const double mean_halite_in_area = grid.mean_halite_in_manhattan_radius(return_point, 8); // TUNE!
            const double bonus = MINING_RETURN_RICHNESS_BONUS_MULT.get(0.007) * mean_halite_in_area;   // TUNE!
            return_points_and_bonuses.push_back({return_point, bonus});             
            Log::flog(return_point, "mean halite in area = %f", mean_halite_in_area);
            Log::flog(return_point, "bonus = %f", bonus);
        }
        PosMap<double> return_richness_bonus(0.0);
        for (Vec v : grid.positions) {
            int smallest_dist = 999999;
            double bonus = 0.0;
            for (const pair<Vec, double> &return_point_and_bonus : return_points_and_bonuses) {
                const int dist = grid.dist(v, return_point_and_bonus.first);
                if (dist < smallest_dist) {
                    smallest_dist = dist;
                    bonus = return_point_and_bonus.second;
                }
            }
            return_richness_bonus(v) = bonus;
//            if (Game::turn < 200) Log::flog(v, "return_richness_bonus = %f", return_richness_bonus(v));
        }

        PosMap<int> tgt_to_ship_id(-1);
        deque<Ship> q;
        std::copy(miners.begin(), miners.end(), std::back_inserter(q));
        map<int, double> sid_to_route_finish_time;
        while (!q.empty()) {
            Ship ship = q.front();
            q.pop_front();

            const double halite_needed = return_halite_thresh - ship.halite;

            //const int my_dist_from_base = sid_to_base_dist.at(ship.id);

            const PosSet &impassable = sid_to_impassable.at(ship.id);

            // pick the best target.
            double best_halite_rate = -1e99;
            Vec best_tgt = ship.pos;
            for (Vec pos : grid.positions) {
                if (impassable(pos)) continue;
                double halite = effective_halite(pos);
                if (halite <= 0) continue;
                //if (ram_targets(pos) && DONT_MINE_RAM_TARGETS.get(false)) continue;

                const int dist_from_me = grid.dist(pos, ship.pos);

                // we can't pick a square that's already been picked
                // unless we are closer than the current owner
                const int owner_id = tgt_to_ship_id(pos);
                if (owner_id != -1) {
                    const Ship owner = Game::me->id_to_ship.at(owner_id);
                    const int dist_from_owner = grid.dist(pos, owner.pos);
                    if (dist_from_owner <= dist_from_me) {
                        //Log::flog(ship, "can't pick %s b/c we aren't closer than %d", +pos.toString(), owner.id);
                        continue;
                    }
                }

                // TODO: consider distance to all our structures, and the relative richness of the area around each structure
                int dist_from_base = Game::me->dist_from_structure(pos);
                if (want_dropoff) dist_from_base = min(dist_from_base, grid.dist(pos, planned_dropoff_pos));

                // silly late-game thing: don't choose a target if by the time we get there
                // we'd have to collapse to base for the end of the game
                // NOTE: this has no effect on self-play performance, so commenting it out for now
                //const int turns_left_on_arrival = Game::turns_left() - dist_from_me;
                //if (turns_left_on_arrival < dist_from_base + TURNS_LEFT_TO_RETURN) continue;

                // let's use a crude estimate of halite/turn. roughly speaking we're gonna
                // get (cell halite) / EXTRACT_RATIO halite per turn. And let's assume we can
                // mine at a similar rate from nearby cells. But multiply by a fudge factor of 0.7
                // because the halite decreases as we mine it. Then it'll take this long to fill up:
                const double naive_mine_rate = MINE_RATE_MULT.get(0.7) * halite * Constants::INV_EXTRACT_RATIO;  // TUNE. in some tests, the 0.7 is not terrible
                // Let's try to account for the 
                // fact that when you mine many small squares instead of one big square, you have to waste
                // time and halite moving.
                // Let's say your mining rate is 50% slower that=n the naive calculation
                const double low_halite_penalty = LOW_HALITE_EFFICIENCY_PENALTY.get(0.5);  // TUNE. 1.0 is bad. 0.0 is probably worse than 0.5
                const double efficiency = (1.0 - low_halite_penalty) + low_halite_penalty * max(0.0, min((double)Constants::MAX_HALITE, halite)) / (double)Constants::MAX_HALITE;  // TUNE
                const double mine_rate = efficiency * naive_mine_rate;
                const double turns_to_fill_up = halite_needed / mine_rate;
                const double total_time = turns_to_fill_up 
                                        + dist_from_base 
                                        + MINING_DIST_FROM_ME_MULT.get(2.0) * dist_from_me;  // TUNE. MINING_DIST_FROM_ME_MULT 1.0 is worse than 2.0, 3.0 about the same as 1.0
                double overall_halite_rate = halite_needed / total_time;
                overall_halite_rate += return_richness_bonus(pos);
//                if (Game::turn < 200 && ship.id == 1) Log::flog(pos, "overall_halite_rate = %f", overall_halite_rate);

                if (overall_halite_rate > best_halite_rate) {
                    best_halite_rate = overall_halite_rate;
                    best_tgt = pos;
                    sid_to_route_finish_time[ship.id] = total_time;
                }
            }

            // if our target is owned by another ship, kick out the owner
            // also, cancel any reservation they have
            const int owner_id = tgt_to_ship_id(best_tgt);
            if (owner_id != -1) {
                q.push_back(Game::me->id_to_ship.at(owner_id));
            }

            // claim this target
            tgt_to_ship_id(best_tgt) = ship.id;
        }

        for (Vec tgt : grid.positions) {
            const int ship_id = tgt_to_ship_id(tgt);
            if (ship_id == -1) continue;
            const Ship ship = Game::me->id_to_ship.at(ship_id);

            if (ship.pos == tgt) {
                plan(ship, ship.pos, Purpose::Mine);
            } else {  // tgt is not the current position
                // decide whether to actually move toward the target
                const double tgt_eff_halite = effective_halite(tgt);
                const double here_eff_halite = effective_halite(ship.pos);

                bool should_move = tgt_eff_halite > MINE_MOVE_ON_MULT.get(3.0) * here_eff_halite;  // TUNE

                // tune the factor here
                if (should_move) {
                    plan(ship, tgt, Purpose::Mine);
                } else {
                    plan(ship, ship.pos, Purpose::Mine);
                }
            }
        }
    }


    // returns ship and tgt pos
    void consider_making_dropoff(vector<Command> &commands)
    {
        want_dropoff = false;

        if (Game::turns_left() < 75) {  // TUNE
            Log::log("too late to make dropoff");
            return;
        }

        int ships_for_first_dropoff = SHIPS_FOR_FIRST_DROPOFF.get(17);
        int ships_per_later_dropoff = SHIPS_PER_LATER_DROPOFF.get(20);
        if (Game::num_players == 2) {
            // 2 player

        } else {
            // 4 player
            if (grid.width == 32) {
                ships_for_first_dropoff = SHIPS_FOR_FIRST_DROPOFF.get(11);
                ships_per_later_dropoff = SHIPS_PER_LATER_DROPOFF.get(20);
            } else if (grid.width == 40) {
                ships_for_first_dropoff = SHIPS_FOR_FIRST_DROPOFF.get(11);
                ships_per_later_dropoff = SHIPS_PER_LATER_DROPOFF.get(20);
            } else if (grid.width == 48) {
                ships_for_first_dropoff = SHIPS_FOR_FIRST_DROPOFF.get(13);
                ships_per_later_dropoff = SHIPS_PER_LATER_DROPOFF.get(20);
            } else if (grid.width == 56) {
                ships_for_first_dropoff = SHIPS_FOR_FIRST_DROPOFF.get(15);
                ships_per_later_dropoff = SHIPS_PER_LATER_DROPOFF.get(20);
            } else if (grid.width == 64) {
                ships_for_first_dropoff = SHIPS_FOR_FIRST_DROPOFF.get(17);
                ships_per_later_dropoff = SHIPS_PER_LATER_DROPOFF.get(18);  // NOTE 18 NOT 20 !!  weak evidence for this in a test
            }
        }

        if (Game::me->ships.size() < ships_for_first_dropoff + ships_per_later_dropoff * Game::me->dropoffs.size()) {  // TUNE
            Log::log("not enough ships to make dropoff");
            return;
        }

        Vec best_pos;
        int best_score = -999999;
        for (Vec pos : grid.positions) {
            const int dist_to_allied_structure = Game::me->dist_from_structure(pos);
            const int dist_to_enemy_structure =  grid.smallest_dist(pos, Game::enemy_structures);
            const int dist_to_enemy_ship = grid.smallest_dist(pos, Game::enemy_ships);
            const int dist_to_enemy_shipyard = grid.smallest_dist(pos, Game::enemy_shipyards);

            //if (impassable(pos)) continue;
            // have to come up with something new now that impassable is ship-dependent:
            if (dist_to_enemy_ship <= 1 && grid.smallest_dist(pos, Game::me->ships) > 0) continue;

            // don't build structures too close together
            if (dist_to_allied_structure < MIN_DROPOFF_SEPARATION.get(9)) continue;  // TUNE

            // can't build on top of enemy structures
            if (dist_to_enemy_structure <= 2) continue;

            if (DONT_BUILD_NEAR_ENEMY_STRUCTURES.get(false)) {
                // crude heuristic:
                // don't build at a spot that's closer to an enemy structure than it
                // is to any of our structures, unless there are no nearby enemy ships
                // in a big radius        
                if (dist_to_enemy_structure <= dist_to_allied_structure && dist_to_enemy_ship < 20) continue;  // TUNE
            } else {
                // much more relaxed condition:
                if (dist_to_enemy_structure <= 4) continue;  // TUNE
            }

            if (DONT_BUILD_NEAR_ENEMY_SHIPYARDS.get(true)) {
                // don't build at a spot that's closer to an enemy SHIPYARD than it is to
                // any of our structures.
                if (dist_to_enemy_shipyard < dist_to_allied_structure) {
                    continue;  // TUNE?
                }
            }

            // let's say there have to be 2 allied ships within 10
            //if (grid.num_within_dist(pos, Game::me->ships, 10) < 2) continue; 
            
            const int radius = 7;  // TUNE
            int new_halite_in_radius = 0;
            int old_halite_in_radius = 0;
            // let's only count halite that's significantly closer to this new dropoff
            // than to any existing structure
            for (int dy = -radius; dy <= radius; ++dy) {
                for (int dx = -radius; dx <= radius; ++dx) {
                    Vec v = grid.add(pos, dx, dy);
                    if (grid.dist(pos, v) < Game::me->dist_from_structure(v)) {
                        new_halite_in_radius += grid(v).halite;
                    } else {
                        old_halite_in_radius += grid(v).halite;
                    }
                }
            }
            //Log::flog(pos, "new_halite_in_radius = %d", new_halite_in_radius);
            //Log::flog(pos, "old_halite_in_radius = %d", old_halite_in_radius);
            if (new_halite_in_radius < DROPOFF_MIN_NEW_HALITE_IN_RADIUS.get(25000)) {  // TUNE
                continue;
            }

            const double avg_ship_dist = grid.average_dist(pos, Game::me->ships);
            const double ship_dist_factor = exp(-DROPOFF_SHIP_DIST_PENALTY.get(0.02) * avg_ship_dist);

            /*const int dist_to_closest_enemy = grid.smallest_dist(pos, Game::enemy_ships);
            const double enemy_dist_factor = exp(-DROPOFF_ENEMY_DIST_PENALTY.get(0.02) * max(7, dist_to_closest_enemy));*/

            double score = new_halite_in_radius * ship_dist_factor;
            /*if (Game::turn < 200) {
                Log::flog(pos, "score = %f", score);
                Log::flog(pos, "ship_dist_factor = %f", ship_dist_factor);
            }*/

            if (score > best_score) {
                best_score = score;
                best_pos = pos;
            }
        }

        if (best_score < 0) {
            // not doing anything
            return;
        }

        Log::flog_color(best_pos, 255, 0, 0, "best dropoff target: score = %d", best_score);

        bool actually_built = false;

        // find the nearest ship
        Ship ship = grid.closest(best_pos, Game::me->ships);
        Log::flog_color(ship.pos, 0, 0, 255, "going to dropoff target");
        if (ship.pos == best_pos) {
            // we're at the target!
            const int adjusted_dropoff_cost = Constants::DROPOFF_COST - ship.halite - grid(ship.pos).halite;
            if (Game::me->halite >= adjusted_dropoff_cost) {
                // can build, so do it
                commands.push_back(Command::construct(ship.id));
                // immediately update our player data, including our halite total
                Game::me->immediately_add_dropoff(adjusted_dropoff_cost, ship.pos);
                actually_built = true;
            } else {
                // Can't build yet; chill out.
                // Put this in the planning system so other ships path around us.
                // Purpose::Return is a hack, just want an early priority.
                // Purpose::Stuck would be bad b/c ships would try to wait for us to get unstuck instead
                // of pathing around.
                plan(ship, best_pos, Purpose::Return);
            }
        } else {
            // not there yet; move toward the target
            plan(ship, best_pos, Purpose::Mine);  // maybe want a different Purpose
        }

        if (!actually_built) {
            // if we did build, then our Player structures list and halite already got updated
            // to reflect the new structure. If we didn't build, let's remember the fact that we
            // want to build
            want_dropoff = true;
            planned_dropoff_pos = best_pos;
        }
        busy_ship_ids.insert(ship.id);
    }

    void consider_ramming(vector<Command> &commands)
    {
        if (Game::num_players != 2 && !CONSIDER_RAMMING_IN_4P.get(false)) return;

        for (Ship enemy_ship : Game::enemy_ships) {
            if (contains(Game::enemy_structures, enemy_ship.pos)) continue;  // don't ram on top of enemy structures

            // let's just use the check in set_impassable()
            //if (impassable(enemy_ship.pos)) continue;

            // find the best adjacent allied rammer, if any
            Ship best_rammer;
            best_rammer.id = -1;
            best_rammer.halite = 99999;
            for (Ship my_ship : Game::me->ships) {
                if (grid.dist(my_ship.pos, enemy_ship.pos) != 1) continue;            
                if (sid_to_impassable.at(my_ship.id)(enemy_ship.pos)) continue;  // if it's impassable, it's probably bad to ram on that square
                if (!can_move(my_ship)) continue;
                if (busy_ship_ids.count(my_ship.id)) continue;

                const int max_halite_to_ram = enemy_ship.halite + (grid(enemy_ship.pos).halite / 2);  // TUNE?
                if (my_ship.halite >= max_halite_to_ram) continue;  // TUNE. TODO: maybe relax this condition when we heavily outnumber?

                if (my_ship.halite < best_rammer.halite) {
                    best_rammer = my_ship;
                }
            }
            if (best_rammer.id != -1) {
                // ramming speed!
                busy_ship_ids.insert(best_rammer.id);
                ram_targets(enemy_ship.pos) = true;
                Log::flog(enemy_ship.pos, "ram target");
                // go through the planning system so other ships know to avoid this square
                plan(best_rammer, enemy_ship.pos, Purpose::Ram);
                // consider making ram targets not get chosen as mining targets. 
                // that's probably a good idea if we expect the enemy to successfully evade
            }
        }
    }



    void consider_fleeing()
    {
        vector<Ship> fleeing_ships;
        for (Ship ship : Game::me->ships) {
            if (busy_ship_ids.count(ship.id)) {
                Log::flog(ship.pos, "not fleeing b/c busy");
                continue;
            }
            /*if (returning_ship_ids.count(ship.id)) {
                Log::flog(ship.pos, "not fleeing b/c returning");
                continue;  // returning ships will already path to avoid enemies. except, not necessarily??
            }*/
            if (!can_move(ship)) {
                Log::flog(ship.pos, "not fleeing b/c can't move");
                continue;
            }
            const PosSet &impassable = sid_to_impassable.at(ship.id);
            if (!impassable(ship.pos)) {
                Log::flog(ship.pos, "not fleeing b/c not impassable");
                continue;  // this location is apparently fine, don't flee
            }

            if (Game::num_players != 2) {
                // in 4p games, it's much less likely that we are actually going to get rammed,
                // even if this square is nominally unsafe.
                // Let's ask the PlayerAnalyzer if it thinks any adjacent ships are going to ram us this turn
                bool gonna_get_rammed = false;
                for (int pid = 0; pid < Game::num_players; ++pid) {
                    if (pid == Game::my_player_id) continue;
                    for (const Ship enemy_ship : Game::players[pid].ships) {
                        if (!can_move(enemy_ship)) continue;  // can't ram if you can't move
                        if (grid.dist(enemy_ship.pos, ship.pos) == 1 &&
                            PlayerAnalyzer::player_might_ram(pid, ship, enemy_ship)) {
                            //Log::flog_color(ship.pos, 255, 125, 0, "IM GONNA GET RAMMED");
                            //Log::flog_color(enemy_ship.pos, 255, 0, 0, "THIS GUYS GONNA RAM ME");
                            gonna_get_rammed = true;
                            break;
                        }
                    }
                }
                if (!gonna_get_rammed) {
                    Log::flog(ship.pos, "not fleeing b/c not gonna get rammed");
                    continue;  // we're probably OK, don't flee
                }
            }

/*            if (ship.halite < 100) {
                Log::flog_color(ship.pos, 0, 255, 0, "i have low halite so im not afraid");
                continue;  // TEST don't fee with low halite
            }*/

            // TODO: DONT JUST FLEE IF ITS IMPASSABLE! WE SHOULD ACTUALLY BE OUTNUMBERED BEFORE WE FLEE!
            fleeing_ships.push_back(ship);
            //Log::flog_color(ship.pos, 255, 125, 0, "I'm scared!");
        }

        PosMap<int> dest_to_sid(-1);
        map<int, Vec> sid_to_dest;

        for (int i = 0; i < 3; ++i) { // go through the assignment process a few times
            for (Ship ship : fleeing_ships) {
                Log::flog(ship.pos, "=== consider_fleeing round %d ===", i);
                // try to move toward a location ranked by
                // - not-already-taken then
                // - passability, then
                // - not-currently-occupied-by-an-ally, then
                // - structure distance (close is better)
                // Note that we will steal an already-taken dest if it's our only option
                const PosSet &impassable = sid_to_impassable.at(ship.id);
                Vec best_dest;
                int best_score = -999999999;
                bool found_dest = false;
                for (int d = 0; d < 4; ++d) {
                    Vec adj = grid.add(ship.pos, (Direction)d);
                    if (grid.smallest_dist(adj, Game::enemy_ships) == 0) continue; // don't move onto enemy positions. TODO: it might not be totally crazy to try to swap with the enemy??
                    
                    if (grid.smallest_dist(adj, Game::me->ships) == 0) continue;  // FIXME!!
                    
                    int score = 0;
                    const int owner_sid = dest_to_sid(adj);
                    if (owner_sid == -1 || owner_sid == ship.id) score += 1000000;  // adj isn't taken by someone else -- that's good
                    if (!impassable(adj)) score += 10000;  // adj isn't impassable -- that's good
                    score += 100 - Game::me->dist_from_structure(adj);  // closer to a structure is better
                    Log::flog(ship.pos, "%s score %d", +desc((Direction)d), score);

                    // this location is better
                    if (score > best_score) {
                        best_dest = adj;
                        best_score = score;
                        found_dest = true;
                    }
                }
                if (found_dest) {
                    Log::flog(ship.pos, "%s is best", +desc(grid.adj_dir(ship.pos, best_dest)));
                    // first, relinquish any dest we previously owned
                    auto it = sid_to_dest.find(ship.id);
                    if (it != sid_to_dest.end()) {
                        const Vec prev_dest = it->second;
                        dest_to_sid(prev_dest) = -1;
                        sid_to_dest.erase(it);
                        Log::flog(ship.pos, "relinquished previous dest %s", +prev_dest.toString());
                    }

                    // next, if another ship currently claims this dest, kick them out
                    const int owner_id = dest_to_sid(best_dest);
                    if (owner_id != -1) {
                        sid_to_dest.erase(owner_id);
                        dest_to_sid(best_dest) = -1;  // unecessary, but whatever
                        Log::flog(ship.pos, "kicked out previous owner %d", owner_id);
                    }

                    // finally, claim this dest
                    sid_to_dest[ship.id] = best_dest;
                    dest_to_sid(best_dest) = ship.id;
                }
            }
        }

        // finally, issue the actual plans
        for (const pair<int, Vec> &sid_and_dest : sid_to_dest) {
            const Ship ship = Game::me->id_to_ship.at(sid_and_dest.first);
            const Vec dest = sid_and_dest.second;
            Log::flog(ship.pos, "fleeing to %s", +dest.toString());
            plan(ship, dest, Purpose::Flee);
            busy_ship_ids.insert(ship.id);
        }
    }

    // possibly this should be merged into plan_returning somehow?
    void plan_final_collapse()
    {
        for (Ship ship : Game::me->ships) {
            if (busy_ship_ids.count(ship.id)) continue;

            // at the end of the game, return to the closest base
            const Vec closest_structure = grid.closest(ship.pos, Game::me->structures);
            const int dist_from_base = grid.dist(ship.pos, closest_structure);
            if (Game::turns_left() < dist_from_base + TURNS_LEFT_TO_RETURN) {
                plan(ship, closest_structure, Purpose::Return);
                busy_ship_ids.insert(ship.id);
                continue;
            }
        }
    }

    // Emulate SiestaGuru's dropoff block, for testing
    void TEST_consider_dropoff_blocking()
    {
        if (!DO_DROPOFF_BLOCKING.get(false)) return;

        // Currently this is just for testing, and we'll always target player zero
        const int tgt_pid = 0;
        if (tgt_pid == Game::my_player_id) return;

        if (Game::turns_left() > 100) return;

        for (Ship ship : Game::me->ships) {
            if (busy_ship_ids.count(ship.id)) continue;

            if (ship.halite > 300) continue;

            const Vec structure_to_block = grid.closest(ship.pos, Game::players.at(tgt_pid).structures);
            const int dist_to_block_tgt = grid.dist(ship.pos, structure_to_block);
            if (dist_to_block_tgt > 15) continue;

            Log::flog_color(ship.pos, 255, 0, 0, "blocking!");
            if (dist_to_block_tgt > 1) {
                plan(ship, structure_to_block, Purpose::Mine);
            } else {
                plan(ship, ship.pos, Purpose::Mine);
            }
            busy_ship_ids.insert(ship.id);
        } 
    }

    bool ship_could_possibly_return(Ship ship, const Player &player)
    {
        return player.dist_from_structure(ship.pos) <= Game::turns_left() + 1;
    }

    Vec ideal_blocking_pos(Ship enemy_ship, Vec enemy_ship_dest)
    {
        const Direction best_enemy_dir = grid.approach_dirs(enemy_ship.pos, enemy_ship_dest)[0];
        const Vec move_dest = grid.add(enemy_ship.pos, best_enemy_dir);
        return move_dest;
    }

    void consider_blocking_final_collapse()
    {
        // currently only block the final collapse
        if (Game::turns_left() > grid.width) return;
        Log::log("consider_blocking_final_collapse start");

        map<int, int> pid_to_profit_plus_carried;
        for (const Player &player : Game::players) {
            int profit_plus_carried = player.halite;
            for (Ship ship : player.ships) {
                if (ship_could_possibly_return(ship, player)) {
                    profit_plus_carried += ship.halite;
                } else {
                    // highlight ships that can't possibly return
                    Log::flog_color(ship.pos, 0, 0, 255, "CANT POSSIBLY RETURN");
                }
            }
            pid_to_profit_plus_carried[player.id] = profit_plus_carried;
            Log::flog(Game::me->shipyard, "profit_plus_carried of player %d = %d", player.id, profit_plus_carried);
        }

        // only bother blocking players with close halite totals to us (except always block in 2p)
        set<int> pids_to_block;
        for (int pid = 0; pid < Game::num_players; ++pid) {
            if (pid == Game::my_player_id) continue;
            const int halite_diff = abs(pid_to_profit_plus_carried[pid] - pid_to_profit_plus_carried[Game::my_player_id]);
            if (Game::num_players == 2 || halite_diff < 5000) {
                Log::flog(Game::me->shipyard, "will try to block player %d!", pid);
                pids_to_block.insert(pid);
            }
        }

        deque<Ship> eligible_blocker_q;

        for (Ship ship : Game::me->ships) {
            if (busy_ship_ids.count(ship.id)) continue;
            const bool could_possibly_return = ship_could_possibly_return(ship, *Game::me);
            if (ship.halite > 100 && could_possibly_return) continue;  // return that halite instead of blocking!
            Log::flog_color(ship.pos, 250, 250, 0, "eligible to block!");
            eligible_blocker_q.push_back(ship);
        }

        map<int, Ship> enemy_sid_to_blocker;

        while (!eligible_blocker_q.empty()) {
            const Ship ship = eligible_blocker_q.front();
            eligible_blocker_q.pop_front();

            const bool could_possibly_return = ship_could_possibly_return(ship, *Game::me);

            // find the best ship to block
            Ship ship_to_block;
            // squelch some compiler complaints about using unitialized variables :(
            ship_to_block.halite = 0;
            ship_to_block.id = -1;
            ship_to_block.pos = {-1, -1};
            bool found_ship_to_block = false;
            for (const Player &enemy : Game::players) {
                if (enemy.id == Game::my_player_id) continue;
                if (!pids_to_block.count(enemy.id)) continue;
                for (Ship enemy_ship : enemy.ships) {
                    // if we could still deliver our halite, only try to block people with signficantly more halite than us
                    if (could_possibly_return && enemy_ship.halite <= ship.halite + 400) continue;

                    // don't bother blocking this ship if we've already found a better one to block
                    if (found_ship_to_block && ship_to_block.halite > enemy_ship.halite) continue;

                    // if the enemy ship cannot possibly return, don't bother blocking it!
                    if (!ship_could_possibly_return(enemy_ship, enemy)) continue;

                    Vec enemy_ship_dest = grid.closest(enemy_ship.pos, enemy.structures);
                    const int enemy_dist_from_dest = grid.dist(enemy_ship.pos, enemy_ship_dest);

                    // only try to block when we're pretty sure the enemy must be heading back
                    // TODO: reenable something like this
                    //if (enemy_dist_from_dest < Game::turns_left() + 15) continue;  // TUNE
                    //Log::flog(ship.pos, "block BBB enemy_ship = %s<br/>enemy_ship_dest = %s", +enemy_ship.toString(), +enemy_ship_dest.toString());
                    
                    // check if we could possibly block this enemy ship in principle
                    // TODO: There's something missing here. Sometimes this will think we can
                    // block when at best we can meet them at the dropoff
                    const int my_dist_from_dest = grid.dist(ship.pos, enemy_ship_dest);
                    if (enemy_dist_from_dest < my_dist_from_dest) continue;
                    
                    // for now let's also require the the enemy is closer to us than they are to
                    // their dest.
                    // TODO: try lifting this
                    // TODO: this misses a case where we are "closer" to the enemy, but in the wrong
                    // direction around the torus
                    const int enemy_dist_from_me = grid.dist(ship.pos, enemy_ship.pos);
                    if (enemy_dist_from_me > enemy_dist_from_dest) continue;

                    // finally, check if another allied ship is already blocking this ship, and is better
                    // placed to do so than we are
                    auto it = enemy_sid_to_blocker.find(enemy_ship.id);
                    if (it != enemy_sid_to_blocker.end()) {
                        const Ship existing_blocker = it->second;
                        const Vec block_pos = ideal_blocking_pos(enemy_ship, enemy_ship_dest);
                        if (grid.dist(existing_blocker.pos, block_pos) <= grid.dist(ship.pos, block_pos)) {
                            // the existing_blocker is at least as well positioned to block as we are,
                            // so let this enemy_ship go
                            continue;
                        }
                    }

                    // this ship is blockable and has more halite than the current ship_to_block (if any)
                    // make this ship our new ship_to_block
                    ship_to_block = enemy_ship;
                    found_ship_to_block = true;
                }
            }

            if (found_ship_to_block) {
                // if there was an existing blocker but we decided we were better positioned to block,
                // then make the existing blocker re-choose a ship to block
                auto it = enemy_sid_to_blocker.find(ship_to_block.id);
                if (it != enemy_sid_to_blocker.end()) {
                    const Ship existing_blocker = it->second;
                    eligible_blocker_q.push_back(existing_blocker);
                    Log::flog(existing_blocker.pos, "kicked off of block target by %d", ship.id);
                }

                // register ourselves as the blocker of this enemy ship
                enemy_sid_to_blocker[ship_to_block.id] = ship;
                Log::flog(ship.pos, "picking block target %d", ship_to_block.id);
            }
        }


        // finally, go through enemy_sid_to_blocker and make our blocking plans
        for (const pair<int, Ship> &enemy_sid_and_blocker : enemy_sid_to_blocker) {
            const int enemy_sid = enemy_sid_and_blocker.first;
            const Ship ship_to_block = Game::id_to_ship.at(enemy_sid);
            const Ship blocker = enemy_sid_and_blocker.second;
            
            const int enemy_pid = Game::sid_to_pid.at(enemy_sid);
            const Vec ship_to_block_dest = grid.closest(ship_to_block.pos, Game::players[enemy_pid].structures);
            const Vec block_pos = ideal_blocking_pos(ship_to_block, ship_to_block_dest);

            busy_ship_ids.insert(blocker.id);
            plan(blocker, block_pos, Purpose::Mine);
            // make some alterations to the impassability map
            PosMap<bool> &impassable = sid_to_impassable.at(blocker.id);
            impassable(block_pos) = false;  // PROBLEM: we might just ram a different enemy?
            for (int d = 0; d < 4; ++d) {
                const Vec adj = grid.add(block_pos, (Direction)d);
                impassable(adj) = false;  // SAME PROBLEM EVEN WORSE
            }
            impassable(ship_to_block.pos) = false;  
            //Log::flog_color(blocker.pos, 0, 255, 0, "trying to block %d!", ship_to_block.id);
            //Log::flog_color(block_pos, 0, 50, 0, "block_pos of %d for blocking %d", blocker.id, ship_to_block.id);
            //Log::flog_color(ship_to_block.pos, 255, 0, 0, "%d is trying to block this ship", blocker.id);
        }
    }

    vector<Command> turn()
    {
        PlayerAnalyzer::analyze();

        predictor_data_collector.collect();
        if (TRAINING_DATA_SAVE_FILENAME.given()) {
            if (Game::turns_left() == 0) {
                predictor_data_collector.save_training_data(TRAINING_DATA_SAVE_FILENAME.get());
            }
        }
        if (JUST_MAKE_TRAINING_DATA.get(false)) return {};

        plans.clear();
        busy_ship_ids.clear();
        ram_targets.fill(false);

        set_impassable();

        vector<Command> commands;

        consider_making_dropoff(commands);

        consider_ramming(commands);

        //TEST_consider_dropoff_blocking();

        consider_blocking_final_collapse();

        consider_fleeing();    

        plan_final_collapse();

        vector<Ship> miners;

        for (const Ship &ship : Game::me->ships) {
            if (busy_ship_ids.count(ship.id)) continue;

            bool returning;
            if (returning_ship_ids.count(ship.id)) {  // this ship has been returning
                if (ship.halite <= 0) {
                    returning_ship_ids.erase(ship.id);
                    returning = false;
                } else {
                    returning = true;
                }
            } else {  // this ship has not been returning
                if (ship.halite >= return_halite_thresh) {  // TUNE
                    returning_ship_ids.insert(ship.id);
                    returning = true;
                } else {
                    returning = false;
                }
            }

            if (!returning) {
                miners.push_back(ship);
            }
        }

        plan_mining(miners);
        plan_returning();

        // let's try this as a postprocessing step. anyone who can't move 
        // must plan to stay where they are.
        for (Plan &p : plans) {
            if (!can_move(p.ship)) {
                p.tgt = p.ship.pos;
                p.purpose = Purpose::Stuck;
            }
        }

        make_special_final_collapse_commands(commands);

        resolve_plans_into_commands(commands);

        still_spawning_ships = want_more_ships();
        if (still_spawning_ships && can_afford_a_ship() && shipyard_will_be_clear(commands)) {
            commands.push_back(Command::generate());
        }

        return commands;
    }
};


int main(int argc, char **argv)
{
    //string mystring;
    //cin >> mystring;
    //cerr << "BOT START!!" << mystring << ".\n";
    //cout << "BOT START!!" << mystring << ".\n";
    //exit(-1);

    ParameterBase::parse_all(argc, argv);

    Network::init();

    Bot bot(argc, argv);

    // start the main game loop
    do {
        Network::begin_turn();

        vector<Command> commands = bot.turn();

        PlayerAnalyzer::remember_my_commands(commands);
        Network::end_turn(commands);
    } while (Game::turn < Constants::MAX_TURNS);

    Log::log("Exiting.");

    return 0;
}
