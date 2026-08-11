# Feature Specification: Full Subdivision Surface Support

**Feature Branch**: `010-full-subdivision-support`

**Created**: 2026-08-10

**Status**: Draft

**Input**: User description: "Review the current support to subdivision surfaces in the codebase and propose the correct requirements to add full support for it, following the geometry/hider/shading separation precedent of spec 009-nurbs-trim-curves. Motion blur (and every artifact effect, present or future) must never receive per-hider special-case treatment — each must be implemented once, generically, so every visually capable hider (REYES, raytrace, and a planned future PathTrace hider) inherits it uniformly."

## Clarifications

### Session 2026-08-11

- Q: Does the photon-mapping hider need to gain motion-blur support in this feature (matching raytrace), or should it be treated like CShow — test scenes authored to document the gap, but not required to pass? → A: Treated like CShow — photon-hider motion-blur test scenes are authored as deliverables but not required to pass; this feature's motion-blur scope (User Story 1) stays limited to REYES/stochastic and ray-tracing.
- Q: For the crease-quality investigation (User Story 4), if the reproduced problem turns out to be a performance/efficiency issue rather than a visual artifact, what counts as "reproduced"? → A: Qualitative confirmation only — a visibly worse render (artifact) or a noticeably slower render relative to a comparable lightly-creased mesh; no numeric performance threshold is required for this feature.
- Q: When a hierarchical subdivision mesh override targets a face index or subdivision level that doesn't exist on the base mesh, should the renderer reject the whole primitive, or just skip that one invalid override? → A: Skip just the invalid override (with a diagnostic) and render the rest of the mesh normally, matching this codebase's existing fail-small precedent (e.g. spec 009 rejects one malformed trim loop, not the whole NuPatch).
- Q: Does the ray-tracing hider need a new motion-blur mechanism built for this feature, or does one already exist? → A: One already exists — confirmed by direct source read of `CTesselationPatch::sampleTesselation()`/`intersect()` (`src/ri/surface.cpp:1393-1513`, `1164-1319`), reached generically via the `CObject`/`CSurface` virtual-dispatch contract (`object.cpp:533-574`) for any primitive with `moving() == true`, including subdivision surfaces (`CSubdivision::sample()`, `subdivision.cpp:149-188`, already selects the correct time sample via the `PARAMETER_BEGIN_SAMPLE`/`PARAMETER_END_SAMPLE` flags it is passed). User Story 1 and FR-001/FR-002 are corrected accordingly: this feature verifies and documents the existing mechanism rather than building a new one.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Cross-hider motion blur is verified and its test-coverage gap closed (Priority: P1)

A scene author animates any moving primitive — not just a subdivision surface — and expects motion blur to appear whether the scene is rendered with the REYES/stochastic hider or the ray-tracing hider. Investigation for this feature found that the ray-tracing hider already supports motion blur today, generically, via the shared tessellation-and-intersection layer (`CTesselationPatch`, `src/ri/surface.cpp`): `CTesselationPatch::sampleTesselation()` (surface.cpp:1393-1513) samples any wrapped `CObject`/`CSurface` twice — once via `context->displace(..., PARAMETER_BEGIN_SAMPLE)` at time 0.0 and once via `PARAMETER_END_SAMPLE` at time 1.0 — whenever the wrapped object's `moving()` returns true, and `CTesselationPatch::intersect()` (surface.cpp:1164-1319) LERPs between the two stored tessellation grids at `cRay->time` before intersecting. This pathway is reached purely through the existing `CObject`/`CSurface` virtual-dispatch contract (`object.cpp:533-574`), so it already covers every primitive type with `moving() == true` — including subdivision surfaces, since `CSubdivision::moving()` (subdivision.h:45) returns `vertexData->moving`, and `CSubdivision::sample()` (subdivision.cpp:149-188) already selects the correct one of its two doubled-time-sample vertex buffers via the `PARAMETER_BEGIN_SAMPLE`/`PARAMETER_END_SAMPLE` flags it is passed. There is no missing mechanism to build. What is missing is dedicated cross-hider test coverage — this pathway has none today — and a `HIDER_PARITY.md` entry that names subdivision surfaces specifically (its existing motion-blur closure note, spec 008/D10, predates this feature and does not mention them). This story closes that verification and documentation gap.

