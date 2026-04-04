# waffle-fuse Makefile
# Builds a standalone FUSE driver for DrawBridge/Waffle floppy reader.
# Supports Linux (libfuse2 as well as libfuse3), macOS (macFUSE) and Windows (WinFsp).
#
# Dependencies:
#   Linux:   sudo apt install libfuse3-dev libfuse-dev
#   macOS:   brew install macfuse
#   Windows: https://github.com/winfsp/winfsp/releases
#
# Flags:
#   USE_FUSE2=0   Force FUSE 3 on Linux (useful to test macOS/Windows compat)
#                 Example: make USE_FUSE2=0

CXX := g++
# Default is for FUSE2
USE_FUSE2 ?= 1

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
    CXX          := clang++
    # dlopen/dlclose are in libSystem on macOS; no -ldl needed
    _EXTRA_LDFLAGS := -lpthread
    TARGET_EXT   :=
    RM_CMD       := rm -f
    INSTALL_CMD  := install -m 755

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
    _EXTRA_LDFLAGS :=
    TARGET_EXT   := .exe
    RM_CMD       := del /Q
    INSTALL_CMD  :=

else
    # Linux: libfuse3-dev by default; USE_FUSE2=1 selects libfuse-dev (FUSE 2)
    ifeq ($(USE_FUSE2),1)
        _FUSE_CFLAGS := $(shell pkg-config --cflags fuse) -DUSE_FUSE2
        _FUSE_LIBS   := $(shell pkg-config --libs   fuse)
    else
        _FUSE_CFLAGS := $(shell pkg-config --cflags fuse3)
        _FUSE_LIBS   := $(shell pkg-config --libs   fuse3)
    endif
    _EXTRA_LDFLAGS := -lpthread -ldl
    TARGET_EXT   :=
    RM_CMD       := rm -f
    INSTALL_CMD  := install -m 755

endif

CXXFLAGS := -std=c++17 -O2 -Wall -Wextra \
            -D_FILE_OFFSET_BITS=64 \
            -DOUTPUT_TIME_IN_NS \
            -Isrc -Ilib -Iinc \
            $(_FUSE_CFLAGS)

LDFLAGS  := $(_FUSE_LIBS) $(_EXTRA_LDFLAGS)

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

FUSE_SRCS := src/fat_fs.cpp \
             src/affs_fs.cpp \
             src/waffle_fuse.cpp

NBD_SRCS  := src/nbd_server.cpp \
             src/waffle_nbd.cpp

COMMON_OBJS := $(COMMON_LIB_SRCS:.cpp=.o) $(COMMON_SRC_SRCS:.cpp=.o)
FUSE_OBJS   := $(FUSE_SRCS:.cpp=.o)
NBD_OBJS    := $(NBD_SRCS:.cpp=.o)

TARGET_FUSE := waffle-fuse$(TARGET_EXT)
TARGET_NBD  := waffle-nbd$(TARGET_EXT)

ALL_TARGETS := $(TARGET_FUSE) $(TARGET_NBD)

# ── Build rules ───────────────────────────────────────────────────────────────
.PHONY: all clean install

all: $(ALL_TARGETS)

$(TARGET_FUSE): $(COMMON_OBJS) $(FUSE_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

$(TARGET_NBD): $(COMMON_OBJS) $(NBD_OBJS)
	$(CXX) -o $@ $^ $(_EXTRA_LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) -c $< -o $@

# capsapi includes debugmsg from inc/
lib/capsapi/CapsPlug.o: lib/capsapi/CapsPlug.cpp
	$(CXX) $(CXXFLAGS) -Iinc -c $< -o $@

clean:
	$(RM_CMD) $(COMMON_OBJS) $(FUSE_OBJS) $(NBD_OBJS) $(ALL_TARGETS)
	$(RM_CMD) $(COMMON_OBJS:.o=.d) $(FUSE_OBJS:.o=.d) $(NBD_OBJS:.o=.d)

install: $(ALL_TARGETS)
ifeq ($(UNAME),Windows)
	@echo "Install not supported on Windows – copy targets manually."
else
	$(INSTALL_CMD) $(TARGET_FUSE) /usr/local/bin/$(TARGET_FUSE)
	$(INSTALL_CMD) $(TARGET_NBD) /usr/local/bin/$(TARGET_NBD)
endif

# ── Dependency generation ─────────────────────────────────────────────────────
-include $(COMMON_OBJS:.o=.d) $(FUSE_OBJS:.o=.d) $(NBD_OBJS:.o=.d)
