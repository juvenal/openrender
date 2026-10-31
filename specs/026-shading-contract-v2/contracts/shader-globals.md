# Contract: `ShaderGlobals`, ray type, hit mode (Shading)

**Decisions**: every plan question referenced here (Q1-Q14) was decided on 2026-10-30. The maintainer accepted each recommended answer; they are recorded in spec.md, Clarifications, "Session 2026-10-30 (plan-stage decisions)". Where this file says "(Qn)", the design described is the decided one.

**Requirements**: FR-015 to FR-018; SC-006.
**Files**: `src/libshader/shading/shaderGlobals.h` and `src/libshader/shading/situation.{h,cpp}`
(new).

Nothing in the render path produces or consumes these in 026 (FR-018). Only the test doubles and
`ShadingContract_Situations` do. Spec 029 switches `shade()` to the structure, and spec 049 adapts it
to OSL's.

## `shaderGlobals.h`

```cpp
#include "common/sampling.h" // openrender::sampling::SampleKey
#include "serviceTypes.h"    // ObjectHandle, ShaderHandle

namespace openrender::shading {

enum class BatchShape : uint8_t { Grid, Span, Point };
enum class RayType : uint8_t { None, Camera, Secondary, Shadow, Photon };  // append only
enum class HitMode : uint8_t { Shade, Primitive };                         // append only
enum class TransportMode : uint8_t { Radiance, Importance };               // append only
enum class ShadingPass : uint8_t { Full, DisplaceOnly, DisplaceEstimate }; // append only

// One per-point field: element i of point k is data[k * stride + i]. stride 0: one value for
// every point. data == nullptr: the field is absent; test present() before reading.
template <class T> struct Strided {
    const T *data = nullptr;
    int stride = 0;
    bool present() const { return data != nullptr; }
    const T *at(int point) const { return data + point * stride; }
};

// A batch of shading points: a grid (u fastest), a span of independent points, or one point.
// Structure of arrays with a runtime point count; no fixed batch width. Owns nothing.
// Version: kRendererServicesVersion (rendererServices.h).
struct ShaderGlobals {
    // ---- shape
    int numPoints = 0;          // real points
    int numDerivativeTail = 0;  // 0, or 2 * numPoints: the du-offset then dv-offset copies
    BatchShape shape = BatchShape::Point;
    int gridU = 0, gridV = 0;   // Grid only; gridU * gridV == numPoints
    const int *activeTags = nullptr; // 0 = active (tags[]); nullptr: every real point is active

    // ---- per batch (always present)
    ObjectHandle object;
    ShaderHandle shader;
    RayType rayType = RayType::None;
    HitMode hitMode = HitMode::Shade;
    TransportMode transport = TransportMode::Radiance;
    ShadingPass pass = ShadingPass::Full;
    int pathDepth = 0;               // ray levels below the camera (CShadingContext::currentRayDepth)
    float shadingCellFootprint = 1;  // pixels: CAttributes::shadingRate, already sqrt(area)
    float shutterOpen = 0, dtime = 0;

    // ---- per point (presence: the table below)
    Strided<float> P, N, Ng, I;      // stride 3
    Strided<float> u, v, du, dv;     // stride 1; du/dv are the parametric steps
    Strided<float> rayWidth;         // stride 1; da * t + db of the ray that hit
    Strided<float> dPdu, dPdv;       // stride 3
    Strided<float> dPdx, dPdy;       // stride 3; reserved: no producer fills them in 026
    Strided<float> time;             // stride 1
    Strided<float> dPdtime;          // stride 3
    Strided<openrender::sampling::SampleKey> sampleKey; // stride 1
};

} // namespace openrender::shading
```

## Presence rules

