#pragma once

#include <string>
#include <sstream>
#include "Log.h"
using namespace std;

// a numerical parameter that can be overriden on the command line
template<typename T>
class Parameter final 
{
  public:
    Parameter(const char* _name) : name(_name) {}

    void parse(int argc, char **argv) 
    {
        const string prefix(name + "=");
        for (int i = 1; i < argc; ++i) {
            const string arg(argv[i]);
            if (arg.find(prefix) == 0) {
                stringstream ss(arg.substr(prefix.length()));
                ss >> override_value;
                if (ss.fail()) Log::die("Failed to parse parameter argument %s", +arg);
                overridden = true;
                Log::log("Parameter %s override = %f", +name, (double)override_value);
            }
        }
    }

    T get(T default_value)
    {
        if (overridden) return override_value;
        else return default_value;
    }

  private:
    bool overridden = false;
    T override_value;
    string name;
};


