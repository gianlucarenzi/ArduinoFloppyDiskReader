# Simple makefile
# ---------------

# If need to be cross-compiled invoke with:
# make CROSS=yes all
# otherwise
# make all
#
ifeq ($(DEBUG),yes)
	DEBUGFLAGS = -g -O0 -pg
endif

ifeq ($(CROSS),yes)
	CROSS_COMPILE ?= arm-none-linux-gnueabi-
	MTUNE ?= arm926ej-s
	MARCH ?= armv5te
	MODE ?= -marm
	FPU ?= -msoft-float
	PLATFORM_CFLAGS += \
		-mtune=$(MTUNE) \
		-march=$(MARCH) \
		$(MODE) $(FPU)
	SDL_CFLAGS ?= -D_GNU_SOURCE=1 \
		-D_REENTRANT \
		-I/usr/include/SDL
	SDL_LDFLAGS ?= -L$(TOOLCHAIN) \
		-lSDL
else
	SDL_CFLAGS ?= `sdl-config --cflags`
	SDL_LDFLAGS ?= `sdl-config --libs`
endif

# Toolchain definition.
CC = $(CROSS_COMPILE)gcc
CXX = $(CROSS_COMPILE)g++
OBJCOPY = $(CROSS_COMPILE)objcopy
OBJSIZE = $(CROSS_COMPILE)size
OBJDUMP = $(CROSS_COMPILE)objdump
STRIP = $(CROSS_COMPILE)strip
NM = $(CROSS_COMPILE)nm
AR = $(CROSS_COMPILE)ar
AS = $(CROSS_COMPILE)gcc -x assembler-with-cpp
LD = $(CROSS_COMPILE)ld
RM = rm

# Makefile starting point
CURRENTDIR	= .

# Where the source codes are
SRCDIR = $(CURRENTDIR)/src

# Where the includes are
INCLUDEDIRS = -I$(CURRENTDIR)/inc

OPTFLAG = \
	-ffunction-sections \
	-fdata-sections \
	-O3

# Useful for C programs
CSTDFLAG = -std=gnu99

# Define C warning options here
CWARN = -Wall \
		-Wmissing-declarations \
		-Wredundant-decls \
		-Wundef \
		-Wunknown-pragmas \


#		-Wmissing-prototypes \
#		-Wstrict-prototypes \

#		-Wunreachable-code \
#		-Wextra \
#		-Wpadded \
#		-Wshadow \
#		-Winline \

# Add here all special defines to be passed to code in pre-processing
DDEFS = 


# Compiler flags definition.
CFLAGS = \
		$(OPTFLAG) \
		$(CSTDFLAG) \
		$(CWARN) \
		$(INCLUDEDIRS) \
		$(DDEFS) \
		$(DEBUGFLAGS) \
		-Wa,-ahlms=$(@:.o=.lst)

CFLAGS += \
	$(PLATFORM_CFLAGS) \

CFLAGS += $(SDL_CFLAGS)

#		-fno-rtti \
#		-fno-exceptions \

#		-mthumb -DTHUMB \
#		-mfix-cortex-m3-ldrd

#The following are useful if C++ is used.
CPPFLAGS += \
	-fno-rtti \
	-fno-exceptions \

LDFLAGS = \
	$(SDL_LDFLAGS)

LDFLAGS += \
	-lSDL_image \
	-lSDL_mixer

#Project files (add all source codes here)
PROJECT_OBJS = \
	$(SRCDIR)/ximageview.o \


# All objects together
CORE_OBJS = $(PROJECT_OBJS)

# Executable filename
EXEC ?= ximageview

# Default goal
.PHONY: all

all: $(CORE_OBJS)
	$(CC) -o  $(EXEC) $(CORE_OBJS) $(LDFLAGS)
	$(STRIP) $(EXEC)

%.o: %.c
	$(CC) -c $(CFLAGS) -o $@ $<

%.o: %.cpp
	$(CXX) -c $(CPPFLAGS) $(CFLAGS) -o $@ $<

clean:
	@echo Cleaning...
	$(RM) -rf $(CORE_OBJS)
	$(RM) -rf $(SRCDIR)/*.lst

distclean: clean
	@echo Deep Cleaning...
	$(RM) -rf $(EXEC)

