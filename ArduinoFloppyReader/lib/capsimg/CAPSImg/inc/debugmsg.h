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
    static void init(bool enabled) NO_INSTRUMENT { enabledFlag() = enabled; }
    static void print(const char* func, const std::string& msg) NO_INSTRUMENT { if (enabledFlag()) { printf("%s: %s\n", func, msg.c_str()); } }
    static bool enabled() NO_INSTRUMENT { return enabledFlag(); }

private:
    static bool& enabledFlag() NO_INSTRUMENT
    {
        static bool s_enabled = false;
        return s_enabled;
    }
};

#ifndef DEBUGMSG
#define DEBUGMSG(fmt, ...) \
    do { if (DebugMsg::enabled()) { printf("%s: " fmt "\n", __func__, ##__VA_ARGS__); } } while(0)
#endif

#endif // DEBUGMSG_H
