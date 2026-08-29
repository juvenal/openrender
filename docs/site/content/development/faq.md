---
title: "FAQ"
date: 2026-08-29
---

# Frequently Asked Questions

## Troubleshooting

- [Why won't openRender run?](#why-wont-openrender-run) - Common issues and solutions
- [What's wrong with my render?](/openrender/development/whats-wrong-with-my-render/) - Rendering problems and fixes
- [How do I install on macOS?](/openrender/development/installing-on-osx/) - Installation notes for macOS

## Why Won't openRender Run?

This is a list of things to check if you can't get openRender to run.

## Are the requirements satisfied

openRender is built from source; there is no binary distribution to unpack.
The full prerequisite list is in
[Installing / running openRender](/openrender/manual/reference/installing-and-running/#prerequisites),
but the ones that most often go missing are:

- **libtiff, libpng and zlib** — required. Textures and output images go
  through them.
- **bison** — required to regenerate the parsers, and on macOS it must be
  Homebrew's, because the system bison is version 2.3 and far too old. See
  [Installing on macOS](/openrender/development/installing-on-osx/).
- **LLVM** — optional. Without it, CMake prints
  `LLVM not found -- JIT shader path disabled` and the renderer uses the
  bytecode shader interpreter. That is a working configuration, not a broken
  one.
- **FLTK** — optional, only for the `oshow` viewer. Build without it using
  `-DBUILD_SHOW=OFF`.
- **X11** (Linux only) — the framebuffer display helper `orender-fb-linux`
  links against it. macOS uses a native helper and needs no X11.

If configuration fails, read the CMake output: it names each dependency it
could not find.

## Check that the `ORENDERHOME` environment variable is set

`ORENDERHOME` tells openRender where to find its shaders, display drivers and
default configuration. It is the directory `cmake --install` wrote to, and
which one that is depends on how you installed:

- **Self-contained** (`-DINSTALL_SELFCONTAINED=ON`) — everything lives under
  one prefix, and `ORENDERHOME` is that prefix. For example, after
  `cmake --install build --prefix /usr/local/openrender`, set
  `export ORENDERHOME=/usr/local/openrender`.
- **System / FHS layout** (the default) — binaries go to `<prefix>/bin`,
  libraries to `<prefix>/lib`, and the renderer's data to
  `<prefix>/share/openRender`. That last path is `ORENDERHOME`.

You will usually also want `$ORENDERHOME/bin` on your `PATH`, and
`$ORENDERHOME/lib` on `LD_LIBRARY_PATH` (Linux) or `DYLD_LIBRARY_PATH`
(macOS).

To confirm the renderer found its home, run a scene with
`orender -x 3 scene.rib` and look for the message that `.orenderrc` was
loaded.

## Errors about libtiff

libtiff is used for reading textures and writing final images. If you get
complaints about an incompatible version, install a current one through your
platform's package manager — `brew install libtiff` on macOS, your
distribution's `libtiff` development package on Linux — and reconfigure so
CMake picks the new one up.

## Errors about X11 (Linux)

The framebuffer display opens its window through a separate helper process,
`orender-fb-linux`, which links against X11. If a render that uses the
framebuffer fails to show anything, check that `DISPLAY` is set, that X11 is
running, and that `xauth` permissions let you open a window on that display.

This does not apply to macOS, which uses a native helper (`orender-fb-macos`)
and needs no X11 at all. Renders that write to a file rather than the
framebuffer need neither.

## Compiling hangs

`src/libshader/shading/execute.cpp`, and to a lesser extent
`src/ri/stochastic.cpp`, take a long time to compile. This is expected: both
use macros to instantiate the same code under many combinations of options, so
that at render time the executed path is exactly the one needed and no more.

The historical figure quoted here was 20–30 minutes on a single-core G5, which
is no longer a useful yardstick — on current hardware it is minutes, not tens
of minutes, and a parallel build (`cmake --build build -j`) hides most of it.
If a build appears stuck, check whether it is one of these two files before
assuming it has hung.
