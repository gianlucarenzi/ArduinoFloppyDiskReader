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
	make -C CAPSImg CROSS_COMPILE=$(CROSS_COMPILE)
	@sh -c '\
	# Prefer explicit DLL files in CAPSImg/; otherwise try to detect a PE DLL among produced artifacts, fallback to libcapsimage* names. \
	if ls CAPSImg/*.dll >/dev/null 2>&1; then \
		SRC=$$(ls CAPSImg/*.dll 2>/dev/null | head -n1); \
	else \
		# look for any PE file in CAPSImg/ (linker may produce a PE file with a .so extension) \
		SRC=$$(for f in CAPSImg/*; do [ -f "$$f" ] || continue; file "$$f" 2>/dev/null | grep -qi PE && { echo "$$f"; break; } done); \
		if [ -z "$$SRC" ]; then \
			SRC=$$(ls CAPSImg/libcapsimage*.dylib CAPSImg/libcapsimage*.so* 2>/dev/null | head -n1); \
		fi; \
	fi; \
	if [ -z "$$SRC" ]; then \
		echo "ERROR: cannot find built capsimg library in CAPSImg/"; exit 1; \
	fi; \
	$(CROSS_COMPILE)strip "$$SRC" 2>/dev/null || true; \
	case "$$SRC" in \
		*.dll) DEST=CAPSImg.dll ;; \
		*.dylib) DEST=libcapsimage.dylib ;; \
		*.so*) DEST=libcapsimage.so ;; \
		*) DEST=$(TARGET_LIBRARY) ;; \
	esac; \
	cp "$$SRC" "$$DEST"; \
	if [ "$(os)" = "macOS" ]; then install_name_tool -id "$$DEST" "$$DEST"; fi; \
	'
clean:
	make -C CAPSImg CROSS_COMPILE=$(CROSS_COMPILE) clean
	rm -f $(TARGET_LIBRARY) dist.fs

