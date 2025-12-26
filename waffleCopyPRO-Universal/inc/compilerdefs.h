#ifndef COMPILERDEFS_H
#define COMPILERDEFS_H

#if !defined(__PRETTY_FUNCTION__) && !defined(__GNUC__)
#define __PRETTY_FUNCTION__ __FUNCSIG__
#endif

#endif // COMPILERDEFS_H
