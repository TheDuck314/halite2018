#include <vector>
#include <unordered_set>
#include "hlt/Constants.h"
#include "hlt/Game.h"
#include "hlt/Network.h"
#include "hlt/Grid.h"
#include "hlt/Log.h"
#include "hlt/Path.h"
#include "hlt/PosMap.h"
using namespace std;

unordered_set<int> returning_ship_ids;

struct Plan 
{
    Ship ship;
    Vec tgt;
};

vector<Plan> plans;

const int TURNS_LEFT_TO_RETURN = 15;

void plan(const Ship &ship, Vec tgt)
{
    plans.push_back({ship, tgt});
}

bool should_spawn()
{
    if (Game::me->halite < Constants::SHIP_COST) return false;  // can't spawn
    if (Game::turns_left() < 200) return false;

    int halite_left = 0;
    for (int x = 0; x < grid.width; ++x) {
        for (int y = 0; y < grid.height; ++y) {
            halite_left += grid({x, y}).halite;
        }
    }
    // check if we already have plenty of ships
    if (Game::num_players == 4) {
        if (halite_left < 3500 * (int)Game::me->ships.size()) return false;
    } else { // two players
        if (halite_left < 2500 * (int)Game::me->ships.size()) return false;
    }

    return Game::turn < Constants::MAX_TURNS - 200;
}

void return_halite(const Ship &ship)
{
    Vec tgt = grid.closest(ship.pos, Game::me->structures);
    Log::flog(ship, "return_halite: tgt = %s", +tgt.toString());
    plan(ship, tgt);
}

Vec pick_mining_target(const Ship &ship, const PosSet &invalid_tgts)
{
    double best_score = -1e99;
    Vec best_tgt = ship.pos;
    for (int x = 0; x < grid.width; ++x) {
        for (int y = 0; y < grid.height; ++y) {
            Vec pos{x, y};
            if (pos != ship.pos && invalid_tgts(pos)) continue;
            const int halite = grid(pos).halite;
            if (halite <= 0) continue;

            const int dist_from_me = grid.dist(pos, ship.pos);
            const Vec closest_structure = grid.closest(pos, Game::me->structures);
            const int dist_from_base = grid.dist(pos, closest_structure);

            // let's use a crude estimate of halite/turn. roughly speaking we're gonna
            // get (cell halite) / EXTRACT_RATIO halite per turn. And let's assume we can
            // mine at a similar rate from nearby cells. Then it'll take this long to fill up:
            const double mine_rate = halite * Constants::INV_EXTRACT_RATIO;
            const double turns_to_fill_up = Constants::MAX_HALITE / mine_rate;
            // note the 5* fudge factor:
            const double overall_rate = (mine_rate * turns_to_fill_up) / (turns_to_fill_up + dist_from_base + 3 * dist_from_me);

            if (overall_rate > best_score) {
                best_score = overall_rate;
                best_tgt = pos;
            }
        }
    }
    Log::flog(ship, "mining target: %s",+best_tgt.toString());
    return best_tgt;
}

void mine_halite(const Ship &ship, const PosSet &invalid_mining_tgts)
{
    Log::flog(ship, "mine_halite"); 

    const Vec tgt = pick_mining_target(ship, invalid_mining_tgts);
    const int tgt_halite = grid(tgt).halite;
    const int here_halite = grid(ship.pos).halite;
    if (tgt_halite > 5 * here_halite) {
        Log::flog(ship, "going to move toward mining target %s", +tgt.toString());
        plan(ship, tgt);
    } else {
        Log::flog(ship, "staying at %s", +ship.pos.toString());
        plan(ship, ship.pos);
    }
}


