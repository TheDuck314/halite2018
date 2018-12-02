#pragma once
#include <string>
#include <fstream>
#include <cstdarg>
#include "StringUtil.h"
#include "Geometry.h"
using namespace std;

class Log 
{
  public:
    static void init(int player_id);

    static void log(const string& msg);

    template<typename Arg1, typename... argv>
    static void log(const char* format, Arg1 arg1, argv... args)
    {
        const string msg = stringf(format, arg1, args...);
        log(msg);
    }

    static void die(const string& msg)
    {
        fprintf(stderr, "%s", +msg);
        exit(-1);
    }

    template<typename Arg1, typename... argv>
    static void die(const char* format, Arg1 arg1, argv... args)
    {
        const string msg = stringf(format, arg1, args...);
        die(msg);
    }

    static void flog(Vec pos, const string& msg);

    template<typename Arg1, typename... argv>
    static void flog(Vec pos, const char* format, Arg1 arg1, argv... args)
    {
        const string msg = stringf(format, arg1, args...);
        flog(pos, msg);
    }

    static void flog_color(Vec pos, int r, int g, int b, const string& msg);
    template<typename Arg1, typename... argv>
    static void flog_color(Vec pos, int r, int g, int b, const char* format, Arg1 arg1, argv... args)
    {
        const string msg = stringf(format, arg1, args...);
        flog_color(pos, r, g, b, msg);
    }

  private:
    static ofstream f_main;
    static ofstream f_flog;
    static bool inited;
    static bool first_flog;
};
