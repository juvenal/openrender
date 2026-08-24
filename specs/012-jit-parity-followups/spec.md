# Feature Specification: JIT/Interpreter Parity Follow-ups (post-011)

**Feature Branch**: `012-jit-parity-followups`

**Created**: 2026-08-24

**Status**: Draft

**Input**: User description: "Three follow-up defects/gaps found during `specs/011-jit-opcode-parity`, all explicitly deferred to their own controlled spec: (1) a reproducible `.rslo` interpreter crash on varying-index reads of `uniform string` arrays (`usfroma`); (2) the JIT's `illuminance` support is a hand-synced parallel reimplementation of the interpreter's light-iteration logic rather than genuine shared-function delegation; (3) the JIT emitter's `emitBin`/`emitUn`/`emitTern` dispatch pays a full `numVerts`-sized call for every instruction, while the interpreter short-circuits uniform-classified instructions to run once — the confirmed, documented root cause of spec 011's SC-006 shortfall (JIT-vs-interpreter wall-clock parity not met, `lessons-learned.md` Phase 10)."

## Clarifications

### Session 2026-08-24

- Q: How should the three issues be prioritized as User Stories? → A: Correctness first. P1 = the interpreter crash (the only issue that is an outright failure a shader author can hit today); P2 = the uniform-dispatch performance gap (closes spec 011's unmet SC-006, largest surface area); P3 = the `illuminance` delegation convergence (structural; nothing currently known to produce wrong output).
- Q: What counts as success for the uniform-dispatch performance work — is spec 011's "JIT ≤ 90% of interpreter wall-clock" a hard gate again? → A: No. The gate is *measurable improvement on every measured scene with zero output regression*; spec 011's 90% bar is restated here as a **stretch goal** whose per-scene attainment must be measured and reported, not as a pass/fail criterion. Rationale: the uniform-collapse change is the confirmed dominant cause of the shortfall but is not guaranteed to be the only one, and a spec that fails on a bar this single change may not reach alone would mis-describe the outcome.
- Q: How much process gating applies to the interpreter fix (the only interpreter-touching work in this feature)? → A: Pre-authorized in principle under the spec-011-amended FR-009 discipline, with a mandatory STOP: reproduction and root-cause investigation proceed freely, but the confirmed root cause and the proposed narrowest change MUST be presented to and approved by the maintainer before any interpreter source is modified.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - A shader that indexes a uniform string array with a varying index renders instead of crashing (Priority: P1)

A shader author writes RSL that reads an element of a `uniform string`
array using an index that varies per shading point — for example, choosing
a named material or texture channel per point from a fixed lookup table.
Today, rendering that shader with the interpreter backend does not produce
wrong pixels or a diagnostic: the renderer **crashes outright**, taking the
whole render with it. This story makes such a shader render to completion,
producing the same element-selection behavior the equivalent
uniform-indexed read already produces correctly.

**Why this priority**: This is the only one of the three issues that is an
outright failure rather than a performance or maintainability gap, and the
only one a shader author can hit today with no workaround other than
rewriting their shader. It is also the narrowest and most self-contained of
the three.

**Independent Test**: Render the existing minimal reproduction shader (a
handful of lines: a correctly-sized fixed-length `uniform string` array read
at a provably in-range index derived from a varying value) with the
interpreter backend. Before the fix, the render terminates abnormally;
after the fix, it completes and the selected-element behavior matches a
hand-computed expectation and the uniform-indexed equivalent.

**Acceptance Scenarios**:

1. **Given** a shader that reads a `uniform string` array element at a
   varying, in-range index, **When** it is rendered with the interpreter
   backend, **Then** the render completes normally (no abnormal
   termination) and produces the expected per-point element selection.
2. **Given** that same shader, **When** it is rendered with the JIT
   backend, **Then** its output matches the interpreter backend's output
   within the project's existing visual-regression tolerance.
3. **Given** the project's diagnostic shader for array element access,
   **When** the varying-index string-array read is restored to it and the
   visual regression suite is run, **Then** the suite passes — the
   construct is covered by an executing test, not only by the existing
   reachability-only coverage guard.
4. **Given** every other array element access form already known to work
   (matched-uniformity numeric reads, mismatched-uniformity numeric reads,
   and the uniform-index string read), **When** the full test suite is run
   after the fix, **Then** all of them still behave exactly as before.

---

### User Story 2 - Shaders dominated by uniform computation are not slower under the JIT backend than the interpreter (Priority: P2)

A shader author selects the JIT backend expecting it to be at least as fast
as the interpreter. For shaders whose work is dominated by computation that
is the *same for every shading point* (uniform computation), that
expectation is currently violated: the JIT repeats each such computation
once per shading point, while the interpreter performs it once. The more
uniform computation a shader contains, the worse the JIT compares — to the
point of being measurably *slower* than the interpreter. This story removes
that per-point repetition for uniform computation so the JIT's speed
advantage no longer degrades with uniform density.

**Why this priority**: This is the confirmed, documented root cause of the
one success criterion spec 011 did not meet, and it affects every shader
run under the JIT backend rather than one construct family — the largest
surface area of the three. It ranks below the crash because it degrades
performance rather than preventing rendering.

**Independent Test**: Using the fixed set of measurement scenes established
by spec 011, measure JIT and interpreter wall-clock rendering time for each
scene before and after the change, under identical scene and render
settings, and compare the JIT-to-interpreter ratios. Independently, compare
the rendered images before and after the change to confirm output is
unchanged.

**Acceptance Scenarios**:

1. **Given** each measurement scene from spec 011's fixed set, **When**
   rendering time is measured before and after this change, **Then** the
   JIT-to-interpreter wall-clock ratio improves for every scene, and
   improves by no less than zero (no scene regresses).
2. **Given** a shader whose work is dominated by uniform computation,
   **When** its JIT-to-interpreter ratio is compared against that of a
   shader with near-zero uniform computation, **Then** the gap between the
   two ratios is materially narrower after the change than before —
   demonstrating the fix addresses the uniform-density dependency
   specifically, not just overall throughput.
3. **Given** every measurement scene, **When** the images rendered before
   and after the change are compared using the project's established
   image-comparison methodology, **Then** no difference exceeds the
   same-configuration noise floor measured with the unchanged binary.
4. **Given** shading work that executes conditionally (only for a subset of
   shading points, e.g. inside a conditional or a light-iteration
   construct), **When** it is rendered after the change, **Then** its
   output is unchanged — the per-point active/inactive state that governs
   which points a computation applies to is preserved.
5. **Given** shading contexts where the number of shading points a
   computation is dispatched over differs from the number of real surface
   points (as occurs when surface derivatives are being computed), **When**
   they are rendered after the change, **Then** output is unchanged — the
   correct count is used at every dispatch site rather than one count being
   assumed to generalize.

---

### User Story 3 - Light iteration has exactly one implementation, shared by both backends (Priority: P3)

A maintainer fixes a bug in how the renderer iterates over lights for the
`illuminance` construct — for example, a light-culling or light-category
matching correction. Today, that fix lands in only one of two independently
written implementations of light iteration: one used by the interpreter
backend, one written separately by hand for the JIT backend. Nothing
detects that the other copy was missed. This story converges the two onto a
single shared implementation so a fix in one place is a fix everywhere.

**Why this priority**: Lowest of the three — nothing here is currently
known to produce wrong output, so this is structural risk reduction against
*future* divergence rather than a fix for a present defect. It is the same
class of work as spec 011's Story 5 (which converged the area / calculated
normal / depth / derivative helpers), applied to the one construct family
that spec 011's planning wrongly assumed was already converged.

