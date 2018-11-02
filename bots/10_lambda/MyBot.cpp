#include <algorithm>
#include <deque>
#include <queue>
#include <vector>
#include <set>
#include <unordered_set>
#include "hlt/Constants.h"
#include "hlt/Game.h"
#include "hlt/Network.h"
#include "hlt/Grid.h"
#include "hlt/Log.h"
#include "hlt/Path.h"
#include "hlt/PosMap.h"
#include "hlt/Util.h"
using namespace std;

unordered_set<int> returning_ship_ids;

enum class Purpose
{
    Stuck = 0,          // must stay put b/c too little halite
    Ram = 1,            // trying to hit an adjacent enemy
    EvadeReturner = 2,  // getting out of the way of someone with the Return purpose
    Return = 3,         // bringing halite back to base
    Mine = 4,           // getting halite
};

string desc(Purpose p)
{
    switch (p) {
        case Purpose::Stuck: return "Stuck"; 
        case Purpose::EvadeReturner: return "EvadeReturner"; 
        case Purpose::Return: return "Return"; 
        case Purpose::Mine: return "Mine";
        default: return "PURPOSE::ERROR";
    }
}

struct Plan 
{
    Ship ship;
    Vec tgt;
    Purpose purpose;

    string toString() const { return stringf("Plan(ship=%s tgt=%s purpose=%s", +ship.toString(), +tgt.toString(), +desc(purpose)); }
};

vector<Plan> plans;
PosSet impassable(false);

const int TURNS_LEFT_TO_RETURN = 15;

void plan(const Ship &ship, Vec tgt, Purpose purpose)
{
    plans.push_back({ship, tgt, purpose});
}

// whether a ship has enough halite to move
bool can_move(Ship ship)
{
    return grid(ship.pos).halite <= Constants::MOVE_COST_RATIO * ship.halite;
}

bool should_spawn(int dropoff_ship_id)
{
    int min_halite_to_spawn = Constants::SHIP_COST;
    if (dropoff_ship_id != -1) min_halite_to_spawn += Constants::DROPOFF_COST;
    if (Game::me->halite < min_halite_to_spawn) return false;  // can't spawn

    int halite_left = 0;
    for (int x = 0; x < grid.width; ++x) {
        for (int y = 0; y < grid.height; ++y) {
            halite_left += grid({x, y}).halite;
        }
    }
    // check if we already have plenty of ships
    if (Game::num_players == 4) {
        if (Game::turns_left() < 200) return false;
        if (halite_left < 3500 * (int)Game::me->ships.size()) return false;
    } else { // two players
        if (Game::turns_left() < 140) return false;
        if (halite_left < 2500 * (int)Game::me->ships.size()) return false;
    }

    return true;
}

