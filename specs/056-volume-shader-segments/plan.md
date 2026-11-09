# Implementation Plan: Volume Shader Segments

**Branch**: `056-volume-shader-segments` | **Date**: 2026-11-07 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/056-volume-shader-segments/spec.md` (after clarify, 2026-11-07)

## Summary

Today, the volume shader that `trace()` and `gather()` pick for a secondary ray is overwritten in `shade()` before
anything reads it. As a result, `Interior` and `Exterior` run only on CSG fragments, and they are picked by the side
the ray *arrives* on.

This spec moves the volume step into Shading's bundles:
- **One rule.** A new `CSegmentBundle` base class records the spawning surface and the volume shader chosen by the sign of the ray direction against the surface's `N`. Its `postShade` overrides run the shader once per ray segment, in a fresh 0-D shading state, through a new `CShadingContext::shadeVolumeSegments()`.
- **Hits and misses.** The step runs for hits (in every hit mode) and for misses. A miss segment ends at a finite limit, or at the world bound.
- **Transparency.** Classification uses the hit's own opacity; compositing uses the attenuated values. The result is exact for `fog`-like shaders.
- **Transmission.** `traceTransmission` gets the same interior/exterior split `traceReflection` has.
- **No new seams.** The hiders, `ITracer`, `CRendererServices` and the JIT emitter do not change.
- **CSG.** The `shade()` overwrite, `selectVolumeShader` and `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` are retired, so CSG solids follow the general rule.
- **`incident()`/`opposite()`.** They choose their side from vertex 0's `sign(I·N)`.

With no `Interior`/`Exterior` bound, every image is byte-identical.

## Technical Context

**Language/Version**: C++20 (`-Werror`), CMake >= 3.19; RSL test shaders compiled by `oshader` (`.rslo`) and `oshader --jit` (`.slo`)

**Primary Dependencies**: none new. LLVM (JIT) unchanged; no emitter change, no `.slo` format change

**Storage**: N/A, memory only. New test references and goldens are recorded files (approval points below)

**Testing**: ctest. Standalone shading-contract cases (`tests/shading_contract/`), rendered scenes checked with `test_pixel_probe`, `add_parity_test`, `test_golden_compare`, `add_thread_configs_test`; `-L architecture`; the determinism sweep

**Target Platform**: macOS (Clang) and Linux (Ubuntu 24.04, GCC 13), both validated

**Project Type**: renderer (C++ libraries and the `orender` CLI)

**Performance Goals**: no cost when no volume is bound (one null test per bundle call). With a volume bound, one extra 0-D shader run per segment chunk

**Constraints**:
- byte-identical output without `Interior`/`Exterior` (FR-016);
- no allowlist line (FR-017);
- no services or ABI version change;
- no threshold widened;
- no task depends on the merge (FR-021);
- builds and tests only through `cpujob`, at most `-j4`

**Scale/Scope**: about 10 source files in Shading, 1 in Graphics State, 1 in Geometry, 3 docs; 2 new contract cases; about 15 scene families (the camera-hider families on 3 hiders and 2 backends; random-fog and the atmosphere reflection probe on raytrace only); 2 rewritten CSG scenes

All clarifications are resolved (spec Clarifications 2026-11-07; research.md R1-R12). The open questions below are
maintainer choices with a default; none blocks the design.

## Constitution Check

*GATE: must pass before Phase 0 research. Re-checked after Phase 1 design: passes.*

**Principle III (TDD)**: tests come first and are seen red on the starting tree (FR-019). The phase order below puts
every new test and capture before the change. New tests and references are listed for the maintainer's approval.

**Principle VIII: Domain Separation and Modular Boundaries**:

- [x] **Domains touched are named.**
  - **Shading**: the bundles, context, builtins, light runner, `shaderPl`, test shaders.
  - **Graphics State**: deletion of `selectVolumeShader` and the flag in `src/ri/state/attributes.h`.
  - **Geometry**: deletion of one flag write in `src/ri/geometry/csgSolid.cpp:273`.
  - **Hiders**: not touched.
  - **Renderer core**: not touched. `imager.cpp:86` stays, because `CShadingState::postShader` is kept.
- [x] **No new cross-domain dependency.** Shading reads attributes and shaders only through the handles it already uses (`objectAttributes()`, `shaderInstanceOf`), and the world bound through the existing `CRendererServices::worldBmin/Bmax`. Graphics State and Geometry only lose code.
- [x] **No interface change.** No cross-domain interface is added or altered. The boundary-level changes are Shading's own (`contracts/segment-step.md`, `data-model.md`). `CRendererServices`, `ITracer` and `kRendererServicesVersion` (2.1.0) are unchanged. Two optional comment corrections are listed under Q5.
- [x] **No hider-specific special case.** The step is in the bundles, which every camera hider reaches through the same `ITracer::traceBundle`. The CSG special case is removed.
- [x] **No plug-in interface changes**: shadeop, procedural and display are untouched.
- [x] **No registered violation is extended.** DB-004 and DB-017 have lines keyed on `csgSolid.cpp` (spec 031's). They are untouched, because the deleted line names no foreign symbol. No allowlist line is added or removed.

Other principles: I (the new code is small, named functions; no magic numbers: the volume-step batch tag is a named
constant), II (C++20/C17 unchanged), IV (CLI unchanged), V (no dependency), VI (both platforms validated), VII (site
docs: the CSG page and the RISpec table are corrected per FR-020/Q8; `DEVNOTES`/`RISPEC_GAPS` per FR-020).

## Project Structure

### Documentation (this feature)

```text
specs/056-volume-shader-segments/
├── spec.md
├── plan.md              # this file
├── research.md          # Phase 0: R1-R12
├── data-model.md        # Phase 1
├── quickstart.md        # Phase 1
├── contracts/
│   ├── segment-step.md  # the volume segment step
│   └── message-side.md  # incident()/opposite() by side
├── checklists/requirements.md
└── tasks.md             # /speckit-tasks (not created here)
```

### Source Code (files this spec changes)

```text
src/libshader/shading/
├── shading.h            # CRayBundle::postShader = nullptr; CShadingState: postShader comment, volumeSegment, lightSideN;
│                        # CTraceLocation::sideN; shadeVolumeSegments() declaration
├── bundles.h / .cpp     # CSegmentSpawn, CSegmentRay, CSegmentBundle; the three bundles' postTraceAction/postShade
├── shading.cpp          # shade(): delete :1384-1400 and :1452-1456; shadeVolumeSegments(); newState() init (:1523-1527);
│                        # iterateLights (:1852) snapshot; incident/opposite (:1747-1753); gatherHeaderBegin (:4517),
│                        # gatherSample (:4376); JIT location fills (:3191, :3203, :3359)
├── executeMisc.cpp      # traceTransmission (:696) split + begin(); traceReflection (:837) gating, sideN, begin()
├── giFunctions.h        # TRANSMISSIONEXPR location fill (:68): sideN
├── shaderFunctions.h    # ENVIRONMENT (:1880) and SHADOW (:1986) location fills: sideN
└── shaderPl.h / .cpp    # CGatherVariable::globalEntry() (Q1 approved)
src/ri/state/attributes.h        # delete ATTRIBUTES_FLAGS_SOLID_FRAGMENT (:59) and selectVolumeShader (:251-263)
src/ri/geometry/csgSolid.cpp     # delete the flag write (:273)
shaders/                         # new test shaders (listed below)
examples/rib/tests/              # new scenes; the two CSG interior scenes rewritten in place
examples/rib/tests/references/   # new references; the two CSG references re-rendered (approval)
tests/visual/CMakeLists.txt      # new probe, parity, golden and determinism tests; CSG comments (:2334-2353) updated
tests/visual/golden/, golden/linux/   # atmosphere goldens, captured before the change
tests/shading_contract/          # case_volume_segments.cpp, case_message_side.cpp, test shaders, CMake registration
tests/unit/csg/                  # CSG_InteriorExteriorSelection retired (file + CMakeLists.txt:63-64); its clone/flip case carried to test_boundary_rule.cpp (approval)
tests/unit/state/test_state_isolation.cpp     # :450-458 removed (approval)
tests/unit/csg/test_procedural_operand.cpp    # :382 removed, test renamed (approval)
DEVNOTES.md, DEVNOTES_DETAILS/RISPEC_GAPS.md  # FR-020
docs/site/content/manual/reference/solid-csg-operations.md   # Q8
```

**Structure Decision**: the single existing source tree. No new directory, library, target or link edge. The new
contract cases join the existing `test_shading_contract_standalone` program (or the spec 029 per-case program
pattern, `or029_contract_program`, if they need a hand-written tracer double).

## Design decisions (details in research.md)

| # | Decision | Alternatives rejected |
|---|---|---|
| D1 | `CSegmentBundle`/`CSegmentRay` in Shading between `CRayBundle`/`CRay` and the three bundles; the bundle holds the context and the spawning surface; the hiders and `tracer.h` do not change (R1) | fields on `CRayBundle` (would leak into `CPrimaryBundle`); a hider-side step (Principle VIII); running the volume in `shade()` (misses, primitive hit modes and FR-004 all fail) |
| D2 | The step runs in a fresh 0-D state from the free list, at the segment's depth; globals zeroed, then `P`, `I`, `E`, `Ci`, `Oi`, `time` set; the spawning surface is the shaded object; `volumeSegment` flag; `inShadow` cleared and restored; ray keys as point keys (R2) | a null-object state (`gatherSample` dereferences `currentAttributes`); the hit's state (the shader may have rewritten `I`/`P`, and gather outputs read them) |
| D3 | Transparency is classified on the hit's own opacity and composited with the attenuated values; per-segment composition is exact for over-form shaders like `fog` (R3, R4) | classifying on the attenuated opacity (one-ulp spurious continuations); applying the volume over the whole path to each hit (counts in-scattering more than once) |
| D4 | A miss ends at `min(ray t, limit)`, else at the world-bound exit (camera space), clamped to the segment start; transmission records its destination because the hider resets `t` on continuation (R5) | far clip (overflow); always clipping to the bound (loses fog toward a distant light); a new `ITracer` member (version step) |
| D5 | Refusal is detected by `levelTraced`, set in `postTraceAction()`, which `tracer.h` guarantees is not called at the maximum depth (R5) | comparing ray depths (assumes the tracer enters the level; the canned doubles do not) |
| D6 | The side normal is the surface's global `N` for every entry point; `shadow()` inside a light uses a snapshot of `N` taken by `iterateLights`; new `CTraceLocation::sideN` at all six fill sites (R7) | `Ns` (usually faceforwarded, flips the side for points seen from inside); varying `N` in a light (area lights overwrite it) |
| D7 | `traceTransmission` split into interior/exterior bundles only when the shaders differ (R8) | an always-two-bundle split (changes summation order with no volume) |
| D8 | `shade()` loses the overwrite and the tail; `CShadingState::postShader` and its `enterRayLevel` copy are kept so `ShadingContract_RayLevel` stays as approved (R9) | removing the field (needs an approved-test edit, Q4) |
| D9 | `incident()`/`opposite()` choose the source in `incidentParameter`/`oppositeParameter` (one place serves both backends); the accessor constants stay (R10) | changing the accessor constants too (two approved-test edits for no output change) |
| D10 | `environment("raytrace")` gets the step like `trace()`: same entry point (Q2) | excluding it (a second path through `traceReflection`) |
| D11 | Gather miss: the step's colour and opacity go into `surface:Ci`/`surface:Oi` only; the `else` branch, tags and other outputs are unchanged (R6, Q1) | no step for gather misses (FR-007 narrowed) |

## Implementation phases (for /speckit-tasks; each ends with a pause for the maintainer)

*(Amended at the task stage: see "Task-stage amendments" below for the changes to this order.)*

0. **Characterize the starting tree**, before any source edit.
   - Capture the atmosphere goldens (`atmosphere-{fog,depthcue}-{reyes,zbuffer,raytrace}`, `-slo` variants) on macOS and on Linux.
   - Record the red results of the new contract cases and probe scenes.
   - Approval: the new scenes, shaders and goldens.
1. **Tests first (red).**
   - The contract cases (`ShadingContract_VolumeSegments{Interp,Jit}`, `ShadingContract_MessageSide{Interp,Jit}`).
   - The probe scenes and their parity, golden and determinism registrations.
   - All seen failing for the right reason (the step does not exist; `incident()` is fixed to Interior).
2. **The step.**
   - Types: `CSegmentSpawn`, `CSegmentRay`, `CSegmentBundle`, `begin()`, `levelTraced`.
   - `shadeVolumeSegments()`.
   - The three bundles' `postShade` overrides.
   - Gating and side tests in `traceReflection`/`gatherSample`; `traceTransmission` split.
   - `sideN` at the six fill sites, and the `lightSideN` snapshot.
   - Delete the `shade()` overwrite and tail.
   - Run `-L architecture` and the full run; non-volume images byte-identical.
3. **CSG.**
   - Delete `selectVolumeShader`, the flag and its write.
   - Approved-test edits, each shown as a diff first: `State_Isolation`, `CSG_ProceduralOperand`. Retire `CSG_InteriorExteriorSelection`, carrying its clone and flip assertions into a surviving test (diff-first).
   - Rewrite the two CSG scenes in place, re-render and present the references image by image; update the CMake comments.
4. **`incident()`/`opposite()`.**
5. **Documents, then full validation.**
   - `RISPEC_GAPS.md` volume entry (status, citations, verification date), the `DEVNOTES.md` row, the site page `docs/site/content/manual/reference/solid-csg-operations.md` ("Interior / Exterior shading", `:221-256`, which documents the retired CSG rule: Q8), and a draft `CLAUDE.md` note for the maintainer (not written by an agent).
   - `-LE determinism-sweep -j4`, `-L architecture`, the determinism sweep at `-j2`, and the Linux run (GCC 13, `-Werror`, over ssh with remote `cpujob -j2`).

## Approval points (nothing below is edited or recorded without the maintainer's approval)

**Approved tests and references that change:**

1. `csg-sphere-sphere-difference-interior-raytrace.rib` and `-inside.rib`: rewritten in place with a reflective or refractive surface (names and test IDs kept). Both references are re-rendered and each is approved image by image (FR-012). The comments at `tests/visual/CMakeLists.txt:2334-2353` are rewritten to the new rule. The `-inside` determinism-sweep line (`determinism-scenes.cmake:197`) keys on the name and stays.
2. `CSG_InteriorExteriorSelection` retired: the file and `tests/unit/csg/CMakeLists.txt:63-64`. Approved at clarify. Its last case, `instance_leaves_carry_the_fragment_flag_and_the_flip` (`test_interior_exterior_selection.cpp:119-147`), also asserts that each resolved leaf is a clone of its template and that only the subtrahend's `ATTRIBUTES_FLAGS_INSIDE` is toggled. Those assertions do not name the retired symbols and FR-011/A9 rely on them, so they are carried (flag line and `selectVolumeShader` lines dropped) into a surviving CSG unit test (proposed: `test_boundary_rule.cpp`, `CSG_BoundaryRule`, which already uses `CCsgSession` and tests `operandFlip`). That is an approved-test change: it is shown as a diff first (tasks T089, T091).
3. **New:** `State_Isolation`: the `selectVolumeShader` checks at `tests/unit/state/test_state_isolation.cpp:450-458` are removed (the function no longer exists). Shown as a diff first.
4. **New:** `CSG_ProceduralOperand`: `tests/unit/csg/test_procedural_operand.cpp:382` asserts the retired flag. The assertion is removed, the `INSIDE` parity check is kept, and the case is renamed from `expanded_objects_carry_the_fragment_flag_and_inside_parity` to `expanded_objects_carry_the_inside_parity`. Shown as a diff first.
5. FR-022, `tests/shading_contract/case_messages.cpp:135-136` and `check_message_accessors.cmake`: **verified, no edit needed** (`contracts/message-side.md`). If the implementation finds otherwise, the diff is shown before any edit.
6. `ShadingContract_RayLevel` (`case_ray_level.cpp:121, 132`): no edit under D8. An edit only if Q4 is answered "remove the field".

**New tests, shaders and references (approved at plan level 2026-11-07; each still shown at the implement TEST-APPROVAL pause):**

- Contract cases:
  - `ShadingContract_VolumeSegments{Interp,Jit}`: a canned tracer that enters the ray level; probe volume `cv_segment.sl`, which encodes `I` (and in a second pass `P`) in `Ci`; covers hit, miss to the bound (more misses than `maxGridSize()` in one call), gather `maxdist`, transmission destination, continued segment, refusal, the volume shader's own `trace()`, the probes, and the side split with different `Interior`/`Exterior` for `trace()`, `transmission()` and `gather()` (the `giFunctions.h:68` and `shading.cpp:3359` location fills). The canned tracer never calls `shade()`, so FR-005 (primitive hit mode) is covered by the rendered `volume-primitive-hitmode` scene, not here.
  - `ShadingContract_MessageSide{Interp,Jit}`.
- Test shaders in `shaders/`:
  - `volume_through_probe` (surface: `Ci = trace(P, normalize(I))`, opaque);
  - `volume_reflect_probe` (surface: one reflected `trace()`);
  - `volume_transmission_probe` (surface: `transmission()` toward a fixed point);
  - `volume_gather_probe` (gather of `surface:Ci` plus `ray:direction`);
  - `volume_shadow_light` (a light with a ray-traced `shadow()`, or the shipped `shadowpoint`, if it suffices);
  - `incident_opposite_probe` (surface encoding both answers);
  - `volume_random_fog` (volume, `fog` with a `random()` term).
  - Scenes and shaders added at the analyze stage are listed under "Analyze-stage amendments" below.
- Scenes in `examples/rib/tests/`. Every probed region has a constant segment length (orthographic camera, slabs or parallel planes, axis-aligned polygon bounds, a non-identity camera transform: research R11). Unless noted raytrace-only (`volume-random-fog`, `atmosphere-fog-reflection`), each has a reyes, zbuffer and raytrace variant, `.rslo` and `.slo`, and a float output for `test_pixel_probe`; only the glass sphere and the two CSG scenes get 8-bit references (Task-stage amendments); the glass sphere is `.rslo` only for tests until GitHub #157 (glass under the JIT) is fixed. No-volume twins are named `<family>-none-<hider>[-slo]`:
  - `volume-interior-fog-slab` (constant segment length; see research R11), plus the `volume-interior-fog-slab-none-<hider>` twin with no `Interior`, which proves AS1.2;
  - `volume-interior-fog-glass-sphere`: FR-019's literal scene, a non-CSG `glass` sphere with `Interior "fog"`. Checked against its reference image and by the parity pairs, not by a pixel probe: the chord varies across each pixel, so there is no closed-form value;
  - `volume-exterior-fog-reflection` (hit and miss pixels);
  - `volume-transmission-interior`;
  - `volume-gather`;
  - `volume-miss-clipping` (far clip `1e30`);
  - `volume-primitive-hitmode` (the hit object in `specularhitmode "primitive"`);
  - `volume-incident-opposite`;
  - `volume-shadow-light` and `volume-environment-raytrace`, each with **different** `Interior` and `Exterior`, so every `sideN` fill site is exercised in both backends;
  - `volume-random-fog-raytrace` (determinism);
  - `atmosphere-fog`, `atmosphere-depthcue` (goldens per platform);
  - `atmosphere-fog-reflection-raytrace`;
  - `volume-csg-fragment` (reyes, zbuffer, raytrace; `.rslo` and `.slo`), added at the task stage;
  - 8-bit atmosphere twins (analyze stage, see below).
- Registrations: `VolumeProbe_*` (pixel probes, label `volume`), `Parity_volume-*` (default threshold 20), `Golden_atmosphere-*`, `Parity_atmosphere-*` (8-bit twins, default threshold 20), `Determinism_volume-random-fog` (`add_thread_configs_test`).

## Overlap with the concurrent specs (030, 031, 032)

None of the three is a dependency of this spec, and this spec is a dependency of none of them. Every shared file is
listed below with how a collision is avoided. The order of rebasing is the maintainer's.

| Spec | Shared item | What each does | How collision is avoided |
|---|---|---|---|
| 031 | `src/ri/geometry/csgSolid.cpp`, `CCSGSolid::build` around `:273` | 031 rewrites the dice and ray-side entry points (DB-006, DB-017); 056 deletes the one line `clone->flags \|= ATTRIBUTES_FLAGS_SOLID_FRAGMENT;` | Possibly the same hunk. This spec's change is a one-line deletion with no semantic link to 031's edits, so whichever spec rebases second reapplies it trivially. Neither spec's tests assert the other's change. |
| 031 | `src/libshader/shading/shading.h`, `shading.cpp` | 031 retires `shade(CSurface *, ...)`, `displace`, `displaceEstimate` and the producer hook (031 FR-006); 056 edits `CRayBundle`, `CShadingState`, `CTraceLocation`, `shade(const ShaderGlobals &, ...)`'s body, `enterRayLevel`'s neighbours, `iterateLights`, the message members, the JIT location fills and `gatherSample` | Different declarations and functions. Textual conflicts, if any, are adjacent-line only. 056 adds nothing to the Geometry-facing entry points 031 removes. |
| 031 | `src/ri/hiders/trace.cpp` | 031 moves it to Geometry's ray-side interface | 056 does not edit `trace.cpp`; the bundle protocol it relies on is the one `tracer.h` documents |
| 032 | `src/libshader/shading/giFunctions.h` | 032 replaces the irradiance cache call at `:235`; 056 adds `sideN` to the location fill at `:68` | Different macros |
| 032 | `src/libshader/shading/shading.cpp` | 032 edits the JIT cache call at `:3531`; 056 edits `:3191`, `:3203`, `:3359` and other functions | Different functions |
| 032 | `rendererServices.h`, `kRendererServicesVersion` | 032 may step the version (032 FR-009) | 056 steps nothing. Its only possible edit there is the optional comment correction of Q5, which is wording only; if 032 lands first it is a one-line rebase. |
| 032 | `ITracer` and the Shading-owned sampler interface (032 FR-008, FR-027) | 032 may add an interface the `CHider` base installs | 056 adds no `ITracer` member and no interface |
| 030 | `DEVNOTES.md`, `CLAUDE.md` | 030 FR-034 edits the hider paragraphs, Active Technologies and Recent Changes at its end; 056 edits the RISpec-gaps row of `DEVNOTES.md` and proposes a `CLAUDE.md` draft | Different rows and entries. The Recent Changes and Active Technologies list entries are separate lines. |
| 030 | the `classic` integrator | 030 registers `classic` with a pass-through lifecycle; the transport call sites (`trace`, `gather`, `illuminance`) stay where they are (030 Clarifications) | 056 defines classic's volume behaviour inside those call sites; it holds with or without 030 (A1) |
| all | `tests/architecture/allowlist.txt`, `DOMAIN_BOUNDARIES.md` | 030, 031 and 032 delete lines and move entries | 056 adds and deletes no allowlist line and moves no registry entry |
| all | `tests/visual/CMakeLists.txt` | each adds registrations | 056's additions are a new block, and the CSG comment edit is in its own block (`:2334-2353`) |

## Maintainer answers (2026-11-07, plan approved)

The maintainer approved the plan with every default. The spec was corrected to match (spec.md, Clarifications,
"Plan-stage corrections (2026-11-07)": FR-001, FR-007, FR-012, FR-020, FR-022).

- **Q1**: yes. A gather miss runs the step and writes into `surface:Ci`/`surface:Oi` only (D11; `CGatherVariable::globalEntry()` is in scope).
- **Q2**: yes. `environment("raytrace")` gets the step like `trace()` (D10).
- **Q3**: accepted, and documented in `RISPEC_GAPS.md`. The side comes from the global `N` at the call; `shadow()` from a light uses the `N` snapshot taken in `iterateLights` (D6).
- **Q4**: keep `CShadingState::postShader`, so `ShadingContract_RayLevel` needs no edit (D8).
- **Q5**: both comment-only fixes are made in this spec, with no version step. The `rendererServices.h:312` fix is made only after the non-identity-camera probe confirms that the bound is in camera space. The `tracer.h` "Re-enters Shading" note is the other.
- **Q6**: approved to file both pre-existing behaviours as P1 Bug Tracker issues: the continued transmission ray losing its destination, and `traceReflection`'s default label. The coordinator files them. They are **out of this spec**: no task here touches them.
- **Q7**: yes. US3 tests the hand-computed grey attenuation; no new coloured-opacity shader.
- **Q8**: yes. The site page section `docs/site/content/manual/reference/solid-csg-operations.md:221-256` is rewritten at the implement stage and shown to the maintainer first.
- **Approved-test changes**: `State_Isolation` (`test_state_isolation.cpp:450-458`) and `CSG_ProceduralOperand` (`test_procedural_operand.cpp:382`) may change. Each is shown as a diff before it is applied.
- **New tests**: the list above is approved at plan level. Each test is still shown at the implement stage's TEST-APPROVAL pause before it is recorded.

## Open questions for the maintainer (answered 2026-11-07, see "Maintainer answers" above; kept for the record)

- **Q1 (gather misses, FR-007)**: a gather miss runs the volume step and records the result into `surface:Ci`/`surface:Oi` for the missed rays. The miss still runs the `else` branch, which then sees the fogged values. *Default: yes.* The alternative is no step for gather misses (FR-007 narrowed to `trace()`); it is simpler, and drops `CGatherVariable::globalEntry()`.
- **Q2 (`environment("raytrace")`)**: it gets the step, as an instance of `trace()` (the same `traceReflection` entry). *Default: yes.*
- **Q3 (side normal)**: the side is decided by the surface's global `N` at the call: as the shader holds it for `trace()`, `transmission()` and `gather()`, and as snapshotted when the lights ran for `shadow()`. A shader that rewrites `N` (for example `N = faceforward(N, I)`) therefore flips the side, as the existing split already does. *Default: accept, and document it in `RISPEC_GAPS.md`.* Alternatives for `shadow()`: `Ns`, the illuminance normal (usually faceforwarded), or the varying `N` (overwritten by area lights). Both are rejected in research R7.
- **Q4 (`CShadingState::postShader`)**: keep the field and its `enterRayLevel` copy, read by nothing, so `ShadingContract_RayLevel` stays as approved. *Default: keep.* The alternative is to remove it and edit `case_ray_level.cpp:121, 132`, shown as a diff.
- **Q5 (two comment corrections in the versioned headers, wording only, no version step, following the spec 038 precedent for `getDSO`)**: *Default: make both here.* Either can instead be left to whichever of 032 or 056 merges second.
  - `rendererServices.h:312` says the world bound is in world space. It is in camera space. Made only once the miss probe with a non-identity camera confirms it.
  - `tracer.h`'s "Re-enters Shading" paragraph could say that `postShade` may run the segment's volume shader on `context`.
- **Q6 (two pre-existing behaviours found while planning, not changed here)**: should Bug Tracker issues be filed? *Default: file both, outside this spec.*
  - A continued transmission ray loses its destination (`trace.cpp:239`), so after a transparent hit a shadow ray can hit geometry beyond its light.
  - `traceReflection`'s default ray label is `rayLabelTransmission` (`executeMisc.cpp:855-856`).
- **Q8 (site page, beyond FR-020's list)**: `docs/site/content/manual/reference/solid-csg-operations.md:221-256` describes the retired arrival-side CSG rule and the two old scenes. *Default: rewrite that section at the implement stage, alongside FR-020's documents, shown to the maintainer.*
- **Q7 (US3 expectation)**: `fog` mixes `Oi` toward white, so a transmission through an interior fog is attenuated with a grey value, not a tint. *Default: test the hand-computed grey attenuation; no new coloured-opacity volume shader.*

## Task-stage amendments (2026-11-09, tasks.md approved)

`/speckit-tasks` found the following gaps between this plan and the code. The maintainer approved every change (tasks.md, "Maintainer answers"). They change the design as follows; nothing else in this plan moves.

- **Phase order.** The deletion of the `shade()` overwrite and tail (`shading.cpp:1384-1400`, `:1452-1456`) moves from Phase 2 to Phase 3, beside `selectVolumeShader`. It is the only change that alters existing images, and doing it in Phase 2 would leave the two approved CSG interior scenes red at Checkpoint 2. Phase 5 is split: documents are Phase 5, final validation is Phase 6. The atmosphere goldens are captured in Phase 1 after the TEST-APPROVAL (the `-none` twins need the new shaders), still on the unchanged starting tree on both hosts.
- **Test shaders in `shaders/` touch two approved manifests.** `ShaderCompilerImmutability` needs `tests/shaders/references/<name>.rslo` for each new shader, and `Shaders_BytesUnchanged` hashes every shipped `.rslo`/`.slo` against `src/libshader/tests/reference/{macos,linux}/shaders.sha256`. Both gain added lines only, behind the Phase 1 TEST-APPROVAL. Contract-only shaders (`cv_*.sl`) live in `tests/shading_contract/shaders/` and touch neither.
- **Contract harness.** `ShadingContract_VolumeSegments` runs in the rendered program (the standalone harness has no shaded object, `contract_harness.h:99`); its level-entering tracer is a new class in the new case file (`recording_tracer.h` and `contract_harness.h` are not edited). `ShadingContract_MessageSide` overrides `VARIABLE_I` locally.
- **Parity.** `test_hider_parity` reads 8-bit RGBA only, so `Parity_volume-*` covers the 8-bit glass sphere only; the float probe scenes prove agreement by hitting the same hand-computed value on every hider and backend. `VolumeDiffer_glass-sphere-*` (the scene differs from its no-`Interior` twin) gives the glass sphere its red-to-green test, and its `Visual_` reference is made after the step behind a PAUSE. Only the glass sphere and the two CSG scenes get 8-bit references.
- **Scenes and registrations.** The scenes are generated by a checked-in `tools/gen_scenes.py`; registrations live in a new `tests/visual/volume-scenes.cmake` (one `include()` line in `tests/visual/CMakeLists.txt`); the new scenes are appended to `tests/visual/determinism-scenes.cmake` and the figure of 419 scenes changes in the `CLAUDE.md` draft. `Determinism_volume-random-fog` is paired with `VolumeProbe_random-fog-runs` so a green result means the fog ran. Probe tolerances: 1e-4 absolute where the value contains `exp()`, 1e-5 for pass-through colours; goldens and `-none` twins bit for bit per platform.
- **Documents.** The site-page pause also covers `docs/site/content/references/renderman-rispec.md:30` (same false claim, outside FR-020); `NEWS.md`/`ChangeLog.md` lines are added with the `DEVNOTES.md` approval; the `DEVNOTES.md` RISpec-gaps counts change. The `attributes.h:59` comment is corrected by deleting the flag. The `tracer.h` note to edit is the `traceBundle` paragraph (`:111-112`), not `traceRay`'s (`:70`).
- **Camera-space probe.** The `rendererServices.h:312` edit is conditional on a recorded probe result (`volume-miss-clipping`, tasks T074/T075).

## Analyze-stage amendments (2026-11-09, analyze report M1-M8 and L1-L13 approved)

`/speckit-analyze` found no CRITICAL or HIGH issue. The maintainer approved every recommended fix; they change the design and the tasks as follows, and nothing else in this plan moves.

- **M1, endpoints in the context.** `shadeVolumeSegments()` takes per-ray `segStart`/`segEnd` (`segEnd = C_INFINITY` means "to the world-bound exit") and derives `P`, `I` and the exit itself. A bundle never calls `rendererWorldBmin()/Bmax()`, which are `protected` in the `final` context (`shading.h:1038-1052`) and stay so (data-model.md, research R5).
- **M2, M3 (manifests).** The recapture runs with `-DRECAPTURE=ON` on both hosts (`capture-differences` otherwise keeps an existing Linux file); `oshader_help.txt` must stay unchanged. The `src/` gate of Phase 1 excludes `src/libshader/tests/reference`, which T044/T052 edit by design.
- **M4.** The CSG candidate references are rendered at T100, after T094 and T098, not at T086.
- **M5.** A compositing case in the volume-segments contract: two canned hits (`Oi < 1`, then opaque) and an over-form volume (`cv_overfog`) check the R4 identity numerically.
- **M6.** 8-bit atmosphere twins (`atmosphere-{fog,depthcue}-8bit-<hider>`) carry `Parity_atmosphere-*` (raytrace against reyes and zbuffer, default threshold 20), characterizing US6 AS1 on the starting tree.
- **M7.** The clone and flip assertions of the retired test's last case are carried into a surviving test as a diff-first approval (Approval points, item 2).
- **M8.** `volume-csg-fragment` is generated for reyes, zbuffer and raytrace, in `.rslo` and `.slo` (FR-014).
- **Lows.** Twin naming is `<family>-none-<hider>[-slo]`; A7 (matte spawning surface) and A8 (`occlusion()`, `indirectdiffuse()` get no step) get contract cases; ambient-light run sites are documented in research R7; T094/T095 name the pauses they wait for; Phase 1 gains a Linux full run and `-L architecture`; SC-004 is reworded; counts and line numbers are corrected.

## Complexity Tracking

No constitution violation. Nothing to justify.

## Notes for the next stage

- The project rules followed are the `## Spec-kit agent rules` of `CLAUDE.md`. The worktree copy and the main-checkout copy are identical: both last changed in commit `14d8ce2d`, with no uncommitted change.
- The optional `before_plan`/`after_plan` auto-commit hooks were declined: no commits.
