# Installation Instructions

This document describes how to build and install openRender using CMake.

## Prerequisites

- **C++20** compliant compiler (GCC 10+, Clang 10+, or MSVC 2019+)
- **CMake** 3.15 or higher
- **Git** (for cloning the repository)

## Dependencies

- **libtiff**: Image format support (<http://www.libtiff.org>)
- **libpng**: PNG image support
- **flex / bison**: Parser generation (available on Unix platforms by default)
- **fltk**: GUI support for the interactive viewer `oshow` (<http://www.fltk.org>)
- **OpenEXR**: High dynamic range image support (<http://www.openexr.com>) — optional

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
| `BUILD_SHOW` | Build the interactive viewer (oshow) | ON |
| `INSTALL_SELFCONTAINED` | Self-contained install under prefix (vs FHS) | ON |

Example:

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DBUILD_SHOW=OFF
```

## Installation Layout

With `INSTALL_SELFCONTAINED=ON` (default), documentation, shaders, and other data are installed under `CMAKE_INSTALL_PREFIX/share/doc`, `CMAKE_INSTALL_PREFIX/.../shaders`, etc. Executables go to `bin/`, libraries to `lib/`, and headers to `include/`.

See [INSTALL_ARTIFACTS.md](INSTALL_ARTIFACTS.md) for a detailed list of installed files.
