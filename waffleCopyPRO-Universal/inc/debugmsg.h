#ifndef DEBUGMSG_H
#define DEBUGMSG_H

#include <QString>

class DebugMsg
{
public:
    static void init(bool enabled);
    static void print(const char* func, const QString& msg);

private:
    static bool s_enabled;
};

#endif // DEBUGMSG_H
