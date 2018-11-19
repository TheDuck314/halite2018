#pragma once
#include <vector>

template<class T>
bool contains(const vector<T> &v, T t)
{
    for (auto it = v.begin(); it != v.end(); ++it) {
        if (*it == t) return true;
    }
    return false;
}