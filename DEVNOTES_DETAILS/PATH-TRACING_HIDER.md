# Path-Tracing Hider (PBR) + OSL Integration — Feasibility Analysis

**Written:** 2026-06-04
**Status:** Analysis only — not started. Not yet scheduled against other planned features.
**Reference implementation studied:** pbrt-v3 at `/Volumes/Projects/Development/CLI/pbrt-v3`

---

## Context

The proposal is to integrate physically-based rendering (PBR) capabilities into openrender — modeled after pbrt-v3's integrator architecture — alongside support for OpenShading Language (OSL), which would be exposed as a new `Bxdf` RIB statement (the Pixar/RenderMan convention). Both features would be delivered as a new Hider named `"pathtracer"` (or `"pbr"`), sitting alongside the existing `stochastic`, `zbuffer`, `raytrace`, and `photon` hiders.

This document is a technical analysis of scope, impact, and feasibility. It is **not** a step-by-step implementation plan — it is a perspective to anchor a future implementation effort.

---

## 1. Current openrender Architecture — What Already Exists

### 1.1 Hider System

All hiders implement the `CShadingContext` abstract interface (`src/ri/shading.h`):

```
CShadingContext (abstract)
├── CReyes (bucket rasterizer, reyes.cpp)
│   ├── CStochastic — stochastic sampling, motion blur, DOF
│   └── CZbuffer    — classic depth buffer
├── CRaytracer      — primary camera rays (not path tracing)
├── CPhotonHider    — photon map pass
└── CShow           — debug/viz hider
```

Hider selection is a plain `strcmp` chain in `renderer.cpp:beginFrame()` (~line 908). Adding a new hider requires:
- A new `#include` and two `if/else if` branches in that chain (~10 lines)
- A new class inheriting `CShadingContext`

**Key insight:** `CRaytracer` proves that hiders outside the REYES rasterization path already work. A path tracer can follow the same pattern without touching the REYES pipeline.

### 1.2 Existing Ray Infrastructure

openrender already has:
- `CRay` / `CRayBundle` — ray type with origin, direction, tmin/tmax, time
- BVH/hierarchy (`renderer.h:180` — `hierarchyMutex`, `CTracable` interface)
- `CShadingContext::trace(CRayBundle*)` — fires secondary rays through the BVH and invokes `postShade()`
- `pointHierarchy.h/cpp` — spatial acceleration structure

This means intersection infrastructure does not need to be written from scratch.

### 1.3 Shader System (RSL)

RSL shaders compile through:
```
.sl source → oshader (rslo.y grammar) → TCode[] opcodes + IR (Layer 2)
           → .rslo binary (cached in globalFiles trie)
           → CProgrammableShaderInstance (runtime, bound to CAttributes)
           → per-point execution in shading.cpp via varying[][] float buffers
```

`CAttributes` currently holds shader slots: `surface`, `displacement`, `atmosphere`, `interior`, `exterior`. A `bxdf` slot does not exist yet.

RSL shaders compute `Ci` and `Oi` directly — they are **not** closures. This is the fundamental reason they cannot drive a path tracer without a bridging strategy.

### 1.4 RIB Parsing — What's Missing

`rib.y` handles `Surface`, `Displacement`, `Atmosphere`, `Interior`, `Exterior`. **`Bxdf` is not parsed.** Zero references to `Bxdf` or OSL exist anywhere in the codebase.

---

## 2. pbrt-v3 — What's Relevant to Port

pbrt-v3 (~77K lines, 229 files) is a full PBR renderer with its own scene format, geometry, and acceleration. We are **not porting pbrt-v3** — we are borrowing its algorithmic patterns and selectively extracting specific implementations.

### 2.1 What to Borrow (Algorithmically)

| pbrt-v3 Component | openrender Use | Lines to Extract/Adapt |
|---|---|---|
| `PathIntegrator::Li()` | Core recursive path integration | ~220 |
| `EstimateDirect()` + MIS | Direct light sampling with multiple importance sampling | ~150 |
| `BxDF` abstract interface | BSDF evaluation / sampling / PDF | ~60 |
| `LambertianReflection` | Reference diffuse BSDF | ~40 |
| `MicrofacetReflection` (GGX) | Reference specular BSDF | ~200 |
| `FresnelConductor/Dielectric` | Fresnel equations | ~80 |
| `DisneyMaterial` | Production BSDF (optional Phase 3+) | ~500 |
| Halton/Sobol samplers | Low-discrepancy sample sequences | ~300 |
| `FilmTile` accumulation | HDR radiance splat per pixel | ~150 |

