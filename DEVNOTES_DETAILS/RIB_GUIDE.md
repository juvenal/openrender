# RIB Output Guide

## Overview

openRender provides a standardized RIB output mechanism through the `CRibOut` class. Recent updates have unified the initialization logic and added automatic emission of standard RenderMan RIB headers.

## Standard RIB Headers

When a RIB output is initialized (either via a filename or an existing file stream), the renderer now automatically emits a standard preamble to ensure compliance with the RenderMan RIB-Structure 1.1 specification.

### Example Header

```rib
##RenderMan RIB-Structure 1.1
##Creator openRender 1.0.0
##CreationDate Wed May 20 19:53:31 2026
```

- **RIB-Structure**: Declares compliance with the RIB structure 1.1.
- **Creator**: Identifies the renderer and its version.
- **CreationDate**: Records the timestamp when the RIB was generated.

## Implementation Details

### Unified Initialization

The `CRibOut` constructors have been refactored to use a shared `completeInit()` method. This ensures that the RIB preamble is consistently emitted regardless of whether the output is a file or a pipe/stdout.

### Language Bindings Parity

The **Python** and **Lua** bindings have also been updated to match this behavior. When the `Begin()` method is called in these bindings, the same standard header is written to the output stream, ensuring that RIB files generated from scripts are identical in structure to those generated from C++ or the core renderer.

## Usage

No changes are required from the user side. The headers are emitted automatically upon calling `RiBegin` or its equivalent in the language bindings.

## Option "sampling" (spec 039)

```rib
Option "sampling" "string sampler" ["sobol"]   # "sobol" (default), "pmj02" or "independent"
Option "sampling" "int seed" [0]               # any integer, default 0
```

- The sampler supplies every random number of the render: the camera samples (film jitter, time,
  lens, importance) through camera slots indexed by the absolute subpixel and the frame, and
  every stochastic shading call through a sub-stream of (point, shader, call site, occurrence,
  ray depth). No generator lives in a shading context any more.
