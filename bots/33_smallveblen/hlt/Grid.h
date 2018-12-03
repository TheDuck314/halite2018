#pragma once
#include <vector>
#include "Geometry.h"
#include "Ship.h"
using namespace std;

struct Cell
{
    int halite;
};

class Grid final
{
  public:
    void resize(int _width, int _height);

    bool inited = false;

    int width;
    int height;
    vector<Vec> positions;

    Cell& operator()(int x, int y);
    Cell& operator()(Vec pos) { return (*this)(pos.x, pos.y); }
    const Cell& operator()(Vec pos) const { return const_cast<Grid*>(this)->operator()(pos); }

    Vec pos_delta(Vec src, Vec dest) const;

    int dist(Vec a, Vec b) const;

    Vec closest(Vec src, const vector<Vec> &dests) const;
    Ship closest(Vec src, const vector<Ship> &ships) const;
    int smallest_dist(Vec src, const vector<Vec> &dests) const;
    int smallest_dist(Vec src, const vector<Ship> &dests) const;
    int smallest_dist_except(Vec src, const vector<Ship> &ships, Ship exclude) const;

    int second_smallest_dist(Vec src, const vector<Ship> &ships) const;

    int num_within_dist(Vec src, const vector<Ship> &ships, int R) const;

    double average_dist(Vec src, const vector<Ship> &ships) const;

    Vec add(Vec pos, Direction dir) const;
    Vec add(Vec pos, int dx, int dy) const;

    // assuming src and dest are adjacent or equal, finds the Direction from src to dest
    Direction adj_dir(Vec src, Vec dest) const;

    // list of Directions that could naively be used to go from src toward dest
    vector<Direction> approach_dirs(Vec src, Vec dest) const;
    // don't try lateral moves when pos delta is aligned with a coordinate axis:
    vector<Direction> strict_approach_dirs(Vec src, Vec dest) const;

    int halite_in_square(Vec center, int R) const;
    int halite_in_manhattan_radius(Vec center, int R) const;
    
  private:
    vector<Cell> cells;
};

// let's have a singleton Grid
extern Grid grid;