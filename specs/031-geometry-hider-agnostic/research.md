# Research: Geometry, Hider-Agnostic (spec 031)

Plan stage, 2026-11-07, master `e83d005d`, static reading only (no build was run for the plan). Every figure
below was taken from the tree; the ones marked **(measure at Phase 1)** are re-measured on a build before any
`src/` edit. Each decision lists the alternatives considered. The maintainer answered every question on 2026-11-07 (recommended options; orthographic depth of field kept off):
see [plan.md](plan.md) "Maintainer answers". This file gives the evidence; each **Decided** note records the answer.

## R1. Call-site inventory: what Geometry uses the two context types for

`grep` over `src/ri/geometry/*.{h,cpp}` and `src/ri/render/geometryDispatch.cpp` (the Geometry bodies that live
in the renderer directory today).

| Use | Where | Count | New home |
|---|---|---|---|
| Per-thread scratch arena (`memBegin`/`memEnd`/`ralloc`/`memSave`/`memRestore` on `context->threadMemory`) | `patches.cpp` 7, `surface.cpp` 5, `polygons.cpp` 3, `curves.cpp` 3, `subdivisionCreator.cpp` 1; `geometryDispatch.cpp` (`cluster`, `CPoints::dice`) | 19 + 2 | `IRayContext::scratch()` returning `CMemPage *&` (the macros work on the lvalue) |
| The live varying arrays (`currentShadingState->varying`): write `u`, `v`, `time` (and `I`, `du` for hits), call a shading entry, read `P`, `N` back | `surface.cpp:264` (probe), `:1684` (tessellation), `dlobject.cpp:153`, `implicitSurface.cpp:213`, `geometryDispatch.cpp:386` (`CSurface::shade`), `:416` (`CCurve::dice`) | 6 | `IRayContext::varyings()`; read at the same points as today (the pointer is captured where it is captured now) |
| `displaceEstimate` (`shade(..., 3)`, the dicing probe) | `surface.cpp:304`, `:332`; `geometryDispatch.cpp:455`, `:492` | 4 | `IDicer::probe()` |
| `displace` (`shade(..., TRUE)`, raytrace tessellation) | `surface.cpp:1706`, `:1737`, `:1756`, `:1775`, `:1794` | 5 | `IRayContext::displace()` |
| `shade` (ray hits) | `dlobject.cpp:173` (`SHADING_2D`), `implicitSurface.cpp:242` (`SHADING_0D`, GitHub #119 kept), `geometryDispatch.cpp:405` | 3 | `IRayContext::shade()` |
| `saveState`/`restoreState` | `surface.cpp:1818`, `:2102`, `:2275` | 3 | `IRayContext::saveShadingState()`/`restoreShadingState()` |
| Thread index (`context->thread`) | `surface.cpp:1038` (per-thread tessellation caches) | 1 | `IRayContext::threadIndex()` |
| `CReyes::drawObject`, `drawGrid`, `drawPoints` | `surface.cpp:721`, `:732`, `:742`; `geometryDispatch.cpp` (`CObject::dice`, curves, points) | 3 + 7 | `IDicer` |
| `CReyes::shadingContext()` (to reach the above, `resolve()`, `create()`) | `patches.cpp` 2, `curves.cpp` 1, `polygons.cpp` 1, `subdivisionCreator.cpp` 1, `subdivisionLoop.cpp` 1, `surface.cpp` 3, `geometryDispatch.cpp` 9 | 18 | `IDicer::rayContext()` |
| Pass-through only (the parameter is forwarded, never dereferenced) | `CRegionPredicate::accepts`/`classifyBound`, every `countCrossings`, `classifyOperand*`, `CCSGEvaluator::*`, `countFreeFormCrossings`; `CObject::setChildren` (unnamed, unused, `object.cpp:140`) | — | nullable `IRayContext *` for the region and crossing hooks; **removed** from `setChildren` |

Through `CRenderer::context` (the renderer core): the three `processDelayed*` calls in `geometryDispatch.cpp`
(`:714`, `:733`, `:755`, `:774`), each under `CRenderer::delayedMutex` with a double-checked `processed` flag.
`processDelayedObject`/`processDelayedInstance` (`rendererContext.cpp:441`, `:475`) pass the context only to
`setChildren`, which ignores it. **So the procedural expander needs no scratch or thread handle at all**: this
answers the spec's "opaque handle the procedural expander needs" note.

Callers of Geometry's entry points outside Geometry: `reyes.cpp:374`, `:570` (`dice(this)`), `:1042`
(`regionCoverage(&context, ...)`), `:906`, `:967`, `:1060`, `:1114` (`context.shade(object, ...)`, the forwarding
entry used by the hider itself); `trace.cpp:210` (`shade`), `:338` (`intersect`), `:350` (`cluster`);
`renderer.cpp:1062` and `rendererContext.cpp:465`, `:494` (`setChildren`); `previewContext.cpp:139`
(`instantiate(..., this)`). Shading calls none of them.

Shading's side of the coupling (gotcha 35): `CShadingContext::shade(CSurface *, ...)` (`shading.h:428`,
`shading.cpp:891`), the inline `displace`/`displaceEstimate` (`shading.h:443-444`), `setProducer`/`producerObject`
and `IShadingProducer` (`shadingProducer.h`), implemented by `CHider::shadeSurface` (`shadingProducer.cpp`). No
test calls `setProducer` or matches the producer warning text (grep over `tests/`).

## R2. The interfaces: names, shape, and the diamond

**Decision**: two abstract Geometry-owned interfaces plus the existing host, at global scope like every Geometry
type:

- `IRayContext` (`src/ri/geometry/rayContext.h`): one per render thread. Thread index, scratch arena, varying
  arrays, `shade`, `displace`, save/restore of the shading state, and a non-virtual `frame()` accessor. Implemented
  by `CHider`, so every hider (Reyes and its two derived hiders, raytrace, photon) is one.
- `IDicer` (`src/ri/geometry/dicer.h`): what dicing needs beyond that. `rayContext()` (the dicing thread's
  `IRayContext`), `drawObject`, `drawGrid`, `drawPoints`, `probe`, `inFrustum`, and `frame()`. Implemented by
  `CReyes`.
- `CGeometryHost` (existing, `geometryHost.h`) gains the expansion of delayed objects and instances and the
  coordinate-system lookup (R4). No `IProceduralExpander` class: FR-004 asks to extend the host unless there is a
  reason not to, and there is none (the host is already the registered, thread-safe, renderer-implemented service
  for exactly this kind of expansion).