- `"legacy"` (internal before Phase 9) is now an unknown name. An unknown name raises
  `CODE_BADTOKEN` (`Unknown sampler "<name>" (valid: sobol, pmj02, independent); keeping the
  current sampler`) and keeps the current value: an earlier valid setting survives, it is not
  reset to the default (GitHub #76, pinned by `SamplingOption_UnknownKeepsCurrent`).
- Shaders query it with `option("sampling:sampler", s)` / `option("sampling:seed", n)`.
- Non-GI renders are bit-identical at any `-t`/`-P`, and a frame rendered alone (`-f n`) equals
  the same frame of the full sequence. GI caches (irradiance cache with `maxerror` > 0, photon
  map) are deterministic too since spec 040; see "Global-illumination determinism" below.
- **Limitation (FR-021):** a uniform `random()` under the raytrace hider is keyed by the shading
  batch, which depends on the bucket, so it MAY change when the bucket size changes.
- A `CropWindow` draws the same samples for a pixel as the full frame (the hiders key subpixels by their position in the full image). A raytrace crop is bit-identical to the same pixels with a 1 x 1 box filter; a reyes crop is not (its dicing changes, GitHub #72), and the raytrace default filter's crop corners are GitHub #70.
- The 8-bit display dither is a hash of (x, y, channel) in `file_base.cpp`.
- User documentation: `docs/site/content/manual/reference/options.md` ("Sampling options"),
  `multithreading.md`, `network-parallel-rendering.md`, `sl-functions.md`.

### Global-illumination determinism (spec 040)

- **The photon map** is the same on every run and at every thread count (`-t:n`): the photons are sorted into a canonical order
  (key, then the bytes of the photon) when the map is balanced, every byte of a written `.gpm` is defined, and a lookup is a pure
  function of the map and the query (the old lookup memo is gone: a render that reads the photon map through `photonmap()` or
  `indirectdiffuse()` is slower than before, GitHub #178 is the follow-up).
- **The irradiance cache** (`occlusion()` and `indirectdiffuse()` with `maxerror` > 0): what a lookup sees is the records read from
  the cache file (`filemode "r"` or `"rw"`) plus the records made earlier by the same shading call (a REYES or zbuffer grid, a raytrace
  shading group). Nothing else is visible until the frame ends, so the image does not depend on the order the grids are shaded in. At
  the end of the frame the records are merged, clamped and written in a canonical order (equal keys commit once), so the `.icache`
  file is byte-identical across runs, thread counts and `-P:n`.
- **The directions** a miss traces are drawn by sample index from the point's own sub-stream: `occlusion()` and `indirectdiffuse()`
  with the cache off take `samples` keyed directions, with the cache on the `nt x np` grid of jittered cells (the gradients need it).
  The accept decision of a cached record is a hash of the call's key and the record's identity, not a draw.
- **Cost:** reusing a cache that an earlier frame wrote (`filemode "r"`), at a fine shading rate, costs about twice as much per later
  frame as it did before spec 040: the cache holds many more, smaller-batch records (8 times as many in the measured scene) and each
  lookup examines more of them (GitHub #179). A single frame is not affected. The two-frame idiom is therefore not recommended as a
  speed-up here until #179 is resolved.
- **Known gap:** under the interpreter (`.rslo`), `indirectdiffuse()` traces no ray (GitHub #173); the `.slo` shaders do.

## Option "imageio" (spec 048)

```
Option "imageio" "string backend" [ "legacy" ]   # default
Option "imageio" "string backend" [ "oiio" ]     # OpenImageIO, in a build with OPENRENDER_ENABLE_OIIO
MakeTexture "in.png" "out.tex" "periodic" "periodic" "gaussian" 2 2 "string imageio" [ "oiio" ]
Display "beauty.exr" "oiio" "rgba"
```

The Option selects the library that decodes sources and bakes textures from that point; the per-call `"imageio"` parameter (also on `MakeShadow`,
`MakeLatLongEnvironment`, `MakeCubeFaceEnvironment`) wins over it; `Display ... "oiio"` is the display driver. An unknown value reports
`Unknown image backend "<name>" (expected "legacy" or "oiio")` and keeps the previous one. See [IMAGE_IO.md](IMAGE_IO.md).

## AreaLightSource and bound area lights (spec 057)

`AreaLightSource name handle ...` makes a light and **binds** every primitive created while it is current: the geometry in its
attribute block is the emitter. User documentation: `docs/site/content/manual/reference/area-lights.md`.

```rib
AttributeBegin
    Attribute "arealight" "int samples" [16]
    AreaLightSource "arealight" 1 "float intensity" [50]
    Surface "constant"
    Polygon "P" [3 5 -2  5 5 -2  5 7 -2  3 7 -2]     # emits into the side of its normal
AttributeEnd
Illuminate 1 1                                       # the light list is an attribute: off after the block
AttributeBegin
    Attribute "visibility" "int transmission" [1]   # FR-023: only flagged objects cast area-light shadows
    Surface "matte"
    Sphere 1 -1 1 360
AttributeEnd
```

- **Scope rules.** The current area light is a slot on the attributes (an opaque handle: Geometry never sees the light). `AttributeBegin`
  inherits it, `AttributeEnd` clears it, `AreaLightSource "null"` clears it inside the block, and a light statement that fails
  (a missing shader, a wrong-type shader) closes the scope and leaves no pending instance key. `Illuminate handle 0|1` switches it.
- **Controls.** `Option "arealight" "int samples" [n] "string mode" ["full"|"fast"|"off"]` (the scene) and the same under
  `Attribute "arealight"`: the attribute in force at the `AreaLightSource` statement is the light's, the one in force on the lit
  object is the object's. Most specific first (object, light, scene), `samples` and `mode` separately; `off` at any level wins;
  `fast` is `max(1, ceil(n / 4))`; the default is 16 and `full`. A bad token or value is `CODE_BADTOKEN`/`CODE_RANGE`, one
  message, the setting unchanged. They round-trip through `ribOut`.
- **The FR-023 rule.** An object occludes a bound area light's samples only with `Attribute "visibility" "int transmission" [1]`
  (the flag `visibility()`, `transmission()` and ray-traced `shadow()` use everywhere); `transmission 0` is the opt-out. Nothing
  else shadows, and no scene without area lights changes.
- **Emitters stay geometry.** The camera sees an emitter and its pixels are its own `Surface`'s (FR-025); the renderer adds no
  radiance. A bound light does not light its own emitter primitives, and its emitters never block its samples (exclusion by the
  emitter set's identity, in the three intersection gates of `surface.cpp`, reached by the polygon, quadric, patch and subdivision
  intersectors). A primitive of an unsupported kind inside the scope (points, curves, implicit surfaces, DSO objects) is not one of
  the light's emitters and does not pass through those gates; the raytrace hider does not intersect points and curves (GitHub #195), so they cast no
  shadow on any light's probe, area or classic, inside the scope or outside it (scratch renders, validation.md section 34), and
  only implicit surfaces and DSO objects could be seen blocking their own light (not rendered). A blobby emits, through the
  polygon mesh `RiBlobby` derives from it (`AreaLightProbe_BlobbyIrradiance_*`).
- **Supported kinds.** Polygons, polygon meshes, subdivision surfaces and blobbies (a derived mesh); the seven quadrics; bilinear and bicubic patches, patch
  meshes and NURBS patches with trims; the leaves of procedurals, delayed archives, instances and CSG solids (whole, without the
  solid's region). The others (points, curves, implicit surfaces, DSO objects) are reported once per light and stay ordinary geometry. Displacement is not applied to an emitter.
  A bound light with no emitting geometry contributes nothing (one warning).
- **Motion.** An emitter inside `MotionBegin` is sampled at the shutter time of the point being shaded (assumption A8). The raytrace hider gives every
  pixel sample its own time, so a moving emitter is blurred there; reyes and zbuffer shade a surface once per key pose of that surface (`reyes.cpp`:
  time 0, and time 1 for a moving grid), so a static receiver sees a moving emitter at its shutter-open pose (GitHub #196; `AreaLight_EmitterMotion`,
  `AreaLight_EmitterMotion_GridTime`).
- **Photons.** `Hider "photon"` emits a bound light's photons from points of its emitters, along the emitter's normal side; the
  photon map is not a parity subject.

## `Bxdf` and `closuremode` (spec 035)

```rib
Option "shade" "string closuremode" [ "both" ]          # the scene's mode: color (the default), both or closures
WorldBegin
    AttributeBegin
        Attribute "shade" "closuremode" [ "closures" ]   # the object's mode wins over the scene's
        Bxdf "plastic" "float roughness" [ 0.2 ] "color specularcolor" [ 1 1 1 ]
        Sphere 1 -1 1 360
    AttributeEnd
    Bxdf "dielectric" "glass1" "float ior" [ 1.5 ]       # an optional second string is the handle
    Sphere 1 -1 1 360
WorldEnd
```

- **`Bxdf "name" ["handle"] <parameter list>`** binds one of the built-in Bxdfs (`diffuse`, `plastic`, `conductor`, `dielectric`) in the
  current attribute block. Of `Surface` and `Bxdf`, the one bound **last** governs the block; `AttributeEnd` brings back the enclosing
  choice. The handle is optional. Without one, a first parameter whose value is a bare string must be written in brackets
  (`"string x" ["y"]`), or the parser reads it as the handle (the `LightSource` rule). The statement draws nothing: it takes no instance key,
  so no later shader's random stream changes.
- **Not valid inside `ObjectBegin`/`ObjectEnd`** (RISpec 3.2 section 5.10: no shader assignment in an object definition): the statement is
  refused as any out-of-block request, and `Surface` and `Attribute` there behave as before. An instance takes the binding current at
  `ObjectInstance`.
- **Names.** `sl:` and `osl:` are reserved for RSL 2.0 and OSL Bxdfs (specs 036, 049); an unknown name, a refused interface MAJOR and a
  refusing parameter (a negative `plastic` `ior`, a `dielectric` `ior` of 0 or less) are errors that name the reason and leave the
  binding unchanged. An unknown parameter (`Ka` is not a parameter) is skipped with an error; an out-of-range one is clamped with a
  `CODE_RANGE` message. A `color` parameter follows `ColorSamples`.
- **`Option`/`Attribute "shade" "string closuremode"`** takes `color` (today's behaviour), `both` or `closures`; a bad value is a
  `CODE_RANGE` error and the previous value stays. The attribute wins over the option. A block, a `SolidBegin` and an `ObjectBegin` save and
  restore it like any attribute.
- **Interim behaviour** (until specs 036, 041 and 042). Shipped shaders are not closure capable, so under `both` or `closures` a surface
  renders as `color`, with one warning per (shader, mode) per frame. A governing Bxdf with no consumer renders the default surface (the one
  `WorldBegin` binds; the no-shader path when a `Surface` was given before `WorldBegin`), never a user `Surface`, with one warning per Bxdf
  name per frame. See `CLOSURES.md`.
- `ribOut` writes both statements at their positions; the Python and Lua helpers write `Bxdf` (see `BINDINGS_GUIDE.md`).
