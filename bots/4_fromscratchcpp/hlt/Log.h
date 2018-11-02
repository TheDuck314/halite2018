#pragma once
#include <string>
#include <fstream>
#include <cstdarg>
#include "StringUtil.h"
#include "Player.h"
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
        log(msg);
        exit(-1);
    }

    template<typename Arg1, typename... argv>
    static void die(const char* format, Arg1 arg1, argv... args)
    {
        log(format, arg1, args...);
        exit(-1);
    }

    static void flog(Vec pos, const string& msg);
    static void flog(Ship ship, const string& msg) { flog(ship.pos, msg); }

    template<typename Arg1, typename... argv>
    static void flog(Vec pos, const char* format, Arg1 arg1, argv... args)
    {
        const string msg = stringf(format, arg1, args...);
        flog(pos, msg);
    }
    template<typename Arg1, typename... argv>
    static void flog(Ship ship, const char* format, Arg1 arg1, argv... args) {
        flog(ship.pos, format, arg1, args...);
    }

    /*
    static void log(const char *fmt, va_list ap)
    {
        const string msg = stringf(fmt, ap);
        log(msg);
    }

    static void die(const char *fmt, va_list ap)
    {
        log(fmt, ap);
        exit(-1);
    }
    */

  private:
    static ofstream f_main;
    static ofstream f_flog;
    static bool inited;
    static bool first_flog;
};
