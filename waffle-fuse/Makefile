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
.PHONY: all clean install deb

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
	$(RM_CMD) waffle-nbd_*.deb

install: $(TARGET_NBD)
ifeq ($(UNAME),Windows)
	@echo "Install not supported on Windows – copy target manually."
else
	$(INSTALL_CMD) $(TARGET_NBD) /usr/local/bin/$(TARGET_NBD)
endif

# ── Debian package ────────────────────────────────────────────────────────────
VERSION  ?= 1.0.0
ARCH     := $(shell dpkg --print-architecture 2>/dev/null || echo amd64)
PKG_NAME := waffle-nbd_$(VERSION)_$(ARCH)
PKG_DIR  := /tmp/$(PKG_NAME)

.PHONY: deb
deb: $(TARGET_NBD)
	rm -rf "$(PKG_DIR)"
	install -d "$(PKG_DIR)/DEBIAN"
	install -d "$(PKG_DIR)/usr/bin"
	install -d "$(PKG_DIR)/usr/lib/waffle-nbd"
	install -d "$(PKG_DIR)/etc/udev/rules.d"
	install -d "$(PKG_DIR)/etc/polkit-1/rules.d"
	install -d "$(PKG_DIR)/usr/lib/tmpfiles.d"
	install -d "$(PKG_DIR)/usr/share/icons/hicolor/scalable/devices"
	install -m 755 $(TARGET_NBD)                    "$(PKG_DIR)/usr/bin/waffle-nbd"
	install -m 755 scripts/waffle-udev.sh           "$(PKG_DIR)/usr/lib/waffle-nbd/waffle-udev.sh"
	install -m 755 scripts/waffle-nbd-handler.sh    "$(PKG_DIR)/usr/lib/waffle-nbd/waffle-nbd-handler.sh"
	install -m 644 pkg/99-waffle-nbd.rules          "$(PKG_DIR)/etc/udev/rules.d/99-waffle-nbd.rules"
	install -m 644 scripts/85-waffle-nbd.rules      "$(PKG_DIR)/etc/polkit-1/rules.d/85-waffle-nbd.rules"
	install -m 644 pkg/waffle-nbd.conf              "$(PKG_DIR)/usr/lib/tmpfiles.d/waffle-nbd.conf"
	install -m 644 icons/amiga-floppy.svg           "$(PKG_DIR)/usr/share/icons/hicolor/scalable/devices/"
	install -m 644 icons/dos-floppy.svg             "$(PKG_DIR)/usr/share/icons/hicolor/scalable/devices/"
	sed "s/@VERSION@/$(VERSION)/; s/@ARCH@/$(ARCH)/" pkg/control > "$(PKG_DIR)/DEBIAN/control"
	install -m 755 pkg/postinst "$(PKG_DIR)/DEBIAN/postinst"
	install -m 755 pkg/prerm    "$(PKG_DIR)/DEBIAN/prerm"
	install -m 755 pkg/postrm   "$(PKG_DIR)/DEBIAN/postrm"
	dpkg-deb --build --root-owner-group "$(PKG_DIR)" $(PKG_NAME).deb
	rm -rf "$(PKG_DIR)"
	@echo ""
	@echo "Package ready: $(PKG_NAME).deb"
	@echo "Install with: sudo dpkg -i $(PKG_NAME).deb"


-include $(COMMON_OBJS:.o=.d) $(NBD_OBJS:.o=.d)
