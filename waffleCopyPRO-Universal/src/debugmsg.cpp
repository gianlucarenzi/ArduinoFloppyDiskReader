#include "debugmsg.h"
#include <QTextStream>
#include <stdio.h>

bool DebugMsg::s_enabled = false;

void DebugMsg::init(bool enabled)
{
    s_enabled = enabled;
}

void DebugMsg::print(const char* func, const QString& msg)
{
    if (s_enabled)
    {
        QTextStream(stdout) << func << ": " << msg << Qt::endl;
    }
}
