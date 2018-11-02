#include "Path.h"

#include <deque>
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

Direction Path::find(Vec src, Vec dest, const PosSet &blocked)
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
    return simple(src, dest, blocked);;
}


// TEST
// never consider moving away from the target
Direction Path::find_strict(Vec src, Vec dest, const PosSet &blocked)
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
    vector<Direction> initial_dirs = grid.strict_approach_dirs(src, dest);
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
        for (Direction d : grid.strict_approach_dirs(e.pos, dest)) {
            Vec adj = grid.add(e.pos, d);
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