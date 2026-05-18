# RenderMan Interface — Lua Binding Example

`colorcircles.lua` demonstrates how to drive openRender (or any RenderMan-compatible
renderer) through the Lua `Ri` API. It generates a 101-frame animation of colored
spheres arranged in a 3-D grid and rotating around the Z axis.

## Prerequisites

- **Lua 5.1+** or **LuaJIT** (No external dependencies required)

The script automatically configures its `package.path` to include `src/lua/prman.lua`, so no special installation steps are needed.

## Running

`colorcircles.lua` takes a single argument: either a RIB filename or the name of a renderer executable to pipe output directly to it.

**Write RIB to a file** (inspect or post-process the scene):

```bash
./colorcircles.lua scene.rib
```

**Pipe directly to openRender** (render all 101 frames in one shot):

```bash
SHADERS="$(pwd)/../../../openrender/shaders" \
ORENDERHOME="$(pwd)/../../../openrender" \
DISPLAYS="$(pwd)/../../../openrender/displays" \
GEOMETRIES="$(pwd)/../../../openrender/geometry" \
./colorcircles.lua orender
```

Each frame is written to `colorSpheres.NNN.tif` (1280 × 720) in the working directory.

## How it works

`ColorSpheres(ri, n, s)` iterates over an *n×n×n* grid and maps the (x, y, z) grid
position to an RGB color, placing a scaled sphere at each cell. The outer loop in
`main()` animates 101 frames: each frame adds a 15° Z rotation and slightly shrinks
the spheres, producing a spiral-collapse effect.

The call to `ri:Begin(renderer)` with a filename argument opens a RIB stream;
passing `"orender"` launches the renderer as a subprocess and connects to its stdin pipe — a standard mechanism for live rendering directly from Lua.