**Total algorithmic extraction:** ~1,200–1,700 lines, adapted to openrender's types (`vector` instead of `Vector3f`, `CAttributes` instead of `Material*`, etc.)

### 2.2 What NOT to Port

- pbrt-v3's scene format (`.pbrt`) — openrender uses RIB
- pbrt-v3's BVH/KdTree — openrender already has its own hierarchy
- pbrt-v3's film/display system — openrender has a display channel infrastructure
- pbrt-v3's camera model — openrender's camera is already functional
- pbrt-v3's light class hierarchy — adapt to openrender's existing RSL light shaders

### 2.3 OSL — pbrt-v3 Has None

pbrt-v3 has zero OSL support. OSL integration is a separate, parallel effort that does **not** derive from pbrt-v3.

---

## 3. OpenShading Language — What It Is and How It Integrates

### 3.1 OSL Overview

OSL (Sony Pictures Imageworks, open source: `github.com/AcademySoftwareFoundation/OpenShadingLanguage`) is a closure-based shading language:

- **Compiler:** `oslc` (.osl source → .oso bytecode) — an external tool, not part of openrender
- **Runtime:** `liboslexec` — a shared library openrender would link against; uses LLVM JIT
- **Key difference from RSL:** OSL shaders return **closure trees** (e.g., `diffuse(N)`, `microfacet(...)`, `reflection(dir, eta)`). The renderer samples and evaluates these closures during path integration. RSL shaders compute a final `Ci`/`Oi` directly — no closure, no sampling.

### 3.2 The `Bxdf` RIB Convention (Pixar/RenderMan)

In RenderMan 19+, the syntax is:

```
# RSL surface shader (existing — computes Ci/Oi directly)
Surface "matte" "float Kd" [0.8]

# OSL Bxdf shader (new — returns BSDF closures)
Bxdf "PxrDiffuse" "myMaterial" "color diffuseColor" [0.5 0.3 0.1]
Bxdf "PxrDisney" "skin"  "color baseColor" [0.8 0.6 0.5]  "float roughness" [0.4]
```

The `Bxdf` statement:
1. Names an OSL shader (compiled .oso file, found on the shader search path)
2. Provides an instance name (for multi-layer materials)
3. Binds parameters

When the path tracer hits a surface, it:
1. Executes the OSL shader → gets a closure tree
2. Traverses the tree to `eval(wo, wi)`, `sample(wo)`, `pdf(wo, wi)` at each bounce
3. MIS weights direct illumination against BSDF samples

### 3.3 OSL Closure Primitives Relevant to openrender

| OSL Closure | Meaning | BSDF Implementation Needed |
|---|---|---|
| `diffuse(N)` | Lambertian reflection | Cosine-weighted hemisphere sampling |
| `oren_nayar(N, sigma)` | Rough diffuse | Oren-Nayar formula |
| `reflection(N, eta)` | Perfect mirror | Delta distribution |
| `refraction(N, eta)` | Perfect refraction | Snell's law delta |
| `microfacet(dist, N, xdir, ax, ay, eta, refract)` | GGX/Beckmann | Microfacet BSDF (can lift from pbrt-v3) |
| `emission()` | Emissive surface | Contributes to Le() |
| `background()` | Environment/background | Infinite light contribution |
| `holdout()` | Matte/holdout | Alpha compositing |

---

## 4. Integration Architecture — The New `CPathTracer` Hider

### 4.1 Class Design

```cpp
// src/ri/pathtracer.h
class CPathTracer : public CShadingContext {
public:
    CPathTracer(int thread);
    ~CPathTracer();

    // CShadingContext interface
    void renderingLoop() override;
    void drawObject(CObject *) override;
    void drawGrid(CSurface *, int u, int v, ...) override;
    void drawPoints(CSurface *, int) override;

    // Static hider lifecycle
    static void preDisplaySetup();
    static void postDisplaySetup();

private:
    // Per-thread film accumulation buffer
    float *tileBuffer;   // HDR float4 (R, G, B, alpha) per sample

    // Core path integration
    void   pathTrace(int xBucket, int yBucket);
    vector Li(CRay &ray, int depth, CSobol<4> &sampler);
    vector estimateDirect(const SurfaceHit &hit, const vector &wo, const CBsdf &bsdf);

    // BSDF dispatch (RSL → bridged BSDF, OSL → closure BSDF)
    void  evalBsdf(const CAttributes *, const SurfaceHit &, CBsdf &out);
};
```

