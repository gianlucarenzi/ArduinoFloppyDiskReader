distdir		= x86_64-linux-gnu-capsimage
prefix = /usr/local
exec_prefix = ${prefix}
libdir = ${exec_prefix}/lib

CROSS_COMPILE ?=
CC ?= gcc
CXX ?= g++
# allow CROSS_COMPILE prefix (e.g. x86_64-w64-mingw32-)
ifneq ($(strip $(CROSS_COMPILE)),)
CC := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)g++
endif

INSTALL		= /usr/bin/install -c
TAR		= tar

CFLAGS		= -Wall -Wno-sign-compare -Wno-missing-braces -Wno-parentheses -g  -O2 -fomit-frame-pointer  -I./../inc -I./../LibIPF -I./../Codec -I./../Core -I./../Device -I.
CXXFLAGS	= -Wall -Wno-sign-compare -Wno-missing-braces -Wno-parentheses -g  -O2 -fomit-frame-pointer  -fno-exceptions -fno-rtti -std=c++17 -DCAPSIMG_STANDALONE -I./../inc -I./../LibIPF -I./../Codec -I./../Core -I./../Device -I.
PICFLAGS	= -fPIC

# detect Windows cross-compilers (heuristic: prefer CROSS_COMPILE containing 'mingw', fallback to compiler names)
ifneq ($(findstring mingw,$(CROSS_COMPILE)),)
PICFLAGS :=
# When cross-compiling for Windows (mingw), define TARGET_WINDOWS and build a DLL
CFLAGS += -DTARGET_WINDOWS
CXXFLAGS += -DTARGET_WINDOWS
TARGET_LIBRARY := CAPSImg.dll
IMPLIB := libcapsimage.a
LDFLAGS := -shared -Wl,--out-implib,$(IMPLIB)
LIBS :=
else ifneq ($(findstring mingw,$(CC)$(CXX)),)
PICFLAGS :=
CFLAGS += -DTARGET_WINDOWS
CXXFLAGS += -DTARGET_WINDOWS
TARGET_LIBRARY := CAPSImg.dll
IMPLIB := libcapsimage.a
LDFLAGS := -shared -Wl,--out-implib,$(IMPLIB)
LIBS :=
else
TARGET_LIBRARY := libcapsimage.so
# set soname to TARGET_LIBRARY (no version number). If versioned soname desired, adjust SONAME variable.
LDFLAGS := -shared -Wl,-soname,$(TARGET_LIBRARY) -Wl,--version-script,libcapsimage.map
LIBS :=
endif

# macOS override when building on Darwin hosts
ifneq ($(findstring Darwin,$(shell uname -s)),)
TARGET_LIBRARY := libcapsimage.dylib
endif

LIBRARY := $(TARGET_LIBRARY)


OBJECTS		=  ../Codec/CTRawCodec.o ../Codec/CTRawCodecDecompressor.o ../Codec/DiskEncoding.o ../Core/BaseFile.o ../Core/BitBuffer.o ../Core/CRC.o ../Core/DiskFile.o ../Core/MemoryFile.o CapsAPI.o CapsFDCEmulator.o CapsFile.o CapsFormatMFM.o CapsImage.o CapsImageStd.o CapsLoader.o DiskImage.o DiskImageFactory.o stdafx.o StreamCueImage.o StreamImage.o


.PHONY: all clean distclean dist

all: $(LIBRARY)

$(LIBRARY): $(OBJECTS)
ifneq ($(filter %mingw%,$(CROSS_COMPILE)),)
	$(CXX) $(LDFLAGS) $(PICFLAGS) $^ $(LIBS) -o $(TARGET_LIBRARY); \
	# If the linker produced a different artifact (e.g., libcapsimage.so), copy it to $(TARGET_LIBRARY)
	if [ -f $(TARGET_LIBRARY) ]; then :; else LIBFILE=$$(ls libcapsimage* 2>/dev/null | head -n1); if [ -n "$$LIBFILE" ]; then cp "$$LIBFILE" $(TARGET_LIBRARY); fi; fi
else
	$(CXX) -shared $(LDFLAGS) $(PICFLAGS) $^ $(LIBS) -o $(TARGET_LIBRARY)
endif

capsimage.o: capsimage.c
	$(CC) $(CFLAGS) -c $< -o $@

%.o: %.c
	$(CC) $(CFLAGS) $(PICFLAGS) -c $< -o $@

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(PICFLAGS) -c $< -o $@

clean:
	$(RM) $(LIBRARY) $(OBJECTS)

distclean:
	$(RM) $(LIBRARY) $(OBJECTS) Makefile config.status config.log config.cache config.h

dist: $(LIBRARY)
	$(RM) -r $(distdir)
	$(INSTALL) -d $(distdir)
	$(INSTALL) -s $(LIBRARY) $(distdir)
	$(INSTALL) HISTORY LICENSE README $(distdir)
	$(TAR) czf $(distdir).tar.gz $(distdir)

install:
	install -d $(DESTDIR)$(libdir)
	install $(LIBRARY) $(DESTDIR)$(libdir)/$(LIBRARY)

# Explicitly build a Windows DLL when cross-compiling for mingw
ifneq ($(filter %mingw%,$(CROSS_COMPILE)),)
$(TARGET_LIBRARY): $(OBJECTS)
	$(CXX) $(LDFLAGS) $(PICFLAGS) $^ $(LIBS) -o $(TARGET_LIBRARY)
endif
