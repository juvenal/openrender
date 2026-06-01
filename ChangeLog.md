# Changelog

All notable changes to openRender are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

- (2026-05-27) Added Python (`prman.py`) and Lua (`prman.lua`) binding install targets to the CMake build. Both bindings are installed under `python/` and `lua/` (self-contained) or `share/openRender/python/` and `share/openRender/lua/` (FHS). Install destinations can be overridden with `-DOPENRENDER_PYTHONDIR` and `-DOPENRENDER_LUADIR` (f01c59c).
- (2026-05-21) Added `CRibGeometryContext` — lightweight `CRiInterface` subclass for geometry-only RIB parsing without renderer initialization. Lifted `addObject()` to `CRiInterface`; changed all geometry `instantiate()` signatures from `CRendererContext*` to `CRiInterface*`. Added `RiBeginLite()` to `ri.cpp`/`rib.h`. Extended all tessellators with per-vertex surface color output. Updated Metal wireframe shaders for per-vertex color buffers (8f94f66).
- (2026-05-19) Added `orender-wire`: interactive RIB wireframe previewer — Metal/AppKit on macOS, GTK 4/OpenGL on Linux. Includes `libribpreview` static library with tessellators for all RenderMan primitive types (polygon, patch, NURBS, quadric, curve, points, subdivision, procedural), arcball camera, camera export/replace, and full test suite (117d77c).
- (2026-05-19) Added automatic standard RenderMan RIB preamble headers to all RIB generated via C++, Python, and Lua (e7a63b1, 97c3265).
- (2026-03-10) Added compiled shader extension `.rslo` support and dual-lookup logic for backward compatibility.
- (2026-02-08–2026-02-10) Added comprehensive README for openRender and Geometry RIB statement support with updated geometry definitions (7cd1eed, 43b23a0, a992b91).
- (2026-01-02–2026-01-04) Added new shaders, debugging scripts, wood rendering examples, custom geometry directory, Utah Teapot RIB, and NSI PDF references (3186153, 9483a60, 12f9aef, c95d2df, 8f6148d).
- (2025-12-18) Added Homebrew setup guide and quickstart script for openRender (133fe82).
- (2025-12-12–2025-12-16) Added Hugo documentation migration workflow, theme subrepo, site content, and GitHub Actions deployment for the documentation site (b7a69ec, 67ba426, a809bcb, 6913619, dbb592b).
- (2025-10-23) Added speckit templates and agent context management for developer tooling (8ef2931).

### Changed

