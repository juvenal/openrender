# Research: Volume Shader Segments (spec 056)

Every line reference below was read from the worktree at master `e83d005d` on 2026-11-07. The spec's Clarifications
(2026-11-07) are decisions; this file resolves only what they left to the plan.

## R1. Where the segment step runs, and how `postShade` reaches a context

**Decision**: The step lives in Shading's bundles (A3). A new Shading class `CSegmentBundle` (in
`src/libshader/shading/bundles.h`) sits between `CRayBundle` and the three Shading bundles (`CTraceBundle`,
`CTransmissionBundle`, `CGatherBundle`). It holds a reference to the context that built it and a record of the
spawning surface, both captured when the builtin builds the bundle. A new public member of the context,
`CShadingContext::shadeVolumeSegments()`, runs the volume shader. The bundles call it from their existing
`postShade` overrides. A matching ray base, `CSegmentRay`, sits between `CRay` and `CTraceRay`, `CTransmissionRay`
and `CGatherRay`, and records where the ray's current segment starts and the furthest it may reach.

**Rationale**:
- The hider calls `bundle->postShade(n, rays, varying)` and `bundle->postShade(n, rays)` (`src/ri/hiders/trace.cpp:214, 217, 267`). The step goes inside those calls, so the hiders stay as they are. `tracer.h` (the `ITracer` protocol) and `CPrimaryBundle` (`src/ri/hiders/raytracer.h:123`) do not change, and the step adds no symbol to the Hiders' imports.
- Every builtin that builds one of the three bundles is a `CShadingContext` member: `traceReflection` and `traceTransmission` (`executeMisc.cpp:837`, `:696`) and `gatherHeaderBegin` (`shading.cpp:4517`). Each has `this` and the current shading state at hand.
- `CRayBundle` itself stays the protocol type the tracer sees. The only change to it is an in-class initialiser, `postShader = nullptr`. `CPrimaryBundle` never sets that field. It is not read by anything today: the raytracer drives `traceGroups(&primaryBundle)` directly, not through `enterRayLevel` (no `traceBundle`/`enterRayLevel` in `raytracer.cpp`). Until now the unconditional overwrite in `shade()` hid the uninitialised value.

**Alternatives considered**:
- *Put the fields on `CRayBundle`*. Rejected: `CPrimaryBundle` (Hiders) would inherit volume state it must never use, and the tracer-facing type would grow.
- *Have the hider call a "volume step" after `postShade`*. Rejected: Hiders would have to name a volume concept, which goes against FR-017 and Principle VIII rule 3. It would also need a change to the `tracer.h` protocol.
- *Run the volume shader inside `shade()` on the hit's state (Pixie's original design, which is what the dead tail at `shading.cpp:1452-1456` was for)*. Rejected for three reasons. It misses every miss and every primitive-hit-mode early return (`:972-984`, `:998-1013`), which would leave FR-005 and FR-007 unmet. The hit state's `I` and `P` belong to the hit: a shader may rewrite them (`mirror.sl` does `I = normalize(I)`), and the gather outputs read them, so rewriting them would break FR-003 and FR-004.

## R2. The state the volume shader runs in

**Decision**: `shadeVolumeSegments()` takes a fresh shading state from the context's free list (`newState()` when the
list is empty), the same way `enterRayLevel` does (`shading.cpp:582-587`). It returns the state to the list afterwards.
It does **not** change the ray depth or the ray label: the shader runs at its segment's depth (FR-009). The state is set
up as follows:

