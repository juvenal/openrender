---
title: "Installing And Running"
date: 2026-08-29
---

# Installing And Running

**This tutorial shows what you need to do to render images with openRender.**

openRender is a photorealistic renderer which communicates with modelers or your application through a RenderMan - like interface. openRender is not a modeler or an animation system. So it does not have any graphical user interface.

The scenes you want to render are described in a text file in a language very similar to Pixar's RenderMan. openRender also comes as a C/C++ library which you can link against your application. In order to find the details of this interface, you should read RenderMan Companion or RenderMan interface on [http://www.pixar.com](http://www.pixar.com).

There is no binary distribution. openRender is built from source, with CMake,
from the repository at
[github.com/juvenal/openrender](https://github.com/juvenal/openrender).

## Prerequisites

Required:

- A **C++20** compiler — GCC 10+, Clang 10+, or MSVC 2019+
- **CMake** 3.16 or newer
- **libtiff**, **libpng**, **zlib**
- **flex** and **bison**, to regenerate the RIB and shading-language parsers.
  Turn this off with `-DUSE_FLEX_BISON=OFF` if you want to use the generated
  sources shipped in the tree instead.

Optional, each enabling a component:

- **LLVM** — the JIT shading backend (`oshader --jit`, `.slo` shaders). If
  CMake does not find LLVM it prints `LLVM not found -- JIT shader path
  disabled` and builds the bytecode interpreter only. Everything still works;
  shaders just run interpreted.
- **OpenEXR** and **Imath** — the OpenEXR display driver
- **FLTK** — the `oshow` viewer. Turn it off with `-DBUILD_SHOW=OFF`.
- **GTK 4** (4.20 or newer) — the `orender-wire` scene previewer on Linux. On
  macOS the previewer uses Metal and AppKit, which need no extra packages.

## Building

The same three commands on every platform:

```bash
git clone https://github.com/juvenal/openrender.git
cd openrender
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Then, optionally, run the test suite and install:

```bash
ctest --test-dir build
cmake --install build --prefix /usr/local/openrender
```

The prefix you install to becomes your `ORENDERHOME` — see
[Common instructions](#common-instructions-for-using-openrender) below.

### macOS

Dependencies come from [Homebrew](https://brew.sh):

```bash
brew install cmake libtiff libpng bison
brew install llvm openexr fltk        # optional components
```

Two notes specific to macOS, both already handled by the build system:

- **You do not need `brew install flex`.** The system flex at `/usr/bin/flex`
  is current enough and is what CMake picks up.
- **You do need Homebrew's bison.** macOS ships bison 2.3, which is far too
  old for this grammar. Homebrew's bison is keg-only, so it is deliberately
  not on your `PATH` — but `CMake/openRenderUseBison.cmake` looks for it at
  `/opt/homebrew/opt/bison/bin` and `/usr/local/opt/bison/bin` directly, so
  no `PATH` juggling is needed. The same applies to Homebrew's keg-only LLVM,
  which CMake falls back to at `/opt/homebrew/opt/llvm`.

If you keep Homebrew somewhere unusual, point CMake at it with
`-DCMAKE_PREFIX_PATH=<your-brew-prefix>`.

### Linux

Install your distribution's development packages for libtiff, libpng, zlib,
flex and bison, plus any of the optional components you want — LLVM, OpenEXR
with Imath, FLTK, and GTK 4 for the previewer. Package names differ between
distributions; the CMake configure step names anything it cannot find.

### Windows

Generate a Visual Studio project with CMake and build that — `INSTALL.md` in
the repository has the exact invocation for VS 2019 and newer. Note that the
routine development and test work happens on macOS and Linux, so the Windows
path is the least exercised of the three.

## Common instructions for using openRender

You must set **ORENDERHOME** (the render root directory) to the location of your installation. At install time this corresponds to the CMake install destination: for a self-contained install it is the same as the install prefix (e.g. `/usr/local` or `/usr/local/openrender`); for a system/FHS install it is typically `share/openRender` under the prefix (e.g. `/usr/local/share/openRender`). You may also want to add the `bin` directory to your `PATH` and the `lib` directory to `LD_LIBRARY_PATH` on Unix or `DYLD_LIBRARY_PATH` on macOS.

For Windows:

- Open Control Panel -> System -> Advanced system settings -> Environment Variables
- Hit New
- Variable name: ORENDERHOME
- Variable value: <path-to-openrender-install>

For bash:

- export ORENDERHOME=<path-to-openrender-install>
- export PATH=$PATH:$ORENDERHOME/bin
- export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:$ORENDERHOME/lib

You can add these lines in your ".profile" script in your home directory (~/.profile).

For tcsh:

- setenv ORENDERHOME <path-to-openrender-install>
- set path=($path $ORENDERHOME/bin)
- setenv LD_LIBRARY_PATH $LD_LIBRARY_PATH:$ORENDERHOME/lib

You can add these lines in your ".tcshrc" script in your home directory (~/.tcshrc).

### ORENDERHOME and default configuration (`.orenderrc`)

The renderer looks for a default configuration file at **`$ORENDERHOME/.orenderrc`**. When you run `make install` (or the equivalent), the project’s `.orenderrc` is installed into the ORENDERHOME destination (the same directory that holds shaders, ribs, etc. for self-contained installs, or `share/openRender` for system installs). You can override the render root at runtime by setting `ORENDERHOME` to a different directory; the renderer will then look for `.orenderrc` at `$ORENDERHOME/.orenderrc`. The file contains pure RIB and is parsed before your scene RIBs, so you can set default options (e.g. `Format`, `PixelSamples`, search paths) there.

To verify the installed configuration: install to a test prefix, set `ORENDERHOME` to that directory, and run `orender` on a simple RIB; with log level INFO (e.g. `orender -x 3 scene.rib`) you should see a message that `.orenderrc` was loaded.

You can also specify search paths for various external resources that openRender may need using:

```
Option "searchpath" "..." "..."
```

Your binary distribution should have the following structure:

|   | `openrender/` (or install prefix) |   |   | Set **ORENDERHOME** to this directory |
|---|---|---|---|---|
|   |   | `bin/` |   | The executables are here. |
|   |   |   | `orender` | The RIB renderer. |
|   |   |   | `oshader` | Shading language compiler |
|   |   |   | `rsloinfo` | Get information about a compiled shader |
|   |   |   | `otexmake` | Texture preparation tool. |
|   |   |   | `oshow` | A viewer for photon maps/irradiance caches etc.. |
|   |   | `include/` |   | The header files |
|   |   | `lib/` |   | The library files. |
|   |   | `displays/` |   | The display drivers. |
|   |   | `tutorials/` |   | The openRender tutorials/examples |
|   |   | `doc/` |   | Documentation for openRender |
|   |   | `.orenderrc` |   | Default renderer config (pure RIB); loaded at `RiBegin` when `ORENDERHOME` is set |

## Compiling Shaders

openRender uses the `oshader` compiler to compile RenderMan Shading Language (.sl) files into compiled object files (.rslo).

To compile a shader:
```bash
oshader my_shader.sl
```
This will produce `my_shader.rslo`.

### JIT-compiled shaders

If openRender was built with LLVM, `oshader --jit` compiles a shader to LLVM
bitcode instead, producing a `.slo`:

```bash
oshader --jit -o my_shader.slo my_shader.sl
```

Both formats are loadable at render time, and which one a primitive uses is
selected by `Attribute "shade" "shaderformat"` for one primitive, or
`Option "shaderformat" "default"` for the whole scene.

Note that nothing in the build regenerates `.slo` files: they are compiled
artefacts, and a `.slo` built by an older `oshader` will not be refreshed by
rebuilding. If you change a shader, recompile it explicitly.

### Legacy Compatibility
For backward compatibility, the renderer and tools also support the older `.sdr` extension. If you need to force the compiler to output `.sdr` files, use the `--legacy-sdr` flag:
```bash
oshader --legacy-sdr my_shader.sl
```

The renderer (`orender`) and shader info tool (`rsloinfo`) will automatically search for `.rslo` files first, and fall back to `.sdr` if the newer format is not found.