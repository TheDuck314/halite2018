#pragma once
#include <vector>
#include "Geometry.h"
using namespace std;

struct Cell
{
    int halite;
};

class Grid final
{
  public:
    void resize(int _width, int _height);

    int width;
    int height;
    vector<Vec> positions;

    Cell& operator()(int x, int y);
    Cell& operator()(Vec pos) { return (*this)(pos.x, pos.y); }

    Vec pos_delta(Vec src, Vec dest) const;

    int dist(Vec a, Vec b) const;

    Vec closest(Vec src, const vector<Vec> &dests) const;

    Vec add(Vec pos, Direction dir) const;
    Vec add(Vec pos, int dx, int dy) const;

    // assuming src and dest are adjacent or equal, finds the Direction from src to dest
    Direction adj_dir(Vec src, Vec dest) const;

    // list of Directions that could naively be used to go from src toward dest
    vector<Direction> approach_dirs(Vec src, Vec dest) const;
    
  private:
    vector<Cell> cells;
};

// let's have a singleton Grid
extern Grid grid;