#ifndef DEBUGMSG_H
#define DEBUGMSG_H

/* Standalone version - no Qt dependency, for waffle-fuse FUSE driver */

#include <stdio.h>
#include <string>

#if defined(__GNUC__) || defined(__clang__)
#define NO_INSTRUMENT __attribute__((no_instrument_function))
#else
#define NO_INSTRUMENT
#endif

class DebugMsg {
public:
    static void init(bool enabled) NO_INSTRUMENT;
    static void initVerbose(bool enabled) NO_INSTRUMENT;
    static void print(const char* func, const std::string& msg) NO_INSTRUMENT;
    static bool enabled() NO_INSTRUMENT;
    static bool verboseEnabled() NO_INSTRUMENT;

private:
    static bool s_enabled;
    static bool s_verboseEnabled;
};

#ifndef DEBUGMSG
#define DEBUGMSG(fmt, ...) \
    do { if (DebugMsg::verboseEnabled()) { printf("%s: " fmt "\n", __func__, ##__VA_ARGS__); } } while(0)
#endif

#endif // DEBUGMSG_H
