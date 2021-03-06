#include "Path.h"

#include <deque>
#include <queue>
#include "Constants.h"
#include "Log.h"
#include "Grid.h"
#include "Util.h"
using namespace std;

struct PathQueueElem
{
    Vec pos;
    Direction initial_dir;
};

// very dumb
Direction Path::simple(Vec src, Vec dest, const PosSet &blocked)
{
    for (Direction d : grid.strict_approach_dirs(src, dest)) {
        Vec d_pos = grid.add(src, d);
        if (!blocked(d_pos)) {
            // found a valid move
            return d;
        }
    }
    return Direction::Still;
}

Direction Path::find(Vec src, Vec dest, const PosSet &blocked, const PosSet &prefer_to_avoid)
{
    if (src == dest) {
        return Direction::Still;
    }
    if (blocked(dest)) {
        // do the best we can
        return simple(src, dest, blocked);
    }
    PosSet finished(blocked);
    finished(src) = true;
    deque<PathQueueElem> q;

    // It helps to pick the initial directions in the order of which
    // ones naively bring us toward the target.
    vector<Direction> initial_dirs = grid.approach_dirs(src, dest);
    for (int d = 0; d < 4; ++d) {
        if (!contains(initial_dirs, (Direction)d)) {
            initial_dirs.push_back((Direction)d);
        }
    }

    vector<Direction> initial_dirs_with_avoid_ordering;
    for (Direction d : initial_dirs) {
        if (!prefer_to_avoid(grid.add(src, d))) initial_dirs_with_avoid_ordering.push_back(d);
    }
    for (Direction d : initial_dirs) {
        if (prefer_to_avoid(grid.add(src, d))) initial_dirs_with_avoid_ordering.push_back(d);
    }
    initial_dirs = initial_dirs_with_avoid_ordering;

    for (Direction d : initial_dirs) {
        Vec adj = grid.add(src, d);
        if (adj == dest) {
            return d;
        }
        if (finished(adj)) {
            continue;
        }
        q.push_back({adj, d});
        finished(adj) = true;
    }
    while (!q.empty()) {
        PathQueueElem e = q.front();
        q.pop_front();
        for (int d = 0; d < 4; ++d) {
            Vec adj = grid.add(e.pos, (Direction)d);
            if (finished(adj)) {
                continue;
            }
            if (adj == dest) {
                return e.initial_dir;
            }
            q.push_back({adj, e.initial_dir});
            finished(adj) = true;
        }
    }    

    // fancy search failed. Do a simple fallback thing.
    return simple(src, dest, blocked); 
}


struct LowHalitePathQueueElem
{
    Vec pos;
    Direction initial_dir;
    int dist_so_far;
    int halite_so_far;
};

