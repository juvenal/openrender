# Contract: `shade()`'s input: `ShaderGlobals`, the point source and the producer (Shading, Hiders)

**Status**: plan-stage proposal, assuming the recommended answers to Q1, Q3, Q4 and Q5
([../research.md](../research.md) R1, R8, R9, R10).

**Requirements**: FR-018 to FR-022, FR-015; US7; SC-005, SC-008.
**Files**: new `src/libshader/shading/pointSource.h`, `src/libshader/shading/shadingProducer.h`; edited
`shaderGlobals.h`, `situation.cpp`, `shading.{h,cpp}`; Hiders: `surfacePointSource.h`, `shadingProducer.cpp`.

## `pointSource.h` (Shading-owned, header-only)

```cpp
class CAttributes;  // Graphics State (Shading may depend on it)
class CSurface;     // opaque: never dereferenced by Shading

namespace openrender::shading {
    // What shade() asks of the primitive it shades (spec 029, Q1 option B of the specify stage). Implemented in
    // Hiders over CSurface (CSurfacePointSource). Version: kRendererServicesVersion; introduced in 3.0.0.
    // Thread-safety: one per shading call, on the calling thread's stack. Re-enters Shading: never.
    class IPointSource {
        public:
            virtual ~IPointSource() = default;
            virtual const CAttributes *attributes() const = 0;   // the primitive's attributes, never null
            virtual uint64_t stableId() const = 0;               // spec 039's stable identity (cell keys)
            virtual const CSurface *surface() const = 0;         // opaque, for the observers' reports
            virtual unsigned int requestedParameters() const = 0; // PARAMETER_* the caller needs sampled
            // CSurface::sample(start, count, varying, locals, parameters): fills the varyings of points
            // [start, start + count); reads and may change `parameters`, as today (shading.cpp:1074-1075).
            virtual void sample(int start, int count, float **varying, float ***locals, unsigned int &parameters) const = 0;
            // CSurface::interpolate(count, varying, locals): the primitive variables into the shader locals.
            virtual void interpolate(int count, float **varying, float ***locals) const = 0;
    };
}
```

## `shadingProducer.h` (Shading-owned, header-only)

```cpp
namespace openrender::shading {
    // The target of CShadingContext's Geometry-facing forwarding entry, shade(CSurface *, ...) (Q1 (a)).
    // Implemented by the hider that owns the context (CHider) and installed with setProducer(). It builds the
    // point source and the batch description and calls CShadingContext::shade(const ShaderGlobals &, IPointSource &).
    // Introduced in 3.0.0. Re-enters Shading: yes, exactly once per call.
    class IShadingProducer {
        public:
            virtual ~IShadingProducer() = default;
            virtual void shadeSurface(CShadingContext &context, CSurface *surface, int uVertices, int vVertices,
                                      EShadingDim dim, unsigned int usedParameters, int displaceOnly) = 0;
    };
}
```

## `ShaderGlobals` changes (MAJOR with the rest of 3.0.0)

- `BatchShape` gains `PointSet`, appended: a 0-D point set (`SHADING_0D`: points, curves' vertices, implicit
  hits, a phony light batch). `Span` is narrowed to 2-D ray hits (`SHADING_2D`). `Grid` is `SHADING_2D_GRID`.
- The presence rules are restated per direction:
  - **input of `shade()`**: every per-batch field is set; every per-point field is absent. The per-point values
    are in the context's varyings (written by the hider or Geometry before the call: `u`, `v`, `time`, and for
    ray hits `P`, `Ng`, `I`, `du`), or are sampled by the point source inside `shade()`;
  - **a full description** (what the doubles, `ShadingContract_ShaderGlobalsDouble` and spec 049 build): the
    rules of spec 026, unchanged ("P and time always present").
- Nothing else changes; no field is removed or reordered.

## The producer's rules (`CHider::shadeSurface`, FR-019)

