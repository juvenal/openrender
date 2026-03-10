# openRender Build Artifacts & Installation Locations

## Build Configuration

- **Default Install Prefix**: `/usr/local`
- **Install Mode**: `INSTALL_SELFCONTAINED=ON` (default)
- **Architecture**: ARM64 (single arch on ARM Macs)

---

## Executables (6 binaries)

Installed to: `${CMAKE_INSTALL_BINDIR}` → `/usr/local/bin/`

| Binary | Description | Location |
|--------|-------------|----------|
| `orender` | Main renderer executable | `/usr/local/bin/orender` |
| `oshader` | Shader compiler | `/usr/local/bin/oshader` |
| `sdrinfo` | Shader information utility | `/usr/local/bin/sdrinfo` |
| `otexmake` | Texture creation utility | `/usr/local/bin/otexmake` |
| `oshow` | GUI viewer (FLTK-based, optional) | `/usr/local/bin/oshow` |
| `precomp` | Preprocessor (not installed by default) | Build only |

---

## Shared Libraries (2 dylibs)

Installed to: `${CMAKE_INSTALL_LIBDIR}` → `/usr/local/lib/`

| Library | Description | Location |
|---------|-------------|----------|
| `libri.dylib` | RenderMan Interface library (core) | `/usr/local/lib/libri.dylib` |
| `libsdr.dylib` | Shader runtime library | `/usr/local/lib/libsdr.dylib` |

---

## Display Driver Modules (4 plugins)

Installed to: `${OPENRENDER_DISPLAYSDIR}` → `/usr/local/displays/`

| Module | Description | Location |
|--------|-------------|----------|
| `framebuffer.so` | X11 framebuffer display driver | `/usr/local/displays/framebuffer.so` |
| `file.so` | File output driver (TIFF, PNG) | `/usr/local/displays/file.so` |
| `rgbe.so` | RGBE (Radiance) format driver | `/usr/local/displays/rgbe.so` |
| `openexr.so` | OpenEXR display driver (optional) | `/usr/local/displays/openexr.so` |

---

## Development Libraries (1 static library)

**Not installed by default** (build artifact only)

| Library | Description | Location |
|---------|-------------|----------|
| `libopenrendercommon.a` | Common utilities static library | Build only |

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

## Man Pages (4 files)

Installed to: `${CMAKE_INSTALL_MANDIR}/man1` → `/usr/local/share/man/man1/`

| Man Page | Command |
|----------|---------|
| `orender.1` | orender(1) |
| `oshader.1` | oshader(1) |
| `sdrinfo.1` | sdrinfo(1) |
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
│   ├── oshader
│   ├── sdrinfo
│   ├── otexmake
│   └── oshow
├── lib/
│   ├── libri.dylib
│   └── libsdr.dylib
├── include/
│   ├── dlo.h
│   ├── dsply.h
│   ├── implicit.h
│   ├── ptcapi.h
│   ├── ri.h
│   └── shadeop.h
├── displays/            # Display driver modules
│   ├── framebuffer.so
│   ├── file.so
│   ├── rgbe.so
│   └── openexr.so
├── shaders/             # Shader files (54 files)
│   ├── *.sl
│   └── *.sdr / *.rslo
├── share/
│   ├── man/man1/        # Man pages
│   │   ├── orender.1
│   │   ├── oshader.1
│   │   ├── sdrinfo.1
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
  system "#{bin}/sdrinfo", "--version"
  # oshow is optional (requires fltk)
end
```

### Required Libraries
```ruby
test do
  assert_predicate lib/"libri.dylib", :exist?
  assert_predicate lib/"libsdr.dylib", :exist?
end
```

### Display Drivers
```ruby
test do
  assert_predicate lib/"displays/file.so", :exist?
  assert_predicate lib/"displays/framebuffer.so", :exist?
  assert_predicate lib/"displays/rgbe.so", :exist?
  # openexr.so is optional
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

When `INSTALL_SELFCONTAINED=ON` (default, self-contained):

- All files under `CMAKE_INSTALL_PREFIX`
- `OPENRENDER_DISPLAYSDIR` = `${PREFIX}/displays`
- `OPENRENDER_SHADERDIR` = `${PREFIX}/shaders`

---

## Total Artifact Count

| Category | Count |
|----------|-------|
| Executables | 6 (5 installed + 1 build-only) |
| Shared Libraries | 2 |
| Display Modules | 4 |
| Header Files | 6 |
| Shader Files | 54 |
| Man Pages | 4 |
| Documentation Files | 8 |
| **Total Files** | **84** |
