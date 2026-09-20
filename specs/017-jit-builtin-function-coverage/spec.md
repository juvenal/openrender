# Feature Specification: LLVM JIT Builtin-Function Coverage

**Feature Branch**: `017-jit-builtin-function-coverage`

**Created**: 2026-09-20

**Status**: Draft

**Input**: User description: "Fix the LLVM JIT builtin-function coverage gap (GitHub issue #3)." (full background: issue #1 fixed `random()`/`urandom()` silently no-op'ing under `--jit`, tracing the root cause to `llvmEmitter.cpp`'s `emitFunction()` silently skipping any RSL builtin function opcode absent from its `kHandledOpcodes[]` allowlist — zero LLVM IR emitted, no error, destination buffer never written. Filing issue #3 surfaced that 26 more RSL builtin functions hit this same gate, including `visibility()`/`transmission()`, which is what actually blocks issue #1's own repro scenes (`quadlight.rib`/`spherelight.rib`) from rendering correctly under `--jit` — a real JIT "show-stopper".)

## Clarifications

### Session 2026-09-20

- Q: How should this be scoped as a spec-kit feature, given the size (visibility/transmission/trace alone need brand-new ray-batch JIT machinery with no existing precedent)? → A: One spec, phased by user story — P1 (the functions that unblock shipped shaders, plus closing the defect class via gate-hardening and a coverage-guard extension), P2/P3 (the remaining 20, in decreasing real-world-impact order).
- Q: Should hardening the JIT's silent-skip gate into a build-time error be in this spec's scope, or left as a documented recommendation for later? → A: In scope — it's the actual root-cause fix for the defect *class* (not just today's functions), confirmed safe at the emitter level (only ever fires on opcodes already producing zero IR) and safe at the build level once the shipped-shader-blocking functions land (confirmed zero shipped shaders reference anything else missing).
- Q: All builds going forward should rely solely on the project's vcpkg-vendored toolchain (not Homebrew) — vcpkg's overlay triplets correctly target the project's actual minimum macOS deployment version (13.3), resolving the linker warnings seen building against Homebrew's LLVM (built for whatever SDK Homebrew's own CI happened to use). → Confirmed; local development for this feature uses the vcpkg toolchain exclusively.
- Q: Should every fixed function get its own persisted `.slo`-vs-`.rslo` equivalence regression test, added as each function lands, or is a final end-of-feature verification pass sufficient? → A: Per-function (or per-tightly-related-group) persisted `ctest -L visual` regression pairs, added at implementation time — not batched to the end.
- Q: Should this feature require the new JIT implementations (especially the P1 raytracing tier) to be measurably faster than the interpreter, mirroring spec 011's precedent? → A: No — correctness parity only. Spec 011's equivalent bar (JIT ≥10% faster than interpreter) is documented as never actually met project-wide; this feature's `FR-007` (avoid ~3× redundant ray tracing via the `numRealVertices` discipline) already prevents the most obvious performance regression as a correctness requirement, without gating the fix on a historically-unmet wall-clock target.
- Q: Should `quadlight.rib`/`spherelight.rib` (currently unregistered example scenes, not part of `ctest -L visual`) become permanent, CI-visible regression tests, or is a one-time manual verification sufficient? → A: Register them permanently — they are the real-world scenes that motivated issues #1 and #3; leaving them unregistered would let a future regression in `visibility()`/`transmission()`'s JIT path silently reopen the exact defect this feature exists to close, with nothing in CI to catch it.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Shipped shaders using `visibility()`/`transmission()`/`trace()`/`occlusion()`/`indirectdiffuse()`/`comp()` render correctly under the JIT (Priority: P1)

A shader author renders a scene that uses one of openRender's shipped area-light,
raytracing, or ambient-occlusion shaders (`quadlight`, `spherelight`,
`shadowarea`, `rayarea`, `raypoint`, `raydistant`, `basictrace`, `glass`,
`ambientocclusion`, `ambientindirect`) — or a shader of their own calling one
of these six builtin functions — with the JIT (`.slo`) shading backend
selected (`Attribute "shade" "shaderformat" ["slo"]`, or set as the scene/
compile-time default). Today, each of these six functions is silently
skipped by the JIT: the shader compiles and runs with no error, but the
function's result is simply never written, leaving stale/uninitialized data
in its place — the exact same failure mode issue #1 found and fixed for
`random()`/`urandom()`, now confirmed to also affect these six. This story
fixes all six so JIT output matches the interpreter's output for shaders
that use them, closing the actual blocker on issue #1's own repro scenes.

**Why this priority**: These are the only functions, of the 26 covered by
this feature, that any currently-shipped shader actually calls — this is
the concrete, user-visible "JIT show-stopper," not a theoretical gap.
`comp()` is the simplest of the six (a pure indexed read) but is bundled
into this story because it, too, has real shipped-shader impact
(`array_ops_probe.sl`, `matrix_ops_probe.sl`); the other five
(`visibility`/`transmission`/`trace`/`occlusion`/`indirectdiffuse`) are
substantially more involved — they are the first JIT-compiled code to ever
construct and consume a batch of traced rays or cache lookups, since every
already-JIT-handled builtin that resembles them (`shadow`, `texture`,
`environment`) is in fact a deterministic position→value lookup with no
raytracing, RNG, or per-vertex batching involved.

**Independent Test**: Render `examples/rib/quadlight.rib` and
`examples/rib/spherelight.rib` with `Attribute "shade" "shaderformat"
["slo"]` added, and confirm each now matches its `.rslo`-backed reference
image within the project's existing visual-regression tolerance (this is
the end-to-end closure criterion for issue #1's original repro) — as a
permanent, CI-visible `ctest -L visual` regression pair, not a one-time
manual check, so a future regression in these functions' JIT paths cannot
silently reopen this defect. In addition, for each of the six functions,
render a minimal shader exercising it in isolation, once via each backend,
and confirm the two match.

**Acceptance Scenarios**:

1. **Given** a shader that calls `visibility()` between two points, **When**
   rendered with the JIT backend, **Then** the output (an occlusion
   float) matches the interpreter backend within the project's existing
   visual-regression tolerance.
2. **Given** a shader that calls `transmission()` between two points,
   **When** rendered with the JIT backend, **Then** the resulting
   transmitted color matches the interpreter backend.
3. **Given** a shader that calls `trace()` (either its float/nearest-hit
   form or its color/reflection form), **When** rendered with the JIT
   backend, **Then** the result matches the interpreter backend.
4. **Given** a shader that calls `occlusion()` or `indirectdiffuse()`,
   **When** rendered with the JIT backend, **Then** the resulting
   occlusion/indirect-light value matches the interpreter backend.
5. **Given** a shader that reads a vector or matrix component via `comp()`,
   **When** rendered with the JIT backend, **Then** the read value matches
   the interpreter backend, for both the two-operand (vector) and
   three-operand (matrix) call forms.
6. **Given** a scene rendered under raytrace-derivative shading (where the
   JIT evaluates extra derivative-offset shading points alongside each real
   one), **When** any of `visibility`/`transmission`/`trace`/`occlusion`/
   `indirectdiffuse` is called, **Then** the underlying trace/cache lookup
   executes only once per real shading point (not once per derivative-offset
   point too), and its single result is copied into the derivative-offset
   positions — matching the interpreter's behavior exactly, including its
   effect of never producing a nonzero apparent derivative for these
   constructs.
7. **Given** `examples/rib/quadlight.rib`/`examples/rib/spherelight.rib`
   rendered with the JIT backend, **Then** the output matches the `.rslo`
   reference image within tolerance (no remaining checkerboard-noise or
   all-black artifact from any builtin function these shaders call), and
   this comparison is a permanent, persisted regression test rather than a
   one-time check.

---

### User Story 2 - A missing JIT builtin function can never again ship unnoticed (Priority: P1)

A maintainer changes or extends the JIT shading backend in the future, or a
gap pre-dating this feature is found later. If that change (or the
pre-existing gap) leaves some RSL builtin function unhandled, `oshader
--jit` itself fails to compile the affected shader, with a diagnostic
naming the specific unhandled function — instead of silently producing a
`.slo` that compiles and runs but quietly omits that function's effect, as
happened for `random()`/`urandom()` (issue #1) and the 26 functions this
feature inventories (issue #3). Independently, the project's automated test
suite also fails if a reachable builtin function lacks JIT handling, mirroring
the existing `OPCODE_`-only coverage guard's approach but extended to the
full `FUNCTION_`-family builtin set.

**Why this priority**: This is the actual root-cause fix for the defect
*class*, not just the functions this feature happens to name today — without
it, the exact same silent-failure pattern that produced two real,
independently-discovered bugs (`random()`/`urandom()`, then this feature's
26) can recur indefinitely. Sequenced after Story 1 specifically: confirmed
by direct inspection that, once Story 1's six functions are implemented,
zero currently-shipped shaders reference any of the remaining 20 functions
this feature also inventories — so hardening the compile-time gate at that
point causes no build breakage, whereas hardening it before Story 1 lands
would break the build for every shipped shader Story 1 fixes.

**Independent Test**: Deliberately introduce, in a local uncommitted
change, a call to a still-unhandled or newly-invented builtin function and
confirm `oshader --jit` fails to compile it with a message naming that
function, and separately confirm the test suite fails the same way.

**Acceptance Scenarios**:

1. **Given** a shader that calls a builtin function with no JIT handling,
   **When** `oshader --jit` compiles it, **Then** compilation fails with a
   diagnostic naming the specific unhandled function, and no `.slo` file is
   produced.
2. **Given** every builtin function this feature confirms JIT-handled,
   **When** the project is built from a clean tree, **Then** every shipped
   `.sl` file compiles to `.slo` successfully with no new build failures.
3. **Given** the project's automated test suite, **When** it is run after
   this feature, **Then** it fails, naming the specific unhandled function,
   if any reachable builtin function lacks JIT handling — including one
   introduced after this feature ships, since the check re-derives the
   full builtin-function set at test-run time rather than checking against
   a list frozen at the end of this feature.

---

### User Story 3 - Remaining raytracing/GI-adjacent builtin functions are available under the JIT (Priority: P2)

A shader author writes RSL using `photonmap()`, `rayinfo()`, `raylabel()`,
`raydepth()`, or `ptlined()` — none of which any currently-shipped shader
calls today, but each of which silently no-ops under `--jit` exactly like
Story 1's six functions did. This story fixes all five so JIT output
matches the interpreter for shaders using them.

**Why this priority**: Lower priority than Story 1 because nothing
currently shipped depends on these — but still real, silently-wrong
behavior today for any shader author who does use them, and closing this
gap is far simpler than Story 1's raytracing tier (no ray-batch
construction beyond `photonmap()`'s existing kd-tree lookup; three of the
five are single-field reads with no batching or state at all).

**Independent Test**: For each of the five functions, render a minimal
shader exercising it in isolation, once via each backend, and confirm the
two match.

**Acceptance Scenarios**:

1. **Given** a shader that calls `photonmap()` (either its two- or
   three-argument overload), **When** rendered with the JIT backend,
   **Then** the result matches the interpreter backend, including the same
   once-per-real-vertex-then-replicate behavior under derivative shading as
   Story 1's raytracing-tier functions.
2. **Given** a shader that calls `rayinfo()` with any of its five supported
   query strings (`"label"`, `"depth"`, `"origin"`, `"direction"`,
   `"length"`), **When** rendered with the JIT backend, **Then** the result
   matches the interpreter backend.
3. **Given** a shader that calls `raylabel()` or `raydepth()`, **When**
   rendered with the JIT backend, **Then** the result matches the
   interpreter backend.
4. **Given** a shader that calls `ptlined()` to compute a point-to-line-
   segment distance, **When** rendered with the JIT backend, **Then** the
   result matches the interpreter backend.

---

### User Story 4 - Remaining pure math/string builtin functions are available under the JIT (Priority: P3)

A shader author writes RSL using any of the remaining 14 builtin functions
this feature inventories — `degrees`, `determinant`, `distance`, `match`,
`min`, `refract`, `rotate`, `round`, `scale`, `setcomp`, `step`,
`translate`, `concat`, `format` — none of which any currently-shipped
shader calls today, and each of which is a pure function of its operands
with no raytracing or per-thread state involved. This story fixes all 14
so JIT output matches the interpreter for shaders using them.

**Why this priority**: Lowest priority — no shipped shader is affected
today, and each of these is the smallest, most mechanical class of fix in
this feature (every needed math primitive already exists and is already
reachable from the JIT runtime; each fix is expected to closely follow an
already-JIT-handled sibling function as a template).

**Independent Test**: For each function (or small group of closely-related
functions sharing one probe shader, where that materially reduces
redundant test infrastructure without reducing coverage), render a minimal
shader exercising it, once via each backend, and confirm the two match.

**Acceptance Scenarios**:

1. **Given** a shader that calls any of this story's 14 functions, **When**
   rendered with the JIT backend, **Then** the result matches the
   interpreter backend — including reproducing, not "fixing", any
   pre-existing interpreter quirk in a given function's current behavior
   (e.g. `match()`'s plain-string-equality semantics, `round()`'s
   truncating-cast semantics, `min()`'s two-argument-only support mirroring
   `max()`'s existing same limitation), since the interpreter is this
   feature's reference implementation throughout.

---

### Edge Cases

- What happens for a builtin function call that appears in a shader source
  file but is never actually executed at render time (e.g. inside a branch
  never taken for any rendered scene)? Once Story 2's gate-hardening lands,
  `oshader --jit` still fails to compile that shader — the hardened check
  is a static, whole-shader compile-time check, not a dynamic/runtime one;
  a call site's mere presence in the compiled IR is sufficient to trigger
  it, independent of whether it is ever reached at shading time. This
  matches the existing, unchanged behavior of every other class of
  compile-time diagnostic `oshader` already produces.
- What happens when a shader relies on the interpreter's `Oi = 1;`
  documented workaround from a pre-existing probe shader's now-stale
  comment (which claimed the JIT does not default `Oi` to opaque)? That
  claim no longer holds — confirmed via the project's existing
  `usedParameters` gating tests (`test_used_parameters_gating.cpp`,
  `test_used_parameters_oracle.cpp`), which already pass and already cover
  exactly this scenario. New probe shaders added by this feature do not
  need that workaround; this feature does not change that prior fix.
- What happens to a shader author who uses the trailing optional named-
  argument form of `visibility()`/`transmission()` (the `"!"`-suffixed
  prototype extension, e.g. `visibility(P, D, "samples", 4)`)? Confirmed
  by inspection that no currently-shipped shader uses this form (one
  commented-out example exists, unused). This feature's JIT implementation
  supports only the plain positional-argument form; the optional
  named-argument extension remains unsupported under the JIT and is
  explicitly out of scope for this feature — a future feature would need to
  either implement the JIT-side equivalent of the interpreter's PL-cache
  binding machinery or take a different approach.
- What happens to the two `DSO` (dynamically-loaded shadeop plugin)
  dispatcher table rows that share the literal placeholder name `"XXX"` in
  issue #3's original inventory? Confirmed these are not a 27th missing
  function — `"XXX"` is a placeholder name/prototype string used because a
  real DSO call's actual name/prototype is only known once the plugin is
  loaded at runtime, not statically at compile time. Full DSO/plugin JIT
  support is a distinct, unimplemented, orthogonal feature and is out of
  scope for this feature entirely (not deferred to a later story).

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The JIT shading backend MUST produce output matching the
  interpreter backend, within the project's existing visual-regression
  tolerance, for shaders calling `visibility()`.
- **FR-002**: The JIT shading backend MUST produce output matching the
  interpreter backend for shaders calling `transmission()`.
- **FR-003**: The JIT shading backend MUST produce output matching the
  interpreter backend for shaders calling `trace()`, in both its float
  (nearest-hit distance) and color (reflected radiance) forms.
- **FR-004**: The JIT shading backend MUST produce output matching the
  interpreter backend for shaders calling `occlusion()`.
- **FR-005**: The JIT shading backend MUST produce output matching the
  interpreter backend for shaders calling `indirectdiffuse()`.
- **FR-006**: The JIT shading backend MUST produce output matching the
  interpreter backend for shaders calling `comp()`, in both its
  vector-indexing and matrix-indexing call forms.
- **FR-007**: For every function covered by FR-001 through FR-005, the JIT
  implementation MUST evaluate the underlying trace/cache-lookup operation
  only once per real shading point during raytrace-derivative shading (not
  once per derivative-offset point as well), and MUST copy that single
  result into the corresponding derivative-offset destination positions —
  matching the interpreter's existing behavior for these functions exactly,
  including its effect of always producing an exact-zero apparent
  derivative for them.
- **FR-008**: `oshader --jit` MUST fail to compile a shader containing a
  call to a builtin function with no JIT implementation, producing a
  diagnostic that names the specific unhandled function and producing no
  `.slo` output file for that shader — rather than compiling successfully
  and silently omitting that function's effect.
- **FR-009**: FR-008's compile-time failure MUST NOT be enabled until zero
  currently-shipped shaders in the project would newly fail to build under
  it — i.e., only once every builtin function any shipped shader currently
  calls (FR-001 through FR-006's six functions) has JIT support.
- **FR-010**: The project's automated test suite MUST independently fail,
  with a message identifying the specific unhandled function, if any
  builtin function that the compiler can actually produce is left without
  JIT handling — re-deriving the full reachable builtin-function set at
  test-run time (not from a list frozen at the end of this feature), so it
  also catches functions introduced after this feature ships. This is a
  test-suite-time check, independent of and in addition to FR-008/FR-009's
  build-time check.
- **FR-011**: The JIT shading backend MUST produce output matching the
  interpreter backend for shaders calling `photonmap()` (both its
  two-argument and three-argument overloads), following FR-007's
  once-per-real-vertex-then-replicate discipline.
- **FR-012**: The JIT shading backend MUST produce output matching the
  interpreter backend for shaders calling `rayinfo()`, for all five of its
  supported query strings.
- **FR-013**: The JIT shading backend MUST produce output matching the
  interpreter backend for shaders calling `raylabel()` or `raydepth()`.
- **FR-014**: The JIT shading backend MUST produce output matching the
  interpreter backend for shaders calling `ptlined()`.
- **FR-015**: The JIT shading backend MUST produce output matching the
  interpreter backend for shaders calling any of: `degrees`, `determinant`,
  `distance`, `match`, `min`, `refract`, `rotate`, `round`, `scale`,
  `setcomp`, `step`, `translate`, `concat`, `format`.
- **FR-016**: Every fix delivered under FR-001 through FR-006 and FR-011
  through FR-015 MUST compute its result by invoking the same underlying
  implementation the interpreter backend already uses for that function,
  not by re-implementing the function's logic independently for the JIT.
- **FR-017**: The interpreter (`.rslo`) backend remains the reference
  implementation throughout this feature and its behavior MUST NOT change,
  including any pre-existing quirk in a given function's current behavior
  (e.g. `match()`'s plain-string-equality semantics, `round()`'s
  truncating-cast semantics, `min()`'s/`max()`'s shared two-argument-only
  support) — every JIT fix under this feature mirrors the interpreter's
  actual current behavior exactly, not a corrected or idealized version of
  it.
- **FR-018**: Every function fixed under this feature MUST have a
  persisted, automated `.slo`-vs-`.rslo` equivalence regression test
  (following the project's existing probe-shader/paired-RIB-scene
  pattern), added at the time that function is implemented rather than
  deferred to the end of the feature.
- **FR-019**: `examples/rib/quadlight.rib` and `examples/rib/spherelight.rib`
  MUST be registered as permanent, persisted `.slo`-vs-`.rslo` regression
  tests under this feature — not verified once and left unregistered —
  since they are the real-world scenes that motivated issues #1 and #3.

### Key Entities

- **Shading backend**: One of two interchangeable engines that execute a
  compiled shader per-point during rendering — the interpreter (`.rslo`,
  reference/ground-truth behavior) and the JIT (`.slo`, compiled native
  code). Selected per-scene or per-primitive.
- **RSL builtin function**: A named, callable RenderMan Shading Language
  function (as distinct from a bytecode-level operator) that the compiler
  lowers into an intermediate call instruction consumed by both backends —
  the unit this feature's inventory (26 functions) is organized around.
- **Coverage gate**: The JIT compiler's existing single point of dispatch
  that decides, per instruction, whether JIT code is emitted for it; this
  feature extends and eventually hardens this gate rather than replacing
  it.
- **Coverage guard**: The automated, test-suite-time check (existing for
  bytecode operators, extended by this feature to builtin functions) that
  fails the test process when a reachable construct has no JIT handling.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: 100% of the ten shipped shaders that call one of Story 1's
  six functions (`quadlight`, `spherelight`, `shadowarea`, `rayarea`,
  `raypoint`, `raydistant`, `basictrace`, `glass`, `ambientocclusion`,
  `ambientindirect`) produce JIT output matching their interpreter output
  within the project's existing visual-regression tolerance.
- **SC-002**: `examples/rib/quadlight.rib` and `examples/rib/spherelight.rib`,
  rendered with the JIT backend, match their `.rslo`-backed reference
  images within tolerance, as permanent `ctest -L visual` entries — the
  concrete, CI-enforced closure criterion for issue #1's original repro.
- **SC-003**: A deliberately-introduced unhandled builtin function call
  fails both `oshader --jit` compilation (with a naming diagnostic) and the
  automated test suite (with a naming diagnostic) 100% of the time, with
  zero manual/visual inspection required to detect it.
- **SC-004**: After Story 2 lands, `cmake --build` from a clean tree
  succeeds with zero new failures — confirming the compile-time hardening
  introduced no build breakage for any currently-shipped shader.
- **SC-005**: 100% of the 26 builtin functions this feature inventories
  (6 in Story 1, 5 in Story 3, 15 in Story 4 — `comp` counted once, in
  Story 1) have a passing, persisted JIT-vs-interpreter equivalence
  regression test under `ctest -L visual`.
- **SC-006**: Zero instances remain, after this feature, of a JIT-side fix
  under this feature re-implementing math or trace logic that already
  exists in the interpreter's own implementation, rather than reusing it.

## Assumptions

- "The JIT shading backend" refers to openRender's `.slo` LLVM-JIT-compiled
  shader execution path, and "the interpreter" refers to its `.rslo`
  bytecode-interpreted path; these are existing, already-shipping backends
  — this feature changes JIT behavior only, bringing it into parity with
  the interpreter, which remains the reference (see FR-017).
- "Visual-regression tolerance" refers to the project's existing image-diff
  comparison thresholds already used to validate rendering changes; this
  feature does not change that tolerance, only adds/passes comparisons
  under it.
- This feature's builtin-function inventory (26 functions across Stories 1,
  3, and 4) is drawn from GitHub issue #3's original list minus `"XXX"`
  (confirmed a DSO-dispatch placeholder, not a real function — see Edge
  Cases) and minus `random`/`urandom` (already fixed by issue #1, merged
  prior to this feature).
- Development and verification for this feature use the project's
  vcpkg-vendored toolchain (LLVM, PNG, TIFF, zlib, OpenEXR all resolved
  under `VCPKG_INSTALLED_DIR`) exclusively, not Homebrew — per Clarifications
  above, this is the standing local-build convention for this feature and
  beyond, not a one-off choice specific to a single build.
- The optional trailing named-argument extension to `visibility()`/
  `transmission()`'s prototypes (the `"!"`-suffixed form) is explicitly out
  of scope for FR-001/FR-002 — confirmed unused by any currently-shipped
  shader (see Edge Cases); the JIT implementation covers the plain
  positional-argument form only.
- This feature does not add new RSL language surface, new shading
  built-ins, or change what scenes/shaders are considered valid — it only
  brings existing, already-valid RSL builtin functions to parity between
  the two already-shipping backends, matching issue #1's precedent and
  spec 011's established constraint for this class of work.
- This feature does not update the Hugo `site/` documentation — it is an
  internal engine parity/bug-fix, not new user-facing functionality that
  the site's content model tracks, matching spec 011's precedent for the
  same reasoning.
- This feature carries no wall-clock performance requirement or success
  criterion — correctness parity with the interpreter (FR-001 through
  FR-016) is the only bar. FR-007's `numRealVertices` discipline is a
  correctness requirement (matching interpreter behavior exactly, which
  happens to also avoid ~3× redundant ray tracing), not a performance
  target in its own right; no fixed function needs to be benchmarked or
  shown faster than the interpreter to satisfy this feature.
