#include "Player.h"
#include "StringUtil.h"
#include "Util.h"

string Dropoff::toString() const
{
    return stringf("Dropoff(id=%d pos=%s)", id, pos.toString().c_str());
}

void Player::post_update()
{
    // structures
    structures.clear();
    structures.push_back(shipyard);
    for (Dropoff dropoff : dropoffs) structures.push_back(dropoff.pos);

    // id_to_ship
    id_to_ship.clear();
    for (Ship ship : ships) id_to_ship[ship.id] = ship;

    // structures last forever, so we never have to set anything to false:
    for (Vec structure : structures) has_structure_at(structure) = true;

    // in the unlikely event that we really needed to optimize this, we could
    // only recompute it when the # of structures changes
    for (Vec v : grid.positions) {
        dist_from_structure(v) = grid.smallest_dist(v, structures);
    }
}

string Player::toString() const
{
    return stringf("Player(id=%d shipyard=%s halite=%d ships=%d dropoffs=%d)", 
                   id, shipyard.toString().c_str(), halite, (int)ships.size(), (int)dropoffs.size());
}

string Player::toLongString() const
{
    string ret = toString();
    ret = ret + string("\nShips:");
    for (const Ship &ship : ships) {
        ret += string("\n - ") + ship.toString();
    }
    ret += string("\nDropoffs:");
    for (const Dropoff &dropoff : dropoffs) {
        ret += string("\n - ") + dropoff.toString();
    }
    return ret;
}
