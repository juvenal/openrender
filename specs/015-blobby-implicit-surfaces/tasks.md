---

description: "Task list for Blobby Implicit Surfaces (spec 015)"
---

# Tasks: Blobby Implicit Surfaces

**Input**: Design documents from `/specs/015-blobby-implicit-surfaces/`

**Prerequisites**: [plan.md](./plan.md), [spec.md](./spec.md), [research.md](./research.md), [data-model.md](./data-model.md), [contracts/](./contracts/), [quickstart.md](./quickstart.md)

**Tests**: **REQUIRED, not optional.** Constitution Principle III (Test-Driven
Development) is marked NON-NEGOTIABLE, and `quickstart.md` §1 fixes the
test-first order. Every story phase below leads with failing tests.

**Organization**: Grouped by user story so each can be implemented, tested, and
delivered independently.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: Which user story the task serves (US1–US8)
- Exact file paths are given in every task

## Path Conventions

Single project. Renderer core in `src/ri/`, tests in `tests/`, documentation in
`site/`. Paths are repository-relative.

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create the new translation units and test scaffolding so every
later phase has somewhere to land.

- [ ] T001 Create empty skeleton headers and sources `src/ri/blobby.h`, `src/ri/blobby.cpp`, `src/ri/blobbyField.h`, `src/ri/blobbyField.cpp`, `src/ri/blobbyPolygonize.h`, `src/ri/blobbyPolygonize.cpp`, `src/ri/blobbyRepeller.h`, `src/ri/blobbyRepeller.cpp` with the project's standard LGPL file header
- [ ] T002 Register the four new source files in `src/ri/CMakeLists.txt`
- [ ] T003 [P] Create `tests/unit/blobby/CMakeLists.txt` and `tests/unit/blobby/blobbyTestUtils.h` mirroring `tests/unit/csg/`, setting `LABELS blobby` via `set_tests_properties` on every test so `ctest -L blobby` matches (an unlabelled suite reports success against zero tests, which is indistinguishable from passing)
- [ ] T004 [P] Register `add_subdirectory(unit/blobby)` in `tests/CMakeLists.txt` alongside the existing `add_subdirectory(unit/csg)`
- [ ] T005 Add blobby counters (`numBlobbies`, `numBlobbyLeaves`, `numBlobbyFieldEvals`, `numBlobbyCellsVisited`, `numBlobbySurfaceCells`, `numBlobbyTriangles`) to `CStats` in `src/ri/stats.h` and zero them in the reset block of `src/ri/stats.cpp`
- [ ] T006 Print the blobby counters and the derived surface-cells-to-visited-cells percentage in `CStats::printStats()` in `src/ri/stats.cpp`, guarded by `> 0` in the same style as the existing U/V split ratios

**Checkpoint**: The project builds with empty blobby units and an empty, correctly-labelled test suite.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Scene-description plumbing, the validated code-array model, and the
field extent. Every user story depends on a `Blobby` statement actually
reaching the renderer.

**⚠️ CRITICAL**: No user story work can begin until this phase is complete.

### Attribute and option plumbing (all four layers must land together)

- [ ] T007 Add `RI_BLOBBYTOLERANCE` and `RI_BLOBBYOPCODEORDER` token constants in `src/ri/ri.h` and define them in `src/ri/ri.cpp`
- [ ] T008 Pre-declare both tokens in `initDeclarations()` in `src/ri/rendererDeclarations.cpp` — without this the RIB parser rejects them with "Parameter not declared" before parsing is ever reached
- [ ] T009 [P] Add fidelity tolerance storage and `find()` support to `CAttributes` in `src/ri/attributes.h` and `src/ri/attributes.cpp`
- [ ] T010 [P] Add opcode-order storage to `COptions` in `src/ri/options.h` and `src/ri/options.cpp`
- [ ] T011 Parse `Attribute "blobby" "float tolerance"` in `RiAttributeV()` in `src/ri/rendererContext.cpp`
- [ ] T012 Parse `Option "blobby" "string opcodeorder"` in `RiOptionV()` in `src/ri/rendererContext.cpp`, accepting `"rispec"` (default) and `"appnote"`, with a diagnostic and default fallback on any other value

### Code-array model (test-first)

