# pragma once

#include "Geometry.h"
#include "PosMap.h"

class Path final 
{
  public:
    static Direction find(Vec src, Vec dest, const PosSet &blocked);
};