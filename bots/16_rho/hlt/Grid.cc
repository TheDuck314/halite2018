#include "Grid.h"
#include "Log.h"

Grid grid;

void Grid::resize(int _width, int _height)
{
    width = _width;
    height = _height;

    cells.resize(width * height);

    positions.resize(width * height);
    for (int x = 0; x < width; ++x) {
        for (int y = 0; y < height; ++y) {
            positions[width*y + x] = {x, y};
        }
    }
}

Cell& Grid::operator()(int x, int y)
{
    if (x < 0 || y < 0 || x >= width || y >= width) {
        Log::die("Grid operator() invalid coords (%d, %d) when grid size is (%d, %d)", x, y, width, height);
    }
    return cells[width*y + x];
}

Vec Grid::pos_delta(Vec src, Vec dest) const
{
    int dx = dest.x - src.x + width;
    if (dx >= width) dx -= width;
    if (dx > width/2) dx -= width; 

    int dy = dest.y - src.y + height;
    if (dy >= height) dy -= height;
    if (dy > height/2) dy -= height;
    return {dx, dy};
}

Vec Grid::add(Vec pos, int dx, int dy) const
{
    Vec ret;
    ret.x = (pos.x + dx + 2 * width) % width;
    ret.y = (pos.y + dy + 2 * height) % height;
    return ret;
}


int Grid::dist(Vec a, Vec b) const
{
    int dx = abs(a.x - b.x);
    if (dx > width/2) dx = width - dx;
    
    int dy = abs(a.y - b.y);
    if (dy > height/2) dy = height - dy;

    return dx + dy;
}

Vec Grid::closest(Vec src, const vector<Vec> &dests) const
{
    if (dests.empty()) {
        Log::die("closest: dests is empty!");
    }

    int best_dist = 999999999;
    Vec ret;
    for (Vec dest : dests) {
        const int this_dist = dist(src, dest);
        if (this_dist < best_dist) {
            best_dist = this_dist;
            ret = dest;
        }
    }
    return ret;
}

Ship Grid::closest(Vec src, const vector<Ship> &ships) const
{
    if (ships.empty()) {
        Log::die("closest: ships is empty!");
    }

    int best_dist = 999999999;
    Ship ret;
    for (Ship ship : ships) {
        const int this_dist = dist(src, ship.pos);
        if (this_dist < best_dist) {
            best_dist = this_dist;
            ret = ship;
        }
    }
    return ret;
}

int Grid::smallest_dist(Vec src, const vector<Vec> &dests) const
{
    if (dests.empty()) {
        Log::die("closest: dests is empty!");
    }

    int best_dist = 999999999;
    for (Vec dest : dests) {
        const int this_dist = dist(src, dest);
        if (this_dist < best_dist) {
            best_dist = this_dist;
        }
    }
    return best_dist;
}

int Grid::smallest_dist(Vec src, const vector<Ship> &ships) const
{
    // this'll return a really big number of ships is empty.
    int best_dist = 999999999;
    for (Ship ship : ships) {
        const int this_dist = dist(src, ship.pos);
        if (this_dist < best_dist) {
            best_dist = this_dist;
        }
    }
    return best_dist;
}

int Grid::smallest_dist_except(Vec src, const vector<Ship> &ships, Ship exclude) const
{
    // this'll return a really big number if ships is empty or just has exclude
    int best_dist = 999999999;
    for (Ship ship : ships) {
        if (ship.id == exclude.id) continue;
        const int this_dist = dist(src, ship.pos);
        if (this_dist < best_dist) {
            best_dist = this_dist;
        }
    }
    return best_dist;   
}

int Grid::second_smallest_dist(Vec src, const vector<Ship> &ships) const
{
    int best_dist = 999999;
    int second_best_dist = 999999;
    for (Ship ship : ships) {
        const int this_dist = dist(src, ship.pos);
        if (this_dist < best_dist) {
            second_best_dist = best_dist;
            best_dist = this_dist;
        } else if (this_dist < second_best_dist) {
            second_best_dist = this_dist;
        }
    }
    return second_best_dist;
}

