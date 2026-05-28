# Display Plugin Guide

This document explains how to write a new file-format display plugin for openRender.

---

## Overview

Display output in openRender is handled by loadable modules installed to
`${OPENRENDER_DISPLAYSDIR}` (typically `<prefix>/displays/`). The renderer
locates a plugin at render time by looking for a file named `<type>.dsply`
(`.dll` on Windows) where `<type>` is the string given in the RIB `Display`
statement:

```renderman
Display "output.tif" "file"        "rgb"   # loads file.dsply
Display "output.exr" "openexr"     "rgb"   # loads openexr.dsply
Display "live"       "framebuffer" "rgba"  # loads framebuffer.dsply
```

The `.dsply` extension makes it unambiguous that these are orender-specific
loadable modules, not general shared libraries. Do **not** use `.so` or
`.dylib` — the renderer no longer searches for those extensions.

---

## The Plugin Interface (`dsply.h`)

Every plugin must export exactly these four C symbols:

```c
#include "ri/dsply.h"

// Called once at render begin. Return an opaque context pointer, or NULL on error.
void *displayStart(const char *name,    // output filename / display name
                   int width, int height,
                   int numSamples,      // number of channels (1=Z, 3=RGB, 4=RGBA, …)
                   const char *samples, // channel name string (e.g. "rgba")
                   TDisplayParameterFunction findParameter);

// Called once per render bucket. Return TRUE to stay active, FALSE to disable
// this display for the remainder of the render.
int displayData(void *handle, int x, int y, int w, int h, float *data);

// Optional alternative for non-float data. Return TRUE (no-op stub is fine).
int displayRawData(void *handle, int x, int y, int w, int h, void *data);

// Called at render end. Must finalize output and free all resources.
void displayFinish(void *handle);
```

`TDisplayParameterFunction` is a callback that lets the plugin query RIB
`Display` parameters by name:

```c
// Query a float array parameter:
float *q = (float *)findParameter("quantize", FLOAT_PARAMETER, 4);  // 4 floats
// Query a string parameter:
const char *s = (const char *)findParameter("compression", STRING_PARAMETER, 1);
```

Returns `NULL` if the parameter was not specified.

---

## Using `CFileOutputBase`

For file-format plugins, `file_base.h` (installed to `<prefix>/include/`) provides
`CFileOutputBase` — a base class that handles everything except the format-specific
write step:

- Scanline accumulation: collects out-of-order bucket tiles and delivers complete
  scanlines in order to the subclass
- Thread safety: mutex protecting all per-instance state
- Color pipeline: gain, gamma correction, quantization, and dither — read from the
  RIB `Display` parameters automatically in the constructor

### Template method pattern

```cpp
#include "file_base.h"

class CMyFormatFramebuffer : public CFileOutputBase {
public:
    CMyFormatFramebuffer(const char *name, int w, int h, int ns,
                         const char *samples, TDisplayParameterFunction fp)
        // pixelSize = bytes per pixel in your native format
        : CFileOutputBase(w, h, ns, ns * sizeof(uint8_t), fp)
    {
        // open the file, write your header, etc.
    }

    ~CMyFormatFramebuffer() override {
        // close the file; do NOT free scanlines[] — base class owns them
    }

    bool success() const override { return !!file_handle; }

protected:
    // Convert nPx float pixels (numSamples components each) into
    // scanlines[row] starting at byte offset xOff * pixelSize.
    // Called under fileMutex, so no additional locking is needed.
    void fillPixels(int row, int xOff, int nPx, const float *src) override {
        uint8_t *dst = scanlines[row] + xOff * numSamples;
        for (int j = nPx * numSamples; j > 0; j--)
            *dst++ = (uint8_t)std::clamp((int)(*src++ * 255.0f), 0, 255);
    }

    // Write the completed scanline scanlines[row] to the output file.
    // Called under fileMutex. The base class frees the buffer afterwards.
    void flushRow(int row) override {
        my_format_write_row(file_handle, scanlines[row], row);
    }

private:
    MyFileHandle *file_handle = nullptr;
};
```

### Protected members available in subclasses

| Member | Type | Description |
|--------|------|-------------|
| `width`, `height` | `int` | Image dimensions |
| `numSamples` | `int` | Channels per pixel |
| `pixelSize` | `int` | Bytes per pixel in native format |
| `lastSavedLine` | `int` | Next row awaiting flush |
| `scanlines` | `uint8_t**` | Per-row buffers (nullptr until tiles arrive) |
| `scanlineUsage` | `int*` | Remaining pixels until row is complete |
| `fileMutex` | `TMutex` | Held during `fillPixels` and `flushRow` |
| `gamma`, `gain` | `float` | Color pipeline parameters |
| `qzero`, `qone`, `qmin`, `qmax`, `qamp` | `float` | Quantize/dither parameters |

The color pipeline (gain, gamma, quantize, dither) is applied by `write()` before
calling `fillPixels()`, so `src` in `fillPixels()` already has the correct output
values. Do **not** re-apply gamma or quantization in `fillPixels()`.

---

## Wiring Up the Plugin Entry Points

```cpp
extern "C" {

void *displayStart(const char *name, int width, int height, int numSamples,
                   const char *samples, TDisplayParameterFunction fp) {
    auto *f = new CMyFormatFramebuffer(name, width, height, numSamples, samples, fp);
    if (!f->success()) { delete f; return nullptr; }
    return f;
}

int displayData(void *im, int x, int y, int w, int h, float *data) {
    assert(im != nullptr);
    static_cast<CMyFormatFramebuffer *>(im)->write(x, y, w, h, data);
    return TRUE;
}

int displayRawData(void * /*im*/, int /*x*/, int /*y*/,
                   int /*w*/, int /*h*/, void * /*data*/) {
    return TRUE;  // no-op stub; required for interface completeness
}

void displayFinish(void *im) {
    assert(im != nullptr);
    delete static_cast<CMyFormatFramebuffer *>(im);
}

} // extern "C"
```

---

## CMake Build Rules

Place your plugin in its own subdirectory and add it to `src/CMakeLists.txt`:

```cmake
# src/myfmt/CMakeLists.txt

add_library(myfmt MODULE myfmt.cpp)
set_target_properties(myfmt PROPERTIES PREFIX "" SUFFIX ".dsply")
# openrenderfilebase brings in openrendercommon transitively
target_link_libraries(myfmt PRIVATE openrenderfilebase MyFormat::MyFormat)
install(TARGETS myfmt LIBRARY DESTINATION "${OPENRENDER_DISPLAYSDIR}")
```

The `src/` directory is already in the include search path for all subdirectories
(set by `src/CMakeLists.txt`), so `#include "file/file_base.h"` will resolve
without any additional `target_include_directories` call.

Enable it conditionally:

```cmake
find_package(MyFormat QUIET)
if(MyFormat_FOUND)
    add_subdirectory(myfmt)
endif()
```

---

## Framebuffer Plugins

The framebuffer plugin (`src/framebuffer/`) follows the same `dsply.h` interface
but uses a different internal design: instead of writing scanlines to disk, it
forwards pixel tiles over a Unix domain socket to a persistent helper process
(`orender-fb-macos` or `orender-fb-linux`). A new window is opened for each
`START` packet; the helper persists across renders.

There is deliberately no shared base class between file-format and framebuffer
plugins — their lifecycles and transport mechanisms are completely different.
The common contract is solely the four C entry points in `dsply.h`.

For details on the IPC protocol and helper architecture, see
[FRAMEBUFFER_GUIDE.md](FRAMEBUFFER_GUIDE.md).