**Why `IDicer` does not derive from `IRayContext`**: `CReyes` is a `CHider`, and `CHider` implements
`IRayContext`. If `IDicer` derived from `IRayContext`, `CReyes` would hold two `IRayContext` bases (an ambiguous
conversion) or need virtual inheritance across a hot path. An accessor (`IDicer::rayContext()`, answered by
`CReyes` with `*this`) gives the dicing code the same services with no diamond. Named `rayContext()`, not
`context()`, because `CHider` already has a data member called `context`.

**Pointer or reference**: references wherever a context is always required (`dice(IDicer &)`,
`intersect(IRayContext &, CRay *)`, `shade(IRayContext &, ...)`, `cluster`, `materialize`, `create`,
`checkRayGuard`, the tessellation): a call without an implementer cannot be written (FR-007). Nullable pointers
where the contract already says the context "may be NULL" (the region hooks, `regionCoverage`, `countCrossings`,
`classifyOperand*`, `CCSGEvaluator`): the edge case of the spec keeps that contract, and the CSG unit tests that
pass `NULL` compile unchanged.

**Names (P-Q1)**: `IDicer` and `IRayContext` as #37 names them; `I` prefix like the newer interfaces (`ICamera`,
`ITracer`). Alternatives: `CDicingContext`/`CRayContext` (the older Geometry `C` convention), or a namespace
`openrender::geometry` (rejected: it lengthens about 120 signatures for no gain; Geometry has no namespace today
except `motion::`).

**Alternatives rejected**:

- One interface for both sides (`IGeometryContext` with draw methods that the raytracer would have to stub):
  contradicts FR-002/FR-003 and puts dicing members on the raytracer.
- `IDicer : virtual public IRayContext`: virtual inheritance on every dicing call for no benefit over the accessor.
- Keeping `CShadingContext *` and only hiding `CReyes`: fails FR-001.

## R3. The shared frame-limits value

What dicing and ray tessellation read from `CRenderer` (DB-004 link lines and the `geometryDispatch.cpp` reads):
`maxGridSize` (9 uses), `maxEyeSplits` (3), `numThreads` (9), `flags` (motion blur, focal blur), `projection`,
`imagePlane`, `dPixeldx`/`dPixeldy`, `pixelLeft`/`pixelTop`, `clipMin` (`CCurve::dice`), `cocFactorPixels`,
`invFocaldistance`, `raytracingFlags` (`CObject::raytraced()`, inline in `object.h:98`), `inFrustrum`.

**Decision**: one Geometry-owned value `CGeometryFrame` (`src/ri/geometry/geometryFrame.{h,cpp}`), built by the
renderer core once per frame from its statics (**stored copies, never recomputed**, gotcha 26) and installed in a
Geometry-owned slot with the pattern of `FrameParams`: `installGeometryFrame(const CGeometryFrame *)` /
`geometryFrame()`, lock-free, a default value when nothing is installed. Both interfaces expose it through a
non-virtual `frame()` accessor that returns the installed value, so dicing and ray tessellation read the same one
(FR-002), and the readers that hold no interface (`CObject::raytraced()`, the `CTesselationPatch` constructor) read
`geometryFrame()`. **The `CTesselationPatch` destructor does not** (analyze stage, M4): a patch can be destroyed after
the frame is uninstalled, so the constructor stores the thread count (`numThreadsAtBuild`) and the destructor, the
`tesselationOverhead` statistic and `purgeTesselations` use that member. `CTesselationPatch::initTesselations` takes the thread count as a parameter
instead (`initTesselations(geoCacheSize, numThreads)`), because it runs before the install point.

- **Install point (corrected at the tasks stage, 2026-11-09, Q2)**: in `CRenderer::beginFrame()` **after the loop that
  constructs `contexts[i]` (`renderer.cpp:816-832`)**, not at `:805`: `CRaytracer::CRaytracer` (`raytracer.cpp:391`) and
  `CPhotonHider::CPhotonHider` (`photon.cpp:74`) OR `ATTRIBUTES_FLAGS_PRIMARY_VISIBLE` into `raytracingFlags` in their
  constructors, so the flag is final only after the loop. The text below that says "next to `beginFrameParams`" and that
  `raytracingFlags` is final at `:720` is superseded; the other reasoning (after `beginDisplays()` `:796`) stands. Not earlier: `beginDisplays` still writes `flags`
  (`rendererDisplay.cpp:798`, `OPTIONS_FLAGS_DEEP_SHADOW_RENDERING`), so a copy taken before it would be stale in a
  bit Geometry does not read today but could later; no hider's `preDisplaySetup()` writes a copied field (grep).
  Every other field is final by then (the depth-of-field block `:446-458`, the flags `:461-471`, `raytracingFlags`
  `:720`, `beginClipping()`, the network override of `numThreads` `:739-746`). No reader runs before it:
  `initTesselations` (`:755`) receives `numThreads` as a parameter. **Uninstall**: at the end of `endFrame()`, after
  `root->destroy()` and `CTesselationPatch::shutdownTesselations()` (`:920`), so that every reader that still reads the frame (the
  constructor, the dicing code, `raytraced()`) runs inside it; the destructors no longer read it (see above), which
  covers a patch that outlives the frame. Phase 3 reports any `geometryFrame()` read with no frame installed in a
  debug build, not only `raytraced()`.
