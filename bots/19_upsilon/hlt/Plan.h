#pragma once
#include <string>
#include "Player.h"
#include "Geometry.h"
#include "StringUtil.h"
using namespace std;

enum class Purpose
{
    Stuck = 0,          // must stay put b/c too little halite
    Ram = 1,            // trying to hit an adjacent enemy
    EvadeReturner = 2,  // getting out of the way of someone with the Return purpose
    Flee = 3,           // moving away from an adjacent enemy
    Return = 4,         // bringing halite back to base
    Mine = 5,           // getting halite
};

string desc(Purpose p);

struct Plan 
{
    Ship ship;
    Vec tgt;
    Purpose purpose;

    string toString() const { return stringf("Plan(ship=%s tgt=%s purpose=%s", +ship.toString(), +tgt.toString(), +desc(purpose)); }
};

