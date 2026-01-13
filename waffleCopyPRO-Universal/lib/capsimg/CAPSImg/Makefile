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

# detect Windows cross-compilers (heuristic: mingw in prefix or compiler name)
ifneq ($(filter %mingw%,$(CROSS_COMPILE) $(CC) $(CXX)),)
PICFLAGS :=
# When cross-compiling for Windows (mingw), define TARGET_WINDOWS and build a DLL
CFLAGS += -DTARGET_WINDOWS
CXXFLAGS += -DTARGET_WINDOWS
LDFLAGS := -shared -Wl,--out-implib,libcapsimage.a
LIBS :=
LIBRARY := CAPSImg.dll
else
LDFLAGS := -shared -Wl,-soname,libcapsimage.so.5 -Wl,--version-script,libcapsimage.map
LIBS :=
LIBRARY := libcapsimage.so.5.1
endif
OBJECTS		=  ../Codec/CTRawCodec.o ../Codec/CTRawCodecDecompressor.o ../Codec/DiskEncoding.o ../Core/BaseFile.o ../Core/BitBuffer.o ../Core/CRC.o ../Core/DiskFile.o ../Core/MemoryFile.o CapsAPI.o CapsFDCEmulator.o CapsFile.o CapsFormatMFM.o CapsImage.o CapsImageStd.o CapsLoader.o DiskImage.o DiskImageFactory.o stdafx.o StreamCueImage.o StreamImage.o


.PHONY: all clean distclean dist

all: $(LIBRARY)

$(LIBRARY): $(OBJECTS)
ifeq ($(filter %mingw%,$(CROSS_COMPILE)),)
	$(CXX) $(LDFLAGS) $(PICFLAGS) $^ $(LIBS) -o $@
else
	$(CXX) -shared $(LDFLAGS) $(PICFLAGS) $^ $(LIBS) -o CAPSImg.dll
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
CAPSImg.dll: $(OBJECTS)
	$(CXX) -shared -Wl,--out-implib,libcapsimage.a $(PICFLAGS) $^ $(LIBS) -o CAPSImg.dll
endif
