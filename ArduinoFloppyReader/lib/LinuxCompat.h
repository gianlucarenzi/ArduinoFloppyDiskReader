#ifndef __LINUX_COMPAT_H__
#define __LINUX_COMPAT_H__

#include <unistd.h>
#include <string.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <sys/time.h>
#include <fcntl.h>
#include <termios.h>

// MSWindows types definition
typedef int HANDLE;
#define DWORD uint32_t
#define WORD  uint16_t
#define sprintf_s sprintf

// MSWindows file handle default value
#define INVALID_HANDLE_VALUE (-1)

#define Sleep(a)		usleep(1000L * a)
#define CloseHandle(a)	close(a)

// CTS status flag
#define MS_CTS_ON 		TIOCM_CTS

// Dummy. Actually not implemented/used
#define GENERIC_READ       0
#define GENERIC_WRITE      1
#define FILE_SHARE_READ    2
#define OPEN_EXISTING      3
#define CREATE_ALWAYS      4

extern bool GetCommModemStatus(HANDLE m_handle, DWORD *mask);
extern HANDLE CreateFile(const wchar_t *file, int o_flag, int s_type, void *p, int o_xflag, int dummy1, int dummy2);
extern HANDLE CreateFileA(const wchar_t *dev, int o_flag, int s_type, void *p, int o_xflag, int dummy1, int dummy2);
extern bool ReadFile(HANDLE m_handle, void *buf, DWORD size, uint32_t *dataRead, void *ptr);
extern bool WriteFile(HANDLE m_handle, const void *buf, DWORD size, uint32_t *dataWritten, void *ptr);
extern void _wcsupr(char *ptr);
extern int _wtoi(char *ptr);

extern int timeouts;

#endif /* __LINUX_COMPAT_H__ */
