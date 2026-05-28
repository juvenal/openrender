# openRender Build Artifacts & Installation Locations

## Build Configuration

- **Default Install Prefix**: `/usr/local`
- **Install Mode**: `INSTALL_SELFCONTAINED=ON` (default)
- **Architecture**: ARM64 (single arch on ARM Macs)

---

## Executables (7 binaries)

Installed to: `${CMAKE_INSTALL_BINDIR}` → `/usr/local/bin/`

| Binary | Description | Location |
|--------|-------------|----------|
| `orender` | Main renderer executable | `/usr/local/bin/orender` |
| `orender-wire` | Interactive RIB wireframe previewer (macOS: `.app` bundle; Linux: GTK 4 binary) | `/usr/local/bin/orender-wire` |
| `oshader` | Shader compiler | `/usr/local/bin/oshader` |
| `rsloinfo` | Shader information utility | `/usr/local/bin/rsloinfo` |
| `otexmake` | Texture creation utility | `/usr/local/bin/otexmake` |
| `oshow` | GUI viewer (FLTK-based, optional) | `/usr/local/bin/oshow` |
| `precomp` | Preprocessor (not installed by default) | Build only |

---

## Shared Libraries (2 dylibs)

Installed to: `${CMAKE_INSTALL_LIBDIR}` → `/usr/local/lib/`

Both libraries carry `VERSION` and `SOVERSION` metadata (e.g. `libri.dylib → libri.1.dylib`). The SOVERSION defaults to the current major version and can be overridden with `-DOPENRENDER_COMPAT_SOVERSION=<n>`.

| Library | Description | Location |
|---------|-------------|----------|
| `libri.dylib` | RenderMan Interface library (core) | `/usr/local/lib/libri.dylib` |
| `librslo.dylib` | Shader runtime library | `/usr/local/lib/librslo.dylib` |

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

## Static Libraries (2 archives)

Installed to: `${CMAKE_INSTALL_LIBDIR}` → `/usr/local/lib/`

Built from the same OBJECT library as the shared variants (one compilation pass, PIC enabled).

| Library | Description | Location |
|---------|-------------|----------|
| `libri.a` | RenderMan Interface static archive | `/usr/local/lib/libri.a` |
| `librslo.a` | Shader runtime static archive | `/usr/local/lib/librslo.a` |

---

## Development Libraries (2 build-only)

**Not installed** (build artifact only)

| Library | Description |
|---------|-------------|
| `libopenrendercommon.a` | Common utilities (object code embedded in libri/librslo; not installed separately) |
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

Installed to: `${CMAKE_INSTALL_INCLUDEDIR}` → `/usr/local/include/`

From `src/ri/CMakeLists.txt`:
- `dlo.h` - Dynamic loading operations
- `dsply.h` - Display interface
- `implicit.h` - Implicit surfaces
- `ptcapi.h` - Point cloud API
- `ri.h` - RenderMan Interface main header
- `shadeop.h` - Shader operations

From `src/file/CMakeLists.txt`:
- `file_base.h` - Base class for file-format display plugins (`CFileOutputBase`)

---

## Shader Files (54 files)

Installed to: `${OPENRENDER_SHADERDIR}` → `/usr/local/shaders/`

Includes:
- `.sl` files - Shader source files (RenderMan Shading Language)
- `.sdr` / `.rslo` files - Compiled shader files

Examples:
- Surface shaders: `plastic.sl`, `matte.sl`, `metal.sl`, etc.
- Light shaders: `pointlight.sl`, `distantlight.sl`, etc.
- Volume shaders, displacement shaders, etc.

---

## Man Pages (5 files)

Installed to: `${CMAKE_INSTALL_MANDIR}/man1` → `/usr/local/share/man/man1/`

| Man Page | Command |
|----------|---------|
| `orender.1` | orender(1) |
| `orender-wire.1` | orender-wire(1) |
| `oshader.1` | oshader(1) |
| `rsloinfo.1` | rsloinfo(1) |
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
│   ├── rsloinfo
│   ├── otexmake
│   └── oshow
├── lib/
│   ├── libri.dylib          # shared (versioned: libri.1.dylib)
│   ├── libri.a              # static
│   ├── librslo.dylib        # shared (versioned: librslo.1.dylib)
│   └── librslo.a            # static
├── include/
│   ├── dlo.h
│   ├── dsply.h
│   ├── implicit.h
│   ├── ptcapi.h
│   ├── ri.h
│   └── shadeop.h
├── displays/            # Display driver modules (.dsply; .dll on Windows)
│   ├── framebuffer.dsply
│   ├── file.dsply
│   ├── rgbe.dsply
│   └── openexr.dsply
├── shaders/             # Shader files (54 files)
│   ├── *.sl
│   └── *.sdr / *.rslo
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
  # oshow is optional (requires fltk)
end
```

### Required Libraries
```ruby
test do
  assert_predicate lib/"libri.dylib", :exist?
  assert_predicate lib/"librslo.dylib", :exist?
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

## Total Artifact Count

| Category | Count |
|----------|-------|
| Executables | 7 (6 installed + 1 build-only) |
| Shared Libraries | 2 (installed) |
| Static Libraries | 2 (installed) + 2 (build-only) |
| Display Modules | 4 |
| Header Files | 6 |
| Language Binding Files | 2 |
| Shader Files | 54 |
| Man Pages | 5 |
| Documentation Files | 8 |
| **Total Installed Files** | **89** |
