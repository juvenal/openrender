# Developer Notes

## Project Status

| Area | Status | Detail File |
|------|--------|-------------|
| oshader compiler | Complete — IR, optimization passes, `.rslo` / `rsloinfo` | [OSHADER_UPDATES.md](DEVNOTES_DETAILS/OSHADER_UPDATES.md) |
| LLVM JIT shading engine | Complete — `oshader --jit` produces `.slo` LLVM bitcode; `libshader` extraction; JIT runtime; `sloinfo` inspector; 43-scene visual test suite | [OSHADER_UPDATES.md](DEVNOTES_DETAILS/OSHADER_UPDATES.md) |
| Imager shaders | Complete — all 7 spec variables, thread-safe, spec-correct pipeline order (Exposure → Imager → Quantize) | [OSHADER_UPDATES.md](DEVNOTES_DETAILS/OSHADER_UPDATES.md) |
| RIB output | Complete — unified init, standard preamble headers | [RIB_GUIDE.md](DEVNOTES_DETAILS/RIB_GUIDE.md) |
| Framebuffer IPC display | Complete — Unified driver, macOS/Linux parity | [FRAMEBUFFER_GUIDE.md](DEVNOTES_DETAILS/FRAMEBUFFER_GUIDE.md) |
| Language bindings | Complete — Python, Lua, C/C++ | [BINDINGS_GUIDE.md](DEVNOTES_DETAILS/BINDINGS_GUIDE.md) |
| Geometry statements | Complete — in-place expansion, circularity detection | [GEOMETRY_STATEMENT.md](DEVNOTES_DETAILS/GEOMETRY_STATEMENT.md) |
| Scene wireframe previewer | Complete — orender-wire (macOS Metal + Linux GTK4/OpenGL), libribpreview, full test suite | [VERIFICATION_LINUX_PREVIEW.md](DEVNOTES_DETAILS/VERIFICATION_LINUX_PREVIEW.md) |
| Hider parity | Partial — filtering and jitter done; motion blur, transparency pending | [HIDER_PARITY.md](DEVNOTES_DETAILS/HIDER_PARITY.md) |
| RISpec 3.2 gaps | 1 of 7 implemented | [RISPEC_GAPS.md](DEVNOTES_DETAILS/RISPEC_GAPS.md) |
| C++20 / C17 migration | Phase 2 complete — portable I/O, binary security; Phase 3 future | [CXX20_MIGRATION.md](DEVNOTES_DETAILS/CXX20_MIGRATION.md) |

## Recent Major Refactors

