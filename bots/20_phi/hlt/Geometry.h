#pragma once
#include <string>
using namespace std;

enum class Direction
{
    North = 0,
    South = 1,
    East  = 2,
    West  = 3,
    Still = 4,
};

string desc(Direction d);

struct Vec 
{
    int x;
    int y;

    string toString() const;

    bool operator==(Vec rhs) const { return x == rhs.x && y == rhs.y; }
    bool operator!=(Vec rhs) const { return x != rhs.x || y != rhs.y; }
    bool operator<(Vec rhs) const {
        if (y == rhs.y) return x < rhs.x;
        else return y < rhs.y;
    }
};


Direction closest_dir(Vec displacement);