# Implementation Plan: orender-wire — Scene Wireframe Previewer

**Branch**: `006-scene-wireframe-viewer` | **Date**: 2026-05-18 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/006-scene-wireframe-viewer/spec.md`

## Summary

`orender-wire` is a standalone, GPU-accelerated wireframe viewer for RIB scene files. It is a new binary independent from the framebuffer helpers (`orender-fb-macos`, `orender-fb-linux`). The implementation is split into three layers:

1. **libribpreview** — a C++20 static library (shared across platforms) that subclasses `CRendererContext`, intercepts `addObject()`, and tessellates every supported primitive type into a flat world-space line-list vertex buffer. Camera parameters are extracted from `COptions` and `CXform`.
2. **orender-wire (macOS)** — Swift 6 / Metal / AppKit binary. Source lives in `src/preview/orender-wire-macos/`; the installed binary is named `orender-wire`. Mirrors the `orender-fb-macos` SPM + CMake pattern.
3. **orender-wire (Linux)** — C++20 / OpenGL 3.3 Core / GTK 4 binary. Source lives in `src/preview/orender-wire-linux/`; the installed binary is named `orender-wire`. GTK 4 (≥4.20) provides windowing, OpenGL context (`GtkGLArea`), input event controllers, and file dialogs (`GtkFileDialog`).

The arcball camera is ported from `src/gui/interface.h` (`CInterface::toSphere`, quaternion orbit, `computeMatrices`), not from `opengl.cpp` (which only contains legacy drawing helpers). After the port is validated, `src/gui/` is removed from the repository.

## Technical Context

**Language/Version**: C++20 (libribpreview, orender-wire Linux sources), Swift 6 (orender-wire macOS sources). The installed binary is named `orender-wire` on both platforms.

**Primary Dependencies**:
- macOS: Metal, MetalKit, AppKit, simd — all system frameworks; no external deps
- Linux: OpenGL 3.3 Core, GTK 4 (≥4.20) — `GtkGLArea` (OpenGL context), `GtkFileDialog` (file save), GTK 4 event controllers (input); `pkg_check_modules(GTK4 REQUIRED gtk4>=4.20)`
- libribpreview: links against the `ri` static library (CRendererContext, CObject, CXform, CPl, COptions, CAttributes) but NOT against the renderer runtime, shader execution engine, or display driver

**Storage**: N/A

**Testing**: C++20 unit tests under `tests/preview/` (catch2 or a lightweight test runner); integration test: load `examples/rib/camera-dof.rib` through libribpreview and assert non-empty line list

**Target Platform**: macOS 12.0+ (primary), Linux X11/Wayland (secondary)

**Project Type**: Desktop application (CLI-invoked native GUI viewer)

**Performance Goals**: Window open <1 s; geometry visible <5 s for 50,000 primitives; ≥30 fps arcball navigation on any scene that loads

**Constraints**:
- No per-frame re-tessellation; geometry uploaded to GPU once at startup
- Point clouds capped at 100,000 display points with uniform subsampling above that threshold
- No Qt, FLTK, SDL, GLFW, or other third-party GUI toolkits; GTK 4 (≥4.20) is the approved Linux GUI toolkit
- `stdout` unused during normal operation; all warnings go to `stderr`

**Scale/Scope**: Single binary per platform; ~50K primitive reference scene

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|-----------|--------|-------|
| I. Clean Code | PASS | libribpreview split into focused tessellator files; each handles one primitive family |
| II. Language Standards | PASS | C++20 for C++ code; Swift 6 for macOS binary |
| III. TDD (NON-NEGOTIABLE) | GATE | Unit tests for all tessellator functions must be written and failing before implementation begins |
| IV. CLI Interface | PASS | argc/argv; errors → stderr; exit code 0 on success, non-zero on failure; --help supported |
| V. Minimal Dependencies | PASS | System-only: Metal/AppKit (macOS); GTK 4 (≥4.20) + OpenGL (Linux) — GTK 4 is a widely distributed system library requiring no GNOME or KDE desktop environment |
| VI. Platform Targeting | PASS | Platform-specific GPU/window code isolated in `orender-wire-macos/` and `orender-wire-linux/`; libribpreview is platform-neutral |
| VII. Documentation | GATE | `site/` must be updated with orender-wire documentation before merge |

**Gate actions**:
- TDD: geometry-extraction tests under `tests/preview/` must exist and fail before `libribpreview` is implemented
- Documentation: a `site/content/tools/orender-wire.md` page must be added

## Project Structure

### Documentation (this feature)

```text
specs/006-scene-wireframe-viewer/
├── plan.md              ← this file
├── research.md          ← Phase 0 output
├── data-model.md        ← Phase 1 output
├── quickstart.md        ← Phase 1 output
├── contracts/
│   └── cli-interface.md ← Phase 1 output
└── tasks.md             ← Phase 2 output (/speckit-tasks)
```

### Source Code

```text
src/preview/
├── CMakeLists.txt                      ← libribpreview + platform targets
├── libribpreview/
│   ├── CMakeLists.txt
│   ├── previewTypes.h                  ← float3, PreviewCamera, PreviewScene structs
│   ├── previewContext.h                ← CPreviewContext : CRendererContext
│   ├── previewContext.cpp
│   └── tessellators/
│       ├── tessPolygon.cpp             ← CPolygonMesh → line list
│       ├── tessPatch.cpp               ← CBilinearPatch, CBicubicPatch, CPatchMesh
│       ├── tessNurbs.cpp               ← CNURBSPatch, CNURBSPatchMesh
│       ├── tessQuadric.cpp             ← CSphere, CCone, CCylinder, CDisk, CTorus,
│       │                                  CParaboloid, CHyperboloid
│       ├── tessCurve.cpp               ← CCurveMesh
│       ├── tessPoints.cpp              ← CPoints (cross-hair markers, 100K cap)
│       └── tessSubdivision.cpp         ← CSubdivision (control cage)
├── ribpreview_api.h                    ← extern "C" flat C API for Swift/C bridging
├── orender-wire-macos/
│   ├── CMakeLists.txt                  ← swift build custom target (mirrors orender-fb-macos)
│   ├── Package.swift
│   ├── Info.plist
│   └── Sources/
│       ├── main.swift                  ← argv[1] → libribpreview → open window
│       ├── AppDelegate.swift
│       ├── WireframeRenderer.swift     ← MTKView; uploads line-list once; draw(in:)
│       ├── ArcballCamera.swift         ← quaternion arcball; NSResponder; trackpad gestures
│       ├── Shaders.metal               ← vertex MVP + fragment flat color; grid + axis pass
│       └── ribpreview_bridge.h         ← Swift bridging header imports ribpreview_api.h
└── orender-wire-linux/
    ├── CMakeLists.txt
    ├── arcball.h / arcball.cpp         ← C++20 arcball camera (platform-neutral math)
    └── main.cpp                        ← GtkApplication + GtkWindow + GtkGLArea;
                                           VAO/VBO; GTK 4 event controllers; GLSL 3.30 shaders

tests/preview/
├── CMakeLists.txt
├── test_polygon.cpp
├── test_patch.cpp
├── test_quadric.cpp
├── test_nurbs.cpp
└── test_camera.cpp

site/content/tools/
└── orender-wire.md                     ← documentation page (Constitution VII gate)
```

**Structure Decision**: Single-project layout extending the existing `src/` tree. A new `src/preview/` subtree contains all preview-specific code. `tests/preview/` parallels existing test structure. `src/gui/` is removed after arcball port is validated.

## Complexity Tracking

No constitution violations requiring justification. Platform-native GPU code (Metal + GL) is mandated by the spec and the constitution's minimal-dependencies principle (system libraries over third-party toolkits).