**Independent Test**: Render shaders that use `illuminance` (including
category-filtered and non-filtered forms, with multiple light types)
under the JIT backend before and after the change and confirm output is
byte-for-byte unchanged, while confirming by inspection that only one
implementation of the light-iteration logic remains.

**Acceptance Scenarios**:

1. **Given** a shader using `illuminance` in any of its supported forms,
   **When** it is rendered with the JIT backend before and after this
   change, **Then** the output is unchanged.
2. **Given** the same shader, **When** it is rendered with the interpreter
   backend before and after this change, **Then** the output is unchanged —
   the interpreter's observable behavior is not altered by the
   convergence.
3. **Given** the converged implementation, **When** the light-iteration
   logic is inspected, **Then** exactly one implementation of it exists and
   both backends reach it, rather than two implementations kept in sync by
   hand.

---

### Edge Cases

- What if the interpreter crash's root cause turns out to require a change
  broader than the narrowest possible correction (for example, a shared
  data-representation change affecting other array access forms)? The
  mandatory STOP checkpoint applies: the finding and the proposed scope are
  presented for approval before any interpreter source is changed, and the
  broader change is not made unilaterally.
- What if the existing minimal reproduction shader is no longer present or
  no longer reproduces the crash? A reproduction MUST be re-established
  empirically before any fix is attempted; a root cause derived only from
  reading code is explicitly insufficient to authorize an interpreter
  change.
