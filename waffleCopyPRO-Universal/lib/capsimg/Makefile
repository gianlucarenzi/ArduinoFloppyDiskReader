uname := $(shell uname -s)

ifeq ($(findstring MINGW,$(uname)),MINGW)
  os := Windows
else ifeq ($(findstring MSYS,$(uname)),MSYS)
  os := Windows
else ifeq ($(findstring CYGWIN,$(uname)),CYGWIN)
  os := Windows
else ifeq ($(findstring Darwin,$(uname)),Darwin)
  os := macOS
else
  os := Linux
endif

# If CROSS_COMPILE suggests mingw, treat as Windows target so top-level copies DLL
ifneq ($(filter %mingw%,$(CROSS_COMPILE)),)
  os := Windows
endif

all: build

build:
	make -C CAPSImg CROSS_COMPILE=$(CROSS_COMPILE)
	# locate the built library in CAPSImg/ (supports various names)
	LIBPATH=$$(ls CAPSImg/CAPSImg.dll CAPSImg/libcapsimage*.dylib CAPSImg/libcapsimage*.so* 2>/dev/null | head -n 1); \
	if [ -z "$$LIBPATH" ]; then \
		echo "ERROR: cannot find built capsimg library in CAPSImg/"; exit 1; \
	fi; \
	if [ "$(os)" = "Windows" ]; then \
		cp "$$LIBPATH" capsimg.dll; \
	elif [ "$(os)" = "macOS" ]; then \
		cp "$$LIBPATH" capsimg.so; install_name_tool -id capsimg.so capsimg.so; \
	else \
		cp "$$LIBPATH" capsimg.so; \
	fi

clean:
	make -C CAPSImg CROSS_COMPILE=$(CROSS_COMPILE) clean
	rm -f capsimg.so capsimg.dll dist.fs

distclean: clean
	make -C CAPSImg CROSS_COMPILE=$(CROSS_COMPILE) distclean