These are the rules from [../data-model.md](../data-model.md#per-point-fields-and-presence-rules),
restated in the header comment.

- **Always present**: `P`, `time` and every per-batch field. A consumer may assume nothing else.
- **The derivative tail** follows the real points and is never active. Its `sampleKey` is the real
  point's key.
- **Inactive lanes** hold unspecified values.

## `situation.h`: the derivation as a pure function

```cpp
namespace libshader {

struct ShadingSituation {
    int dim;              // EShadingDim
    int displaceOnly;     // shade()'s argument: FALSE, TRUE, 3
    int inShadow;         // CShadingContext::inShadow
    int rayDepth;         // currentRayDepth
    bool illuminationHook;
    bool rasterHider;     // the frame's hider rasterises (reyes, zbuffer)
    char cameraHitMode, specularHitMode, transmissionHitMode; // 's' or 'p'
};

struct ShadingClass {
    openrender::shading::RayType rayType;
    openrender::shading::HitMode hitMode;
    openrender::shading::TransportMode transport;
    openrender::shading::ShadingPass pass;
};

ShadingClass classifyShading(const ShadingSituation &s);

} // namespace libshader
```

Called only by tests (FR-018).

## Situation table

The rows reproduce the code at `d01da47`. "Hit mode read" is the attribute `shade()` actually tests,
with its location.

| # | Situation | Determined by (code) | Ray type | Hit mode read | Transport | Pass |
|---|---|---|---|---|---|---|
| 1 | REYES/zbuffer grid, camera | `reyes.cpp:1061, 1115` -> `shade(..., SHADING_2D_GRID, ...)` | `None` | `camerahitmode`, applied **after** the shaders: `"p"` overwrites `Oi` with the attribute opacity and keeps `Ci` (`shading.cpp:1374-1382`) | Radiance | Full |
| 2 | REYES points/curves (0-D) | `reyes.cpp:907, 968` -> `SHADING_0D` | `None` | none (always `Shade`; `camerahitmode` ignored) | Radiance | Full |
| 3 | Raytrace camera ray hit | `raytracer.cpp:652` `trace(&primaryBundle)` at depth 0 -> `CSurface::shade` (`geometryDispatch.cpp:405`, `SHADING_2D`) | `Camera` | `specularhitmode` (`shading.cpp:915`), **not** `camerahitmode` | Radiance | Full |
| 4 | Secondary hit (`trace`, ray-traced `environment`, `gather`, reflection/refraction) | `traceEx` -> depth >= 1, `!inShadow`, `SHADING_2D` | `Secondary` | `specularhitmode` (`:915`); `diffusehitmode` is parsed but never read (`rendererContext.cpp:3701-3704`) | Radiance | Full |
| 5 | Shaded shadow/transmission hit (`transmission()`, ray-traced `shadow`) | `traceTransmission` sets `inShadow` (`executeMisc.cpp:717`) -> `shade()` | `Shadow` | `transmissionhitmode` (`shading.cpp:889`): `"p"` copies the attribute opacity and returns; `"s"` runs the surface only (`:907`) | Radiance | Full |
| 6 | Implicit-surface ray hit | `implicitSurface.cpp:258` -> `SHADING_0D` | `Camera` (depth 0) / `Secondary` / `Shadow` | none (0-D skips the specular test) | Radiance | Full |
| 7 | DSO-object ray hit | `dlobject.cpp:176` -> `shade(this, n, -1, SHADING_2D, 0)` (`vVertices = -1`: a defect, filed as O2, research R13.4) | as rows 3-5 | as rows 3-5 | Radiance | Full |
| 8 | Displacement-only pass (dicing; ray tessellation) | `reyes.cpp:790, 870, 936, 1047, 1104`; `surface.cpp:1706-1794` (`PARAMETER_RAYTRACE`) -> `displace()` | `None` | none | Radiance | DisplaceOnly |
| 9 | Displacement bound or grid-size estimate | `surface.cpp:304, 332, 334`; `geometryDispatch.cpp:455, 492` -> `displaceEstimate()` (`displaceOnly = 3`) | `None` | none | Radiance | DisplaceEstimate |
| 10 | Photon emission (light shaders) | `photon.cpp:166-230`: phony object, `SHADING_0D`, illumination hook on; no `shade()` | `Photon` | none | Importance | Full |
| 11 | Photon hits a surface | `photon.cpp:609-700`: shading model from the attributes; **no shader runs** | not a shading situation | | | |
| 12 | Imager | `rendererDisplay.cpp:315` -> `imager.cpp:43`: no object, no `shade()` | `None` | none | Radiance | Full |
| 13 | Probe rays (`visibility()`, probe `transmission()`, `occlusion`/`indirectdiffuse` rays, the core's hard-coded lights) | `trace(CRay *)` only | not shaded: the ray's type is `Shadow` (visibility/transmission) or `Secondary` (occlusion/indirect) for spec 029's tracer | | | |
| 14 | `RSLShading::shade()` façade, bare execution | `RSLShading.cpp` | `None` | none | Radiance | Full |

Further rules the table relies on:

- **The atmosphere** runs only at depth 0 (`shading.cpp:1361`); a matte object runs none (`:933-936`).
- **Derivatives**: a `SHADING_2D` hit with `PARAMETER_DERIVATIVE` gets the derivative tail
  (`:1065-1071`); `SHADING_0D` never does (`:1063`).

`ShadingContract_Situations` checks rows 1 to 5, 8 and 9 against an in-process render (research R13). Rows 6 and 7 (implicit-surface and DSO-object
hits, whose present behaviour is an oddity, O5 #119 and O2 #116) and rows 10-14 are checked against `classifyShading()` alone (B8).
