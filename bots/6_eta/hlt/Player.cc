#include "Player.h"
#include "StringUtil.h"
#include "Util.h"

string Ship::toString() const
{
    return stringf("Ship(id=%d pos=%s halite=%d)", id, pos.toString().c_str(), halite);
}

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
}

bool Player::has_structure_at(Vec pos) const
{
    return contains(structures, pos);
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
