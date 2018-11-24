#pragma once

#include <vector>
using namespace std;

#include "Geometry.h"

class PathHasher
{
  public:
    PathHasher();

    int hash(const vector<Vec> &path);

  private:
    static constexpr size_t MAX_LEN = 10;
    vector<int> turn_square_bits;
};