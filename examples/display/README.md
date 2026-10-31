# simpledisplay: a display driver in C

`simpledisplay.c` is a complete display 2.0 driver in strict C17. It collects the image
the renderer sends and writes it as a binary PPM file when the display finishes. It
includes only the installed headers `dsply.h` and `orplugin.h`, so it builds against an
installed openRender and nothing else.

## Build

The headers are installed in one directory, which depends on the install layout:

| Layout | Include directory |
|---|---|
| self-contained (`INSTALL_SELFCONTAINED=ON`, the default) | `<ORENDERHOME>/include` |
| FHS (`INSTALL_SELFCONTAINED=OFF`) | `<prefix>/include/openRender` |

With `INC` set to that directory:

```bash
# Linux
cc -std=c17 -O2 -fPIC -shared -I "$INC" simpledisplay.c -o simpledisplay.dsply

# macOS
cc -std=c17 -O2 -bundle -I "$INC" simpledisplay.c -o simpledisplay.dsply
```

Nothing is linked: the module exports `orDisplayPlugin` and `orDisplayAbiVersion` and
calls no renderer function. CMake users get the same include directory from
`find_package(openRender CONFIG REQUIRED)` (the `openRender_INCLUDE_DIR` variable, and
the interface include directories of every `openRender::` target).

## Run

Put `simpledisplay.dsply` in the displays directory (`<ORENDERHOME>/displays`) or in any
directory the RIB file names with `Option "searchpath" "display"`, and ask for it by
name:

```
Option "searchpath" "display" ["/path/to/the/module/directory"]
Display "image.ppm" "simpledisplay" "rgb"
```

## What it shows

The version declaration, `OR_DISPLAY_DECLARE_VERSION()`, once at file scope. The one
entry point, `orDisplayPlugin`, which returns the `OrDisplayPlugin` table (`start`,
`data`, `finish`). The threading contract stated in `dsply.h`: `start` runs once per
display, `data` may run concurrently for disjoint tiles, and `finish` runs exactly once
for every started display, after the last `data` call. The driver copies what it needs
out of `OrDisplayInfo`, which is valid only until `finish` returns.

The interface rules and the version history are in `dsply.h` and in
`DEVNOTES_DETAILS/DISPLAY_PLUGIN_GUIDE.md`.
