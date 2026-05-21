# Developer Notes

## Project Status

| Area | Status | Detail File |
|------|--------|-------------|
| oshader compiler | Complete — IR, optimization passes, `.rslo` / `rsloinfo` | [OSHADER_UPDATES.md](DEVNOTES_DETAILS/OSHADER_UPDATES.md) |
| Imager shaders | Complete — all 7 spec variables, thread-safe | [OSHADER_UPDATES.md](DEVNOTES_DETAILS/OSHADER_UPDATES.md) |
| RIB output | Complete — unified init, standard preamble headers | [RIB_GUIDE.md](DEVNOTES_DETAILS/RIB_GUIDE.md) |
| Framebuffer IPC display | Complete — Unified driver, macOS/Linux parity | [FRAMEBUFFER_GUIDE.md](DEVNOTES_DETAILS/FRAMEBUFFER_GUIDE.md) |
| Language bindings | Complete — Python, Lua, C/C++ | [BINDINGS_GUIDE.md](DEVNOTES_DETAILS/BINDINGS_GUIDE.md) |
| Geometry statements | Complete — in-place expansion, circularity detection | [GEOMETRY_STATEMENT.md](DEVNOTES_DETAILS/GEOMETRY_STATEMENT.md) |
| Scene wireframe previewer | Complete — orender-wire (macOS Metal + Linux GTK4/OpenGL), libribpreview, full test suite | — |
| Hider parity | Partial — filtering and jitter done; motion blur, transparency pending | [HIDER_PARITY.md](DEVNOTES_DETAILS/HIDER_PARITY.md) |
| RISpec 3.2 gaps | 1 of 7 implemented | [RISPEC_GAPS.md](DEVNOTES_DETAILS/RISPEC_GAPS.md) |
| C++20 / C17 migration | Phase 2 complete — portable I/O, binary security; Phase 3 future | [CXX20_MIGRATION.md](DEVNOTES_DETAILS/CXX20_MIGRATION.md) |

## Recent Major Refactors

- **Scene Wireframe Previewer (`orender-wire`)**: Added `orender-wire`, an interactive RIB wireframe viewer shipping as a Metal/AppKit `.app` on macOS and a GTK 4/OpenGL binary on Linux. Built on a new `libribpreview` static library that tessellates all RenderMan primitive types into a flat line-list vertex buffer with per-vertex surface colors.
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
- [ ] LLVM integration and binary shader compilation — see [OSHADER_UPDATES.md](DEVNOTES_DETAILS/OSHADER_UPDATES.md)

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
| [CXX20_MIGRATION.md](DEVNOTES_DETAILS/CXX20_MIGRATION.md) | C++20/C17 migration, portable I/O, binary format changes |
