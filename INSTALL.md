# Installation Instructions

This document describes how to build and install openRender using CMake.

## Prerequisites

- **C++20** compiler with `<format>` and `<source_location>`:
  **GCC 13 or newer**, or Clang 17+ with libc++, or Clang against libstdc++ 13+.
  `src/include/logging.hpp` uses `std::format`, and libstdc++ only gained
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

### macOS: Homebrew vs. vcpkg for libtiff/libpng/zlib/OpenEXR

By default these come from Homebrew. Homebrew's bottles are prebuilt for
whatever macOS SDK Homebrew's own CI happened to run on, which can be newer
than this project's `CMAKE_OSX_DEPLOYMENT_TARGET` (13.3) — usually harmless
for local development, but not something a distributable binary should rely
on (see the release workflow, which vendors these instead).

To use the same vendored dependencies locally — recommended if you maintain
multiple worktrees and want one shared, version-pinned copy instead of
depending on whatever Homebrew currently has installed:

```bash
scripts/setup-vcpkg-macos.sh
```

This clones and bootstraps a shared vcpkg checkout (default `~/.vcpkg`,
pinned to the same commit as `vcpkg.json`) and prints two `export` lines to
add to your shell profile yourself:

```bash
export VCPKG_ROOT=~/.vcpkg
export VCPKG_INSTALLED_DIR=~/.cache/vcpkg-installed/openrender
```

With those set, `cmake -B build -S .` picks up vcpkg automatically — no
extra flags — and every worktree pointed at the same `VCPKG_INSTALLED_DIR`
shares one compiled tree (~13MB) rather than each paying for its own.
Leaving `VCPKG_ROOT` unset keeps using Homebrew exactly as before; nothing
else about the build changes either way.

**Switching modes on an existing build directory doesn't work — delete it
first.** `CMAKE_TOOLCHAIN_FILE` is only honored on a build directory's first
configure; setting or unsetting `VCPKG_ROOT` and re-running `cmake -B build`
on the same directory leaves it silently linked against whichever backend it
started with. Run `rm -rf build` (or use a fresh build directory) whenever
you flip `VCPKG_ROOT` for a worktree that already has one configured.

Optional, each enabling a component:

- **LLVM** 15 or newer: the JIT (`.slo`) shader backend. Skip with
  `-DOPENRENDER_ENABLE_JIT=OFF`.
- **OpenEXR** (2.5+ or 3.x) — with **Imath** for 3.x, or **IlmBase** for 2.x:
  the EXR display driver (<http://www.openexr.com>)
- **X11**: required by the `orender-fb-linux` framebuffer helper.
  **Wayland** + **wayland-protocols** + **libdecor** are used when present.
- **GTK 4** (4.10+) and **libadwaita** (1.4+): the `orender-wire` previewer on
  Linux. macOS uses Metal/AppKit and needs neither.

## Optional: OpenImageIO (`OPENRENDER_ENABLE_OIIO`)

Off by default; nothing about a default build changes. With `-DOPENRENDER_ENABLE_OIIO=ON` the build adds `libRIBImageIO` (the only image that carries
OpenImageIO), the `oiio` display driver and the `Option "imageio"` / `-imageio oiio` selection (see `DEVNOTES_DETAILS/IMAGE_IO.md`). The minimum is
OpenImageIO 3.0 (3.1.14 was validated). Continuous integration, the FHS build and the release builds do not bundle OpenImageIO in this release.

- **macOS, vcpkg** (`VCPKG_ROOT` set): the `oiio` manifest feature builds OpenImageIO into its **own install directory**, `<VCPKG_INSTALLED_DIR>-oiio`
  (or `vcpkg_installed-oiio` next to the source tree), never into the shared tree the OIIO-off builds use, because manifest mode would uninstall the
  packages those builds need. The configure refuses the shared tree. The closure is large (OpenColorIO 2.3, OpenEXR, Imath, ...): budget for it.
- **macOS, Homebrew** (no `VCPKG_ROOT`): `brew install openimageio` (3.2.x as of this writing; a large closure) and configure with the option. **Untested:** spec 048 never configured or built this mode (it validated vcpkg static OpenImageIO on macOS and a shared prefix build on Linux), so treat it as unverified.
- **Linux**: Ubuntu 24.04's OpenImageIO is 2.4.17, below the minimum. Build OpenImageIO 3.1.14 into a prefix and point the configure at it:
  `cmake -S <oiio-src> -B <oiio-build> -DCMAKE_INSTALL_PREFIX=$HOME/oiio-prefix -DOpenImageIO_BUILD_MISSING_DEPS=all`, then build and install it, and configure
  openRender with `-DOPENRENDER_ENABLE_OIIO=ON -DOpenImageIO_ROOT=$HOME/oiio-prefix` (needs OpenColorIO 2.3, which `BUILD_MISSING_DEPS=all` builds). **pystring
  pin:** OIIO 3.1.14.0 pins pystring to the branch `master` plus commit `381829c2c1696ffec9277b339952a9588e6e67cf`, and upstream `master` has moved, so the
  first configure stops at the tag check (`pystring: Tag master resolved to commit ..., but expected ...`). Do not set
  `OpenImageIO_DEPENDENCY_BUILD_ALLOW_UNVERIFIED_TAGS=ON`: in `<oiio-build>/deps/pystring` run `git fetch --unshallow` and
  `git checkout -B master 381829c2c1696ffec9277b339952a9588e6e67cf`, then configure again (the verification still runs and passes). The runtime finds the
  prefix through the build and install RPATH.

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
| `OPENRENDER_ENABLE_OIIO` | Build the optional OpenImageIO image backend and the `oiio` display driver (needs OpenImageIO 3.0 or newer) | OFF |

Example:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DINSTALL_SELFCONTAINED=OFF
```

## Installation Layout

With `INSTALL_SELFCONTAINED=ON` (default), documentation, shaders, and other data are installed under `CMAKE_INSTALL_PREFIX/share/doc`, `CMAKE_INSTALL_PREFIX/.../shaders`, etc. Executables go to `bin/`, libraries to `lib/`, and headers to `include/`.

The libraries are `libRIBRender`, `libRIBVector` and `libRIBOut` (the RenderMan Interface), `libRSLRuntime` (the shading engine and the shader loaders), `libRSLInfo` (shader metadata and the `.rslo` inspection reader) and `libshader_compiler` (the only `libshader_*` left), and `libRIBCommon`, the shared infrastructure library every other one depends on. Each is installed shared and, except `libRSLRuntime`, static. The CMake package's optional `Static` component still needs the shared targets: the static renderer links the shared `libRSLRuntime` and `libRIBCommon`. The old names `libshader_shading` and `libshader_runtime` resolve for one release (compatibility links, CMake `ALIAS` targets and exported aliases); a binary linked against `libshader_shading.1.dylib`/`.so.1` or `libshader_runtime.1.dylib`/`.so.1` must be relinked.

See [INSTALL_ARTIFACTS.md](INSTALL_ARTIFACTS.md) for a detailed list of installed files.