- [ ] T013 [P] Write failing unit tests for every malformed-declaration case in `tests/unit/blobby/test_code_validation.cpp` — unknown opcode, reserved 1004–1099, truncated instruction, variable-arity count zero/negative/overrunning, self-reference, forward reference, operand index past `floats`/`strings`/prior results, and `nleaf` mismatch (per `contracts/rib-binding.md` §6). At least 15 cases, satisfying SC-005
- [ ] T014 Implement `CBlobbyInstruction` and `CBlobbyProgram` with leaf-index assignment and full validation in `src/ri/blobbyField.h` and `src/ri/blobbyField.cpp` to pass T013. Leaf index is the ordinal among `opcode >= 1000` instructions counting all four primitive types (FR-016); an `nleaf` mismatch is a diagnostic that continues using the actual count, never fatal — Pixar's own hand example declares 21 while emitting 22

### Field extent

- [ ] T015 Write failing unit tests for the field extent in `tests/unit/blobby/test_extent.cpp` — an ellipsoid field's extent is its transformed unit sphere, a segment's is its capsule, a constant field contributes **no spatial support**, and a repeller is **unbounded across its ground plane**; assert the stated rule for a program containing an unbounded or support-free field
- [ ] T016 Implement field extent computation on `CBlobbyProgram` in `src/ri/blobbyField.cpp` as the union of each primitive field's bounded support, with an explicit documented rule for constant and repeller fields (passes T015). This value is consumed twice downstream and produced nowhere else: it seeds and terminates the extraction walk (T031) and supplies the default cell size (T073)

### RIB and API entry points