- **libshader extraction & LLVM JIT shading engine**: Extracted the shader compiler (`src/oshader` → `src/libshader/compiler`) and interpreter runtime (`src/rslo` → `src/libshader/runtime`) into a new `libshader` static library hierarchy. Added a LLVM-based JIT backend: `oshader --jit` compiles RSL shaders to LLVM bitcode (`.slo`) with embedded metadata (shader type, `usedParameters` bitmask, parameter defaults). The JIT runtime loads `.slo` via LLJIT and dispatches through the same `op_*`/`rsl_*` C-linkage ABI used by the interpreter. New `sloinfo` binary auto-detects `.slo` vs `.rslo` by file magic bytes. Symbol retention for macOS dead-stripping handled by `jitSymbolRetain.cpp` with `__attribute__((constructor))`. Coordinate-transform ops (`op_vtransform`, `op_ntransform`, `op_ptransform`) corrected. Shader-space parameter defaults at bind time fixed via `jitSetInitXform` thread-local fallback. Shader format selection: `Attribute "shade" "shaderformat"` (per-primitive), `Option "shaderformat" "default"` (scene-wide), or `OPENRENDER_DEFAULT_FORMAT` (compile-time fallback). See [OSHADER_UPDATES.md](DEVNOTES_DETAILS/OSHADER_UPDATES.md).
- **CSE Pass — Intra-Block Scope Fix**: The CSE optimizer previously shared its expression cache across all IR basic blocks, enabling incorrect substitutions across if/else branch boundaries. `exprMap` is now cleared per block; cross-block CSE requires dominator analysis not yet encoded in the IR. The regression manifested as a corrupted `windowhighlight.sl` highlight shape. See [OSHADER_UPDATES.md](DEVNOTES_DETAILS/OSHADER_UPDATES.md).
- **Shader Built-In Scope Enforcement**: A new `globalVarScope` map on `CScriptContext` records per-variable shader-type restrictions. `getVariable()` is restructured to enforce these restrictions regardless of which internal lookup path resolves the variable. `Ps` is now rejected in surface shaders; imager output variables (`Ci`, `Oi`, `alpha`) have their scope masks corrected to include `SLC_IMAGER`. See [OSHADER_UPDATES.md](DEVNOTES_DETAILS/OSHADER_UPDATES.md).
- **Imager Shader Pipeline Order Fix**: `CRenderer::dispatch()` now applies exposure (gain/gamma) to color (Ci) and coverage (Oi/alpha) channels before executing the imager shader, per the RenderMan spec pipeline (Render → Exposure → Imager → Quantize). Previously the imager saw raw linear-light values. Exposure is removed from `CFileOutputBase::applyColorPipeline()`, which is now quantize-only. The `gain` member is removed from `CFileOutputBase`; `gamma` is retained for PNG gAMA metadata embedding.
- **File Display Output Base (`CFileOutputBase`)**: Extracted shared scanline accumulation, mutex management, quantization, and dither into a new `CFileOutputBase` class in `src/file/file_base.h` / `file_base.cpp`. All four file-format display plugins (TIFF, PNG, OpenEXR, RGBE) now implement only `fillPixels()` and `flushRow()`. Display modules renamed to the `.dsply` extension (e.g., `file.dsply`). `file_base.h` is now installed to `<prefix>/include/` for third-party plugin authors. See [DISPLAY_PLUGIN_GUIDE.md](DEVNOTES_DETAILS/DISPLAY_PLUGIN_GUIDE.md).
- **macOS `orender-wire` App Bundle — Versioned RI Dylib**: The orender-wire macOS `.app` bundle now copies `libri.<SOVERSION>.dylib` into `Contents/Frameworks/` and creates an unversioned `libri.dylib` symlink alongside it, matching install-tree naming and enabling correct `@rpath` resolution at runtime.
- **Build System — RPATH, Library Versioning, and Distribution Packaging**: Bumped `cmake_minimum_required` to 3.16. Both `libri` and `librslo` now build as OBJECT libraries, producing shared (with `VERSION`/`SOVERSION`) and static archives (`libri.a`, `librslo.a`) from a single compilation pass. Self-contained installs embed an `RPATH` into all executables (`@loader_path/../lib` / `$ORIGIN/../lib`) and bundle external Homebrew dependencies via `file(GET_RUNTIME_DEPENDENCIES)` with `install_name_tool` rewrites and re-signing. `libopenrendercommon` removed from the install step (its object code is embedded in libri/librslo). Python and Lua bindings (`prman.py`, `prman.lua`) are now proper CMake install targets with configurable destinations (`OPENRENDER_PYTHONDIR`, `OPENRENDER_LUADIR`).
- **Scene Wireframe Previewer (`orender-wire`)**: Added `orender-wire`, an interactive RIB wireframe viewer shipping as a Metal/AppKit `.app` on macOS and a GTK 4/OpenGL binary on Linux. Built on a new `libribpreview` static library that tessellates all RenderMan primitive types into a flat line-list vertex buffer with per-vertex surface colors. Linux implementation refined for distribution with GTK 4.20 compatibility, terminal detachment, and RPATH resolution (see [VERIFICATION_LINUX_PREVIEW.md](DEVNOTES_DETAILS/VERIFICATION_LINUX_PREVIEW.md)).
- **RI Context Decoupling (`CRibGeometryContext`)**: Introduced `CRibGeometryContext` as a lightweight `CRiInterface` subclass for geometry-only parsing. `CPreviewContext` now extends this instead of `CRendererContext`, eliminating dependencies on display plugins, shader search paths, and network subsystems. `addObject()` lifted to `CRiInterface`; all geometry `instantiate()` signatures changed from `CRendererContext*` to `CRiInterface*` across ~30 call sites. `RiBeginLite()` added to `ri.cpp`/`rib.h` for minimal RI initialization without a full `RiBegin()` cycle.
- **Legacy GUI Removal (`src/gui/`)**: Deleted the unmaintained Qt/FLTK GUI directory. The arcball camera math was re-implemented in Swift (`ArcballCamera.swift`) for macOS and as standalone C++20 inline functions for Linux.
- **Shader Compiler Subsystem Rename (`sdr` → `rslo`)**: Renamed the entire shading language object subsystem to `rslo` (RenderMan Shading Language Object). This includes directories (`src/rslo`, `src/rsloinfo`), libraries (`librslo`), and the inspection tool (`rsloinfo`).
- **Unified RIB Output**: Consolidated `CRibOut` initialization and added standard RenderMan compliant preamble headers to all RIB output (C++, Python, Lua).
- **Unified IPC Framebuffer**: Merged platform-specific display drivers into a single platform-neutral IPC driver, isolating windowing logic into standalone helper binaries.

