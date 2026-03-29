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
            src/fat_fs.cpp \
            src/affs_fs.cpp \
            src/waffle_fuse.cpp

ALL_SRCS := $(LIB_SRCS) $(SRC_SRCS)

OBJS     := $(ALL_SRCS:.cpp=.o)

TARGET   := waffle-fuse

# ── Build rules ───────────────────────────────────────────────────────────────
.PHONY: all clean install

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# capsapi includes debugmsg from inc/
lib/capsapi/CapsPlug.o: lib/capsapi/CapsPlug.cpp
	$(CXX) $(CXXFLAGS) -Iinc -c $< -o $@

clean:
	rm -f $(OBJS) $(TARGET)

install: $(TARGET)
	install -m 755 $(TARGET) /usr/local/bin/$(TARGET)

# ── Dependency generation ─────────────────────────────────────────────────────
-include $(OBJS:.o=.d)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -MMD -MP -c $< -o $@