- It is 0-D (`SHADING_0D`), with `numVertices = numRealVertices = n` real points and no derivative tail, so `Du`/`Dv` of anything are 0.
- Every global the services list (`numGlobalVariables()`) is zero-filled for the `n` points. Then `P` (the segment's endpoint), `I` (endpoint minus origin, not normalised), `E` (camera origin `(0,0,0)`), `Ci`, `Oi` and `time` (the ray's time) are written. A volume shader that reads a surface-only global sees 0, deterministically (edge case "volume shader reads N, Cs, u, v").
- The shaded object is the **spawning surface**: its opaque surface pointer, its `ObjectHandle` and its attributes, through `setShadedObject()`. `attribute()`, the message builtins and `illuminance()` inside a volume shader therefore answer for the surface the segment belongs to (RISpec: the volume is attached to the surface the ray leaves).
- A new state flag, `volumeSegment = TRUE`, is set while the step runs and cleared when the state goes back to the free list. `traceReflection`, `traceTransmission` and `gatherSample` attach no volume shader to rays built in such a state (FR-009: rays a volume shader spawns have no spawning surface).
- `inShadow` is saved, cleared and restored around the step. A `transmission()` or `trace()` inside a volume shader that runs for a shadow segment is an ordinary secondary-ray call: it does not hit the `assert(inShadow == FALSE)` at `executeMisc.cpp:714`, and its hits classify as `Secondary`. This does not make #148 worse. #148 is a shadow *hit* in `"shader"` hit mode calling `transmission()`, which this spec leaves alone.
- The point keys are the rays' keys (`pointKeys[i] = rays[i]->key`, `pointKeysFromRays = TRUE`). The batch key is mixed from the first ray's key, the ray depth and a fixed tag for the volume step. The per-point call-site occurrence counters start empty for each step. The volume shader draws from its own sub-streams, keyed on its own shader key, so the step is the same whichever thread runs it (FR-015).
- The volume shader is prepared with `prepare(shaderStateMemory, varying, n)` into `locals[ACCESSOR_POSTSHADER]`, inside a `memBegin`/`memEnd` pair on `shaderStateMemory` and on `threadMemory`, and executed with `execute(this, ...)`. The instance runs as the interpreter or as JIT code depending on how it was loaded, so neither the emitter nor the `.slo` format changes.

**Rationale**: A separate state keeps the hit's varyings intact, which FR-004 needs (`gather` records `surface:*` from
the hit state's `varying`, `shaderPl.cpp` `CShaderVectorVariable::record`). It gives the miss case and the hit case the
same code path. It uses only machinery the context already owns.

**Alternatives considered**:
- *No shaded object in the volume state (null attributes)*. Rejected. `gatherSample` reads `currentAttributes->flags` with no null check (`shading.cpp:4384`), so a `gather()` inside a volume shader would crash. `illuminance()` would have no light list, and `attribute()` would answer for nothing. The explicit flag gives FR-009 without those hazards.
- *Run the step in the hit's state after `shade()` returns*. Rejected (see R1).

## R3. Hits: what the step reads and writes, and transparency

**Decision**: In `postShade(n, rays, varying)` the bundle first records each hit's own (pre-volume) opacity. It then runs
the step with the hit's `Ci` and `Oi` (the first `n` entries of `varying[VARIABLE_CI]` and `varying[VARIABLE_OI]`),
writes the results back into those entries, and continues with the existing logic. **The transparent/opaque test uses
the pre-volume opacity; the compositing uses the post-volume colour and opacity.**

- `CTraceBundle`: as today, but compositing the attenuated values.
- `CTransmissionBundle`: a hit that is opaque before the volume blocks the ray whatever the volume does, so the step runs only on the transparent hits, which saves work.
- `CGatherBundle`: the step runs before the outputs are recorded, so `surface:Ci` and `surface:Oi` record the attenuated values. Every other output, the `ray:*` outputs (recorded in `postTraceAction`) and the tags are unchanged (FR-004).
- Primitive hit modes: `shade()` returns early after writing `Ci`/`Oi` from the attributes (`shading.cpp:975-983`, `:1000-1012`), and the hider still calls `postShade` (`trace.cpp:214`), so those segments are attenuated with no change to `shade()` (FR-005).

**Rationale**: The bundles test opacity bit for bit (`(opacity[k].integer ^ one.integer)`, `bundles.cpp:65-67`). `fog`
returns `mix(Oi, 1, d)`, which can land one ulp below 1.0 for an opaque hit and turn it into a spurious continuation
ray. Classifying on the hit's own opacity keeps the ray sequence the same as without a volume.

## R4. Composition across transparent hits (FR-008)

**Decision**: Each pass of the hider's transparency loop (`trace.cpp:78-245`) is one segment. The segment's origin is
the previous hit (`segmentStart`, which the bundle records as `ray->t` when it queues a continuing ray, before the
hider resets `t` at `trace.cpp:238-239`), or the ray's origin (`segmentStart = 0`) on the first pass. The step attenuates
that segment's endpoint colour and opacity, and the existing front-to-back compositing in `CTraceBundle::postShade` and
`CTransmissionBundle::postShade` combines the passes.

