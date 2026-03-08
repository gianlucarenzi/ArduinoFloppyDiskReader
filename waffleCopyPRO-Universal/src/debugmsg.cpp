#include "debugmsg.h"

#ifndef _WIN32
#include <QTextStream>
#include <QString>
#include <stdio.h>

bool DebugMsg::s_enabled = false;
bool DebugMsg::s_verboseEnabled = false;

void DebugMsg::init(bool enabled)
{
    s_enabled = enabled;
}

void DebugMsg::initVerbose(bool enabled)
{
    s_verboseEnabled = enabled;
}

bool DebugMsg::enabled()
{
    return s_enabled;
}

bool DebugMsg::verboseEnabled()
{
    return s_verboseEnabled;
}

void DebugMsg::print(const char* func, const QString& msg)
{
    if (s_enabled)
    {
        QTextStream(stdout) << func << ": " << msg << endl;
    }
}
#endif // _WIN32
