# Contract: `CShadingContext` after spec 029 (Shading)

**Status**: plan-stage proposal. It assumes the recommended answers to P-Q1 to P-Q12 ([../research.md](../research.md));
an answer that differs changes the marked parts.

**Requirements**: FR-007, FR-008, FR-013, FR-016, FR-017, FR-018; SC-004, SC-007.
**File**: `src/libshader/shading/shading.h` (and `shading.cpp`).

## Shape

- `class CShadingContext final`, with no virtual function: the destructor becomes non-virtual,
  `renderingLoop()` moves to `CHider` ([hider.md](hider.md)), the four illumination-hook virtuals become the
  `IIlluminationHook` interface ([illumination-hook.md](illumination-hook.md)).
- Constructible alone: `explicit CShadingContext(int thread)`. With no services, no hider, no tracer, no
  producer and no hook, every routed lookup answers as specs 024/026 define (warn once per kind, not found or
  miss), and `newState()` keeps its documented refusal.
- `friend class CProgrammableShaderInstance;` stays (Shading's own). The friends `CPhotonHider`, `CSphereLight`
  and `CQuadLight` go (FR-008).
- Checked at compile time by a test: `static_assert(!std::is_polymorphic_v<CShadingContext>)`,
  `static_assert(std::is_final_v<CShadingContext>)` (P-Q11).

## Members removed

| Member | Today | After |
|---|---|---|
| `virtual ~CShadingContext()` | `shading.h:363` | non-virtual |
| `virtual void renderingLoop() = 0` | `:366` | `CHider::renderingLoop()` |
| `trace(CRayBundle *)`, `traceEx(CRayBundle *)`, `trace(CRay *)` | `:391-393`, defined in `src/ri/hiders/trace.cpp:47, 253, 303` | `CHider` members in `trace.cpp` ([hider.md](hider.md)) |
| `traceAny(CRay *)` | `:409`, defined nowhere | deleted |
| `solarBegin`, `solarEnd`, `illuminateBegin`, `illuminateEnd` (protected virtuals) | `:1007-1010` | `IIlluminationHook` |
| `refine2D`, `refine1D`, `displace(CSurface *, int, CQuadVertex **)` (private, declared, defined nowhere) | `:1135-1137` | deleted (dead declarations naming Geometry types) |
| `friend class CPhotonHider; CSphereLight; CQuadLight;` | `:1175, 1177, 1178` | deleted |

## Members added (public, documented in the header)

```cpp
// ---- The shading entry (spec 029, FR-018; contracts/point-source-and-globals.md)
// Shade a batch: `globals` describes it (per-batch fields only on input), `source` samples the primitive.
void shade(const openrender::shading::ShaderGlobals &globals, openrender::shading::IPointSource &source);

// The Geometry-facing forwarding entry (P-Q1 (a)). `surface` is opaque: Shading never dereferences it and hands it
// to the producer the owner installed. With none installed: one CODE_NOTSTARTED warning per process, nothing shaded.
// Keeps the mangled symbol that Geometry's allowlisted calls bind to (DB-006, allowlist.txt:109-113).
void shade(CSurface *surface, int uVertices, int vVertices, EShadingDim dim, unsigned int usedParameters,
           int displaceOnly = FALSE);
inline void displace(CSurface *s, int u, int v, EShadingDim d, unsigned int up) { shade(s, u, v, d, up, TRUE); }        // unchanged
inline void displaceEstimate(CSurface *s, int u, int v, EShadingDim d, unsigned int up) { shade(s, u, v, d, up, 3); }   // unchanged
void setProducer(openrender::shading::IShadingProducer *producer);   // set by the owning hider; nullptr: none

// ---- Tracing (P-Q2 (a), FR-016)
void setTracer(openrender::shading::ITracer *tracer);  // set by the owning hider
openrender::shading::ITracer *tracer() const;          // the owner's tracer, nullptr if none (no services fallback here)

// ---- The ray level (FR-017): the one public way to go one ray level deeper
struct CRayLevel {                     // what leaveRayLevel() restores
    CShadingState *savedState;
    const char *savedLabel;
};
// If rayDepth() < maxRayDepth: depth + 1, label = bundle.label, a fresh state from the free list (newState() when
// it is empty; asserted non-null as traceEx does at trace.cpp:263), state->postShader = bundle.postShader; returns
// true. Otherwise changes nothing and returns false: the caller makes the miss call bundle->postShade(numRays, rays).
// The depth rule is applied here and nowhere else.
bool enterRayLevel(const CRayBundle &bundle, int maxRayDepth, CRayLevel &saved);
// Depth - 1, the state back on the free list, label and state restored, in traceEx's order (trace.cpp:271-276).
void leaveRayLevel(const CRayLevel &saved);
int rayDepth() const;          // currentRayDepth
bool inShadowRay() const;      // inShadow (set by traceTransmission, executeMisc.cpp:717)
// The per-thread object hash the owner's trace groups rays with (P-Q16 (a)): SHADING_OBJECT_CACHE_SIZE entries,
// allocated and filled by the constructor (GitHub #53); an empty entry's object is (CSurface *)this of the context.
TObjectHash *objectHash();
void countTracedRay();         // ++numTracedRays (the statistics flushed by the destructor, shading.cpp:545)

// ---- Illumination hooks (FR-013)
void setIlluminationHook(openrender::shading::IIlluminationHook *hook);   // nullptr: none
openrender::shading::IIlluminationHook *illuminationHook() const;
bool rendererIlluminationHook() const;   // moved from protected to public, unrenamed (shading.h:972)

// ---- The shaded object (P-Q3 (a))
// Sets the state's opaque surface (for the observers' reports), its handle (objectHandle()) and its attributes
// (objectAttributes() and every attribute read in Shading). The producer passes the CObject * upcast as the handle,
// the value CRendererServicesImpl casts back.
void setShadedObject(const CSurface *surface, openrender::shading::ObjectHandle handle, const CAttributes *attributes);

// ---- Tests (P-Q11)
int vertexMemoryInUse() const;   // vertexMemory, read-only
```

`objectHandle()` and `objectAttributes()` keep their signatures and return the stored values.

## `CShadingState` (P-Q3 (a))

`CSurface *currentObject` (`shading.h:190`) is replaced by three fields, written only through
`setShadedObject()` (on the context, for the current state) or `shade()`:

| Field | Type | Read by |
|---|---|---|
| `currentSurface` | `const CSurface *` (opaque) | the two observers' `surface` field (`CPointKeyReport`, `CPointDisplacedReport`) |
| `currentObject` | `ObjectHandle` | `objectHandle()`, every service request |
| `currentAttributes` | `const CAttributes *` | `objectAttributes()`, the eight reads in `shading.cpp`, `hcshader.cpp:143, 458` (through `objectAttributes()`) |

`shade()` saves and restores all three where it saves and restores `currentObject` today
(`shading.cpp:1022-1023, 1389`).

## What stays exactly as it is

- Everything the interpreter and the JIT reach: the executor, the `jit*` members, the draw members (spec 039),
  the texture option block (spec 027), the services accessors and query wrappers (spec 026).
- `newState`/`freeState`/`deleteState`/`saveState`/`restoreState`, `updateState()`, `threadMemory`,
  `currentXBucket`/`currentYBucket`, `thread`, the statistics counters and their flush.
- `hookOperand()` (static helper; the operand rule of GitHub #68).
- The three test observers and their report structures (`CPointKeyReport::surface` keeps its type).
- `CRayBundle` and its protocol; `CRay` (`src/ri/core/ray.h`, Infra).

## Includes

`shading.cpp` loses `object.h`, `bundles.h` (now Shading's own, local), `irradiance.h` and `photonMap.h`;
`execute.cpp` loses `irradiance.h` and `photonMap.h`. `shading.h` gains `illuminationHook.h`, `pointSource.h`,
`shadingProducer.h` and `tracer.h` (all Shading, header-only). It keeps forward declarations of `CSurface` and
`CObject` as opaque types only.
