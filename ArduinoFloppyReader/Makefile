# Makefile
PREFIX = /usr/local
CROSS_COMPILE ?=
NONINTERACTIVE ?=

# Platform specific Makefile tweaks based on uname detection

uname_S := $(shell sh -c 'uname -s 2>/dev/null || echo not')
uname_M := $(shell sh -c 'uname -m 2>/dev/null || echo not')
uname_O := $(shell sh -c 'uname -o 2>/dev/null || echo not')
uname_R := $(shell sh -c 'uname -r 2>/dev/null || echo not')
uname_P := $(shell sh -c 'uname -p 2>/dev/null || echo not')
uname_V := $(shell sh -c 'uname -v 2>/dev/null || echo not')

ifdef MSVC
	# avoid the MingW and Cygwin configuration sections
	uname_S := Windows
	uname_O := Windows
endif

ifeq ($(uname_S),Linux)
	LINUXDEF = -D__LINUX__
	WINDOWSDEF =
	DARWINDEF =
endif

ifeq ($(uname_S),Darwin)
	LINUXDEF =
	WINDOWSDEF =
	DARWINDEF = -D__MACOS__
endif

ifeq ($(uname_S),Windows)
	LINUXDEF =
	WINDOWSDEF = -D__MINGW32__
	DARWINDEF =
endif

# Environment variables
CC = $(CROSS_COMPILE)gcc
WINDRES = $(CROSS_COMPILE)windres
AR = $(CROSS_COMPILE)ar
STRIP = $(CROSS_COMPILE)strip
CXX = $(CROSS_COMPILE)g++
CPP = $(CROSS_COMPILE)cpp

src = \
	ArduinoFloppyReader/Main.cpp \
	lib/ArduinoInterface.cpp \
	lib/ADFWriter.cpp \
	lib/LinuxCompat.cpp \


obj = $(src:.cpp=.o)
dep = $(obj:.o=.d)
bin = avrfloppyreader

CXXFLAGS = -Wall -g -pg -O3 -Ilib -IArduinoFloppyReader $(LINUXDEF) $(WINDOWSDEF) $(DARWINDEF)

CXXFLAGS += \
	$(NONINTERACTIVE) \


$(bin): $(obj)
	$(CXX) -o $@ $(obj) $(LDFLAGS)

-include $(dep)

%.d: %.cpp
	@$(CPP) $(CXXFLAGS) $< -MM -MT $(@:.d=.o) >$@

.PHONY: clean
clean:
	rm -f $(dep) $(obj) $(bin)

.PHONY: cleandep
cleandep:
	rm -f $(dep)