- [ ] T017 Wire both `RIB_BLOBBY` productions in `src/ri/rib.y` (currently `// FIXME: Not implemented` at lines 2617 and 2626) to call `RiBlobbyV`, following the neighbouring `parameterListCheck()`/`sizeCheck()` idiom; the three-array form passes an empty strings array
- [ ] T018 Replace the `CODE_INCAPABLE` stub in `CRendererContext::RiBlobbyV()` in `src/ri/rendererContext.cpp` (line 5571) with `CBlobbyProgram` construction and diagnostics, and remove the `netNumServers > 0` early return; emits no geometry yet
- [ ] T019 Implement a real `Blobby` statement emit in `CRibOut::RiBlobbyV()` in `src/ri/ribOut.cpp`, replacing the `RIE_UNIMPLEMENT` stub at line 1418 (FR-004 — this and T018's early-return removal together are what stop a blobby being silently lost under network rendering), and verify the base no-op `CRiInterface::RiBlobbyV()` in `src/ri/riInterface.cpp` (line 309) is still correct

**Checkpoint**: A `Blobby` statement parses, validates, reports clear errors, knows its extent, and round-trips through RIB output — but renders nothing yet.

---

## Phase 3: User Story 1 - Model an organic shape from self-blending blobs (Priority: P1) 🎯 MVP

**Goal**: Ellipsoid fields combined by add and max resolve to a correct blended
level surface that renders identically under every hider.

**Independent Test**: Render two ellipsoid fields summed — far apart, close
enough to influence, and merged — and confirm two separate surfaces, two
surfaces drawn toward each other, and one merged surface with a smooth waist.
Repeat the merged case under each hider and confirm the silhouettes agree.

### Tests for User Story 1 ⚠️ Write first, confirm they FAIL

- [ ] T020 [P] [US1] Write failing unit tests for the ellipsoid field and its gradient in `tests/unit/blobby/test_field_primitives.cpp`, asserting the hand-computable anchors from `quickstart.md` §1 — `F(0)=1`, `F(0.5)=0.421875`, `F(1)=0`, `F(R>1)=0` exactly — plus gradient direction, and the degenerate cases `∇F=0` at `R=0` and `R=1`
- [ ] T021 [P] [US1] Write failing unit tests for the add and maximum combining operations and their gradients in `tests/unit/blobby/test_field_combining.cpp`
- [ ] T022 [P] [US1] Write failing tests for a singular ellipsoid matrix contributing no field, and for a field that never crosses the threshold yielding no geometry, in `tests/unit/blobby/test_field_primitives.cpp`
- [ ] T023 [US1] Write failing threshold-calibration tests in `tests/unit/blobby/test_threshold_calibration.cpp` asserting the value's **bracket on field values**, which needs only the evaluator and no extraction: at the midpoint between two adjacent blobs of the published octahedron (unit-sphere fields at ±0.89 on each axis, summed) the field must be **above** threshold, and at the midpoint of the published unblended pair it must be **below**. These two constraints are what "bracket the value" means (FR-015), and they must pass before T024's absolute-radius assertions can mean anything
- [ ] T024 [P] [US1] Write failing analytic ground-truth tests in `tests/unit/blobby/test_polygonize_analytic.cpp` — a lone ellipsoid field produces vertices lying on that exact ellipsoid within tolerance, two coincident identical blobs produce a sphere of the analytically predicted larger radius, per-vertex normals match the analytic surface normal, and (the geometric counterpart of T023) the octahedron resolves to **one** connected component while the unblended pair resolves to **two** (SC-003)
- [ ] T025 [P] [US1] Write a failing watertightness test in `tests/unit/blobby/test_polygonize_watertight.cpp` asserting every triangle edge is shared by exactly two triangles and the Euler characteristic matches the expected genus (prerequisite for FR-027 — a leaky mesh corrupts boolean resolution silently)
- [ ] T026 [P] [US1] Write a failing determinism test in `tests/unit/blobby/test_determinism.cpp` asserting that extracting the same declaration twice produces bit-identical vertex and triangle arrays (FR-023a)
- [ ] T027 [P] [US1] Write a failing test in `tests/unit/blobby/test_extent.cpp` that the emitted mesh reports an object-space bound fully containing its surface, and a visual scene assertion that a blobby entirely outside the frustum is culled with no error and no visible geometry (FR-028, US1 scenario 5). Note `CPolygonMesh`'s constructor already derives `bmin`/`bmax` from `pl->data0` and `pl->data1` (`src/ri/polygons.cpp:1261-1272`), so this is expected to pass once geometry is emitted — it is a guard against that changing, not new implementation

### Implementation for User Story 1

- [ ] T028 [US1] Implement the spherical bump `F(R)=(1-R²)³` with bounded support and its analytic gradient, and opcode 1001 ellipsoid evaluation through the inverse 4×4, in `src/ri/blobbyField.cpp` (passes T020, T022)
- [ ] T029 [US1] Implement the cheap evaluator entry point `evaluate(point, time, field, gradient)` plus opcodes 0 (add) and 2 (maximum) with gradient composition in `src/ri/blobbyField.cpp` (passes T021). Max/min gradient must use the same tie-break as the field evaluation so normals and geometry agree at a seam
- [ ] T030 [US1] Derive and record the surface threshold with its justification in `src/ri/blobbyField.h`, passing T023's field-value bracket (FR-015 — calibrate, do not hard-code the commonly cited value)
- [ ] T031 [US1] Implement the seeded continuation marching-tetrahedra walk in `src/ri/blobbyPolygonize.cpp` — one seed cell per primitive field in code-array order, bounded by the T016 extent, a FIFO frontier, an **ordered** integer-keyed visited set (never a hash container, per research Decision 3), 6-tetrahedra cube decomposition, and a shared-edge vertex cache so adjacent tetrahedra reuse vertices (passes T024, T025, T026)
- [ ] T032 [US1] Add the everywhere-above-threshold guard to `src/ri/blobbyPolygonize.cpp` so a field with no boundary terminates promptly with a diagnostic instead of walking outward indefinitely
- [ ] T033 [US1] Emit analytic gradient normals per vertex during extraction in `src/ri/blobbyPolygonize.cpp`, with a guard for the degenerate zero-gradient case rather than normalizing a zero vector (FR-024, passes T024)
- [ ] T034 [US1] Build the `CPolygonMesh` in `src/ri/blobby.cpp` — pack `P` and `N` as `CONTAINER_VERTEX` `CPlParameter` entries into a `CPl`, following `csgBuildMeshForAttributeGroup` (`src/ri/csgTree.cpp:600-615`), and construct with the blobby's **own** `CXform`, not identity (vertices stay in object space; `CSurface::sample()` applies `xform->from`, which preserves instancing)
- [ ] T035 [US1] Call `addObject()` with the finished mesh from `CRendererContext::RiBlobbyV()` in `src/ri/rendererContext.cpp`, replacing T018's no-geometry placeholder (passes T027)
- [ ] T036 [US1] Read the `u`/`v`/`s`/`t` convention off the subdivision-surface path and match it for blobby in `src/ri/blobby.cpp` (FR-021 — blobbies have no global parameterisation; shaders must read defined values, not uninitialised ones)
- [ ] T037 [US1] Increment the blobby statistics counters at their evaluation, cell-visit, and emission sites in `src/ri/blobbyField.cpp`, `src/ri/blobbyPolygonize.cpp`, and `src/ri/blobby.cpp`
- [ ] T038 [P] [US1] Add visual scenes to `examples/rib/` — two ellipsoid fields summed at far, influencing, and merged separations, the same pair combined by maximum, and one blobby positioned entirely outside the frustum
- [ ] T039 [US1] Register the US1 scenes in `tests/visual/CMakeLists.txt` so both `Visual_` and `Parity_` tests are emitted; the `Parity_` test is SC-004's cross-hider agreement check and needs no new harness

**Checkpoint**: A blobby renders. MVP complete and independently demonstrable under every hider.

---

## Phase 4: User Story 2 - Control blending precisely with the full operation set (Priority: P2)

**Goal**: The remaining combining operations work, including both readings of
the contradictory opcodes 4 and 5.

**Independent Test**: Render the reference hand three ways — unblended, fully
summed, and selectively grouped under a maximum — and confirm the third shows
fingers merging into the palm with no webs between fingers. Separately, dent a
large field with a subtracted small one, then elongate it to punch through.

### Tests for User Story 2 ⚠️ Write first, confirm they FAIL

- [ ] T040 [P] [US2] Extend `tests/unit/blobby/test_field_combining.cpp` with failing tests for multiply, minimum, negate, and identity, including gradient composition (product rule for multiply, winner's gradient for minimum)
- [ ] T041 [P] [US2] Write failing tests for **both** opcode 4/5 operand orders in `tests/unit/blobby/test_opcode_order.cpp` — the same code array evaluated under `"rispec"` (4=subtract, 5=divide) and `"appnote"` (4=divide, 5=subtract) — and assert the default is the RISpec order (SC-002)
- [ ] T042 [P] [US2] Write failing tests for divide-by-zero and other degenerate combining inputs producing a defined, finite result in `tests/unit/blobby/test_field_combining.cpp`

### Implementation for User Story 2

- [ ] T043 [US2] Implement opcodes 1 (multiply), 3 (minimum), 6 (negate), and 7 (identity) with gradient composition in `src/ri/blobbyField.cpp` (passes T040)
- [ ] T044 [US2] Implement opcodes 4 and 5 with the operand mapping resolved **once at construction** from `CBlobbyProgram::opcodeOrder`, not branched per evaluation point, in `src/ri/blobbyField.cpp` (passes T041)
- [ ] T045 [US2] Add the degenerate-input guards for divide and the other combining operations in `src/ri/blobbyField.cpp` (passes T042)
- [ ] T046 [P] [US2] Add visual scenes to `examples/rib/` — the reference hand in its three blending configurations, and a blob dented then pierced by a subtracted blob
- [ ] T047 [US2] Register the US2 scenes in `tests/visual/CMakeLists.txt`

**Checkpoint**: All eight combining operations work; both opcode 4/5 readings are selectable and tested.

---

## Phase 5: User Story 3 - Build tubular shapes from segment blobs (Priority: P2)

**Goal**: Segment and constant fields work, so piecewise-linear skeletons can be
modelled directly.

**Independent Test**: Render a chain of segment fields along a curved path and
inspect for constant thickness, no bulges at joints, and rounded caps at the
free ends.

### Tests for User Story 3 ⚠️ Write first, confirm they FAIL

- [ ] T048 [P] [US3] Extend `tests/unit/blobby/test_field_primitives.cpp` with failing tests for the opcode 1002 segment field and gradient, and for opcode 1000 constant
- [ ] T049 [P] [US3] Write a failing test that a zero-length segment degenerates to a sphere of the declared radius rather than an error or crash, in `tests/unit/blobby/test_field_primitives.cpp`
- [ ] T050 [P] [US3] Extend `tests/unit/blobby/test_polygonize_analytic.cpp` with a failing test that a lone segment field produces a capsule of the declared radius about the declared endpoints (SC-003)

### Implementation for User Story 3

- [ ] T051 [US3] Implement opcode 1002 as the convolution of a segment impulse with the same spherical bump the ellipsoid uses, reading 23 floats (two endpoints, radius, 4×4), with its analytic gradient, in `src/ri/blobbyField.cpp` (passes T048, T050). The shared bump is what makes end-to-end segments join without bulges
- [ ] T052 [US3] Implement opcode 1000 constant and the zero-length-segment degeneracy in `src/ri/blobbyField.cpp` (passes T048, T049), and extend the T016 extent rule to cover both
- [ ] T053 [P] [US3] Add visual scenes to `examples/rib/` — a multi-segment tube along a curved path, and one blobby mixing segment, ellipsoid, and constant fields
- [ ] T054 [US3] Register the US3 scenes in `tests/visual/CMakeLists.txt`

**Checkpoint**: All four primitive-field types except the repeller are implemented.

---

## Phase 6: User Story 4 - Give each blob its own shading values (Priority: P3)

**Goal**: Per-blob primvars blend across the surface in step with the geometry.

**Independent Test**: Render the six-blob coloured octahedron and confirm each
blob's colour dominates at its centre with smooth mixing across every blend
region; pull the blobs apart and confirm no bleed.

### Tests for User Story 4 ⚠️ Write first, confirm they FAIL

- [ ] T055 [P] [US4] Write failing tests for weight propagation in `tests/unit/blobby/test_value_blending.cpp` — proportional apportionment for add and multiply, winner-takes-all for maximum and minimum, and **zero** contribution from a negated operand and from a subtraction's subtrahend
- [ ] T056 [P] [US4] Write a failing test for the zero-denominator fallback (equal split among contributing operands) producing a continuous, finite result in `tests/unit/blobby/test_value_blending.cpp` (FR-019a)
- [ ] T057 [P] [US4] Write a failing test that two colour groups combined by maximum show no cross-group bleed, in `tests/unit/blobby/test_value_blending.cpp` — this is the case a flat field-strength average would get wrong (US4 scenario 4)

### Implementation for User Story 4

- [ ] T058 [US4] Implement the weighted evaluator entry point `evaluateWeights(point, time, field, gradient, leafWeights)` in `src/ri/blobbyField.cpp`, propagating per-leaf weights up the code array alongside the field in the same tree walk (passes T055, T056, T057)
- [ ] T059 [US4] Call the weighted entry point **only at vertex emission**, keeping the cheap `evaluate()` on the traversal path, in `src/ri/blobbyPolygonize.cpp` — weights cost an O(numLeaves) write per call and the traversal makes millions of calls that would discard the result, which would directly undermine SC-012 on the 500-field spiral
- [ ] T060 [US4] Blend author-declared varying and vertex primvars by leaf weight and pack them as additional `CONTAINER_VERTEX` `CPlParameter` entries in `src/ri/blobby.cpp`, with reads clamped to the shorter of the declared and actual leaf counts so an `nleaf` mismatch cannot read past the end (FR-017)
- [ ] T061 [P] [US4] Add visual scenes to `examples/rib/` — the six-blob coloured octahedron blended, the same six pulled apart, and a subtracted blob confirming its colour does not tint the carved surface
- [ ] T062 [US4] Register the US4 scenes in `tests/visual/CMakeLists.txt`

**Checkpoint**: Per-blob shading values blend in step with the geometry.

---

## Phase 7: User Story 5 - Keep a solid texture attached to a bending blob chain (Priority: P3)

**Goal**: The `mpoint` reference-space type works, so solid textures adhere to a
deforming blob chain.

**Independent Test**: Render a straight three-blob chain with reference mappings
matching its placements and confirm an undistorted solid checker; translate the
middle blob and confirm its texture moves with it while the outer blobs stay
anchored.

### Tests for User Story 5 ⚠️ Write first, confirm they FAIL

- [ ] T063 [P] [US5] Write failing tests in `tests/unit/blobby/test_mpoint.cpp` that an `mpoint` value at a surface point equals the point carried back through the blob's own inverse matrix and forward through the `mpoint` matrix, and that it is delivered to shaders as a `point` rather than a matrix

### Implementation for User Story 5

- [ ] T064 [US5] Add `TYPE_MPOINT` to `EVariableType` in `src/ri/rendererc.h`, following the `TYPE_QUAD` (`// For "Pw"`) precedent of a type whose RIB form differs from its shader form
- [ ] T065 [US5] Add the `TYPE_MPOINT` case in `src/ri/pl.cpp` (near line 720)
- [ ] T066 [US5] Add the `TYPE_MPOINT` cases in `src/ri/ribOut.cpp` (near lines 1609 and 1722)
- [ ] T067 [US5] Add the `TYPE_MPOINT` case in `src/ri/rendererContext.cpp` (near line 1274). T064–T067 must land together — a partially-cased enum breaks exhaustiveness warnings under the project's warnings-as-errors policy and half-wires the type
- [ ] T068 [US5] Register the `mpoint` type in `initDeclarations()` in `src/ri/rendererDeclarations.cpp`
- [ ] T069 [US5] Implement per-blob `mpoint` evaluation in `src/ri/blobby.cpp` — invert each blob's matrix once at build time and compose with the `mpoint` matrix so per-evaluation cost is one matrix-vector multiply — and blend the result through the US4 weight mechanism (passes T063)
- [ ] T070 [P] [US5] Add visual scenes to `examples/rib/` — a three-blob chain with `vertex mpoint Pref` under a solid texture, straight and with the middle blob displaced
- [ ] T071 [US5] Register the US5 scenes in `tests/visual/CMakeLists.txt`

**Checkpoint**: Solid textures adhere to bending blob chains.

---

## Phase 8: User Story 6 - Trade surface fidelity against render cost (Priority: P3)

**Goal**: The fidelity attribute plumbed in Phase 2 acquires its default,
validation, and inheritance behaviour.

**Independent Test**: Render one blobby filling the frame at default fidelity
and again tightened, confirming a visibly smoother silhouette; confirm a scene
that never sets it is unchanged.

### Tests for User Story 6 ⚠️ Write first, confirm they FAIL

- [ ] T072 [P] [US6] Write failing tests in `tests/unit/blobby/test_tolerance.cpp` that the default cell size is derived from the T016 field extent, that tightening it measurably reduces deviation from the analytic surface, and that zero, negative, and absurdly large values each produce a diagnostic and a usable fallback rather than a hang or memory exhaustion

### Implementation for User Story 6

- [ ] T073 [US6] Derive the default cell size from the T016 field extent in `src/ri/blobby.cpp` so scenes that never set the attribute render smoothly at typical framing (FR-025)
- [ ] T074 [US6] Add value validation with diagnostic and fallback in `src/ri/blobby.cpp` (passes T072)
- [ ] T075 [P] [US6] Add visual scenes to `examples/rib/` — one blobby filling the frame at default and at a tightened tolerance, plus a scene confirming attribute-scope inheritance
- [ ] T076 [US6] Register the US6 scenes in `tests/visual/CMakeLists.txt` and confirm SC-006: smooth at typical framing by default, and smooth at full-frame close-up once tightened

**Checkpoint**: Authors can tune fidelity; defaults look right untouched.

---

## Phase 9: User Story 7 - Repel a blobby off an irregular ground surface (Priority: P4)

**Goal**: Opcode 1003 works, including reading a depth file outside any shading
context.

**Independent Test**: Render a blob descending toward a repelling ground plane
at several heights and confirm it flattens, bulges, hovers without
interpenetrating, and is unaffected above the cut-off height. Vary each shaping
parameter in isolation.

### Tests for User Story 7 ⚠️ Write first, confirm they FAIL

- [ ] T077 [P] [US7] Write failing tests for `bump()` and `ease()` in `tests/unit/blobby/test_repeller.cpp` asserting `bump(0)=0`, `bump(1)=1`, `bump(2)=0`, zero outside `0..2`, and `ease` clamping at both ends — assert all three `bump` anchors, because the published C for this function is corrupted (`if(r=2.)` is an assignment that silently returns 0 always) and a transcription slip would otherwise pass unnoticed
- [ ] T078 [P] [US7] Write failing tests for `repulsion(z,A,B,C,D)` in `tests/unit/blobby/test_repeller.cpp` — zero at and above `A`, finite at `z` near zero via the `ZCLAMP` guard, and each of A, B, C, D changing the profile independently in the documented direction
- [ ] T079 [P] [US7] Write a failing test that a missing or unreadable depth file yields a diagnostic naming the file, a zero field contribution, and a continuing render, in `tests/unit/blobby/test_repeller.cpp`

### Implementation for User Story 7

- [ ] T080 [US7] Implement a context-free depth-file loader in `src/ri/blobbyRepeller.cpp` composing existing pieces — `CRenderer::locateFile()` for path resolution, direct `TIFFOpen`/`TIFFGetField`/`TIFFReadScanline` under the existing `tiffErrorHandler`, and the shadow loader's `toNDC`/`toCamera` recovery pattern (`src/ri/texture.cpp:1292-1293`). Do **not** call `CTexture::lookupz()`: it reaches `lookupPixel()`, which dereferences `context->thread`, and no `CShadingContext` exists at build time — a null context is a crash, not a degradation
- [ ] T081 [US7] Implement `bump()`, `ease()`, and `repulsion()` in `src/ri/blobbyRepeller.cpp` per `contracts/field-semantics.md` §2, using the **corrected** guard `r <= 0. || r >= 2.` (passes T077, T078)
- [ ] T082 [US7] Wire opcode 1003 into field evaluation in `src/ri/blobbyField.cpp` — vertical distance measured in the view direction the depth file was generated in — with a numeric gradient along the depth-map normal, and apply the T016 unbounded-field extent rule
- [ ] T083 [US7] Implement the invalid-file path (diagnostic, zero contribution, render continues) in `src/ri/blobbyRepeller.cpp` (passes T079)
- [ ] T084 [P] [US7] Add a depth file fixture and visual scenes to `examples/rib/` — a blob at several heights above an irregular repelling ground plane, plus one scene per shaping parameter varied
- [ ] T085 [US7] Register the US7 scenes in `tests/visual/CMakeLists.txt`

**Checkpoint**: All four primitive-field opcodes are complete. RISpec 3.2 §5.6 is fully implemented.

---

## Phase 10: User Story 8 - Blur a moving blobby (Priority: P4)

**Goal**: A moving blobby blurs through the same generic mechanism as every
other primitive.

**Independent Test**: Render a blobby and an ordinary primitive moving
identically in one motion block and confirm both smear over the same extent;
remove the motion block and confirm the blobby is sharp.

### Tests for User Story 8 ⚠️ Write first, confirm they FAIL

- [ ] T086 [P] [US8] Write failing tests in `tests/unit/blobby/test_motion.cpp` that the second sample has identical vertex count, ordering, and triangle connectivity to the first — the only shape a two-sample `CPl` can represent
- [ ] T087 [P] [US8] Write a failing test that advection uses a **fixed** step count, so the advected position is a deterministic function of its input, in `tests/unit/blobby/test_motion.cpp`. An "iterate until converged" loop makes the step count a floating-point predicate that varies with compiler flags and FMA contraction, reintroducing exactly the cross-machine divergence FR-023a forbids, worst at vertices near a topology change
- [ ] T088 [P] [US8] Write a failing test that topology-changing motion (lobes merging, a piece vanishing) produces a bounded, non-crashing result with unconverged vertices left in place, in `tests/unit/blobby/test_motion.cpp`
- [ ] T089 [P] [US8] Extend `tests/unit/blobby/test_determinism.cpp` with a failing assertion that a moving blobby's second sample is bit-identical across runs

### Implementation for User Story 8

- [ ] T090 [US8] Implement fixed-step gradient advection of shutter-open vertices onto the shutter-close level set in `src/ri/blobbyPolygonize.cpp` (passes T086, T087, T088, T089)
- [ ] T091 [US8] Populate `CPl::data1` with the advected sample in `src/ri/blobby.cpp` — `CPolygonTriangle::moving()` is exactly `pl->data1 != NULL` (`src/ri/polygons.h:104`), so no hider changes are needed
- [ ] T092 [P] [US8] Add visual scenes to `examples/rib/` — a blobby and an ordinary primitive under identical motion in one block, the same scene without the motion block, and a blobby whose field parameters change over the shutter
- [ ] T093 [US8] Register the US8 scenes in `tests/visual/CMakeLists.txt`, labelling the motion scenes `slow` if they follow the existing motion tests' cost profile

**Checkpoint**: All eight user stories are independently functional.

---

## Phase 11: Polish & Cross-Cutting Concerns

**Purpose**: Integration properties, scale validation, non-regression, and
documentation.

### Integration and scale

- [ ] T094 [P] Add a visual scene to `examples/rib/` placing a blobby inside a `SolidBegin`/`SolidEnd` boolean block, and register it in `tests/visual/CMakeLists.txt`, verifying FR-027 and SC-008. This needs **no implementation**: `addObject()` already chains into `currentSolid->leafObjects` (`src/ri/rendererContext.cpp:504`) and `csgTessellateOperand` already handles a `CPolygonMesh` leaf (`src/ri/csgTree.cpp:316`); the procedural-capture guard is never on this path. The real risk it covers is mesh watertightness, which fails silently rather than erroring
- [ ] T095 Add the ~500-segment toroidal spiral scene to `examples/rib/` and register it in `tests/visual/CMakeLists.txt`, confirming it completes and that the surface-cell ratio from T006 demonstrates cost tracking the surface rather than the bounding volume (SC-012)
- [ ] T096 Verify the RIB round-trip and distributed paths produce the same image with no seam where different servers' geometry meets, per `quickstart.md` §7 (SC-009)
- [ ] T097 [P] Verify the Python (`src/python/prman.py:469`) and Lua (`src/lua/prman.lua:588`) `Blobby` emitters round-trip end to end (FR-005)

### Non-regression

- [ ] T098 Run the full existing visual regression suite and confirm no scene without a blobby changed (SC-007)
- [ ] T099 Confirm every new unit test is picked up by `ctest -L blobby` and that the count is non-zero — a mislabelled suite reports success against zero tests

### Documentation

- [ ] T100 [P] Document the `Blobby` statement in `site/` — both RIB forms, every opcode with its operands, per-blob parameters including `mpoint`, and worked examples an author can copy and render (FR-032)
- [ ] T101 [P] Document `Attribute "blobby" "float tolerance"` and `Option "blobby" "string opcodeorder"` in `site/`, **including the opcode 4/5 erratum that motivates the option** with citations to both primary sources — an author whose subtraction renders wrong needs to be able to find this
- [ ] T102 [P] Document the known limitations in `site/` — two motion samples (a renderer-wide format property, not a blobby one) and topology-changing motion being bounded but not faithful
- [ ] T103 Update `DEVNOTES.md` with the feature's status and add any hard-won gotchas to the project's known-gotchas record

### Final validation

- [ ] T104 Walk the `quickstart.md` Definition of Done checklist end to end and confirm every success criterion SC-001 through SC-012 is met

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately
- **Foundational (Phase 2)**: Depends on Setup — **BLOCKS every user story**
- **US1 (Phase 3)**: Depends on Foundational. Builds the evaluator and polygonizer that every later story extends
- **US2, US3 (Phases 4–5)**: Depend on US1's evaluator and polygonizer
- **US4 (Phase 6)**: Depends on US1. US2/US3 make its tests richer but are not required
- **US5 (Phase 7)**: Depends on US4's weight mechanism
- **US6 (Phase 8)**: Depends on US1's polygonizer and T016's extent
- **US7 (Phase 9)**: Depends on US1's evaluator only — the most independent story
- **US8 (Phase 10)**: Depends on US1's polygonizer
- **Polish (Phase 11)**: Depends on the stories whose properties it validates

### Story Dependency Notes

Unlike a typical spec-kit feature, the stories here are **not** fully
independent: US1 necessarily builds the shared field evaluator and
polygonizer, because a primitive that renders nothing cannot be demonstrated.
Every later story extends those two components rather than standing alone.
US7 (repeller) is the most separable — it touches only field evaluation plus
its own file — which is why it is safe as the lowest-priority story.

### Within Each Story

Tests are written and confirmed failing before implementation (Constitution
Principle III). Field evaluation before extraction; extraction before mesh
emission; mesh emission before scene registration.

### Parallel Opportunities

- T003, T004 in Setup
- T009, T010 in Foundational (different files); T007→T008 and T011/T012 are sequential (shared files)
- All test-authoring tasks within a story phase are marked [P] — separate files, no interdependencies. The one exception is T023, which gates T024's absolute assertions
- Scene-authoring tasks (T038, T046, T053, T061, T070, T075, T084, T092) are [P] with each other; their `tests/visual/CMakeLists.txt` registrations are **not**, since they share one file
- All three documentation tasks (T100–T102) are [P]
- Once Foundational and US1 are complete, US2/US3/US6/US7/US8 can proceed in parallel across contributors; US4 then US5 form the one serial chain

---

## Parallel Example: User Story 1

```bash
# Author the US1 tests together, confirm every one FAILS before implementing:
Task: "Ellipsoid field and gradient tests in tests/unit/blobby/test_field_primitives.cpp"
Task: "Add/maximum combining tests in tests/unit/blobby/test_field_combining.cpp"
Task: "Analytic ground-truth tests in tests/unit/blobby/test_polygonize_analytic.cpp"
Task: "Watertightness test in tests/unit/blobby/test_polygonize_watertight.cpp"
Task: "Determinism test in tests/unit/blobby/test_determinism.cpp"
Task: "Extent and frustum-culling test in tests/unit/blobby/test_extent.cpp"
```

---

## Implementation Strategy

### MVP scope: Phases 1–3 (US1 only)

1. Phase 1 Setup
2. Phase 2 Foundational — **blocks everything**
3. Phase 3 US1
4. **STOP and VALIDATE**: two ellipsoid fields blend correctly and render
   identically under every hider

That is a genuinely useful renderer capability on its own: the classic
blobby-molecule shapes are expressible with ellipsoids, add, and max alone.

### Incremental delivery

Each subsequent phase adds a demonstrable capability without destabilising
what came before: US2 the full operation set, US3 tubular shapes, US4 per-blob
colour, US5 solid-texture adhesion, US6 the fidelity control, US7 ground
repulsion, US8 motion blur.

### Sequencing risks worth front-loading

- **T016's extent is consumed twice and produced once.** It bounds the
  extraction walk (T031) and supplies the default cell size (T073). Its rule
  for constant fields (no spatial support) and repellers (unbounded across the
  ground plane) has to be decided there, not improvised at each call site.
- **T023 must stay a pure field-value test.** It brackets the threshold using
  the evaluator alone, so it can run before the polygonizer exists. The
  connectivity form of the same constraint lives in T024, after extraction —
  writing T023 as a connectivity test would make T030 depend on T031 and break
  the TDD ordering.
- **T031's container choice is a correctness decision, not a performance one.**
  An unordered visited set would pass every single-machine test and fail only
  as a seam in a distributed render.
- **T059 must not be folded into T058.** Collapsing the two evaluator entry
  points is the natural-looking simplification and is exactly what breaks
  SC-012 on the spiral.
