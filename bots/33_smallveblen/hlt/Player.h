#pragma once

#include <map>
#include <string>
#include <vector>
#include "Geometry.h"
#include "PosMap.h"
#include "Ship.h"
using namespace std;

struct Dropoff
{
    int id;
    Vec pos;

    string toString() const;
};

struct Player
{
    // set by Network
    int id;
    Vec shipyard;
    int halite;

    vector<Ship> ships;
    vector<Dropoff> dropoffs;

    // computed by post_update()
    vector<Vec> structures;
    map<int, Ship> id_to_ship;

    PosMap<bool> has_structure_at;
    PosMap<int> dist_from_structure;

    Player()
      : has_structure_at(false),
        dist_from_structure(0)
    {}

    // update auxiliary structures after basic info is initialized at the beginning of the turn
    void post_update();

    // update our data mid-turn to account for building a dropoff this turn
    // adjusted_cost is the cost after taking out the savings from the halite
    // on the build square and the halite in the building ship
    void immediately_add_dropoff(int adjusted_cost, Vec pos);

    string toString() const;
    string toLongString() const;
};
