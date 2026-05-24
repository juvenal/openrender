# Quickstart: orender-wire Developer Guide

**Date**: 2026-05-18 | **Branch**: `006-scene-wireframe-viewer`

---

## Build

### Full build (all platforms)

```bash
cmake --build build --config Release
```

`orender-wire` is installed to `build/bin/` on both platforms alongside `orender`. The source directories are named `orender-wire-macos/` and `orender-wire-linux/` to reflect their platform-specific contents, but both CMake targets produce a binary named `orender-wire`.

### Build libribpreview only (for TDD iteration)

```bash
cmake --build build --target ribpreview
```

### Build and run tests

```bash
cmake --build build --target preview_tests
ctest --test-dir build -R preview --output-on-failure
```

---

## Run

orender-wire requires **no** renderer environment variables (`ORENDERHOME`, `SHADERS`, `DISPLAYS` are for the full renderer and are not used here).

```bash
# Basic usage — no env vars needed
build/orender-wire.app/Contents/MacOS/orender-wire examples/rib/camera-dof.rib

# Scenes with Geometry "name" statements need GEOMETRIES
GEOMETRIES="$(pwd)/openrender/geometry" \
build/orender-wire.app/Contents/MacOS/orender-wire examples/rib/teapot.rib

# Verify it exits cleanly with an invalid path
build/orender-wire.app/Contents/MacOS/orender-wire /nonexistent.rib; echo "exit: $?"
# Expected: exit: 2
```

---

## Adding a New Primitive Tessellator

1. Write the failing test in `tests/preview/test_<type>.cpp` (TDD gate).
2. Add `#include "ri/<type>.h"` and `#include "previewTypes.h"` in the new tessellator.
3. Implement `void tess<Type>(const C<Type>& obj, std::vector<float3>& out)` in `src/preview/libribpreview/tessellators/tess<Type>.cpp`.
4. Register a `dynamic_cast` branch in `CPreviewContext::addObject()` in `previewContext.cpp`.
5. Run tests; all must pass before committing.

---

## Key Source Locations

| What | Where |
|---|---|
| Geometry object classes | `src/ri/polygons.h`, `patches.h`, `quadrics.h`, `curves.h`, `points.h`, `subdivision.h` |
| CXform (transform matrices) | `src/ri/xform.h` — `from` = object→world, `to` = world→object |
| CPl (parameter list, vertex data) | `src/ri/pl.h` — `data0` is the flat float array |
| CRibGeometryContext (geometry context) | `src/preview/libribpreview/ribGeometryContext.h` — lightweight base; override `addObject()` |
| Original arcball math | `src/gui/interface.h` — `CInterface::toSphere`, `computeMatrices` |
| orender-fb-macos CMake pattern | `src/framebuffer/orender-fb-macos/CMakeLists.txt` |
| Linux GTK 4 CMake pattern | `pkg_check_modules(GTK4 REQUIRED gtk4>=4.20)` in `src/preview/orender-wire-linux/CMakeLists.txt` |

---

## Coordinate Convention Quick Reference

| Layer | Convention |
|---|---|
| CXform matrices (`from`, `to`) | Row-major, right-handed (openRender convention) |
| libribpreview output (`PreviewSceneC`) | Column-major after transpose in `ribpreview_load()` |
| Metal `simd_float4x4` | Column-major |
| OpenGL mat4 uniform | Column-major |
| RIB Transform statement | Row-major (RenderMan convention) |

The single transpose happens inside `ribpreview_load()`, at the C++/platform boundary. Neither the Swift nor the Linux C++ caller needs to transpose.

---

## Deleting src/gui/ (cleanup step)

Once `ArcballCamera.swift` and the Linux arcball C++ class are reviewed and tests pass:

```bash
git rm -r src/gui/
git commit -m "chore: remove legacy src/gui/ (arcball ported to orender-wire)"
```

Verify nothing in `src/CMakeLists.txt` references `gui` before deletion (confirmed: no `add_subdirectory(gui)` entry).
