# pragma once

#include "Geometry.h"
#include "PosMap.h"

class Path final 
{
  public:
    static Direction simple(Vec src, Vec dest, const PosSet &blocked);
    static Direction find(Vec src, Vec dest, const PosSet &blocked, const PosSet &prefer_to_avoid);
    static Direction find_low_halite(Vec src, Vec dest, const PosSet &blocked);

    // For each location on the map, compute the minimum-halite route from src to that location
    // that is also a minimum-distance route. Compute the total halite along the route, including
    // src but excluding the final location. Divide by MOVE_COST_RATIO to get the halite burned
    // by moving along this route. Return a map of this route-halite-cost for every
    // possible destination square.
    static PosMap<int> build_halite_travel_cost_map(Vec src);
};