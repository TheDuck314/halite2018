#include "StringUtil.h"

#include <cstdarg>
#include <vector>

// https://stackoverflow.com/questions/69738/c-how-to-get-fprintf-results-as-a-stdstring-w-o-sprintf#69911
static string vstringf(const char *fmt, va_list ap)
{
    // Allocate a buffer on the stack that's big enough for us almost
    // all the time.
    char buf[1024];
    int size = sizeof(buf) / sizeof(buf[0]);

    // Try to vsnprintf into our buffer.
    va_list apcopy;
    va_copy(apcopy, ap);
    int needed = vsnprintf(&buf[0], size, fmt, ap);
    // NB. On Windows, vsnprintf returns -1 if the string didn't fit the
    // buffer.  On Linux & OSX, it returns the length it would have needed.

    if (needed <= size && needed >= 0) {
        // It fit fine the first time, we're done.
        return string(&buf[0]);
    } else {
        // vsnprintf reported that it wanted to write more characters
        // than we allotted.  So do a malloc of the right size and try again.
        // This doesn't happen very often if we chose our initial size
        // well.
        vector<char> buf;
        size = needed;
        buf.resize(size);
        needed = vsnprintf(&buf[0], size, fmt, apcopy);
        return string(&buf[0]);
    }
}

string stringf(const char *fmt, ...)
{
    va_list ap;
    va_start (ap, fmt);
    string buf = vstringf(fmt, ap);
    va_end(ap);
    return buf;
}

const char* operator+(const string &s) { return s.c_str(); }