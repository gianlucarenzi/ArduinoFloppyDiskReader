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
else ifeq ($(os),Windows)
TARGET_LIBRARY := CAPSImg.dll
else ifeq ($(os),macOS)
TARGET_LIBRARY := libcapsimage.dylib
else
TARGET_LIBRARY := libcapsimage.so
endif

all: build

build:
	make -C CAPSImg CROSS_COMPILE=$(CROSS_COMPILE) all
	# Copy produced DLLs and dylibs; for shared objects, copy only unversioned libcapsimage.so (create it from versioned if needed). \
	if ls CAPSImg/*.dll >/dev/null 2>&1; then \
		cp -f CAPSImg/*.dll .; \
	fi; \
	if ls CAPSImg/libcapsimage*.dylib >/dev/null 2>&1; then \
		cp -f CAPSImg/libcapsimage*.dylib .; \
	fi; \
	# Remove any previously copied versioned shared objects to avoid leaving libcapsimage.so.* files in this directory
	-rm -f libcapsimage.so.* 2>/dev/null || true; \
	# Prefer unversioned .so; if missing, create libcapsimage.so from the first versioned file found in CAPSImg/ \
	if [ -f CAPSImg/libcapsimage.so ]; then \
		cp -f CAPSImg/libcapsimage.so .; \
	elif ls CAPSImg/libcapsimage*.so* >/dev/null 2>&1; then \
		SRC=$$(ls CAPSImg/libcapsimage*.so* 2>/dev/null | head -n1); \
		cp -f "$$SRC" libcapsimage.so; \
	fi; \
	# Attempt to strip any copied binaries if a strip tool is available
	for f in *.dll libcapsimage*.dylib libcapsimage.so; do [ -f "$$f" ] || continue; $(CROSS_COMPILE)strip "$$f" 2>/dev/null || true; done; \
	# Choose a representative artifact to copy/rename to $(TARGET_LIBRARY)
	if ls *.dll >/dev/null 2>&1; then \
		SRC=$$(ls *.dll | head -n1); \
	elif ls libcapsimage*.dylib >/dev/null 2>&1; then \
		SRC=$$(ls libcapsimage*.dylib | head -n1); \
	elif [ -f libcapsimage.so ]; then \
		SRC=libcapsimage.so; \
	else \
		SRC=; \
	fi; \
	if [ -n "$$SRC" ]; then \
		cp -f "$$SRC" "$(TARGET_LIBRARY)"; \
		if [ "$(os)" = "macOS" ]; then install_name_tool -id "$(TARGET_LIBRARY)" "$(TARGET_LIBRARY)"; fi; \
	fi
clean:
	make -C CAPSImg CROSS_COMPILE=$(CROSS_COMPILE) clean

distclean:
	make -C CAPSImg CROSS_COMPILE=$(CROSS_COMPILE) clean
	-rm -f *.dll *.so *.dylib

