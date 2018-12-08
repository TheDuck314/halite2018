#include "PosMap.h"

#include <sstream>

string toString(const PosSet &ps)
{
    stringstream ss;
    ss << "{";
    for (int x = 0; x < grid.width; ++x) {
        for (int y = 0; y < grid.height; ++y) {
            Vec pos{x, y};
            if (ps(pos)) ss << pos.toString() << " ";
        }
    }
    ss << "}";
    return ss.str();
}