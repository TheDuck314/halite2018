#include "Log.h"

#include <unistd.h>
#include <cstring>

#include "StringUtil.h"
#include "Game.h"
#include "Parameter.h"

ofstream Log::f_main;
ofstream Log::f_flog;
bool Log::inited = false;
bool Log::first_flog = true;

static bool ENABLED = true;  // flip me to false to disable logging

PARAM(bool, NOLOG);

void Log::init(int player_id)
{
    // dumb thing: don't log on ec2 or we'll run out of disk:
    // also don't log in live I guess
    char hostname[1024];
    gethostname(hostname, 1024);
    if (strcmp(hostname, "home") != 0) ENABLED = false;

    if (NOLOG.get(false)) ENABLED = false;

    if (!ENABLED) return;

    string fn = stringf("./log-%d.txt", player_id);
    f_main.open(fn.c_str());

    fn = stringf("./fluorine-flog-%d.txt", player_id);
    f_flog.open(fn.c_str());
    f_flog << "[" << endl;

    inited = true;    
}

void Log::log(const string& msg)
{
    if (!inited) return;
    f_main << msg << endl;
    f_main.flush();
}

void Log::flog(Vec pos, const string& msg)
{
    if (!inited) return;
    if (!first_flog) f_flog << ",";
    f_flog << stringf("{\"t\":%d, \"x\":%d, \"y\":%d, \"msg\":\"%s<br/>\"}",
        Game::turn, pos.x, pos.y, msg.c_str());
    f_flog << endl;
    f_flog.flush();
    first_flog = false;

    log(stringf("flog: %s: %s", +pos.toString(), +msg));
}

void Log::flog_color(Vec pos, int r, int g, int b, const string& msg)
{
    if (!inited) return;
    if (!first_flog) f_flog << ",";
    f_flog << stringf("{\"t\":%d, \"x\":%d, \"y\":%d, \"color\":\"rgb(%d,%d,%d)\", \"msg\":\"%s<br/>\"}",
        Game::turn, pos.x, pos.y, r, g, b, msg.c_str());
    f_flog << endl;
    f_flog.flush();
    first_flog = false;

    log(stringf("flog: %s: rgb(%d,%d,%d): %s", +pos.toString(), r, g, b, +msg));    
}

