#
# 'make depend' uses makedepend to automatically generate dependencies
#               (dependencies are added to end of Makefile)
# 'make'        build executable file
# 'make clean'  removes generated executable files
#

# define the C++ compiler to use
CXX = g++
UNAME_S := $(shell uname -s)

# define any compile-time flags -m32
# Allow DEBUG=yes to enable debug build
ifdef DEBUG
CXXFLAGS = -Wall -std=c++14 -Wno-psabi -g -O0 -pthread
else
CXXFLAGS = -Wall -std=c++14 -Wno-psabi -O3 -pthread
endif

# define any directories containing header files other than /usr/include
CPPFLAGS = -DCAPS_USER \
	-I../lib \
	-I../lib/capsimg \
	-I../lib/capsimg/inc \
	-I../lib/capsimg/Core \
	-I../lib/capsimg/LibIPF \
	-I../lib/capsimg/Codec \
	-I../lib/capsimg/Device \
	-I../lib/capsimg/CAPSImg \
	-I../lib/capsimg/CAPSImg/inc

# define any libraries to link into executable
ifeq ($(UNAME_S),Darwin)
LIBS =
else
LIBS = -ldl
endif

# define the C++ source files
SRCS = Main.cpp \
	../lib/ftdi.cpp \
	../lib/SerialIO.cpp \
	../lib/RotationExtractor.cpp \
	../lib/pll.cpp \
	../lib/ArduinoInterface.cpp \
	../lib/ADFWriter.cpp \
	../lib/capsapi/CapsPlug.cpp \
	../lib/ibm_sectors.cpp \
	../lib/capsimg/CAPSImg/CAPSImg.cpp \
	../lib/capsimg/CAPSImg/StreamImage.cpp \
	../lib/capsimg/CAPSImg/CapsFormatMFM.cpp \
	../lib/capsimg/CAPSImg/DiskImageFactory.cpp \
	../lib/capsimg/CAPSImg/DiskImage.cpp \
	../lib/capsimg/CAPSImg/CapsImage.cpp \
	../lib/capsimg/CAPSImg/CapsFile.cpp \
	../lib/capsimg/CAPSImg/CapsImageStd.cpp \
	../lib/capsimg/CAPSImg/CapsAPI.cpp \
	../lib/capsimg/CAPSImg/CapsLoader.cpp \
	../lib/capsimg/CAPSImg/StreamCueImage.cpp \
	../lib/capsimg/Codec/CTRawCodec.cpp \
	../lib/capsimg/Codec/CTRawCodecDecompressor.cpp \
	../lib/capsimg/Codec/DiskEncoding.cpp \
	../lib/capsimg/Core/CRC.cpp \
	../lib/capsimg/Core/DiskFile.cpp \
	../lib/capsimg/Core/BitBuffer.cpp \
	../lib/capsimg/Core/BaseFile.cpp \
	../lib/capsimg/Core/MemoryFile.cpp

# define the executable file
MAIN = drawbridge

.PHONY: depend clean

all: $(MAIN)
	@echo drawbridge ready

$(MAIN): $(SRCS)
	$(CXX) $(CXXFLAGS) $(CPPFLAGS) -o $(MAIN) $(SRCS) $(LIBS)

clean:
	$(RM) *~ $(MAIN)

depend: $(SRCS)
	makedepend $(CPPFLAGS) $^

# DO NOT DELETE THIS LINE -- make depend needs it