**Why this priority**: Every other story in this spec that touches subdivision-surface motion (exercising `CSubdivision::sample()`'s moving-geometry code path across hiders) depends on confirming the ray-tracing hider actually renders motion blur correctly, even though the mechanism itself needs no new code. This is also the first concrete application of the standing architectural rule that no artifact effect may have per-hider special-case treatment — this story demonstrates the rule is already satisfied for motion blur, setting the documented pattern every future effect (e.g. depth of field) and future hider (e.g. a planned PathTrace hider) must follow.

**Independent Test**: Render a scene with a translating primitive and a scene with a rotating primitive, each once under REYES/stochastic and once under ray-tracing, using an existing non-subdivision primitive first (to confirm the pre-existing mechanism is generic, not subdivision-specific) and then a subdivision surface (User Story 2's domain), and confirm both hiders produce a comparable motion-blurred result within the project's existing block-average visual-diff threshold — with no code changes required for either scene to pass.

**Acceptance Scenarios**:

1. **Given** a moving primitive with a pure translation, **When** rendered under the ray-tracing hider, **Then** the output shows a motion-blurred streak comparable to the equivalent REYES/stochastic render within the existing visual-diff threshold, using the pre-existing `CTesselationPatch` mechanism with no code changes.
2. **Given** a moving primitive undergoing pure rotation authored via an object-level `MotionBegin`/`Rotate`/`MotionEnd` block, **When** rendered under both hiders, **Then** both apply the same two-time-sample position interpolation to the primitive's tessellated geometry and produce comparable results within the existing visual-diff threshold. Camera-driven rotation blur (a separate mechanism gated by `CRenderer::cameraHasRotation` that SLERPs the camera's own transform, not object geometry) is out of scope for this scenario; whether it also needs to reach the ray-tracing hider's primary-ray generation is tracked as a research question, not asserted here.
3. **Given** the ray-tracing hider's motion-blur mechanism, **When** reviewed against the codebase, **Then** it is confirmed to already live entirely in the shared ray-tracing/hider layer with no primitive-type-specific branch — any primitive with `moving() == true` is blurred without additional code.
4. **Given** a static (non-moving) scene, **When** rendered under the ray-tracing hider, **Then** rendering time and output are unaffected (no motion-blur code path is exercised) — confirming this feature's verification work introduces no regression.

---

### User Story 2 - Facevarying UV-seam data survives on shared vertices (Priority: P1)

A scene author authors a `SubdivisionMesh` with facevarying data (typically UV coordinates) that deliberately differs across the corners of a shared vertex — the standard way to represent a texture seam. Today that per-corner data silently collapses to a single value: `CSVertex` stores one `facevarying` pointer per topological vertex, and the per-face assignment loop that builds it overwrites the same vertex's pointer once per incident face, so only the last-processed face's value survives. This story fixes that data-loss bug so each face corner retains its own authored facevarying value.

**Why this priority**: This is a confirmed, silent data-loss bug (verified by direct source read, not inference) that defeats the entire purpose of facevarying data. It blocks correct texturing of any subdivision mesh with a UV seam and must be fixed before any other facevarying-adjacent work (missing tags, hierarchical overrides) can be verified as correct.

**Independent Test**: Render a subdivision mesh with a UV seam (a shared vertex whose incident faces carry distinct facevarying UV values) under the ray-tracing hider — the shading ground-truth hider per this project's conventions — and confirm the texture seam is visible and correctly discontinuous, rather than smeared to one shared value.

**Acceptance Scenarios**:

1. **Given** a subdivision mesh where a shared vertex has two incident faces with distinct facevarying UV values, **When** the mesh is subdivided and shaded, **Then** each face-corner limit-surface point uses its own face's facevarying value, not a single shared value.
2. **Given** the same mesh rendered under both REYES/stochastic and ray-tracing, **When** compared, **Then** the seam appears in both, within the project's existing visual-diff threshold (the two hiders sample the limit surface differently but must agree on the seam's presence).
3. **Given** a subdivision mesh with no facevarying data at all, **When** rendered, **Then** behavior is unchanged from today (this fix must not alter meshes that never carried per-corner facevarying data).