### 4.2 Data Flow Through the PBR Hider

```
RIB:  Hider "pathtracer"
      Bxdf "PxrDiffuse" "mat" "color diffuseColor" [0.8 0.6 0.4]
      Sphere 1 -1 1 360

renderer.cpp:beginFrame()
  → new CPathTracer(i) for each thread
  → geometry tessellated, inserted into BVH (existing infrastructure)

CPathTracer::renderingLoop()
  for each bucket {
    for each pixel in bucket {
      for each sample (Halton sequence) {
        ray ← camera.generateRay(x + jx, y + jy, lens_u, lens_v, time)
        L   ← Li(ray, depth=0, sampler)
        tileBuffer[pixel] += L / numSamples
      }
    }
    commit tileBuffer → existing display channel infrastructure
  }

Li(ray, depth):
  hit ← BVH.intersect(ray)
  if !hit → return environment light sample

  // Execute OSL bxdf shader → closure tree
  if hit.attrs->bxdf:
    oslExecute(hit.attrs->bxdf, hit) → closureTree
    bsdf ← CBsdf(closureTree)   // wraps OSL closures
  elif hit.attrs->surface:
    // RSL fallback: treat Ci/Oi as emission (non-participating in GI)
    shade(hit) → Ci, Oi
    return Ci * (1 - Oi)   // emission-only path

  Le ← hit.emittedRadiance()      // for area lights
  Ld ← estimateDirect(hit, bsdf)  // direct illumination (shadow rays + MIS)

  // Indirect: sample BSDF direction
  wi, f, pdf ← bsdf.sample(wo, sampler.get2D())
  if russianRoulette(depth) → return Le + Ld
  return Le + Ld + f * Li(spawnRay(hit.P, wi), depth+1) / pdf
```

### 4.3 OSL Runtime Wrapper

New files: `src/ri/oslContext.h/cpp`

```cpp
class COslContext {
public:
    static void init();   // Create OSL::ShadingSystem, register closures
    static void destroy();

    // Load a .oso shader by name (searched on shader path, like .rslo)
    static OSL::ShaderGroupRef loadShader(const char *name);

    // Execute a shader group at a surface hit point, returns closure tree
    static const OSL::ClosureColor *execute(
        OSL::ShaderGroupRef &group,
        const CAttributes   *attrs,
        const SurfaceHit    &hit,
        OSL::ShadingContext *ctx    // per-thread context
    );
};
```

Key integration points:
- `OSL::ShadingSystem` is shared across threads; `OSL::ShadingContext` is per-thread
- openrender's `ORENDERHOME/shaders` search path feeds `OSL::ShadingSystem::LoadShader()`
- OSL's `RendererServices` interface needs implementations for: `texture()`, `get_matrix()`, `trace()` — the latter two map to existing openrender functionality

### 4.4 BSDF Closure Bridge

New files: `src/ri/oslBsdf.h/cpp`

The closure tree from OSL is a recursive structure of component closures, mix nodes, and add nodes. The bridge evaluates it:

```cpp
struct CBsdf {
    // From OSL closure tree
    vector eval(const vector &wo, const vector &wi) const;
    bool   sample(const vector &wo, vector &wi, vector &weight, float &pdf,
                  float u1, const vector2 &u2) const;
    float  pdf(const vector &wo, const vector &wi) const;

private:
    // Flatten closure tree into component list during construction
    struct Component {
        ClosureType type;   // diffuse, microfacet, reflection, ...
        vector      weight;
        // type-specific params (roughness, eta, ...)
    };
    std::vector<Component> components;
    float totalWeight;
};
```

---

## 5. Changes Required by File

### 5.1 Build System

| File | Change | LoC |
|---|---|---|
| `CMakeLists.txt` (root) | `find_package(OSL REQUIRED)`, `find_package(LLVM REQUIRED)` | 20 |
| `src/ri/CMakeLists.txt` | Link `oslexec`, `oslcomp`, `oslquery`; add new .cpp files | 30 |

### 5.2 RIB Layer

