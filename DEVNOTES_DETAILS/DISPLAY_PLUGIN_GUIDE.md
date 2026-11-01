# Display Plugin Guide

This document explains how to write a display driver for openRender: what the host
calls, what it hands you, what it promises about threads and lifetimes, and how a
driver written for the old interface keeps working. The current interface is
display 2.0.0. The shared rules every plug-in family follows (version encoding, size
rule, status codes, ownership) are in [PLUGIN_ABI.md](PLUGIN_ABI.md); the version
numbers below are quoted from the version-history table at the top of `dsply.h`
(`src/display/dsply.h`), which is their single source.

---

## Overview

Display output in openRender is handled by loadable modules installed to
`${OPENRENDER_DISPLAYSDIR}` (typically `<prefix>/displays/`). The renderer
locates a plugin at render time by looking for a file named `<type>.dsply`
(`.dll` on Windows) where `<type>` is the string given in the RIB `Display`
statement, in the directories of the display search path:

```renderman
Display "output.tif" "file"        "rgb"   # loads file.dsply
Display "output.exr" "openexr"     "rgb"   # loads openexr.dsply
Display "live"       "framebuffer" "rgba"  # loads framebuffer.dsply
```

The `.dsply` extension makes it unambiguous that these are orender-specific
loadable modules, not general shared libraries. Do **not** use `.so` or
`.dylib` — the renderer no longer searches for those extensions.

A third-party driver needs two installed headers, `dsply.h` and `orplugin.h`, and
nothing else from openRender. Where they are depends on the install layout:

| Layout | Include directory |
|--------|-------------------|
| self-contained (`INSTALL_SELFCONTAINED=ON`, the default) | `<ORENDERHOME>/include` |
| FHS (`INSTALL_SELFCONTAINED=OFF`) | the `openRender` subdirectory of the system include directory (`/usr/local/include/openRender` by default) |

`find_package(openRender CONFIG)` puts the right directory on every `openRender::`
target and sets `openRender_INCLUDE_DIR`, so a CMake build needs no hand-written path.

---

## Versions

The interface is display 2.0.0. A driver declares the version it was built against,
once, at file scope:

```c
#include "dsply.h"

OR_DISPLAY_DECLARE_VERSION()   /* exports orDisplayAbiVersion() */
```

The host calls that symbol before it calls anything else in the module and decides
by MAJOR: a MAJOR it supports loads, a MINOR newer than the host's loads with one
warning, any other MAJOR is rejected with an error naming the file and both versions
(rebuild against the installed header). PATCH never gates loading. The version
history (quoted from the header):

| Version | Change |
|---|---|
| 1.0.0 (display) | The Pixie-era interface: `displayStart`, `displayData`, `displayRawData` and `displayFinish`, with a parameter callback that answers from the renderer's own state. Modules without `orDisplayAbiVersion` are 1.0.0. |
| 2.0.0 (display) | The function table `OrDisplayPlugin`, a per-display context, a `const` snapshot of the parameters, channel descriptors and the threading contract below; `displayRawData` removed. |

MAJOR 1 stays supported through the host's version 1 adapter (see "Drivers written
for the old interface"). The interface MAJORs are independent of the shared
library version (`OPENRENDER_COMPAT_SOVERSION`): moving to display 2.0.0 changed
neither.

---

## The entry points

A driver exports one function, `orDisplayPlugin`, which returns a static table of
three entry points, and declares its version (above):

```c
static const OrDisplayPlugin table = {
    { sizeof(OrDisplayPlugin), OR_DISPLAY_ABI_VERSION },  /* the table header */
    start, data, finish
};

const OrDisplayPlugin *orDisplayPlugin(const OrHostInfo *host) {
    (void)host;
    return &table;
}
```

```c
/* Called once per display. OR_STATUS_OK starts it; *instance is yours (it may stay
   NULL) and comes back unchanged in every later call. Any other status means "not
   started": no data call, no finish call. */
OrStatus start(OrDisplayContext *ctx, const OrDisplayInfo *info, void **instance);

/* Called for each tile of pixels. Non-OK disables this display. */
OrStatus data(OrDisplayContext *ctx, void *instance,
              int x, int y, int width, int height, float *pixels);

/* Called exactly once for every display that started. Free your instance here. */
void finish(OrDisplayContext *ctx, void *instance);
```

The table header records the size of the table as you compiled it and the interface
version; the host refuses a table smaller than the 2.0.0 table and reads a field added
by a later MINOR only when your declared version includes it. `OrStatus` and the
`OR_STATUS_*` codes come from `orplugin.h` (`src/include/orplugin.h`); a C++ driver
can build the table with `orplugin::makeTable` from `orplugin.hpp`, which produces the
same bytes as the initializer above.

