#include "LinuxCompat.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <ctype.h>

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

static void dump_buffer(const unsigned char *ptr, int len)
{
	int len_rem = len;
	int line_width = 16;           // number of bytes per line
	int line_len;
	int offset = 0;                // zero-based offset counter
	const unsigned char *ch = ptr;

	if (len <= 0)
	{
		fprintf(stderr, "%s No LEN. Exit\n", __FUNCTION__);
		return;
	}

	// data fits on one line
	if (len <= line_width)
	{
		print_hex_ascii_line(ch, len, offset);
		fprintf(stdout, "Small Line. Exiting\n");
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
}

bool ReadFile(HANDLE m_handle, void *buf, DWORD size, uint32_t *dataRead, void *ptr)
{
	static struct timeval tv_zero;
	fd_set rd;
	DWORD r; 
	unsigned char * buffer = (unsigned char *)buf;
	DWORD bytesread = 0;
	DWORD len = size;
	int rval;
	int err;

	fprintf(stderr, "%s To read: %d\n", __FUNCTION__, len);

retry:
	FD_ZERO(&rd);
	FD_SET(m_handle, &rd);

	tv_zero.tv_sec = timeouts / 1000;
	tv_zero.tv_usec = timeouts * 1000;

	rval = select(m_handle + 1, &rd, NULL, NULL, timeouts > 0 ? &tv_zero : 0);

	err = errno;
	if (rval < 0)
	{
		if (errno == EINTR || errno == EAGAIN)
		{
			goto retry;
		}
		else
		{
			fprintf(stderr, "%s ERROR ON SELECT\n", __FUNCTION__);
			return false;
		}
	}
	else
	if (rval == 0)
	{
		// Timeout
		fprintf(stderr, "%s TIMEOUT\n", __FUNCTION__);
		return false;
	}
	else
	{
		int ctscounter = 0;
		for (;;)
		{
			r = ioctl(m_handle, FIONREAD, &bytesread);
			if (bytesread > 0)
			{
				ctscounter = 0;
				fprintf(stderr, "%s To read: %d\n", __FUNCTION__, len);
				r = read(m_handle, buffer, len);
				err = errno;
				if (r < 0)
				{
					if (err == EINTR || err == EAGAIN)
						continue;
					else
					{
						fprintf(stderr, "%s ERROR READING %d\n", __FUNCTION__, r);
						perror("reading serial");
						bytesread = 0;
						break;
					}
				}
				else
				if (r >= 0)
				{
					if (r < len)
					{
						len -= r;
						buffer += r;
					}
					else
					{
						bytesread = size;
						break;
					}
				}
			}
			else
			{
				fprintf(stderr, "%s [%d] Nothing to read\n", __FUNCTION__, ctscounter);
				usleep(1000);
				ctscounter++;
				if (ctscounter > 10)
				{
					ctscounter = 0;
				}
			}
		}
	}

	*dataRead = (uint32_t) bytesread;

//	fprintf(stderr, "%s read: %d\n", __FUNCTION__, bytesread);

	//dump_buffer((const unsigned char *) buf, bytesread);

	return bytesread == size;
}

bool WriteFile(HANDLE m_handle, const void *buf, DWORD size, uint32_t *dataWritten, void *ptr)
{
	DWORD r;
//	fprintf(stderr, "%s To write: %d\n", __FUNCTION__, size);
	r = write(m_handle, buf, size);
	if (r > 0)
		*dataWritten = r;
	else
		*dataWritten = 0;
//	fprintf(stderr, "%s Written: %d\n", __FUNCTION__, r);
	return r == size;
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