void set_impassable()
{
    impassable.fill(false);

    for (Ship enemy_ship : Game::enemy_ships) {
        // we need to ignore enemy ships that are on top of our structures
        // or they'll block us from delivering. it's fine to ram them.
        if (Game::me->has_structure_at(enemy_ship.pos)) continue;

        for (int d = 0; d < 5; ++d) {
            const Vec pos = grid.add(enemy_ship.pos, (Direction)d);

            // also, if this is our structure don't avoid it just because there's an
            // adjacent enemy ship
            if (Game::me->has_structure_at(pos)) continue;

            // in 4-player games we'll currently be very pacifist and avoid moving
            // next to an enemy
            if (Game::num_players == 4) {
                impassable(pos) = true;
                continue;
            }
            // below is for 2-player games

            // let's say it's passable if there are 3 allied ships closer than the second-closest enemy
            // otherwise it's impassable
            const int second_closest_enemy_dist = grid.second_smallest_dist(pos, Game::enemy_ships);
            const int num_closer_allies = grid.num_within_dist(pos, Game::me->ships, second_closest_enemy_dist - 1);
            //Log::flog(pos, "second_closest_enemy_dist = %d", second_closest_enemy_dist);
            //Log::flog(pos, "num_closer_allies = %d", num_closer_allies);
            if (num_closer_allies >= 3) {
                //Log::flog_color(pos, 0, 55, 0, "passable!");
                impassable(pos) = false;
            } else {
                //Log::flog_color(pos, 55, 0, 0, "IMpassable");
                impassable(pos) = true;
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
void make_final_collapse_commands(vector<Command> &commands)
{
    if (Game::turns_left() > TURNS_LEFT_TO_RETURN + 1) return;

    vector<Plan> remaining_plans;
    for (Plan p : plans) {
        Vec structure = grid.closest(p.ship.pos, Game::me->structures);
        if (grid.dist(p.ship.pos, structure) <= 1 && can_move(p.ship)) {
            Direction d = grid.adj_dir(p.ship.pos, structure);
            commands.push_back(Command::move(p.ship, d));
        } else {
            remaining_plans.push_back(p);
        }
    }
    plans = remaining_plans;
}


void resolve_plans_into_commands(vector<Command> &commands)
{
    // priority queue of plans with lowest (earliest) purpose coming first.
    auto pri_cmp = [](Plan a, Plan b) { return a.purpose < b.purpose; };
    priority_queue<Plan, vector<Plan>, decltype(pri_cmp)> q(pri_cmp);
    for (Plan p : plans) q.push(p);

    map<Vec, Plan> dest_to_plan;

    while (!q.empty()) {
        Plan p = q.top();
        q.pop();
//        Log::flog(p.ship, "popped %s", +p.toString());

        // decide where this ship should move to act on its plan.
        // can only pick a move that doesn't interfere with an existing move
        // of an earlier-priority plan. If there's no such move, we'll pick
        // a new earlier-priority purpose.
        Vec move_dest;
        if (p.tgt == p.ship.pos) {  // simplest case: plan calls for staying still
            move_dest = p.tgt;
        } else {  // plan calls for moving to some target
            // Plan a path that avoids any plans of equal or earlier priority.
            //PosSet blocked_for_pathing(avoid_positions);
            PosSet blocked_for_pathing(impassable);
            for (const pair<Vec, Plan> &vp : dest_to_plan) {
                if (vp.second.purpose <= p.purpose) {
                    // if you're a miner, don't consider returners' paths blocked except for adjacent squares.
                    // the idea is that even if you run into them you'll just switch places; there's no point
                    // trying to path around them. 
                    // might want to expand this to other cases? like, no point pathing around any very distant ships?
                    if (p.purpose == Purpose::Mine && 
                        vp.second.purpose == Purpose::Return && 
                        grid.dist(vp.first, p.ship.pos) > 1) {
                        continue;
                    }
                    // otherwise, don't path over this square
                    blocked_for_pathing(vp.first) = true;
                }
            }
            Direction path_dir = Path::find(p.ship.pos, p.tgt, blocked_for_pathing);
            move_dest = grid.add(p.ship.pos, path_dir);
        }
//        Log::flog(p.ship, "move_dest = %s", +move_dest.toString());
        
        // Check if anyone else is trying to move to the same square
        auto conflict_it = dest_to_plan.find(move_dest);
        if (conflict_it != dest_to_plan.end()) {  // someone is
            Plan conflict = conflict_it->second;

            // If we came up with a non-Still move, it's guaranteed to avoid intefering
            // with any plans of equal or earlier priority. But it's possible that we're
            // staying still and that will interfere with a plan of earlier priority.
            if (move_dest == p.ship.pos && conflict.purpose < p.purpose) {
                // It happened. We want to try to get out of that plan's way. Do this by promoting
                // ourselves to an evasion purpose with an earlier priority.
                if (!(p.purpose == Purpose::Mine && conflict.purpose == Purpose::Return)) {
                    Log::die("case AAA I thought shouldn't happen id1=%d id2=%d", p.ship.id, conflict.ship.id);
                }
                // for now, let's just swap places with the guy who's coming onto our square.
                // this should be guaranteed to succeed.
                p.purpose = Purpose::EvadeReturner;
                p.tgt = conflict.ship.pos;
                q.push(p);
//                Log::flog(p.ship, "can't stay still b/c of ship id %d. going to evade.", conflict.ship.id);
                continue;
            }

            // The other possibilities are:
            // - we are staying still and have equal or earlier priority to the conflicting plan
            // - we are moving, in which case we made sure that any conflict is with a later-priority plan
            // So if there's a conflict, we win and the other guy has to re-plan
//            Log::flog(p.ship, "I win the conflict with ship id %d", conflict.ship.id);
//            Log::flog(conflict.ship, "Forced to replan by ship id %d", p.ship.id);
            q.push(conflict);
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
//        Log::flog(p.ship, "move_dest %s: %s is moving %s", +move_dest.toString(), +p.toString(), +desc(d));
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
            Log::log("shipyard won't be empty b/c ship at %s is moving onto it", +src.toString());
            return false;
        }
    }
    Log::log("shipyard will be clear!");
    return true;
}

void plan_returning(const vector<Ship> &returners)
{
    // compute the average halite richness near each possible destination
    const int R = 7;
    vector<pair<Vec, double>> structure_scores;
    for (Vec s : Game::me->structures) {
        double score = grid.halite_in_square(s, R);
        score /= (2*R + 1) * (2*R + 1);
        Log::flog(s, "STRUCTURE: avg halite in radius %d is %f", R, score);
        structure_scores.push_back({s, score});
    }

    for (Ship ship : returners) {
        Log::flog(ship, "return dests:");
        double best_score = -1e99;
        Vec tgt;
        for (pair<Vec, double> ss : structure_scores) {
            const Vec structure = ss.first;
            const double structure_score = ss.second;
            // TODO: consider tuning this
            double score = structure_score - 10 * grid.dist(ship.pos, structure);
            Log::flog(ship, "%s: %f", +structure.toString(), score);
            if (score > best_score) {
                best_score = score;
                tgt = structure;
            }
        }
        Log::flog(ship, "best return dest is %s", +tgt.toString());
        plan(ship, tgt, Purpose::Return);
    }
}

void plan_mining(const vector<Ship> &miners)
{
    Log::log("plan_mining start");

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
    for (Vec pos : grid.positions) {
        if (nearby_enemy_ship_count(pos) >= Constants::INSPIRATION_SHIP_COUNT) {
            effective_halite(pos) *= 1 + Constants::INSPIRED_BONUS_MULTIPLIER;
        }
    }
    

    // In four-player games it's probably good to try to mine in the general vicinity of
    // enemies. This should help attract them so they can get inspired, and then we can
    // get inspired too. 
    // First pass: let's implement this as a halite multiplier that's a function of distance
    // from enemy bases, but which goes away once someone builds a dropoff
    /*int total_dropoff_count = 0;
    IF YOU UNCOMMENT THIS CHANGE IT TO JUST MODIFY effective_halite
    for (const Player &p : Game::players) total_dropoff_count += p.dropoffs.size();
    if (Game::num_players == 4 && total_dropoff_count == 0) {
        for (Vec pos : grid.positions) {
            const int enemy_base_dist = grid.smallest_dist(pos, Game::enemy_structures);
            double mult;
            if (enemy_base_dist <= 10) {
                mult = 1.5;                
            } else if (enemy_base_dist <= 20) {
                mult = 1.0 + 0.05 * (20 - enemy_base_dist);
            } else {
                mult = 1.0;
            }
            enemy_base_dist_multiplier(pos) = mult;
            //Log::flog(pos, "enemy_base_dist=%d<br/>enemy_base_dist_multiplier=%f", enemy_base_dist, enemy_base_dist_multiplier(pos));
        }
    }*/

    // Halite in enemy ships will partially count toward increasing the effective halite on
    // the square. This will only have an effect if we consider the square passable, in which
    // case it will be an incentive to ram loaded enemy ships
    for (Ship enemy_ship : Game::enemy_ships) {
        effective_halite(enemy_ship.pos) += 0.5 * enemy_ship.halite;
    }

    // TEST: we'll remember each ship's expected halite rate, and use that to decide
    // whether to move to a distant mining target, or stay where we are
    map<int, double> sid_to_halite_rate;

    //map<int, int> sid_to_base_dist;
    //for (Ship s : Game::me->ships) sid_to_base_dist[s.id] = grid.smallest_dist(s.pos, Game::me->structures);

    PosMap<int> tgt_to_ship_id(-1);
    deque<Ship> q;
    std::copy(miners.begin(), miners.end(), std::back_inserter(q));
    while (!q.empty()) {
        Ship ship = q.front();
        q.pop_front();

        const double halite_needed = Constants::MAX_HALITE - ship.halite;

        //const int my_dist_from_base = sid_to_base_dist.at(ship.id);

        // pick the best target.
        double best_halite_rate = -1e99;
        Vec best_tgt = ship.pos;
        for (int x = 0; x < grid.width; ++x) {
            for (int y = 0; y < grid.height; ++y) {
                Vec pos{x, y};
                if (impassable(pos)) continue;
                const double halite = effective_halite(pos);
                if (halite <= 0) continue;

                const int dist_from_me = grid.dist(pos, ship.pos);

                // we can't pick a square that's already been picked
                // unless we are closer than the current owner
                const int owner_id = tgt_to_ship_id(pos);
                if (owner_id != -1) {
                    const Ship owner = Game::me->id_to_ship.at(owner_id);
                    //const int owner_dist_from_base = sid_to_base_dist.at(owner_id);
                    const int dist_from_owner = grid.dist(pos, owner.pos);
                    if (dist_from_owner <= dist_from_me) {
                        //Log::flog(ship, "can't pick %s b/c we aren't closer than %d", +pos.toString(), owner.id);
                        continue;
                    }
                    // TEST: can't take it if owner is closer to base, or if we're equal distances from base and we're not closer to the square itself
                    // consider the following hack: if you're on a structure, you can't choose a mining target that's next to the structure
                    // and already owned by someone who's on top of it???
                    /*if ((owner_dist_from_base < my_dist_from_base) 
                        || (owner_dist_from_base == my_dist_from_base && dist_from_owner <= dist_from_me)) {
                        continue;
                    }*/
                }

                // TODO: consider distance to all our structures, and the relative richness of the area around each structure
                const int dist_from_base = grid.smallest_dist(pos, Game::me->structures);
                // let's use a crude estimate of halite/turn. roughly speaking we're gonna
                // get (cell halite) / EXTRACT_RATIO halite per turn. And let's assume we can
                // mine at a similar rate from nearby cells. But multiply by a fudge factor of 0.7
                // because the halite decreases as we mine it. Then it'll take this long to fill up:
                const double naive_mine_rate = 0.7 * halite * Constants::INV_EXTRACT_RATIO;
                // Let's try to account for the 
                // fact that when you mine many small squares instead of one big square, you have to waste
                // time and halite moving.
                // Let's say your mining rate is 50% slower that=n the naive calculation
                const double efficiency = 0.5 + 0.5 * max(0.0, min((double)Constants::MAX_HALITE, halite)) / (double)Constants::MAX_HALITE;
                const double mine_rate = efficiency * naive_mine_rate;
                const double turns_to_fill_up = halite_needed / mine_rate;
                const double total_time = turns_to_fill_up + dist_from_base + 2 * dist_from_me;
                const double overall_halite_rate = halite_needed / total_time;

                if (overall_halite_rate > best_halite_rate) {
                    best_halite_rate = overall_halite_rate;
                    best_tgt = pos;
                }
            }
        }

        Log::flog(ship, "best_tgt = %s", +best_tgt.toString());

        // if our target is owned, kick out the owner
        const int owner_id = tgt_to_ship_id(best_tgt);
        if (owner_id != -1) {
            Log::flog(ship, "(kicking out previous owner %d)", owner_id);
            Log::flog(Game::me->id_to_ship.at(owner_id), "kicked out! by %d", ship.id);
            q.push_back(Game::me->id_to_ship.at(owner_id));
        }
        tgt_to_ship_id(best_tgt) = ship.id;
        sid_to_halite_rate[ship.id] = best_halite_rate;
    }

    for (Vec tgt : grid.positions) {
        const int ship_id = tgt_to_ship_id(tgt);
        if (ship_id == -1) continue;
        const Ship ship = Game::me->id_to_ship.at(ship_id);
        Log::flog(ship, "final mining tgt: %s", +tgt.toString());

        if (ship.pos == tgt) {
            Log::flog(ship, "mining tgt is here!");
            plan(ship, ship.pos, Purpose::Mine);
        } else {  // tgt is not the current position
            // decide whether to actually move toward the target
            const double tgt_eff_halite = effective_halite(tgt);
            const double here_eff_halite = effective_halite(ship.pos);
//            Log::flog(ship, "tgt_eff_halite = %f", tgt_eff_halite);
//            Log::flog(ship, "here_eff_halite = %f", here_eff_halite);
//            const double stay_halite_rate = here_eff_halite * Constants::INV_EXTRACT_RATIO;  // halite/turn over 1 turn of staying
//            const double go_halite_rate = sid_to_halite_rate.at(ship_id);  // calculated halite/turn of going to tgt
//            const double h_ratio = tgt_eff_halite/(double)here_eff_halite;
//            Log::flog(ship, "stay_halite_rate = %f", stay_halite_rate);
//            Log::flog(ship, "go_halite_rate = %f", go_halite_rate);
//            Log::flog(ship, "tgt/here halite ratio = %f", h_ratio);

            // tune the factor here
            if (tgt_eff_halite > 3.0 * here_eff_halite) { // default is 5
                Log::flog(ship, "going to move toward mining target %s", +tgt.toString());
                plan(ship, tgt, Purpose::Mine);
            } else {
                Log::flog(ship, "staying at %s", +ship.pos.toString());
                plan(ship, ship.pos, Purpose::Mine);
            }
        }

    }
    Log::log("plan_mining end");
}


// returns ship and tgt pos
int consider_making_dropoff(vector<Command> &commands)
{
    if (Game::turns_left() < 75) {
        Log::log("too late to make dropoff");
        return -1;
    }
    if (Game::me->ships.size() < -3 + 20 * (1 + Game::me->dropoffs.size())) {
        Log::log("not enough ships to make dropoff");
        return -1;
    }

    Vec best_pos;
    int best_score = -999999;

    // maybe this should be a global computed at the start of the turn
    PosMap<int> dist_to_structure(0);
    for (Vec pos : grid.positions) {
        dist_to_structure(pos) = grid.smallest_dist(pos, Game::me->structures);
        //Log::flog(pos, "dist_to_structure = %d", dist_to_structure(pos));
    }

    PosMap<int> dist_to_enemy_structure(0);
    for (Vec pos : grid.positions) {
        dist_to_enemy_structure(pos) = grid.smallest_dist(pos, Game::enemy_structures);
        //Log::flog(pos, "dist_to_enemy_structure = %d", dist_to_enemy_structure(pos));
    }

    PosMap<int> dist_to_enemy_ship(0);
    for (Vec pos : grid.positions) {
        dist_to_enemy_ship(pos) = grid.smallest_dist(pos, Game::enemy_ships);
        //Log::flog(pos, "dist_to_enemy_structure = %d", dist_to_enemy_structure(pos));
    }

    for (Vec pos : grid.positions) {
        // don't build structures too close together
        if (dist_to_structure(pos) < 17) continue;

        // can't build on top of enemy structures
        if (dist_to_enemy_structure(pos) == 0) continue;

        // crude heuristic:
        // don't build at a spot that's closer to an enemy structure than it
        // is to any of our structures, unless there are no nearby enemy ships
        // in a big radius        
        if (dist_to_enemy_structure(pos) <= dist_to_structure(pos) && dist_to_enemy_ship(pos) < 20) continue;

        const int radius = 7;        
        int new_halite_in_radius = 0;
        int old_halite_in_radius = 0;
        // let's only count halite that's significantly closer to this new dropoff
        // than to any existing structure
        for (int dy = -radius; dy <= radius; ++dy) {
            for (int dx = -radius; dx <= radius; ++dx) {
                Vec v = grid.add(pos, dx, dy);
                if (grid.dist(pos, v) < dist_to_structure(v)) {
                    new_halite_in_radius += grid(v).halite;
                } else {
                    old_halite_in_radius += grid(v).halite;
                }
            }
        }
        //Log::flog(pos, "new_halite_in_radius = %d", new_halite_in_radius);
        //Log::flog(pos, "old_halite_in_radius = %d", old_halite_in_radius);
        if (new_halite_in_radius < 25000) {
            continue;
        }

        if (new_halite_in_radius > best_score) {
            best_score = new_halite_in_radius;
            best_pos = pos;
        }
    }

    if (best_score < 0) {
        // not doing anything
        return -1;
    }

    Log::flog_color(best_pos, 255, 0, 0, "best dropoff target: score = %d", best_score);

    // find the nearest ship
    Ship ship = grid.closest(best_pos, Game::me->ships);
    Log::flog_color(ship.pos, 0, 0, 255, "going to dropoff target");
    if (ship.pos == best_pos) {
        // we're at the target!
        const int total_halite = Game::me->halite + ship.halite + grid(ship.pos).halite;
        if (total_halite >= Constants::DROPOFF_COST) {
            // can build, so do it
            commands.push_back(Command::construct(ship.id));
        } else {
            // can't build yet; chill out
            commands.push_back(Command::stay(ship));
        }
    } else {
        // not there yet; move toward the target
        plan(ship, best_pos, Purpose::Mine);  // maybe want a different Purpose
    }

    return ship.id;
}

set<int> consider_ramming(vector<Command> &commands, int dropoff_ship_id)
{
    set<int> ram_ship_ids;
    if (Game::num_players != 2) return ram_ship_ids;

    for (Ship enemy_ship : Game::enemy_ships) {
        if (contains(Game::enemy_structures, enemy_ship.pos)) continue;  // don't ram on top of enemy structures

        // let's say we need 2 friendly ships that are both closer than the closest enemy ship
        int dist_to_closest_enemy = 9999;
        for (Ship other_enemy : Game::enemy_ships) {
            if (other_enemy.id == enemy_ship.id) continue;
            const int dist = grid.dist(other_enemy.pos, enemy_ship.pos);
            if (dist < dist_to_closest_enemy) dist_to_closest_enemy = dist; 
        }
        int num_closer_allies = 0;
        for (Ship ally : Game::me->ships) {
            if (grid.dist(ally.pos, enemy_ship.pos) < dist_to_closest_enemy) num_closer_allies += 1;
        }
        //Log::flog(enemy_ship.pos, "dist_to_closest_enemy = %d", dist_to_closest_enemy);
        //Log::flog(enemy_ship.pos, "num_closer_allies = %d", num_closer_allies);
        if (num_closer_allies < 2) continue;

        // find the best adjacent allied rammer, if any
        Ship best_rammer;
        best_rammer.id = -1;
        best_rammer.halite = 99999;
        for (Ship my_ship : Game::me->ships) {
            if (grid.dist(my_ship.pos, enemy_ship.pos) != 1) continue;            
            if (!can_move(my_ship)) continue;
            if (my_ship.id == dropoff_ship_id) continue;
            if (ram_ship_ids.count(my_ship.id)) continue;            
            if (my_ship.halite >= enemy_ship.halite) continue;  // TODO: maybe relax this condition when we heavily outnumber?

            if (my_ship.halite < best_rammer.halite) {
                best_rammer = my_ship;
            }
        }
        if (best_rammer.id != -1) {
            // ramming speed!
            //Direction d = grid.adj_dir(best_rammer.pos, enemy_ship.pos);
            //commands.push_back(Command::move(best_rammer, d));
            ram_ship_ids.insert(best_rammer.id);
            // go through the planning system so other ships know to avoid this square
            plan(best_rammer, enemy_ship.pos, Purpose::Ram);
        }
    }
    return ram_ship_ids;
}

vector<Command> turn()
{
    plans.clear();

    set_impassable();

    vector<Command> commands;

    int dropoff_ship_id = consider_making_dropoff(commands);
    Log::log("dropoff_ship_id = %d", dropoff_ship_id);

    set<int> ram_ship_ids = consider_ramming(commands, dropoff_ship_id);    

    vector<Ship> miners;
    vector<Ship> returners;

    for (const Ship &ship : Game::me->ships) {
        if (ship.id == dropoff_ship_id) continue;
        if (ram_ship_ids.count(ship.id)) continue;

        // at the end of the game, return to the closest base
        const Vec closest_structure = grid.closest(ship.pos, Game::me->structures);
        const int dist_from_base = grid.dist(ship.pos, closest_structure);
        if (Game::turns_left() < dist_from_base + TURNS_LEFT_TO_RETURN) {
            plan(ship, closest_structure, Purpose::Return);
            continue;
        }


        bool returning;
        if (returning_ship_ids.count(ship.id)) {
            // this ship has been returning
            if (ship.halite <= 0) {
                returning_ship_ids.erase(ship.id);
                returning = false;
            } else {
                returning = true;
            }
        } else {
            // this ship has not been returning
            if (ship.halite >= Constants::MAX_HALITE) {
                returning_ship_ids.insert(ship.id);
                returning = true;
            } else {
                returning = false;
            }
        }

        if (returning) {
            returners.push_back(ship);
        } else {
            miners.push_back(ship);
        }
    }

    plan_returning(returners);
    plan_mining(miners);

    /*for (Plan p1 : plans) {
        for (Plan p2 : plans) {
            if (p1.ship.id == p2.ship.id) continue;
            if (grid.dist(p1.ship.pos, p2.ship.pos) == 1 && 
                p2.tgt == p2.ship.pos &&
                grid.dist(p1.tgt, p2.ship.pos) == 1 &&
                grid.adj_dir(p1.ship.pos, p2.ship.pos) == grid.adj_dir(p2.ship.pos, p1.tgt)) {
                Log::flog_color(p1.ship.pos, 255, 255, 0, "I'm trying to go right through %d", p2.ship.id);
                Log::flog_color(p2.ship.pos, 0, 255, 255, "I'm between %d and its target", p1.ship.id);
            }
        }
    }*/

    // let's try this as a postprocessing step. anyone who can't move 
    // must plan to stay where they are.
    for (Plan &p : plans) {
        if (!can_move(p.ship)) {
            Log::flog(p.ship, "Stuck b/c I have too little halite");
            p.tgt = p.ship.pos;
            p.purpose = Purpose::Stuck;
        }
    }

    make_final_collapse_commands(commands);

    resolve_plans_into_commands(commands);
    if (should_spawn(dropoff_ship_id) && shipyard_will_be_clear(commands)) {
        commands.push_back(Command::generate());
    }

    return commands;
}


int main()
{
    Network::init();

    // start the main game loop
    while (true) {
        Network::begin_turn();



        vector<Command> commands = turn();

        Network::end_turn(commands);
    }

    return 0;
}