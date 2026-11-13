# Contract: the Geometry-owned dicing and ray-side interfaces and the frame value

**Status**: approved 2026-11-07 (P-Q1 names, P-Q2 pointer/reference split, P-Q6 tessellation lock in Geometry,
P-Q12 frustum test in the core). **Requirements**: FR-001, FR-002, FR-003, FR-005, FR-006, FR-007, FR-015, FR-019, FR-021; SC-003.
**Files**: new `src/ri/geometry/rayContext.h`, `src/ri/geometry/dicer.h`, `src/ri/geometry/geometryFrame.{h,cpp}`,
`src/common/shadingDim.h`; edited `src/ri/hiders/hider.{h,cpp}`, `reyes.{h,cpp}`, `trace.cpp`,
`shadingProducer.cpp`, every Geometry file that names `CShadingContext` or `CReyes` (R1).

> **Plan changed in the lens phase (2026-11-12, maintainer-approved).** The frame struct below first carried `float cocFactorPixels, invFocaldistance`; the lens phase replaced
> both by `openrender::LensModel lens` (`state/camera.h`), and `minCocPixels` reads `|lens.circleOfConfusion(z)|` times the pixel size, where `circleOfConfusion(z)` is the one signed
> `imagePlane * aperture * (1/f - 1/z)` shared by Reyes and the raytracer (contracts/lens-model.md). `CRenderer::cocFactorPixels/Samples/Screen` and `FrameParams::cocFactorScreen/cocFactorSamples` no longer exist.

## `src/common/shadingDim.h` (Infra)

```cpp
// The dimensionality of a shading or displacement call: what the points of the batch are. Moved from
// libshader/shading/shading.h (spec 031) so Geometry can name it without including Shading; same values.
typedef enum {
    SHADING_0D,      // Shading points
    SHADING_2D_GRID, // Shading a 2D grid
    SHADING_2D       // Shading a 2D surface that has been arbitrarily sampled
} EShadingDim;
```

`shading.h` includes it in place of its own definition. Infra: every domain may include it.

## `src/ri/geometry/geometryFrame.h`

```cpp
#include "camera.h"             // openrender::LensModel (Graphics State)
#include "common/algebra.h"
#include "options.h"            // OPTIONS_PROJECTION_*, OPTIONS_FLAGS_* (Graphics State)

// The frame limits and constants that dicing and ray tessellation share (spec 031, FR-002). Built by the
// renderer core once per frame from its own values (stored copies, never recomputed) and installed until the
// end of the frame; read lock-free. With nothing installed every field is 0.
struct CGeometryFrame {
    int numThreads, maxGridSize, maxEyeSplits;
    unsigned int flags;           // OPTIONS_FLAGS_MOTIONBLUR, OPTIONS_FLAGS_FOCALBLUR, ...
    unsigned int raytracingFlags; // the attribute flags that make an object raytraced
    int projection;               // an EProjectionType value
    float imagePlane, dPixeldx, dPixeldy, pixelLeft, pixelTop, clipMin;
    openrender::LensModel lens;   // the frame's lens (state/camera.h): the circle of confusion the dicing heuristic reads
};

void installGeometryFrame(const CGeometryFrame *frame); // the renderer core; nullptr uninstalls
const CGeometryFrame &geometryFrame();                   // never null: the installed value or the zero default

// camera2pixels() of renderer.h over the frame's copies: the same expression, character for character
inline void camera2pixels(const CGeometryFrame &f, int n, float *P) { /* renderer.h:529-541 with f.* */ }
```

## `src/ri/geometry/rayContext.h`