## Open Issues

- [ ] Purging tessellations for raytracing (no cache eviction mechanism found)
- [ ] Moving raytraced surface (`CRaytracer` lacks native motion blur support)
- [ ] Efficient subdivision surface creases
- [ ] Subdivision highly creased surface issues
- [ ] Irradiance accuracy issues

## Todos

- [ ] OpenEXR input for textures (output is supported; input/texture reading is missing)
- [ ] Trace subsets (`trace()` does not yet filter by subset)
- [ ] Patch crack stitching (currently handled via displacement bounds)
- [ ] Hider parity completion — see [HIDER_PARITY.md](DEVNOTES_DETAILS/HIDER_PARITY.md)

## See Also

| File | Coverage |
|------|----------|
| [BUGS.md](DEVNOTES_DETAILS/BUGS.md) | Full open issues and resolved bugs with fix notes |
| [RIB_GUIDE.md](DEVNOTES_DETAILS/RIB_GUIDE.md) | Standard RIB output, preamble headers, and `CRibGeometryContext` |
| [RISPEC_GAPS.md](DEVNOTES_DETAILS/RISPEC_GAPS.md) | RenderMan Spec 3.2 compliance gaps |
| [OSHADER_UPDATES.md](DEVNOTES_DETAILS/OSHADER_UPDATES.md) | Shader compiler and imager shader implementation |
| [FRAMEBUFFER_GUIDE.md](DEVNOTES_DETAILS/FRAMEBUFFER_GUIDE.md) | Unified IPC framebuffer display architecture |
| [BINDINGS_GUIDE.md](DEVNOTES_DETAILS/BINDINGS_GUIDE.md) | Python, Lua, and C++ language bindings |
| [GEOMETRY_STATEMENT.md](DEVNOTES_DETAILS/GEOMETRY_STATEMENT.md) | Geometry RIB statement implementation |
| [HIDER_PARITY.md](DEVNOTES_DETAILS/HIDER_PARITY.md) | Stochastic vs. raytrace hider alignment |
| [VERIFICATION_LINUX_PREVIEW.md](DEVNOTES_DETAILS/VERIFICATION_LINUX_PREVIEW.md) | Linux orender-wire and orender-fb verification results |
| [CXX20_MIGRATION.md](DEVNOTES_DETAILS/CXX20_MIGRATION.md) | C++20/C17 migration, portable I/O, binary format changes |
