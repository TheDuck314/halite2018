#include "Geometry.h"
#include "StringUtil.h"
#include "Log.h"

string desc(Direction d)
{
    switch (d) {
        case Direction::North: return "North";
        case Direction::South: return "South";
        case Direction::East:  return "East";
        case Direction::West:  return "West";
        case Direction::Still: return "Still";
        default:
            Log::die("desc invalid direction %d", (int)d);
            return "";
    }
}

string Vec::toString() const
{
    return stringf("{%2d, %2d}", x, y);
}

Direction closest_dir(Vec displacement)
{
    if (displacement.x == 0 and displacement.y == 0) return Direction::Still;
    if (abs(displacement.x) > abs(displacement.y)) {
        if (displacement.x > 0) return Direction::East;
        else return Direction::West;
    } else {
        if (displacement.y > 0) return Direction::South;
        else return Direction::North;
    }
}