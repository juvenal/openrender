# RenderMan Interface — C/C++ Binding Example

`colorcircles.c` demonstrates how to drive openRender (or any RenderMan-compatible
renderer) through the C `Ri*` API. It generates a 101-frame animation of colored
spheres arranged in a 3-D grid and rotating around the Z axis.

## Prerequisites

| Requirement | macOS | Linux |
|-------------|-------|-------|
| Compiler | Xcode Command Line Tools (`clang`) or Homebrew `gcc` | `clang` or `gcc` |
| openRender build | `cmake --build build --config Release` from project root | same |

The Makefile links against `openrender/lib/libri` and includes headers from
`openrender/include`, both produced by the project build.

## Building

Run `make` from **this directory** (`examples/bindings/c-c++/`):

```bash
make
```

The Makefile auto-detects the OS and prefers `clang`; to override the compiler:

```bash
make CC=gcc
```

This produces the `colorcircles` executable in the current directory. The binary
embeds a relative rpath so it can find `libri` without setting `DYLD_LIBRARY_PATH`
or `LD_LIBRARY_PATH`.

To remove the built binary:

```bash
make clean
```

## Running

`colorcircles` takes a single argument: either a RIB filename or `#` to pipe output
directly to the renderer.

**Write RIB to a file** (inspect or post-process the scene):

```bash
./colorcircles scene.rib
```

**Pipe directly to openRender** (render all 101 frames in one shot):

```bash
SHADERS="$(pwd)/../../../openrender/shaders" \
ORENDERHOME="$(pwd)/../../../openrender" \
DISPLAYS="$(pwd)/../../../openrender/displays" \
GEOMETRIES="$(pwd)/../../../openrender/geometry" \
./colorcircles '#'
```

Each frame is written to `colorSpheres.NNN.tif` (1280 × 720) in the working directory.

## How it works

`ColorSpheres(n, s)` iterates over an *n×n×n* grid and maps the (x, y, z) grid
position to an RGB color, placing a scaled sphere at each cell. The outer loop in
`main()` animates 101 frames: each frame adds a 15° Z rotation and slightly shrinks
the spheres, producing a spiral-collapse effect.

The call to `RiBegin(renderer)` with a filename argument opens a RIB stream;
passing `"#"` connects to the renderer's stdin pipe — the standard RenderMan
mechanism for live rendering.