| File | Change | LoC |
|---|---|---|
| `src/ri/rib.l` | Add `Bxdf` token (`RIB_BXDF`) | 5 |
| `src/ri/rib.y` | Grammar rule for `Bxdf`: `RIB_BXDF RIB_TEXT RIB_TEXT ribPL { RiBxdfV($2,$3,...) }` | 15 |
| `src/ri/ri.cpp` | `RiBxdfV()` — calls `getShader()` in OSL mode, stores in attributes | 50 |
| `src/ri/rendererContext.cpp` | `RiBxdf()` handler alongside `RiSurface()` | 60 |

### 5.3 Attributes

| File | Change | LoC |
|---|---|---|
| `src/ri/attributes.h` | Add `CShaderInstance *bxdf;` field | 3 |
| `src/ri/attributes.cpp` | Init/copy/destroy `bxdf` in constructor/destructor/restore() | 15 |

### 5.4 Renderer Core

| File | Change | LoC |
|---|---|---|
| `src/ri/renderer.h` | Declare `static COslContext *oslContext;` | 5 |
| `src/ri/renderer.cpp` | Add `"pathtracer"` branch in `beginFrame()` hider switch | 15 |

### 5.5 New Files

| File | Purpose | Estimated LoC |
|---|---|---|
| `src/ri/pathtracer.h` | `CPathTracer` class declaration | 80 |
| `src/ri/pathtracer.cpp` | Path integration, direct illumination, film accumulation | 1,400 |
| `src/ri/oslContext.h` | `COslContext` declaration, OSL ShadingSystem wrapper | 60 |
| `src/ri/oslContext.cpp` | OSL init/destroy, shader loading, per-thread execution | 550 |
| `src/ri/oslBsdf.h` | `CBsdf` and closure traversal | 80 |
| `src/ri/oslBsdf.cpp` | Closure bridge: eval/sample/pdf for each OSL closure type | 500 |
| `src/ri/brdf.h` | Standalone BSDF math (GGX, Fresnel, Lambertian) for RSL fallback | 300 |
| `shaders/osl/PxrDiffuse.osl` | Example OSL shader (diffuse surface) | 30 |
| `shaders/osl/PxrDisney.osl` | Example Disney BSDF OSL shader | 120 |
| `examples/rib/pbr-sphere.rib` | Test scene demonstrating Bxdf and pathtracer hider | 30 |

**Total new code: ~3,200 lines**
**Total modified code: ~200 lines**
**Grand total: ~3,400 lines changed/added**

---

## 6. Phased Rollout

### Phase 0 — Bxdf RIB Scaffolding (1 week)
Prerequisite for everything. No OSL yet — just parse `Bxdf`, store in `CAttributes::bxdf`, ignore during rendering (stochastic hider unchanged). Validates the RIB pipeline.

Files: `rib.l`, `rib.y`, `ri.cpp`, `rendererContext.cpp`, `attributes.h/cpp`

### Phase 1 — OSL Runtime (3–4 weeks)
Wire liboslexec into the renderer. Write `COslContext` and `CBsdf`. At this stage, OSL shaders execute correctly at hit points and produce closures, but only `estimateDirect()` is implemented (no recursion). Useful for direct-only PBR renders.

Files: `oslContext.h/cpp`, `oslBsdf.h/cpp`, `CMakeLists.txt` changes

### Phase 2 — `CPathTracer` Hider (3–4 weeks)
Full recursive path integrator. Activates with `Hider "pathtracer"`. Russian roulette termination, Halton sampling, HDR splat accumulation. Basic OSL closures (diffuse + reflection) work end-to-end.

Files: `pathtracer.h/cpp`, `brdf.h`, `renderer.cpp`

### Phase 3 — BSDF Library Completeness (2 weeks)
Port GGX microfacet, Disney BSDF from pbrt-v3. Add all OSL closure types to `CBsdf`. Deliver `PxrDiffuse.osl` and `PxrDisney.osl` as shipped example shaders.

### Phase 4 — Light Sampling (2 weeks)
Implement `sample_Li()` / `pdf_Li()` for openrender's existing light types (area, point, spot, directional, environment). MIS between BSDF sampling and light sampling for variance reduction.

### Phase 5 — Integration & Validation (2 weeks)
Side-by-side renders with BMRT/PRMan reference. Convergence tests. Multi-thread correctness under TSan.

**Total timeline: 13–15 weeks** for production-quality delivery.

---

## 7. Risk Factors

### 7.1 Heavy Dependencies (High Impact)

