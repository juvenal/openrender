# openRender Language Bindings Guide

## Overview

openRender provides first-class language bindings for **Python** and **Lua**, allowing developers to drive the renderer through a RenderMan-compliant `Ri` interface. These bindings are designed to mirror the C API as closely as possible, making it easy to translate existing RIB generation logic or C-based renderer calls into high-level scripts.

## Supported Languages

### Python Binding
The Python binding provides a `Ri` class that implements the standard RenderMan Interface.

- **Capabilities**:
  - Full RIB generation support (geometry, shading, transforms, motion blocks, solid CSG).
  - Flexible output: write to `stdout`, files, or pipe directly to the `orender` process.
  - Comprehensive block nesting and parameter handling.
- **Location**: `src/bindings/python/`

### Lua Binding
The Lua binding provides a full RIB-emitting interface, mirroring the functionality of the Python and C bindings.

- **Capabilities**:
  - complete RIB spec coverage including graphics state, camera/display options, and all geometric primitives.
  - Support for compound RIB features like `MotionBegin`, `SolidBegin`, and `ObjectInstance`.
  - Validated test suite for file output and process pipe modes.
- **Location**: `src/bindings/lua/`

## Examples: Animated Colorcircles

A comprehensive example, `colorcircles`, demonstrates how to use the bindings to generate a 101-frame animation of a colored sphere grid with per-frame Z-rotation and scale transformations.

- **C/C++ Example**: Located in `examples/bindings/c/`, with a cross-platform Makefile.
- **Python Example**: Located in `examples/bindings/python/`, including a README with run commands.
- **Lua Example**: Located in `examples/bindings/lua/`, mirroring the animated grid logic.

Each example includes documentation on prerequisites and usage, providing a practical starting point for building complex scene generators in your language of choice.

## Integration & Tests

The bindings are integrated into the build system and include regression tests that validate:
- Correct RIB syntax generation.
- Pipe-based communication with the renderer.
- Handling of complex nested blocks (e.g., Solid modeling, Motion blur).