```cpp
#include "common/shadingDim.h"
#include "geometryFrame.h"

class CMemPage;
class CSurface;

// What intersection, ray-hit shading, clustering, lazy child creation, the region and crossing hooks and the
// raytrace tessellation need from the render thread that calls them (spec 031, FR-003). Geometry owns it and
// never implements it; every hider implements it (CHider). One per render thread.
class IRayContext {
    public:
        virtual ~IRayContext() = default;

        virtual int threadIndex() const = 0;     // this render thread's index, 0 <= threadIndex() < frame().numThreads
        virtual CMemPage *&scratch() = 0;        // the thread's scratch arena: memBegin/memEnd/ralloc/memSave/memRestore
        virtual float **varyings() = 0;          // the current shading state's varying arrays (VARIABLE_* indices):
                                                 // the caller writes the points' inputs, calls shade/displace (or
                                                 // IDicer::probe) and reads the outputs back from the same arrays

        // Shade the points written into varyings(): u x v points of `surface`, `dim` as today, `up` the
        // PARAMETER_* flags. shade() runs the full shading of ray hits; displace() the displacement only.
        virtual void shade(CSurface *surface, int u, int v, EShadingDim dim, unsigned int up) = 0;
        virtual void displace(CSurface *surface, int u, int v, EShadingDim dim, unsigned int up) = 0;

        virtual void *saveShadingState() = 0;            // as CShadingContext::saveState()
        virtual void restoreShadingState(void *state) = 0; // as CShadingContext::restoreState()

        const CGeometryFrame &frame() const { return geometryFrame(); }
};
```

## `src/ri/geometry/dicer.h`

```cpp
#include "rayContext.h"

class CObject;
class CSurface;
struct CGridSnap; // patchLayout.h

// What a surface's dice() and splitToChildren() need from the hider that dices (spec 031, FR-002). Implemented
// by the hiders that rasterize (CReyes, and CStochastic and CZbuffer through it); never by Geometry.
class IDicer {
    public:
        virtual ~IDicer() = default;

        virtual IRayContext &rayContext() = 0; // the dicing thread's context: scratch, varyings, CSG resolve, mesh create

        virtual void drawObject(CObject *object) = 0;                // queue an object (children, split pieces)
        virtual void drawGrid(CSurface *surface, int udiv, int vdiv, float umin, float umax, float vmin, float vmax,
                              const CGridSnap *snap) = 0;            // dice and rasterize a grid (snap may be null)
        virtual void drawPoints(CSurface *points, int numPoints) = 0; // RiPoints

        // The probe: displacement estimate of the u x v points written into rayContext().varyings()
        // (today's displaceEstimate, shade(..., 3)); the results are read back from the same arrays.
        virtual void probe(CSurface *surface, int u, int v, EShadingDim dim, unsigned int up) = 0;

        virtual int inFrustum(const float *bmin, const float *bmax) = 0; // TRUE if the camera-space box may be visible

        const CGeometryFrame &frame() const { return geometryFrame(); }
};
```

No default argument on a virtual: the two call sites without a snap pass `nullptr` (`surface.cpp:742`, `CCurve::dice`).

## Implementations (Hiders)

```cpp
class CHider : public openrender::shading::ITracer, public IRayContext {   // IShadingProducer removed
    public:
        // IRayContext
        int threadIndex() const override { return context.thread; }
        CMemPage *&scratch() override { return context.threadMemory; }
        float **varyings() override { return context.currentShadingState->varying; }
        void shade(CSurface *s, int u, int v, EShadingDim d, unsigned int up) override { shadeSurface(s, u, v, d, up, FALSE); }
        void displace(CSurface *s, int u, int v, EShadingDim d, unsigned int up) override { shadeSurface(s, u, v, d, up, TRUE); }
        void *saveShadingState() override { return context.saveState(); }
        void restoreShadingState(void *state) override { context.restoreState(state); }
        ...
    protected:
        // today's IShadingProducer::shadeSurface body (shadingProducer.cpp), unchanged except the dropped ctx argument
        void shadeSurface(CSurface *surface, int uVertices, int vVertices, EShadingDim dim, unsigned int up, int displaceOnly);
};

class CReyes : public CHider, public IDicer {
    public:
        IRayContext &rayContext() override { return *this; }
        void drawObject(CObject *) override;                                                       // body unchanged
        void drawGrid(CSurface *, int, int, float, float, float, float, const CGridSnap *) override; // body unchanged
        void drawPoints(CSurface *, int) override;                                                 // body unchanged
        void probe(CSurface *s, int u, int v, EShadingDim d, unsigned int up) override { shadeSurface(s, u, v, d, up, 3); }
        int inFrustum(const float *bmin, const float *bmax) override; // reyes.cpp: CRenderer::inFrustrum(bmin, bmax)
};
```

