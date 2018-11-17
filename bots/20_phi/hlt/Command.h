#pragma once
#include <string>
#include "Geometry.h"
#include "Player.h"
using namespace std;

struct Command
{
    // three kinds of command
    // - spawn from shipyard: 'g'
    // - move ship: 'm <shipid> [nsewo]'
    // - turn ship into dropoff: 'c <shipid>'

    enum class Type
    {
        Generate  = 0,
        Move      = 1,
        Construct = 2
    };

    Type type;
    int ship_id;
    Direction move_dir;

    static Command generate();
    static Command move(int _ship_id, Direction _move_dir);
    static Command move(const Ship & ship, Direction _move_dir);
    static Command stay(const Ship & ship) { return move(ship, Direction::Still); }
    static Command construct(int _ship_id);

    string encode() const;
};