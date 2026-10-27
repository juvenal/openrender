# Data Model: Camera and Motion Layer

**Plan**: [plan.md](plan.md) | **Research**: [research.md](research.md)

All new types are in namespace `openrender`. Line numbers are on master `4eb683b`.

## FrameParams (Graphics State, `src/ri/state/frameParams.h`)

The immutable per-frame set of shutter, raster, lens and camera-motion values (FR-001). A plain
aggregate of floats, ints, bools, `matrix` and `quaternion` (`common/algebra.h`), trivially
copyable, with no pointer and no method beyond `const` accessors.

| Field | Type | Copied from (`CRenderer::`) | Set at | Readers in 028 |
|---|---|---|---|---|
| `sampling` | `sampling::CSamplingConfig` | the `beginFrame` local `samplingConfig` (`renderer.cpp:775`) | `renderer.cpp:775` | camera (`screenLeft`, `screenTop`) |
| `shutterOpen`, `shutterClose` | `float` | `shutterOpen`, `shutterClose` | `copyOptions` | (none; FR-001 completeness, #107) |
| `shutterTime` | `float` | `shutterTime` | `renderer.cpp:459` | (none) |
| `invShutterTime` | `float` | `invShutterTime` (1 when the shutter is empty) | `renderer.cpp:460-463` | `motion::shutterRate()` |
| `projection` | `int` | `projection` (`OPTIONS_PROJECTION_*`) | `copyOptions` | camera |
| `imagePlane`, `invImagePlane` | `float` | `imagePlane`, `invImagePlane` | `renderer.cpp:347-383` | camera |
| `dxdPixel`, `dydPixel` | `float` | `dxdPixel`, `dydPixel` | `renderer.cpp:405-406` | camera |
| `aperture` | `float` | `aperture` (0 for orthographic or below `C_EPSILON`) | `renderer.cpp:438-451` | camera |
| `focaldistance`, `invFocaldistance` | `float` | `focaldistance`, `invFocaldistance` | `copyOptions`, `renderer.cpp:450` | camera, lens model |
| `focallength`, `fstop` | `float` | `focallength`, `fstop` | `copyOptions` | lens model |
| `camera.fromWorld`, `camera.toWorld` | `matrix` | `fromWorld`, `toWorld` (shutter open) | `renderer.cpp:294-295` | `motion::transformRay` (`toWorld`) |
| `camera.fromWorld1`, `camera.toWorld1` | `matrix` | `fromWorld1`, `toWorld1` (shutter close) | `renderer.cpp:300-301, 332-333` | (none in 028; spec 031) |
| `camera.relRotQ` | `quaternion` | `relRotQ` | `renderer.cpp:315-316` | `motion::transformRay` |
| `camera.relTrans` | `vector` | `relTrans` | `renderer.cpp:310-312` | (none in 028; spec 031) |
| `camera.hasMotion`, `camera.hasRotation`, `camera.rotationOnly` | `bool` | `cameraHasMotion`, `cameraHasRotation`, `cameraRotationOnly` | `renderer.cpp:302-336` | `motion::transformRay` (`hasRotation`) |

**Validation rules**:
- Every field equals the static it is copied from, bit for bit (`Frame_ParamsEqualCore`).
- No field duplicates a `CSamplingConfig` field (FR-002): the screen-window origin, crop origin,
  `nx`/`ny`, jitter, frame and seed are only in `sampling`.
- `invShutterTime` is the stored reciprocal, never recomputed from `shutterTime` by a reader
  (decision 5).

**Default value** (nothing installed): `invShutterTime = 1`, `projection = OPTIONS_PROJECTION_PERSPECTIVE`,
`imagePlane = invImagePlane = 1`, `aperture = 0`, identity matrices, identity `relRotQ`, every flag
false, `sampling` as `CShadingContext`'s default (`shading.h:894`). It exists so a Geometry call
outside a frame reads defined values; no rendering path reaches it.

### Lifecycle

```text
RiWorldBegin -> CRenderer::beginFrame
                 ... statics computed (renderer.cpp:270-517) ...
                 samplingConfig built (renderer.cpp:775)
                 beginFrameParams(samplingConfig)   // rendererFrame.cpp: copy statics, install, build CFrameCamera
                 contexts created: new CRaytracer(i, frameCamera()) ...
   render threads read frameParams() and the camera (read-only, no lock)
RiWorldEnd  -> CRenderer::endFrame
                 contexts deleted
                 endFrameParams()                   // uninstall, delete the camera
```

Installed for exactly one frame. A second `RiBegin`/`RiEnd` session installs a fresh value.

## Frame-parameter slot (Graphics State, `src/ri/state/frameParams.{h,cpp}`)

| Function | Meaning |
|---|---|
| `void installFrameParams(const FrameParams *fp)` | the renderer core installs the frame's value; `nullptr` uninstalls. Not thread-safe; called only before render threads start and after they stop. |
| `const FrameParams &frameParams()` | the installed value, or the default one. Inline, one pointer load. |

## Camera types (Graphics State, `src/ri/state/camera.h`)

| Type | Fields / role |
|---|---|
| `CameraSample` | `x`, `y` (full-image pixels), `time` (shutter fraction), `lensU`, `lensV` (unit disk). §7.4's `CameraSample`. |
| `CameraRay` | `from[3]`, `dir[3]` (camera space, `dir` normalized), `time`, `da`, `db` (ray differentials, as `CRay`). |
| `LensModel` | `projection`, `imagePlane`, `aperture` (radius), `focaldistance`, `invFocaldistance`, `focallength`, `fstop`; `bool thinLens() const` (`aperture != 0`); `float cocFactorScreen() const` (`renderer.cpp:447`'s expression, for spec 031). |
| `CameraSupport` | `enum class { Supported, NotSupported }`. |
| `CameraImportance`, `CameraConnectionSample` | declared, not defined (reserved, §7.4; defined by the BDPT spec). |
| `ICamera` | the interface; see [contracts/camera.md](contracts/camera.md). |
| `CFrameCamera final : ICamera` | built from one `FrameParams`, holds a copy of the camera fields it reads; perspective or orthographic, pinhole or thin lens. |

## Motion keys and the evaluator (Geometry, `src/ri/geometry/motion.h`)

**Motion keys**: an object's two states, shutter open (`t = 0`) and shutter close (`t = 1`). Each
family keeps its own storage; the evaluator does not change it:

| Family | Key storage | Moving when | Key 1 address |
|---|---|---|---|
| quadrics (7) | parameter block | `nextData != NULL` | `nextData[k]` |
| quadrics, transform motion | `CXform` | `xform->next != NULL` | `xform->next->from/to` |
| bilinear patch | interleaved vertices | `variables->moving` | `vertex + 4 * vertexSize` |
| bicubic, NURBS patches (`sample()`) | interleaved vertices | `variables->moving` | `vertex + vertexSize * N` |
| polygon triangle, quad | separate arrays | `moving` / vertex arrays | `vertices + v * 3` |
| tessellation patch | key grids | `thisTesselation` keyed | `Pt0 + 4 * 3`, `cP0 + (div + 1)^2 * 3` |
| curves, points | interleaved vertices | `variables->moving` | `+ vertexSize` |
| patch grid, B-spline patch grid | key grids | `variables->moving` | `vertex + vertexSize * grid` |
| subdivision (Q2) | key grids | `vertex` keyed | `vertex + vertexSize * K` |
| implicit surface | none (the plug-in's time) | always | n/a |
| `CDLObject` | `CXform` only | `xform->next != NULL` | `xform->next->to` |

**Evaluator state**: none. Every function is a pure function of its arguments and of
`frameParams()`.

**Invariants** (pinned by `Motion_TwoKeyModel`):
- `atTimeD(k0, k1, 0) == k0` and `atTimeD(k0, k1, 1) == k1` for finite keys (compared with `==`;
  a `-0.0` key may come back as `+0.0`, as today).
- `atTimeF == atTime<float>`, `atTimeD == (float)atTime<double>` and `atTimeV` equals three
  `atTimeD`, bitwise, over a dense set of keys and times including `t < 0` and `t > 1`
  (extrapolation, no clamp).
- `atTimeD` and `atTimeV` equal verbatim copies of the starting tree's quadric expression and of
  `interpolatev`, bitwise; `atTimeF` equals a copy of `subdivision.cpp:186`'s expression.
- `keyAtTime(scratch, k0, nullptr, t) == k0` (pointer identity: a static object is returned
  unchanged).
- `toShutterRate(v)` equals `mulvf(v, CRenderer::invShutterTime)` because `FrameParams` holds the
  same reciprocal.

## State transitions

None. `FrameParams` and the camera are immutable from install to uninstall, and the evaluator is
stateless.

## Relationships

```text
renderer core ──builds/installs──▶ FrameParams (State) ◀──reads── motion.h (Geometry) ◀──calls── primitives
      │                                  │
      └──builds──▶ CFrameCamera (State) ─┘ copies camera fields
                         ▲
CRaytracer (Hiders) ─────┘ holds const ICamera &, calls generateRay per camera sample
```

Edges added: Core -> State, Core -> Geometry (`motion.h` is not needed by the core), Geometry ->
State, Hiders -> State. All are allowed by the table (Core -> *, Geometry -> GraphicsState, Hiders ->
GraphicsState).
