#include "Command.h"

#include "Log.h"

Command Command::generate()
{
    Command ret;
    ret.type = Type::Generate;
    return ret;
}

Command Command::move(int _ship_id, Direction _move_dir)
{
    Command ret;
    ret.type = Type::Move;
    ret.ship_id = _ship_id;
    ret.move_dir = _move_dir;
    return ret;
}

Command Command::move(const Ship &ship, Direction _move_dir)
{
    return move(ship.id, _move_dir);
}

Command Command::construct(int _ship_id)
{
    Command ret;
    ret.type = Type::Construct;
    ret.ship_id = _ship_id;
    return ret;
}

static char encode_dir(Direction d)
{
    switch (d) {
        case Direction::North: return 'n';
        case Direction::South: return 's';
        case Direction::East:  return 'e';
        case Direction::West:  return 'w';
        case Direction::Still: return 'o';
        default:
            Log::die("encode_dir unrecognized Direction %d", (int)d);
            return 'x';
    }
}

string Command::encode() const
{
    if (type == Type::Generate) {
        return "g";
    } else if (type == Type::Move) {
        return stringf("m %d %c", ship_id, encode_dir(move_dir));
    } else if (type == Type::Construct) {
        return stringf("c %d", ship_id);
    } else {
        Log::die("Command::encode() unknown type %d", (int)type);
        return "";
    }
}