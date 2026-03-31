# waffle-fuse Makefile
# Builds a standalone FUSE driver for DrawBridge/Waffle floppy reader.
# Supports Linux (libfuse3), macOS (macFUSE) and Windows (WinFsp).
#
# Dependencies:
#   Linux:   sudo apt install libfuse3-dev
#            sudo apt install libfuse-dev   # for USE_FUSE2=1
#   macOS:   brew install macfuse
#   Windows: https://github.com/winfsp/winfsp/releases
#
# Flags:
#   USE_FUSE2=1   Force FUSE 2 on Linux (useful to test macOS/Windows compat)
#                 Example: make USE_FUSE2=1

CXX := g++

# ── Platform detection ────────────────────────────────────────────────────────
UNAME := $(shell uname -s 2>/dev/null || echo Windows)

ifeq ($(UNAME),Darwin)
    # macFUSE – try pkg-config first, fall back to known Homebrew paths
    _FUSE_CFLAGS := $(shell pkg-config --cflags fuse 2>/dev/null)
    _FUSE_LIBS   := $(shell pkg-config --libs   fuse 2>/dev/null)
    ifeq ($(_FUSE_CFLAGS),)
        # Apple Silicon: /opt/homebrew  |  Intel: /usr/local
        BREW_PREFIX  := $(shell brew --prefix 2>/dev/null || echo /usr/local)
        _FUSE_CFLAGS := -I$(BREW_PREFIX)/include/osxfuse
        _FUSE_LIBS   := -L$(BREW_PREFIX)/lib -losxfuse
    endif
    CXX := clang++

else ifeq ($(UNAME),Windows)
    # WinFsp – prefer vcpkg if VCPKG_ROOT is set (env var or make variable).
    #
    # Install via vcpkg:
    #   vcpkg install          (reads vcpkg.json in this directory)
    # Then build:
    #   make VCPKG_ROOT=C:/vcpkg
    # or set the VCPKG_ROOT environment variable and just run: make
    #
    # Without vcpkg, set WINFSP_ROOT to the WinFsp MSI install directory:
    #   make WINFSP_ROOT="C:/Program Files (x86)/WinFsp"
    VCPKG_TRIPLET ?= x64-windows
    ifdef VCPKG_ROOT
        _VCPKG_INC   := $(VCPKG_ROOT)/installed/$(VCPKG_TRIPLET)/include
        _VCPKG_LIB   := $(VCPKG_ROOT)/installed/$(VCPKG_TRIPLET)/lib
        _FUSE_CFLAGS := -I"$(_VCPKG_INC)"
        _FUSE_LIBS   := -L"$(_VCPKG_LIB)" -lwinfsp-x64
    else
        # Fallback: direct WinFsp MSI installation
        WINFSP_ROOT  ?= C:/Program Files (x86)/WinFsp
        _FUSE_CFLAGS := -I"$(WINFSP_ROOT)/inc/fuse"
        _FUSE_LIBS   := -L"$(WINFSP_ROOT)/lib" -lwinfsp-x64
    endif

else
    # Linux: libfuse3-dev by default; USE_FUSE2=1 selects libfuse-dev (FUSE 2)
    ifeq ($(USE_FUSE2),1)
        _FUSE_CFLAGS := $(shell pkg-config --cflags fuse) -DUSE_FUSE2
        _FUSE_LIBS   := $(shell pkg-config --libs   fuse)
    else
        _FUSE_CFLAGS := $(shell pkg-config --cflags fuse3)
        _FUSE_LIBS   := $(shell pkg-config --libs   fuse3)
    endif

endif

CXXFLAGS := -std=c++17 -O2 -Wall -Wextra \
            -D_FILE_OFFSET_BITS=64 \
            -DOUTPUT_TIME_IN_NS \
            -Isrc -Ilib -Ilib/capsapi -Iinc \
            $(_FUSE_CFLAGS)

LDFLAGS  := $(_FUSE_LIBS) -lpthread -ldl

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
