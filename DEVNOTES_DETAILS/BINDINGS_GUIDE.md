# openRender Language Bindings Guide

## Overview

openRender provides first-class language bindings for **Python** and **Lua**, allowing developers to drive the renderer through a RenderMan-compliant `Ri` interface. These bindings are designed to mirror the C API as closely as possible, making it easy to translate existing RIB generation logic or C-based renderer calls into high-level scripts.

## Supported Languages

### Python Binding
The Python binding provides a `Ri` class that implements the standard RenderMan Interface.

- **Capabilities**:
  - Full RIB generation support (geometry, shading, transforms, motion blocks, solid CSG).
  - Standard RIB preamble emission: Automatically writes `##RenderMan RIB-Structure 1.1` and creator metadata when `Begin()` is called.
  - Flexible output: write to `stdout`, files, or pipe directly to the `orender` process.
  - Comprehensive block nesting and parameter handling.
- **Source**: `src/python/prman.py`
- **Installed to**: `${PREFIX}/python/prman.py` (self-contained) or `${PREFIX}/share/openRender/python/prman.py` (FHS)

### Lua Binding
The Lua binding provides a full RIB-emitting interface, mirroring the functionality of the Python and C bindings.

- **Capabilities**:
  - complete RIB spec coverage including graphics state, camera/display options, and all geometric primitives.
  - `Ri:HierarchicalSubdivisionMesh` (spec 010-full-subdivision-support) — per-face/per-level tag overrides layered on a base `Ri:SubdivisionMesh`, following the same argument-marshalling shape (`prman.lua:568,573`). Lua-only, matching `Ri:SubdivisionMesh`/`RiSubdivisionMeshV` itself, which has no Python binding either.
  - Standard RIB preamble emission: Automatically writes `##RenderMan RIB-Structure 1.1` and creator metadata when `Begin()` is called.
  - Support for compound RIB features like `MotionBegin`, `SolidBegin`, and `ObjectInstance`.
  - Validated test suite for file output and process pipe modes.
- **Source**: `src/lua/prman.lua`
- **Installed to**: `${PREFIX}/lua/prman.lua` (self-contained) or `${PREFIX}/share/openRender/lua/prman.lua` (FHS)

## Examples: Animated Colorcircles

A comprehensive example, `colorcircles`, demonstrates how to use the bindings to generate a 101-frame animation of a colored sphere grid with per-frame Z-rotation and scale transformations.

- **C/C++ Example**: Located in `examples/bindings/c/`, with a cross-platform Makefile.
- **Python Example**: Located in `examples/bindings/python/`, including a README with run commands.
- **Lua Example**: Located in `examples/bindings/lua/`, mirroring the animated grid logic.

Each example includes documentation on prerequisites and usage, providing a practical starting point for building complex scene generators in your language of choice.

## Integration & Tests

The bindings are integrated into the CMake build system as proper install targets (added in commit `f01c59c`). The install destination can be customized at configure time:

```bash
cmake .. -DOPENRENDER_PYTHONDIR=/usr/lib/python3/dist-packages \
         -DOPENRENDER_LUADIR=/usr/share/lua/5.4
```

Regression tests validate:
- Correct RIB syntax generation.
- Pipe-based communication with the renderer.
- Handling of complex nested blocks (e.g., Solid modeling, Motion blur).