`CReyes::inFrustum` is defined **out of line in `reyes.cpp`**, whose `-> CRenderer::inFrustrum` allowlist line
already exists; an inline body in `reyes.h` would emit the reference in `stochastic.cpp` and `zbuffer.cpp` too and
create new link edges.

Reyes's own shading calls (`reyes.cpp:906`, `:967`, `:1060`, `:1114`, today `context.shade(object, ...)`) call
`shadeSurface(object, ...)` with the same arguments; its `object->dice(this)` (`:374`, `:570`) becomes
`object->dice(*this)` and `regionCoverage(&context, ...)` (`:1042`) becomes `regionCoverage(this, ...)`.
`trace.cpp:210`, `:338`, `:350` pass `*this`.

## Rules

- **Names that could collide**: `threadIndex()`, not `thread()`, because every hider constructor has a parameter
  named `thread` (`reyes.h:234`, `stochastic.h:42`, `zbuffer.h:41`, `raytracer.h:149`, `photon.h:47`). `frame()` is a
  non-virtual member of both interfaces, so it is ambiguous inside `CReyes` and its subclasses: hiders never call it
  (they read `FrameParams` and their own state); Geometry calls it on the one interface reference it holds, which is
  unambiguous. No hider declares `shade`, `displace`, `probe`, `scratch`, `varyings` or `rayContext` today (grep).

- **Expressions do not move**: every float expression in `estimateDicing`, `probe`, `cull`, the tessellation and
  the region stages keeps its text; only the receivers of its inputs change (`CRenderer::maxGridSize` ->
  `f.maxGridSize`, `r->shadingContext().displaceEstimate(...)` -> `dicer.probe(...)`). The varying pointer is
  captured where it is captured today (`surface.cpp:264`, before the probe loop).
- **The tessellation lock (P-Q6)** becomes Geometry's: a `std::mutex` local to `surface.cpp` guards what
  `CRenderer::tesselateMutex` guards today (the `children` test-and-set in `CSurface::intersect` and
  `checkRayGuard`, the tessellation list in `CTesselationPatch`), with the same scopes; `CRenderer::tesselateMutex`
  is deleted from the core (`renderer.h:176`, `rendererMutexes.cpp:81`, `:144`, `:167`). The lock sites are
  `surface.cpp:1625-1636` (`CTesselationPatch::tesselate`), `surface.cpp:2440` and `:2480` (`purgeTesselations`) and
  `geometryDispatch.cpp:284`, `:294`, `:337`, `:348`. **Design in two steps (tasks stage, finding F3, analyze M5)**:
  Phases 4 and 5 export a pair, `CTesselationPatch::lockTessellation()`/`unlockTessellation()` (declared in
  `surface.h`, backed by the `std::mutex` in `surface.cpp`), because `geometryDispatch.cpp` is another translation
  unit until Phase 6; at T080 the last core-side caller moves into `surface.cpp`, the declarations leave `surface.h`
  and the pair becomes TU-local to `surface.cpp`, so the end state is the mutex local to `surface.cpp` described
  above and it is never public API. `CTesselationPatch` also stores the thread count at construction
  (`numThreadsAtBuild`) and its destructor never reads the frame (a patch can be destroyed after the frame is
  uninstalled). Alternative: an
  `IRayContext::lockTessellation()` pair answered by the core's mutex (FR-003's wording lists "the tessellation
  lock"); rejected because the lock guards Geometry's own data structure and nothing else takes it.
- **CSG stays generic** (FR-019, gotchas 13-14): the interfaces name no CSG type; `CCSGSolid::dice` resolves through
  `dicer.rayContext()` and dices its leaves like any object; classification still bypasses ray filters.
- **No member for spec 057** (FR-021): area sampling can attach to `IRayContext` later (a sampling request answered
  by the hider) without a Hiders or Shading type in Geometry; nothing is added now.
