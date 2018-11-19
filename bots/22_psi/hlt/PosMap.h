#pragma once

#include <string>
#include "Geometry.h"
#include "Grid.h"
#include "Log.h"

template<class T>
class PosMap final
{
  public:
    explicit PosMap(T filler)
    {
        if (!grid.inited) {
            fprintf(stderr, "trying to make PosMap before grid is inited!");
            exit(-1);
        }
        N = grid.width * grid.height;
        arr = new T[N];
        fill(filler);
    }
    PosMap(const PosMap &rhs)
    {
        N = rhs.N;
        arr = new T[N];
        *this = rhs;
    }
    PosMap& operator=(const PosMap &rhs)
    {
        if (this->N != rhs.N) Log::die("PosMap size mismatch");
        std::copy(rhs.arr, rhs.arr + N, arr);
        return *this;
    }
    ~PosMap() { delete [] arr; }

    T& operator()(Vec pos) { 
        const int index = grid.width * pos.y + pos.x;
        if (index < 0 || index > N) Log::die("PosMap out of bounds");
        return arr[index];
    }
    const T& operator()(Vec pos) const { return const_cast<PosMap<T>*>(this)->operator()(pos); }

    void fill(T filler) { std::fill(arr, arr + N, filler); }

  private:
      T *arr;  // not vector, because vector<bool> is crazy
      int N;
};

using PosSet = PosMap<bool>;


string toString(const PosSet &ps);