- What if the uniform-dispatch change lands measurable improvement but
  still leaves some scenes short of the stretch bar? That is an accepted
  outcome (see Clarifications). The per-scene results MUST be recorded,
  and any residual gap MUST be characterized (what still dominates) rather
  than left unexplained.
- What if the information needed to decide whether a given instruction is
  uniform is not currently available at the point the JIT builds its
  dispatch? Then making that information available is a prerequisite step
  of this story, not an assumption — its absence changes the size of the
  work but not its required outcome.
- What if converging light iteration onto a single implementation cannot be
  done without changing observable behavior? Then it stops being a pure
  refactor and becomes an interpreter-behavior change, which requires the
  same confirmed-defect process and STOP checkpoint as User Story 1 before
  proceeding.
- What if a scene's measured timing varies enough between runs to obscure a
  real improvement? The measurement method MUST establish a run-to-run
  variance baseline so a reported improvement is distinguishable from
  measurement noise.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: Rendering a shader that reads an element of a `uniform
  string` array at a varying, in-range index MUST complete normally under
  the interpreter backend and produce the correct per-point element
  selection, rather than terminating abnormally.
- **FR-002**: Before any interpreter source is changed under FR-001, the
  defect MUST be demonstrated by an empirical, repeatable reproduction —
  not inferred from code reading — and the confirmed root cause plus the
  proposed change MUST be presented for explicit maintainer approval.
- **FR-003**: Once FR-001 is satisfied, the varying-index string-array read
  MUST be exercised by an executing test in the project's regression suite,
  not solely by the existing reachability-only coverage guard.
- **FR-004**: For an instruction the compiler has classified as uniform
  (its result is identical for every shading point), the JIT backend MUST
  perform the underlying computation once rather than once per shading
  point, matching the interpreter's existing behavior for the same
  instruction.
- **FR-005**: The change made under FR-004 MUST NOT alter rendered output.
  For every measurement scene, images rendered before and after the change
  MUST agree within the same-configuration noise floor established with the
  unchanged binary.
- **FR-006**: The change made under FR-004 MUST preserve per-point
  active/inactive state semantics, so computation that applies to only a
  subset of shading points continues to apply to exactly that subset.
- **FR-007**: The change made under FR-004 MUST use the shading-point count
  that is correct for each individual dispatch site; it MUST NOT assume a
  single count generalizes across sites, given that the count a computation
  is dispatched over can exceed the number of real surface points in
  derivative-computing shading contexts.
- **FR-008**: After FR-004, the JIT backend's wall-clock rendering time
  relative to the interpreter's MUST improve, by more than the measured
  run-to-run variance, on every scene in spec 011's measurement set whose
  shader contains meaningful uniform computation; and MUST NOT regress on
  any scene. No measurable change on a scene with near-zero uniform
  computation is the expected, conforming outcome for that scene, not a
  failure.
- **FR-009**: Light iteration for the `illuminance` construct MUST have
  exactly one implementation, invoked by both backends, replacing the
  current pair of independently-written implementations — without changing
  either backend's observable output.
- **FR-010**: Every change delivered by this feature that computes a
  rendering result MUST do so by invoking the same underlying
  implementation the interpreter backend already uses, rather than
  re-deriving that logic independently for the JIT backend.
- **FR-011**: The interpreter (`.rslo`) backend remains the reference
  implementation throughout. Its behavior MUST NOT change except to fix a
  confirmed defect — demonstrated by an empirical reproduction, not
  inferred from code reading — under an explicit, maintainer-approved scope
  decision, using the narrowest change that corrects the specific defect
  (no incidental refactoring), and verified against the full existing test
  suite (`ctest -L libshader`, `ctest -L visual`) before and after to
  confirm zero unintended behavior change elsewhere.
- **FR-012**: Project documentation recording these three items as open
  issues MUST be updated to reflect their resolved state, including the
  record of spec 011's unmet performance criterion and its outcome under
  this feature.

### Key Entities

- **Shading backend**: One of two interchangeable engines that execute a
  compiled shader per shading point — the interpreter (`.rslo`,
  reference/ground-truth behavior) and the JIT (`.slo`, compiled native
  code). Selected per-scene or per-primitive.
- **Uniform classification**: The compiler's determination that an
  instruction's result is identical across all shading points in a batch
  (as opposed to varying per point). Already computed and already acted on
  by the interpreter; the basis for FR-004.
- **Shading point count**: The number of points a single dispatched
  computation covers. Distinct from the number of real surface points,
  which it can exceed in derivative-computing shading contexts — the
  asymmetry FR-007 guards against.
