#include "debugmsg.h"

bool DebugMsg::s_enabled = false;
bool DebugMsg::s_verboseEnabled = false;

void DebugMsg::init(bool enabled)       { s_enabled = enabled; }
void DebugMsg::initVerbose(bool enabled){ s_verboseEnabled = enabled; }
bool DebugMsg::enabled()                { return s_enabled; }
bool DebugMsg::verboseEnabled()         { return s_verboseEnabled; }

void DebugMsg::print(const char* func, const std::string& msg)
{
    if (s_enabled)
        printf("%s: %s\n", func, msg.c_str());
}
