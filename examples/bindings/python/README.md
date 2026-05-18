# RenderMan Interface — Python Binding Example

`colorcircles.py` demonstrates how to drive openRender (or any RenderMan-compatible
renderer) through the Python `Ri` API. It generates a 101-frame animation of colored
spheres arranged in a 3-D grid and rotating around the Z axis.

## Prerequisites

- **Python 3** (No external dependencies required)

The script automatically configures its `sys.path` to include `src/python/prman.py`, so no special installation steps are needed.

## Running

`colorcircles.py` takes a single argument: either a RIB filename or the name of a renderer executable to pipe output directly to it.

**Write RIB to a file** (inspect or post-process the scene):

```bash
./colorcircles.py scene.rib
```

**Pipe directly to openRender** (render all 101 frames in one shot):

```bash
SHADERS="$(pwd)/../../../openrender/shaders" \
ORENDERHOME="$(pwd)/../../../openrender" \
DISPLAYS="$(pwd)/../../../openrender/displays" \
GEOMETRIES="$(pwd)/../../../openrender/geometry" \
./colorcircles.py orender
```

Each frame is written to `colorSpheres.NNN.tif` (1280 × 720) in the working directory.

## How it works

`color_spheres(ri, n, s)` iterates over an *n×n×n* grid and maps the (x, y, z) grid
position to an RGB color, placing a scaled sphere at each cell. The outer loop in
`main()` animates 101 frames: each frame adds a 15° Z rotation and slightly shrinks
the spheres, producing a spiral-collapse effect.

The call to `ri.Begin(renderer)` with a filename argument opens a RIB stream;
passing `"orender"` launches the renderer as a subprocess and connects to its stdin pipe — a standard mechanism for live rendering directly from Python.
