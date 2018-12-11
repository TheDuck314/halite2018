#include "Ship.h"
#include "StringUtil.h"

string Ship::toString() const
{
    return stringf("Ship(id=%d pos=%s halite=%d)", id, pos.toString().c_str(), halite);
}

