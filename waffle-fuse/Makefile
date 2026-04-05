# waffle-nbd Makefile
# Builds waffle-nbd: an NBD server for DrawBridge/Waffle floppy reader.
#
# Dependencies:
#   Linux:   build-essential (no additional libraries required)
#   macOS:   Xcode command line tools

CXX := g++

# ── Platform detection ────────────────────────────────────────────────────────
UNAME := $(shell uname -s 2>/dev/null || echo Windows)

ifeq ($(UNAME),Darwin)
    CXX            := clang++
    _EXTRA_LDFLAGS := -lpthread
    TARGET_EXT     :=
    RM_CMD         := rm -f
    INSTALL_CMD    := install -m 755

else ifeq ($(UNAME),Windows)
    _EXTRA_LDFLAGS :=
    TARGET_EXT     := .exe
    RM_CMD         := del /Q
    INSTALL_CMD    :=

else
    _EXTRA_LDFLAGS := -lpthread -ldl
    TARGET_EXT     :=
    RM_CMD         := rm -f
    INSTALL_CMD    := install -m 755

endif

CXXFLAGS := -std=c++17 -O2 -Wall -Wextra \
            -D_FILE_OFFSET_BITS=64 \
            -DOUTPUT_TIME_IN_NS \
            -Isrc -Ilib -Iinc

LDFLAGS  := $(_EXTRA_LDFLAGS)

# ── Source files ──────────────────────────────────────────────────────────────
COMMON_LIB_SRCS := lib/ADFWriter.cpp \
                   lib/ArduinoInterface.cpp \
                   lib/SerialIO.cpp \
                   lib/RotationExtractor.cpp \
                   lib/pll.cpp \
                   lib/ibm_sectors.cpp \
                   lib/ftdi.cpp \
                   lib/debugmsg.cpp

COMMON_SRC_SRCS := src/disk_cache.cpp

NBD_SRCS  := src/nbd_server.cpp \
             src/waffle_nbd.cpp

COMMON_OBJS := $(COMMON_LIB_SRCS:.cpp=.o) $(COMMON_SRC_SRCS:.cpp=.o)
NBD_OBJS    := $(NBD_SRCS:.cpp=.o)

TARGET_NBD  := waffle-nbd$(TARGET_EXT)

# ── Build rules ───────────────────────────────────────────────────────────────
.PHONY: all clean install

all: $(TARGET_NBD)

$(TARGET_NBD): $(COMMON_OBJS) $(NBD_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# capsapi includes debugmsg from inc/
lib/capsapi/CapsPlug.o: lib/capsapi/CapsPlug.cpp
	$(CXX) $(CXXFLAGS) -Iinc -c $< -o $@

clean:
	$(RM_CMD) $(COMMON_OBJS) $(NBD_OBJS) $(TARGET_NBD)
	$(RM_CMD) $(COMMON_OBJS:.o=.d) $(NBD_OBJS:.o=.d)

install: $(TARGET_NBD)
ifeq ($(UNAME),Windows)
	@echo "Install not supported on Windows – copy target manually."
else
	$(INSTALL_CMD) $(TARGET_NBD) /usr/local/bin/$(TARGET_NBD)
endif

# ── Dependency generation ─────────────────────────────────────────────────────
-include $(COMMON_OBJS:.o=.d) $(NBD_OBJS:.o=.d)