| Field | Value |
|---|---|
| `shape`, `gridU`, `gridV`, `numPoints` | `SHADING_2D_GRID` -> `Grid`, `gridU = u`, `gridV = v`; `SHADING_2D` -> `Span`; `SHADING_0D` -> `PointSet`; `numPoints = u * v` |
| `pass` | `displaceOnly` FALSE / TRUE / 3 -> `Full` / `DisplaceOnly` / `DisplaceEstimate` |
| `object` | `ObjectHandle{static_cast<const CObject *>(surface)}` |
| `pathDepth` | `context.rayDepth()` |
| `rayType`, `hitMode`, `transport` | `classifyShading()` of the situation built from: `dim`, `displaceOnly`, `context.inShadowRay()`, `context.rayDepth()`, `context.rendererIlluminationHook()`, `rasterizes()`, and the three hit-mode attributes of `surface->attributes` (after Q5's correction, below) |
| `shadingCellFootprint` | `attributes->shadingRate` |
| `shutterOpen`, `dtime` | from `FrameParams` (`frameParams()`, Graphics State), as spec 026's double does |

The producer calls the same `classifyShading()` the test oracle uses, so FR-019 holds by construction for the
three classified fields, and `ShadingContract_ProducerSituations` checks it on rendered scenes through a
test observer (`shadeBatchObserver`, null in production, the precedent of spec 039's `pointKeyObserver`).

## `shade(const ShaderGlobals &g, IPointSource &src)`: the branches (FR-018)

| Today (`shading.cpp`) | After |
|---|---|
| `object->attributes` (`:853`) | `src.attributes()` |
| `displaceOnly == FALSE` (`:880`), `displaceOnly == TRUE` (`:1039`), `displaceOnly & 2` (`:958`) | `g.pass == Full`, `== DisplaceOnly`, `== DisplaceEstimate` |
| `inShadow == TRUE` then `transmissionHitMode == 'p'` (`:883-898`) | `g.rayType == Shadow` then `g.hitMode == Primitive` |
| `dim == SHADING_2D && specularHitMode == 'p'` (`:912`) | `(g.rayType == Camera or Secondary) && g.hitMode == Primitive` |
| `dim == SHADING_2D_GRID && cameraHitMode == 'p'` (`:1372`) | `g.rayType == None && g.hitMode == Primitive` |
| `currentRayDepth == 0` for the atmosphere (`:1359`) | `g.pathDepth == 0` |
| `dim` elsewhere (`:1060-1062, 1152, 1224`, `fillPointKeys`) | the shape: `Grid`/`Span`/`PointSet` |
| `object->sample(...)`, `object->interpolate(...)` | `src.sample(...)`, `src.interpolate(...)`, same arguments, same order |
| `object->stableId` (`fillPointKeys`, `:809`) | `src.stableId()` |
| `currentShadingState->currentObject = object` (`:1023`) | the three fields of the state, from the batch and the source |
| `usedParameters` argument | `src.requestedParameters()`, copied into the same local before the first use |

The derivative-offset construction (`:1060-1146`) and the grid du/dv (`:1148-1219`) are not touched (FR-020).
Every row of the left column is reproduced because the producer sets `hitMode` to `Shade` wherever today's code
tests no hit mode, and `rayType` as today's branches imply (the corrected situation table below).

## `classifyShading()` corrected to the code (Q5 (a))

`shade()` never reads the illumination hook, so the hook may decide only the transport and the emission row.
Order of the rules (`situation.cpp`): the passes (rows 8, 9); then:

1. **Photon emission** (hook on, 0-D, depth 0, not in a shadow): `Photon`, `Shade`, `Importance` (row 10).
   Today's function applies the hook rule before every other rule, so a shadow or secondary ray traced by a light
   shader *during* emission classifies as `Photon` while `shade()` tests its hit modes.
2. **In a shadow** (`inShadow`): `Shadow`, `hitMode` from `transmissionhitmode` at **every** dimension
   (`shading.cpp:883-898` has no dimension test). Today's function returns `Shade` for 0-D (row 6's shadow case).
3. **A grid** (`SHADING_2D_GRID`): `None`, `hitMode` from `camerahitmode` (row 1).
4. **A 0-D batch at depth 0 in a raster frame**: `None`, `Shade` (row 2).
5. **Otherwise** (a ray hit, 2-D or 0-D, at any depth, in any frame): `Camera` at depth 0, else `Secondary`;
   `hitMode` from `specularhitmode` for 2-D, `Shade` for 0-D (rows 3, 4, 6, 7). Today's function sends every
   non-grid batch of a raster frame to rows 1/2, so a reflection traced from a REYES grid is misclassified.

`transport` is `Importance` whenever the hook is on (rows 2 to 5 included), `Radiance` otherwise; no branch of
`shade()` reads it.

The 026 situation table (`specs/026-shading-contract-v2/contracts/shader-globals.md`) is not edited (a merged
spec's record); 029's data model carries the corrected table, and the registry's DB-018 note points to it.
