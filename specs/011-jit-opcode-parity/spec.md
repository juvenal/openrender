# Feature Specification: LLVM JIT Opcode-Coverage Parity Sweep

**Feature Branch**: `011-jit-opcode-parity`

**Created**: 2026-08-14

**Status**: Draft

**Input**: User description: "LLVM JIT (.slo) opcode-coverage parity sweep with the .rslo interpreter." (full background: investigation of the documented `cfrom` silent-drop bug in `DEVNOTES_DETAILS/BUGS.md`, plus a broader audit of the LLVM JIT shading backend for additional opcodes that are silently dropped or silently mis-lowered relative to the `.rslo` interpreter, which is the RenderMan-spec reference implementation.)

## Clarifications

### Session 2026-08-14

- Q: Should the coverage guard (FR-006/Story 4) keep working automatically if someone adds a brand-new RSL construct after this feature ships, or is it only required to catch the specific constructs this feature's inventory (FR-004) already found? → A: Dynamic (re-derives the reachable-opcode set each run and fails on any future gap too), executed as part of the test suite, not the build.
- Q: Does the JIT backend need to keep its performance advantage over the interpreter for the constructs this feature fixes, or is matching correctness the only requirement even if a fix makes the JIT slower for that construct? → A: Yes — for every fixed construct, JIT rendering time for a given shader MUST be at least 10% faster than the interpreter's rendering time for the same shader (JIT time ≤ 90% of interpreter time).

### Session 2026-08-21

- Q: During Phase 7 (US3b, `gather()`) verification, the JIT-side fix required threading strides through `op_gatherHeader` so a `CUniformLiftingPass`-promoted single-element uniform operand isn't walked past across a multi-vertex batch. Since `.rslo` is a separate implementation of the same opcode family, is it in scope to investigate whether the interpreter shares this defect class, given FR-009 currently forbids any interpreter change? → A: Amend FR-009. The interpreter remains the reference implementation and its behavior MUST NOT change except for a confirmed defect (empirically reproduced, not inferred), fixed under an explicit, user-approved scope decision, with the narrowest possible change and full regression coverage. See Phase 7a in `tasks.md` for the investigation this authorizes.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Explicit-colorspace color/matrix constructors render correctly under JIT (Priority: P1)

A shader author writes RSL that uses the explicit-colorspace color constructor
(`color "hsv" (h, s, v)`), the equivalent matrix constructor, or the
`ctransform()` builtin, then renders a scene with the JIT (`.slo`) shading
backend selected (the default backend unless overridden). Today, the color
and matrix constructors are silently dropped — the shader compiles and runs
with no error, but the assigned value never reaches `Ci`/`Oi`, producing
wrong pixels with no diagnostic. `ctransform()` compiles and runs but
silently computes the wrong result (a geometric transform instead of a
colorspace conversion). This story fixes those three constructs so JIT
output matches the interpreter's output for the same shader.

**Why this priority**: This is the specific, already-reported, user-facing
defect (`DEVNOTES_DETAILS/BUGS.md`) that motivated this feature. It's the
smallest fully-testable slice and unblocks removing the documented
workaround (pinning `Option "shaderformat" "default" ["rslo"]`).

**Independent Test**: Render a bare-sphere scene with a diagnostic shader
that writes `Ci` via `color "hsv" (...)`, once with the JIT backend selected
and once with the interpreter backend selected, using otherwise-identical
scene state. Before the fix, the two renders differ; after the fix, they
match within the existing visual-regression tolerance.

**Acceptance Scenarios**:

1. **Given** a shader that assigns `Ci` using the explicit-colorspace color
   constructor, **When** rendered with the JIT backend, **Then** the output
   pixel colors match a render of the same shader with the interpreter
   backend, within the project's existing visual-regression tolerance.
2. **Given** a shader that assigns a matrix using the explicit-space matrix
   constructor, **When** rendered with the JIT backend, **Then** the
   resulting matrix-dependent output matches the interpreter backend.
3. **Given** a shader that calls `ctransform()` to convert a color between
   two named colorspaces, **When** rendered with the JIT backend, **Then**
   the resulting color matches the interpreter backend (not a
   geometrically-transformed value).

---

### User Story 2 - Every opcode the JIT can silently mishandle is inventoried, and the true gap is known (Priority: P1)

Before more fixes are made, the actual (not speculative) set of RSL language
constructs that the JIT drops or mis-lowers must be known with confidence.
A raw textual comparison of the interpreter's opcode list against the JIT's
dispatch code overstates the gap (some listed names are never actually
produced by the compiler, due to internal formatting/casing differences),
so a shader author or maintainer needs a verified list — with a working
minimal example per gap — before further work is prioritized or trusted.

