#include "PathHasher.h"

#include <random>

#include "Grid.h"
#include "Log.h"

PathHasher::PathHasher()
  : turn_square_bits(64 * 64 * MAX_LEN)
{
    std::mt19937 rng(12345);
    for (unsigned i = 0; i < turn_square_bits.size(); ++i) {
        turn_square_bits[i] = rng();
    }
}

int PathHasher::hash(const vector<Vec> &path)
{
    if (path.size() > MAX_LEN) Log::die("asked to hash too-long path");

    int ret = 0;
    for (unsigned i = 0; i < path.size(); ++i) {
        const Vec square = path[i];
        const int index = (i << 12) | (square.y << 6) | square.x;
        ret |= turn_square_bits[index];
    }
    return ret;
}