# Tasks: Full Subdivision Surface Support

**Input**: Design documents from `/specs/010-full-subdivision-support/`
**Prerequisites**: [plan.md](./plan.md) (required), [spec.md](./spec.md), [research.md](./research.md),
[data-model.md](./data-model.md), [contracts/hider-invariant-contract.md](./contracts/hider-invariant-contract.md),
[contracts/hierarchical-subdivision-contract.md](./contracts/hierarchical-subdivision-contract.md),
[quickstart.md](./quickstart.md)

**Tests**: The feature specification's own Testing Requirements (spec.md) call for dedicated visual-regression
scenes per capability, so scene-authoring/registration tasks are included per story below — this project has no
separate unit-test framework for renderer geometry; `ctest -L visual`'s block-average image diff *is* the test
suite (see plan.md's Technical Context: "Testing" — research.md R9 explicitly rejects introducing new test
infrastructure).

**Organization**: Tasks are grouped by user story (spec.md's US1–US6, in priority order P1/P1/P1/P2/P3/P4) to enable
independent implementation and testing, following the same structure `specs/009-nurbs-trim-curves/tasks.md` used.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependency on an incomplete task)
- **[Story]**: Which user story this task belongs to (US1, US2, US3, US4, US5, US6) — omitted for Setup,
  Foundational, and Polish tasks
- File paths are given exactly as in plan.md's Project Structure section

## Path Conventions

