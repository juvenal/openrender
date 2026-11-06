# openRender Build Artifacts & Installation Locations

## Build Configuration

- **Default Install Prefix**: `/usr/local`
- **Install Mode**: `INSTALL_SELFCONTAINED=ON` (default)
- **Architecture**: ARM64 (single arch on ARM Macs)

---

## Executables (7 binaries + 1 symlink)

Installed to: `${CMAKE_INSTALL_BINDIR}` → `/usr/local/bin/`

| Binary | Description | Location |
|--------|-------------|----------|
| `orender` | Main renderer executable | `/usr/local/bin/orender` |
| `orender-wire` | Interactive RIB wireframe previewer (macOS: `.app` bundle; Linux: GTK 4 binary) | `/usr/local/bin/orender-wire` |
| `oshader` | Shader compiler | `/usr/local/bin/oshader` |
| `sloinfo` | Unified shader inspector (auto-detects `.slo` / `.rslo` by file magic) | `/usr/local/bin/sloinfo` |
| `rsloinfo` | Symlink to `sloinfo` for backward compatibility | `/usr/local/bin/rsloinfo` → `sloinfo` |
| `otexmake` | Texture creation utility | `/usr/local/bin/otexmake` |
| `precomp` | Preprocessor (not installed by default) | Build only |

---

## Shared Libraries (8 dylibs)

Installed to: `${CMAKE_INSTALL_LIBDIR}` → `/usr/local/lib/`

All shared libraries carry `VERSION` and `SOVERSION` metadata (e.g. `libRIBRender.dylib → libRIBRender.1.dylib → libRIBRender.1.0.0.dylib`). The SOVERSION defaults to the current major version and can be overridden with `-DOPENRENDER_COMPAT_SOVERSION=<n>`.

| Library | Description | Location |
|---------|-------------|----------|
| `libRIBRender.dylib` | RenderMan Interface library — full renderer | `/usr/local/lib/libRIBRender.dylib` |
| `libRIBVector.dylib` | Geometry-only RI extraction (no shading/LLVM), used by `orender-wire` | `/usr/local/lib/libRIBVector.dylib` |
| `libRIBOut.dylib` | RIB text output (`CRibOut`) — re-serializes Ri calls as RIB | `/usr/local/lib/libRIBOut.dylib` |
| `libshader_compiler.dylib` | RSL shader compiler library | `/usr/local/lib/libshader_compiler.dylib` |
| `libshader_runtime.dylib` | `.rslo`/`.slo` shader runtime loader | `/usr/local/lib/libshader_runtime.dylib` |
| `libshader_shading.dylib` | Shading engine (JIT-callable `op_*`/`rsl_*` ops + builtins) | `/usr/local/lib/libshader_shading.dylib` |
| `libRIBShadeop.dylib` | Shader DSO operands (spec 037): module scanning, the version check, the declaration parser and the per-thread state of shadeop modules; the renderer, the shading engine and the shader compiler link it | `/usr/local/lib/libRIBShadeop.dylib` |
| `libRIBCommon.dylib` | Infrastructure every other library uses: diagnostics, the scratch arena, statistics, RI tokens, `CVariable`, math and sampling | `/usr/local/lib/libRIBCommon.dylib` |

`libRIBShadeop` carries the same `VERSION`/`SOVERSION` metadata as the others (`libRIBShadeop.dylib → libRIBShadeop.1.dylib → libRIBShadeop.1.0.0.dylib`); `PluginAbi_ShadeopSoversion` checks that its SOVERSION equals `OPENRENDER_COMPAT_SOVERSION`.

**The plug-in interface versions are independent of `OPENRENDER_COMPAT_SOVERSION`.** Each plug-in family (display, shader DSO operands, procedural DSO, implicit surface) carries its own `MAJOR.MINOR.PATCH` in its header (`dsply.h` is 2.0.0). Adding `libRIBShadeop` and moving the display interface to 2.0.0 changed neither the SOVERSION of any library nor `OPENRENDER_COMPAT_SOVERSION`: a plug-in's MAJOR changes only when that family's binary interface breaks, and a library's SOVERSION only when the library's own ABI does.

`libRIBCommon` is the only image that defines this process-wide state (one diagnostic sink, one arena, one RI token table per process). Since spec 023 it is a shared library; on macOS `libRIBRender`, `libRIBVector` and `libRIBOut` re-export it, so programs linked against an earlier install, which bound `RI_*` tokens and `error()` to those libraries, keep running unchanged. On Linux each RI library lists it in `DT_NEEDED`.