vector<Command> resolve_plans_into_commands()
{
    // keep a queue of ships that we need to route. do this loop:
    // - pop a ship off the queue
    // - find a move for this ship that doesn't hit a currently-blocked square
    // - if successful
    //     - dest square of this ship becomes blocked
    //     - remember the dest square -> ship gridping, because we may need to undo this
    // - if unsuccessful
    //     - stay put
    //     - if another ship is trying to come to this square, kick it out and put it back in the queue
    map<int, Vec> ship_id_to_tgt;
    for (const Plan &p : plans) ship_id_to_tgt[p.ship.id] = p.tgt;

    vector<Command> commands;

    // build a set of positions we must not move onto.
    // for now, just the positions of enemy ships, or positions they
    // could move to next turn
    PosSet avoid_positions(false);
    for (const Player &player : Game::players) {
        if (player.id == Game::me->id) continue;
        for (const Ship &enemy_ship : player.ships) {
            // we need to ignore enemy ships that are on top of our structures
            // or they'll block us from delivering. it's fine to ram them.
            bool on_my_structure = false;
            for (Vec s : Game::me->structures) {
                if (enemy_ship.pos == s) {
                    on_my_structure = true;
                    break;
                }
            }
            if (on_my_structure) continue;

            // avoid this enemy ship and adjacent positions, but don't
            // avoid our own structures
            for (int d = 0; d < 5; ++d) {
                Vec adj = grid.add(enemy_ship.pos, (Direction)d);
                if (!Game::me->has_structure_at(adj)) { 
                    avoid_positions(adj) = true;
                }
            }
        }
    }

    if (Game::turns_left() < TURNS_LEFT_TO_RETURN) {
        // hack: if there are multiple ships within 1 of a strucutre, they are all 
        // gonna go onto the same square, and our pos_to_arriving_ship framework 
        // doesn't really handle that. So just make commands for those guys right now,
        // and remove them from plans.
        vector<Plan> unhandled_plans;
        for (const Plan &p : plans) {
            bool handled = false;
            for (Vec structure : Game::me->structures) {
                if (grid.dist(p.ship.pos, structure) <= 1 && p.tgt == structure) {
                    Direction d = grid.adj_dir(p.ship.pos, structure);
                    commands.push_back(Command::move(p.ship, d));                    
                    handled = true;
                    Log::flog(p.ship, "moving %s as part of final collapse", +desc(d));
                    break;
                }
            }
            if (!handled) unhandled_plans.push_back(p);
        }
        plans = unhandled_plans;
    }

    Log::log("going to resolve these plans into commands:");
    for (const Plan &p : plans) {
        Log::log("%s -> %s", +p.ship.toString(), +p.tgt.toString()); 
    }

    PosMap<int> pos_to_arriving_ship_id(-1);

    // little optimization:
    // let's first handle all the plans that just require staying put.
    // this'll make pathfinding better since it will know to path around these guys
    vector<Plan> unhandled_plans;
    for (const Plan &p : plans) {
        if (p.ship.pos == p.tgt) {
            avoid_positions(p.tgt) = true;
            commands.push_back(Command::stay(p.ship));
        } else {
            unhandled_plans.push_back(p);
        }
    }
    plans = unhandled_plans;

    unsigned i = 0;  // gonna use plans as a crude queue with pop() implemented as i++
    while (i < plans.size()) {
        Plan p = plans[i++];
        Direction path_dir = Path::find(p.ship.pos, p.tgt, avoid_positions);
        Log::flog(p.ship, "want to go to %s. initial path_dir=%s", 
                   +p.tgt.toString(), +desc(path_dir));
        if (path_dir == Direction::Still) {
            // try a dumber pathfinding algorithm
            for (Direction d : grid.approach_dirs(p.ship.pos, p.tgt)) {
                Vec d_pos = grid.add(p.ship.pos, d);
                if (avoid_positions(d_pos)) continue;
                // found a valid move
                path_dir = d;
                break;
            }
        }
        Log::flog(p.ship, "after potential fallback pathing, dir = %s", +desc(path_dir));
        if (path_dir != Direction::Still) {
            // one of the pathfinding attempts worked
            Vec d_pos = grid.add(p.ship.pos, path_dir);
            pos_to_arriving_ship_id(d_pos) = p.ship.id;
            avoid_positions(d_pos) = true;
        } else {
            // we couldn't find any useful move
            Log::flog(p.ship, "staying put");
            int interloper_id = pos_to_arriving_ship_id(p.ship.pos);
            if (interloper_id != -1) {
                // make the interloper rethink his move -- put him back in the queue
                Ship interloper = Game::me->id_to_ship.at(interloper_id);
                Vec tgt = ship_id_to_tgt[interloper_id];
                Log::flog(p.ship, "kicking out %s who thought he was going to come into my square!", +interloper.toString());
                Log::flog(interloper, "got kicked out of %s by %s", +p.ship.pos.toString(), +p.ship.toString());
                plans.push_back({interloper, tgt});
            }
            pos_to_arriving_ship_id(p.ship.pos) = p.ship.id;
            avoid_positions(p.ship.pos) = true;
        }
    }

    Log::log("pos_to_arriving_ship_id looks like this:");
    for (int x = 0; x < grid.width; ++x) {
        for (int y = 0; y < grid.height; ++y) {
            Vec pos{x, y};
            int arriving_id = pos_to_arriving_ship_id(pos);
            if (arriving_id == -1) continue;
            Ship arriving_ship = Game::me->id_to_ship[arriving_id];
            Direction d = grid.adj_dir(arriving_ship.pos, pos);
            Log::flog(arriving_ship, "arriving at %s by moving %s", +pos.toString(), +desc(d));
            commands.push_back(Command::move(arriving_ship, d));
        }
    }

    Log::log("returning resolved commands");
    return commands;
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

int consider_making_dropoff()
{
    if (Game::me->halite < Constants::DROPOFF_COST) {
        Log::log("not enough halite to make dropoff");
        return -1;
    }
    if (Game::turns_left() < 75) {
        Log::log("too late to make dropoff");
        return -1;
    }
    if (Game::me->ships.size() < 15) {
        Log::log("not enough ships to make dropoff");
        return -1;
    }

    int best_id = -1;
    int best_score = -999999;

    for (Ship ship : Game::me->ships) {
        bool too_close = false;
        for (Vec s : Game::me->structures) {
            if (grid.dist(ship.pos, s) < 17) {
                too_close = true;
                break;
            }
        }
        if (too_close) {
            Log::log("%s too close to structure to make dropoff", +ship.toString());
            continue;
        }

        const int radius = 7;
        int halite_in_radius = 0;
        for (int dx = -radius; dx <= radius; ++dx) {
            for (int dy = -radius; dy <= radius; ++dy) {
                Vec pos = grid.add(ship.pos, dx, dy);
                halite_in_radius += grid(pos).halite;
            }
        }
        if (halite_in_radius < 25000) {
            Log::log("%s halite in radius %d is too small to make dropoff", +ship.toString(), halite_in_radius);
            continue;
        }

        if (halite_in_radius > best_score) {
            Log::flog(ship, "halite in radius = %d", halite_in_radius);
            best_score = halite_in_radius;
            best_id = ship.id;
        }
    }

    return best_id;
}


vector<Command> turn()
{
    plans.clear();

    // build the set of invalid mining targets: any square that
    // has one of our ships on it, or is adjacent to an enemy ship
    PosSet invalid_mining_tgts(false);
    for (const Ship &ship : Game::me->ships) {
        Log::log("invalid mining tgt (our ship): %s", +ship.pos.toString());
        invalid_mining_tgts(ship.pos) = true;
    }
    for (const Player &player : Game::players) {
        if (player.id == Game::me->id) continue;
        for (const Ship &ship : player.ships) {
            for (int d = 0; d < 5; ++d) {
                const Vec pos = grid.add(ship.pos, (Direction)d);
                Log::log("invalid mining tgt (adj to enemy): %s", +pos.toString());
                invalid_mining_tgts(pos) = true;
            }   
        }
    }

    int dropoff_ship_id = consider_making_dropoff();
    if (dropoff_ship_id != -1) {
        Game::me->halite -= Constants::DROPOFF_COST;
    }
    Log::log("dropoff_ship_id = %d", dropoff_ship_id);

    for (const Ship &ship : Game::me->ships) {
        if (ship.id == dropoff_ship_id) continue;

        // if a ship doesn't have enough halite to move, just stay put
        if (grid(ship.pos).halite / Constants::MOVE_COST_RATIO > ship.halite) {
            Log::flog(ship, "Stuck b/c I have too little halite");
            plan(ship, ship.pos);
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
        // also, at the end of the game, return
        const Vec closest_structure = grid.closest(ship.pos, Game::me->structures);
        const int dist_from_base = grid.dist(ship.pos, closest_structure);
        if (Game::turns_left() < dist_from_base + TURNS_LEFT_TO_RETURN) {
            returning_ship_ids.insert(ship.id);
            returning = true;
        }


        if (returning) {
            return_halite(ship);
        } else {
            mine_halite(ship, invalid_mining_tgts);
        }
    }

    vector<Command> commands = resolve_plans_into_commands();
    if (dropoff_ship_id != -1) {
        commands.push_back(Command::construct(dropoff_ship_id));
    }
    if (should_spawn() && shipyard_will_be_clear(commands)) {
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