Direction Path::find_low_halite(Vec src, Vec dest, const PosSet &blocked)
{
//    Log::flog(src, "find_low_halite to %s", +dest.toString());
    if (src == dest) {
//        Log::flog(src, "find_low_halite at dest already!");
        return Direction::Still;
    }

    if (blocked(dest)) {
        // do the best we can
//        Log::flog(src, "find_low_halite dest is blocked :(");
        return simple(src, dest, blocked);
    }

    auto elem_cmp = [](LowHalitePathQueueElem a, LowHalitePathQueueElem b) {
        if (a.dist_so_far != b.dist_so_far) return a.dist_so_far > b.dist_so_far;
        return a.halite_so_far > b.halite_so_far;
    };
    priority_queue<LowHalitePathQueueElem, vector<LowHalitePathQueueElem>, decltype(elem_cmp)> q(elem_cmp);

    PosSet finished(blocked);
    finished(src) = true;

    for (int d = 0; d < 4; ++d) {
        Vec adj = grid.add(src, (Direction)d);
        if (adj == dest) {
//            Log::flog(src, "find_low_halite dest is adjacent");
            return (Direction)d;
        }
        if (finished(adj)) {
            continue;
        }
        q.push({adj, (Direction)d, 2, 0});
    }

    while (!q.empty()) {
//        Log::log("q.size() = %d", (int)q.size());
        LowHalitePathQueueElem e = q.top();
        q.pop();
//        Log::log("popped e.pos=%s e.initial_dir=%s e.dist_so_far=%d e.halite_so_far=%d", +e.pos.toString(), +desc(e.initial_dir), e.dist_so_far, e.halite_so_far);

        if (finished(e.pos)) {
            // we already got to this position by a lower-halite route.
            continue;
        }
        finished(e.pos) = true;

        if (e.pos == dest) {
//            Log::flog(src, "find_low_halite e.dist_so_far=%d e.halite_so_far=%d initial_dir=%s", e.dist_so_far, e.halite_so_far, +desc(e.initial_dir));
            return e.initial_dir;
        }

        const int dist_so_far = e.dist_so_far + 1;
        const int halite_so_far = e.halite_so_far + grid(e.pos).halite;
        for (int d = 0; d < 4; ++d) {
            Vec adj = grid.add(e.pos, (Direction)d);
            if (finished(adj)) {
                continue;
            }
            LowHalitePathQueueElem to_push{adj, e.initial_dir, dist_so_far, halite_so_far};
//            Log::log("pushing: to_push.pos=%s to_push.initial_dir=%s to_push.dist_so_far=%d to_push.halite_so_far=%d", +to_push.pos.toString(), +desc(to_push.initial_dir), to_push.dist_so_far, to_push.halite_so_far);
            q.push(to_push);
        }
    }

    // fancy search failed. Do a simple fallback thing.
//    Log::flog(src, "find_low_halite search failed :(");
    return simple(src, dest, blocked);;
}

struct TravelCostQueueElem
{
    Vec pos;
    int dist_so_far;
    int halite_so_far;
};

PosMap<int> Path::build_halite_travel_cost_map(Vec src)
{
    PosMap<int> travel_cost(0);

    auto elem_cmp = [](TravelCostQueueElem a, TravelCostQueueElem b) {
        if (a.dist_so_far != b.dist_so_far) return a.dist_so_far > b.dist_so_far;
        return a.halite_so_far > b.halite_so_far;
    };
    priority_queue<TravelCostQueueElem, vector<TravelCostQueueElem>, decltype(elem_cmp)> q(elem_cmp);

    PosSet finished(false);
    finished(src) = true;

    const int src_halite = grid(src).halite;
    for (int d = 0; d < 4; ++d) {
        Vec adj = grid.add(src, (Direction)d);
        if (finished(adj)) continue;
        q.push({adj, 2, src_halite});
    }

    while (!q.empty()) {
        TravelCostQueueElem e = q.top();
        q.pop();

        if (finished(e.pos)) continue;
        finished(e.pos) = true;

        travel_cost(e.pos) = e.halite_so_far / Constants::MOVE_COST_RATIO;

        const int dist_so_far = e.dist_so_far + 1;
        const int halite_so_far = e.halite_so_far + grid(e.pos).halite;
        for (int d = 0; d < 4; ++d) {
            Vec adj = grid.add(e.pos, (Direction)d);
            if (finished(adj)) continue;
            TravelCostQueueElem to_push{adj, dist_so_far, halite_so_far};
            q.push(to_push);
        }
    }

    return travel_cost;
}

struct TravelTimeQueueElem
{
    Vec pos;
    int dist_so_far;
};

PosMap<int> Path::build_halite_travel_time_map(Vec src, const PosSet &blocked)
{
    PosMap<int> travel_time_map(9999);  // blocked squares will have cost 9999
    PosMap<bool> finished(false);
    deque<TravelTimeQueueElem> q;

    q.push_back({src, 0});

    while (!q.empty()) {
        TravelTimeQueueElem e = q.front();
        q.pop_front();

        if (finished(e.pos)) continue;
        travel_time_map(e.pos) = e.dist_so_far;
        finished(e.pos) = true;

        for (int d = 0; d < 4; ++d) {
            Vec adj = grid.add(e.pos, (Direction)d);
            if (finished(adj) || blocked(adj)) {
                continue;
            }
            q.push_back({adj, e.dist_so_far + 1});
        }
    }    

    // fancy search failed. Do a simple fallback thing.
    return travel_time_map;
}