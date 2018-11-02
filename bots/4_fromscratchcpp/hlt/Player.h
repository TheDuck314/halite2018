#pragma once

#include <map>
#include <string>
#include <vector>
#include "Geometry.h"
using namespace std;

struct Ship
{
    int id;
    Vec pos;
    int halite;

    string toString() const;
};

struct Dropoff
{
    int id;
    Vec pos;

    string toString() const;
};

struct Player
{
    int id;
    Vec shipyard;
    int halite;

    vector<Ship> ships;
    vector<Dropoff> dropoffs;

    // update auxiliary structures after basic info is initialized at the beginning of the turn
    void post_update();

    vector<Vec> structures;
    map<int, Ship> id_to_ship;

    bool has_structure_at(Vec pos) const;

    string toString() const;
    string toLongString() const;
};