OSL requires **LLVM**. This adds a substantial build dependency (~200MB headers, ~1GB installed). On macOS with Homebrew: `brew install openshadinglanguage`. On Linux: available via most distros. CI/CD pipelines need updating.

Mitigation: wrap the entire OSL path in `#ifdef OPENRENDER_OSL`. When OSL is absent, `Bxdf` statements log a warning and fall back to `Surface`.

### 7.2 OSL Thread Safety (Medium Risk)

`OSL::ShadingSystem` is shared and thread-safe, but `OSL::ShadingContext` must be per-thread. openrender allocates one `CShadingContext` per thread — the OSL `ShadingContext` fits naturally as a per-thread member of `CPathTracer`.

### 7.3 Geometry Representation Mismatch (Medium Risk)

The REYES pipeline dices patches into micropolygons on-the-fly, per-bucket. The path tracer needs the full static scene in the BVH for secondary ray intersection. openrender already does this for `CRaytracer` (the `hierarchyMutex`/`CTracable` path). The path tracer should use the same mechanism — but verify that all geometry types (NURBS, subdivision, curves, quadrics) are represented in the BVH, not just polygons.

### 7.4 RSL Shaders in PBR Mode (Low-Medium Risk)

RSL `Surface` shaders compute `Ci/Oi` directly — they are incompatible with the closure-based path integration model. Strategy options:

| Approach | Behavior | Effort |
|---|---|---|
| **A: Emission-only** | Treat RSL Ci as emitted radiance; surface is black for indirect | Trivial |
| **B: Lambertian bridge** | Shade RSL once, wrap Ci as albedo of a Lambertian BSDF | Low |
| **C: RSL bridge macro** | Define `illuminate {}` callbacks that inject energy into the path | High |

Recommendation: start with **B** (Lambertian bridge) — it gives physically plausible results for most existing scenes without rewriting all shaders.

### 7.5 No Spectral Rendering (Low Risk — Known Limitation)

openrender works in RGB. True PBR benefits from spectral rendering (accurate Fresnel, dispersion, iridescence). RGB is an acceptable v1 limitation — OSL's `Spectrum` maps to openrender's `vector`.

### 7.6 Film Output Format

The path tracer accumulates HDR float radiance. The existing display channel infrastructure supports `float` channels — `"rgba"` with `"float"` type is compatible. Tone mapping (ACES, Reinhard) should be left to a display filter, consistent with how BMRT handles this.

---

## 8. Example RIB Usage (Target State)

```rib
## test-pbr.rib — PBR sphere with OSL Bxdf shader
Display "test-pbr.exr" "file" "rgba"
Hider "pathtracer"
  "int minsamples" [64]
  "int maxsamples" [256]
  "int maxdepth" [8]
Format 512 512 1
Projection "perspective" "float fov" [45]
ShadingRate 1

WorldBegin
  # OSL Bxdf shader: Disney BSDF
  Bxdf "PxrDisney" "chrome"
    "color baseColor"  [0.8 0.7 0.6]
    "float metallic"   [1.0]
    "float roughness"  [0.15]

  Sphere 1  -1 1 360

  # Area light as emissive sphere (OSL emission closure)
  AttributeBegin
    Translate 3 3 -2
    Bxdf "PxrEmission" "areaLight"
      "color emitColor" [15 12 9]
    Sphere 0.5 -0.5 0.5 360
  AttributeEnd
WorldEnd
```

---

## 9. Summary Assessment

| Dimension | Assessment |
|---|---|
| **Feasibility** | High — openrender already has ray infrastructure; the hider model is designed for extensibility |
| **Scope** | Large but well-bounded — ~3,400 lines of code changes across 12 files |
| **Architectural fit** | Excellent — the new Hider integrates as a peer to CRaytracer with zero disruption to existing hiders |
| **Biggest risk** | OSL/LLVM build dependency; manageable with conditional compilation |
| **Sequencing** | OSL must precede the PBR hider; Bxdf parsing must precede OSL |
| **Timeline** | 13–15 weeks to production quality; 6–8 weeks to a working prototype |
| **pbrt-v3 role** | Reference implementation for path integration math and BSDF library, not a direct port |

The `Bxdf` + `pathtracer` design is architecturally sound and faithful to Pixar's RenderMan model. It preserves full backward compatibility with all existing RSL shaders and hiders. The primary cost is the liboslexec dependency and the learning curve of the OSL `RendererServices` interface.
