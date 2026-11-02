# Research: Hider / Shading Separation (spec 029)

**Status**: plan stage, 2026-11-02. **Decided 2026-11-02**: the maintainer accepted the recommended option of every
question, P-Q1 to P-Q16, and replaced P-Q8 by a versioning policy (R11: `kRendererServicesVersion` 2.0.0 -> 2.1.0, MAJOR
stays at 2 until spec 055). Each question is kept as written at the plan stage, followed by its decision; "the
design" in this file is the decided option. The answers are recorded in spec.md, Clarifications, Session 2026-11-02.

**Method**: static reading of the tree at master `d6910bd0` in the worktree, `grep` counts, the spec 026
records under `specs/026-shading-contract-v2/tools/`, and two read-only inventories (the hiders' and
Geometry's uses of inherited `CShadingContext` members; the link wiring and the tests that exist because of
it). No build was run; Phase 1 re-measures everything that a build decides (R15).

---

## R1. The Geometry-facing `shade()` entry: the one conflict inside the spec (P-Q1)

**Finding.** Three Geometry sources call `CShadingContext::shade(CSurface *, int, int, EShadingDim,
unsigned int, int)` by that exact mangled symbol, and the allowlist keys each call by (source file, symbol):

| Allowlist line | Call site |
|---|---|
| `allowlist.txt:109` DB-006 `dlobject.cpp -> _ZN15CShadingContext5shadeEP8CSurfaceii11EShadingDimji` | `CDLObject::shade`, `dlobject.cpp:173` |
| `:110` DB-006 `implicitSurface.cpp -> (same)` | `CImplicit::shade`, `implicitSurface.cpp:242` |
| `:112` DB-006 `surface.cpp -> (same)` | `displaceEstimate()` (`surface.cpp:304, 332, 334`) and `displace()` (`:1706, 1737, 1756, 1775, 1794`), both inline in `shading.h:387-388` |
| `:111`, `:113` DB-006 `surface.cpp -> restoreState / saveState` | `surface.cpp:1818, 2102, 2275` |

The ray-hit path is: the hider's trace (`trace.cpp:208`) calls the Geometry virtual
`CObject::shade(CShadingContext *, int, CRay **)`, which fills `P`, `Ng`, `I` in the context's varyings and
then calls `context->shade(this, n, 1, dim, 0)`. The Geometry virtual's implementations live in Geometry
(`CDLObject`, `CImplicit`) and in the core (`CSurface::shade`, `render/geometryDispatch.cpp:385-405`).

The spec asks for four things that cannot all hold literally:

- FR-018: `shade()`, `displace()` and `displaceEstimate()` take `ShaderGlobals` plus the point source instead
  of `CSurface *`;
- FR-021/FR-022: no Geometry source changes and Geometry gains no edge;
- FR-010: no Geometry interface changes shape;
- FR-028: no allowlist line is added or re-keyed; a renamed or re-signed symbol that Geometry still calls
  would stale three lines and need three new ones.

**P-Q1. How does Geometry keep calling `shade()`?** *Decided 2026-11-02: the recommended option, accepted by the maintainer.* (a): the opaque forwarding entry.

- **(a) Recommended.** Keep `shade(CSurface *, ...)`, `displace()` and `displaceEstimate()` as a thin
  *forwarding entry* in Shading. It treats the `CSurface *` as an opaque, forward-declared pointer, never
  dereferences it, and hands it to a Shading-owned interface, `IShadingProducer`, installed on the context
  by the hider that owns it (`contracts/point-source-and-globals.md`). The hider (`CHider`, Hiders) builds the
  point-source adapter over the surface and the `ShaderGlobals`, and calls the new entry
  `shade(const ShaderGlobals &, IPointSource &)`. With no producer installed (a context with no hider), the
  forwarding entry warns once (`CODE_NOTSTARTED`) and returns.
  - Geometry is unchanged for this path; the five DB-006 lines stay matched (same symbol, still defined in
    Shading); `object.h` leaves Shading (DB-006 31 -> 30, FR-022 holds).
  - Spec impact: FR-018 and US7 AS2 are amended to say "the shading entry takes `ShaderGlobals` and the
    point source; a Geometry-facing forwarding entry keeps its `CSurface *` signature as an opaque handle
    until spec 031 replaces it with `IDicer`/`IRayContext`".
- **(b)** Define `shade(CSurface *, ...)` in Hiders (as `trace.cpp` defines `trace` today). Rejected: it
  recreates DB-005's pattern (a Shading member defined in Hiders), which this spec removes.
- **(c)** Change Geometry's calls to a Hiders function and re-key the five lines with the maintainer's
  sign-off (FR-028 allows it only that way). It also changes `CObject::shade(CShadingContext *, ...)`, a
  Geometry interface (FR-010), and pulls part of spec 031 forward.
- **(d)** Narrow US7: only the hiders' own calls take `ShaderGlobals`; the Geometry path keeps
  `shade(CSurface *)` with `object.h` in Shading. DB-006 stays at 31, and SC-005/SC-008 are amended.

## R2. The hiders as owners: what the composition touches