---

### User Story 3 - Author facevarying and crease tags currently rejected outright (Priority: P1)

A scene author uses `RiSubdivisionMesh` tags beyond the four already supported (`hole`, `crease`, `corner`, `interpolateboundary`) — specifically `facevaryinginterpolateboundary`, `facevaryingpropagatecorners`, and `creasemethod`, all standard RISpec tags for controlling facevarying boundary behavior and crease evaluation — and expects the renderer to honor them. Today any tag outside the existing four hits a hard `CODE_BADTOKEN` error, so these tags are entirely unusable.

**Why this priority**: These tags are part of the baseline RISpec conformance surface for `RiSubdivisionMesh`; a scene that relies on any of them cannot render at all today, not even with degraded quality. This is grouped with User Story 2 as P1 because both are required for genuine single-level `RiSubdivisionMesh` conformance, the foundation every later tier (creases, hierarchical edits, Loop scheme) builds on.

**Independent Test**: Render a subdivision mesh that sets each of the three new tags (independently, in separate test scenes) and confirm each is accepted and visibly changes the rendered result relative to the tag's absence (e.g. `facevaryinginterpolateboundary` changes UV behavior at mesh boundaries), rather than erroring out.

**Acceptance Scenarios**:

1. **Given** a `RiSubdivisionMesh` tag list containing `facevaryinginterpolateboundary`, `facevaryingpropagatecorners`, or `creasemethod` with a valid value, **When** parsed, **Then** the renderer accepts the tag and applies its documented RISpec behavior instead of raising `CODE_BADTOKEN`.
2. **Given** any of these three new tags with an out-of-range or unrecognized value, **When** parsed, **Then** the renderer reports a diagnostic identifying the offending tag/value rather than crashing or silently ignoring it.
3. **Given** a mesh using only the four previously-supported tags, **When** rendered after this change, **Then** its output is unchanged.

---

### User Story 4 - A reproducible crease-quality test case exists and is root-caused (Priority: P2)

A renderer maintainer investigating the two open, currently-unreproduced crease-quality reports (`DEVNOTES.md`: "Efficient subdivision surface creases," "Subdivision highly creased surface issues") needs a concrete failing test scene before committing to any fix. This story builds a heavily-creased test mesh (multiple crease levels/sharpness values converging at a shared vertex) that demonstrates the reported problem, and root-causes it before any fix is written.

**Why this priority**: Per explicit user decision, no fix should be committed sight-unseen against an unreproduced report. This tier is gated: its deliverable may be "reproduced and root-caused, fix deferred with a written rationale" rather than "fixed," and that is an acceptable outcome for this feature.

**Independent Test**: Render the new heavily-creased test scene and confirm it visibly demonstrates the reported quality/efficiency problem (e.g. a visible artifact at the multi-crease vertex, or a measurable performance cliff), establishing a reproducer that did not exist before.

**Acceptance Scenarios**:

1. **Given** a subdivision mesh with multiple crease edges of varying sharpness converging at one vertex, **When** rendered, **Then** the test scene either reproduces the reported artifact/inefficiency (documented with the specific symptom observed) or, if it does not reproduce, that negative result is documented as evidence the original report may no longer apply.
2. **Given** a reproduced artifact, **When** investigated, **Then** the root cause is documented, including whether it shares a cause with the User Story 2 facevarying bug or is independent.
3. **Given** a root-caused issue where a fix is judged safe and scoped, **When** implemented, **Then** the fix lives entirely in the geometry layer and does not alter rendering of any existing non-creased or lightly-creased mesh.

---

### User Story 5 - Hierarchical per-level tag overrides (Priority: P3)

A scene author models a base subdivision mesh and wants to override specific tags (e.g. a crease sharpness, a hole) at specific faces and subdivision levels without re-authoring the entire mesh, using the RISpec hierarchical subdivision mesh primitive. Today this primitive does not exist in the renderer at any layer — no RIB grammar token, no RenderMan Interface entry point, no implementation.