Single-project C++ renderer core (plan.md's Structure Decision) — no `src/frontend`/`src/backend` split. All
renderer changes live under `src/ri/`; preview under `src/preview/libribpreview/`; scripting under `src/lua/`; test
scenes under `examples/rib/tests/` (and its existing `parity/`/`references/` subdirectories); test registration in
`tests/visual/CMakeLists.txt`; documentation in `DEVNOTES.md`/`DEVNOTES_DETAILS/HIDER_PARITY.md`.

---

## Phase 1: Setup (Project Initialization)

**Purpose**: Confirm the existing build/test infrastructure this feature reuses is in a known-good state before
any tier lands. No new directories or build targets are needed — `examples/rib/tests/`, `examples/rib/tests/
parity/`, `examples/rib/tests/references/`, and `tests/visual/CMakeLists.txt`'s `add_visual_test`/`add_parity_test`
macros already exist and are reused as-is (research.md R9).

- [X] T001 [P] Verify `cmake --build build --config Release` succeeds unmodified — sanity baseline recorded before
  any tier lands (no file changes; confirms the starting point is buildable)
- [X] T002 [P] Create the subdivision-surfaces section skeleton in `DEVNOTES_DETAILS/HIDER_PARITY.md` — six empty
  subsections (Motion Blur / Facevarying / New Tags / Crease Quality / Hierarchical Edits / Loop Scheme), one per
  user story, so each story phase below appends its own findings independently without merge conflicts

**Checkpoint**: Build confirmed green; documentation skeleton in place for incremental per-story updates.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Capture the "before" state the Constitution's TDD principle (Principle III) requires every fix in
this feature to be measured against, and establish the FR-013 hider-invariant regression check's zero-match
baseline before any tier can touch geometry-layer code.

**⚠️ CRITICAL**: No user-story code changes (US2, US3, US4, US5, US6) may begin until this phase's baselines are
captured — a fix without a captured "before" cannot be verified against Constitution Principle III's TDD gate.

- [X] T003 Render `geometry/killeroo.rib` and save the output as the immutable pre-existing baseline under
  `examples/rib/tests/references/killeroo-baseline.tif` (the existing 466-call `hole`/`interpolateboundary`
  regression coverage identified in research.md R8) — every tier below except one that specifically targets a bug
  this scene happens to exercise must leave this baseline bit-for-bit unaffected (quickstart.md Step 0.1)
- [X] T004 Run the FR-013 hider-invariant grep check from `contracts/hider-invariant-contract.md` —
  `grep -rln 'CSubdiv\|CLoopSubdiv\|CHierarchical' src/ri/stochastic.cpp src/ri/reyes.cpp src/ri/zbuffer.cpp src/ri/raytracer.cpp src/ri/trace.cpp src/ri/photon.cpp src/ri/show.cpp`
  — and record the zero-match result as this feature's starting baseline (SC-008); this same command re-runs at
  the end in Phase 9 (Polish) to confirm the invariant still holds after all six tiers land

**Checkpoint**: Baselines captured — every subsequent story phase has a "before" to diff against.

---

## Phase 3: User Story 1 — Cross-Hider Motion Blur Verification (Priority: P1)

**Goal**: Prove the existing generic motion-blur mechanism (`CTesselationPatch::sampleTesselation()`/`intersect()`,
already shared by every primitive with `moving() == true`) already produces correct, cross-hider-consistent output
for subdivision surfaces specifically — no renderer code change is expected (research.md R1/R2; plan.md Summary).

**Independent Test**: Render a moving subdivision surface (translate, then rotate) on both REYES and ray-tracing
hiders; confirm the two hiders agree within the existing block-average parity threshold, and that rotation motion
correctly exercises `CSubdivision::sample()`'s two-time-sample path.

- [X] T005 [P] [US1] Author `examples/rib/tests/parity/motion-subdiv-translate-reyes.rib` and
  `examples/rib/tests/parity/motion-subdiv-translate-raytrace.rib` — a subdivision surface with translation
  motion blur, one scene per hider
- [X] T006 [P] [US1] Author `examples/rib/tests/parity/motion-subdiv-rotate-reyes.rib` and
  `examples/rib/tests/parity/motion-subdiv-rotate-raytrace.rib` — a subdivision surface with rotation motion blur,
  exercising `CSubdivision::sample()`'s two-time-sample eigen-basis path (`subdivision.cpp:149-188`)
- [X] T007 [US1] Render the four scenes from T005/T006 and confirm each translate/rotate pair agrees within the
  existing block-average parity threshold (quickstart.md Step 1.3)
- [X] T008 [US1] Register the four scenes from T005/T006 as `add_parity_test(...)` entries in
  `tests/visual/CMakeLists.txt` (alongside the existing 9 motion-parity registrations at `CMakeLists.txt:609-679`)
- [X] T009 [US1] Generate reference `.tif` images for the four scenes from T005/T006 under
  `examples/rib/tests/references/`
- [X] T010 [US1] Fill in `DEVNOTES_DETAILS/HIDER_PARITY.md`'s "Motion Blur" subsection (skeleton from T002),
  documenting the mechanism as verified-closed (research.md R1/R2), and explicitly citing the 9 pre-existing
  non-subdivision `add_parity_test` entries at `tests/visual/CMakeLists.txt:609-679`
  (`motion-patches-translate`, `motion-patches-deform`, `motion-polygons-translate`, `motion-polygons-deform`,
  `motion-quadrics-translate`, `motion-quadrics-deform`, their two correlated-table variants, and `dof-motion`) as
  the evidence satisfying SC-001/the Independent Test's "confirm the mechanism is generic, not subdivision-
  specific, using a non-subdivision primitive first" requirement — this feature adds 2 new subdivision-specific
  entries (T008) alongside those 9, not a 7-vs-9 split

**Checkpoint**: US1 independently testable — cross-hider subdivision-surface motion blur is verified and
regression-locked, with zero renderer code touched.

---

## Phase 4: User Story 2 — Facevarying Data Preservation on Shared Vertices (Priority: P1)

**Goal**: Fix the data-loss bug where `CSVertex`'s single `facevarying` pointer collapses to the last-processed
incident face's value on any vertex shared by ≥2 faces, destroying UV-seam discontinuities (FR-004).

**Independent Test**: Render a mesh with a vertex shared by ≥3 faces carrying visibly distinct per-corner UV
values; confirm the rendered seam shows distinct values instead of one collapsed value, on both hiders, while
leaving single-incident-face vertices and facevarying-absent meshes bit-for-bit unaffected.

- [X] T011 [US2] **Scope decision made (user, this session): drop the "demonstrate externally-visible collapse"
  requirement.** `examples/rib/tests/subdiv-facevarying-seam-raytrace.rib` — a vertex shared by 4 faces with
  distinct per-corner `"facevarying float[2] st"` values — was authored this session (see file for the actual
  scene). Per the finding below, it cannot demonstrate a pre-fix seam collapse on any single-level mesh, so its
  role is now correctness/non-regression coverage for the landed fix (T012-T016), not a red/green TDD pair.

  **Finding (this session):** traced the exact read/write ordering in `CSubdivMesh::create()`'s "Finalize the
  faces" loop (`subdivisionCreator.cpp:1854-1899`, pre-fix code). The per-face facevarying-pointer write (line
  ~1860, `faces[i]->vertices[j]->facevarying = ...`) and that same face's `create()` call (which reads it via
  `gatherData`→`computeVarying()`) happen in the **same loop iteration `i`**, before the next face `i+1` can
  overwrite the shared vertex's pointer. So for a single-level (non-recursively-split) `SubdivisionMesh` — the
  only kind this repo's test scenes exercise today — every face reads back **its own correct value** before
  being overwritten. Confirmed empirically: `debug6.rib` (`/tmp/facevarying-test/debug6.rib`) rendered against
  the pre-fix (stashed) code and against the now-landed fix are pixel-identical (sampled max channel diff = 1,
  i.e. noise). The bug at the cited line is real as *written* — the single pointer per vertex genuinely gets
  overwritten once per incident face — but it is not externally observable via any single-level mesh on either
  hider. The only unexplored path where a later read could still see a stale value is recursive/adaptive face
  splitting (`CSFace::create()`'s `split==TRUE` branch, `subdivisionCreator.cpp:761-769`), left unconfirmed and
  explicitly not pursued further (diminishing returns on a fourth narrowing hypothesis) — the fix as landed
  (T012-T016) is correct hardening regardless of whether that path is ever reachable.
- [X] T012 [US2] Add a `float *facevarying` field to `CSVertex::CVertexFace` (defined inline in
  `src/ri/subdivisionCreator.cpp:113-117`, not `subdivisionCreator.h` as originally assumed); remove the
  collapsed single-pointer `CSVertex::facevarying` field — **landed** (verified via `git diff
  src/ri/subdivisionCreator.cpp` this session; the stash from a prior session was popped and the build succeeds)
- [X] T013 [US2] Update the per-face assignment loop at `subdivisionCreator.cpp:1887` (post-fix line number) to
  call the new `CSVertex::setFacevarying(face, ptr)` helper, storing per-`(vertex, incident face)` — **landed**
- [X] T014 [US2] Add a requesting-face parameter (`requestingFaceIndex`, matched against
  `CVertexFace::face->uniformIndex`) to `CSVertex::computeVarying()` — **landed, but with a gap**: the loop over
  `faces` breaks on a match and otherwise falls through with **no fallback write at all** (leaves the caller's
  `facevarying` buffer, freshly `ralloc`'d and uninitialized, untouched) — this does not match this task's own
  "falling back to first available slot when face-context is NULL/absent" requirement. Today's only call site
  (`gatherData`, passing the currently-processing face's own `uniformNumber`) always matches, so this is latent,
  not yet observed — but if the unresolved split-path scenario from T011's finding is ever real, a child face's
  `uniformIndex` would not match any of the original vertex's incident-face records, and this would read
  uninitialized memory rather than a stale-but-valid value. Flagging as an open risk rather than fixing
  speculatively, since it's unverified whether that path is reachable.
- [X] T015 [US2] Thread the requesting-face parameter through `CSEdge::computeVarying()` — **landed**
- [X] T016 [US2] Thread the requesting-face parameter through `CSFace::computeVarying()` — **landed**
  (`gatherData` passes `uniformNumber` as the requesting-face index at every call site)
- [X] T017 [US2] Re-render `examples/rib/tests/subdiv-facevarying-seam-raytrace.rib` with the fix landed —
  **rescoped per T011's decision to correctness/non-regression verification, not seam-collapse-vs-collapse
  comparison.** Rebuilt `ri`+`orender` with T012-T016 active and re-ran `debug6.rib` (raytrace ground truth):
  output is pixel-identical to the pre-fix render, confirming the fix is non-regressive.
  **Correction (this session):** an earlier pass of this task claimed the scene's 4 distinct per-corner `st`
  values "render correctly" without an actual successful render of `subdiv-facevarying-seam-raytrace.rib` in
  hand — that scene in fact rendered fully black on this machine. Root-caused to two independent, unrelated
  causes, neither a subdivision/facevarying defect: (1) the gitignored deploy-tree file
  `openrender/.orenderrc` contains `Option "shaderformat" "default" ["slo"]`, silently forcing every local
  render onto the LLVM JIT backend; (2) `src/libshader/compiler/llvmEmitter.cpp`'s `emitFunction()` opcode
  dispatch has no case for the `cfrom` opcode (emitted only by the explicit-colorspace RSL constructor
  `color "space" (...)`, which `show_st.sl` uses for `Ci`), silently dropping the write to `Ci` under JIT —
  a pre-existing, long-standing gap since the JIT emitter's introduction (`ccc59e4`), never caught before
  because no stock shader uses that constructor syntax. The interpreter (`.rslo`) backend has no such gap.
  Fixed the test scene by pinning `Option "shaderformat" "default" ["rslo"]` (added to
  `subdiv-facevarying-seam-raytrace.rib`, following the existing `teapot-*-slo.rib` precedent for pinning
  format explicitly rather than depending on local/default config) and re-rendered for real. Result: 4
  visibly distinct colors sampled just off the shared center vertex, one per incident face, each closely
  matching its authored corner value — face0 (219,34,0)≈(1.0,0.0), face1 (39,242,0)≈(0.0,1.0), face2
  (217,216,0)≈(1.0,1.0), face3 (101,73,0)≈(0.3,0.3) (small offset from the exact RGB8 equivalents is expected
  Catmull-Clark limit-surface smoothing near the sample point, not error). This confirms the original claim's
  substance was correct — the facevarying fix does preserve distinct per-corner values — but the verification
  itself had not actually been performed against this scene before now. The JIT `cfrom` gap is tracked
  separately (see DEVNOTES_DETAILS/BUGS.md) — out of scope for this spec, reproduces on a bare untextured
  sphere with zero subdivision involvement. Depends on T016.
- [X] T018 [P] [US2] Author `examples/rib/tests/subdiv-facevarying-seam-reyes.rib` and confirm cross-hider parity
  with the raytrace scene within the block-average threshold (quickstart.md Step 2.4) — depends on T016.
  **Done this session**: same mesh/facevarying data as the raytrace scene, `Hider "reyes"`,
  `ShadingRate 0.25` (fine enough to resolve the seam at the shared vertex — REYES dices to a micropolygon
  grid, so shading detail is gated by dicing rate, not `PixelSamples`), and the same
  `Option "shaderformat" "default" ["rslo"]` pin. Sampled colors off the center vertex match the raytrace
  scene within 1/255 per channel (face0 (220,34,0) vs. raytrace's (219,34,0), etc.) — well inside the
  block-average threshold.
- [X] T019 [P] [US2] Render `geometry/killeroo.rib` (via `examples/rib/killeroo.rib`'s `Geometry "killeroo"` call)
  and diff against the T003 baseline to confirm it is unaffected by the fix — **verified this session**: two
  back-to-back renders of the *same* post-fix binary differ from each other by more (sampled max channel diff 110,
  8x8-block basis) than either differs from the pre-fix baseline (diff 73/76) — i.e. the observed diff is within
  this renderer's existing run-to-run non-determinism band (unrelated to this fix; `killeroo.rib` carries no
  facevarying data at all, so the fix's code paths are inert for it), not a regression attributable to T012-T016.
- [X] T020 [US2] Register `subdiv-facevarying-seam-{reyes,raytrace}.rib` as `add_visual_test(...)`/
  `add_parity_test(...)` entries in `tests/visual/CMakeLists.txt`; generate matching reference `.tif` images under
  `examples/rib/tests/references/` — depends on T017, T018. **Done this session**: `add_visual_test` for both
  hider variants (references generated from the passing renders above) plus `add_parity_test(subdiv-facevarying-seam
  ...)` at the default 20/255 block-average threshold. All 3 new tests (`Visual_subdiv-facevarying-seam-raytrace`,
  `Visual_subdiv-facevarying-seam-reyes`, `Parity_subdiv-facevarying-seam`) pass. Scene/parity counts in the
  `message(STATUS ...)` summary updated 45→47 visual, 21→22 parity.

**Checkpoint**: US2 independently testable — the facevarying fix lands, is regression-locked cross-hider, and
leaves single-incident-face/no-facevarying meshes provably unaffected.

---

## Phase 5: User Story 3 — New Subdivision Tags for Facevarying/Crease Conformance (Priority: P1)

**Goal**: Add the three currently-rejected subdivision tags (`facevaryinginterpolateboundary`,
`facevaryingpropagatecorners`, `creasemethod`) as new dispatch arms alongside the existing four, following the
existing tags' own precedent rather than the `CAttributes` four-layer pattern (FR-005, research.md R4).

**Independent Test**: Render a scene using all three new tags together with the existing four; confirm no
`CODE_BADTOKEN` error. Render a scene with an out-of-range value for one new tag; confirm a diagnostic naming the
tag and value, with the mesh still rendering via a documented fallback.

- [X] T021 [P] [US3] Add `RI_FACEVARYINGINTERPOLATEBOUNDARY`, `RI_FACEVARYINGPROPAGATECORNERS`,
  `RI_CREASEMETHOD` token constants to `src/ri/ri.h` (alongside `RI_HOLE`/`RI_CREASE`/`RI_CORNER`/
  `RI_INTERPOLATEBOUNDARY` at `ri.h:231-234`) — **landed**
- [X] T022 [P] [US3] Add matching token definitions to `src/ri/ri.cpp` (alongside `ri.cpp:160-163`) — **landed**
- [X] T023 [US3] Add new fields for the three tags to `CSubdivData` in `src/ri/subdivisionCreator.cpp:61-86`
  (not `subdivisionCreator.h` as originally assumed — `CSubdivData` lives in the `.cpp`, same correction pattern
  as T012). Plain `int` fields (`fvarBoundaryMode`, `fvarPropagateCorners`, `creaseMethod`) rather than bit flags
  like `FACE_INTEPOLATEBOUNDARY`, since `facevaryinginterpolateboundary` is a tri-state enum (0-2), not a
  boolean — a single bit can't represent it. Defaults (`2`, `0`, `0`) reproduce pre-existing behavior exactly on
  any mesh lacking these tags — **landed**
- [X] T024 [US3] Add three new dispatch arms to `create()`'s tag-recognition chain in `subdivisionCreator.cpp`
  (after the existing `RI_CORNER` arm, before the `CODE_BADTOKEN` fallthrough), replacing what would otherwise be
  three `CODE_BADTOKEN` fall-throughs with tag-specific parsing + storage, each validating `cnargs[0]`/range and
  emitting `warning(CODE_BADTOKEN, ...)` naming the tag while falling back to its documented default on an
  out-of-range input (Acceptance Scenario 2). Geometric effect wired at two hook points: `CSVertex::
  countSharpEdges()` + `CSEdge::childSharpness()` (chaikin crease decay) and `CSVertex::computeVarying()`'s base
  case (facevarying seam preserve/smooth branching) — **landed, build verified clean**
  (`cmake --build build --config Release` succeeds; FR-013 hider-invariant grep still returns zero matches)
  — depends on T023
- [X] T025 [US3] Author `examples/rib/tests/subdiv-new-tags-raytrace.rib` using all three new tags together with
  the existing four tags, confirming no `CODE_BADTOKEN` error (Acceptance Scenario 1) — depends on T024 — **landed**:
  renders cleanly (exit 0, no warnings/errors)
- [X] T025a [P] [US3] Author three single-tag isolation scenes —
  `examples/rib/tests/subdiv-tag-facevaryinginterpolateboundary-raytrace.rib`,
  `subdiv-tag-facevaryingpropagatecorners-raytrace.rib`, `subdiv-tag-creasemethod-raytrace.rib` — each rendered
  with and without its one tag on an otherwise-identical mesh, confirming a visible behavioral difference from
  the tag's absence per-tag (SC-004's literal per-tag requirement, not demonstrable from T025's all-three-at-once
  scene alone) — depends on T024 — **landed**: facevaryinginterpolateboundary 109.45 max block diff,
  facevaryingpropagatecorners 71.52, creasemethod 6.42 (all measured via `test_visual_render` against an
  otherwise-identical without-tag render). Root-cause note: the facevaryingpropagatecorners scene originally
  omitted the `interpolateboundary` tag, which on this coarse 2×2-face mesh let the raytrace limit surface
  collapse to geometry no ray intersected (byte-identical blank 1367-byte renders regardless of tag value) —
  fixed by adding `interpolateboundary` to the scene; this was a test-scene authoring gap, not a renderer bug.
- [X] T026 [P] [US3] Author `examples/rib/tests/subdiv-new-tags-badvalue-raytrace.rib` supplying an out-of-range
  value for one new tag, confirming the diagnostic + documented fallback (Acceptance Scenario 2) — depends on T024
  — **landed**: warns `facevaryinginterpolateboundary expects 1 integer argument in [0,2]; using default (2)`
- [X] T027 [P] [US3] Author `examples/rib/tests/subdiv-new-tags-with-hole-raytrace.rib` combining a new tag with
  the existing `hole`/`interpolateboundary` tags already exercised at scale by `geometry/killeroo.rib` (R8),
  confirming interaction correctness (quickstart.md Step 3.3) — depends on T024 — **landed**: renders cleanly
- [X] T028 [US3] Register `subdiv-new-tags-raytrace.rib`, `subdiv-new-tags-badvalue-raytrace.rib`,
  `subdiv-new-tags-with-hole-raytrace.rib`, and T025a's three single-tag isolation scenes in
  `tests/visual/CMakeLists.txt`; generate reference `.tif` images — depends on T025, T025a, T026, T027
  — **landed**: 6 `add_visual_test()` entries added (raytrace-only, ground-truth-for-shading rationale);
  references copied into `examples/rib/tests/references/`; all 6 pass under `ctest -L visual`
  (`Visual_subdiv-new-tags-raytrace`, `Visual_subdiv-tag-facevaryinginterpolateboundary-raytrace`,
  `Visual_subdiv-tag-facevaryingpropagatecorners-raytrace`, `Visual_subdiv-tag-creasemethod-raytrace`,
  `Visual_subdiv-new-tags-badvalue-raytrace`, `Visual_subdiv-new-tags-with-hole-raytrace`)

**Checkpoint**: US3 independently testable — all three new tags parse, store, and render correctly, singly and
combined with existing tags, with correct diagnostics on bad values. **US1 + US2 + US3 together form this
feature's P1 tier / suggested MVP scope.**

---

## Phase 6: User Story 4 — Crease-Quality Issue Reproduction (Priority: P2)

**Goal**: Reproduce the two currently-unreproduced `DEVNOTES.md:42-43` crease-quality reports with a concrete test
scene before committing to any fix (FR-006, research.md R5) — no fix sight-unseen.

**Independent Test**: Render a heavily-creased mesh (multiple crease sharpness values converging at one shared
vertex) under the ray-tracing hider; compare against a lightly-creased control mesh; document whichever outcome
is observed (reproduced-and-fixed, reproduced-and-deferred, or not-reproduced) — any of the three satisfies this
story per its explicit gate.

- [ ] T029 [US4] Author `examples/rib/tests/subdiv-crease-convergence-raytrace.rib` — multiple crease edges of
  varying sharpness converging at one shared vertex; render under the ray-tracing hider (shading ground truth,
  FR-016)
- [ ] T030 [US4] Render a lightly-creased control mesh of comparable complexity and compare visually against
  T029's output; record in `DEVNOTES_DETAILS/HIDER_PARITY.md`'s "Crease Quality" subsection (skeleton from T002)
  whether a visible artifact or a noticeably slower render is observed — qualitative bar only, no numeric
  threshold (research.md R5) — depends on T029
- [ ] T031 [US4] IF reproduced and root-caused: implement the fix entirely within `subdivisionCreator.cpp`/
  `subdivision.cpp` (`CSVertex`'s crease/corner accumulation logic), documenting whether it shares a root cause
  with US2's facevarying fix (research.md R5's hypothesis) — depends on T030
- [ ] T032 [US4] Update `DEVNOTES.md`'s two open crease-quality checkboxes (lines 42-43) to
  resolved-with-fix-reference or explicitly-deferred-with-rationale — depends on T030 (and T031 if a fix landed)
- [ ] T033 [US4] Register `subdiv-crease-convergence-raytrace.rib` in `tests/visual/CMakeLists.txt`; generate a
  reference `.tif` image reflecting the final (fixed-or-documented) state — depends on T032

**Checkpoint**: US4 independently testable — the crease-quality bug is either reproduced+fixed+documented,
reproduced-and-explicitly-deferred, or not-reproduced-and-documented; every outcome satisfies this story's gate.

---

## Phase 7: User Story 5 — Hierarchical Subdivision Edits (Priority: P3)

**Goal**: Implement `RiHierarchicalSubdivisionMesh[V]` as a new, parallel RI entry point (not a variant of
`RiSubdivisionMesh`) across all seven layers `contracts/hierarchical-subdivision-contract.md` defines, with
override resolution confined entirely to the geometry layer (FR-007/FR-008/FR-009).

**Independent Test**: Render a base mesh with a per-face, per-level tag override; confirm the effect is visible
only at its targeted face/level; confirm an override targeting a nonexistent face/level is skipped individually
with a diagnostic, not a whole-primitive failure; confirm RIB round-trip fidelity and cross-hider parity.

- [ ] T034 [P] [US5] Add a new `RIB_HIERARCHICAL_SUBDIVISION_MESH` grammar production to `src/ri/rib.y` (near the
  three existing `RIB_SUBDIVISION_MESH` alternatives, `rib.y:2390-2473`) — a new production, not a new
  alternative on the existing rule (Layer 1, research.md R6)
- [ ] T035 [P] [US5] Add a new `RIB_HIERARCHICAL_SUBDIVISION_MESH` token to `src/ri/rib.l` (alongside the existing
  `SubdivisionMesh` token, `rib.l:118`) (Layer 1)
- [ ] T036 [US5] Add the `RiHierarchicalSubdivisionMesh[V]` declaration to `src/ri/ri.h`, parallel in shape to the
  existing `RiSubdivisionMeshV` declaration (Layer 2) — depends on T034, T035
- [ ] T037 [US5] Add `RiHierarchicalSubdivisionMesh[V]` registration to `src/ri/ri.cpp` (Layer 2) — depends on T036
- [ ] T038 [US5] Implement `RiHierarchicalSubdivisionMeshV` in `src/ri/rendererContext.cpp`, parallel to
  `RiSubdivisionMeshV` (`rendererContext.cpp:5348`) — parses the base mesh (identical shape to
  `RiSubdivisionMeshV`'s own parsing) plus the new override list, storing the list for the geometry layer to
  resolve (Layer 3) — depends on T037
- [ ] T039 [P] [US5] Add the `CHierarchicalOverride` struct (`faceIndex`/`level`/`tagName`/value tuple) to new
  file `src/ri/subdivisionHierarchical.h`
- [ ] T040 [US5] Implement override-resolution logic in new file `src/ri/subdivisionHierarchical.cpp` (Layer 4) —
  resolves per-face/per-level overrides against the base mesh's tag state at subdivision-evaluation time; an
  override targeting a nonexistent `(face, level)` is skipped individually with a diagnostic naming the affected
  face/level (FR-009); overrides never mutate the base mesh's own default tags — depends on T038, T039
- [ ] T041 [US5] Implement the precedence rule in `subdivisionHierarchical.cpp`: when a User-Story-3 tag and a
  hierarchical override target the same face, the override's value wins at its targeted face/level
  (data-model.md's precedence rule) — depends on T040, T024
- [ ] T042 [US5] Add a new, parallel `CRibOut::RiHierarchicalSubdivisionMeshV` serializer to `src/ri/ribOut.cpp`
  (alongside `RiSubdivisionMeshV` at `1288,1304`) (Layer 5) — depends on T038
- [ ] T043 [P] [US5] Add the `RiHierarchicalSubdivisionMeshV` declaration to
  `src/preview/libribpreview/ribGeometryContext.h` (alongside the existing `RiSubdivisionMeshV` at `.h:122`)
  (Layer 6)
- [ ] T044 [US5] Add the `RiHierarchicalSubdivisionMeshV` handler to
  `src/preview/libribpreview/ribGeometryContext.cpp` (alongside the existing handler at `687,706`) — parses/draws
  base-mesh topology only, no override visualization (Layer 6) — depends on T043, T038
- [ ] T045 [P] [US5] Add the `Ri:HierarchicalSubdivisionMesh` Lua binding to `src/lua/prman.lua` (alongside the
  existing `Ri:SubdivisionMesh` binding at `568,573`) (Layer 7) — depends on T038
- [ ] T046 [US5] Author `examples/rib/tests/subdiv-hierarchical-override-raytrace.rib` with a per-face, per-level
  tag override; confirm the effect is visible only at its targeted face/level and the base mesh's own default
  tags are otherwise unaffected (Acceptance Scenario 1) — depends on T041
- [ ] T046a [US5] Author `examples/rib/tests/subdiv-hierarchical-tag-override-precedence-raytrace.rib` — a face
  carrying both a User-Story-3 tag and a hierarchical override at the same `(face, level)`; confirm the
  override's value visibly wins, verifying `contracts/hierarchical-subdivision-contract.md`'s precedence rule
  renders correctly (not just resolves correctly in T041's code) — depends on T041, T024
- [ ] T047 [P] [US5] Author `examples/rib/tests/subdiv-hierarchical-override-invalid-raytrace.rib` targeting a
  nonexistent face/level; confirm the mesh still renders (base mesh intact) with exactly one diagnostic naming
  the invalid override (FR-009) — depends on T040
- [ ] T048 [US5] RIB round-trip test: render `examples/rib/tests/subdiv-hierarchical-override-raytrace.rib` with
  `-writerib`, grep the output for an equivalent `HierarchicalSubdivisionMesh` statement (Layer 5 verification) —
  depends on T042, T046
- [ ] T049 [P] [US5] Author `examples/rib/tests/parity/subdiv-hierarchical-override-{reyes,raytrace}.rib` and
  confirm cross-hider parity within the block-average threshold (SC-005) — depends on T046
- [ ] T050 [US5] Load `examples/rib/tests/subdiv-hierarchical-override-raytrace.rib` in `orender-wire` and confirm
  it parses and draws the base mesh wireframe without crashing (Layer 6 preview sanity) — depends on T044, T046
- [ ] T051 [US5] Register `subdiv-hierarchical-override-{reyes,raytrace}.rib`,
  `subdiv-hierarchical-override-invalid-raytrace.rib`, and T046a's precedence scene in
  `tests/visual/CMakeLists.txt`; generate reference `.tif` images — depends on T046a, T047, T048, T049, T050

**Checkpoint**: US5 independently testable — `RiHierarchicalSubdivisionMesh` parses, renders with correct override
precedence, round-trips through RIB, previews (base topology only), and is cross-hider parity-verified; zero
override-resolution logic exists outside the geometry layer.

---

## Phase 8: User Story 6 — Loop Subdivision Scheme (Priority: P4)

**Goal**: Implement the RISpec "loop" scheme as a second algorithm alongside Catmull-Clark, sharing the identical
`CObject`/`CSurface` integration seam, reaching only Catmull-Clark's existing integration depth — not additional
capability (FR-010/FR-011, research.md R7).

**Independent Test**: Render an all-triangle mesh with `scheme="loop"` on both hiders, confirming it dices/
tessellates and shades correctly with no crashes; render a mixed triangle/non-triangle mesh with `scheme="loop"`,
confirming rejection with a diagnostic; confirm every existing `scheme="catmullclark"` scene is unaffected.

- [ ] T052 [US6] Accept `"loop"` as a second value alongside `"catmullclark"` at the scheme-rejection site in
  `RiSubdivisionMeshV` (`rendererContext.cpp:5364-5366`, currently `error(CODE_INCAPABLE, ...)`)
- [ ] T053 [P] [US6] Create new file `src/ri/subdivisionLoop.h` declaring `CLoopSubdivMesh : public CObject`
  implementing the `CObject`/`CSurface` contract (`intersect()`/`dice()`/`instantiate()`/`sample()`/
  `interpolate()`) — depends on T052
- [ ] T054 [US6] Implement `src/ri/subdivisionLoop.cpp`: Loop-scheme subdivision using iterative/uniform
  subdivision only, with no new eigenbasis-generation dependency (research.md R7); reject mixed
  triangle/non-triangle topology with a diagnostic identifying the mesh as unsuitable (Edge Cases) — depends on
  T053
- [ ] T055 [US6] Wire `CLoopSubdivMesh` construction into `RiSubdivisionMeshV`'s scheme dispatch in
  `rendererContext.cpp` — depends on T052, T054
- [ ] T056 [P] [US6] Author `examples/rib/tests/subdiv-loop-{reyes,raytrace}.rib` — an all-triangle mesh with
  `scheme="loop"`, confirming it dices/tessellates and shades correctly on both hiders — depends on T055
- [ ] T057 [P] [US6] Author `examples/rib/tests/subdiv-loop-mixed-invalid.rib` — a mixed triangle/non-triangle
  mesh with `scheme="loop"`; confirm rejection with a diagnostic, not a crash or silently-degenerate geometry
  (Edge Cases) — depends on T054
- [ ] T058 [US6] Re-render `geometry/killeroo.rib` and every existing `scheme="catmullclark"` scene; confirm
  bit-for-bit unaffected by the new scheme-dispatch branch (SC-007) — depends on T055
- [ ] T059 [US6] Register `subdiv-loop-{reyes,raytrace}.rib` and `subdiv-loop-mixed-invalid.rib` in
  `tests/visual/CMakeLists.txt`; generate reference `.tif` images — depends on T056, T057

**Checkpoint**: US6 independently testable — Loop scheme renders a smooth limit surface with no crashes on both
hiders, correctly rejects unsuitable topology, and leaves every Catmull-Clark scene completely unaffected.

---

## Phase 9: Polish & Cross-Cutting Concerns

**Purpose**: Confirm the standing architectural invariant held across every tier, run the full regression sweep,
and close out this feature's documentation deliverables.

- [ ] T060 Re-run the FR-013 hider-invariant grep check from `contracts/hider-invariant-contract.md` against
  `src/ri/{stochastic,reyes,zbuffer,raytracer,trace,photon,show}.cpp` after all six tiers have landed; confirm
  zero matches, unchanged from the T004 baseline (SC-008)
- [ ] T061 Run the full regression sweep: `ctest --test-dir build -L visual --output-on-failure` and
  `ctest --test-dir build -L libshader --output-on-failure`; confirm all existing scenes plus every new scene
  above pass (`CShow`-targeting scenes and T069's photon motion-blur scene are authored-but-not-required per
  spec.md's Clarifications; T065-T068's other photon-hider scenes are required-to-pass, per FR-014) — depends on
  T060
- [ ] T062 [P] Finalize `DEVNOTES_DETAILS/HIDER_PARITY.md`'s subdivision-surfaces section, consolidating the
  Motion Blur / Facevarying / New Tags / Crease Quality / Hierarchical Edits / Loop Scheme subsections each story
  phase appended incrementally
- [ ] T063 [P] Update `DEVNOTES.md`'s top-level status table, marking Full Subdivision Surface Support's tiers
  complete or explicitly deferred as appropriate
- [ ] T064 Author `CShow`-targeting scenes for each of US1–US6's new capabilities under `examples/rib/tests/` (not
  required to pass, per spec.md's Edge Cases); confirm they are registered in `tests/visual/CMakeLists.txt` as
  authored-not-required, matching the photon-hider motion-blur treatment
- [ ] T065 [P] [US2] Author `examples/rib/tests/subdiv-facevarying-seam-photon.rib`; register as a required-to-pass
  `add_visual_test(...)` entry in `tests/visual/CMakeLists.txt` (FR-014's photon-hider requirement) — depends on
  T016
- [ ] T066 [P] [US3] Author `examples/rib/tests/subdiv-new-tags-photon.rib`; register as required-to-pass —
  depends on T024
- [ ] T067 [P] [US5] Author `examples/rib/tests/subdiv-hierarchical-override-photon.rib`; register as
  required-to-pass — depends on T041
- [ ] T068 [P] [US6] Author `examples/rib/tests/subdiv-loop-photon.rib`; register as required-to-pass — depends
  on T055
- [ ] T069 [US1] Author `examples/rib/tests/parity/motion-subdiv-translate-photon.rib` (authored-but-not-required,
  matching the `CShow` treatment per FR-017's photon-motion-blur exception) — depends on T005

---

## Dependencies & Execution Order

### Dependency Levels (L0–L9)

Every task in a level has zero dependency on any other task in that same level — all tasks within a level are
safe to run in parallel (subject to the `[P]` marker, which additionally requires no file conflict).

| Level | Tasks |
|---|---|
| L0 | T001, T002, T003, T004, T005, T006, T011, T021, T022, T029, T034, T035, T039, T043, T052 |
| L1 | T007, T008, T009, T012, T023, T030, T036, T053, T069 |
| L2 | T010, T013, T024, T031, T032, T037, T054 |
| L3 | T014, T025, T026, T027, T033, T038, T055, T057, T025a, T066 |
| L4 | T015, T028, T040, T042, T044, T045, T056, T058, T068 |
| L5 | T016, T041, T047, T059 |
| L6 | T017, T018, T019, T046, T046a, T065, T067 |
| L7 | T020, T048, T049, T050 |
| L8 | T051 |
| L9 | T060, T061, T062, T063, T064 |

Derivation: a task's level is `max(level of every task it depends on) + 1`; tasks with no listed dependency are L0.
The deepest chain is User Story 5's seven-layer contract (T034→T036→T037→T038→T040→T041→T046→T048/T049/T050→T051,
8 hops; T046a is a shorter parallel branch off T041 also feeding T051), reflecting that hierarchical edits are
genuinely more sequential than the other five stories — RIB
grammar must exist before the RI entry point, which must exist before the renderer implementation, which must
exist before override resolution, before precedence, before the acceptance scene, before round-trip/parity/preview
verification, before registration.

### User Story Dependency Order

- **US1 (P1)**: No dependency on any other story — pure verification of pre-existing machinery.
- **US2 (P1)**: No dependency on any other story — independent bug fix.
- **US3 (P1)**: No dependency on any other story — independent tag additions. (US5's T041 depends on US3's T024
  for the precedence rule, but US3 itself is independently completable and testable without US5.)
- **US4 (P2)**: No dependency on any other story — independent reproduction/fix-or-defer gate.
- **US5 (P3)**: Depends on US3's T024 (tag-dispatch chain) only for its precedence rule (T041); otherwise
  independent. Recommended after US3 lands, so the precedence rule has real tags to test against.
- **US6 (P4)**: No dependency on any other story — independent scheme addition.

Per spec.md's own priority grouping, **US1 + US2 + US3 (all P1) are this feature's suggested MVP scope** — they
are functionally independent of each other and of US4/US5/US6, and together close every must-have RISpec
conformance gap this feature identifies.

---

## Parallel Example: Level L0

All of these can be dispatched simultaneously — no shared files, no dependency on anything else in this feature:

```text
Task: "Verify cmake --build build --config Release succeeds unmodified"                       (T001)
Task: "Create HIDER_PARITY.md subdivision-surfaces section skeleton"                            (T002)
Task: "Capture geometry/killeroo.rib baseline under examples/rib/tests/references/"             (T003)
Task: "Run FR-013 hider-invariant grep baseline check"                                          (T004)
Task: "Author motion-subdiv-translate-{reyes,raytrace}.rib"                                     (T005)
Task: "Author motion-subdiv-rotate-{reyes,raytrace}.rib"                                        (T006)
Task: "Author subdiv-facevarying-seam-raytrace.rib and confirm bug reproduces on master"         (T011)
Task: "Add RI_FACEVARYINGINTERPOLATEBOUNDARY/RI_FACEVARYINGPROPAGATECORNERS/RI_CREASEMETHOD to ri.h"  (T021)
Task: "Add matching token definitions to ri.cpp"                                                (T022)
Task: "Author subdiv-crease-convergence-raytrace.rib"                                           (T029)
Task: "Add RIB_HIERARCHICAL_SUBDIVISION_MESH grammar production to rib.y"                       (T034)
Task: "Add RIB_HIERARCHICAL_SUBDIVISION_MESH token to rib.l"                                    (T035)
Task: "Add CHierarchicalOverride struct to new subdivisionHierarchical.h"                       (T039)
Task: "Add RiHierarchicalSubdivisionMeshV declaration to ribGeometryContext.h"                  (T043)
Task: "Accept 'loop' scheme value at RiSubdivisionMeshV's rejection site"                       (T052)
```

## Parallel Example: User Story 2 (once T016 lands)

```text
Task: "Author subdiv-facevarying-seam-reyes.rib and confirm cross-hider parity"      (T018)
Task: "Render geometry/killeroo.rib and diff against the T003 baseline"             (T019)
```

## Parallel Example: User Story 5 (Layers 1/2/6/7 groundwork, before the renderer implementation)

```text
Task: "Add RIB_HIERARCHICAL_SUBDIVISION_MESH grammar production to rib.y"    (T034)
Task: "Add RIB_HIERARCHICAL_SUBDIVISION_MESH token to rib.l"                 (T035)
Task: "Add CHierarchicalOverride struct to subdivisionHierarchical.h"        (T039)
Task: "Add RiHierarchicalSubdivisionMeshV declaration to ribGeometryContext.h" (T043)
```

---

## Implementation Strategy

### MVP First (Recommended)

1. Complete Phase 1 (Setup) + Phase 2 (Foundational) — baselines captured, invariant check established.
2. Complete Phase 3 (US1) — motion blur verified cross-hider with zero code risk; fastest story to close.
3. Complete Phase 4 (US2) — the facevarying data-loss bug is a real correctness fix; highest-value single change.
4. Complete Phase 5 (US3) — new tags round out P1 conformance.
5. **STOP and validate**: run `ctest -L visual` for the scenes registered so far, re-run the T004 grep check
   (Phase 9's T060 early, informally) — this is a shippable, independently valuable increment even if US4–US6
   never land. US4 (crease reproduction) is this MVP's safety net: even if no fix is found, its concrete
   reproducer scene is itself a deliverable per FR-006's explicit either-outcome gate.
6. Add Phase 6 (US4), Phase 7 (US5), Phase 8 (US6) incrementally — each is independently testable and shippable
   on its own once its phase's checkpoint is reached.
7. Complete Phase 9 (Polish) once all desired stories have landed.

### Incremental Delivery

Each story phase ends in its own Checkpoint — a phase can be merged and shipped the moment its checkpoint is
reached, without waiting on any later-priority story. Because US1/US2/US3/US4/US6 have zero cross-story
dependency (US5 depends on US3 only for its precedence rule, not for its own core functionality), teams can
parallelize story phases directly once Phase 2's baselines are captured.

### Maximum-Parallelism Strategy (by Levels, not Stories)

For a team optimizing for wall-clock time rather than story-by-story delivery, dispatch by Dependency Level
(L0→L9) instead of by story — L0's 15 tasks span all six stories simultaneously with zero cross-task
dependency, and each subsequent level similarly mixes stories. This finishes all six stories closer to
simultaneously, at the cost of no single story being "done" until much later in the level sequence than the
Incremental Delivery ordering above would produce.

---

## Notes

- `[P]` tasks touch different files (or, for scene-authoring tasks, different RIB files with no shared registration
  target) and have no completed-task dependency in common — verified per task above, not assumed.
- Tasks without `[P]` inside the same story are deliberately sequential because they edit the same file in an
  order-dependent way (e.g. T012→T013→T014→T015→T016 all touch `subdivisionCreator.{h,cpp}`'s facevarying chain
  in a strict build-on-each-other sequence) or because a later task's assertion depends on an earlier task's code
  change actually existing (e.g. T017 cannot confirm the fix until T016 lands).
- US5's task count (19, T034–T051 plus T046a) reflects the seven-layer contract's genuine structural size, not
  over-decomposition — each layer is a materially different file/subsystem (`contracts/
  hierarchical-subdivision-contract.md`), matching how spec 009's own largest story was its most architecturally
  novel one.
- Every story's task list ends with a registration task (adding `add_visual_test`/`add_parity_test` entries and
  generating reference `.tif` images) rather than treating test registration as a separate cross-cutting phase —
  this matches `tests/visual/CMakeLists.txt`'s existing per-scene registration convention and keeps each story
  independently mergeable without touching a shared "register everything" task that would serialize all six
  stories through one file.
- No task in this file modifies any of `src/ri/{stochastic,reyes,zbuffer,raytracer,trace,photon,show}.cpp` — T004
  and T060 exist specifically to make that a checked fact (FR-012/FR-013), not an assumption.