**Finding (inventory, file:line evidence in the plan's appendix).**

- **Private members**: no hider reaches one. The only uses are inside `CShadingContext::trace`/`traceEx`,
  which are Shading members defined in `trace.cpp` (R6). The friend `CPhotonHider` uses no private member
  (confirms the spec's assumption).
- **Protected members reached by inheritance**: `sampling` (`photon.cpp:211`, twice; the public
  `samplingConfig()` returns the same object), `numTracedRays` (`trace.cpp:324`, inside a Shading member
  body), and the four illumination hooks (overridden only by `CPhotonHider`, `photon.h:53-56`).
- **Public members reached bare** (become `context.` or `context().`): `currentXBucket`/`currentYBucket`,
  `thread`, `threadMemory`, `currentShadingState`, `shade`, `displace`, `samplingConfig`, `cameraSeeds`,
  `drawNext`, `drawCounters`, `bindDraws`, `resetDrawOccurrences`, `trace`. Counts: `reyes.cpp` 77,
  `stochastic.cpp` 13, `raytracer.cpp` 15, `photon.cpp` 79 (text occurrences, macro bodies once).
- **Implicit `this` conversions that break** (good: the compiler finds them): `reyes.cpp:229` and
  `raytracer.cpp:420` (`activeContext = this`), `reyes.cpp:1043` (`regionCoverage(this, ...)`),
  `photon.cpp:219` (`illuminate(this, ...)`).
- **Silent traps** (compile, then misbehave; each must be rewritten explicitly):
  - `reyes.cpp:1994` `CReyes *hider = (CReyes *)CRenderer::contexts[i];`: a C-style cast of a `CShadingContext *`
    becomes a `reinterpret_cast` once `CReyes` no longer derives from `CShadingContext`. With `contexts` retyped to
    `CHider **` (P-Q6 (a)) the same cast is a `static_cast` and is correct; it is still rewritten explicitly as
    `static_cast<CReyes *>`, so a later retyping cannot turn it silent;
  - `renderer.cpp:1029` `dynamic_cast<CReyes *>(contexts[0])`: against `CHider **` it stays valid (a downcast of a
    polymorphic type); a `dynamic_cast` from `CShadingContext *`, which has no virtual after this spec, does not
    compile (it would not silently return null);
  - `trace.cpp:124, 175` and `shading.cpp:452`: the object-hash sentinel `(CObject *)this` /
    `(CSurface *)this`, written by the context's constructor and compared in `trace()`. Once the bodies are
    `CHider` members, `this` is the hider: the comparison and the reset must name `&context` explicitly (R6;
    P-Q16 (a) keeps the hash and its sentinel in the context), or every lookup misses and the grouping order
    (hence the shading order and the replay) changes.
- **Geometry** reaches inherited members through a `CReyes *` in `surface.cpp:264` (`currentShadingState`)
  and `:304, 332, 334` (`displaceEstimate`), and passes a `CReyes *` where a `CShadingContext *` is expected
  at `curves.cpp:720`, `patches.cpp:1604, 1882`, `polygons.cpp:1314`, `subdivisionCreator.cpp:1868`,
  `subdivisionLoop.cpp:354` (`create(r)`) and `surface.cpp:452` (`classifyBound(r, ...)`). The core does the
  same at `geometryDispatch.cpp:101, 416, 455, 492, 587-600, 698, 733, 774`. FR-010 allows exactly these
  edits: an inline accessor, `r->shadingContext()`, adds no link symbol and no include (Geometry already
  includes `reyes.h`, DB-006), so no allowlist line moves.
- **Tests**: nine test classes subclass `CShadingContext`, not five (spec correction C1). Four of them also
  override the illumination hooks (`test_illumination_hook.cpp:131`, `test_illumination_hook_conditions.cpp:127`,
  `test_illumination_hook_operands.cpp:146`, `test_false_if_constructs.cpp:62`), and one reads the protected
  `vertexMemory` (`test_state_services.cpp:64-70`).

**Decision proposed (part of P-Q8 below for the shape; the rest follows FR-007).** A Hiders base class
`CHider` (`src/ri/hiders/hider.{h,cpp}`, header mostly inline) owns `CShadingContext context` by value,
constructed with the thread number. It declares `virtual void renderingLoop() = 0`, the accessor
`CShadingContext &shadingContext()` (inline), and implements `ITracer` (R6) and `IShadingProducer` (R9).
`CReyes`, `CRaytracer`, `CPhotonHider` derive from `CHider`; `CStochastic`/`CZbuffer` through `CReyes`.
Construction and destruction order is unchanged: today the base `CShadingContext` is built before the hider's
members and destroyed after the hider's destructor body; a `CHider` member is built before the derived
class's members and destroyed after the derived destructor body, the same relative order (the photon
hider's destructor writes its maps while the context is still alive).

**Rationale.** It is the registry's own direction (DB-005), Hiders -> Shading is allowed, and keeping one
context per hider per thread keeps all per-thread state where it is.

## R3. `CRenderer::contexts` and `activeContext` (P-Q6, P-Q7)

**Finding.** The core stores the frame's contexts as `CShadingContext **CRenderer::contexts`
(`renderer.h:394`), read by `rendererJobs.cpp:88-89`, `rendererDeclarationsDispatch.cpp:78-82`,
`imager.cpp:130-134`, `renderer.cpp:844-845, 1029, 1052, 1068`, and by one hider, `reyes.cpp:1994`
(allowlist DB-011 `reyes.cpp -> _ZN9CRenderer8contextsE`, line 232). A static data member's mangled name
carries no type, so retyping it keeps that line matched. `activeContext` (`thread_local CShadingContext *`,
`renderer.h:387`) is written only by `reyes.cpp:229` and `raytracer.cpp:420` and read only by
`imager.cpp:128`; it costs four platform-tagged DB-011 lines (`allowlist.txt:164, 192, 202, 241`).

**P-Q6. The core's array of hiders.** *Decided 2026-11-02: the recommended option, accepted by the maintainer.* (a).
- **(a) Recommended.** Retype `CRenderer::contexts` to `CHider **` and keep its name. Consumers that need the
  shading context call `contexts[i]->shadingContext()`; `reyes.cpp:1994` becomes a `static_cast<CReyes *>`;
  `renderer.cpp:1029` stays a `dynamic_cast`, now from `CHider *`. DB-011 line 232 stays matched. A clearer
  name is spec 030's (the hider registry).
- (b) A new `CRenderer::hiders` array next to `contexts`. `reyes.cpp:1994` would read the new symbol: a new
  DB-011 line, which FR-028 forbids.
- (c) Remove `reyes.cpp`'s read of the core altogether (the hiders find their siblings through Hiders). A
  DB-011 reduction, but spec 030's scope.

**P-Q7. Who sets `activeContext`.** *Decided 2026-11-02: the recommended option, accepted by the maintainer.* (a).
- **(a) Recommended.** The core's dispatcher (`rendererDispatchThread`, `renderer.cpp:1051-1053`) sets
  `activeContext = &contexts[w]->shadingContext()` before `renderingLoop()`, for every hider. The two hider
  writes go, and with them the four DB-011 lines (168 -> 164) and two Hiders metric references. The photon
  hider's threads start setting it too, which is unobservable: its only reader is the imager, and a photon
  frame never displays (`photon.cpp:116` sets `HIDER_NODISPLAY`; the photon loop dispatches no display tile).
- (b) Keep the writes in the two loops, as `CRenderer::activeContext = &context;`. No line moves.

## R4. Bundles move into Shading (FR-011)

**Finding.** `bundles.cpp` includes only `bundles.h`, `common/diagnostics.h`, `shaderPl.h` and
`common/stats.h`; `bundles.h` only `common/global.h`, `ray.h`, `shading.h`. `CGatherVariable` is Shading's
(`shaderPl.h:219`). The five outside includers (`renderer.cpp:34`, `rendererContext.cpp:39`,
`rendererStatics.cpp:38`, `irradiance.cpp:29`, `irradianceDispatch.cpp:53`) use no bundle type (grep). The
file is compiled into `ribRender` *and* `ribVector` (`src/ri/CMakeLists.txt:73`; it is not in the
`REMOVE_ITEM` list at `:365-397`), and nothing in `ribVector` references it.

**Decision proposed (P-Q9).** `git mv src/ri/hiders/bundles.{h,cpp} src/libshader/shading/`; add
`bundles.cpp` to `libshader_shading`'s sources and remove it from `ri_sources`; delete the five vestigial
includes (a build confirms; if one is not vestigial, include the moved header, Core/Hiders -> Shading being
allowed). `ribVector` loses nothing it uses. The relocation is byte-for-byte: no expression changes.
Alternative: keep the files in `src/ri/hiders/` and compile them into the shading library; rejected, archcheck
classifies by location, so the library would carry Hiders objects.

## R5. The photon-map lookup interface (FR-012, S-Q2 of the spec: option A)

**Finding.** Shading calls exactly one `CPhotonMap` member, `lookup(float *, const float *, int)`, at
`giFunctions.h:298` (`PHOTONMAP2EXPR`), `:344` (its 3-argument sibling) and `shading.cpp:4023`
(`jitPhotonMap`), on the pointer `getPhotonMap()` returns. `CPhotonMap` already has four bases
(`CMap<CPhoton>`, `CFileResource`, `CDataView`, `CMapRefTarget`).

**Decision proposed (P-Q10 names it).** A header-only, Shading-owned abstract class
`openrender::shading::IPhotonLookup` (`photonLookup.h`) with one pure virtual
`void lookup(float *C, const float *P, int maxFound)`, a defaulted inline virtual destructor and no data.
`CPhotonMap` derives from it publicly and its existing `lookup(float *, const float *, int)` becomes the
override (the 4-argument overload is untouched). `CRendererServices::getPhotonMap` returns
`IPhotonLookup *` (a breaking change within 2.x, R11). The core's implementation returns the `CPhotonMap *` it returns today, upcast.

- **Byte identity**: the same function body runs; only the call becomes virtual.
- **ribVector**: it compiles `photonMap.cpp` and does not link `libshader_shading`. The interface is
  header-only (inline key function), so its vtable and typeinfo are emitted weakly where used; `ribVector`
  needs no Shading symbol. The same holds today for `ITracer` and `CHiderTracer`.

## R6. The hiders are the tracers (FR-016, FR-017)

**Finding.** `trace(CRayBundle *)` (`trace.cpp:47-245`), `traceEx` (`:253-283`) and `trace(CRay *)`
(`:303-399`) are Shading members defined in Hiders. Their uses of the context: the private `traceObjectHash`
(`:98-147, 175`), `currentRayDepth` (`:205, 254-271`), `currentRayLabel` (`:256-275`), `freeStates`
(`:261-273`), the public `newState()`, `currentShadingState`, `currentXBucket`/`currentYBucket`,
`threadMemory`, `rayImportance()`, and the protected `numTracedRays` (`:324`). The DB-011 lines that read the
core from those bodies are keyed by `trace.cpp` (`allowlist.txt:280-284`: `clipPlanes`, `maxGridSize`,
`maxRayDepth`, `hierarchyMutex`, `root`), and DB-017's `trace.cpp -> CObject::cluster(CShadingContext*)`
(line 387).

**Decision proposed.**

1. The three bodies become `CHider` members **in `trace.cpp`** (the file keeps its name, so its DB-011 and
   DB-017 lines stay matched): `CHider::traceRay(CRay *)` (today's `trace(CRay *)`),
   `CHider::traceGroups(CRayBundle *)` (today's `trace(CRayBundle *)`, also called by
   `raytracer.cpp:652` for the primary bundle at depth 0), and the `ITracer` overrides
   `traceRay(CShadingContext &, CRay *)` / `traceBundle(CShadingContext &, CRayBundle *)` (today's
   `trace(CRay *)` / `traceEx`). The `ITracer` signatures do not change.
2. `traceObjectHash` stays in the context, allocated and filled by its constructor as GitHub #53 fixed it,
   and reaches the owning hider through one documented public accessor, `TObjectHash *objectHash()`
   (FR-017's first option; P-Q16). The sentinel stays the context's own address, so `traceGroups` compares
   against `(CSurface *)&context`, the value the constructor wrote (R2's trap: the comparison must use the
   context's address, not the hider's). `ShadingParity_ContextNoServices` (`test_context_no_services.cpp`,
   #53), which counts the constructor's sentinel writes through the hash pointer, keeps its assertion.
3. **The ray level** becomes a documented public interface of the context (FR-017's first option):
   `bool enterRayLevel(const CRayBundle &bundle, int maxRayDepth, CRayLevel &saved)` and
   `void leaveRayLevel(const CRayLevel &saved)` (`contracts/shading-context.md`). The depth rule
   (`currentRayDepth < maxRayDepth`) is applied once, inside `enterRayLevel`; the limit is passed by the
   hider, which reads `CRenderer::maxRayDepth` in `trace.cpp` as today (the DB-011 line stays; the limit is
   not a service since spec 026 removed `maxRayDepth()` from the interface). On `false` the hider makes the
   one miss call `bundle->postShade(numRays, rays)`, exactly as `traceEx` does.
4. `numTracedRays` gets a public counting member, `countTracedRay()` (inline `++numTracedRays`), so the
   statistics the context flushes in its destructor stay the same.
5. `traceAny` (declared at `shading.h:409`, defined nowhere) is deleted.
6. The core's hard-coded lights (`hcshader.cpp:199, 522`) call the context's tracer directly (not through
   `libshader::traceRay`, which reports to the trace observer: going through it would add requests to the
   replay and change it).
7. `CHiderTracer` (`hiderTracer.{h,cpp}`) and `CRenderer::frameTracer` are deleted; `hiderTracer.cpp` leaves
   `ri_sources` and the `ribVector` removal list.

**P-Q2. Where Shading finds its tracer.** *Decided 2026-11-02: the recommended option, accepted by the maintainer.* (a): no test changes how it installs a tracer.
- **(a) Recommended.** A per-context slot, `ITracer *CShadingContext::tracer()`, set by the owning hider
  (`context.setTracer(this)` in `CHider`'s constructor). `libshader::traceRay`/`traceBundle` use the
  context's tracer and fall back to `getServices()->tracer()` when the context has none (a context with no
  hider: every contract test double). The renderer's `CRendererServicesImpl::tracer()` override is deleted,
  so its answer becomes the base default, `nullptr`. Result: in a render every context uses its own hider;
  the contract tests that inject a tracer through their services double (`case_canned_trace.cpp:43`,
  `case_gather_requests.cpp:28`, `case_no_tracer.cpp:26`) run unchanged, because their `CRProbeContext`
  has no hider. `tracer()` on the services keeps its signature; its meaning ("a tracer for a context whose
  owner installed none") is recorded in 2.1.0 (R11).
- (b) A per-context slot only; `CRendererServices::tracer()` is removed. Single route, but the three rendered
  cases, `contract_harness.h`, `rendered_harness.h`, `contract_forwarder.h:40-73`, `contract_services.h:148,
  396` and the three shading_parity wrappers (`recording_services.h:96`, `table_services.h:89`,
  `recording_texture_system.h:258`) are edited (TEST-APPROVAL).
- (c) Keep the per-frame slot and an adapter. Contradicts FR-016.

**Concurrency** (the spec's edge case; argued, as spec 026 argued FR-010): a hider and its context are
created and destroyed together by the core, one pair per render thread; the tracer is the hider that owns the
context, so no two contexts share a tracer and no slot is shared by two hiders. A photon pass and a camera
pass are separate frames with separate hiders.

## R7. The illumination hooks (FR-013)

**Finding.** Shading calls the four hooks at 12 places, each behind `rendererIlluminationHook()`:
`shaderOpcodes.h:216, 272, 325, 382, 421, 473` (interpreter) and `shading.cpp:2148, 2192, 2240, 2311, 2360,
2390` (JIT). The core's lights call them as a friend, guarded by `CRenderer::hiderFlags &
HIDER_ILLUMINATIONHOOK` (`hcshader.cpp:90, 120, 135, 400, 435, 450`). Overridden by `CPhotonHider` and four
test classes.

**Decision proposed.** A Shading-owned interface `openrender::shading::IIlluminationHook` (`illuminationHook.h`,
header-only) with the four members and today's signatures. The context holds a nullable
`IIlluminationHook *` (`setIlluminationHook()`, `illuminationHook()`); every call site becomes
`if (IIlluminationHook *h = illuminationHook()) h->illuminateBegin(...)`. With nothing installed the calls are
no-ops, as today's empty virtuals are. `CPhotonHider` implements it and installs itself on the context it
owns. `hookOperand()` stays a static helper of the context (it is the operand rule, not the hook). The tests
install a hook object instead of overriding virtuals (TEST-APPROVAL, FR-025 allows it).

## R8. The shaded object in the shading state (P-Q3)

**Finding.** `CShadingState::currentObject` is a `CSurface *` (`shading.h:190`). In Shading it is read for
`->attributes` (`shading.cpp:413, 1776, 1835, 2064, 2653, 2818-2820, 4014-4015, 4288`), `->stableId` (`:809`)
and the upcast in `objectHandle()` (`:403`). Outside Shading: `hcshader.cpp:143, 458`
(`->attributes->bias`), `photon.cpp:178` (writes the phony object), `imager.cpp:82` (writes null). Tests
write it directly: `contract_harness.h:103`, `rendered_harness.h:104`, `case_offending_object.cpp:50, 59`,
`case_type_mismatch_real.cpp:87`. The observers report it as `const CSurface *surface` and two tests
dereference that (`tests/unit/sampling/scene_probe.h:74-75`, `test_displace_consistency.cpp:55`).

**P-Q3. How the state holds the shaded object.** *Decided 2026-11-02: the recommended option, accepted by the maintainer.* (a).
- **(a) Recommended.** The state holds three values, set together through one member
  `setShadedObject(const CSurface *surface, ObjectHandle handle, const CAttributes *attributes)`:
  the surface as an opaque pointer (for the observers' `surface` field, never dereferenced by Shading), the
  handle (what `objectHandle()` returns; the producer computes it as the `CObject *` upcast, exactly the value
  `CRendererServicesImpl` casts back), and the attributes (what `objectAttributes()` and the eight reads
  return). `fillPointKeys` takes the stable id from the point source. `object.h` leaves Shading. The nine test
  writes (the five named at the plan stage and four more found at the tasks stage) become `setShadedObject(...)` calls,
  and `hcshader.cpp`/`photon.cpp`/`imager.cpp` use it (TEST-APPROVAL for the tests; FR-025 is amended to allow it,
  spec corrections C7 and C8 in plan.md).
- (b) Keep `CSurface *currentObject` opaque and add a process-wide accessor hook installed by the core
  (attributes, handle and stable id of a surface). No test edits, but a process-wide hook where a per-call
  value suffices, and Shading still stores a Geometry pointer it cannot interpret.

## R9. `shade()` consumes `ShaderGlobals` and a point source (FR-018 to FR-021)

**Finding** (`shading.cpp:852-1390`). `shade()` reaches the object for: `attributes` (the hit modes, the
shaders, `surfaceColor`/`surfaceOpacity`, `shadingRate`, `flags`, `next`, `usedParameters`),
`sample(start, count, varying, locals, unsigned int &parameters)` (`:972, 1074, 1142, 1163, 1230`; the
`parameters` argument is read and written), `interpolate(count, varying, locals)` (`:973, 1145, 1166, 1233`),
`stableId` (via `fillPointKeys`, `:809`) and the opaque pointer (observers, `currentObject`). Its inputs
`uVertices`, `vVertices`, `dim`, `usedParameters` and `displaceOnly` map onto the batch shape, the requested
parameters and the pass. The derivative-offset construction (`:1060-1146`) and the grid du/dv formula
(`:1148-1219`) stay in Shading verbatim (FR-020).

`ShaderGlobals` (`shaderGlobals.h`) cannot express two things `shade()` needs:

- **0-D versus 2-D**: `BatchShape::Span` is documented as "independent points (ray hits, `SHADING_0D`)",
  covering both `SHADING_2D` and `SHADING_0D`, and `shade()` branches on the difference (derivatives,
  hit modes, cell keys).
- **The requested parameters** (`PARAMETER_*`, including `PARAMETER_RAYTRACE`, which `shade()` tests at
  `:957, 961`) are a sampling request, not a shading global.

**P-Q4. What `shade()`'s input carries.** *Decided 2026-11-02: the recommended option, accepted by the maintainer.* (a).
- **(a) Recommended.**
  - `BatchShape` gains `PointSet` (a 0-D point set: never differentiated, no hit mode except in shadow);
    `Span` is narrowed to 2-D ray hits. The redefinition is recorded in 2.1.0 (R11).
  - On input to `shade()`, the producer fills every per-batch field (`numPoints`, `shape`, `gridU`/`gridV`,
    `object`, `rayType`, `hitMode`, `transport`, `pass`, `pathDepth`, `shadingCellFootprint`, the shutter
    pair); the per-point fields are **absent** on input. The values they describe are already in the
    context's varyings (the hider or Geometry wrote `u`, `v`, `time`, and for ray hits `P`, `Ng`, `I`, `du`)
    or are sampled by the point source inside `shade()`. The presence rule "P and time always present" is
    restated as an *output* rule, and US7's own wording ("its batch description") is what it covers.
  - The requested parameters are the point source's: `IPointSource::requestedParameters()`.
- (b) As (a), plus per-point views of the varyings the producer filled (`u`, `v`, `time`, `I`, `du`).
  More to keep right, and `shade()` would still read the varyings.
- (c) Carry `EShadingDim` as an appended field instead of a new enumerator.

**Decision proposed for the rest** (follows from P-Q1 (a), P-Q4 (a), FR-018/FR-020):

- `IPointSource` (`pointSource.h`, Shading-owned, header-only): `attributes()`, `stableId()`, `surface()`
  (opaque), `requestedParameters()`, `sample(start, count, varying, locals, unsigned int &parameters)`,
  `interpolate(count, varying, locals)`.
- `IShadingProducer` (`shadingProducer.h`, Shading-owned): one member,
  `shadeSurface(CShadingContext &, CSurface *, int u, int v, EShadingDim, unsigned int, int displaceOnly)`.
  The forwarding entry calls it.
- The Hiders adapter `CSurfacePointSource` (`src/ri/hiders/surfacePointSource.h`, inline, Hiders may include
  `object.h`) forwards each member to the unchanged `CSurface` virtuals.
- The producer (`CHider::shadeSurface`, `src/ri/hiders/shadingProducer.cpp`) fills the batch: the shape from
  `dim`, the pass from `displaceOnly` (FALSE/TRUE/3 -> Full/DisplaceOnly/DisplaceEstimate), `rayType` and
  `hitMode` from the same inputs as `classifyShading()` (FR-019): `dim`, `displaceOnly`, the context's
  `inShadow` and ray depth (two new public read accessors), the illumination-hook query
  (`rendererIlluminationHook()`, moved to the public section), the
  hider kind (`CHider::rasterizes()`, true for `CReyes`), the three hit-mode attributes from the source.
- `shade(const ShaderGlobals &, IPointSource &)` branches on the batch only:
  - `pass != Full`: today's displacement-only code;
  - `rayType == Shadow`: `hitMode == Primitive` copies `surfaceOpacity` and returns (`:886-898`), else the
    surface only;
  - `rayType` is `Camera` or `Secondary` with `hitMode == Primitive`: copies opacity and colour and returns
    (`:912-927`);
  - `rayType == None` with `hitMode == Primitive`: runs the shaders, then overwrites `Oi` (`:1372-1380`);
  - the atmosphere runs when `pathDepth == 0` (`:1359`), the same value as `currentRayDepth`.
  The producer guarantees that `hitMode` is `Shade` wherever today's code tests no hit mode (0-D outside a
  shadow; a raster 0-D batch), so the branch set reproduces the code row by row.
- Hiders' new files (`hider.cpp`, `shadingProducer.cpp`) reference no `CRenderer::` symbol (a new file would
  need new DB-011 lines), and join `ribVector`'s `REMOVE_ITEM` list like `trace.cpp`.

## R17. Does a plain static link register the render backend? (P-Q17)

**Finding (static, added at the analyze stage).** `RiBegin` resolves its implementation through two function pointers set by static initializers: `g_rendererContextRegistrar` (`src/ri/render/rendererContext.cpp:6516-6524`, `RiRegisterDefaultContextFactory`) and `g_ribOutRegistrar` (`src/ri/output/ribOut.cpp:2181-2187`, `RiRegisterRibOutFactory`). `ri.cpp:119-127` and `:307-326` never name the backend (Principle VIII rule 4) and report "Full-render mode is not available in this build (no renderer context registered)" when none registered. A static archive member is pulled only when something references one of its symbols, and nothing references these objects' registrars, so a link of `libRIBRender.a` that satisfies only the bundle imports may leave `RiBegin` with no renderer. The package's `-force_load`/`--whole-archive` block hides that today (it pulls every member).

**Method.** Phase 1 (T015, T022) links the scratch consumer without the block with `-Wl,-why_load` (macOS) and `-Wl,-Map=<file>` (Linux) and records whether `rendererContext.o` and `ribOut.o` are pulled and by what, and whether the consumer's `RiBegin` reports no backend. T-L2 and T-L4 assert a backend registered, and `test_common_crosslib.cpp` (cases 0 and 2, which run `RiBegin`/`RiEnd` sessions) is read once T062 makes its link plain.

**Decision.** P-Q17, open until the measurement. The implementer does not choose a mechanism and `ri.cpp` does not name the backend.

## R10. `classifyShading()` disagrees with `shade()` in three places (P-Q5)

**Finding** (an in-scope defect of spec 026's test-only function, found while designing FR-019).

1. **A 0-D hit in a shadow.** `shade()` tests `transmissionhitmode` whenever `inShadow` is set, whatever the
   dimension (`shading.cpp:883-898`). `classifyShading()` returns `HitMode::Shade` for a 0-D shadow hit
   (`situation.cpp:73-76`), and `ShadingContract_Situations` pins that (`case_situations.cpp:278-279`: an
   implicit-surface shadow hit with `transmissionhitmode` `"primitive"` expected `Shade`). The code copies
   the attribute opacity there; row 6 of the 026 table ("none (0-D skips the specular test)") is right only
   for the non-shadow case.
2. **A secondary hit in a REYES or zbuffer frame.** `classifyShading()` tests `rasterHider` before the
   dimension and the depth (`situation.cpp:78-87`): a `SHADING_2D` ray hit of a reflection traced from a REYES
   grid classifies as row 1 (`RayType::None`, `camerahitmode`), while `shade()` tests `specularhitmode` for
   every `SHADING_2D` batch (`:912`) and never `camerahitmode` for it (`:1372`). Likewise a 0-D hit at depth
   >= 1 in a raster frame classifies as row 2 (`None`) instead of `Secondary`. The test's `expectedRow`
   (`case_situations.cpp:177-212`) mirrors the same order, and none of its scenes renders a raster frame with
   secondary rays (`:313-323`), so the defect is latent.

3. **A ray traced during photon emission.** `classifyShading()` tests the illumination hook before every
   other rule (`situation.cpp:62-66`), so a shadow ray of `transmission()`/`shadow()` called from a light
   shader while photons are emitted, or a `trace()` from it, classifies as row 10 (`Photon`, `Shade`).
   `shade()` never reads the hook and tests `transmissionhitmode`/`specularhitmode` there. Whether a suite
   scene reaches this path is measured in Phase 1 (the photon scenes' light shaders); the rule is corrected
   either way, because the producer calls `classifyShading()`.

A producer that follows the code (which byte-identical images require) would disagree with
`classifyShading()` on all three, and FR-019's comparison would fail on any such scene.

**P-Q5.** *Decided 2026-11-02: the recommended option, accepted by the maintainer.* (a): D-spec-1, FR-019, US7 AS1 and SC-008 are amended to the corrected function.
- **(a) Recommended.** Correct `classifyShading()` to the code (dimension and depth first; a shadow hit reads
  `transmissionhitmode` at every dimension), correct the 026 situation table's rows 2, 4 and 6 text in
  029's contract, and edit `ShadingContract_Situations` (`case_situations.cpp:177-212, 273-280`, plus two new
  scenes: a REYES frame with a secondary hit under each `specularhitmode`, and an implicit-surface shadow hit
  under `transmissionhitmode "primitive"`). TEST-APPROVAL. No image changes: `classifyShading()` is test-only.
- (b) Leave `classifyShading()` and exclude the two cases from FR-019's comparison. Weakens FR-019 and keeps
  a wrong oracle for spec 049.

## R11. The boundary version: 2.0.0 -> 2.1.0 (FR-015)

**Decided 2026-11-02 by the maintainer's versioning policy, which replaces the plan stage's P-Q8**: MAJOR is a
breaking change, MINOR a non-breaking addition, PATCH a bug fix only, but MAJOR stays at 2 until spec 055,
because nothing versioned has been distributed (MAJOR 1 is the pre-versioning interface). Spec 029 takes
`kRendererServicesVersion` to **2.1.0**; the header's rule comment and history say that 2.x is unstable until
the milestone closes, so this MINOR step may break ([contracts/renderer-services-version.md](contracts/renderer-services-version.md)).

**Changes the 2.1.0 entry lists as breaking within 2.x** (each would be MAJOR after the milestone):
`getPhotonMap` returns `IPhotonLookup *` (a result changed); `tracer()`'s meaning changes (P-Q2 (a));
`BatchShape::Span` is narrowed and the presence rule is restated for `shade()`'s input (P-Q4 (a)).
**Additions**: `IPhotonLookup`, `IIlluminationHook`, `IPointSource`, `IShadingProducer`, `BatchShape::PointSet`.

The version is set in Phase 3 (the first boundary change) and the one history entry grows with the later phases.
Every implementer's `static_assert` stays on MAJOR 2 (`src/ri/render/rendererServicesImpl.h:35`, `hiderTracer.h:25`
until Phase 6 deletes it, the new `hider.h`, and in tests `contract_services.h:49`, `contract_forwarder.h:31`,
`recording_tracer.h:31`, `case_globals_double.cpp:33`): none of these lines is edited.

## R12. Link wiring and the static consumer (FR-001 to FR-005)

**Finding** (line numbers verified on this tree): `src/libshader/shading/CMakeLists.txt` header comment
`:11-14`, APPLE comment `:82-84` and `-undefined dynamic_lookup` `:85`, ELF comment `:87-98` and INTERFACE
`--allow-shlib-undefined` `:100`, include list `:103-119`; `CMake/openRenderConfig.cmake.in:79-97`;
`tests/unit/common/CMakeLists.txt:69-84` (`Common_DiagnosticsCrossLibraryStatic`, cases 0 and 2 of the
one-sink check); `tests/shading_contract/load_stub.cpp` (65 lines, 4 classes' members), wired at
`tests/shading_contract/CMakeLists.txt:152`; `src/common/CMakeLists.txt:79-81` (`--no-undefined` on
`libRIBCommon`, the precedent). The `--no-as-needed` workaround is on **four** targets, not two
(`tests/shading_parity/CMakeLists.txt:316-326, 338-346, ~360-368, ~380-389`), and `LibShader_McompConsistency`
links `ribRender` only for the imports (`src/libshader/tests/CMakeLists.txt:88-112`).

**Decision proposed.** Phase 3 removes the imports; Phase 4 removes the wiring:

- macOS: delete `:85` and its comment (ld64's default is `-undefined error` for dylibs; spec 023 research);
- Linux: replace `:100` with `target_link_options(libshader_shading PRIVATE "LINKER:--no-undefined")`;
- the package block and its copy go; `Common_DiagnosticsCrossLibraryStatic` links plainly and keeps its two
  cases;
- `load_stub.cpp` goes (Phase 3, when its symbols stop being imported: kept past that point it would define
  trapping copies of the bundles beside the library's real ones);
- the four `--no-as-needed` targets and `LibShader_McompConsistency` drop `ribRender` where the test calls no
  `libRIBRender` symbol (each checked at the Phase 4 Test gate; a target that still uses `ribRender` keeps
  it and loses only the workaround).

**P-Q14. SC-002's installed-package half.** *Decided 2026-11-02: the recommended option, accepted by the maintainer.* (a); SC-002 and FR-004 amended. `cmake --install <build> --prefix <scratch>` fails in a
self-contained build (GitHub #23: the `.orenderrc` rule writes to the configure-time prefix), and the
libraries are in the same unnamed component as that rule, so the spec 037 scratch-prefix technique (which
installs only the `Development` component) does not reach the libraries.
- **(a) Recommended.** Automate the build-tree half (`Link_StaticConsumer`, every checkpoint, both
  platforms). Run the package half once per platform in the final phase as an opt-in test
  (`Link_PackageStaticConsumer`, label `link-package`, like `determinism-sweep`): it configures a scratch tree
  with `CMAKE_INSTALL_PREFIX=<scratch>` (so #23 is not hit), builds and installs it, and builds and runs a
  `find_package(openRender COMPONENTS Static)` consumer. Its red evidence is taken once in Phase 1 on the
  starting tree.
- (b) Fix #23 in 029 (out of scope).
- (c) Amend SC-002 to the build tree only.

## R13. What proves "no image changes" (FR-023, SC-006)

- **Manifests**: Phase 1 records, on the starting tree, the bytes of every image the suite writes and of
  every image the determinism sweep writes, on macOS and on Linux (each against itself), plus a noise list
  from a second run (spec 026's `image_manifest.sh`, copied unchanged). Every checkpoint diffs the suite
  manifest and the `MaxBlockAvgDiff` list; the final phase diffs the sweep manifest on both platforms.
- **The trace replay** (`tests/shading_contract/replay/{macos,linux}.txt`, 16 sections) is never re-recorded:
  the tracers move in Phase 6, and the replay proves every request arrives in the same order with the same
  rays.
- **The derivative arithmetic** stays in `shading.cpp` untouched (FR-020): the point source only replaces
  `object->sample`/`interpolate`, with the same arguments in the same order.
- **A stop rule**: any differing byte outside the noise list, any changed `MaxBlockAvgDiff`, any replay
  difference stops the phase for the maintainer. No threshold, reference or replay is changed.

## R14. Performance (FR-026, SC-010)

Added per call: one virtual call to the producer per `shade()` (once per grid or ray batch); a virtual
`sample`/`interpolate` hop through the adapter (once or twice per batch); a virtual photon lookup per lookup
call (a virtual call replacing a direct one); the context reached through the hider's member (no extra
indirection: a member at a fixed offset). The method is spec 026's SC-010: five interleaved timing series
against a starting-tree build, on `somewood`/`wood`/`blue_marble`, a `gather` scene, a photon-map scene and a
heavy REYES grid scene.

## R15. What the plan could not measure without a build

- that the two `irradiance.h` includes are vestigial (no `CIrradianceCache` use; a transitive need is
  possible);
- which further include directories of `libshader_shading` can go (`render`, `output`, `dataviewer`;
  `parse` stays for `ri.h`): a compile decides (DB-023 reduction, not required);
- macOS: that removing `dynamic_lookup` on the starting tree fails on exactly the four symbols; Linux: that
  `--no-undefined` does the same and that no other undefined symbol of the 465 is unsatisfied by the
  library's own link line;
- the DB-011/metric deltas (P-Q7) and the final allowlist count;
- the Linux measurements: `ubuntu-2404-desktop.v2-labs.io` did not resolve from the planning machine on
  2026-11-02 (access restored the same day); they are Phase 1's, and its Linux baseline stays the gate for Phase 2.

## R16. Smaller decisions (recommendation first; overrule cheaply)

- **P-Q9** (R4, decided 2026-11-02 as recommended): physically move the bundle files into `src/libshader/shading/`.
- **P-Q10** (decided as recommended): names: `IPhotonLookup`, `IIlluminationHook`, `IPointSource`, `IShadingProducer`, `CHider`,
  `CSurfacePointSource`, `CRayLevel`; files `photonLookup.h`, `illuminationHook.h`, `pointSource.h`,
  `shadingProducer.h` (Shading), `hider.{h,cpp}`, `surfacePointSource.h`, `shadingProducer.cpp` (Hiders).
- **P-Q11** (decided as recommended; FR-025 amended): `CShadingContext` is declared `final`, and a test asserts `!std::is_polymorphic_v<CShadingContext>`
  and `std::is_final_v`. The nine test subclasses construct the context directly; `test_state_services.cpp`
  reads the vertex memory through a new public read-only `vertexMemoryInUse()` (the alternative: drop that
  assertion from the test, which FR-025 forbids).
- **P-Q12** (decided as recommended): the public surface the hiders need is added as named members, not by making protected members
  public wholesale: `rayDepth()`, `inShadowRay()`, `countTracedRay()`, today's protected
  `rendererIlluminationHook()` moved to the public section unrenamed (its 12 macro and JIT callers do not
  change), `enterRayLevel`/`leaveRayLevel`, `setTracer`/`tracer`,
  `setIlluminationHook`/`illuminationHook`, `setProducer`, `setShadedObject`, `vertexMemoryInUse()`. The
  photon hider reads `samplingConfig()` instead of the protected `sampling`.
- **P-Q13** (decided as recommended): the new tests (plan "Test design") and labels: `link` for the link tests (in the everyday run),
  `link-package` for the opt-in package consumer, `shading_contract` for the contract additions.
- **P-Q15** (decided as recommended): the include-path reduction beyond `hiders` and `geometry` (R15): take every directory that the
  compile lets go.
- **P-Q16. Where the object hash lives** (R6 item 2). *Decided 2026-11-02: the recommended option, accepted by the maintainer.* (a). **Recommended (a)**: in the context, behind
  `objectHash()`, as today; `test_context_no_services.cpp` (the #53 regression, which constructs a context
  over sentinel storage and counts writes through the hash pointer, `:83-116`) keeps its assertion, and the
  sentinel value does not change. (b) Move it to `CHider` (FR-017's other option): `struct TObjectHash` and
  `SHADING_OBJECT_CACHE_SIZE` leave `shading.h`, the #53 test loses its subject and must be rewritten against
  a test hider (TEST-APPROVAL), and the sentinel becomes the hider's address.
