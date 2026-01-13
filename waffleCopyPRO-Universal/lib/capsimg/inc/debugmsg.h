#ifndef DEBUGMSG_H
#define DEBUGMSG_H

#include <cstdio>
#include <string>

#if defined(__GNUC__) || defined(__clang__)
#define NO_INSTRUMENT __attribute__((no_instrument_function))
#else
#define NO_INSTRUMENT
#endif

class DebugMsg {
public:
    static void init(bool enabled) NO_INSTRUMENT { s_enabled = enabled; }
    static void print(const char* func, const std::string& msg) NO_INSTRUMENT { if (s_enabled) { printf("%s: %s\n", func, msg.c_str()); } }
    static bool enabled() NO_INSTRUMENT { return s_enabled; }

private:
    static bool s_enabled;
};

inline bool DebugMsg::s_enabled = false;

#ifndef DEBUGMSG
#define DEBUGMSG(fmt, ...) \
    do { if (DebugMsg::enabled()) { printf("%s: " fmt "\n", __func__, ##__VA_ARGS__); } } while(0)
#endif

#endif // DEBUGMSG_H