**Why this priority**: This is a substantially larger, independently valuable capability layered on top of a correct single-level mesh (User Stories 2-3); it is lower priority than baseline conformance but higher than the Loop scheme, since hierarchical edits are a documented RISpec capability gap while Loop is an alternate algorithm for content that mostly already works with Catmull-Clark.

**Independent Test**: Render a base subdivision mesh with a hierarchical edit that overrides one face's crease sharpness at a specific subdivision level, and confirm the override is visible only on the targeted face/level while the rest of the mesh subdivides as if no override existed.

**Acceptance Scenarios**:

1. **Given** a hierarchical subdivision mesh primitive with one or more per-face, per-level tag overrides, **When** parsed via RIB, **Then** the renderer accepts the primitive and stores its overrides without affecting the base mesh's default tags.
2. **Given** the same primitive, **When** rendered, **Then** only the targeted face/level shows the overridden behavior; all other faces/levels subdivide using the base mesh's tags.
3. **Given** a hierarchical subdivision mesh rendered and then written back out via RIB output, **When** the output RIB is re-parsed and re-rendered, **Then** the result matches the original render within the existing visual-diff threshold (round-trip fidelity).
4. **Given** the override-resolution logic, **When** reviewed against the codebase, **Then** it lives entirely in the geometry layer; no hider file contains any override-specific branch.

---

### User Story 6 - Loop subdivision scheme as an alternative to Catmull-Clark (Priority: P4)

A scene author sets `RiSubdivisionMesh`'s scheme argument to `"loop"` (a triangle-mesh-oriented subdivision algorithm distinct from Catmull-Clark) and expects the renderer to subdivide and render the mesh using that scheme. Today any scheme other than `"catmullclark"` is rejected outright with `CODE_INCAPABLE`.

**Why this priority**: Lowest priority because it is an additive alternate algorithm, not a conformance or correctness gap in already-exposed functionality — content authored for Catmull-Clark is entirely unaffected if Loop is never implemented, unlike the facevarying/tag/hierarchical gaps above.

**Independent Test**: Render a triangle mesh with `scheme="loop"` and confirm it subdivides and shades correctly (smooth limit surface, no crashes, no missing geometry) under both REYES and ray-tracing, without needing to match Catmull-Clark's output on the same input (the two schemes produce different limit surfaces by design).

**Acceptance Scenarios**:

