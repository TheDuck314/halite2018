#pragma once
#include <vector>
#include "Command.h"
using namespace std;
class Network final
{
  public:
    static void init();
    static void begin_turn();
    static void end_turn(const vector<Command> &commands);
};