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

# set TARGET_LIBRARY name per platform (Windows, macOS, Linux)
ifneq ($(filter %mingw%,$(CROSS_COMPILE)),)
TARGET_LIBRARY := CAPSImg.dll
else ifeq ($(os),macOS)
TARGET_LIBRARY := libcapsimage.dylib
else
TARGET_LIBRARY := libcapsimage.so
endif

all: build

build:
	make -C CAPSImg CROSS_COMPILE=$(CROSS_COMPILE)
	@sh -c '\
	if [ -f CAPSImg/CAPSImg.dll ]; then \
		# copy the explicit DLL produced by CAPSImg build to the appropriate name; \
		$(CROSS_COMPILE)strip CAPSImg/$(TARGET_LIBRARY) 2>/dev/null || true; \
		cp CAPSImg/$(TARGET_LIBRARY) $(TARGET_LIBRARY); \
	else \
		LIBPATH=$$(ls CAPSImg/libcapsimage*.dylib CAPSImg/libcapsimage*.so* 2>/dev/null | head -n 1); \
		if [ -z "$$LIBPATH" ]; then \
			echo "ERROR: cannot find built capsimg library in CAPSImg/"; exit 1; \
		fi; \
		$(CROSS_COMPILE)strip "$$LIBPATH" 2>/dev/null || true; \
		cp "$$LIBPATH" $(TARGET_LIBRARY); \
		if [ "$(os)" = "macOS" ]; then install_name_tool -id $(TARGET_LIBRARY) $(TARGET_LIBRARY); fi; \
	fi'

clean:
	make -C CAPSImg CROSS_COMPILE=$(CROSS_COMPILE) clean
	rm -f $(TARGET_LIBRARY) dist.fs

