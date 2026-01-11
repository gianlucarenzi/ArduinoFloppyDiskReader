#ifndef DEBUGMSG_H
#define DEBUGMSG_H

class QString;

#if defined(__GNUC__) || defined(__clang__)
#define NO_INSTRUMENT __attribute__((no_instrument_function))
#else
#define NO_INSTRUMENT
#endif

class DebugMsg
{
public:
    static void init(bool enabled) NO_INSTRUMENT;
    static void print(const char* func, const QString& msg) NO_INSTRUMENT;
    static bool enabled() NO_INSTRUMENT;

private:
    static bool s_enabled;
};

#ifndef DEBUGMSG
#include <stdio.h>
#define DEBUGMSG(fmt, ...) \
    do { if (DebugMsg::enabled()) { printf("%s: " fmt "\n", __func__, ##__VA_ARGS__); } } while(0)
#endif // DEBUGMSG

#endif // DEBUGMSG_H
