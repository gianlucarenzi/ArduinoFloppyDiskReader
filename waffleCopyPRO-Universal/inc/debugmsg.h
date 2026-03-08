#ifndef DEBUGMSG_H
#define DEBUGMSG_H

#include <QString>
#include <QTextStream>
#include <stdio.h>

#if defined(__GNUC__) || defined(__clang__)
#define NO_INSTRUMENT __attribute__((no_instrument_function))
#else
#define NO_INSTRUMENT
#endif

class DebugMsg
{
public:
#ifdef _WIN32
    // Inline implementations on Windows to avoid requiring a separate compilation unit
    static void init(bool enabled) NO_INSTRUMENT { s_enabled = enabled; }
    static void initVerbose(bool enabled) NO_INSTRUMENT { s_verboseEnabled = enabled; }
    static void print(const char* func, const QString& msg) NO_INSTRUMENT { if (s_enabled) { QTextStream(stdout) << func << ": " << msg << endl; } }
    static bool enabled() NO_INSTRUMENT { return s_enabled; }
    static bool verboseEnabled() NO_INSTRUMENT { return s_verboseEnabled; }

private:
    static inline bool s_enabled = false;
    static inline bool s_verboseEnabled = false;
#else
    // On non-Windows platforms, provide declarations only; definitions come from debugmsg.cpp
    static void init(bool enabled) NO_INSTRUMENT;
    static void initVerbose(bool enabled) NO_INSTRUMENT;
    static void print(const char* func, const QString& msg) NO_INSTRUMENT;
    static bool enabled() NO_INSTRUMENT;
    static bool verboseEnabled() NO_INSTRUMENT;

private:
    static bool s_enabled;
    static bool s_verboseEnabled;
#endif
};

#ifndef DEBUGMSG
#define DEBUGMSG(fmt, ...) \
    do { if (DebugMsg::verboseEnabled()) { printf("%s: " fmt "\n", __func__, ##__VA_ARGS__); } } while(0)
#endif // DEBUGMSG

#endif // DEBUGMSG_H
