# Contract: build options and dependencies (FR-002, FR-003, FR-024)

## CMake

```cmake
# Root CMakeLists.txt, BEFORE project() (the vcpkg toolchain installs at project()):
option(OPENRENDER_ENABLE_OIIO "Build the optional OpenImageIO >= 3.0 image backend (spec 048)" OFF)

if(OPENRENDER_ENABLE_OIIO AND <vcpkg toolchain selected by the existing VCPKG_ROOT block>)
    list(APPEND VCPKG_MANIFEST_FEATURES oiio)
    # separate install tree: OIIO-on and OIIO-off manifests must never share one (manifest mode removes packages)
    if(NOT <VCPKG_INSTALLED_DIR given on the command line>)
        if(DEFINED ENV{VCPKG_INSTALLED_DIR})
            set(VCPKG_INSTALLED_DIR "$ENV{VCPKG_INSTALLED_DIR}-oiio" CACHE STRING "" FORCE)
        else()
            set(VCPKG_INSTALLED_DIR "${CMAKE_CURRENT_SOURCE_DIR}/vcpkg_installed-oiio" CACHE STRING "" FORCE)
        endif()
    endif()
    if(<resolved VCPKG_INSTALLED_DIR> STREQUAL "$ENV{VCPKG_INSTALLED_DIR}" OR <it equals the manifest-relative default>)
        message(FATAL_ERROR "OPENRENDER_ENABLE_OIIO=ON must not use the shared vcpkg install tree …")
    endif()
endif()

# After project(), next to the OpenEXR block:
set(HAVE_OIIO OFF)
if(OPENRENDER_ENABLE_OIIO)
    find_package(OpenImageIO CONFIG QUIET)          # version-less: OIIO's version file is SameMajorVersion
    if(OpenImageIO_FOUND AND OpenImageIO_VERSION VERSION_GREATER_EQUAL 3.0)
        set(HAVE_OIIO ON)
        message(STATUS "Found OpenImageIO ${OpenImageIO_VERSION} -- OIIO image backend enabled")
    elseif(OpenImageIO_FOUND)
        message(WARNING "OpenImageIO ${OpenImageIO_VERSION} is older than 3.0 -- building without the OIIO backend")
    else()
        message(WARNING "OpenImageIO not found -- building without the OIIO backend (set OpenImageIO_ROOT)")
    endif()
endif()
```

- `HAVE_OIIO` is a target compile definition on `ribRender_obj` and the test targets, never in `config.h`. It is
  the `HAVE_OPENEXR` pattern, so `libRIBVector` stays OIIO-free.
- With `HAVE_OIIO` off, `src/imageio/` and `src/display/oiio/` are not added, no OIIO test is registered, and
  `ImageIO_AbsentBuild` plus `ImageIO_SelectWithoutOIIO` are registered instead (FR-003).
- `CMake/openRenderConfig.cmake.in` gains `OPENRENDER_CONFIG_HAVE_OIIO`. Static consumers get `RIBImageIO` (shared)
  in `ribRender_static`'s INTERFACE. No `find_dependency(OpenImageIO)` is needed, because OIIO is PRIVATE to the
  library.
- The CMake floor stays 3.19. OIIO 3.1 itself needs 3.18.2 (`cmake_minimum_required(VERSION 3.18.2...4.0)`).

## vcpkg (macOS)

`vcpkg.json` gains a feature; the top-level `dependencies` list is unchanged:

```json
"features": {
  "oiio": {
    "description": "OpenImageIO 3 image backend (spec 048): pixel I/O only, no colour conversion",
    "dependencies": [ { "name": "openimageio", "default-features": false } ]
  }
}
```

- Resolves openimageio 3.1.14 plus 8 new ports: expat, fmt, minizip-ng, opencolorio, pystring, robin-map,
  sse2neon and yaml-cpp (research R10). OpenColorIO is required by the port and by OIIO's build, and cannot be
  dropped. No colour conversion API is called.
- Triplets are unchanged (`arm64-osx-openrender`, `x64-osx-openrender`: static, release-only, 13.3). OIIO and OCIO
  are static archives folded into `libRIBImageIO.dylib` behind the exported-symbols list.
- `builtin-baseline` is unchanged, so no LLVM rebuild is triggered by a baseline bump.
- **No vcpkg run happens in this worktree before the maintainer approves M1** (quickstart). Even then it targets the
  private tree `~/.cache/vcpkg-installed/openrender-oiio`, the derived `-oiio` directory, never
  `~/.cache/vcpkg-installed/openrender`.

## Homebrew (macOS without `VCPKG_ROOT`)

`brew install openimageio` gives 3.2.x as a shared library, with a large closure (ffmpeg, libheif, libraw, jxl, …;
research R13). That is acceptable for a developer opting in, and it is never the default.

## Linux (Ubuntu 24.04)

- Stock packages are too old (OIIO 2.4.17, OCIO 2.1.3; measured), so OIIO is **absent by default**, and an ON
  configure warns and builds the legacy tree.
- To enable it, build OIIO 3.1.14.0 into a prefix (commands: quickstart M3a), then
  `cmake -DOPENRENDER_ENABLE_OIIO=ON -DOpenImageIO_ROOT=$HOME/oiio-048/prefix …`. `libRIBImageIO` gets an RPATH to
  the prefix's `lib`. Development only; no package bundles it in 048 (D24).
- Documented in `INSTALL.md`/`COMPILING.txt`: the minimum version 3.0, the option, `OpenImageIO_ROOT`, and the OCIO
  2.3 requirement.

## CI (`.github/workflows/release.yml`)

- No job enables OIIO in 048 (P-Q11), so every CI job is a "without OIIO" job (FR-003).
- The cache key changes with `vcpkg.json`. The `restore-keys` prefix restores the previous archives, and LLVM's ABI
  hash is unaffected (research R11). Expected first-run cost: archive restore only, no LLVM build.
- The FHS and release builds keep linking what they link today; nothing is bundled.
