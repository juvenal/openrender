# Installation Instructions

This document describes how to build and install openRender using CMake.

## Prerequisites

- **C++20** compiler with `<format>` and `<source_location>`:
  **GCC 13 or newer**, or Clang 17+ with libc++, or Clang against libstdc++ 13+.
  `src/includes/logging.hpp` uses `std::format`, and libstdc++ only gained
  `<format>` in GCC 13 — GCC 11 and 12 cannot build this tree, which is why
  Ubuntu 22.04's stock toolchain does not work.
- **CMake** 3.19 or higher
- **Git** (for cloning the repository)

Supported baseline is **Ubuntu 24.04 LTS** (GCC 13, CMake 3.28, LLVM 18,
OpenEXR 3.1). See
[Installing / running openRender](https://juvenal.github.io/openrender/manual/reference/installing-and-running/#linux)
for a copy-pasteable `apt-get` line.

## Dependencies

Required:

- **libtiff**: Image format support (<http://www.libtiff.org>)
- **libpng**: PNG image support
- **zlib**
- **flex / bison**: Parser generation — **mandatory**; no pre-generated parser
  sources are kept in the repository. On macOS you need Homebrew's bison, as
  the system one is 2.3; the system flex is fine.

Optional, each enabling a component:

- **LLVM** 15 or newer: the JIT (`.slo`) shader backend. Skip with
  `-DOPENRENDER_ENABLE_JIT=OFF`.
- **OpenEXR** (2.5+ or 3.x) — with **Imath** for 3.x, or **IlmBase** for 2.x:
  the EXR display driver (<http://www.openexr.com>)
- **X11**: required by the `orender-fb-linux` framebuffer helper.
  **Wayland** + **wayland-protocols** + **libdecor** are used when present.
- **GTK 4** (4.10+) and **libadwaita** (1.4+): the `orender-wire` previewer on
  Linux. macOS uses Metal/AppKit and needs neither.

## Building

### Unix / Linux / macOS

1. Clone the repository and enter the source directory:

   ```bash
   git clone https://github.com/juvenal/openrender.git
   cd openrender
   ```

2. Create a build directory and configure with CMake:

   ```bash
   mkdir build && cd build
   cmake .. -DCMAKE_BUILD_TYPE=Release
   ```

3. Compile (use multiple cores if available):

   ```bash
   make -j$(nproc 2>/dev/null || sysctl -n hw.ncpu 2>/dev/null || echo 4)
   ```

4. Run tests (optional):

   ```bash
   ctest
   ```

5. Install:

   ```bash
   sudo make install
   ```

   By default, files are installed under `/usr/local`. To use a different prefix:

   ```bash
   cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/opt/openrender
   make -j$(nproc)
   sudo make install
   ```

### Windows

1. Open a command prompt or PowerShell, then:

   ```cmd
   git clone https://github.com/juvenal/openrender.git
   cd openrender
   mkdir build
   cd build
   ```

2. Configure (example for Visual Studio 2019, 64-bit):

   ```cmd
   cmake .. -G "Visual Studio 16 2019" -A x64 -DCMAKE_BUILD_TYPE=Release
   ```

3. Build:

   ```cmd
   cmake --build . --config Release
   ```

4. Install (optional; may require elevated permissions):

   ```cmd
   cmake --install . --config Release
   ```

   To install to a custom directory:

   ```cmd
   cmake .. -G "Visual Studio 16 2019" -A x64 -DCMAKE_INSTALL_PREFIX=C:\openrender
   cmake --build . --config Release
   cmake --install . --config Release
   ```

## CMake Options

| Option | Description | Default |
|--------|-------------|---------|
| `USE_FLEX_BISON` | Use flex and bison to regenerate parsers | ON |
| `INSTALL_SELFCONTAINED` | Self-contained install under prefix (vs FHS) | ON |
| `OPENRENDER_COMPAT_SOVERSION` | SOVERSION for libri/librslo shared libraries | Major version |
| `OPENRENDER_PYTHONDIR` | Install destination for `prman.py` | `python/` (self-contained) or `share/openRender/python/` (FHS) |
| `OPENRENDER_LUADIR` | Install destination for `prman.lua` | `lua/` (self-contained) or `share/openRender/lua/` (FHS) |

Example:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DINSTALL_SELFCONTAINED=OFF
```

## Installation Layout

With `INSTALL_SELFCONTAINED=ON` (default), documentation, shaders, and other data are installed under `CMAKE_INSTALL_PREFIX/share/doc`, `CMAKE_INSTALL_PREFIX/.../shaders`, etc. Executables go to `bin/`, libraries to `lib/`, and headers to `include/`.

See [INSTALL_ARTIFACTS.md](INSTALL_ARTIFACTS.md) for a detailed list of installed files.
