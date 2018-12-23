# Makefile
PREFIX = /usr/local
CROSS_COMPILE ?=

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

CXXFLAGS = -pedantic -Wall -g -Ilib -IArduinoFloppyReader

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
