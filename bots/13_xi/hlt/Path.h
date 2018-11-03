# pragma once

#include "Geometry.h"
#include "PosMap.h"

class Path final 
{
  public:
    static Direction simple(Vec src, Vec dest, const PosSet &blocked);
    static Direction find(Vec src, Vec dest, const PosSet &blocked);
    static Direction find_low_halite(Vec src, Vec dest, const PosSet &blocked);
    static Direction find_strict(Vec src, Vec dest, const PosSet &blocked);
};