A missing `start`, `data` or `finish` in the table is reported as "has missing
implementation" and the display is not used. A driver compiled with hidden symbol
visibility still exports its entry points: `dsply.h` marks them `OR_VISIBLE`.

---

## What `start` receives: the snapshot

`OrDisplayInfo` is a read-only snapshot, one per display. Every pointer in it is valid
from `start` until `finish` returns and never aliases renderer state: a driver cannot
change a clip plane or another display's quantizer however it writes. Copy what you
need for longer.

| Field | Meaning |
|---|---|
| `name` | the output name, frame expansions applied |
| `type` | the display type as written in the RIB (`file`, `tiff`, `png`, ...) |
| `mode` | the mode string as written, informational |
| `width`, `height` | the image size |
| `numSamples` | floats per pixel in `data` (the sum over the channels) |
| `channels` | one descriptor per channel (below) |
| `parameters` | the RIB display parameters in order, then the renderer-provided ones |

### Parameters

Look a parameter up with `orDisplayFindParameter(info, name, type, numItems)`. It
returns the first entry whose name, type and count all match (the RIB's own
parameters come first), or `NULL`:

```c
const float *quantize = orDisplayFindParameter(info, "quantize", OR_DISPLAY_PARAM_FLOAT, 4);
const char  *type     = orDisplayFindParameter(info, "type", OR_DISPLAY_PARAM_STRING, 1);
```

A string parameter's `data` is the NUL-terminated string itself. The renderer always
adds these after the RIB's own: `quantize` (4 floats) and `dither` (1) — the display's
own `Quantize` when it set one, otherwise the depth defaults for a mode of exactly `z`
and the colour defaults for everything else — `near` and `far`, the matrices `Nl`, `NP`
and `screen` (16 floats each), `gamma`, `gain`, and the strings `Software` and `type`.
RIB parameters of a type the interface does not carry (`double`, `quad`, `mpoint`) are
dropped with a warning (one per parameter) when the RIB is read; they never reach a driver.

### Channel descriptors

`channels` has `numChannels` entries in mode order. Each gives the channel `name`, its
`numSamples`, a `valueType` (`OR_DISPLAY_VALUE_FLOAT` for the standard rgba and z
channels and float AOVs, or colour, point, vector, normal, matrix for AOVs of that
type; the storage is always 32-bit float), the `offset` of its first float inside one
pixel, and an optional per-channel `quantize` array that overrides the display's. The
`numSamples` of the channels add up to `info->numSamples` on every tile, and a driver
no longer has to parse the mode string.

---

## Threads and lifetimes

This is the contract the host keeps (it is also stated at the top of `dsply.h`):

1. `start` runs once per display, on one thread, with no other call for that display
   in flight.
2. `data` may run concurrently from several threads, for **disjoint tiles** of the same
   display. `pixels` holds `width * height * info->numSamples` floats, row-major. It is
   the host's per-call copy: you may modify it in place (the in-tree drivers clamp and
   quantize it there), and it is valid only during the call.
3. A non-OK `data` disables the display. The host starts no new `data` call once it has
   seen the failure; calls already running on other threads may still arrive and should
   return quickly (return non-OK again).
4. `finish` runs exactly once for every display that started, enabled or disabled,
   after every `data` call has returned, on one thread. Nothing is called after it.
   Free the instance in `finish`, never in `data`.
5. `info` and everything it points to is valid from `start` until `finish` returns.
6. The module is unloaded only after `finish` has returned for every display that uses
   it. A display that never started releases its reference at the same point.
7. Different displays, even of one module, may be in `data` at the same time. State you
   share between them (module-wide) is yours to protect.

Two consequences worth stating. A driver does not need a lock for tile writes if each
tile touches only its own pixels, because tiles are disjoint. And there is no global
"current display": anything you need later, keep in your instance.

---

## An example: `simpledisplay.c`

`examples/display/simpledisplay.c` is a complete driver in strict C17: it collects the
tiles into a buffer in `start`/`data` and writes a binary PPM in `finish`. It includes
only the installed `dsply.h` and `orplugin.h`. `examples/display/README.md` has the compile line
for each layout:

```bash
# Linux
cc -std=c17 -O2 -fPIC -shared -I "$INC" simpledisplay.c -o simpledisplay.dsply
# macOS
cc -std=c17 -O2 -bundle -I "$INC" simpledisplay.c -o simpledisplay.dsply
```

`INC` is the include directory of your layout (table above). Put the module in the
displays directory, or in any directory a RIB names with
`Option "searchpath" "display"`, and ask for it by name:

