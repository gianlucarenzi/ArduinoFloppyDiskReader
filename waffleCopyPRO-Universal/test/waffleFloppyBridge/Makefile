CXXFLAGS =	-O0 -g -pg -Wall -fmessage-length=0

OBJS =	\
	waffleFloppyBridge.o \
	ArduinoInterface.o \
	ftdi.o \
	pll.o \
	RotationExtractor.o \
	SerialIO.o \

INCLUDES = -I.


LIBS = -lpthread -ldl

TARGET =	waffleFloppyBridge

$(TARGET):	$(OBJS)
	$(CXX) $(INCLUDES) -o $(TARGET) $(OBJS) $(LIBS)

all:	$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)