int Grid::num_within_dist(Vec src, const vector<Ship> &ships, int R) const
{
    int n = 0;
    for (Ship ship : ships) {
        if (dist(src, ship.pos) <= R) n += 1;
    }
    return n;
}

Vec Grid::add(Vec pos, Direction dir) const
{
    switch (dir) {
        case Direction::North:
            pos.y -= 1;
            if (pos.y < 0) pos.y += height;
            break;

        case Direction::South:
            pos.y += 1;
            if (pos.y >= height) pos.y -= height;
            break;

        case Direction::East:
            pos.x += 1;
            if (pos.x >= width) pos.x -= width;
            break;

        case Direction::West:
            pos.x -= 1;
            if (pos.x < 0) pos.x += width;
            break;

        default: // Direction::Still
            break;
    }

    return pos;
}

Direction Grid::adj_dir(Vec src, Vec dest) const
{
    Vec delta = pos_delta(src, dest);
    if (delta.x > 0) return Direction::East;
    else if (delta.x < 0) return Direction::West;
    else if (delta.y > 0) return Direction::South;
    else if (delta.y < 0) return Direction::North;
    else return Direction::Still;
}


vector<Direction> Grid::approach_dirs(Vec src, Vec dest) const
{
    const Vec d = pos_delta(src, dest);
    const int dx = d.x;
    const int dy = d.y;

    // 13 possibilities
    if (dx > 0) {
        if (dy > dx) return { Direction::South, Direction::East };
        else if (dy > 0) return { Direction::East, Direction::South };
        else if (dy == 0) return { Direction::East, Direction::North, Direction::South };
        else if (dy > -dx) return { Direction::East, Direction::North };
        else /* dy <= -dx */ return {Direction::North, Direction::East };
    } else if (dx == 0) {
        if (dy > 0) return { Direction::South, Direction::East, Direction::West };
        else if (dy == 0) return {};
        else /* dy < 0 */ return {Direction::North, Direction::West, Direction::East };
    } else { /* dx < 0 */
        if (dy > -dx) return { Direction::South, Direction::West };
        else if (dy > 0) return { Direction::West, Direction::South };
        else if (dy == 0) return { Direction::West, Direction::South, Direction::North };
        else if (dy > dx) return { Direction::West, Direction::North };
        else /* dy <= dx */ return { Direction::North, Direction::West };
    }
}

vector<Direction> Grid::strict_approach_dirs(Vec src, Vec dest) const
{
    const Vec d = pos_delta(src, dest);
    const int dx = d.x;
    const int dy = d.y;

    // 13 possibilities
    if (dx > 0) {
        if (dy > dx) return { Direction::South, Direction::East };
        else if (dy > 0) return { Direction::East, Direction::South };
        else if (dy == 0) return { Direction::East};
        else if (dy > -dx) return { Direction::East, Direction::North };
        else /* dy <= -dx */ return {Direction::North, Direction::East };
    } else if (dx == 0) {
        if (dy > 0) return { Direction::South};
        else if (dy == 0) return {};
        else /* dy < 0 */ return {Direction::North};
    } else { /* dx < 0 */
        if (dy > -dx) return { Direction::South, Direction::West };
        else if (dy > 0) return { Direction::West, Direction::South };
        else if (dy == 0) return { Direction::West};
        else if (dy > dx) return { Direction::West, Direction::North };
        else /* dy <= dx */ return { Direction::North, Direction::West };
    }
}

int Grid::halite_in_square(Vec center, int R) const
{
    int ret = 0;
    for (int dx = -R; dx <= R; ++dx) {
        for (int dy = -R; dy <= R; ++dy) {
            Vec pos = add(center, dx, dy);
            ret += (*this)(pos).halite;
        }
    }
    return ret;
}

int Grid::halite_in_manhattan_radius(Vec center, int R) const
{
    int ret = 0;
    for (int dx = -R; dx <= R; ++dx) {
        const int y_radius = R - abs(dx);
        for (int dy = -y_radius; dy <= y_radius; ++dy) {
            Vec pos = add(center, dx, dy);
            ret += (*this)(pos).halite;
        }
    }
    return ret;
}