**Why this priority**: Every other story in this feature depends on this
inventory being accurate; committing to fixes for gaps that turn out to be
unreachable wastes effort, and missing a gap that IS reachable means a
future user hits it exactly the way `cfrom` was hit — silently, with no
error, discovered by accident.

**Independent Test**: Produce a list of confirmed-reachable, currently
unhandled language constructs, each backed by a minimal shader that
demonstrably exercises it in the compiled output. The list can be reviewed
and spot-checked independently of any fix being implemented yet.

**Acceptance Scenarios**:

1. **Given** the full set of RSL-level constructs suspected of having no
   JIT handling, **When** each is exercised by a minimal shader and
   compiled, **Then** the inventory records, per construct, whether it is
   actually reachable (produced by the compiler) or not.
2. **Given** a construct confirmed reachable and unhandled, **When** it is
   rendered under the JIT backend, **Then** the inventory records the
   observed failure mode (value silently dropped, value silently wrong, or
   a hard compile/render failure).

---

### User Story 3 - Confirmed-reachable gaps beyond the color/matrix family are fixed (Priority: P2)

Once User Story 2 has produced a verified list, every construct on it —
matrix arithmetic, the `gather()` global-illumination construct,
comparison/logic operators, and array element access — is fixed so JIT
output matches the interpreter for shaders that use them.

**Why this priority**: This is the "full sweep" the user chose over the
narrower P1-only fix — broader parity coverage, but dependent on Story 2's
inventory to know what's actually in scope, and lower priority than the
already-diagnosed P1 defect.

