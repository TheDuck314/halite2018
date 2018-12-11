#pragma once

#include <string>
using namespace std;

/*
template<typename Arg1, typename... argv>
string stringf(const char* format, Arg1 arg1, argv... args) {
    const size_t size = std::snprintf(NULL, 0, format, arg1, args...);

    string output;
    output.resize(size+1);
    snprintf(&(output[0]), size+1, format, arg1, args...);
    return move(output);
}
*/


string stringf(const char *fmt, ...);

const char* operator+(const string &s);