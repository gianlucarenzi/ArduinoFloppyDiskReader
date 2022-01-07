#ifndef WAFFLECONFIG_H
#define WAFFLECONFIG_H

#ifdef _WIN32
#define TRACKFILE  "C:\.waffle_track"
#define SIDEFILE   "C:\.waffle_side"
#define STATUSFILE "C:\.waffle_status"
#else
    // The /tmp/ folder needs to be readable/writable by user
#define TRACKFILE  "/tmp/.waffle_track"
#define SIDEFILE   "/tmp/.waffle_side"
#define STATUSFILE "/tmp/.waffle_status"
#endif

#endif // WAFFLECONFIG_H
