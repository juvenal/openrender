# Research: Area Light Geometry (spec 057)

**Date**: 2026-11-14. **Base**: `master` at `278b0394`, built from scratch in this worktree (Release, `-j4`, through
`cpujob`; 137 `.rslo` + 137 `.slo`, no compiler warning, only the usual `ld` duplicate-library notes).
Every source location below was read in this tree. Measurements were made with the build-tree environment of the visual
suite (`SHADERS=build/shaders`, `ORENDERHOME=build/tests/visual/_home`, `DISPLAYS=build/src/display/{file,rgbe,framebuffer}`),
because this worktree has no `openrender/` deploy tree, so the literal "Running a render" environment of CLAUDE.md
cannot run here. The two are the same renderer and the same shaders; only the paths differ.

---

## R1. FR-023 probe on current master: a flagged occluder casts no shadow through `visibility()` (a defect, filed as GitHub #169)

**Question** (spec FR-023 plan task): does an occluder carrying `Attribute "visibility" "int transmission" [1]` cast a
shadow from `quadlight.sl` on a fresh build of current master?

**Scenes** (all under `specs/057-area-light-geometry/tools/fr023-probe/`, rendered by its `run.sh` through `cpujob`):

| Scene | Light | Occluder | Variants |
|---|---|---|---|
| `q-*.rib` | `examples/rib/quadlight.rib` unchanged (quadlight, 16 samples) | its sphere | `default` (as committed), `flagged` (the attribute in the sphere's block), `none` (sphere removed) |
| `p-quad-*.rib` | quadlight, 2x2 quad at y=4 facing down, intensity 8, 16 samples | disk r=0.8 at y=2 | default, flagged, none |
| `p-vis-*.rib` | `vis_probe.sl`: point light at (0,4,0), `Cl = intensity * visibility(Ps, from)` | same disk | default, flagged, none |
| `p-trans-*.rib` | `trans_probe.sl`: same, `Cl = intensity * transmission(Ps, from)` | same disk | default, flagged, none |

Floor under the disk, centre block (pixels 120..136 x 150..166, mean luminance, 0..255), and the whole floor
(rows 150..256), each against the scene's own no-occluder render:

| Light | default vs none | flagged vs none |
|---|---|---|
| quadlight (`p-quad`) | identical (69.07 / 69.07, floor max diff 0) | **identical** (69.07 / 69.07, floor max diff 0) |
| `visibility()` probe (`p-vis`), `.rslo` | identical (135.13 / 135.13) | **identical** (135.13 / 135.13) |
| `visibility()` probe, `.slo` (`Option "shaderformat" "default" ["slo"]`) | not rendered | **identical** (135.13 / 135.13) |
| `transmission()` probe (`p-trans`) | identical (135.13 / 135.13) | **shadow**: 14.25 against 135.13 (mean abs diff 120.9) |
| `quadlight.rib` (`q`) | the three TIFFs: `default` and `flagged` byte-identical (`cmp`); floor rows 160..240 identical to `none` | same |

**Findings.**

1. An unflagged occluder blocks nothing (every row, `default` vs `none`): option B's premise holds on master. Shadow rays
   carry only `ATTRIBUTES_FLAGS_TRANSMISSION_VISIBLE` (`executeMisc.cpp:806`) and the default attribute flags lack it
   (`attributes.cpp:104-110`).
2. The flag works and the tracer works: `transmission()` with a flagged occluder gives a black umbra.
3. **`visibility()` never reports an occluder, flagged or not, under the interpreter and the JIT.** `quadlight.sl` (and
   `spherelight.sl`) test their samples with `visibility()`, so **neither shipped area-light shader casts a shadow on
   master**, with or without the attribute. The committed `quadlight.rib` image has no shadow.

**Candidate root cause (read twice, not yet proven by a fix).** `visibility()` is `TRANSMISSIONEXPR_PRE`/`TRANSMISSIONEXPR`
with `VISIBILITYEXPR_POST` (`giFunctions.h:107-115`), which runs `traceTransmission(numRays, rays, TRUE)` and then sets
`*rays->res = rays->t < C_INFINITY`. In the probe path `rays->t` starts at 0 (`executeMisc.cpp:756`) and
accumulates `cRay->t * multiplier` (`:817`), where `cRay->t` is set to `minDist - rays->bias` before the trace (`:795`); the ITracer contract
(`tracer.h`, `traceRay`) and `CHider::traceRay` (`trace.cpp:289-315`) overwrite `t` with the hit distance on a hit and
**leave it unchanged on a miss**. Both are finite, so the test is true for every ray: `visibility()` returns 1 always (and
the test's sense is also inverted: a hit would mean "not visible"). The JIT reproduces it byte for byte:
`jitTraceBatch` (`shading.cpp:3544`) writes `rays->t < C_INFINITY ? 1.0f : 0.0f` for `wantBoolean`. The expression came with the
initial import (`git log -S` finds `5399a1bb` and the JIT copy in `b4bfc25e`).

**Defect, filed as GitHub #169 (not fixed in 057).**

- *Title*: `visibility()` always returns 1: occluders never shadow `quadlight`/`spherelight` (interpreter and JIT).
- *Scene*: `specs/057-area-light-geometry/tools/fr023-probe/p-vis-flagged.rib` (point light `vis_probe.sl`,
  `Cl = intensity * visibility(Ps, from)`, light at (0,4,0); disk r=0.8 at y=2 with
  `Attribute "visibility" "int transmission" [1]`; floor at y=0; raytrace hider, 256x256).
- *Command*: `cpujob <label> specs/057-area-light-geometry/tools/fr023-probe/run.sh build <scratch>`, which runs
  `build/src/orender/orender p-vis-flagged.rib` with `SHADERS=<scratch>/shaders:build/shaders`,
  `ORENDERHOME=build/tests/visual/_home`, `DISPLAYS=build/src/display/file:...`.
- *Expected*: a black disk shadow on the floor, as `p-trans-flagged.rib` (same scene with `transmission()`) shows (centre
  block 14.25).
- *Observed*: the floor is byte-identical to `p-vis-none.rib` (no occluder) in every pixel (centre block 135.13), under
  `.rslo` and `.slo`; `quadlight.rib` with its sphere flagged is byte-identical to the committed scene.
- *Likely cause*: above (`giFunctions.h:112`, `shading.cpp:3544`).
- *Consequences*: the shipped `quadlight`/`spherelight` area lights never cast shadows; every reference image that uses
  `visibility()` (`quadlight`, `quadlight-slo`, `spherelight`, `spherelight-slo` and any other) was captured without
  occlusion, so the fix will move them and needs the maintainer's re-capture decision.

**Effect on 057** (the defect is GitHub #169). (a) 057's own occlusion does not use `visibility()` or `traceTransmission`'s probe path (R6), so the defect
does not block the spec. (b) It makes SC-006/FR-022's comparison with the old `quadlight` form an agreement **only without
an occluding object** (decision D-1 in plan.md). (c) The `quadlight.sl` header comment that says the JIT renders it as
"checkerboard noise" is stale (spec 017 fixed `random()`; `quadlight` and `quadlight-slo` share one reference and pass); the
deprecation note replaces it only if a comment-only edit is verified not to change the compiled bytes (FR-021).

**Bug Tracker cross-check.** #99 (`visibility()` rejects optional parameter lists) is a different defect. #156 (a transmission
ray with an exactly zero direction component misses) cannot explain the probe: the `transmission()` probe in the same
geometry shadows correctly. #156 matters for 057's own rays only in the measure-zero case of an emitter sample exactly
axis-aligned with the receiver; 057's tests draw their emitter points from the sampler, never at fixed aligned positions.

---

## R2. Where the emitter registry lives, and who answers Shading's sampling request (Principle VIII routing)

**Decision.** The registry type is **Geometry-owned** (`src/ri/geometry/emitterRegistry.{h,cpp}`), filled by the renderer
core (`RiAreaLightSourceV` declares a light, `addObject` records a root) and **installed per frame** like `CGeometryFrame`
(`installEmitterRegistry()` in `beginFrame()`, `emitterRegistry()` read lock-free, uninstalled in `endFrame()`).
Shading reaches it only through a **Shading-owned interface** (`IAreaLightSampler`, `libshader/shading/areaLightSampler.h`)
that the **hider** implements (`CHider`, like `ITracer`): the hider reads the installed registry, draws through Geometry's
`sampleArea(*this, ...)` (the hider is the `IRayContext`), and evaluates the light-surface globals with the leaf's own
`sample()`/`interpolate()`, exactly as it evaluates a ray hit.

**Why.** The four parties' limits leave one route. Shading may depend on Graphics State only among the parties here
(not Geometry, not the core). Geometry's sampler needs an `IRayContext` (lazy subdivision children and tessellation are
built under Geometry's tessellation lock with the thread's scratch and varyings). Hiders may depend on Geometry and Shading
but not the core. The core may depend on everything. So: core fills a Geometry value; hiders read it; Shading asks the hider.

Walk of the allowed-dependency table (every new edge):

| Edge | From domain | To domain | Allowed |
|---|---|---|---|
| `rendererContext.cpp`, `rendererFrame.cpp` include `geometry/emitterRegistry.h` | core | Geometry | yes (composition root) |
| `hiders/hider.{h,cpp}`, `hiders/areaLights.cpp` include `geometry/emitterRegistry.h` | Hiders | Geometry | yes |
| `hiders/hider.h` includes `libshader/shading/areaLightSampler.h` | Hiders | Shading | yes |
| `geometry/*.cpp` read `CAttributes::areaLight` | Geometry | Graphics State | yes |
| `libshader/shading/shading.cpp` reads `CAttributes::areaLight` / the arealight controls | Shading | Graphics State | yes |
| `geometry/rayContext.h` gains a non-virtual exclusion accessor (R6) | Geometry | (own) | - |
| `src/ri/state/attributes.h` gains the slot and controls | Graphics State | (own) | - |

No file includes the core from a domain; no allowlist line is added (`ctest -L architecture`).

**FR-002 wording.** The spec calls it "a per-frame emitter registry of the renderer core". The core *owns its contents and
lifetime* (creates it in `beginFrame()`, fills it, deletes it in `endFrame()`); its *type* is Geometry's so the hiders can
read it. The contract states this; it changes no requirement.

**Alternatives rejected.**
- The core implements the request through `CRendererServices` (core holds the registry). Rejected: the core has no per-thread
  `IRayContext` and Shading would hand the core its context; the evaluation of the light-surface globals is the hider's
  existing ray-hit path.
- The registry in Graphics State (opaque `CObject *` as `void *`). Rejected: Graphics State would hold Geometry objects
  behind a cast, the opposite of spec 025's handle rule.
- A new member on `IRayContext` that samples (031's note "a sampling request answered by the hider" read literally).
  Rejected: the request comes from Shading, which cannot name `IRayContext`; Geometry's `sampleArea` *takes* the
  `IRayContext` instead (FR-010), and the only `IRayContext` addition is the exclusion accessor of R6.

---

## R3. Area sampling in Geometry: exact where it is cheap, one generic tessellated sampler otherwise

**Decision.** Two stages. **Stage A (Geometry, pure)**: `sampleArea(ctx, u1, u2)` returns `{leaf, u, v, P, Ng, pdfA}`;
it chooses an emitter leaf by area, then a point uniformly by area on it. **Stage B (Hiders, the existing evaluation)**:
the hider evaluates the light-surface globals at `(leaf, u, v, time)` with the leaf's own `sample()`/`interpolate()` into the
light's varying buffers, as it evaluates a ray hit; `P`, `N`, `Ng`, `dPdu`, `dPdv`, `u`, `v`, `s`, `t` therefore carry the
emitter's orientation, user normals and `st`, exactly as shading the emitter would.

Samplers by kind:

| Kind | Sampler | Points | `pdfA` |
|---|---|---|---|
| `CPolygonMesh`, `RiPolygon`/`RiGeneralPolygon` (P1) | exact: per-face area table over the triangles and planar quads (quad split into two triangles by area, inverse bilinear to its `(u,v)`) | on the faces | `1/area`, exact |
| `CSubdivMesh`, `CLoopSubdivMesh` (P1) | generic tessellated sampler over the limit patches the ray path builds (`K x K` grid per patch, `K = 8`) | on the limit surface (evaluated at the grid's interpolated `(u,v)`) | `1/tessellated area` |
| sphere, disk, cylinder, cone (P2) | analytic warps (sphere: `z` uniform, Archimedes; disk: `r = sqrt`; cone: linear radius, `sqrt`; partial sweeps and z-limits honoured) | exact | `1/area`, exact |
| paraboloid, hyperboloid, torus (P2), and any quadric under a non-uniform scale or skew | generic tessellated sampler | on the surface | `1/tessellated area` |
| patches, patch meshes, NURBS (P3, decision D-3) | generic tessellated sampler; trims: cells wholly outside the trim dropped, straddling cells kept (documented) | on the surface | `1/tessellated area` |
| points, curves, blobbies, implicit, DSO objects | none: reported once per light, stay ordinary geometry | - | - |

**Emitter choice.** One draw `(u1, u2)`. `u1` selects the leaf in the light's area CDF (binary search), is rescaled into
the chosen bin (in double), then selects the face in the leaf's own CDF and is rescaled again; `(u1', u2)` place the point.
So every draw's density is `1/(total area of the light's emitters)` for the exact samplers (FR-006). A zero-area leaf or face
has an empty CDF bin and is never chosen. A light whose total area is 0 has no emitter (A7).

**Pure function.** The tables are built once per light per frame (R4) from the leaf geometry at shutter open in camera
space, so a draw depends only on `(u1, u2)`, the surface and the time (FR-006); the evaluation (stage B) uses the receiver's
`time` (A8). With non-rigid emitter motion the density is the shutter-open area's (documented approximation).

**Why the generic sampler for subdivision.** The ray path already turns a subdivision mesh into limit patches
(`CSubdivMesh::intersect` creates its children under the tessellation lock) and evaluates any patch with `sample()`. A
fixed `K x K` evaluation per patch gives an area table that is deterministic (independent of ray footprints, unlike the
multi-resolution `CTesselationPatch`) and points that stage B puts on the limit surface. US2 scenario 3's "area agrees with
the tessellated area" is checked against a fine reference tessellation; the residual bias is `O(K^-2)` in the area.

**Alternatives rejected.** Uniform `(u,v)` with a Jacobian pdf (not uniform by area, defeats FR-006's area-proportional
selection); rejection sampling (consumes a variable count of numbers, not a function of two numbers); sampling
`CTesselationPatch`'s ray tessellation (its level depends on the ray footprint, so the table would depend on which ray built it).

---

## R4. When the emitter set is resolved, and determinism

**Decision.** `addObject` records **roots** (the world-block objects whose attributes carry the current area light, in RIB
order; procedural, instance and CSG roots included). The first sampling request for a light **resolves** its set once
(`std::call_once` per light): every root is asked for its sampleable leaves through a new Geometry virtual
`CObject::collectEmitters(IRayContext &, CEmitterCollector &)`. A delayed object or instance expands through the existing
`CGeometryHost::expandDelayedObject/Instance` (the same, idempotent expansion a ray hit triggers); a solid recurses into
its leaves; a mesh or subdivision mesh contributes itself (its own face table); a primitive with no sampler is counted and
reported once. The tables are built in the same call.

**Why.** Procedural and instance leaves exist only after expansion, which today happens on demand on render threads.
Resolving at first use keeps the expansion on the existing path. The result does not depend on which thread resolves: the
roots are in RIB order, each expansion is deterministic, and the tables are built from geometry only.

**Identity of an emitter (FR-017).** Instance leaves are instantiated with the instance's attributes
(`processDelayedInstance`, `rendererContext.cpp:458-466`), procedural leaves under a copy of the procedural's attributes
(`CGeometryHost::expand`), CSG leaves keep their own attributes, and mesh and subdivision children inherit the parent's. So
every leaf created in the scope carries the scope's `areaLight` in its attributes, and identity is tested on the **hit
leaf's attributes**, never on registry pointer membership (a hit returns a child or a `CTesselationPatch`, not the recorded
root).

**Risks to verify in Phases 3-4 (not settled by reading).**
- *Early expansion and shader keys.* Resolution can expand a procedural or instance emitter before any ray reaches it.
  `takeInstanceKey()`'s comment says instances made while rendering are numbered in expansion order; if a shader bound
  inside such an archive takes its key from that ordinal, the order in which threads trigger expansions could differ between
  `-t:1` and `-t:4`. The resolution itself is deterministic (roots in RIB order), but a ray-triggered expansion on another
  thread may race it. `AreaLight_ThreadIdentity` includes a procedural emitter whose archive binds a `Surface` drawing
  `random()`; if it fails, resolution moves to the main thread before rendering starts (with a context built for it).
- *Reyes and concurrent children.* `collectEmitters` may run on a root that Reyes is dicing or has split at that moment
  (subdivision `children`, delayed expansion). The registry's `attach()` keeps the root alive but does not make concurrent
  child creation safe: the subdivision path must take the same tessellation lock `intersect` takes, and the delayed path
  goes through the host's serialized, idempotent expansion. Verified with the three hiders at `-t:4` in Phase 4.

**CSG leaves (FR-009, out of scope "beyond their leaves").** A leaf of a solid is registered and sampled as the leaf
surface **without** its CSG region (the region test needs a ray context per point and would make the density unknown);
the light warns once that its CSG emitter emits from the whole leaf. Decision D-4 in plan.md.

---

## R5. Light-surface execution: one swap upstream of both backends

**Decision.** `iterateLights` keeps the classic path byte for byte. For a **bound** light (the sampler interface says the
light is bound) it runs the light once per sample `k`:

1. Take the 056 `lightSideN` snapshot before any light runs (unchanged position).
2. Skip the light if the receiver is one of its own emitters (`ss->currentAttributes->areaLight == light`, FR-011/FR-017).
3. Resolve `n` (FR-018 precedence) and the mode; `off` skips the light.
4. For each real active vertex draw `(u1, u2)` from a **fixed** sub-stream (R7) at index `k`; derivative tail vertices
   take their real vertex's draw.
5. Ask the context's `IAreaLightSampler` to fill a light-surface varying set (P, N, Ng, dPdu, dPdv, u, v, s, t) and `pdfA`.
6. Build a **composite varying table**: the light-surface buffers for those nine globals, the receiver's own buffers for
   every other global (Ps, L, Cl, Ol, time, I, E, ...). Swap `ss->varying` to it and set `ss->shadingDim` to
   `SHADING_0D` for the run (Du/Dv of the light-surface globals answer 0 on every hider; a grid's neighbours are unrelated
   emitter samples).
7. `light->prepare(...)` with the composite table, `light->illuminate(...)`; restore the table and the dimension.
8. For each `CShadedLight` the run pushed (the entries between the old and new head of `ss->lights`), multiply the saved
   `Cl` of vertex `i` by `V_i / (n * pdfA_i)`, `V_i` the occlusion result (R6) computed only for vertices whose `Cl` is
   non-zero (the ray keys are fixed, so skipping a ray changes no other draw).

**Why.** Both the interpreter (`execute()`, `execute.cpp:487`) and the JIT (`jitEntry(numVertices, stuff, ...)` with
`stuff[SL_GLOBAL_OPERAND] = currentShadingState->varying`, `:494-510`; every `jit*Illuminate*` helper reads
`ss->varying[...]`) take the globals from `currentShadingState->varying` at call time. One swap in `iterateLights` gives
both backends the light-surface globals (FR-013), changes no emitter code and no `.slo`, and leaves the receiver's `P`,
`N` and varyings untouched (the light writes `N = normalize(N)` into the light-surface buffer). The weighting is done once
in `iterateLights` over the pushed entries, not separately in `ILLUMINATEEND_PRE` and `jitIlluminateEnd`.

**Light controls and the illumination hook.** With the hook on (photon emission, gotcha 23) `iterateLights` is not the
path: the photon hider runs lights itself (R9).

**Alternatives rejected.** A second `CShadingState` for the light run (its `lights` list would not be the receiver's; the
JIT's light/illuminance bookkeeping assumes one state); writing the light-surface globals into the receiver's arrays and
restoring them (the clobbering FR-011 forbids, and a copy per sample); accumulating the samples into one `CShadedLight`
(`L` differs per sample; FR-012 asks one contribution per sample).

---

## R6. Occlusion of a sample, and self-exclusion by identity

**Decision.** `ITracer` gains `occluded(CShadingContext &, CRay *ray, const CShaderGroupRefTarget *excludeLight)` (kRendererServicesVersion
2.3.0 -> **2.6.0** (written as 2.4.0), a MINOR addition; renumbered at the 033 and 040 sync, D-10): trace `ray` (flags `ATTRIBUTES_FLAGS_TRANSMISSION_VISIBLE`, `from` = the receiver
point, `dir` = unit vector to the emitter point, `tmin` = the receiver's trace bias, `t = dist - bias`, key
`childKey(pointKey, depth + 1, kAreaLightShadowSite, lightKey32, k)`, `lightKey32` the light's shader key
`mix(shaderNameKey(name), instanceKey)` folded to 32 bits (`uint32_t(key ^ (key >> 32))`), the same identity the emitter
draws use, so turning another light on or off never moves this light's ray keys (nor the rays' `jimp`)), return `ray->object != nullptr`. The default
implementation in the interface calls `traceRay` and ignores `excludeLight` (contract doubles keep compiling); `CHider`
overrides it: it sets its `IRayContext` exclusion (a non-virtual accessor `excludedEmitter()` over a field the hider sets
for the length of the trace) and calls `traceRay`. Geometry's three ray gates (`CSurface::checkRayGuard`,
`CSurface::intersect`, `CTesselationPatch::intersect`) skip an object when
`attributes->areaLight != nullptr && attributes->areaLight == context.excludedEmitter()`.

**Why.** It is the probe semantics of `visibility()` as specified (spec 056: no volume step), without the R1 defect (GitHub #169): hit
means `ray->object != nullptr`, never a test on `t`. FR-017: exclusion by the identity carried by the hit leaf's attributes
(R4), not by a bias, so it holds at grazing angles and on curved emitters. FR-023 option B: only transmission-visible
objects block, through the ordinary flag test; nothing new for unflagged objects.

**Cost.** The gate adds one load and a null test (`attributes` is already loaded for `flags`) per object test; the comparison
with the context runs only for objects in an area-light scope.

**Alternatives rejected.** A field on `CRay` (rays are allocated from raw memory at several sites, `ralloc`; every site
would have to set it, and a missed one reads garbage); continuing the ray past an emitter hit (a bias in all but name, and
a loop); a ray-flag bit (`ray->flags & attributes->flags` would match unrelated attribute bits).

---

## R7. Sub-streams and call-site constants (FR-008, FR-014)

**Decision.** Reserve **`0xFF20`-`0xFF2F`** for spec 057 next to the photon hider's `0xFF11`/`0xFF12` (`photon.cpp:64-65`):
`kAreaLightSampleSite = 0xFF20` (the emitter point draws), `kAreaLightShadowSite = 0xFF21` (the shadow-ray keys),
`kAreaLightPhotonSite = 0xFF22` (photon emission points). Defined once in `src/common/sampling.h` with a comment listing
every renderer-side site (photon included), and pinned by a unit test that fails on a duplicate. The compiler's ordinals
(`CCallSitePass`) are small integers, far below `0xFF00`.

The emitter draws of light `L` at shading point `i`, sample `k`:
`sub = substream(pointKeys[i], lightShaderKey(L), kAreaLightSampleSite, /*occurrence*/ 0, currentRayDepth, n)`,
`(u1, u2) = get2D(backend, sub, 0, k)`. **Not** `drawSubstream()`: its occurrence counter is consumed per call, so a second
`iterateLights` at the same point (diffuse() then specular(), or an illuminance cache miss) would draw other points, and the
shader's own later draws would shift (FR-026). `lightShaderKey(L)` is the key `drawCounters` already builds,
`mix(shaderNameKey(name), instanceKey)`, so two bound lights draw independently and the handle decides identity.

**Spec 040.** At the plan stage (2026-11-14) 040's branch held one design commit and no code collided; re-check at implementation start; 040 must not use `0xFF20`-`0xFF2F`.
057 does **not** change the light enumeration: bound lights are pushed onto `CRenderer::allLights` by the same `getShader`
call as classic ones, in RIB order, so the photon keys `cellKey(mix(0xF07011, i), ...)` (`photon.cpp:204`), which include the
light index `i`, keep their values for every existing scene. A scene that adds an `AreaLightSource` shifts the index of the
lights after it, exactly as adding a `LightSource` does today.

---

## R8. Controls: precedence, defaults, `fast` (FR-018-FR-020)

**Decision.**
- Storage: `COptions::areaLightSamples` (int, default 16) and `areaLightMode` (`Full`), `CAttributes::areaLightSamples`
  (0 = unset) and `areaLightMode` (`Unset`); copied by the attribute copy constructor, so `AttributeBegin` inherits and
  `AttributeEnd` restores.
- Light level: the attribute values in force at the `AreaLightSource` statement, recorded in the registry's light entry
  by `RiAreaLightSourceV`.
- Resolution per receiver batch (one object per batch): `samples` = first set of (receiver attribute, light entry, option);
  `mode` likewise; `off` at any level wins; `fast` uses `max(1, ceil(samples / 4))` samples, each with its occlusion ray.
- Tokens: `Option "arealight" "int samples" / "string mode"`, `Attribute "arealight"` the same; through all four layers
  (token constants in `ri.h`/`ri.cpp` and `riVocabulary.h`/`riTokens.cpp` per gotcha 22, `RiOptionV`/`RiAttributeV`,
  `COptions`/`CAttributes` with the attribute query, `initDeclarations()`); an unknown token or value is `CODE_BADTOKEN`.
- Default count: **16**, the same count `quadlight.rib` uses (FR-019). The noise target is "the new form's run-to-run
  penumbra noise at 16 samples is no larger than the old form's lit-floor noise at 16", measured in the shadow phase with
  the convergence scenes' controls (box filter, `jitter 0`, only the light stream varied), not with `seed` (R10).

**Measured in the shadow phase (T101, T102, approved 2026-11-20): the noise target is NOT met for this comparison, and the default stays 16, `full`.**
Run-to-run noise (box filter, `jitter 0`, the light's stream varied by its handle, the visual suite's 8 x 8 block metric, max / mean over blocks): the
new form at 16 samples measures 1.97 to 3.30 / 0.125 to 0.198 (quad, mesh, subdivision; reyes, zbuffer, raytrace), the old form (`examples/rib/quadlight.rib`,
`numSamples 16`) 1.67 / 0.077: the new form is about 1.2 to 2 times the old form's maximum. The scenes differ (a small bright emitter with a soft shadow
against a large emitter over a lit floor), so the comparison is indicative. Noise falls roughly as 1 / sqrt(n): at 32 samples the new form is at 1.19 to 2.56
(mean 0.075 to 0.136), at 64 at 0.80 to 1.19. `fast` at n = 16 (4 samples) measures 5.88 to 8.08 (mean 0.34 to 0.50), about three times `full` at 16.
Users who need the old form's noise raise `samples` (the cost is linear in the count); see the table in `validation.md` section 19.

**Why `n/4` for fast.** It is the largest simple cut that keeps a 16-sample default at 4 samples (the smallest count whose
noise the probe tests already measure) and never drops below one sample.

---

## R9. Photon emission from a bound light (US8)

**Decision.** In the photon hider's light loop (`photon.cpp:141-216`), a bound light runs with the same composite table as
R5: emitter points drawn from `kAreaLightPhotonSite` with the photon's cell key, the light-surface globals filled through the
same `IAreaLightSampler` call (the photon hider calls it itself and keeps the buffers), then `illuminate()`. The hook cannot
carry per-point origins: `IIlluminationHook::illuminateBegin(P, N, theta)` has no stride, and `hookOperand()`
(`shading.h`, GitHub #68) hands a varying operand from its first active point because "hooks read only the first element".
So, while a bound light runs, `CPhotonHider::illuminateBegin`/`illuminateEnd` take each photon's origin and normal from the
emitter buffers the hider filled (a member set for the length of that light's chunk) and ignore the hook operands; for
every classic light the hook path is untouched, so classic emission is unchanged by construction. The emitted power of a
bound light is multiplied by `1/pdfA` (its total area). Smoke check only: a non-zero photon count with
origins on the emitter (#163 rules out more).

---

## R10. Measurements at plan time

| What | Value | Note |
|---|---|---|
| Fresh Release build of master in this worktree (`-j4`) | completed, exit 0 | 137 `.rslo`, 137 `.slo` |
| `quadlight.rib` render time, 16 samples | 0.24-0.40 s wall | 320x240, raytrace |
| `quadlight.rib` (numSamples 4/16/64) render time | 0.10 / 0.29-0.40 / 1.03-1.04 s | |
| Run-to-run block metric, `quadlight.rib`, seed 1 vs seed 2, at numSamples 4 / 16 / 64 | 7.17 / 4.86 / 5.20 | **total** noise: `seed` also reseeds the camera and filter streams (#166), so this is not area-light noise and does not fall with n |
| numSamples 64 vs 16, same seed | 2.12 | |
| This build's `quadlight.rib` against the committed reference | 7.97 | threshold 20 |

These are the old form's numbers. The default count and every new threshold are fixed in the implementation phases from
measurements of the new estimator in a penumbra region with the light stream alone varied (visual-test noise method).

---

## R11. The `pendingInstanceKey` leak (FR-004, SC-010)

**Decision.** In `RiLightSourceV` and `RiAreaLightSourceV`, when `getShader()` returns `NULL` (the `"null"` name, a missing
shader, a wrong-type shader), set `pendingInstanceKey = 0`. Never call `takeInstanceKey()` there: it advances
`shaderOrdinal`, which would shift every later shader's key. The fix stays in the two callers, which 033 does not move
(its `getShader` move then carries no part of the fix).

**Image risk.** Every tracked scene was scanned for light statements whose shader is `"null"` or not shipped: none outside
the two test-local fixtures that do exist (`sit_photon_light.sl`, `replay_shadow.sl`). So by reading, the fix moves no
reference image; the full suite confirms it in the phase that lands it (if any image moves, it is reported as an FR-026
exception for the maintainer, never re-captured silently).

**Test.** `LightKey_NoLeakAcrossFailedLight` (render pair, byte compare): a scene with
`LightSource "<x>" 7` (`x` = `"null"`, a missing name, a surface shader) followed by a `Surface` that draws `random()`,
against the same scene without the failed statement. Red on master (the surface takes handle 7's key), green after.

---

## R12. Items checked at plan, recorded for the tasks stage

- `iterateLights` caller set: `prepareDiffuse`, `setupIlluminance`, `rsl_run_lights`, `rsl_run_category_lights`,
  `jitIlluminanceBegin`, `callPhong` (6 callers, one function): the bound branch serves all of them.
- No RIB under `examples/` or `tests/` calls `AreaLightSource` (spec, measured); `arealight` is in no visual test.
- `RISPEC_GAPS.md:136` is the entry to move; it still cites `rendererContext.cpp:2045` (now `:2430`).
- 040's and 033's worktrees each held one design commit on 2026-11-14; 033 was not read (coordinator rule); re-check both at implementation start and at the 033 sync.
- Linux is not needed at plan; the Linux validation (GCC 13 `-Werror`, the sweep at `-j2`) is a task of the last phase.