*(For task planning, this story is split into a lower-risk sub-phase covering
matrix arithmetic/comparison-logic/array access, and a higher-risk sub-phase
for `gather()`'s CFG scaffolding — see tasks.md Phases 6-7.)*

**Independent Test**: For each confirmed-reachable construct, render a
minimal shader exercising it under both backends and confirm the outputs
match.

**Acceptance Scenarios**:

1. **Given** a shader using matrix arithmetic (e.g. multiplying two
   matrices, or a matrix by a point), **When** rendered with the JIT
   backend, **Then** the output matches the interpreter backend.
2. **Given** a shader using `gather()` for ambient occlusion or indirect
   illumination sampling, **When** rendered with the JIT backend, **Then**
   the output matches the interpreter backend.
3. **Given** a shader using comparison or boolean-logic operators on
   vector, matrix, or float values to drive a conditional, **When**
   rendered with the JIT backend, **Then** the branch taken (and therefore
   the output) matches the interpreter backend.
4. **Given** a shader reading or writing individual elements of an array
   value, **When** rendered with the JIT backend, **Then** the output
   matches the interpreter backend.

---

### User Story 4 - A silently-dropped construct can never again ship unnoticed (Priority: P2)

A maintainer changes or extends the JIT shading backend in the future. If
that change (or a pre-existing gap not yet found) leaves some RSL construct
unhandled, the build or test suite fails with a clear message identifying
the construct — instead of the renderer silently producing wrong pixels
that only get noticed by a shader author much later, as happened with
`cfrom`.

**Why this priority**: Directly requested by the project maintainer as a
first-class deliverable of this feature — the payoff of Stories 1-3 is
undermined if the same silent-failure class can reopen unnoticed.
Independent of Story 3's specific fixes, but most valuable once Story 2's
verified inventory exists to check against.

*(For task planning, this story is split into two sub-phases: an early
Red-state coverage-guard test authored before Story 1/3 fixes, and a later
Green-state finalization once those fixes land — see tasks.md Phases 4 and 8.)*

**Independent Test**: Deliberately introduce (in a local, uncommitted
change) a new reachable construct with no JIT handling — including one that
did not exist in this feature's original inventory — and confirm the test
suite fails with a message identifying the specific unhandled construct,
rather than succeeding silently.

**Acceptance Scenarios**:

1. **Given** a reachable RSL construct with no corresponding JIT handling,
   **When** the project's test suite is run, **Then** it fails with a
   message naming the specific unhandled construct.
2. **Given** every construct confirmed reachable and now handled by this
   feature, **When** the project's test suite is run, **Then** it passes.
3. **Given** a reachable RSL construct introduced after this feature ships
   (not part of this feature's original inventory) that lacks JIT handling,
   **When** the project's test suite is run, **Then** it still fails with a
   message naming that construct — the guard re-derives the reachable-opcode
   set at run time rather than checking against a list frozen at the end of
   this feature.

---

### User Story 5 - JIT-only helper logic stops silently duplicating interpreter logic (Priority: P3)

Several small pieces of shading math (surface area, calculated normal,
depth, and surface derivatives) are currently implemented twice: once in
the interpreter, once (separately, by hand) for the JIT. A maintainer
fixing a bug in one implementation today has no automatic guarantee the
other implementation is fixed too. This story converges the JIT-side copies
onto the interpreter's implementation so there is exactly one implementation
of each, called from both places.

**Why this priority**: Lowest priority — nothing here is currently known to
produce wrong output (unlike Stories 1 and 3); this is a structural
risk-reduction change that prevents *future* silent divergence, requested
by the maintainer as directly in the spirit of this feature's core
constraint.

**Independent Test**: For each of the five duplicated helpers, render a
shader exercising it under the JIT backend before and after the change and
confirm output is unchanged (since the two implementations are not
currently known to disagree) while confirming, by inspection, that only one
implementation of the underlying math remains.

**Acceptance Scenarios**:

1. **Given** a shader using `area()`, `calculatenormal()`, `depth()`,
   `Du()`, or `Dv()`, **When** rendered with the JIT backend before and
   after this change, **Then** the output is identical.

---

### Edge Cases

- What happens when a shader uses a construct the inventory step (Story 2)
  determines is *not* actually reachable through the compiler? It is
  explicitly out of scope for a fix — the feature must not attempt to
  handle language surface that the compiler itself never produces.
- What happens when a confirmed-reachable gap can't cleanly delegate to an
  existing interpreter function (e.g. because the interpreter's own
  implementation is inlined/macro-based rather than a callable function)?
  The fix must still avoid re-deriving the underlying math independently;
  at minimum, the same formula/constants (e.g. shared epsilon thresholds)
  must be reused, not re-derived by a second, potentially-diverging path.
- What happens for `gather()` specifically, where the current failure mode
  (silent wrong output vs. an outright build/render failure) is not yet
  confirmed? Story 2's inventory step must establish which failure mode
  applies before Story 3 commits to a specific fix shape for it.
- What happens to the documented workaround (pinning the interpreter
  backend via `Option "shaderformat"`) once a construct is fixed? It
  becomes unnecessary for that construct but must remain valid — the
  interpreter backend is unaffected by this feature and stays available as
  an explicit choice.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The JIT shading backend MUST produce output matching the
  interpreter backend, within the project's existing visual-regression
  tolerance, for shaders using the explicit-colorspace color constructor.
- **FR-002**: The JIT shading backend MUST produce output matching the
  interpreter backend for shaders using the explicit-space matrix
  constructor.
- **FR-003**: The JIT shading backend MUST produce output matching the
  interpreter backend for shaders using `ctransform()`, including both its
  single-source-space and explicit-from/to-space forms.
- **FR-004**: An inventory MUST exist, before any construct beyond FR-001
  through FR-003 is fixed, that classifies every candidate RSL-level
  construct suspected of lacking JIT support as either confirmed-reachable
  (with a minimal demonstrating shader) or not reachable through the
  current compiler.
- **FR-005**: For every construct FR-004 confirms reachable, the JIT
  shading backend MUST produce output matching the interpreter backend.
  This explicitly covers, at minimum: matrix arithmetic and constructors,
  the `gather()` construct, comparison/boolean-logic operators, and array
  element access — pending confirmation of reachability per FR-004.
- **FR-006**: The project's automated test suite MUST fail, with a message
  identifying the specific unhandled construct, if any RSL construct that is
  actually reachable through the compiler is left without JIT handling. This
  check MUST re-derive the reachable-opcode set at test-run time (not from a
  list frozen at the end of this feature), so it also catches constructs
  introduced after this feature ships. This is a test-suite-time check, not
  a build/compile-time check.
- **FR-007**: Every fix delivered under FR-001, FR-002, FR-003, and FR-005
  MUST compute its result by invoking the same underlying implementation
  the interpreter backend already uses for that construct, not by
  re-implementing the construct's logic independently for the JIT.
- **FR-008**: The JIT-side implementations of surface area, calculated
  normal, depth, and the `Du()`/`Dv()` surface derivatives MUST be
  converged onto the interpreter's existing implementations, eliminating
  the current independent, duplicate JIT-side implementations.
- **FR-009**: The interpreter (`.rslo`) backend remains the reference
  implementation throughout, and its behavior MUST NOT change as a result
  of this feature *except* to fix a confirmed defect — one demonstrated by
  an empirical repro, not inferred from code reading alone — under an
  explicit, user-approved scope decision (see Clarifications, session
  2026-08-21). Any such fix MUST be the narrowest change that corrects the
  specific defect (no incidental refactoring) and MUST be verified against
  the full existing test suite (`ctest -L libshader`, `ctest -L visual`) to
  confirm zero unintended behavior change elsewhere in the interpreter.
- **FR-010**: Existing project documentation that describes JIT opcode
  coverage MUST be corrected where it is currently inaccurate (e.g. claims
  of a warning mechanism for unhandled constructs, or claims about a
  symbol-retention mechanism, that do not match the current implementation).
- **FR-011**: For every construct fixed under FR-001, FR-002, FR-003, and
  FR-005, the JIT backend's rendering time for a shader exercising that
  construct MUST be at least 10% faster than the interpreter backend's
  rendering time for the same shader (JIT time ≤ 90% of interpreter time).
  A fix that meets FR-007's delegation constraint but fails this bar is not
  complete.

### Key Entities

- **Shading backend**: One of two interchangeable engines that execute a
  compiled shader per-point during rendering — the interpreter (`.rslo`,
  reference/ground-truth behavior) and the JIT (`.slo`, compiled native
  code). Selected per-scene or per-primitive.
- **RSL construct**: A unit of Renderman Shading Language surface syntax
  (an operator, built-in function, or language feature) that the compiler
  lowers into an intermediate form consumed by both backends.
- **Reachability inventory**: The record, produced by Story 2, of which
  suspected-unhandled constructs the compiler actually produces in
  practice, each with a minimal demonstrating shader.
- **Coverage guard**: The automated check, produced by Story 4, that fails
  the build/test process when a reachable construct has no JIT handling.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For the color/matrix constructor family (FR-001 through
  FR-003), 100% of the demonstrating shaders produce output matching the
  interpreter backend, and the previously-documented workaround is no
  longer necessary for those constructs.
- **SC-002**: The reachability inventory (FR-004) covers 100% of the
  originally-suspected candidate constructs, with each classified and,
  where reachable, backed by a passing demonstrating shader.
- **SC-003**: 100% of constructs the inventory confirms reachable produce
  JIT output matching the interpreter backend.
- **SC-004**: A deliberately-introduced unhandled reachable construct,
  including one introduced after this feature ships, is caught by the
  automated test suite 100% of the time, with zero manual/visual inspection
  required to detect it.
- **SC-005**: Zero instances remain, after this feature, of JIT-side
  shading math (area, calculated normal, depth, Du/Dv) being computed by a
  separate implementation from the interpreter's.
- **SC-006**: For 100% of the constructs fixed under FR-001, FR-002, FR-003,
  and FR-005, measured JIT rendering time for a demonstrating shader is at
  most 90% of the interpreter's rendering time for the same shader.

## Assumptions

- "The JIT shading backend" refers to openRender's `.slo` LLVM-JIT-compiled
  shader execution path, and "the interpreter" refers to its `.rslo`
  bytecode-interpreted path; these are existing, already-shipping backends
  — this feature changes JIT behavior only, bringing it into parity with
  the interpreter, which remains the reference.
- "Visual-regression tolerance" refers to the project's existing image-diff
  comparison thresholds already used to validate rendering changes; this
  feature does not change that tolerance, only adds/passes comparisons
  under it.
- The reachability inventory (Story 2) may find that some originally
  suspected constructs are not actually produced by the compiler in any
  circumstance; those are explicitly excluded from the fix scope (FR-005)
  rather than fixed speculatively.
- `gather()`'s current failure mode is not yet confirmed (silent wrong
  output vs. hard failure); this is deliberately left for the inventory
  step rather than assumed here.
- This feature does not add new RSL language surface, new shading
  built-ins, or change what scenes/shaders are considered valid — it only
  brings existing, already-valid RSL constructs to parity between the two
  already-shipping backends.
- The FR-011/SC-006 performance bar (JIT ≤ 90% of interpreter rendering
  time) is measured per demonstrating shader using the project's existing
  render-timing/benchmarking approach, under otherwise-identical scene and
  render settings for both backends; it is evaluated per fixed construct,
  not as an aggregate across the whole feature.
- This feature does not update the Hugo `site/` documentation (Constitution
  Principle VII) — it is an internal engine parity/bug-fix, not new
  user-facing functionality that the site's content model tracks;
  `DEVNOTES_DETAILS/BUGS.md` and `CLAUDE.md` (FR-010) are the applicable
  internal-doc corrections instead.
