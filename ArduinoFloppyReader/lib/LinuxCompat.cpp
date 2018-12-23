#include "LinuxCompat.h"

#ifndef __MINGW32__
#include <errno.h>
#include <fcntl.h>

int timeouts = 0; // Global definition. Will be used in ArduinoInterface.cpp

bool GetCommModemStatus(HANDLE m_handle, DWORD *mask)
{
	int status;
	int r = ioctl(m_handle, TIOCMGET, &status);
	if (r >= 0)
		*mask = (DWORD) status;
	else
		*mask = 0;
	return r >= 0;
}

HANDLE CreateFileA(const wchar_t *dev, int o_flag, int s_type, void *p, int o_xflag, int dummy1, int dummy2)
{
	int fd;

	if((fd = open((const char *)dev, O_NONBLOCK | O_RDWR | O_NOCTTY)) == -1)
	{
		fprintf(stderr, "CreateFile: failed to open device: %s: %s\n", (const char *) dev, strerror(errno));
		return -1;
	}
	return fd;
}

HANDLE CreateFile(const wchar_t *file, int o_flag, int s_type, void *p, int o_xflag, int dummy1, int dummy2)
{
	int fd;
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

	if((fd = open((const char *)file, O_RDWR | O_CREAT, mode)) == -1)
	{
		fprintf(stderr, "CreateFile: failed to open file: %s: %s\n", (const char *) file, strerror(errno));
		return -1;
	}
	return fd;
}

bool ReadFile(HANDLE m_handle, void *buf, DWORD size, uint32_t *dataRead, void *ptr)
{
	static struct timeval tv_zero;
	fd_set rd;
	DWORD r; 

	FD_ZERO(&rd);
	FD_SET(m_handle, &rd);

	tv_zero.tv_sec = timeouts / 1000;
	tv_zero.tv_usec = timeouts * 1000;

	while(select(m_handle + 1, &rd, 0, 0, timeouts > 0 ? &tv_zero : 0) == -1 && (errno == EINTR || errno == EAGAIN))
		;

	r = read(m_handle, buf, size);
	if (r > 0)
		*dataRead = (uint32_t) r;
	else
		*dataRead = 0; 

	return r == size;
}

bool WriteFile(HANDLE m_handle, const void *buf, DWORD size, uint32_t *dataWritten, void *ptr)
{
	DWORD r;
	r = write(m_handle, buf, size);
	if (r > 0)
		*dataWritten = r;
	else
		*dataWritten = 0;
	return r == size;
}
#endif
