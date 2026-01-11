#include "debugmsg.h"
#include <QTextStream>
#include <QString>
#include <stdio.h>

bool DebugMsg::s_enabled = false;

void DebugMsg::init(bool enabled)
{
    s_enabled = enabled;
}

bool DebugMsg::enabled()
{
    return s_enabled;
}

void DebugMsg::print(const char* func, const QString& msg)
{
    if (s_enabled)
    {
        QTextStream(stdout) << func << ": " << msg << endl;
    }
}