```renderman
Option "searchpath" "display" ["/path/to/modules"]
Display "image.ppm" "simpledisplay" "rgb"
```

The build and a render through it are checked in both layouts by the
`PluginAbi_InstallHeaders` test.

---

## Drivers written for the old interface

A module without `orDisplayAbiVersion`, or one declaring MAJOR 1, is a version 1.x
driver. The host still loads it, through an adapter that presents it as a version 2
driver. What changes for such a driver:

- **Writes through returned pointers are lost.** The old parameter callback returned
  pointers into the renderer's own state, and a driver could write through them. The
  adapter hands out mutable *copies*, taken per display; a write changes only that
  copy. (This is what makes a driver unable to corrupt the renderer or another
  display.)
- **The parameter callback works only inside `displayStart`.** A lookup made later, from
  `displayData` or another thread, returns `NULL` and logs once for that display.
  Query everything in `displayStart` and keep it.
- **`displayRawData` is ignored.** The pixels arrive through `displayData` only.
- **`displayData` is serialized.** The adapter never lets two `displayData` calls for
  one display overlap, because the old interface never promised thread safety; a v1
  driver needs no new locking.
- **No `displayFinish` after a failed `displayData`.** Under the old contract a driver
  that returned FALSE had already released itself; the adapter keeps that.
- **`RiCustomDisplay`** callbacks go through the same adapter and receive the same
  values as before.

An *unversioned* module (no `orDisplayAbiVersion` at all) loads as 1.0.0 and prints
one warning per module per process: it will be rejected from openRender 1.2.0.
Declare the version you were built against and rebuild to silence it; a module that
declares 1.x explicitly prints nothing. A driver written against the old header
migrates to 2.0.0 by replacing the four functions with the table above, reading
parameters through `orDisplayFindParameter` from the snapshot in `start`, and moving
its cleanup into `finish`.

---

## In-tree file-format drivers

The in-tree file drivers (`src/display/file/`: TIFF and PNG; `src/display/openexr/`;
`src/display/rgbe/`) share `CFileOutputBase`, a base class that handles everything
except the format-specific write step. It is an in-tree helper and is **not
installed**: a plug-in built outside the tree writes against `dsply.h` alone, as
`simpledisplay.c` does. The base class does:

- Scanline accumulation: collects out-of-order bucket tiles and delivers complete
  scanlines in order to the subclass.
- Thread safety: a mutex protecting all per-instance state.
- Color pipeline: quantization and dither, read from the snapshot
  (`orDisplayFindParameter`) in the constructor. Exposure (gain/gamma) is applied
  upstream, before the imager shader runs; `CFileOutputBase` does not re-apply it.

A subclass overrides `fillPixels` (convert `nPx` quantized float pixels into the row
buffer) and `flushRow` (write one finished row), and reports success. The protected
members (`width`, `height`, `numSamples`, `pixelSize`, the scanline buffers, the
quantize and dither values, `gamma`) are listed in the class declaration. Quantization
and dither are applied before `fillPixels`, so its input is already quantized; do not
quantize or apply gamma again there. The module's entry points are then an ordinary
version 2 driver: `start` builds the object from the snapshot, `data` forwards the tile,
`finish` deletes it.

Add the module to the build in its own subdirectory, linking the helper:

```cmake
# src/display/myfmt/CMakeLists.txt
add_library(myfmt MODULE myfmt.cpp)
set_target_properties(myfmt PROPERTIES PREFIX "" SUFFIX ".dsply")
target_link_libraries(myfmt PRIVATE openrenderfilebase MyFormat::MyFormat)
install(TARGETS myfmt LIBRARY DESTINATION "${OPENRENDER_DISPLAYSDIR}")
```

and enable it conditionally from `src/display/CMakeLists.txt` when its library is
found.

---

## Framebuffer plugins

The framebuffer plugin (`src/display/framebuffer/`) follows the same `dsply.h` interface
but uses a different internal design: instead of writing scanlines to disk, it
forwards pixel tiles over a Unix domain socket to a persistent helper process
(`orender-fb-macos` or `orender-fb-linux`). A new window is opened for each
`START` packet; the helper persists across renders. Its `finish` closes the plug-in's
own connection and frees its object; the helper and the window stay open.

There is deliberately no shared base class between file-format and framebuffer
plugins — their lifecycles and transport mechanisms are completely different.
The common contract is the three entry points of the table above.

For details on the IPC protocol and helper architecture, see
[FRAMEBUFFER_GUIDE.md](FRAMEBUFFER_GUIDE.md).