### Compatibility aliases

Each `libRIB*` library also gets a lowercase-named symlink, shared and static, for consumers not yet updated to the new names:

| Alias | Points to |
|-------|-----------|
| `libri.dylib` / `libri.a` | `libRIBRender.dylib` / `libRIBRender.a` |
| `libribVector.dylib` / `libribVector.a` | `libRIBVector.dylib` / `libRIBVector.a` |
| `libribOut.dylib` / `libribOut.a` | `libRIBOut.dylib` / `libRIBOut.a` |

`libribVector`/`libribOut` differ from their targets only in letter case. On a case-insensitive filesystem (the macOS default) the old spelling already resolves to the real file, so those aliases are skipped there (`CMake/createCompatAlias.cmake`); creating them would overwrite the real library with a symlink to itself.

---

## Display Driver Modules (4 plugins)

Installed to: `${OPENRENDER_DISPLAYSDIR}` → `/usr/local/displays/`

Display plugins use the `.dsply` extension on all platforms except Windows (`.dll`).
This makes it unambiguous that these are orender-specific loadable modules, not
general shared libraries.

| Module | Description | Location |
|--------|-------------|----------|
| `framebuffer.dsply` | Interactive framebuffer display driver (IPC-based) | `/usr/local/displays/framebuffer.dsply` |
| `file.dsply` | File output driver (TIFF, PNG) | `/usr/local/displays/file.dsply` |
| `rgbe.dsply` | RGBE (Radiance .pic) format driver | `/usr/local/displays/rgbe.dsply` |
| `openexr.dsply` | OpenEXR display driver (optional) | `/usr/local/displays/openexr.dsply` |

---

## Static Libraries (7 archives)

Installed to: `${CMAKE_INSTALL_LIBDIR}` → `/usr/local/lib/`

Built from the same OBJECT library as the shared variants (one compilation pass, PIC enabled).

Shared internal code (diagnostics, the arena, statistics, RI tokens, `CVariable`, `os`, `algebra`, `rslConstants`, `sobolTables`, `colorSpace`, sampling) lives **only** in `libRIBCommon`. Every other archive depends on it rather than embedding its own copy, so a program linking several of them gets exactly one copy and never sees duplicate symbols. External dependencies: libtiff/libpng/zlib, OpenEXR when enabled, pthreads/dl, and, for `libRIBRender.a`, the shared `libshader_shading` **and the shared `libRIBCommon`**. `libshader_shading` is shared-only and binds its infrastructure to the shared `libRIBCommon`, so a static renderer program links that shared library rather than `libRIBCommon.a`; a second, static copy would split the diagnostic sink, arena and token table in two. The other archives use `libRIBCommon.a`. Use the CMake package (below) rather than writing the link line by hand.