- **Active-point state**: The per-point record of which shading points a
  computation currently applies to, used to implement conditional and
  light-iteration execution. The semantics FR-006 must preserve.
- **Measurement set**: The fixed set of timing scenes established by spec
  011 (one per construct family it fixed), reused here as the before/after
  benchmark for FR-008 and the stretch bar.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The reproduction case for the varying-index string-array read
  terminates abnormally in at least 5 of 5 repeated runs before the fix, and
  completes normally in 100% of runs (minimum 5) after it. If the pre-fix
  behaviour proves intermittent rather than deterministic, the observed
  reproduction rate is recorded and the criterion is met by any non-zero
  pre-fix failure rate paired with a 100% post-fix pass rate.
- **SC-002**: The varying-index string-array read is covered by at least
  one executing regression test that passes after the fix and would fail if
  the defect returned.
- **SC-003**: The full existing regression suite (`ctest -L libshader` and
  `ctest -L visual`) shows zero newly-failing tests relative to its
  pre-change baseline, verified independently before and after each of the
  three changes this feature delivers.
- **SC-004**: The JIT-to-interpreter wall-clock ratio improves, by more than
  that scene's measured run-to-run variance, on 100% of the measurement
  scenes whose shader contains meaningful uniform computation, and 0
  measurement scenes regress. Scenes with near-zero uniform computation are
  expected to show no measurable change; that outcome is conforming, and
  each scene's uniform-computation density is recorded alongside its ratio
  so the classification is auditable rather than retrofitted.
- **SC-005**: The number of measurement scenes meeting spec 011's stretch
  bar (JIT wall-clock at most 90% of the interpreter's) is measured and
  reported per scene. This is a reported outcome, not a pass/fail gate;
  where the bar is not met, the residual dominant cost is identified.
- **SC-006**: The ratio gap between a uniform-computation-dominated shader
  and a near-zero-uniform-computation shader, measured at identical scene
  scale, narrows by a stated, measured amount — establishing that the
  uniform-density dependency itself was addressed.
- **SC-007**: Across all three changes, zero rendered-output differences
  exceed the same-configuration noise floor, measured with the project's
  established image-comparison methodology using an unchanged-binary
  baseline.
- **SC-008**: Exactly one implementation of light iteration for
  `illuminance` remains after this feature, reachable from both backends.

## Assumptions

- "The JIT backend" refers to openRender's `.slo` LLVM-JIT-compiled shader
  execution path, and "the interpreter" to its `.rslo` bytecode-interpreted
  path; both already ship. This feature changes JIT behavior and fixes one
  confirmed interpreter defect; the interpreter otherwise remains the
  reference.
- The three issues are independent: none blocks another, and each is
  separately deliverable and separately verifiable. They are combined into
  one feature because they share the same origin (spec 011), the same
  constraints (FR-010, FR-011), and the same verification apparatus.
- Spec 011's `perf-manual` measurement scenes and its 8×8 block-averaged
  image-comparison methodology (including the same-binary noise-floor
  baseline used for the stochastic raytrace hider) are reused unchanged as
  this feature's measurement apparatus; this feature does not redefine
  either.
- The performance work is expected to be dominated by, but not necessarily
  limited to, the uniform-dispatch cause identified in spec 011. Residual
  causes, if any, are to be characterized and reported (SC-005), not
  necessarily fixed within this feature.
- The `illuminance` convergence (User Story 3) is expected to be a
  behavior-preserving refactor of *how* the shared logic is reached; if
  investigation shows it cannot be done without changing observable
  behavior, it falls under FR-011's process rather than proceeding as a
  refactor.
- Whether the uniform classification of an instruction is already available
  at the JIT's dispatch-construction point is treated as an open question
  to be settled by investigation, not as an established fact. If it is not
  available, making it available is in scope as a prerequisite to FR-004.
- Work proceeds under checkpoint discipline consistent with spec 011:
  explicit maintainer confirmation between phases, and no automatic
  commits.
- This feature does not update the Hugo `site/` documentation (Constitution
  Principle VII) — it is internal engine defect-fix and performance work,
  not new user-facing functionality that the site's content model tracks.
  The applicable documentation updates under FR-012 are the project's
  internal notes (`DEVNOTES_DETAILS/BUGS.md`,
  `DEVNOTES_DETAILS/OSHADER_UPDATES.md`, and spec 011's records).
- This feature adds no new RSL language surface and changes no scene or
  shader validity rules.