1. **Given** a `RiSubdivisionMesh` with `scheme="loop"` and an all-triangle face topology, **When** rendered, **Then** the renderer accepts the scheme, subdivides the mesh, and produces a smooth limit surface instead of raising `CODE_INCAPABLE`.
2. **Given** a Loop-scheme mesh rendered under both REYES and ray-tracing, **When** compared, **Then** both hiders successfully dice/tessellate and shade the surface (mechanism parity with Catmull-Clark's existing hider integration), within the existing visual-diff threshold between the two hiders' own samplings.
3. **Given** the Loop scheme's implementation, **When** reviewed, **Then** the scheme selection and algorithm live entirely in the geometry layer, sharing the same `CObject`/`CSurface` integration seam as Catmull-Clark, with no hider-side branching on scheme.

---

### Edge Cases

- What happens when a subdivision mesh mixes triangle and non-triangle faces under `scheme="loop"`, which is topologically invalid for Loop subdivision? The renderer MUST NOT crash; it MUST report a diagnostic identifying the mesh as unsuitable for the Loop scheme rather than silently producing degenerate geometry.
- What happens to the ray-tracing hider's existing motion-blur mechanism (User Story 1) when a subdivision surface's two-time-sample eigen-basis evaluation (`CSubdivision::sample()`) is combined with camera rotation? Object-level rotation motion (authored via `MotionBegin`/`Rotate`/`MotionEnd`) is already handled identically to REYES/stochastic via the same two-time-sample position interpolation, since both hiders double and LERP the same underlying tessellated/control-point geometry. Whether camera-driven rotation blur's dedicated SLERP correction (gated by `CRenderer::cameraHasRotation`) also needs to reach the ray-tracing hider's primary-ray generation is an open verification question addressed in research.md, not a requirement this feature commits to sight-unseen.
- What happens when a hierarchical subdivision mesh (User Story 5) overrides a tag on a face at a subdivision level deeper than the mesh actually reaches for a given render (e.g. due to adaptive dicing)? The override MUST still be honored if that level is evaluated; it is not an error for a deep override to have no visible effect at a shallow effective subdivision depth.
- What happens when the new facevarying/crease tags (User Story 3) are combined with a hierarchical override (User Story 5) targeting the same face? The hierarchical override's value MUST take precedence at its targeted face/level, consistent with the RISpec hierarchical-edit model of layering overrides on top of base tags.
- What happens to CShow (debug/visualization hider) test scenes authored for this feature, given CShow is currently non-functional? They MUST be authored and checked into the test suite as deliverables, but MUST NOT be required to run or pass — this is tracked as a pre-existing, separate gap this feature does not fix.
- What happens to photon-hider test scenes authored for this feature's motion-blur capability, given the photon-mapping hider does not gain motion-blur support in this feature (User Story 1's scope is limited to REYES/stochastic and ray-tracing)? They MUST be authored to document the current gap, but MUST NOT be required to pass — the same treatment as CShow. Photon-hider test scenes for every other capability in this feature (facevarying seams, new tags, hierarchical overrides, Loop scheme) are unaffected by this exception and MUST pass like any other hider, since those capabilities need no new photon-hider mechanism.
- What happens when a hierarchical subdivision mesh override (User Story 5) references a face index that does not exist on the base mesh, or a malformed subdivision-level value? The renderer MUST reject only that invalid override — treating it as absent and emitting a diagnostic identifying the affected face/level — rather than rejecting the entire hierarchical primitive, consistent with this codebase's existing fail-small precedent for malformed per-element data.
- What happens to the pre-existing `CSubdivMesh::dice()` code duplication (it duplicates `CObject::dice()`'s loop instead of calling it)? It is explicitly out of scope for this feature — a cosmetic issue with no behavior change must not be bundled in.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The ray-tracing hider's existing, generic motion-blur mechanism (`CTesselationPatch`, `src/ri/surface.cpp`) MUST be verified, via dedicated test scenes, to support motion blur for any primitive with `moving() == true` — including subdivision surfaces — with no primitive-type-specific code path. This is a verification requirement, not new construction: the mechanism already exists and is reached purely through the `CObject`/`CSurface` virtual-dispatch contract.
- **FR-002**: Object-authored rotation motion blur under the ray-tracing hider MUST be verified to reach parity with the existing REYES/stochastic implementation — both hiders apply the same two-time-sample position interpolation to authored object motion, so parity is expected by construction. Whether the ray-tracing hider's primary-ray generation must also honor the dedicated camera-rotation SLERP correction (`CRenderer::cameraHasRotation`) is a research question (see research.md) and is out of scope for this requirement unless research finds it necessary for parity.
- **FR-003**: No artifact/rendering effect (motion blur now; depth of field or any future equivalent later) MAY be implemented with per-hider special-case treatment. Each such effect MUST be implemented once, generically, at the appropriate shared layer, so every current and future visually capable hider (REYES, raytrace, and a planned future PathTrace hider) inherits it without a hider-specific reimplementation.
- **FR-004**: `CSVertex`'s facevarying storage MUST preserve a distinct facevarying value per incident face-corner for any vertex shared by more than one face, rather than collapsing to the last-processed face's value.
- **FR-005**: The renderer MUST accept the `facevaryinginterpolateboundary`, `facevaryingpropagatecorners`, and `creasemethod` subdivision tags on `RiSubdivisionMesh`, apply their documented RISpec behavior, and report a diagnostic (not a hard `CODE_BADTOKEN` failure) for a recognized tag with an invalid value.
- **FR-006**: A concrete, reproducible test scene for the two open crease-quality reports (`DEVNOTES.md`) MUST be built and its root cause documented before any fix targeting those reports is implemented; a fix MUST NOT be committed against an unreproduced report.
- **FR-007**: The renderer MUST support the RISpec hierarchical subdivision mesh primitive (per-face, per-level tag overrides layered on a base mesh), including RIB grammar/parsing, a RenderMan Interface entry point, an implementation resolving overrides at the geometry layer, RIB-output round-trip serialization, orender-wire preview support, and scripting-language bindings (Lua, and Python if present).
- **FR-008**: Hierarchical subdivision mesh override resolution (FR-007) MUST live entirely in the geometry layer; no hider file may branch on, type-check, or downcast to any hierarchical-edit-specific type.
- **FR-009**: When a hierarchical subdivision mesh override references a face index or subdivision level absent from the base mesh, the renderer MUST reject only that invalid override — treating it as absent, with a diagnostic identifying the affected face/level — rather than rejecting the entire hierarchical subdivision mesh primitive.
- **FR-010**: The renderer MUST support `scheme="loop"` on `RiSubdivisionMesh` as a second subdivision algorithm alongside Catmull-Clark, reaching the same hider-integration depth Catmull-Clark already has (REYES dicing and the generic ray-tracing tessellation fallback), without requiring Loop to gain any capability Catmull-Clark itself lacks.
- **FR-011**: Loop-scheme selection and algorithm logic (FR-010) MUST live entirely in the geometry layer, sharing the same `CObject`/`CSurface` virtual-dispatch integration seam Catmull-Clark already uses; no hider file may branch on subdivision scheme.
- **FR-012**: No file under any hider implementation (REYES/stochastic, z-buffer, ray-tracing, photon, show) MAY gain a subdivision-specific branch, type check, or downcast as a result of any work in this feature (facevarying fix, new tags, crease fix, hierarchical edits, Loop scheme). All such logic MUST be reachable purely through the existing `CObject`/`CSurface` virtual-dispatch contract.
- **FR-013**: A verifiable regression check (e.g. an automated grep-based check with no matches expected) MUST exist confirming that no hider source file references any subdivision-specific type, validating FR-012 on an ongoing basis.
- **FR-014**: Dedicated visual-regression test scenes MUST be built, across REYES, ray-tracing, photon, and CShow hiders, for every new or fixed capability in this feature (facevarying seams, the three new tags, the crease reproducer, hierarchical overrides, the Loop scheme, and cross-hider motion blur). CShow scenes MUST be authored but are not required to pass or run, per the Edge Cases section. Photon-hider motion-blur scenes MUST likewise be authored but are not required to pass, since this feature's motion-blur scope (FR-001/FR-002) does not extend to the photon-mapping hider; photon-hider scenes for every other capability in this feature MUST pass like any other hider.
- **FR-015**: Cross-hider visual-regression comparisons introduced by this feature MUST use the project's existing block-average visual-diff metric and thresholds, not pixel-exact equality.
- **FR-016**: Test scenes verifying shading-detail correctness (facevarying seams, crease quality) MUST use the ray-tracing hider as ground truth, since only per-ray shading — not REYES-style micropolygon dicing — genuinely supersamples shading detail.
- **FR-017**: Subdivision-surface motion blur MUST be validated with dedicated test scenes on both REYES/stochastic (existing capability, currently untested) and ray-tracing (new capability, depends on FR-001/FR-002), per the standing effect-uniformity rule (FR-003). A photon-hider motion-blur test scene MUST also be authored to document the current gap, but is not required to pass (FR-014).
- **FR-018**: The project's cross-hider parity documentation (`HIDER_PARITY.md`) MUST gain a subdivision-surfaces section documenting this feature's capabilities and any known remaining gaps (e.g. a deferred crease-quality fix per User Story 4).

### Key Entities

- **Cross-Hider Motion-Blur Mechanism**: The pre-existing, generic, time-sampled motion-blur implementation in the ray-tracing hider's shared tessellation layer (`CTesselationPatch`, `src/ri/surface.cpp`), already consumed by any primitive with `moving() == true`. This feature verifies and documents it — including for subdivision surfaces — rather than building it, and its existence establishes the pattern every future artifact effect must follow.
- **Facevarying Corner Value**: A per-face-corner data value (typically UV coordinates) attached to a subdivision mesh vertex; distinct from the single per-vertex `varying`/`vertex` value, and the subject of the User Story 2 data-loss fix.
- **Subdivision Tag**: A named modifier (`hole`, `crease`, `corner`, `interpolateboundary`, and the three newly-supported `facevaryinginterpolateboundary`, `facevaryingpropagatecorners`, `creasemethod`) attached to specific faces/edges/vertices of a `RiSubdivisionMesh`, controlling boundary and crease evaluation.
- **Hierarchical Subdivision Edit**: A per-face, per-level override of one or more subdivision tags, layered on top of a base mesh's default tags, introduced by the RISpec hierarchical subdivision mesh primitive.
- **Loop Scheme**: An alternate subdivision algorithm for triangle meshes, selected via `RiSubdivisionMesh`'s scheme argument, sharing the same geometry-layer integration seam as the existing Catmull-Clark implementation.
- **Crease-Quality Reproducer**: A heavily-creased test scene built to demonstrate (or disprove) the two open, currently-unreproduced `DEVNOTES.md` crease-quality reports, and the root-cause analysis it enables.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A moving, non-subdivision primitive is confirmed, via a passing test scene, to render a comparable motion-blurred result under both REYES/stochastic and ray-tracing within the project's existing visual-diff threshold — demonstrating the pre-existing mechanism is already generic, before subdivision surfaces are layered on top of it.
- **SC-002**: A subdivision surface with authored motion is confirmed, via a passing test scene, to render correctly within the existing visual-diff threshold on both REYES/stochastic and ray-tracing, using the pre-existing cross-hider mechanism with no code changes.
- **SC-003**: A subdivision mesh with a UV seam (distinct facevarying values on a shared vertex's incident faces) renders the seam correctly rather than smeared, verified under the ray-tracing hider and cross-checked against REYES/stochastic.
- **SC-004**: Each of the three newly-supported subdivision tags is exercised by at least one passing test scene showing a visible behavioral difference from the tag's absence.
- **SC-005**: A concrete, reproducible crease-quality test scene exists, qualitatively demonstrating the reported problem (a visible artifact, or a noticeably slower render relative to a comparable lightly-creased mesh — no numeric threshold required) with a documented root cause, whether or not a fix is landed in this feature.
- **SC-006**: A hierarchical subdivision mesh with per-face, per-level overrides renders correctly, round-trips through RIB output, and matches its original render within threshold after re-parsing; a mesh with one deliberately invalid override (nonexistent face/level) renders the rest of the mesh correctly with only that override skipped and a diagnostic emitted.
- **SC-007**: A Loop-scheme mesh renders successfully (no crash, smooth limit surface) under both REYES and ray-tracing.
- **SC-008**: An automated check confirms zero hider source files reference any subdivision-specific type, before and after this feature lands.
- **SC-009**: `HIDER_PARITY.md` contains a subdivision-surfaces section after this feature lands.

## Assumptions

- The ray-tracing hider's motion-blur mechanism (User Story 1) is confirmed, by direct source read, to already reach feature parity with the existing REYES/stochastic implementation for object-authored translation and rotation (both hiders LERP the same two-time-sample geometry). This feature's User Story 1 work is limited to verification (test scenes) and documentation (`HIDER_PARITY.md`), not new construction. Whether additional work is needed for camera-driven rotation blur specifically is deferred to research.md's findings.
- No PathTrace hider exists yet in this codebase, and this feature does not build one. The forward-looking requirement is that the motion-blur mechanism and the standing effect-uniformity rule (FR-003) must not be architected in a way that would need reinvention when such a hider eventually arrives.
- The Loop scheme (User Story 6) only needs to reach the same hider-integration depth Catmull-Clark already has today (REYES dicing plus the generic ray-tracing tessellation fallback), not additional features Catmull-Clark itself is still missing (e.g. its own independent hierarchical-edit support beyond what User Story 5 already generalizes).
- CShow (debug/visualization hider) test scenes are a deliverable, not a merge gate — this feature does not fix CShow itself.
- The crease-quality tier (User Story 4) may conclude with "reproduced, root-caused, fix deferred with written rationale" as an acceptable outcome; committing a fix against an unreproduced report is explicitly disallowed.
- The only existing `SubdivisionMesh` usage in this project's examples/tests (`geometry/killeroo.rib`) uses plain untagged quads with no facevarying data and no motion; it provides no coverage for this feature's work and is supplemented, not relied upon, by the new test scenes required in FR-014.
