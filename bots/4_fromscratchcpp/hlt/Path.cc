#include "Path.h"

#include <deque>
#include "Log.h"
#include "Grid.h"
using namespace std;

struct PathQueueElem
{
    Vec pos;
    Direction initial_dir;
};

template<class T>
bool contains(const vector<T> &v, T t)
{
    for (auto it = v.begin(); it != v.end(); ++it) {
        if (*it == t) return true;
    }
    return false;
}

Direction Path::find(Vec src, Vec dest, const PosSet &blocked)
{
//    Log::log("find_path src=%s dest=%s blocked_positions=%s", +src.toString(), +dest.toString(), +toString(blocked));
    if (src == dest) {
//        Log::log("trivial b/c src = dest");
        return Direction::Still;
    }
    if (blocked(dest)) {
//        Log::log("trivial b/c dest is blocked!");
        return Direction::Still;
    }
    PosSet finished(blocked);
    finished(src) = true;
    deque<PathQueueElem> q;

    // I think it'll help to pick the initial directions in the order
    // of a naive approach dir.
    vector<Direction> initial_dirs = grid.approach_dirs(src, dest);
    for (int d = 0; d < 4; ++d) {
        if (!contains(initial_dirs, (Direction)d)) {
            initial_dirs.push_back((Direction)d);
        }
    }
/*    Log::flog(src, "find src=%s dest=%s initial_dirs = %s %s %s %s",
        +src.toString(), +dest.toString(),
        +desc(initial_dirs[0]),
        +desc(initial_dirs[1]),
        +desc(initial_dirs[2]),
        +desc(initial_dirs[3]));*/

    for (Direction d : initial_dirs) {
        Vec adj = grid.add(src, d);
        if (adj == dest) {
//            Log::log("adjacent case triggered, d = %s", +desc((Direction)d));
            return d;
        }
        if (finished(adj)) {
//            Log::log("adjacent thing adj=%s d=%s is finished already", +adj.toString(), +desc((Direction)d));
            continue;
        }
        q.push_back({adj, d});
        finished(adj) = true;
    }
    while (!q.empty()) {
        PathQueueElem e = q.front();
        q.pop_front();
//        Log::log("popped e.pos = %s  e.initial_dir = %s", +e.pos.toString(), +desc(e.initial_dir));
        for (int d = 0; d < 4; ++d) {
            Vec adj = grid.add(e.pos, (Direction)d);
//            Log::log("d = %s  adj = %s", +desc((Direction)d), +adj.toString());
            if (finished(adj)) {
//                Log::log("adj is already finished, forget it");
                continue;
            }
            if (adj == dest) {
//                Log::log("found dest! initial_dir = {}", +desc(e.initial_dir));
                return e.initial_dir;
            }
            q.push_back({adj, e.initial_dir});
            finished(adj) = true;
        }
    }
//    Log::log("failed to find a route!!!!");
    return Direction::Still;
}