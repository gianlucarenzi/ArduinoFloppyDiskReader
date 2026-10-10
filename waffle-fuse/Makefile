# waffle-fuse Makefile
# Builds a standalone FUSE driver for DrawBridge/Waffle floppy reader.
# No Qt dependency; requires libfuse3-dev.

CXX      := g++
CXXFLAGS := -std=c++17 -O2 -Wall -Wextra \
            -D_FILE_OFFSET_BITS=64 \
            -DOUTPUT_TIME_IN_NS \
            -Isrc -Ilib -Ilib/capsapi -Iinc \
            $(shell pkg-config --cflags fuse3)

LDFLAGS  := $(shell pkg-config --libs fuse3) -lpthread

PREFIX   ?= /usr/local

# ── Source files ──────────────────────────────────────────────────────────────
LIB_SRCS := lib/ADFWriter.cpp \
            lib/ArduinoInterface.cpp \
            lib/SerialIO.cpp \
            lib/RotationExtractor.cpp \
            lib/pll.cpp \
            lib/ibm_sectors.cpp \
            lib/ftdi.cpp \
            lib/debugmsg.cpp

SRC_SRCS := src/disk_cache.cpp \
            src/format.cpp \
            src/fat_fs.cpp \
            src/affs_fs.cpp \
            src/waffle_fuse.cpp

ALL_SRCS := $(LIB_SRCS) $(SRC_SRCS)

OBJS     := $(ALL_SRCS:.cpp=.o)

TARGET   := waffle-fuse

# ── Build rules ───────────────────────────────────────────────────────────────
.PHONY: all clean install test

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@

clean:
	rm -f $(OBJS) $(OBJS:.o=.d) $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) $(PREFIX)/bin/$(TARGET)

# Filesystem tests on disk images (no hardware needed)
test: $(TARGET)
	tests/run-tests.sh

-include $(OBJS:.o=.d)
