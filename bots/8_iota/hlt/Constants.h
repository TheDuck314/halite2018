#pragma once

#include <string>
using namespace std;

class Constants final {
  public:
    // The maximum amount of halite a ship can carry.
    static int MAX_HALITE;
    // The cost to build a single ship.
    static int SHIP_COST;
    // The cost to build a dropoff.
    static int DROPOFF_COST;
    // The maximum number of turns a game can last.
    static int MAX_TURNS;
    // 1/EXTRACT_RATIO halite (rounded) is collected from a square per turn.
    static int EXTRACT_RATIO;
    // INV_EXTRACT_RATIO = 1/EXTRACT_RATIO
    static double INV_EXTRACT_RATIO;
    // 1/MOVE_COST_RATIO halite (rounded) is needed to move off a cell.
    static int MOVE_COST_RATIO;
    // Whether inspiration is enabled.
    static bool INSPIRATION_ENABLED;
    // A ship is inspired if at least INSPIRATION_SHIP_COUNT opponent ships are within this Manhattan distance.
    static int INSPIRATION_RADIUS;  // 4
    // A ship is inspired if at least this many opponent ships are within INSPIRATION_RADIUS distance.
    static int INSPIRATION_SHIP_COUNT;  // 2
    // An inspired ship mines 1/X halite from a cell per turn instead.
    static int INSPIRED_EXTRACT_RATIO;  // unused?
    // An inspired ship that removes Y halite from a cell collects X*Y additional halite.
    static double INSPIRED_BONUS_MULTIPLIER;  // 2.0
    // An inspired ship instead spends 1/X% halite to move.
    static int INSPIRED_MOVE_COST_RATIO;


    static void parse(const string &line);
};