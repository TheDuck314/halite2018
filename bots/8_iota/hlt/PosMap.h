#pragma once

#include <string>
#include "Geometry.h"
#include "Grid.h"

template<class T>
class PosMap final
{
  public:
    PosMap(T filler)
    {
        arr = new T[grid.width * grid.height];
        fill(filler);
    }
    PosMap(const PosMap &rhs)
    {
        arr = new T[grid.width * grid.height];
        *this = rhs;
    }
    PosMap& operator=(const PosMap &rhs)
    {
        std::copy(rhs.arr, rhs.arr + grid.width * grid.height, arr);
        return *this;
    }
    ~PosMap() { delete [] arr; }

    T& operator()(Vec pos) { return arr[grid.width * pos.y + pos.x]; }
    const T& operator()(Vec pos) const { return const_cast<PosMap<T>*>(this)->operator()(pos); }

    void fill(T filler) { std::fill(arr, arr + grid.width * grid.height, filler); }

  private:
      T *arr;
};

using PosSet = PosMap<bool>;


string toString(const PosSet &ps);