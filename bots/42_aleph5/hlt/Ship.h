#pragma once

#include <string>
#include "Geometry.h"
using namespace std;

struct Ship
{
    int id;
    Vec pos;
    int halite;

    string toString() const;
};

