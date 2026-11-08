# Feature Specification: Volume Shader Segments

**Feature Branch**: `056-volume-shader-segments`
**Created**: 2026-11-06
**Status**: Draft
**Input**: GitHub issue #151 (new capability, layer L4, "Spec 056: Interior/Exterior volume shaders on every traced segment (RISpec §4.2.6)"); the maintainer's plan of 2026-11-05 (volume spec placed after 029, in parallel with 030, before the 042 milestone); `DEVNOTES_DETAILS/RENDER_ARCHITECTURE_REFERENCES.md` §2.7. The maintainer's priority for this spec: it fixes a mistake in the shading code. The shader that `trace()` and `gather()` choose for a secondary ray, the interior or exterior volume shader of the *spawning* surface, is overwritten in `shade()` before anything reads it, so the volume shader RISpec §4.2.6 requires never runs. After this spec, `Interior` and `Exterior` shaders attenuate every traced ray segment for every surface, CSG or not, for hits and for misses, under classic semantics on every camera hider. `Atmosphere` stays a camera-ray effect.

## Clarifications

### Session 2026-11-07

Eight questions were put to the maintainer (the six markers, plus FR-013 and the CSG re-render ruling). Every answer below is encoded in the sections it affects; no clarification marker remains.

- Q: Does a camera that starts inside a CSG solid get a starting volume (FR-011)? -> A: No (option A). The CSG camera-start volume is dropped. `selectVolumeShader` and `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` retire; a CSG solid is purely a case of the general rule, and a camera ray gets no volume step.
- Q: Order of the volume shader and atmosphere on a camera ray (US6)? -> A: Not applicable under the answer above: no camera ray has a volume shader and no secondary ray has an atmosphere. The marker's statement that "volume first, then atmosphere is the order of the tail of `shade()` today" was wrong; the tail runs atmosphere (`shading.cpp:1443-1449`) and then the volume shader (`:1452-1456`).
- Q: Which side does a transmission or shadow segment use (FR-010)? -> A: The spawning (shooting) surface only, by the sign of the ray direction against the shooter's normal, for `trace()`, `gather()` and `transmission()`/`shadow()` alike. A transparent hit does not switch the shader (A5 and FR-008 stand). An unoccluded shadow ray ends at the light or the transmission destination. Tinted glass shadows on arbitrary receivers would need medium switching at transparent hits: a possible follow-up, not this spec. US3's independent test is rewritten so it can pass under this rule (the sphere's own back wall).
- Q: Depth limits for a `trace()` inside a volume shader (FR-009)? -> A: The volume shader runs at its segment's depth; a `trace()` inside it goes one level deeper and counts toward `maxRayDepth`; rays a volume shader spawns get no volume step (no spawning surface). A ray refused past max depth runs no volume; US2 acceptance scenario 3 no longer says otherwise.
- Q: The end of a missed segment (FR-007)? -> A: Where the ray exits the frame's world bound, replaced by a finite `maxdist` where one applies. `gather()` honours `maxdist` today; `trace()` ignores it (#98), which this spec does not change. The default `maxdist` (`C_INFINITY`) counts as unset. `I` is always finite. The far clipping distance is not used.
- Q: Scope of `visibility()` (US3)? -> A: `visibility()` and float-valued `trace()` (the distance form) keep probe semantics and run no volume step.
- Q: `incident()`/`opposite()` side-awareness (FR-013, absorbs #120 item 3)? -> A: Yes. The source is chosen once per call from vertex 0's `sign(I.N)`, the same approximation `shade()` already uses; no services version change; the interpreter macro argument order at `shaderFunctions.h:1331-1344` is covered. Any edit to the approved contract tests (`tests/shading_contract/case_messages.cpp:135-136`, the `cm_*` shaders) is shown to the maintainer as a diff for approval first (FR-022).
- Q: Re-render of the two CSG interior scenes and the approved test `CSG_InteriorExteriorSelection` (FR-012)? -> A: Rewrite the two scenes in place with a reflective or refractive surface so they exercise the new rule, keeping names and test IDs; re-render both references, each inspected and approved by the maintainer image by image, no threshold widened. `CSG_InteriorExteriorSelection` is retired; the maintainer's approval of that retirement is recorded here. These edits happen at the implement stage, not before.

### Plan-stage corrections (2026-11-07)

The maintainer approved the plan (`plan.md`, "Maintainer answers") with every default. The following spec edits record those answers and change no requirement beyond them:
- FR-001 and FR-007: `environment("raytrace")` is the same entry point as `trace()` (`traceReflection`) and gets the step like `trace()` (plan Q2).
- FR-007: a `gather()` miss runs the step, and the result is written only into the `surface:Ci`/`surface:Oi` outputs of the missed rays. The miss still runs the `else` branch, and tags, `numMisses` and every other output are unchanged (plan Q1).
- FR-012: two more approved tests change when `selectVolumeShader` and the flag retire. `State_Isolation` (`tests/unit/state/test_state_isolation.cpp:450-458`) and `CSG_ProceduralOperand` (`tests/unit/csg/test_procedural_operand.cpp:382`) are each shown to the maintainer as a diff before the edit is applied.
- FR-020: the documentation list gains:
  - the site page section `docs/site/content/manual/reference/solid-csg-operations.md:221-256` (plan Q8);
  - two comment-only corrections, with no `kRendererServicesVersion` step (plan Q5): `rendererServices.h:312`, made only after a probe with a non-identity camera confirms that the world bound is in camera space, and the "Re-enters Shading" note in `tracer.h`.
- FR-022: no contract-test diff is needed. The harness sets `I = N = (0,0,1)` (`tests/shading_contract/contract_harness.h:105, 107`), so `incident()` keeps reading Interior and `opposite()` Exterior, as `case_messages.cpp:135-136` expects. The safeguard stays: if implementation finds otherwise, the diff is shown first.

## User Scenarios & Testing *(mandatory)*

The "users" are the people who write RIB scenes and RSL volume shaders (`Interior`, `Exterior`, `Atmosphere`), the maintainers who review image changes and architecture boundaries, and the authors of the later integrator specs (030 registry, 042 direct integrator, 049 and 050 closures) who need the classic behaviour fixed before they decide how a non-classic integrator treats volumes.

### Measured starting point (2026-11-06, master `e83d005d`, after spec 029 merged)

Every line below was read from the source at `e83d005d`. The plan and issue #151 were written at `10ecd67f` before spec 029 merged; their line numbers and the name `traceEx` are stale (see "Discrepancies with the plan" in the Assumptions).

**Where volume shaders are stored and executed.**

- `CAttributes` holds `interior` and `exterior` as `ShaderGroupRef` slots (`src/ri/state/attributes.h:244-246`, read through `interior()` and `exterior()` at `:142-143`, bound by `bindInterior` and `bindExterior`). `RiInteriorV` and `RiExteriorV` (`src/ri/render/rendererContext.cpp:2372`, `:2385`) store them. The slots are unconditional: they apply to every primitive declared after the statement, solid or not.
- A volume shader runs in exactly one place: the tail of `CShadingContext::shade(const ShaderGlobals &, IPointSource &)` (`src/libshader/shading/shading.cpp:1452-1456`), after the surface shader and after the atmosphere shader, on the hit point's own varyings, if `currentShadingState->postShader` is non-null.
- `Atmosphere` runs at `shading.cpp:1443-1449`, only when `g.pathDepth == 0` (camera rays), and never for a matte object (`:1016-1019` selects a null atmosphere for `ATTRIBUTES_FLAGS_MATTE`).

**The overwrite (the defect).**

- `traceReflection` (`src/libshader/shading/executeMisc.cpp:850-851, 909`) and `gatherSample` (`shading.cpp:4440, 4451`) choose the spawning surface's interior or exterior shader by the sign of the ray direction against the spawning point's normal, split the rays into two bundles, and store the choice in `CRayBundle::postShader` (`bundles.h`, inherited by `CTraceBundle`, `CTransmissionBundle`, `CGatherBundle`).
- `CShadingContext::enterRayLevel` (`shading.cpp:588`, called by `CHider::traceBundle`, `src/ri/hiders/trace.cpp:255-266`) copies `bundle.postShader` into the new ray level's shading state. Nothing in `bundles.cpp` ever reads `postShader`. The only consumer is the tail of `shade()`.
- Before that tail, `shade()` assigns `currentShadingState->postShader` unconditionally (`shading.cpp:1396-1400`) from `selectVolumeShader(currentAttributes, isSolidFragment, isExterior)` (`src/ri/state/attributes.h:258-263`). That function returns null unless the *hit* object's attributes carry `ATTRIBUTES_FLAGS_SOLID_FRAGMENT`, a flag only `csgSolid.cpp:273` sets. So for every non-CSG hit the bundle's choice is replaced by null; for a CSG hit it is replaced by the *hit* surface's shader, picked by `sign(I.N)` at the hit.
- The bundle's choice is therefore dead code today. The only live volume behaviour is spec 013/021's "arrival side" rule on CSG solid fragments, which applies to camera rays and secondary rays alike and is documented as spec 013 FR-010, FR-011 and FR-020 (`specs/013-solid-csg-operations/spec.md:96-106`; FR-020 says Interior/Exterior outside a resolved solid "MUST remain a no-op").

**The gaps beyond the overwrite.**

- `traceTransmission` sets `bundle.postShader = NULL` for both the full and the finishing bundle (`executeMisc.cpp:806, 824`) and has no sign test, so `transmission()` and ray-traced `shadow()` never see a volume. `visibility()` is a probe through the same function (`probeOnly`, `executeMisc.cpp:792`): it records distances and never shades anything. The float-valued `trace()` (the distance form) is a probe too (`probeOnly` in `traceReflection`, `executeMisc.cpp:939`; `shading.cpp:3415, 3427`).
- `shade()` is not reached by a ray that hits nothing, so a miss gets no volume. It also returns early before the tail for a shadow hit in primitive hit mode (`:972-984`) and for a camera or secondary hit in primitive hit mode (`:998-1013`), so those hits get none either. When `enterRayLevel` refuses a bundle past `maxRayDepth`, `CHider::traceBundle` calls `postShade` on the unhit rays (`trace.cpp:264-267`) and again no volume runs.
- `incident()` and `opposite()` always read the interior and the exterior shader of the shaded object respectively, through `MessageSource::Interior` and `Exterior` (`shading.cpp:1748, 1752`; `src/ri/render/rendererShadingServices.cpp:418-424`). They do not look at which side of the surface the ray is on. GitHub #120 item 3 records this as unverified against the RISpec.
- `fog` and `depthcue` exist in `shaders/` (`fog` uses `length(I)` and mixes `Ci` toward a background colour, so it needs `I` to be the segment vector, and a finite one). No RIB in the repository uses `Atmosphere`, so camera-ray atmosphere has no test.
- The only RIBs that bind `Interior` or `Exterior` (all under `examples/rib/tests/`) are the two CSG scenes `csg-sphere-sphere-difference-interior-raytrace(-inside).rib`, which use `Surface "matte"` and so trace no secondary ray at all, and the camera-only scenes `sphere-volume-{oi,ci}-default-reyes-{rslo,slo}.rib` and `sphere-shader-parameter-reyes(-slo).rib` (GitHub #60 and spec 017 regressions; they never trace a secondary ray).
- `RISPEC_GAPS.md` (the "Volume Shading" entry) says `Interior` and `Exterior` are "parsed, stored ... and never executed". That is false (they run on CSG fragments), and the entry's cited lines are stale (`shading.cpp:1049`, `rendererContext.cpp:2114/2131`). `DEVNOTES.md` carries the same "stored and never run" claim in its RISpec-gaps row. The comment at `attributes.h:59` still labels `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` "spec 013".

### What RISpec requires (the target)

The RISpec's rule (§4.2.6, §9, §12.3, as summarised by the plan): a volume shader attached to the surface a ray *leaves* attenuates that ray over its whole segment, from the ray's origin to its endpoint (the next hit, or the end of the ray for a miss). Which of the surface's two volume shaders applies is decided where the ray starts, by which side of the surface the ray goes into: into the side the normal points to, the exterior shader; into the other side, the interior shader. The shader runs once per segment, sees `I` as the vector from the ray origin to the endpoint, `P` as the endpoint, and the colour and opacity the ray would otherwise carry in `Ci` and `Oi`, and writes the attenuated colour and opacity back. `Atmosphere` is different: it belongs to the camera, runs only on camera rays, and does not apply to secondary rays.

### User Story 1 - A glass sphere with an interior fog tints what a ray sees through it (Priority: P1)

A scene author puts `Interior "fog"` on a non-CSG sphere with a glass or refractive surface shader that calls `trace()` for the refracted ray. The refracted ray leaves the front surface into the interior side, travels through the sphere and hits the back surface or the world behind. The interior fog must attenuate the segment from where the ray entered to where it hit. Today the fog never runs.

**Why this priority**: This is the defect the maintainer asked to fix. It is the case RISpec §4.2.6 exists for, and the plain non-CSG sphere is the smallest scene that shows it.

**Independent Test**: Render a sphere with `Interior "fog"` (a distance and a background colour that make the effect visible), a refractive surface shader and a lit backdrop, on the raytrace hider. Compare against the same scene with no `Interior`: the image differs, the sphere's refracted view shifts toward the fog background with the path length through the sphere, and the shift matches a fog value computed by hand from the segment length for sample pixels.

**Acceptance Scenarios**:

1. **Given** a non-CSG sphere with `Interior "fog"` and a surface shader that traces a refracted ray into it, **When** the ray from the front hit reaches the back wall, **Then** the fog runs once on that segment with `I` equal to the vector from the entry point to the back-wall hit, and the returned colour is the fog-attenuated one.
2. **Given** the same scene with no `Interior` statement, **When** it is rendered, **Then** the image is byte-identical to the image before this spec.
3. **Given** the same sphere with `Interior "fog"`, **When** the ray leaves the sphere into the exterior side and the sphere has no `Exterior` shader, **Then** no volume shader runs for that segment (the missing side is not an error and not a default).

---

### User Story 2 - An exterior shader attenuates rays that leave a surface outward, including rays that miss (Priority: P1)

A scene author puts `Exterior "fog"` on a reflective sphere. A reflected ray leaves the surface into the exterior side. If it hits another object, the exterior fog attenuates the segment up to that hit. If it hits nothing, the fog still applies to the segment from the origin to the end of the ray, so a reflection of empty space is tinted by the fog background rather than staying black.

**Why this priority**: Hits and misses are both required by the RISpec rule, and misses are where today's code has no hook at all. Without the miss case a foggy room's reflections would be wrong at every silhouette that sees the sky.

**Independent Test**: A reflective sphere with `Exterior "fog"`, a second object to be reflected and a region of empty space to be reflected. Sample pixels in the reflection of the object and in the reflection of empty space; both differ from the no-`Exterior` render by the hand-computed fog value for their segment lengths.

**Acceptance Scenarios**:

1. **Given** a reflective sphere with `Exterior "fog"`, **When** a reflected ray hits another object, **Then** the fog runs on the segment origin to hit and attenuates the colour that segment returns.
2. **Given** the same sphere, **When** a reflected ray hits nothing, **Then** the fog runs on the segment from the origin to the end of that ray (the end is defined by FR-007) and its result is what the ray returns.
3. **Given** a hit surface that is in primitive hit mode, **When** the segment ends there, **Then** the segment is still attenuated by the spawning surface's shader (FR-005). A ray refused past the maximum ray depth returns nothing and runs no volume (FR-009, A6).

---

### User Story 3 - Transmission and shadow rays are attenuated by the volume they cross (Priority: P2)

A light's shadow, or a shader's `transmission()` or `visibility()` call, is a ray from a surface point toward another point. When the shooting surface has a volume shader on the side the ray goes into, the transmission along that segment is reduced by it, so a ray leaving the inside of a glass sphere with `Interior "fog"` toward a light is tinted and partly opaque rather than clear. The shooting surface decides: a shadow ray from an arbitrary receiver (a floor) crossing a foggy sphere is not tinted by the sphere under this spec, since a transparent hit does not switch the shader (A5). Tinted glass shadows on arbitrary receivers would need medium switching at transparent hits and are a possible follow-up, not part of this spec.

**Why this priority**: The plan lists transmission and visibility rays with the others, and `traceTransmission` is the gap with no sign test at all today. It is P2 because `traceTransmission` is the entry point with no sign test at all today.

**Independent Test**: A ray-traced shadow (or `transmission()`) cast from a point on the sphere's own back wall, with `Interior "fog"` bound to the sphere, toward a light so that the ray goes into the interior side. The value differs from the no-`Interior` render by the hand-computed fog attenuation over the segment from the origin to the light (the transmission destination); the transparent front-wall hit does not change the shader.

**Acceptance Scenarios**:

1. **Given** a shooting surface with an `Interior` or `Exterior` shader on the side the transmission ray goes into, **When** `transmission()` or a ray-traced `shadow()` is evaluated, **Then** the result includes the volume's attenuation for that segment.
2. **Given** no volume shader on either side, **When** the same call runs, **Then** the result is byte-identical to the result before this spec.
3. **Given** `visibility()` between two points, or a float-valued `trace()` (the distance form), **When** a volume shader exists on the spawning surface, **Then** the call keeps its probe semantics and no volume step runs: both record a distance and shade nothing.
4. **Given** a shadow or transmission ray that nothing occludes, **When** its segment ends, **Then** it ends at the light or the transmission destination, and the volume step (if any) sees that finite segment.

---

### User Story 4 - `gather()` rays are attenuated like `trace()` rays (Priority: P2)

A shader that gathers rays (hit colours and other hit outputs) sees the same volume rule: gather rays leaving the surface into the interior side are attenuated by the interior shader, into the exterior side by the exterior shader. `gatherSample` already splits the rays this way and hands the choice to the bundle; it is only ignored later.

**Why this priority**: Same defect, same fix, a second entry point. P2 because `trace()` is the first scene and `gather()` shares the mechanism.

**Independent Test**: A gather of the colour output over a hemisphere around a point on a sphere with `Exterior "fog"`; the gathered average differs from the no-`Exterior` render by the hand-computed segment attenuation.

**Acceptance Scenarios**:

1. **Given** a gather with `Interior` and `Exterior` shaders bound, **When** rays go into each side, **Then** each ray's segment is attenuated by the shader of its own side before its outputs are recorded.
2. **Given** a gather that records outputs that need no shading (such as `ray:direction`), **When** a volume shader applies, **Then** those outputs are unchanged by it (the volume changes colour and opacity only, FR-004).

---

### User Story 5 - CSG solids obey the same rule instead of a rule of their own (Priority: P2)

Today the interior and exterior shaders of a CSG solid run only for hits on the solid's boundary fragments, picked by which side the ray *arrives* on. After this spec a CSG solid has no special path: the fragments' attributes carry their shaders like any surface, and the spawning rule applies. Spec 013 FR-020 ("outside a solid block it remains a no-op") is superseded on purpose, since `Interior` and `Exterior` now mean the same thing on every surface.

**Why this priority**: The maintainer asked for CSG to become a case of the general mechanism, which removes the `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` special case and a second selection path. It is P2 because it changes two existing reference images and so needs the maintainer's approval of each before it is re-rendered.

**Independent Test**: The two CSG interior scenes, rewritten in place with a reflective or refractive surface (names and test IDs kept; today they use `Surface "matte"` and trace nothing), render with the general rule: the camera-hit fog the old arrival-side rule painted over the boundary is gone, and the fog appears on secondary rays leaving a solid fragment. Both references are re-rendered, and each is inspected and approved by the maintainer image by image. The rewrite and re-render happen at the implement stage.

**Acceptance Scenarios**:

1. **Given** a CSG difference solid with `Interior` and `Exterior` shaders and a refractive or reflective surface shader, **When** a secondary ray leaves a boundary fragment, **Then** the segment is attenuated by the shader on the side the ray enters, using the fragment's orientation-corrected normal, exactly as for a non-CSG surface.
2. **Given** a camera ray that hits a CSG fragment, including a camera inside the remaining material of a solid, **When** the scene is rendered, **Then** no volume step runs for it: a camera ray has no spawning surface, and there is no CSG-aware camera-start test (FR-011).
3. **Given** `selectVolumeShader` and `ATTRIBUTES_FLAGS_SOLID_FRAGMENT`, **When** the spec is complete, **Then** both are retired, and the unit test `CSG_InteriorExteriorSelection` is retired with it (the maintainer's approval of that retirement is recorded in Clarifications, 2026-11-07).

---

### User Story 6 - Atmosphere is covered and unchanged (Priority: P2)

`Atmosphere "fog"` and `Atmosphere "depthcue"` get scenes, on reyes, zbuffer and raytrace, because none exists today. They must keep their current behaviour: camera rays only, never secondary rays, never a matte object.

**Why this priority**: RISpec says atmosphere is not a per-segment secondary-ray effect, and the new volume step must not leak into it or out of it. The scenes fix the current behaviour as a regression guard before the volume step changes the code beside it.

**Independent Test**: Render each atmosphere scene on the three camera hiders and compare against the pre-change output (they must be byte-identical), and render with a reflective surface in the scene to show the atmosphere does not appear in the reflection.

**Acceptance Scenarios**:

1. **Given** `Atmosphere "fog"`, **When** the scene is rendered on reyes, zbuffer and raytrace, **Then** camera-ray hits are fogged and the three hiders agree within the project's parity rule.
2. **Given** a reflective surface in the same scene, **When** a reflected ray hits an object, **Then** the atmosphere does not run on that hit.
3. **Given** the spawning rule alone, **Then** no camera ray carries a volume shader and no secondary ray carries an atmosphere, so no ordering between them is defined or needed. (For the record, the tail of `shade()` runs atmosphere at `shading.cpp:1443-1449` and then the volume shader at `:1452-1456`; the specify-stage text that claimed the opposite was wrong.)

---

### User Story 7 - `incident()` and `opposite()` read the right side's shader (Priority: P3)

A surface shader that calls `incident("param", x)` reads a parameter of the volume shader on the side the incoming ray comes from, and `opposite()` the other side. Today `incident()` always reads the interior shader and `opposite()` the exterior one, whichever side the ray is on. The plan's test list asks for these to be checked from both sides.

**Why this priority**: It uses the same sign-of-`I.N` decision and the same slots, and a surface shader and a volume shader that cooperate (the surface asks the volume for its density) need both to agree. It is P3 because it is the smallest slice and issue #120 item 3 already tracks it separately (see "Overlapping issues").

**Independent Test**: A surface with `Interior "param_query_volume"` and `Exterior "other_volume"` that differ in a parameter; a camera ray hits it from outside and a second ray from inside; `incident()` returns the exterior shader's value for the first and the interior shader's for the second, `opposite()` the reverse.

**Acceptance Scenarios**:

1. **Given** a ray arriving at the surface from the exterior side, **When** the surface shader calls `incident()` and `opposite()`, **Then** `incident()` reads the exterior shader and `opposite()` reads the interior shader.
2. **Given** a ray arriving from the interior side, **Then** `incident()` reads the interior shader and `opposite()` reads the exterior shader.
3. **Given** the interpreter (`.rslo`) and the JIT (`.slo`), **Then** both give the same result on every case above.

---

### User Story 8 - Nothing else changes (Priority: P1)

Every scene that binds no `Interior` or `Exterior` shader renders byte-for-byte as before on every hider, and the determinism tests still pass. The camera-only scenes that bind the shaders without tracing (GitHub #60) are unchanged.

**Why this priority**: This is a fix inside the shading hot path; the cost of a regression is the whole visual suite.

**Independent Test**: The everyday full run (`-LE determinism-sweep`) and `-L architecture` pass; every reference image except the two superseded CSG scenes is unchanged and no threshold is widened.

**Acceptance Scenarios**:

1. **Given** the existing scenes, **When** the suite runs, **Then** every scene except the superseded ones passes with its existing reference and threshold.
2. **Given** a scene with volume shaders that call `random()`, **When** it is rendered at different thread counts, **Then** the images are identical (the segment step carries sample identities like any other shading).

---

### User Story 9 - The documents tell the truth (Priority: P3)

`RISPEC_GAPS.md`'s volume-shading entry, the `DEVNOTES.md` status row and the stale comment at `attributes.h:59` describe what the code does after this spec.

**Why this priority**: These statements were wrong for months. The plan asks for them to be corrected as part of the spec.

**Independent Test**: A reviewer reads the entry and follows its citations; each cited file and line holds what the entry says.

**Acceptance Scenarios**:

1. **Given** the finished spec, **When** `RISPEC_GAPS.md` is read, **Then** volume shading is marked implemented, the "never executed" claim is gone, and every citation is current.

---

### Edge Cases

- A surface with only one of the two shaders: a ray going into the side that has none gets no volume step. A missing shader is not an error.
- A ray that grazes (`dir . N == 0` exactly): the existing rule decides it (`> 0` is exterior); the spec keeps that rule and does not invent a third case.
- `dir . N` computed from which normal: the shading normal, flipped by `Orientation` and by a CSG operand's flip. The sign must agree with the side `incident()` and `opposite()` treat as exterior.
- Transparent surfaces (Oi < 1) in the way of a ray: the ray keeps going in the same medium (a transparent hit is not a medium boundary), so the continuing segment is attenuated by the same shader. Assumption A5.
- A ray past `maxRayDepth`: it is refused and returns nothing, so no volume runs for it (Assumption A6). A volume shader that calls `trace()` counts toward the depth, and the rays it spawns get no volume step (FR-009).
- A volume shader that reads `N`, `Cs`, `u`, `v`: it only sees the globals RISpec Table 12.3 gives volume shaders; the others are unspecified, and the existing comment that interior and exterior "don't receive pl variables" still holds.
- A volume shader on a matte object: matte hides the object from camera-ray atmosphere today. A matte spawning surface's volume shader still attenuates the rays it spawns (Assumption A7).
- Displacement-only and derivative-estimate passes never run a volume shader.
- Motion blur and depth of field: a segment's endpoint is wherever the ray at its own time hits; the volume step uses that.
- Photon, occlusion and indirect-diffuse rays are not radiance segments: they get no volume step (Assumption A8).
- The segment from a camera to its first hit is the camera's atmosphere, not a surface's interior or exterior: no volume step, including for a camera inside a CSG solid (FR-011).

## Requirements *(mandatory)*

### Functional Requirements

**The rule**

- **FR-001**: For every ray traced by `trace()` (including `environment("raytrace")`, which is the same entry point), by `gather()` and by `transmission()` or ray-traced `shadow()`, the system MUST select the volume shader from the spawning surface (the surface at the ray's origin, the one the shading call is running on). It MUST select the exterior shader if the ray direction has a positive component along that surface's normal and the interior shader otherwise, with the same normal and the same tie rule for all three entry points.
- **FR-002**: The system MUST run the selected volume shader once per ray segment, over the whole segment from the ray origin to its endpoint, for every surface regardless of whether it is a CSG fragment, and regardless of whether the endpoint is a hit or a miss.
- **FR-003**: A volume shader MUST see `I` as the (unnormalised) vector from the segment's origin to its endpoint, `P` as the endpoint, and `Ci` and `Oi` as the colour and opacity the ray would otherwise return; it MUST NOT see surface-only globals. Its writes to `Ci` and `Oi` are the ray's result. (RISpec Table 12.3; `fog` relies on `length(I)`.)
- **FR-004**: The volume step MUST change colour and opacity only. It MUST NOT change a hit's position, normal, hit identity or the gather outputs that need no shading.
- **FR-005**: A hit that is not shaded (a primitive hit mode, a shadow hit that reads the primitive) MUST still have its segment attenuated by the spawning surface's shader; hit mode decides how the *hit* gets its colour, not whether the *segment* is attenuated.
- **FR-006**: `Atmosphere` MUST keep running on camera rays only, never on a secondary ray, never on a matte object, exactly as before. The volume step MUST NOT run the atmosphere and the atmosphere step MUST NOT run a volume shader; the two never apply to the same ray, so no ordering between them exists.

**Misses, depth and the segment sequence**

- **FR-007**: A `trace()` (including `environment("raytrace")`) or `gather()` ray that hits nothing MUST still receive its segment's volume step. For a `gather()` miss, the step's colour and opacity MUST be written only into the `surface:Ci`/`surface:Oi` outputs of the missed rays: the ray stays a miss (the `else` branch, tags and `numMisses` unchanged), and no other output changes. The segment ends where the ray exits the frame's world bound, or at a finite `maxdist` where one applies (`gather()` honours `maxdist` today, `shading.cpp:4414`; `trace()` ignores it, GitHub #98, which this spec does not change). A `maxdist` of `C_INFINITY` (the default) counts as unset. `I` MUST always be finite. The far clipping distance MUST NOT be used: its default is `C_INFINITY` (1e30), which is also the no-hit sentinel, and `length(I)` overflows. A shadow or transmission ray that nothing occludes ends at the light or the transmission destination (FR-010). How a ray whose origin is outside the world bound is measured is a plan detail; it MUST still give a finite `I`.
- **FR-008**: When a ray passes through a transparent surface and continues, each continuing segment MUST be attenuated by the same spawning shader, and the sum of the segments MUST equal the attenuation over the whole path as seen by a shader that integrates the path in one piece up to the shader's own approximation (Assumption A5).
- **FR-009**: A ray that `enterRayLevel` refuses because the ray depth limit is reached MUST NOT run a volume shader (it returns nothing). A volume shader runs at the depth of the segment it attenuates; a `trace()` inside it enters one level deeper like any shader's, counts toward `maxRayDepth`, and a ray refused there returns nothing. Rays a volume shader spawns have no spawning surface and MUST get no volume step.

**Transmission, shadow and visibility**

- **FR-010**: `transmission()` and ray-traced `shadow()` MUST apply FR-001 to FR-005 to their segments, with the same spawning-surface rule as `trace()` and `gather()`: the volume shader is the shooting surface's, chosen by the sign of the ray direction against the shooter's normal. `traceTransmission` therefore needs the same sign split `traceReflection` has. A transparent hit does not switch the shader (A5, FR-008). An unoccluded segment ends at the light or the transmission destination. `visibility()` and the float-valued `trace()` are probes and MUST keep their probe semantics: no volume step runs for them.

**CSG**

- **FR-011**: A CSG solid MUST obey FR-001 to FR-005 through its fragments' own attributes and orientation-corrected normals. The special selection (`selectVolumeShader`, keyed on `ATTRIBUTES_FLAGS_SOLID_FRAGMENT`) and the flag itself MUST be retired. There is no CSG-aware camera-start interior test: a camera ray has no spawning surface and gets no volume step, inside a solid or not.
- **FR-012**: Spec 013 FR-020, the 021 interior scenes `csg-sphere-sphere-difference-interior-raytrace` and `-inside` (in `examples/rib/tests/`), and the unit test `CSG_InteriorExteriorSelection` are superseded on purpose. At the implement stage the two scenes MUST be rewritten in place with a reflective or refractive surface so they exercise the new rule (names and test IDs kept), and both references re-rendered, each inspected and approved by the maintainer image by image. `CSG_InteriorExteriorSelection` is retired; the maintainer approved that retirement (Clarifications, 2026-11-07). Two more approved tests lose code that names the retired symbols. `State_Isolation` loses the `selectVolumeShader` checks at `tests/unit/state/test_state_isolation.cpp:450-458`. `CSG_ProceduralOperand` loses the flag assertion at `tests/unit/csg/test_procedural_operand.cpp:382`; its `INSIDE` parity check is kept. Each edit MUST be shown to the maintainer as a diff and approved before it is applied (plan-stage corrections, 2026-11-07). No tolerance may be widened to keep an old reference green.

**`incident()` and `opposite()`**

- **FR-013**: `incident()` MUST read the volume shader on the side of the surface the incoming ray comes from, and `opposite()` the shader on the other side, in both the interpreter and the JIT (RISpec §15.8 `incident`: the volume that contains the incident ray `I`). The side is chosen once per call from vertex 0's `sign(I.N)`, the approximation `shade()` already uses at `shading.cpp:1396-1400`, by Shading passing the Interior or Exterior source to the existing message query; the services interface and `kRendererServicesVersion` do not change. This covers the interpreter macros' argument order (`shaderFunctions.h:1331-1344`). It absorbs GitHub #120 item 3.

**Parity, determinism, boundaries**

- **FR-014**: The results of FR-001 to FR-013 MUST be the same on reyes, zbuffer and raytrace (the camera hiders; photon and show are not parity subjects), and the same under the interpreter and the JIT, within the project's existing parity thresholds.
- **FR-015**: The volume step MUST be deterministic: the same image for the same scene at any thread count. Points shaded by the segment step carry sample identities like every other shaded point (spec 039).
- **FR-016**: Every scene that binds no `Interior` or `Exterior` shader MUST render byte-identical to the image before this spec, on every hider.
- **FR-017**: The spec MUST add no line to `tests/architecture/allowlist.txt` and no include or link edge forbidden by constitution Principle VIII. Shading MUST reach attributes and shaders only through the handles it already uses. The segment step lives in Shading's bundles (maintainer decision, 2026-11-05); hiders and Geometry name no volume type.
- **FR-018**: The system MUST NOT add a participating-media integrator, a medium stack, an automatic camera medium, volume closures, or any behaviour for non-classic integrators. How those treat volumes belongs to specs 042, 049 and 050.

**Tests and documents**

- **FR-019**: Tests MUST be written before the change and be seen failing on the starting tree (constitution Principle III). The scenes MUST include: a non-CSG glass sphere with `Interior "fog"`; an exterior fog on a reflective object; a transmission shadow through an interior; a miss with an exterior shader; a gather through both sides; `Atmosphere "fog"` and `Atmosphere "depthcue"` on three hiders; and `incident()`/`opposite()` from both sides, each on reyes, zbuffer and raytrace, interpreter and JIT.
- **FR-020**: `DEVNOTES_DETAILS/RISPEC_GAPS.md` (the volume-shading entry, its citations and its verification date), the `DEVNOTES.md` RISpec-gaps row, the comment at `attributes.h:59` and the site page section `docs/site/content/manual/reference/solid-csg-operations.md:221-256` ("Interior / Exterior shading", which documents the retired CSG rule; rewritten at the implement stage and shown to the maintainer first) MUST be corrected. Two comments in the versioned Shading headers MUST also be corrected, wording only, with no `kRendererServicesVersion` step:
  - `rendererServices.h:312`: the world bound is in camera space, not world space. This edit is made only after the non-identity-camera probe confirms it.
  - The "Re-enters Shading" note in `tracer.h`: it gains that `postShade` may run the segment's volume shader.

  Registry entries, if any move, move to Resolved with the spec number and the date.
- **FR-022**: Any edit to an approved contract test that FR-013 requires (`tests/shading_contract/case_messages.cpp:135-136`, the `cm_*` shaders) MUST be shown to the maintainer as a diff and approved before it is made; no such test is edited otherwise. Verified at the plan stage: none is needed. The harness sets `I = N = (0,0,1)` (`contract_harness.h:105, 107`), so `I·N > 0`, and under FR-013 `incident()` keeps reading Interior and `opposite()` Exterior. `ShadingContract_MessageAccessors` is unaffected, because the accessor constants stay. If implementation finds otherwise, the diff is shown first.
- **FR-021**: No requirement, task or success criterion of this spec may depend on the merge. Issue closure is the maintainer's `Closes #NN` line in the merge commit.

### Key Entities

- **Spawning surface**: the surface a traced ray leaves. It owns the Interior and Exterior slots that decide the segment's volume, and the normal and orientation that decide which of the two applies.
- **Ray segment**: the interval of one ray from its origin to its endpoint (a hit, a miss end, or the destination of a transmission ray). The unit a volume shader attenuates. A ray that passes through transparent surfaces is several segments in the same medium.
- **Volume shader**: an `Interior`, `Exterior` or `Atmosphere` shader. Reads `I`, `P`, `Ci`, `Oi` and its own parameters; writes `Ci` and `Oi`.
- **Side**: which of a surface's two volumes a ray enters, decided by the sign of the ray direction against the surface's orientation-corrected normal.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: In each new scene of FR-019 that has an `Interior` or `Exterior` shader, sample pixels in the affected region differ from the same scene without the shader by the attenuation computed by hand from the segment length (within the shader's own arithmetic), and agree across reyes, zbuffer and raytrace and across interpreter and JIT within the existing thresholds.
- **SC-002**: A refraction through a non-CSG glass sphere with an interior fog shows the fog; the same scene on the starting tree does not. The new tests fail on the starting tree and pass after.
- **SC-003**: A reflected ray that misses, with an exterior fog on the reflecting surface, returns the fog's value, not black, in every new miss scene.
- **SC-004**: Every existing scene that binds no `Interior` or `Exterior` shader renders byte-identical, and every existing threshold is unchanged. The only reference changes are the two CSG interior scenes (rewritten in place), and each is re-rendered, inspected and approved by the maintainer image by image.
- **SC-005**: `ctest --test-dir build -LE determinism-sweep -j4` and `ctest --test-dir build -L architecture` pass, the allowlist gains no line, and the determinism set renders each new scene identically at different thread counts.
- **SC-006**: `incident()` and `opposite()` return the exterior and interior shader's values for a ray from outside, and the swapped values for a ray from inside, in both backends.
- **SC-007**: The atmosphere scenes render byte-identical to the pre-change output on each camera hider and show no atmosphere in a reflection.
- **SC-008**: `RISPEC_GAPS.md` has no "never executed" claim, and every file and line it cites holds what it says.
- **SC-009**: In every miss scene, the `I` a volume shader sees is finite, whatever `Clipping` the scene sets, and no volume step runs for `visibility()` or float-valued `trace()`.

## Assumptions

- **A1 (classic semantics only)**: This spec describes behaviour under today's path, which spec 030 will name `Integrator "classic"`. It is written so it holds whether or not 030 has merged: a plain `Hider "raytrace"`, `"reyes"` or `"zbuffer"` scene with no `Integrator` statement behaves as specified. Non-classic integrators (042, 049, 050) decide their own volume treatment.
- **A2 (volume shaders integrate their own segment)**: There is no participating-media integrator; the shader is responsible for integrating its segment, as RISpec §16.3 says.
- **A3 (placement)**: The segment step lives in Shading's bundles (maintainer decision 2026-11-05; spec 029 moved them into `src/libshader/shading/bundles.{h,cpp}`). That is a constraint, not a design; the plan stage decides how `postShade` reaches the context that runs a shader.
- **A4 (hit-agnostic)**: The step depends on the spawning surface and the segment, not on the hit surface's shader, hit mode, matte flag or class.
- **A5 (transparency)**: A transparent hit is not a medium boundary: the ray continues in the same medium. A continuing segment starts where the previous one ended. Tinted glass shadows on arbitrary receivers would need medium switching at transparent hits and are a possible follow-up, not this spec.
- **A6 (depth)**: A refused ray returns nothing, so it gets no volume. A volume shader's own `trace()` counts toward the depth and its rays get no volume step (FR-009).
- **A7 (matte)**: A matte object's volume shaders attenuate the rays it spawns; matte only hides it from camera-ray atmosphere.
- **A8 (other ray kinds)**: `occlusion()`, `indirectdiffuse()`, photon and camera rays get no spawning-surface volume step. Camera rays get only the atmosphere. Float-valued `trace()` and `visibility()` are probes and get none either.
- **A9 (orientation)**: The side test uses the normal as shading sees it (after `Orientation`, `ReverseOrientation` and a CSG operand's flip), the same normal `incident()` and `opposite()` use.
- **A10 (no new attribute or option)**: The existing `Interior` and `Exterior` statements and slots carry everything; no RIB syntax, token, option or shader-language change.
- **A11 (references)**: A superseded reference is re-rendered and inspected only with the maintainer's approval. No threshold is widened.

### Out of scope

- Area lights (spec 057), the hider and integrator registry (030), Geometry becoming hider-agnostic (031, which also touches `csgSolid.cpp`), the GI cache split (032).
- A medium stack, nested media, camera inside a medium, volume closures, medium switching at transparent hits (tinted glass shadows on arbitrary receivers), null-scatter or participating-media integration.
- The hit-mode defects #117 to #119, `trace()`'s ignored `maxdist` (#98; FR-007 does not depend on it), the transmission re-entrancy (#148).

### Discrepancies with the plan and the issue (found at the specify stage)

1. **Line numbers**: the plan's `shading.cpp:1308-1312, 1365, 1356` are now `1384-1400`, `1452-1456`, `1443-1449`; `executeMisc.cpp:891` is `850-851, 909`; `shading.cpp:4327/4345` is `4440/4451` (`gatherSample`); `traceTransmission` NULLs are `806/824` (unchanged); `attributes.h:244-246` is the slot definitions (unchanged) and `:258` the selector.
2. **`traceEx` no longer exists**: the plan says "`traceEx` copies it (`trace.cpp:267`)". Since spec 029 it is `CHider::traceBundle` calling `CShadingContext::enterRayLevel` (`shading.cpp:588`).
3. **"Overwritten before anyone reads it"** is exact, but sharper: the bundles never read `postShader` at all; the choice goes to the shading state and `shade()` replaces it. For a non-CSG hit the replacement is null; for a CSG hit it is the hit's own shader.
4. **The ordering question in the plan's clarify list** (volume before or after atmosphere on camera rays) has no case under the spawning rule: atmosphere is depth 0 only, a volume shader needs a spawning surface (depth 1 and deeper). Closed as not applicable at clarify (the camera-start medium was dropped).
5. **`incident()`/`opposite()` "already resolve through `getMessage`"** is true but they resolve to a fixed shader, not the side-dependent one the plan's "both sides" test needs; that is GitHub #120 item 3.
6. **Hit-mode early returns** (primitive hit mode at `shading.cpp:972-984` and `998-1013`) skip the tail of `shade()`; a fix that only stops the overwrite would not attenuate those segments or any miss, which is why the plan puts the step in the bundle `postShade`.
7. **`RISPEC_GAPS.md`** entry cites also `rendererContext.cpp:2114/2131` (now `2372/2385`); its "Area Light Sources" entry (`:2045`) is stale too but belongs to spec 057. `DEVNOTES.md` repeats the false "stored and never run" claim.
8. **The CSG interior scenes** use `Surface "matte"`, which traces nothing, so under the spawning rule they lose all their fog and render as plain matte solids; they stop testing anything unless rewritten with a reflective or refractive surface (FR-012). The plan's "re-rendered and inspected" understated this.
10. **Found at clarify**: the RIBs are in `examples/rib/tests/`; the default far clip is `C_INFINITY`, not finite; `gather()` and `traceTransmission` honour `maxdist` and only `trace()` ignores it; `probeOnly` is at `executeMisc.cpp:792`, and float `trace()` is a probe too; the specify text had the atmosphere/volume order backwards.
9. **`selectVolumeShader` is in `src/ri/state/attributes.h`**, not `src/ri/attributes.h` as the header comment of `tests/unit/csg/test_interior_exterior_selection.cpp` says.

### Overlapping issues (open Bug Tracker, checked 2026-11-06)

- **#120** (verify three shading behaviours against the RISpec): item 3 (`incident()`/`opposite()` side) is the same defect family and is absorbed by FR-013. Item 1 (atmosphere depth 0 only and never on matte) is preserved by FR-006 and stays open for the maintainer's RISpec check. Item 2 (shadow hit skips displacement) is untouched.
- **#148** (`traceTransmission` asserts when a shadow hit in `"shader"` hit mode calls `transmission()`): the same function this spec edits and the same `inShadow` flag. Not part of this defect and not absorbed. The new transmission scenes must avoid that nesting, and the plan stage must not make it worse.
- **#98** (trace `maxdist` ignored; `sampleBase` spelling): FR-007 no longer depends on it. Not absorbed.
- **#99** (`visibility()` and float `trace()` reject optional parameter lists): touches `visibility()`, which keeps probe semantics here; not absorbed.
- **#117, #118** (hit-mode attributes honoured for some hiders only, `diffusehitmode` unread): decide when a hit is shaded; FR-005 requires the segment step be independent of hit mode. Not absorbed.
- **#138** (trace object hash sentinel): same trace code, cleanup only.
- **#8** (JIT uniform assignment in `if`): closed.
- Concurrent specs: **031** also edits `csgSolid.cpp`; this spec's edit there is only the removal of the fragment flag (one line at `:273`), to be coordinated at merge time. **030** and **032** are untouched.