- **Default** (nothing installed: the preview, the data viewer, unit tests without a frame, and the time between
  frames): every field 0. The preview reads the same today for the one field it can reach
  (`CRenderer::raytracingFlags` is 0 until the first `beginFrame`, `rendererStatics.cpp:196`), so `raytraced()`
  keeps answering false there; a constant `raytraced()` would have changed `setChildren`'s attach behaviour.
  **Between frames** the static is *not* reset (`endFrame` leaves the last frame's value), but no `raytraced()`
  caller runs between frames: the callers are `setChildren` (reached only from lazy creation, expansion, CSG
  resolution and `renderFrame`, all inside a frame), `CRendererContext::capture`/`expand` (`rendererContext.cpp:536`,
  `:599`, render time) and `addObject` while a delayed object is processed (`:664`, render time; an `ObjectBegin`
  record returns before it, `:655-658`), and `renderer.cpp:1011` (in-frame). Phase 3 asserts this in debug (a
  `raytraced()` with no frame installed outside the preview's library logs once).
- **The frustum test** stays the renderer core's: `IDicer::inFrustum()` is answered by `CReyes` with
  `CRenderer::inFrustrum()`, whose `reyes.cpp` allowlist line already exists (no new line). Alternative (B): copy
  the twelve plane coefficients into `CGeometryFrame` and the test into Geometry. Rejected for now: same result
  only if the copied expression contracts identically (FMA on Clang), and it duplicates `rendererClipping.cpp`;
  it is the better shape once the frustum belongs to the camera layer.
- **The pixel transforms**: `surface.cpp:521` and `object.cpp:243` call `camera2pixels(int, float *)`, an inline of
  `renderer.h:529`. `geometryFrame.h` carries the same expression over the frame's copies
  (`camera2pixels(const CGeometryFrame &, int, float *)`), character for character, so the rounding is the same;
  `renderer.h`'s copy stays for `rendererContext.cpp:403`. The duplicate is recorded in the registry history as
  the one expression with two copies, to be merged when the core reads the frame value too.

**Alternatives rejected**: a virtual `frame()` on each interface (two places to answer one value; the slot is
needed anyway for `raytraced()`); putting the limits in `FrameParams` (Graphics State cannot own Geometry's
dicing limits, and the clarify note asks for a Geometry-owned value); passing the frame through every signature
(`raytraced()` and the static tessellation setup have no caller that could pass it).

## R4. Procedural expansion and the coordinate-system lookup

**Decision** (FR-004, FR-010): `CGeometryHost` gains

```cpp
virtual void expandDelayedObject(CDelayedObject *object) = 0;     // once per object: serialized, sets object->processed
virtual void expandDelayedInstance(CDelayedInstance *instance) = 0;
virtual int findCoordinateSystem(const char *name, const float *&from, const float *&to, ECoordinateSystem &system) = 0;
```

- Geometry keeps the outer test of the double-checked pattern (`if (processed == FALSE) host->expand...(this)`); the
  host takes `CRenderer::delayedMutex`, tests `processed` again, calls today's `processDelayedObject` /
  `processDelayedInstance` (minus the unused context parameter) and sets `processed = TRUE`, then unlocks: the same
  lock, the same order, the same frame-held module lifetime (gotcha 27), serialized with `capture()`/`expand()` as
  today. Ray threads and dicing threads take the same path.
- `findCoordinateSystem` moves its body unchanged from `rendererDeclarationsDispatch.cpp` to the host's
  implementation (it needs the core: `definedCoordinateSystems`, `fromWorld`, `context->getXform`).
- **With no host installed** (only a test with no renderer can get there; the preview never dices or intersects):
  the expansion reports `CODE_BUG` once and leaves the object unexpanded; `makeBound` treats a missing host like an
  unknown coordinate system (the plain `maxDisplacement`). Today the preview build would abort on the stub's
  `assert(FALSE)` if a scene gave a `displacementbound` coordinate system; no test depends on that (grep).

**Variable lookup (`blobby.cpp`)**: `CRenderer::retrieveVariable` is literally `return CDeclarations::find(name)`
(`rendererDeclarations.cpp:215-217`), and `CDeclarations` is Graphics State, which Geometry may use. **Decision
(P-Q4, decided 2026-11-07: yes)**: `blobby.cpp` calls `CDeclarations::find` directly; no wrapper service. Alternative: a
`CGeometryHost::retrieveVariable` (or a Geometry-owned lookup interface) as US5 AS2's wording ("a Geometry-owned
service") suggests; rejected because its only implementation would forward to Graphics State, and it would give
the preview a lookup that needs a host it does not have (the preview holds a `CRiStandardSession`, which acquires
`CDeclarations`, so the direct call works there).

## R5. The object sink

**Decision** (FR-028): `src/ri/geometry/objectSink.h`, header only:

```cpp
class IObjectSink {
    public:
        virtual ~IObjectSink() = default;
        virtual void addObject(CObject *object) = 0;
};
```

`CRiInterface : public IObjectSink` (`parse/riInterface.h`), keeping its default no-op `addObject` as the override;
`CObject::instantiate(CAttributes *, CXform *, IObjectSink *) const`. Scene Interface -> Geometry is allowed; the
header is inline-only, so `libRIBOut` (which compiles no Geometry source but has `src/ri/geometry` on its include
path through `src/CMakeLists.txt:18`) needs no new object. `riInterface.h` is not an installed header
(`CMake/openRenderPublicHeaders.cmake`), so the base-class change touches no public ABI. The 24 `addObject` calls
in 9 files and the three callers keep their text.

## R6. The three pairs, `libRIBVector` and the residual list

| Pair | Today | After |
|---|---|---|
| `render/geometryDispatch.cpp` / `vector/geometryDispatchStub.cpp` | Geometry bodies in the renderer directory, error-and-assert bodies for `libRIBVector` | bodies move to their classes' files: `CObject::dice`, `cluster` -> `object.cpp`; `CSurface::intersect`, `dice`, `shade`, `checkRayGuard`, `regionCoverage` -> `surface.cpp` (which becomes the key-function owner of `CSurface`'s vtable); `CCurve::dice`, `CCubicCurve`/`CLinearCurve::splitToChildren` -> `curves.cpp`; `CPoints::dice` -> `points.cpp`; `CDelayedObject`/`CDelayedInstance::intersect`/`dice` -> `delayed.cpp`; `CCSGSolid::dice` -> `csgSolid.cpp`. Both files deleted. |
| `geometry/surfaceCrossingDispatch.cpp` / `vector/surfaceCrossingDispatchStub.cpp` | the Newton core is already renderer-free (`object.h`, `crossingMath.h` only); it was excluded only because `surface.cpp` was | renamed `geometry/surfaceCrossings.{h,cpp}` (the "Dispatch" name described the substitution), compiled for both libraries; `countFreeFormCrossings(IRayContext *, ...)`. Stub deleted. |
| `render/rendererDeclarationsDispatch.cpp` / `vector/rendererDeclarationsDispatchStub.cpp` | `CRenderer::notifyGlobalVariable` and `findCoordinateSystem` | `notifyGlobalVariable` moves to `render/rendererDeclarations.cpp` (the file leaves `libRIBVector`, see below, so it may reach the shading contexts); `findCoordinateSystem` becomes `CRendererContext`'s host override (R4). Both files deleted. |
| `hiders/irradianceDispatch.cpp` / `vector/irradianceDispatchStub.cpp` | Hiders' | **untouched** (spec 032). |

`libRIBVector` after the change (`src/ri/CMakeLists.txt:386-432`):

- **Joins**: `geometry/surface.cpp` and `geometry/surfaceCrossings.cpp`. Needed: the quadrics, patches, polygons,
  curves and subdivision units in `libRIBVector` reference `CSurface`'s vtable (now defined in `surface.cpp`),
  `CPatch`/`CTesselationPatch` (constructed by `CSurface::dice`/`intersect`) and the free-form crossing count.
  After the change both reach only Geometry, Graphics State and Infra symbols plus the two interfaces' virtual
  calls, which need no symbol.
- **Stays excluded**: the CSG units, `dlobject.cpp`, `implicitSurface.cpp`. Nothing in `libRIBVector` references
  them (the preview has no `SolidBegin`, no `dlo`, no implicit); they are whole classes the preview does not need,
  not stand-ins, so FR-008/FR-011's "no stub for a Geometry symbol" holds. Compiling them would pull the Procedures
  loader into `libRIBVector` for nothing. **(P-Q5, decided 2026-11-07: they stay out.)**
- **Leaves**: `render/rendererDeclarations.cpp` (its only `libRIBVector` client was `blobby.cpp`), the three stubs.
- **Residual list** (`tests/architecture/ribvector-core-residual.txt`): 9 -> 5 lines. Deleted: the three stubs and
  `rendererDeclarations.cpp`. Kept: `rendererFileSearch.cpp`, `rendererNetwork.cpp`, `rendererMutexes.cpp` (for
  `blobbyRepeller.cpp`, DB-004's remainder), `rendererStatics.cpp` (its "needed by" cell loses `object.cpp`;
  `blobbyRepeller.cpp` and the Hiders files stay), `irradianceDispatchStub.cpp` (spec 032's).
- **The `nm -u` check** (spec's Assumptions): on the Phase 6 build, `nm -u` over every `libRIBVector` object must
  name no `CShadingContext`, `CReyes`, `CHider`, `CRenderer::` symbol other than the residual list's, and the
  preview and data-viewer links must succeed with no stub (quickstart §6).

## R7. Include fallout: what breaks when Geometry stops including the core and Shading

Geometry headers that other domains include: `object.h` (includes `renderer.h`), `objectMisc.h` (`renderer.h`),
`points.h` and `curves.h` (`shading.h`), `quadrics.h` (`shader.h`). `renderer.h` itself includes `shader.h`. Files
outside Geometry that relied on these transitively must get the declarations another way, and a Hiders,
Texturing or Graphics State file **cannot** add a direct `renderer.h` include: that would be a new allowlist line
(FR-017 forbids it).

Static reading of every non-Geometry file that uses `CRenderer::` without including `renderer.h` and reaches it
through a Geometry header:

- **`hiders/photonMap.cpp`** (4 references: `CRenderer::fromWorld`/`toWorld` at `:102-103`, `:212`, `:217`) includes
  `object.h` and `raytracer.h`, and neither path reaches `renderer.h` once `object.h` drops it. This is the one
  Hiders file that breaks. **(P-Q3) Decided 2026-11-07: (a).** Spec 032 deletes the same two allowlist lines (137-138) in its Phase 5; whichever spec merges second drops its duplicate edit. The two ways out were:
  - (a, recommended) `photonMap.cpp` reads the camera matrices from `frameParams().camera.fromWorld/toWorld`
    (Graphics State, allowed). The copies are bit-identical to the statics (`rendererFrame.cpp:54-55`, pinned by
    `Motion_FrameParamsEqualCore`). Every photon map is read and written inside a frame (the hider's destructor
    writes them before `endFrameParams()`, `renderer.cpp:854-860`), and outside a frame the default value has
    identity camera matrices, which is what the data viewer seeds into the statics today (`dataLoad.cpp:183-186`).
    It deletes the 2 DB-011 lines `photonMap.cpp -> CRenderer::fromWorld/toWorld` as well. It touches a file spec
    032 moves into Texturing (032 must remove the same reads, and both specs delete the same 2 lines).
  - (b) keep `object.h -> renderer.h` as the one DB-004 include line left beyond `blobbyRepeller.cpp`'s, until
    `photonMap.cpp` stops reading the statics (032 or a follow-up). DB-004 ends at 4 lines, not 3 (SC-005/FR-016
    as written are not met).
- Hiders files that include `reyes.h` or `pixelFilter.h` (both allowlisted `-> renderer.h`), or `renderer.h`
  directly, are unaffected: `reyes.cpp`, `stochastic.cpp`, `zbuffer.cpp`, `raytracer.cpp`, `photon.cpp`,
  `trace.cpp`, `compositor.cpp`, `irradiance.cpp`.
- Texturing, Graphics State, Scene Interface and Shading include none of these Geometry headers (grep).
- Core and Consumer files (the renderer, tests, the preview, the data viewer) may include anything; a test file
  that needs an added include is a mechanical edit of an approved test and is listed (R13).

The static reading cannot see every transitive use (macros, inline helpers, `stats`, `CDisplayChannel`, option
constants). **Phase 1 runs an include-fallout probe (measure at Phase 1)**: on a scratch copy, delete the
`renderer.h`/`rendererContext.h`/`shading.h`/`shader.h`/`reyes.h` includes from the Geometry headers and compile
every translation unit with `-fsyntax-only` through `cpujob`; the list of failing non-Geometry files is recorded
in `tools/include-fallout.txt` and every Hiders/Texturing/State entry on it is a stop-and-ask item if it is not
`photonMap.cpp`.

## R8. Reyes on the shared layer (FR-012, FR-013, FR-014)

The 11 DB-011 lines (verified): `reyes.cpp -> CRenderer::aperture, cameraHasRotation, cocFactorSamples,
cocFactorScreen, invFocaldistance, relRotQ, relTrans`; `stochastic.cpp -> cameraHasRotation, cameraRotationOnly,
relRotQ, relTrans` (`stochasticQuad.h` and `stochasticPoint.h` are included by `stochastic.cpp`, so their reads count
there). `cocFactorScreen` is reached through the `cocScreen(z)` macro of `renderer.h:43` at `reyes.cpp:617-618`.

- **Rotation**: every site is `slerpq(Rjt, identQ, q, t); qtoR(M, Rjt);` with `identQ = {0,0,0,1}` declared either
  as `static const quaternion` or `const float[4]` (same values): `reyes.cpp:1426-1434`, `:1599-1605`, `:1636-1643`;
  `stochastic.cpp:213-219` (with `invRotQ`, the conjugate built at `:165-170`); `stochasticQuad.h:497-507`,
  `:859-871`. `motion::cameraRotationAt(M, q, t)` (`motion.h:153-158`) is exactly that pair, and
  `Motion_CameraSlerpEquivalence` pins it bit for bit to Reyes's SLERP. **Decision**: each site calls
  `motion::cameraRotationAt` with `frame.camera.relRotQ` (or the conjugate), and the translation keeps its own
  expression `jt * frame.camera.relTrans[k]` (gotcha 26: the sites keep their expressions; only their inputs move).
  The flags come from `frame.camera.hasRotation`/`rotationOnly` (bool copies of the int statics; the same truth).
  Hiders -> Geometry (`motion.h`) and Hiders -> Graphics State (`frameParams.h`) are allowed.
- **Per-pixel reads**: the rasterizer reads these per pixel sample. The hider takes a `const FrameParams &` once
  per bucket (`rasterBegin`) instead of calling `frameParams()` per sample.
- **Lens values (until the lens phase)**: `FrameParams` gains `cocFactorScreen` and `cocFactorSamples`, stored
  copies written by `beginFrameParams` from the statics (never recomputed; `LensModel::cocFactorScreen()` is the
  same expression and pinned equal by `Camera_LensModel`, but a copy is the rule). Reyes reads `aperture`,
  `invFocaldistance`, `cocFactorScreen`, `cocFactorSamples` from `FrameParams`; `cocScreen(z)` becomes a local
  expression with the same operand order: `absf((1 / z) - fp.invFocaldistance) * fp.cocFactorScreen`.
  `test_frame_params_equal_core.cpp` is extended to pin the two new fields (an approved-test edit, R13).
- **Result**: the 11 lines are deleted, the Hiders `CRenderer::` metric falls by about 53 references (measure at
  Phase 7), and no motion expression changes (FR-014): `Motion_Replay_*` is never re-recorded.

## R9. The lens derivation (FR-022): verified against the code

### The raytracer (`src/ri/state/camera.cpp:94-118`, `CFrameCamera::generateRay`) — **confirmed**

`filmPoint(P, x, y, z)` (`:79-92`) maps a full-image pixel to the camera-space point `(s_x·z/ip, s_y·z/ip, z)` in
perspective (`s` the screen position, `ip` = `imagePlane`). With a lens (`aperture != 0`, the lens radius
`A = focallength / (2·fstop)`, `renderer.cpp:446`):

- origin `from = filmPoint(x, y, 0) = (0, 0, 0)`, then `from += lensUV · A` (`:109`, `:113-114`), `lensUV` on the unit
  disk (the concentric map of the lens slot, `sampler.cpp:98`);
- aim `to = filmPoint(x, y, f) = (s_x·f/ip, s_y·f/ip, f)` (`:110`), `f` = `focaldistance`.

The ray reaches depth `z` at `X = lu·A + (s_x·f/ip − lu·A)·z/f`. Projecting, `ip·X/z = s_x + ip·A·lu·(1/z − 1/f)`.
So a point at depth `z` is seen by the film sample `s` through lens point `lu` where
`s = s_q − ip·A·lu·(1/z − 1/f)`: **the screen-space blur disk has radius `ip·A·|1/z − 1/f|`** (a radius, since
`lensUV` is on the unit disk and `A` is the lens radius), and its image is displaced by `−lu·ip·A·(1/z − 1/f)`:
mirrored between the near and far sides of the focal plane, as through a real lens. In the orthographic branch the
same construction gives `A·|1 − z/f|`, but it is unreachable (below).

This is the exact thin-lens result. With a real focal length `F`, the RISpec circle of confusion on the film at
image distance `v = fF/(f − F)` is `(F/N)·F·|z − f| / (z·(f − F))` (diameter); divided by `v` to normalize to the
film plane at unit distance and halved, it is `A·|1/f − 1/z|`, and `ip` scales normalized coordinates to the screen
window. The magnification `f/(f − F)` cancels.

### Reyes (`renderer.cpp:446-458`, `reyes.cpp`, `stochasticQuad.h`) — **confirmed, with a second, larger factor**

- **Screen factor**: `cocFactorScreen = ip·A·f/(f + A)` (`renderer.cpp:455`; the same expression is
  `LensModel::cocFactorScreen()`, `camera.cpp:35-39`), and Reyes's screen circle of confusion is
  `|1/z − 1/f|·cocFactorScreen` (`cocScreen`, `renderer.h:43`): **the raytracer's value times `f/(f + A)`**, as the
  brief read it. The factor has no physical basis (the exact magnification term cancels, above; it reads like
  `f/(f − F)` with the aperture substituted for the focal length). For the scenes: `DepthOfField 2 1 5` (A = 0.25)
  gives 0.952; `sampling-frames.rib` (`2.8 0.5 5`, A = 0.0893) gives 0.982.
- **What the rasterizer actually scatters is larger again**: the per-vertex value is
  `vertices[9] = |1/z − 1/f|·cocFactorSamples` (`reyes.cpp:1164`, `sampler.cpp:23-24`), with
  `cocFactorSamples = cocFactorScreen·sqrt((dPixeldx·pixelXsamples)² + (dPixeldy·pixelYsamples)²)`
  (`renderer.cpp:456`): the **diagonal** of one screen unit in sample space. It is applied to **each** axis of the
  sample-space vertex (`v + jdx·v0d`, `stochasticQuad.h:614-622`, `:903-911`; `stochasticPoint.h:319-320`), with
  `jdx, jdy` the same unit-disk lens point (`stochastic.cpp:206-207`); no compensating scale exists (grep for
  `jdx`, `jdy`, `[9]`). Sample space scales screen x by `dSampledx = dPixeldx·pixelXsamples` (`renderer.cpp:417`), so
  in screen units along x the scatter is `|1/z − 1/f|·ip·A·f/(f + A)·sqrt(1 + (dSampledy/dSampledx)²)`: an extra
  **`sqrt(2)` for square pixels and equal pixel samples**, and an elliptical blur when `PixelSamples` differs per
  axis.
- **The sign**: `absf` (`sampler.cpp:24`) drops the side of the focal plane, so a foreground and a background point
  are displaced the same way for one lens sample, where the lens displaces them in opposite directions. This is the
  "DOF occlusion model" residual D9 of spec 008 (`DEVNOTES.md:16`, `RISPEC_GAPS.md:60`), and it is closable: it is a
  sign, not an algorithmic limit.

**Result**: Reyes's blur radius is `sqrt(2)·f/(f + A)` times the physical one for square sampling: **1.35x** for
`DepthOfField 2 1 5`, 1.39x for `sampling-frames.rib`. Most of the `Parity_dof` divergence (about 46/255 against a
threshold of 60, `tests/visual/CMakeLists.txt:1396-1400`, `:1894-1897`) is this, not a structural residual. Spec
008's D9 text and `sampler.h:90-94` are corrected to this in the lens phase (FR-022).

### Two related findings (reported, not fixed before the lens phase)

- **Bound inconsistency**: the bucket-assignment bound of an object (`reyes.cpp:616-636`) expands by `cocScreen`
  per axis and then converts with `dSampledx`/`dSampledy`, which is narrower than the scatter by the same
  `sqrt(1 + (dSy/dSx)²)`; the grid-level bounds (`:1466-1480`, `:1553-1559`, `:1738-1750`, `:1886-1888`) use the
  scattered value. A strongly defocused primitive near a bucket edge can therefore be skipped by a bucket its blur
  reaches. Possibly related to #112 (silhouette dropout). **Decided 2026-11-07**: fixed inside the lens phase (FR-031),
  verified by `Hiders_ReyesBucketBoundCoversCoC`; the #112 link is reported, not claimed.
- **Orthographic**: `renderer.cpp:447` forces `aperture = 0` when the projection is orthographic, for both hiders
  (`FrameParams.aperture` is the same static). Today no orthographic render has depth of field. **Decided 2026-11-07 (P-Q10): it stays off in both hiders**; enabling it is out of scope (a possible follow-up).
- **zbuffer** has no focal blur at all (no `FOCALBLUR`/`aperture`/`jdx` in `zbuffer*.{h,cpp}`), as it has no
  motion blur; "reyes" and "stochastic" are the same hider (`CStochastic`). The hiders with depth of field are
  therefore Reyes and raytrace; US7 AS2's four-hider statement reduces to these two (zbuffer stays sharp). **(P-Q9, decided 2026-11-07; spec corrected.)**

### The lens-phase design (decided 2026-11-07: P-Q8 (a), P-Q10 off, P-Q11 accepted, bucket bound in scope as FR-031)

- **The reference (P-Q8)**: (a, recommended) the raytracer's existing construction, i.e. the exact thin lens above.
  Then `CFrameCamera::generateRay` keeps its bits, raytrace depth-of-field images do not change from the formula,
  and only Reyes's change. (b) A model carrying RISpec's focal-length magnification explicitly: identical in
  normalized screen units (the magnification cancels), so it is (a) under another name; it would only differ if the
  maintainer wants film-plane units exposed. No option changes the raytracer's blur diameter.
- **One function**: `LensModel::circleOfConfusion(float z) const` in `camera.{h,cpp}` (Graphics State), inline:
  the **signed** screen-space displacement per unit lens offset, `imagePlane·aperture·(invFocaldistance − 1/z)` in
  perspective, 0 with no lens (and 0 in orthographic unless P-Q10 enables it). `LensModel::fromFrame(const
  FrameParams &)` builds the value; `CFrameCamera::lensModel()` returns it. `CSampler::circleOfConfusion`,
  `LensModel::cocFactorScreen()` and the `FrameParams` copies `cocFactorScreen`/`cocFactorSamples` are deleted.
- **Reyes takes it from that function**: `vertices[9] = lens.circleOfConfusion(z)` (screen units, signed), and the
  per-axis sample scale moves into the lens point: `pixel->jdx = lensU·dSampledx`, `pixel->jdy = lensV·dSampledy`
  (`stochastic.cpp:206-207`). The raster expressions `v + jdx·v0d` stay as they are; the scatter becomes exactly the
  raytracer's displacement for the same lens sample, including the sign and the per-axis scale (derivation in
  [contracts/lens-model.md](contracts/lens-model.md)). The bounds use `|c|·|dSampledx|` and `|c|·|dSampledy|` per
  axis (`reyes.cpp:616-636`, `:1466-1480`, `:1553-1559`, `:1738-1750`, `:1886-1888`). The lens sample stream
  (`CSampler::cameraSample`, spec 039's lens slot) is untouched: no sampler-stream change, no `PixelSamples` change.
- **The raytracer takes it from that function** in the sense of SC-009: `generateRay` stays the ray form of the
  same lens (R9 above), and a new analytic test pins the two forms together (the ray through film sample `s` and
  lens point `l` meets depth `z` at the screen position `s − l·circleOfConfusion(z)`, signs included). Rewriting
  `generateRay` to call `LensModel` helpers was considered and rejected: it gains nothing and risks the raytracer's
  bits (the compound `from[X] += lensUV[0]·aperture` may contract differently once split).
- **Dicing**: `minCocPixels` (`surface.cpp:50-59`) takes `|circleOfConfusion(z)|` and keeps its pixel conversion
  `sqrt(dPixeldx² + dPixeldy²)` (a shading-rate relaxation heuristic, not the blur itself); only the
  `f/(f + A)` factor leaves it. The `CGeometryFrame` copies `cocFactorPixels`/`invFocaldistance` are replaced by a
  `LensModel` member of the frame value.
- **What changes**: every Reyes depth-of-field image (blur radius about 26-28 % smaller for the test scenes, the
  near-field displacement mirrored, per-axis correct). Reyes's smoothness (decoupled shading) is kept; its extra
  softness from the 1.35x over-blur is not. **(P-Q11)**

## R10. The raytracer's depth-of-field noise (FR-025): the investigation, not the fix

The investigation is a lens-phase task that produces a report before any change. Method: the visual-test noise
method (render the same scene twice with different seeds, measure the run-to-run block difference, compare
against the hider's own variance), on `camera-dof-raytrace`, `parity/dof-raytrace`, and a new scratch scene with a
small bright defocused highlight and a defocused foreground over a sharp background (US7 AS4; scratch, not a
tracked scene, unless approved).

Hypotheses, each with the measurement that decides it:

1. **Shading per lens sample** (most likely): the raytracer shades every ray hit, so a specular highlight on a
   defocused surface is sampled at 9 different points per pixel (`PixelSamples 3 3`) and a few samples catch it:
   isolated bright samples. Reyes shades each micropolygon once and scatters the shaded colour. Test: the same scene
   with a matte shader; if the speckle goes, this is the cause.
2. **Too few lens samples for the blur area**: 9 samples per pixel over a disk several pixels wide under-reconstruct
   the bokeh in both hiders; Reyes hides it with shading coherence. Test: speckle against `PixelSamples`, and the
   blur area in pixels against samples per pixel.
3. **Correlation between the film and lens dimensions** of the camera slots (`cameraDraws`, spec 039): structured
   rather than white noise. Test: a 2D histogram of (film jitter, lens point) per pixel and the noise spectrum.
4. **The pixel filter relative to the blur**: reconstruction with a narrow filter shows each lens sample. Test:
   noise against `PixelFilter` width.

Allowed changes (FR-025, FR-030), each put to the maintainer before it is made: the lens point mapping within the
lens slot (another disk map, or stratification of the existing one), and filtering; neither may change the blur
diameter (pinned by the analytic test). **Stop-and-ask**: any change to the sampler's streams or dimensions, to the
`PixelSamples` default, to adaptive sampling, or to the decoupling of shading from visibility in the raytracer.

## R11. The depth-of-field scene set (FR-029)

`grep -l DepthOfField` over `examples/` and `tests/`:

| Scene | Used by | Hider |
|---|---|---|
| `examples/rib/camera-dof-{reyes,raytrace}.rib` | determinism sweep (`top-camera-dof-*`, self-consistency) | both |
| `examples/rib/tests/camera-dof-{reyes,raytrace}.rib` | `Visual_camera-dof-{reyes,raytrace}` (thresholds default, 30), sweep | both |
| `examples/rib/tests/camera-motion-small+dof-{reyes,raytrace}.rib` | `Visual_camera-motion-small-dof-*` (90, 110), sweep | both |
| `examples/rib/tests/parity/dof-{reyes,raytrace}.rib` | `Parity_dof` (60), sweep | both |
| `examples/rib/tests/parity/dof-motion-{reyes,raytrace}.rib` | `Parity_dof-motion` (35), sweep | both |
| `examples/rib/tests/sampling-frames.rib` | `Sampling_FrameReproducible` (two frames compared with each other, no reference) | raytrace |
| `tests/convergence/scenes/dof-{reyes,raytrace}.rib.in` | convergence (`tests/convergence/reference/dof-*.tif`) | both |

`examples/rib/camera-dof.rib` has **no** `DepthOfField` statement (despite its name): it must stay byte-identical,
as must every scene not in the table. The DoF references that may change, image by image with approval:
`examples/rib/tests/references/{camera-dof-*,camera-motion-small+dof-*}.tif`, the parity scenes' rendered pairs (no
stored reference: the parity tests compare the two hiders), and `tests/convergence/reference/dof-{reyes,raytrace}.tif`.
Under recommendation P-Q8 (a), only the Reyes ones are expected to change from the formula.

## R12. Proving "no image changes" (SC-001)

Spec 029's method (its research R13), copied: Phase 1 records on the starting tree, on macOS and on Linux (GCC 13,
`-Werror`, the per-spec remote tree, remote `cpujob -j2`), each against itself:

- the sha256 of every image the suite writes (`-LE determinism-sweep`) and of every image the determinism sweep
  writes, plus a noise list from a second run of each (`image_manifest.sh`, copied unchanged from spec 029's tools);
- the `MaxBlockAvgDiff` list of the suite;
- `archcheck`'s report, the metric, and `nm -u` of `libRIBVector`.

Every phase from 3 to 8 ends with the suite manifest and the `MaxBlockAvgDiff` list compared against the baseline
(any differing byte outside the noise list stops the phase), `Motion_Replay_*` and `Motion_CameraSlerpEquivalence`.
Phase 8 runs the sweep on both platforms and is the gate before the lens phase (FR-029). After the lens phase, the
manifest diff may list only the R11 images.

Timing (no measurable change expected): spec 029's `timing_sc010.sh` (scaled scratch copies) on a heavy Reyes grid
scene, a displaced raytrace scene and a CSG scene, five interleaved series against the starting tree's binaries.
Added per call: a virtual `drawObject`/`drawGrid`/`drawPoints` (once per object or grid), a virtual `varyings()`
(a few per dice), the shading calls already virtual (the producer) and now one hop shorter.

## R13. Tests: new ones and edits to approved ones (all for the maintainer's approval)

**New tests** (written red first, Phase 2; full list with labels in [contracts/tests.md](contracts/tests.md)):
`Geometry_DiceThroughDicer`, `Geometry_IntersectThroughRayContext`, `Geometry_ObjectSink`,
`Geometry_FrameDefault`, `Geometry_ExpansionThroughHost`, `Geometry_NoHiderShadingNames`,
`Geometry_RibVectorNoStubs`; lens phase: `Camera_LensCircleOfConfusion`, `Camera_LensRayMatchesCoC`,
`DoF_BlurDiameter_{reyes,raytrace}` (a measurement scene, proposed, may be dropped).

**Edits to approved tests** (mechanical unless noted; the inventory is re-measured at Phase 1 with the fallout
probe and written to `tools/test-edit-inventory.txt`):

- the `instantiate(..., CRiInterface *)` overrides (signature to `IObjectSink *`): `tests/unit/csg/test_boundary_rule.cpp:56`,
  `test_lazy_resolution.cpp:170`, `test_region_inheritance.cpp:32`, `tests/shading_contract/rendered_harness.h:45`,
  `tests/shading_parity/test_ambient_accumulation.cpp:75`, `texture_substitute_harness.h:63`, `test_false_if_constructs.cpp:82`;
- the `intersect(CShadingContext *, CRay *)` overrides: `test_lazy_resolution.cpp:169`, `test_region_inheritance.cpp:31`
  (and any `CSurface` subclass the probe finds);
- the `CShadingContext *` region and crossing overrides: `test_region_inheritance.cpp:37-38`, `test_boundary_rule.cpp:44`;
- `setChildren(NULL, ...)` (parameter removed): `test_region_inheritance.cpp:59`, `:72`, `:81`, `:93`, `:97`;
- the `ctx()` helpers and their callers (`CShadingContext *` -> `IRayContext *`, `&contexts[0]->shadingContext()` ->
  `contexts[0]`, and `*ctx` where a reference is now required): `tests/unit/csg/csgTestUtils.h:158`, `:180-214`,
  `test_count_crossings.cpp`, `test_classify_quadrics.cpp`, `test_parity_robustness.cpp`, `test_classify_determinism.cpp`,
  `test_multi_primitive_leaf.cpp`, `test_procedural_operand.cpp`, `test_lazy_resolution.cpp`,
  `test_interior_exterior_selection.cpp`, `tests/unit/sampling/scene_probe.h:134`, `test_object_ids.cpp:237`;
- the motion replay harness (never the recordings): `tests/unit/motion/replay_capture.h:166`, `:184-185`,
  `motion_replay.cpp:67` (the member-pointer type of `CCurveMesh::create`), `:503`, `:529`, `:580`, `:691-702`;
- `tests/unit/motion/test_frame_params_equal_core.cpp`: two lines added for the new `FrameParams` copies (Phase 7)
  and removed again in the lens phase (not mechanical: a pinned field list);
- lens phase: `tests/unit/camera/test_camera_lens_model.cpp` (pins `cocFactorScreen()` bit for bit to the Reyes
  expression; the function goes, so the check is replaced by one against `circleOfConfusion`; not mechanical).

No threshold, reference, replay or manifest is changed by these edits.

## R14. The other specs of this round

All three sibling `plan.md` files are still the 127-line template on 2026-11-07, so overlaps are judged from their
`spec.md`; the analyze stage re-checks them against their plans.

| Shared item | 030 (registry) | 032 (GI cache split) | 056 (volume segments) | How 031 avoids colliding |
|---|---|---|---|---|
| `src/ri/render/renderer.cpp` `beginFrame()` | replaces the two `strcmp` chains (`:778-792`, `:815-834`) | — | — | 031 touches other lines only: the frame-value install after the context loop (`:816-832`; the thread count reaches `initTesselations` at `:755` as a parameter), the uninstall at the end of `endFrame()`, `:1062` (`setChildren`), and in the lens phase `:446-458`; no hider selection |
| `src/ri/hiders/hider.{h,cpp}` | may add registration and capabilities to `CHider` | — | — | 031 adds the `IRayContext` base and its overrides and drops `IShadingProducer`; a textual merge in the class head, resolved by the second merger |
| `reyes.cpp` DB-011 lines, `metrics-baseline.txt` Hiders row | deletes `reyes.cpp -> CRenderer::contexts` (and maybe `numThreads`), lowers the Hiders row | deletes 5 DB-011 lines (irradiance, photonMap) | — | different lines of the same block; the Hiders row is re-measured by whichever merges second |
| `photonMap.cpp -> CRenderer::fromWorld/toWorld` (2 DB-011 lines) | — | removes the reads (storage moves to Texturing) | — | only under P-Q3 (a): both delete the same two lines (a clean merge); the second merger records 0 for them |
| DB-025 | — | deletes 11 lines | — | 031 deletes `surface.cpp -> debug.h` and adds a history entry; whichever merges second moves DB-025 to Resolved |
| `geometry/surface.cpp` | — | **does not edit it** (032 FR-016, correcting the brief) | — | no overlap |
| `tests/architecture/ribvector-core-residual.txt` | — | deletes `irradianceDispatchStub.cpp`; edits the `rendererStatics.cpp` row's Hiders part | — | 031 deletes 4 other lines and removes only `object.cpp` from the `rendererStatics.cpp` row's "needed by" cell: the same line, a textual conflict the second merger resolves |
| `src/ri/CMakeLists.txt` `ribVector_sources` | removes `hcshader.cpp` (FR-025) | removes `irradianceDispatch.cpp`/its stub | — | adjacent edits in the same two lists |
| `kRendererServicesVersion` (`rendererServices.h`) | — | steps it if a GI service signature changes (032 FR-009) | — | 031 steps 2.1.0 -> 2.2.0 (R15); whichever merges second takes the next MINOR and renumbers its entry |
| `src/libshader/shading/shading.{h,cpp}` | — | one shared irradiance function | edits the `shade()` tail (`shading.cpp:1396-1456`) | 031 deletes the forwarding entry (`shading.cpp:883-900`), `displace`/`displaceEstimate`, `setProducer`/`producerObject`, and moves `EShadingDim` out of `shading.h:84-89`; other regions |
| `geometry/csgSolid.cpp` | — | — | removes the fragment flag at `:273` in `CCSGSolid::build` | 031 changes `build`'s and `resolve`'s signatures (`:199`, `:227`), `materializeSurfaces` (`:93`), `:317`, `:379`, the include at `:30`, `instantiate` at `:186`, and adds `dice`; line `:273` itself is not edited |
| `hiders/trace.cpp` | — | — | `traceBundle`/`enterRayLevel` (`:255-266`) | 031 edits `:210`, `:338`, `:350` only |

No task of 031 depends on another spec's merge; P-Q3 (a) works whether 032 lands first or not.

## R15. The Shading boundary version

Retiring `CShadingContext::shade(CSurface *, ...)`, `displace`, `displaceEstimate`, `setProducer` and
`IShadingProducer` removes items the 2.1.0 history entry of `rendererServices.h` lists, and `EShadingDim` moves to
`src/common/shadingDim.h` (same name, same values, still visible through `shading.h`). **Decision (P-Q7, decided 2026-11-07: yes; renumbered to the next MINOR if 032 steps it first)**:
`kRendererServicesVersion` 2.1.0 -> **2.2.0**, one history entry: "Removed the Geometry-facing entries and
`IShadingProducer` (spec 031); `EShadingDim` now comes from `common/shadingDim.h`. Breaking, allowed by the 2.x
policy (MAJOR stays 2 until spec 055)." Alternative: no step (the removed items were never used outside the
tree); rejected because gotcha 28 asks for a step on every interface change.

`EShadingDim` must live in Infra, not Geometry: `shading.h` names it and Shading may not depend on Geometry.

## R16. Bugs near this work (not fixed here)

- **#119** (implicit hits shaded 0-D): `implicitSurface.cpp:242` keeps `SHADING_0D` through `IRayContext::shade`.
- **#144** (implicit surfaces in camera space): untouched; the signature change is mechanical.
- **#108**, **#110** (`dPdtime` in bilinear patches and patch grids): the motion expressions are not touched (FR-014).
- **#112** (Reyes silhouette dropout): the bound inconsistency of R9 may contribute; the lens phase makes the bounds
  consistent, and the result is reported against #112 without claiming a fix.
- **#127** (Reyes grid data race): `drawGrid` becomes a virtual call with the same body; no locking changes.
- **#72** (Reyes crop dicing): `pixelLeft`/`pixelTop` keep entering dicing as copies.
- The others listed under Scope Boundaries sit on lines this spec only re-signs; behaviour stays as it is.

## R17. Counts (plan estimate; Phase 1 and each phase re-measure)

| Entry | Start (`e83d005d`) | Deleted by 031 | End |
|---|---|---|---|
| DB-006 | 30 | 30 | 0 (Resolved) |
| DB-017 | 78 | 78 | 0 lines; Open for the irradiance pair (032) |
| DB-004 | 52 | 49 | 3 |
| DB-011 | 164 | 13 (11 + `photonMap.cpp`'s 2, which 032 also deletes) | 151 |
| DB-025 | 12 | 1 | 11 (032 deletes them) |
| **Allowlist** | **344** | **171** | **173** |

SC-007 now reads 344 -> 173 (corrected 2026-11-07). If 032 merges first, its deletion of `photonMap.cpp`'s two
lines counts once. Metric: Geometry 48/15 -> 2/2 (`locateFile`, `texturePath`); Hiders 556/81 -> about 499
under (a) (measure at each phase).
