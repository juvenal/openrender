# Detect operating system
UNAME_S := $(shell uname -s)

# Prefer clang; fall back to gcc when clang is not on PATH
ifeq ($(origin CC), default)
  CC := $(shell command -v clang 2>/dev/null || echo gcc)
endif

# Paths are relative to this Makefile — run make from this directory
OPENRENDER_ROOT := ../../..
INCLUDE_DIR     := $(OPENRENDER_ROOT)/openrender/include
LIB_DIR         := $(OPENRENDER_ROOT)/openrender/lib

CFLAGS  := -std=c99 -Wall -O2 -I$(INCLUDE_DIR)
LDFLAGS := -L$(LIB_DIR) -lri

# macOS: supply the Xcode SDK sysroot and embed a loader-relative rpath
ifeq ($(UNAME_S), Darwin)
  SYSROOT := $(shell xcrun --show-sdk-path 2>/dev/null)
  ifneq ($(SYSROOT),)
    CFLAGS += -isysroot $(SYSROOT)
  endif
  LDFLAGS += -Wl,-rpath,@loader_path/../../../openrender/lib
else
  # Linux: embed an origin-relative rpath so the binary finds libri.so at runtime
  LDFLAGS += -Wl,-rpath,'$$ORIGIN/../../../openrender/lib'
endif

TARGET  := colorcircles
SOURCES := colorcircles.c

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

clean:
	$(RM) $(TARGET)