- (2026-05-28) Overhauled file-format display plugin architecture: extracted shared scanline accumulation, mutex management, quantization, and dither into `CFileOutputBase` (`src/file/file_base.h`/`file_base.cpp`). TIFF, PNG, OpenEXR, and RGBE writers now implement only `fillPixels()` and `flushRow()`. Display modules use the `.dsply` extension on all non-Windows platforms. `file_base.h` is installed to `<prefix>/include/` for third-party authors. Expanded default `ORENDERHOME`-relative search paths (64e15c8).
- (2026-05-28) macOS `orender-wire` app bundle now copies `libri.<SOVERSION>.dylib` into `Contents/Frameworks/` and creates an unversioned symlink, matching install-tree ABI naming and ensuring correct `@rpath` resolution (4a6b762).
- (2026-05-27) Overhauled the CMake build system: bumped `cmake_minimum_required` to 3.16; added `libri.a` and `librslo.a` static archives (built via OBJECT library pattern — one compilation pass for both shared and static); added `VERSION`/`SOVERSION` metadata to shared libraries (`OPENRENDER_COMPAT_SOVERSION` cache var); set `CMAKE_INSTALL_RPATH` globally for self-contained installs (`@loader_path/../lib` on macOS, `$ORIGIN/../lib` on Linux); bundled external Homebrew dependencies into `lib/` on self-contained installs with `file(GET_RUNTIME_DEPENDENCIES)` + `install_name_tool` rewrites + `codesign`; removed `openrendercommon` from the install step (object code is embedded in libri/librslo) (e9dd8a9).
- (2026-05-24) Hardened Linux `orender-wire` startup: lowered GTK requirement to 4.20, linked `epoxy` explicitly, installed binary under `libexec`, configured relative library lookup via RPATH, suppressed Mesa/EGL diagnostics, detached launches from terminal, and allowed independent application instances (6c1c010).
- (2026-05-19) Refactored the shader compiler subsystem rename (`sdr` → `rslo`), including internal symbols, directory structure, and tooling (f80a6ad).
- (2026-05-19) Removed legacy `src/gui/` Qt/FLTK directory; arcball camera math ported to Swift (`ArcballCamera.swift`) and C++20 (`arcball.cpp`) (117d77c).
- (2026-03-10) Updated `oshader` to output `.rslo` by default and added `--legacy-sdr` flag.
- (2026-02-08) Refactored `clampData`, renamed Pixie to openRender and updated related components, added speckit commands, and adjusted configuration formatting (ce208bf, c4e15a5, c3ab17f, 43737b7).
- (2025-12-27–2025-12-31) Replaced max/min macros with clearer conditionals, improved versioning and path handling in options, and refreshed shader sets and `.gitignore` entries (b6dacb9, 24ea6e7, bb70eb6, ddf438e, 6756d8b).
- (2025-12-16–2025-12-18) Updated CMake configuration and project structure, merged upstream changes, refreshed project configuration, and standardized shader formatting (a26eca0, 09029dd, ad42a3d, a8da1a3).
- (2025-12-08) Updated CMake installation paths and modernized the codebase for C++20/C17 with 64-bit portable I/O for photon maps, point clouds, and deep shadow maps (fdcdc94, 3952bf1, a9348a9, f23095a, 602559f, 15cb212).
- (2025-12-07) Updated framebuffer classes, algebra/align headers, and header comments to Doxygen style; added a `.clang-format` configuration; established the project constitution (3be02d7, 0e1b9a5, d121092, 8e6ba32, 855b25a).
- (2025-08-27) Standardized source formatting and cleaned up the build process (17b40c9, 6f91888).

### Fixed

- (2026-05-31) Fixed CSE optimizer cross-block corruption: `CCSEPass::cseFn()` now clears `exprMap` at the start of each IR basic block, restricting Common Subexpression Elimination to intra-block scope. The previous shared map caused constant loads inside if/else branches to replace identical constants in subsequent blocks on unrelated control-flow paths. The concrete regression was `windowhighlight.sl`, where the x-range boundary test was compiled against `yfract` instead of the literal `0` (1742e5b).
- (2026-05-31) Fixed `oshader` wrongly accepting `Ps` (the illuminated surface point) inside surface shaders. `Ps` is only defined for light shaders per RI Spec. `addGlobalVariable()` now records per-variable scope masks in a new `globalVarScope` map; `getVariable()` is restructured so the scope check applies regardless of which internal lookup path resolved the variable. Imager output variables (`Ci`, `Oi`, `alpha`, `ncomps`, `time`, `dtime`) had `SLC_IMAGER` added to their scope masks to prevent false rejections in imager shaders (1742e5b).
- (2026-05-28) Fixed imager shader pipeline order: `CRenderer::dispatch()` now applies exposure (gain/gamma) to color (Ci) and coverage (Oi/alpha) before executing the imager shader, per the RenderMan spec (Render → Exposure → Imager → Quantize). Previously the imager received raw linear-light values, violating the spec. Exposure is removed from `CFileOutputBase::applyColorPipeline()`, which is now quantize-only. The `gain` member is removed from `CFileOutputBase`; `gamma` is retained for PNG gAMA metadata.
- (2026-05-24) Fixed `debug_openrender.sh` to honor the `OPTS` environment variable — caller-supplied renderer options are now forwarded instead of being silently dropped (6d00b99).
- (2026-02-08) Replaced `sprintf` with `snprintf` to prevent buffer overflows and resolve related warnings (fca5271).
- (2025-12-13) Fixed Hugo build workflow issues and parameter placement in the documentation pipeline (8b2f277, 99bf323, 740f97a).

### Docs

- (2025-12-16) Updated Hugo configuration for the openRender documentation site (dbb592b).
- (2025-12-08) Updated migration guide for Phase 2 completion (15cb212).

## [1.0.0] - 2025-02-10

### Added

- openRender 1.0.0: rebrand from Pixie; CMake-based build and install; documentation converted to Markdown; semantic versioning (MAJOR.MINOR.PATCH).