**`libRIBRender.a` links plainly.** `libshader_shading` is self-contained (spec 029): it imports no symbol of the renderer or the hiders, links with undefined symbols as an error, and depends only on `libRIBCommon`, `libRIBShadeop` and LLVM. A static program therefore needs no `-force_load`, `--whole-archive` or `--export-dynamic`, and the CMake package adds none (the target does carry one INTERFACE `-u ribrender_context_anchor` linker option, which keeps the render back-end's registrar in the archive without pulling the whole archive; before spec 029 the library imported 4 symbols from `libRIBRender`, GitHub #24, and the package forced the whole archive to satisfy them). Link `openRender::RIBRender_static` through `find_package(openRender COMPONENTS Static)`; the static component's own dependencies (libtiff and its codecs, OpenEXR) must be findable by the consumer, which on a vcpkg install means configuring the consumer with the same toolchain (GitHub #141).

| Library | Description | Location |
|---------|-------------|----------|
| `libRIBRender.a` | RenderMan Interface (full renderer) static archive | `/usr/local/lib/libRIBRender.a` |
| `libRIBVector.a` | Geometry-only RI extraction static archive | `/usr/local/lib/libRIBVector.a` |
| `libRIBOut.a` | RIB text output static archive | `/usr/local/lib/libRIBOut.a` |
| `libshader_compiler.a` | Shader compiler static archive | `/usr/local/lib/libshader_compiler.a` |
| `libshader_runtime.a` | Shader runtime static archive | `/usr/local/lib/libshader_runtime.a` |
| `libRIBShadeop.a` | Shader DSO operands static archive (only for fully static programs; `libRIBRender.a` uses the shared `libRIBShadeop`) | `/usr/local/lib/libRIBShadeop.a` |
| `libRIBCommon.a` | Infrastructure the archives above depend on (linked once; `libRIBRender.a` uses the shared `libRIBCommon` instead) | `/usr/local/lib/libRIBCommon.a` |

### Using openRender from CMake

The install ships a CMake package in `${CMAKE_INSTALL_LIBDIR}/cmake/openRender/`:

```cmake
find_package(openRender 1.0 CONFIG REQUIRED)                     # shared libraries
target_link_libraries(app PRIVATE openRender::RIBRender)

find_package(openRender 1.0 CONFIG REQUIRED COMPONENTS Static)   # static archives too
target_link_libraries(app PRIVATE openRender::RIBRender_static)  # pulls the shared openRender::RIBCommon, TIFF, PNG, ...
```

| Component | Targets | Needs at `find_package()` time |
|-----------|---------|--------------------------------|
| `Shared` (always) | `openRender::RIBRender`, `RIBVector`, `RIBOut`, `shader_shading`, `shader_runtime`, `shader_compiler`, `RIBShadeop`, `RIBCommon` | nothing; the shared libraries are self-contained |
| `Static` (optional) | `openRender::RIBRender_static`, `RIBVector_static`, `RIBOut_static`, `shader_runtime_static`, `shader_compiler_static`, `RIBShadeop_static`, `RIBCommon_static` | TIFF, PNG, Threads (+ OpenEXR/Imath when built with OpenEXR), and the shared targets: `RIBRender_static` links `shader_shading` and the shared `RIBCommon` |

If the static component's dependencies aren't found, plain `find_package(openRender)` still succeeds with only the shared targets. `COMPONENTS Static` as a requirement fails and names the missing dependency. Point CMake at the install with `-DCMAKE_PREFIX_PATH=<prefix>`.

---

## Development Libraries (1 build-only)

**Not installed** (build artifact only)

| Library | Description |
|---------|-------------|
| `libribpreview.a` | Scene geometry extraction for wireframe preview (`ribpreview_api.h` C API) |

---

## Language Bindings (2 files)

Self-contained install destinations (default):

| File | Language | Install Path |
|------|----------|--------------|
| `prman.py` | Python | `${PREFIX}/python/prman.py` |
| `prman.lua` | Lua | `${PREFIX}/lua/prman.lua` |

FHS install destinations:

| File | Language | Install Path |
|------|----------|--------------|
| `prman.py` | Python | `${PREFIX}/share/openRender/python/prman.py` |
| `prman.lua` | Lua | `${PREFIX}/share/openRender/lua/prman.lua` |

Both destinations can be overridden at configure time with `-DOPENRENDER_PYTHONDIR=<path>` and `-DOPENRENDER_LUADIR=<path>`.

---

## Header Files

Installed to the include directory of the install layout (`OPENRENDER_INCLUDE_INSTALL_DIR`):

| Layout | Include directory | With the default prefix |
|--------|-------------------|-------------------------|
| self-contained (`INSTALL_SELFCONTAINED=ON`, default) | `${CMAKE_INSTALL_INCLUDEDIR}` under the prefix, i.e. `<ORENDERHOME>/include` | `/usr/local/include/` |
| FHS (`INSTALL_SELFCONTAINED=OFF`) | `${CMAKE_INSTALL_INCLUDEDIR}/openRender` | `/usr/local/include/openRender/` |

The installed set is exactly `OPENRENDER_PUBLIC_HEADERS` (`CMake/openRenderPublicHeaders.cmake`), installed by `src/ri/CMakeLists.txt`; nothing else is installed there (`PluginAbi_InstallList`):
- `dlo.h` - Procedural DSO interface
- `dsply.h` - Display driver interface (version 2.0.0)
- `implicit.h` - Implicit surface plug-in interface
- `orplugin.h` - Common plug-in ABI header (status codes, version macros, function-table header); included by `dsply.h`, `shadeop.h`, `dlo.h` and `implicit.h`
- `orplugin.hpp` - C++20 convenience over `orplugin.h` (`orplugin::makeTable`); optional, a plug-in can use `orplugin.h` alone
- `ptcapi.h` - Point cloud API
- `ri.h` - RenderMan Interface main header
- `rslo.h` - `.rslo`/`.slo` shader loader API (`libshader_runtime`)
- `shadeop.h` - Shader DSO operand interface

Headers are included by file name alone (`#include "dsply.h"`). The CMake package (`find_package(openRender)`) puts the layout's include directory on every `openRender::` target and sets `openRender_INCLUDE_DIR`, so a consumer needs no extra flag. `examples/display/simpledisplay.c` is a display driver that builds against this directory alone.

---

## Shader Files

Installed to: `${OPENRENDER_SHADERDIR}` → `/usr/local/shaders/`

Includes:
- `.sl` files - Shader source files (RenderMan Shading Language)
- `includes/*.slh` - Shader headers
- `.rslo` / `.slo` files - Compiled shader files, built by `shaders/CMakeLists.txt` as a build step (`.slo` = LLVM bitcode, only when the JIT is enabled)

Each group is installed in one go and reported with a single summary line instead of one `-- Installing:` line per file (every file is still recorded in `install_manifest.txt`):

```
-- Installing: 104 shader sources (.sl) -> /usr/local/shaders
-- Installing: 20 shader headers (includes/) -> /usr/local/shaders/includes
-- Installing: 104 compiled shaders (.rslo) -> /usr/local/shaders
-- Installing: 104 compiled shaders (.slo, JIT) -> /usr/local/shaders
```

Examples:
- Surface shaders: `plastic.sl`, `matte.sl`, `metal.sl`, etc.
- Light shaders: `pointlight.sl`, `distantlight.sl`, etc.
- Volume shaders, displacement shaders, etc.

---

## Man Pages (6 files)

Installed to: `${CMAKE_INSTALL_MANDIR}/man1` → `/usr/local/share/man/man1/`

| Man Page | Command |
|----------|---------|
| `orender.1` | orender(1) |
| `orender-wire.1` | orender-wire(1) |
| `oshader.1` | oshader(1) |
| `rsloinfo.1` | rsloinfo(1) |
| `sloinfo.1` | sloinfo(1) |
| `otexmake.1` | otexmake(1) |

---

## Documentation Files (8 files)

Installed to: `${OPENRENDER_DOCDIR}` → `/usr/local/share/doc/`

| File | Description |
|------|-------------|
| `AUTHORS.md` | Project authors |
| `ChangeLog.md` | Change history |
| `COPYING.md` | License pointer (see LICENSE.md) |
| `DEVNOTES.md` | Developer notes |
| `INSTALL.md` | Installation instructions |
| `LICENSE.md` | License (LGPL-2.1) |
| `NEWS.md` | News and announcements |
| `README.md` | Project readme |

---

## Installation Directory Structure

```text
/usr/local/
├── bin/
│   ├── orender
│   ├── orender-wire
│   ├── oshader
│   ├── sloinfo
│   ├── rsloinfo -> sloinfo
│   └── otexmake
├── lib/
│   ├── libRIBRender.dylib   # shared (versioned: libRIBRender.1.dylib)
│   ├── libRIBRender.a       # static
│   ├── libRIBVector.dylib   # shared (versioned: libRIBVector.1.dylib)
│   ├── libRIBVector.a       # static
│   ├── libRIBOut.dylib      # shared (versioned: libRIBOut.1.dylib)
│   ├── libRIBOut.a          # static
│   ├── libRIBCommon.dylib   # shared infrastructure (versioned: libRIBCommon.1.dylib)
│   ├── libRIBShadeop.dylib  # shared shader DSO operands (versioned: libRIBShadeop.1.dylib)
│   ├── libRIBShadeop.a      # static shader DSO operands
│   ├── libRIBCommon.a       # static infrastructure (static archives depend on it)
│   ├── cmake/openRender/    # CMake package (find_package(openRender))
│   ├── libri.{dylib,a}      # compat aliases -> libRIBRender
│   ├── libribVector.{dylib,a}  # compat aliases (case-sensitive volumes only)
│   ├── libribOut.{dylib,a}  # compat aliases (case-sensitive volumes only)
│   └── libshader_{compiler,runtime,shading}.*
├── include/             # self-contained layout; the FHS layout uses include/openRender/ instead
│   ├── dlo.h
│   ├── dsply.h
│   ├── implicit.h
│   ├── orplugin.h
│   ├── orplugin.hpp
│   ├── ptcapi.h
│   ├── ri.h
│   ├── rslo.h
│   └── shadeop.h
├── displays/            # Display driver modules (.dsply; .dll on Windows)
│   ├── framebuffer.dsply
│   ├── file.dsply
│   ├── rgbe.dsply
│   └── openexr.dsply
├── shaders/             # Shader files (54 files)
│   ├── *.sl
│   └── *.rslo / *.slo
├── python/
│   └── prman.py         # Python RenderMan binding
├── lua/
│   └── prman.lua        # Lua RenderMan binding
├── share/
│   ├── man/man1/        # Man pages
│   │   ├── orender.1
│   │   ├── orender-wire.1
│   │   ├── oshader.1
│   │   ├── rsloinfo.1
│   │   ├── sloinfo.1
│   │   └── otexmake.1
│   └── doc/             # Documentation
│       ├── AUTHORS.md
│       ├── ChangeLog.md
│       ├── COPYING.md
│       ├── DEVNOTES.md
│       ├── INSTALL.md
│       ├── LICENSE.md
│       ├── NEWS.md
│       └── README.md
```

---

## For Homebrew Formula

When creating the Homebrew formula, you should verify these files are installed:

### Required Executables
```ruby
test do
  system "#{bin}/orender", "--help"
  system "#{bin}/oshader", "--version"
  system "#{bin}/otexmake", "--help"
  system "#{bin}/rsloinfo", "--version"
end
```

### Required Libraries
```ruby
test do
  assert_predicate lib/"libRIBRender.dylib", :exist?
  assert_predicate lib/"libRIBOut.dylib", :exist?
end
```

### Display Drivers
```ruby
test do
  assert_predicate lib/"displays/file.dsply", :exist?
  assert_predicate lib/"displays/framebuffer.dsply", :exist?
  assert_predicate lib/"displays/rgbe.dsply", :exist?
  # openexr.dsply is optional
end
```

### Headers
```ruby
test do
  assert_predicate include/"ri.h", :exist?
end
```

### Man Pages
```ruby
test do
  assert_predicate man1/"orender.1", :exist?
end
```

---

## Installation Path Variables

When `INSTALL_SELFCONTAINED=OFF` (system installation):

- `CMAKE_INSTALL_BINDIR` = `/usr/local/bin`
- `CMAKE_INSTALL_LIBDIR` = `/usr/local/lib`
- `CMAKE_INSTALL_INCLUDEDIR` = `/usr/local/include`
- `CMAKE_INSTALL_MANDIR` = `/usr/local/share/man`
- `CMAKE_INSTALL_DATAROOTDIR` = `/usr/local/share`
- `OPENRENDER_DISPLAYSDIR` = `/usr/local/lib/openRender/displays`
- `OPENRENDER_SHADERDIR` = `/usr/local/share/openRender/shaders`
- `OPENRENDER_PYTHONDIR` = `/usr/local/share/openRender/python`
- `OPENRENDER_LUADIR` = `/usr/local/share/openRender/lua`

When `INSTALL_SELFCONTAINED=ON` (default, self-contained):

- All files under `CMAKE_INSTALL_PREFIX`
- `OPENRENDER_DISPLAYSDIR` = `${PREFIX}/displays`
- `OPENRENDER_SHADERDIR` = `${PREFIX}/shaders`
- `OPENRENDER_PYTHONDIR` = `${PREFIX}/python`
- `OPENRENDER_LUADIR` = `${PREFIX}/lua`

---

## Environment Variables

| Variable | Values | Description |
|----------|--------|-------------|
| `ORENDERHOME` | directory path | Base directory for openRender resources |
| `SHADERS` | directory path | Shader search path (overrides `ORENDERHOME/shaders`) |
| `DISPLAYS` | directory path | Display driver search path |
| `GEOMETRIES` | directory path | Geometry plugin search path |
| `ORENDER_INSTR_LEVEL` | `debug`, `info`, `warn`, `error` | Minimum log verbosity level |
| `ORENDER_INSTR_OUTPUT` | `stderr`, `stdout`, or file path | Log output destination |
| `OPENRENDER_DEFAULT_FORMAT` | `rslo`, `slo` | Default shader format when not overridden by RIB |
| `OPENRENDER_DUMP_JIT_IR` | any non-empty | Print JIT IR after each optimization pass to stderr |

---

## Total Artifact Count

| Category | Count |
|----------|-------|
| Executables | 7 (6 installed + 1 build-only) + 1 symlink (rsloinfo → sloinfo) |
| Shared Libraries | 2 (installed) |
| Static Libraries | 2 (installed) + 4 (build-only) |
| Display Modules | 4 |
| Header Files | 9 |
| Language Binding Files | 2 |
| Shader Files | 54 |
| Man Pages | 6 |
| Documentation Files | 8 |
| **Total Installed Files** | **94** |
