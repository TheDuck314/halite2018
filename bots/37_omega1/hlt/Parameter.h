#pragma once

#include <sstream>
#include <string>
#include <vector>
#include "Log.h"
using namespace std;

#define PARAM(type, name) Parameter<type> name(#name)

class ParameterBase
{
  public:
    static void parse_all(int argc, char **argv);
    
  protected:
    ParameterBase(const string &_name) : name(_name) { param_registry.push_back(this); }

    virtual void parse(int argc, char **argv) = 0;

    string name;

  private:
    static vector<ParameterBase*> param_registry;
};

// a numerical parameter that can be overriden on the command line
template<typename T>
class Parameter : public ParameterBase 
{
  public:
    Parameter(const char* _name) : ParameterBase(_name) {}

    bool given() { return overridden; }

    T get()
    {
        if (!overridden) Log::die("Param %s was not set!", +name);
        return override_value;
    }

    T get(T default_value)
    {
        if (overridden) return override_value;
        else return default_value;
    }

  protected:
    void parse(int argc, char **argv) override
    {
        //Log::log("parsing parameter %s", +name);
        const string prefix(name + "=");
        for (int i = 1; i < argc; ++i) {
            const string arg(argv[i]);
            if (arg.find(prefix) == 0) {
                string to_parse = arg.substr(prefix.length());
                stringstream ss(to_parse);
                ss >> override_value;
                if (ss.fail()) Log::die("Failed to parse parameter argument %s", +arg);
                overridden = true;
                //Log::log("Parameter %s override = '%s'", +name, +to_parse);
            }
        }
    }

  private:
    bool overridden = false;
    T override_value;
};