**Rationale (exactness for `fog`)**: `fog` is an "over" layer: `V(C, O) = (d·bg + T·C, d + T·O)`, with `T = exp(-l/D)` and
`d = 1 - T`. Take two segments of lengths `l1` and `l2` and two hits (C1, O1) and (C2, O2):
- The bundle stores `V1(C1, O1)` and weights the next segment by `1 - (d1 + T1·O1) = T1·(1 - O1)`.
- The result is therefore `d1·bg + T1·C1 + T1·(1 - O1)·(d2·bg + T2·C2)`.
- This is exactly the light transported through medium 1, surface 1, medium 2 and surface 2.
- `T1·T2 = exp(-(l1 + l2)/D)` is the whole-path transmittance.

So the per-segment result equals the whole-path result for any shader of this form. Other shaders hold to it only up to
their own approximation, as FR-008 allows.

**Alternative considered**: *apply the volume once over the whole path from the original origin to each hit (`I` from
`ray->from`)*. Rejected: the background term `d·bg` would then be added once per transparent layer, which counts
in-scattering several times.

## R5. Misses, refusals and where a segment ends (FR-007, FR-009)

**Decision**:
- **Miss inside a level** (`postShade(n, rays)` after the bundle's `postTraceAction()` ran in this request): the step runs with `Ci = Oi = 0` and the result is composited behind what the ray already carries:
  - trace, first pass: `dest += Ci'·m`;
  - trace, later pass: `color += (1 - opacity)·Ci'`, then `dest += color·m`;
  - transmission, first pass: `dest += (1 - Oi')·m`;
  - transmission, later pass: `opacity += (1 - opacity)·Oi'`, then `dest += (1 - opacity)·m`;
  - gather: see R6.
- **Refusal at maximum depth**: per `tracer.h` the tracer then calls `postShade(numRays, rays)` once and **never** calls `postTraceAction()`. A `levelTraced` flag on the bundle is cleared when the builtin submits it and set by its `postTraceAction()`. With the flag clear, the miss form runs no step and keeps today's result (black for trace, fully transmitted for transmission), which is FR-009 and A6. This follows the documented protocol, so it also holds for the contract test doubles (`CCannedTracer`, `recording_tracer.h:168`), which call `postTraceAction()` but not `enterRayLevel`.
- **Where a segment ends**, for a ray with `segmentStart = s` and `segmentLimit = L`:
  - a hit ends at `ray->t`;
  - a miss ends at `e = min(ray->t, L)`. `ray->t` is unchanged on a miss: it is the start-of-pass maximum, already shortened by any user clip plane (`trace.cpp:64-74`), and `C_INFINITY` on a continued pass;
  - if `e >= C_INFINITY` (nothing finite applies), `e` is the far exit of the ray from the world bound (slab test in double precision), clamped to `>= s`. A ray that never enters the box (its origin outside the bound, pointing away) or an empty world gives `e = s`: a zero-length segment, so `I = 0`, which is finite. With no services the bound is unknown and the same rule gives a zero-length segment;
  - `L` is `C_INFINITY` for `trace()` (which ignores `maxdist`, #98, unchanged), the call's `maxDist` for `gather()` (`shading.cpp:4414`), and the destination for `transmission()`/`shadow()`, recorded at creation as the ray's initial `t` (`executeMisc.cpp:779`, `min(d, maxDist) - bias`). The destination has to be recorded because the hider resets a continued ray's `t` to `C_INFINITY` (`trace.cpp:239`), so a shadow ray that passed a transparent surface would otherwise have no end at the light;
  - the far clipping distance is never used.
- `P = from + e·dir` and `I = (e - s)·dir`, from the ray (`dir` has unit length, `tracer.h`), for hits and misses alike.
- **Where it is computed**: inside `CShadingContext::shadeVolumeSegments()`, not in the bundle. The bundle passes each ray's `segStart` and `segEnd` (`C_INFINITY` meaning "to the world-bound exit"). `rendererWorldBmin()/Bmax()` are `protected` members of the `final` context (`shading.h:1038-1052`), which a bundle cannot call, and they stay protected.

**World bound**: `CRendererServices::worldBmin()/worldBmax()` (`rendererServices.h:315-316`), already reached through
`rendererWorldBmin()/Bmax()` (`shading.cpp:606-607`). The values are `CRenderer::worldBmin/worldBmax`
(`rendererServicesImpl.h:64-65`), which the core copies into the ray hierarchy's root (`renderer.cpp:1060-1061`), and the
tracer intersects camera-space rays with that root (`trace.cpp:304`). The bound is therefore in **camera space**.
`rendererServices.h:312` says "world space (not camera space)". That comment is wrong, and the only current consumer
(the world radius at `shading.cpp:2411-2416`) does not depend on the space. The maintainer approved the comment-only fix
in this spec with no version step (Q5, 2026-11-07), to be made only after the non-identity-camera probe confirms
camera space.

**Alternatives considered**:
- *Compare the context's ray depth with the depth the bundle was built at, to tell a refusal from a miss*. Rejected: it relies on the tracer calling `enterRayLevel`, which `tracer.h` allows a tracer not to do, and the canned doubles don't.
- *Clip every miss to the world bound, even with a finite limit*. Rejected: a shadow ray toward a light outside the geometry's bound must end at the light (FR-007, FR-010).
- *A new `ITracer` member for the bound*. Rejected: the bound is already a service, and a new member would step `kRendererServicesVersion`, which would collide with spec 032.

## R6. `gather()` misses (FR-007)

**Decision (approved by the maintainer, 2026-11-07, Q1)**: A gather miss inside a level runs the step on its segment, with
`Ci = Oi = 0`. The result is recorded **only** into the gather outputs that read the global `Ci` or `Oi`
(`surface:Ci`, `surface:Oi`) for the missed rays' vertices. Tags, `numMisses` and the `else` branch are unchanged (the
miss is still a miss), and every other output is left as today. To support this, `CGatherVariable` gets a query for
the global entry it reads (`-1` for `ray:*` and non-global outputs), and `CGatherBundle::postShade(n, rays)` records
from a two-entry scratch varying.

**Rationale**: It is the only observable form FR-007 can take for `gather()`: the `else` branch sees the fogged miss in
the colour outputs it may read. US4's "gathered average" is measured over hits, so its test scene encloses the gather
point so that every ray hits.

**Alternative**: *no step for gather misses* (FR-007 narrowed to `trace()`). It is simpler, and arguably what users
expect from an `else` branch. The maintainer chose the decision above (Q1, 2026-11-07); FR-007 records it.

## R7. The side normal (FR-001, A9)

**Decision**: One rule for every entry point. The side is `dot(dir, Nside) > 0 ? exterior : interior`, the existing
tie rule (`executeMisc.cpp:909`, `shading.cpp:4421`). `Nside` is the **shaded surface's global `N` at the shading
point, as the shader holds it at the call**:
- `trace()`, `transmission()`, `environment("raytrace")`: the location's `N` (`giFunctions.h:68`, `shaderFunctions.h:1880`, JIT `shading.cpp:3203`, `:3359`). This is the current behaviour of `traceReflection`.
- `gather()`: `varying[VARIABLE_N]` (`giOpcodes.h:33`, JIT `shading.cpp:2704`), current behaviour.
- `shadow()`: the location's `N` is deliberately zero (no origin guard: `shaderFunctions.h:1986`, `shading.cpp:3191`), and inside a light shader the varying `N` is not reliable, because an area light writes its own normal into `N` (gotcha 10) and lights run on the surface state's `varying` (`shading.cpp:1905`). The side normal is therefore a **snapshot of the surface's `N` taken when the lights are run**. `iterateLights` (`shading.cpp:1852`, the one runner of *illuminating* lights, shared by the interpreter and the JIT) copies `varying[VARIABLE_N]` into a per-state buffer before the first light executes and exposes it as `lightSideN` for the duration of the light loop. A `shadow()` call outside a light loop (from a surface shader) uses `varying[VARIABLE_N]`. Lights are also run, for `ambient()`, at three other sites (`shading.cpp:1950`, `:2177`, `execute.cpp:415`); they do not set `lightSideN`, so a `shadow()` called from an ambient-run light body uses `varying[VARIABLE_N]` as well. That is accepted: an ambient light has no `illuminate` body, so no area light overwrites `N` there, and the case is not tested (`validation.md` lists it as untested by design).

A new `CTraceLocation` field, `sideN`, carries the side normal, because the locations come from raw `ralloc` memory and
`N` keeps its origin-guard role. It is filled at every site that fills a location:
- `giFunctions.h:68` (TRANSMISSIONEXPR: transmission, visibility, trace);
- `shaderFunctions.h:1880` (ENVIRONMENT traced);
- `shaderFunctions.h:1986` (SHADOW traced);
- JIT: `shading.cpp:3191` (shadow), `:3203` (environment), `:3359` (trace/transmission/visibility).

**Consequence (accepted by the maintainer, 2026-11-07, Q3; documented in `RISPEC_GAPS.md`)**: a shader that rewrites `N` before tracing changes the side, for example
`N = faceforward(N, I)`. The existing split already behaves this way, and `glass.sl`/`mirror.sl` compute the side from a local `Nf`; `mirror.sl` rewrites the global `N` only by
normalising it (`N = normalize(N)`, `mirror.sl:18`), which keeps its direction, so the side does not change.

**Alternatives considered for `shadow()`**:
- *`Ns`, the illuminance normal* (advisor's suggestion). Rejected as the default: shipped shaders pass a faceforwarded `Nf` to `illuminance`/`specular`/`diffuse` (`glass.sl`, `mirror.sl`), which flips the side for any point seen from inside. US3's own test (a shadow ray from the sphere's back wall, seen through the front wall) would read "exterior" where the spawning rule says "interior".
- *The varying `N`*. Rejected: area lights overwrite it.

The maintainer accepted the snapshot (Q3, 2026-11-07).

## R8. `transmission()`/`shadow()` sign split (FR-010)

**Decision**: `traceTransmission` gets the same two-bundle split as `traceReflection`, `interiorBundle`/`exteriorBundle`
with their own ray arrays. The second array is allocated only when the two shaders differ, as at
`executeMisc.cpp:867-870`. With no volume bound, both shaders are null, every ray goes to the one bundle in the same
order as today, and the output is byte-identical (FR-016). `visibility()` (the `probeOnly` branch at `:792`) and the
float `trace()` (`:939`) build no bundle and run no step (FR-010, US3 AS3).

## R9. Removing the overwrite in `shade()` and what stays

**Decision**:
- Delete the selection block at `shading.cpp:1384-1400` and the post-shader execution at `:1452-1456`. `shade()` no longer runs any interior or exterior shader.
- `CShadingState::postShader` (`shading.h:222`) and its copy in `enterRayLevel` (`shading.cpp:588`) **stay**, documented as "the volume shader of the segments this level's rays travel; read by no shading code". This keeps the approved spec 029 contract test `ShadingContract_RayLevel` (`tests/shading_contract/case_ray_level.cpp:121, 132`) unchanged. The initialisation in `newState()` (`shading.cpp:1527`) and `imager.cpp:86` stay as they are.
- `selectVolumeShader` (`src/ri/state/attributes.h:251-263`), `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` (`:59`) and its only writer (`src/ri/geometry/csgSolid.cpp:273`) are deleted (FR-011). The attribute clone at `csgSolid.cpp:272` stays, because it also carries the operand flip (`:274-275`). Bit 26 becomes unused.

**Alternative**: remove `CShadingState::postShader` too. That is cleaner, but it needs an edit to an approved test
(`case_ray_level.cpp`). The maintainer chose to keep the field (Q4, 2026-11-07).

## R10. `incident()` and `opposite()` (FR-013)

**Decision**: `incidentParameter` and `oppositeParameter` (`shading.cpp:1747-1753`) pick the source once per call from
vertex 0 of the current state:
- `I·N < 0`: the ray arrives from the exterior side, so `incident()` reads `Exterior` and `opposite()` reads `Interior`;
- otherwise: `incident()` reads `Interior` and `opposite()` reads `Exterior`.

The tie (`I·N == 0`) reads as "not from the exterior", which is the `> 0` rule applied to `-I`, and agrees with
`shade()`'s old `dotvv(I, N) < 0` test. Both backends reach these two members: the interpreter through
`DEFLINKFUNC`/`DEFFUNC` with `FUNCTION incidentParameter`/`oppositeParameter` (`shaderFunctions.h:1331-1344`), and the JIT
through `jitIncidentParameter`/`jitOppositeParameter` (`shading.cpp:3795-3812`). One change therefore covers both. No
services member or version changes. With no current state or no object (the imager), the query answers not found as
today.

**The approved tests are unaffected (FR-022 verified, no diff needed)**:
- `ShadingContract_Messages{Interp,Jit}`: the harness sets `N = (0,0,1)` and `I = (0,0,1)` for every point (`tests/shading_contract/contract_harness.h:105, 107`), so `I·N = 1 > 0`. `incident()` keeps reading `Interior` and `opposite()` keeps reading `Exterior`, exactly as `case_messages.cpp:135-136` expects.
- `ShadingContract_MessageAccessors` (`check_message_accessors.cmake`) checks the `ACCESSOR_INTERIOR`/`ACCESSOR_EXTERIOR` constants passed to `PARAMETEREXPR_PRE` and `jitParameterFinish`. Those constants are read only for a mutable parameter, which `incident()`/`opposite()` never read (`cVar` is always null for them), so they stay as they are.

If the maintainer prefers that the accessor constants also follow the side, both tests would need edits, shown as diffs
first (FR-022). That is not proposed.

**Image effect**: `sphere-shader-parameter-reyes(-slo).rib` calls these builtins and binds the same shader with the same
defaults as both `Interior` and `Exterior`, so the answer is the same on either side and the image is unchanged.

## R11. Test vehicles

- **Exact checks** use the existing tools: `test_pixel_probe` (`tests/visual/test_pixel_probe.cpp`: one pixel of a float TIFF against expected values) for the hand-computed fog values (SC-001, SC-003, SC-006, SC-009), and `test_golden_compare` (bit-for-bit against a golden image from before the change, per platform: `tests/visual/golden/`, `golden/linux/`) for the atmosphere scenes (SC-007).
- **Parity** uses `add_parity_test` (raytrace against reyes and zbuffer) at the default threshold; no threshold is widened.
- **Determinism** uses `add_thread_configs_test` (`-t:1|-t:2|-t:4`).
- **The step's geometry** (`I`, `P`, the end rules, refusal, the volume shader's own rays, probes) is checked without a renderer in a new standalone contract case. It drives the real bundles through a canned tracer that enters the ray level like `CHider::traceBundle` (`trace.cpp:255-269`), with a probe volume shader that encodes `I` in `Ci`.
- **Analytic scene design**: every probed pixel has a **constant segment length**. Orthographic camera, rays straight through a slab or between two parallel polygons (a probe surface that traces along `I`, so there is no refraction bend), and a constant-colour backdrop. Reyes and zbuffer shade on a grid and interpolate (CLAUDE.md, "REYES shading vs. sampling"), so a per-pixel chord through a sphere would make their value drift from the closed form. With a constant length the same exact value holds on all three hiders, and the parity pairs mean something. Expected value: `Cb·T + bg·d` with `T = exp(-l/D)`. The miss scenes are bounded by axis-aligned polygons, whose bounds are exact, so the world-bound exit is known. The camera transform is **not** the identity (a rotation and translation), so world space and camera space differ and the probe tells which space the bound is in. The correction to the `rendererServices.h` comment (Q5) is made only after this probe confirms camera space.
- `fog` mixes `Oi` toward white, so on a transmission ray it attenuates with a grey value, not a tint. US3's expectation is the hand-computed grey attenuation. A coloured-opacity volume shader is not proposed.
- The spec 026 trace replay scenes (`tests/shading_contract/replay/scenes`) and the canned-bundle cases bind no `Interior`/`Exterior` (checked by search), so the pinned replay cannot change.

## R12. Starting-tree facts checked for the plan (not in the spec)

- `environment("raytrace")` traces through `traceReflection` (`shaderFunctions.h:1934`), so it gets the volume step like `trace()`. `environment` is the same entry point, and the maintainer approved giving it the step (Q2, 2026-11-07). FR-001 and FR-007 now name it.
- `traceReflection`'s default label is `rayLabelTransmission` (`executeMisc.cpp:855-856`). This looks like a pre-existing slip, unrelated to this spec and not changed here. The maintainer approved filing it as a P1 Bug Tracker issue (Q6); the coordinator files it, and it is out of this spec.
- A continued transmission ray's `t` is reset to `C_INFINITY` by the hider (`trace.cpp:239`), so after a transparent hit a shadow ray can hit geometry beyond its light. This pre-existing behaviour is not changed: changing it would alter shadows in scenes with no volume and break FR-016. The step records the destination itself (R5). The maintainer approved filing it as a P1 Bug Tracker issue (Q6); the coordinator files it, and it is out of this spec.
- Retiring the selector and the flag touches two approved tests beyond `CSG_InteriorExteriorSelection`: `State_Isolation` (`tests/unit/state/test_state_isolation.cpp:450-458`, the `selectVolumeShader` checks) and `CSG_ProceduralOperand` (`tests/unit/csg/test_procedural_operand.cpp:382`, which asserts the flag on expanded members). The maintainer approved both changes (2026-11-07); each is shown as a diff before it is applied (spec FR-012).
- `csg-sphere-sphere-difference-interior-raytrace-inside` is also a determinism-sweep scene (`tests/visual/determinism-scenes.cmake:197`). The sweep keys on the name, which is kept. The comments at `tests/visual/CMakeLists.txt:2334-2353` describe the old rule and move with the rewrite.
