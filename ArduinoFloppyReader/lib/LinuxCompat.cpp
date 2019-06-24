#include "LinuxCompat.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>
#include "ArduinoInterface.h"

#define COLORS
//#undef  COLORS

#ifdef COLORS
	#define ANSI_RED        "\033[0;31m"
	#define ANSI_GREEN      "\033[0;32m"
	#define ANSI_YELLOW     "\033[0;33m"
	#define ANSI_BLUE       "\033[0;34m"
	#define ANSI_MAGENTA    "\033[0;35m"
	#define ANSI_CYAN       "\033[0;36m"
	#define ANSI_GREY       "\033[1;36m"
	#define ANSI_RESET      "\033[0m"
#else
	#define ANSI_RED        ""
	#define ANSI_GREEN      ""
	#define ANSI_YELLOW     ""
	#define ANSI_BLUE       ""
	#define ANSI_MAGENTA    ""
	#define ANSI_CYAN       ""
	#define ANSI_GREY       ""
	#define ANSI_RESET      ""
#endif

enum {
	DBG_ERROR = 0,
	DBG_INFO,
	DBG_VERBOSE,
	DBG_NOISY,
};

static int debuglevel = DBG_ERROR;

#define DBG_E(fmt, args...) \
	fprintf(stderr, " " ANSI_RED "ERROR  : %s:%d:%s(): " ANSI_RESET fmt, __FILE__, \
		__LINE__, __func__, ##args);

#define DBG_I(fmt, args...) \
	if (debuglevel >= DBG_INFO) \
		fprintf(stderr, ANSI_GREEN "INFO   : %s:%d:%s(): " ANSI_RESET fmt, __FILE__, \
			__LINE__, __func__, ##args)

#define DBG_V(fmt, args...) \
	if (debuglevel >= DBG_VERBOSE) \
		fprintf(stderr, ANSI_BLUE "VERBOSE: %s:%d:%s(): " ANSI_RESET fmt, __FILE__, \
			__LINE__, __func__, ##args)

#define DBG_N(fmt, args...) \
	if (debuglevel >= DBG_NOISY) \
		fprintf(stderr, ANSI_YELLOW "NOISY  : %s:%d:%s(): " ANSI_RESET fmt, __FILE__, \
			__LINE__, __func__, ##args)

static int timeouts = 0; // Global definition. Will be used in ArduinoInterface.cpp

void SetTimeout(int timer)
{
	DBG_N("arg: %d\n", timer);
	timeouts = timer;
}

bool GetCommModemStatus(HANDLE m_handle, DWORD *mask)
{
	int status;
	DBG_N("args: %d - ptr: %p\n", m_handle, mask);
	int r = ioctl(m_handle, TIOCMGET, &status);
	if (r >= 0)
		*mask = (DWORD) status;
	else
	{
		DBG_E("ioctl %d\n", r);
		perror("ioctl");
		*mask = 0;
	}
	return r >= 0;
}

HANDLE CreateFileA(const wchar_t *dev, int o_flag, int s_type, void *p, int o_xflag, int dummy1, int dummy2)
{
	int fd;

	DBG_N("Port Device: %s\n", (const char *)dev);
	// Serial Port are open in blocking mode
	if((fd = open((const char *)dev, O_RDWR | O_NOCTTY)) == -1)
	{
		DBG_E("failed to open device: %s: %s\n", (const char *) dev, strerror(errno));
		return -1;
	}
	DBG_N("Exit with: %d\n", fd);
	return fd;
}

HANDLE CreateFile(const wchar_t *file, int o_flag, int s_type, void *p, int o_xflag, int dummy1, int dummy2)
{
	int fd;
	mode_t mode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;
	DBG_N("File: %s\n", (const char *) file);

	if((fd = open((const char *)file, O_RDWR | O_CREAT, mode)) == -1)
	{
		DBG_E("failed to open file: %s: %s\n", (const char *) file, strerror(errno));
		return -1;
	}
	DBG_N("Exit with: %d\n", fd);
	return fd;
}

static void print_hex_ascii_line(const unsigned char *payload, int len, int offset)
{
	int i;
	int gap;
	const unsigned char *ch;

	// offset
	fprintf(stdout, "%05d   ", offset);

	// hex
	ch = payload;
	for (i = 0; i < len; i++)
	{
		fprintf(stdout, "%02x ", *ch);
		ch++;
		// print extra space after 8th byte for visual aid
		if (i == 7)
			fprintf(stdout, " ");
	}
	// print space to handle line less than 8 bytes
	if (len < 8)
	{
		fprintf(stdout, " ");
	}

	// fill hex gap with spaces if not full line
	if (len < 16)
	{
		gap = 16 - len;
		for (i = 0; i < gap; i++)
		{
			fprintf(stdout, "   ");
		}
	}
	fprintf(stdout, "   ");

	// ascii (if printable)
	ch = payload;
	for(i = 0; i < len; i++)
	{
		if (isprint(*ch))
		{
			fprintf(stdout, "%c", *ch);
		}
		else
		{
			fprintf(stdout, ".");
		}
		ch++;
	}
	fprintf(stdout, "\n");
}

static void dump_buffer(const unsigned char *ptr, int len, const char * ansicolor)
{
	int len_rem = len;
	int line_width = 16;           // number of bytes per line
	int line_len;
	int offset = 0;                // zero-based offset counter
	const unsigned char *ch = ptr;

	DBG_N("Buffer @ %p - Len: %d\n", ptr, len);

	if (len <= 0)
	{
		return;
	}

	if (ansicolor != NULL)
		fprintf(stdout, "%s", ansicolor);

	// data fits on one line
	if (len <= line_width)
	{
		print_hex_ascii_line(ch, len, offset);
		fprintf(stdout, "" ANSI_RESET);
		return;
	}

	// data spans multiple lines
	for (;;)
	{
		// compute current line length
		line_len = line_width % len_rem;
		// print line
		print_hex_ascii_line(ch, line_len, offset);
		// compute total remaining
		len_rem = len_rem - line_len;
		// shift pointer to remaining bytes to print
		ch = ch + line_len;
		// add offset
		offset = offset + line_width;
		// check if we have line width chars or less
		if (len_rem <= line_width)
		{
			// print last line and get out
			print_hex_ascii_line(ch, len_rem, offset);
			break;
		}
	}
	fprintf(stdout, ANSI_RESET);
}

int SetPortBlock(int fd)
{
	return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) & ~O_NONBLOCK);
}

int SetPortNonBlock(int fd)
{
	return fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
}


bool ReadFile(HANDLE m_handle, void *buf, DWORD size, uint32_t *dataRead, void *ptr)
{
	static struct timeval tv_zero;
	fd_set rd;
	DWORD r;
	unsigned char * buffer = (unsigned char *)buf;
	DWORD bytesRead = 0;
	int timer = 0;
	int rval = 0;
	int err;
	DWORD toread = size;

	// Avoid gcc warning
	tv_zero = tv_zero;
	rd = rd;
	rval = rval;

	DBG_N("Bytes to read: %d (timeouts %dmSec) ONE-AT-TIME\n", size, timeouts);
	do
	{
		r = read(m_handle, buffer, toread);
		err = errno;
		if (r <= 0)
		{
			// Something weird happens in between...
			if (r == 0 || err == EINTR || err == EAGAIN)
			{
				if (r == 0)
				{
					timer++;
					DBG_N("[%04d] Nothing to read\n", timer);
					if (timer > timeouts)
					{
						break;
					}
				}
				usleep(1000); // 1ms timer tick.
				// Do not change this otherwise the timeout setting will not work!
			}
			else
			{
				// This is an error, for sure!
				DBG_E("READING %d - %s\n", r, strerror(err));
				bytesRead = 0;
				break;
			}
		}
		else
		{
			// Ok. 1 byte read, now the next
			buffer += toread;
			bytesRead += toread;
			timer = 0;
		}
	} while (bytesRead < size);

	*dataRead = (uint32_t) bytesRead;

	DBG_V("To read: %d -- read: %d in %d good timer ticks\n",
		size, bytesRead, timer);

	if (debuglevel >= DBG_NOISY)
		dump_buffer((const unsigned char *) buf, bytesRead, ANSI_GREY);

	DBG_N("Exit with %s\n", bytesRead == size ? "TRUE" : "FALSE");
	return bytesRead == size;
}

bool WriteFile(HANDLE m_handle, const void *buf, DWORD size, uint32_t *dataWritten, void *ptr)
{
	DWORD r;
	DWORD bytesWritten = 0;
	unsigned char * buff = (unsigned char *)buf;
	int err;
	int timer = 0;
	DWORD towrite = size;

	DBG_I("To Write: %d (timeouts %d)\n", size, timeouts);

	do
	{
		DBG_N("WRITE : %d\n", towrite);
		r = write(m_handle, buff, towrite);
		err = errno;
		if (r <= 0)
		{
			if (r == 0 || err == EAGAIN || err == EINTR)
			{
				DBG_N("[%04d] No Write. Wait...\n", timer);
				if (timer > timeouts)
				{
					break;
				}
				usleep(1000); // 1ms timer tick
			}
			else
			{
				// This is an error, for sure!
				DBG_E("WRITING %d - %s\n", r, strerror(err));
				perror("writing");
				bytesWritten = 0;
				break;
			}
		}
		else
		{
			// Ok write next
			buff += towrite;
			bytesWritten += towrite;
			if (bytesWritten < size)
			{
				DBG_N("Partially Written: %d of %d\n", bytesWritten, size);
				towrite = size - bytesWritten;
			}
			timer = 0;
		}
	} while (bytesWritten < size);

	*dataWritten = bytesWritten;

	DBG_V("To write: %d - written: %d in %d good timer ticks = returns %s\n",
		size, bytesWritten, timer, size == bytesWritten ? "TRUE" : "FALSE");

	if (debuglevel >= DBG_NOISY)
		dump_buffer((const unsigned char *) buf, bytesWritten, ANSI_MAGENTA);

	DBG_N("Exit with: %s\n", bytesWritten == size ? "TRUE" : "FALSE");
	return bytesWritten == size;
}

void _wcsupr(char *ptr)
{
	char c;
	while (*ptr != '\0')
	{
		char t;
		t = *(ptr);
		c = toupper(t);
		*(ptr) = c;
		ptr++;
	}
}

int _wtoi(char *ptr)
{
	return strtoul(ptr, NULL, 10);
}

