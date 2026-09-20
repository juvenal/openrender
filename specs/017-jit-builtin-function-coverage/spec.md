# Feature Specification: LLVM JIT Builtin-Function Coverage

**Feature Branch**: `017-jit-builtin-function-coverage`

**Created**: 2026-09-20

**Status**: Draft

**Input**: User description: "Fix the LLVM JIT builtin-function coverage gap (GitHub issue #3)." (full background: issue #1 fixed `random()`/`urandom()` silently no-op'ing under `--jit`, tracing the root cause to `llvmEmitter.cpp`'s `emitFunction()` silently skipping any RSL builtin function opcode absent from its `kHandledOpcodes[]` allowlist — zero LLVM IR emitted, no error, destination buffer never written. Filing issue #3 surfaced that 25 more RSL builtin functions hit this same gate, including `visibility()`/`transmission()`, which is what actually blocks issue #1's own repro scenes (`quadlight.rib`/`spherelight.rib`) from rendering correctly under `--jit` — a real JIT "show-stopper".)

## Clarifications

### Session 2026-09-20

- Q: How should this be scoped as a spec-kit feature, given the size (visibility/transmission/trace alone need brand-new ray-batch JIT machinery with no existing precedent)? → A: One spec, phased by user story — P1 (the functions that unblock shipped shaders, plus closing the defect class via gate-hardening and a coverage-guard extension), P2/P3 (the remaining 19, in decreasing real-world-impact order).
- Q: Should hardening the JIT's silent-skip gate into a build-time error be in this spec's scope, or left as a documented recommendation for later? → A: In scope — it's the actual root-cause fix for the defect *class* (not just today's functions), confirmed safe at the emitter level (only ever fires on opcodes already producing zero IR) and safe at the build level once the shipped-shader-blocking functions land (confirmed zero shipped shaders reference anything else missing).
- Q: All builds going forward should rely solely on the project's vcpkg-vendored toolchain (not Homebrew) — vcpkg's overlay triplets correctly target the project's actual minimum macOS deployment version (13.3), resolving the linker warnings seen building against Homebrew's LLVM (built for whatever SDK Homebrew's own CI happened to use). → Confirmed; local development for this feature uses the vcpkg toolchain exclusively.
- Q: Should every fixed function get its own persisted `.slo`-vs-`.rslo` equivalence regression test, added as each function lands, or is a final end-of-feature verification pass sufficient? → A: Per-function (or per-tightly-related-group) persisted `ctest -L visual` regression pairs, added at implementation time — not batched to the end.
- Q: Should this feature require the new JIT implementations (especially the P1 raytracing tier) to be measurably faster than the interpreter, mirroring spec 011's precedent? → A: No — correctness parity only. Spec 011's equivalent bar (JIT ≥10% faster than interpreter) is documented as never actually met project-wide; this feature's `FR-007` (avoid ~3× redundant ray tracing via the `numRealVertices` discipline) already prevents the most obvious performance regression as a correctness requirement, without gating the fix on a historically-unmet wall-clock target.
- Q: Should `quadlight.rib`/`spherelight.rib` (currently unregistered example scenes, not part of `ctest -L visual`) become permanent, CI-visible regression tests, or is a one-time manual verification sufficient? → A: Register them permanently — they are the real-world scenes that motivated issues #1 and #3; leaving them unregistered would let a future regression in `visibility()`/`transmission()`'s JIT path silently reopen the exact defect this feature exists to close, with nothing in CI to catch it.

### Session 2026-09-20 (mid-implementation scope extension)

While implementing User Story 2's `kAllFunctionMnemonics[]` coverage guard,
running it for real (not just reading its own code) surfaced **18
previously-uninventoried unhandled builtin functions** — `surface`,
`displacement`, `atmosphere`, `incident`, `opposite`, `attribute`,
`option`, `rendererinfo`, `textureinfo`, `bake3d`, `texture3d`,
`shadername`, `phong`, `specularbrdf`, `pnoise`, `debug`, `Deriv`,
`clearlighting` — on top of the 19 (of the original 25 minus Story 1's 6)
already known from Stories 3/4. GitHub issue #3's original inventory (and
this spec's own count) never enumerated `shaderFunctions.h` — only
`giFunctions.h` — a genuine gap in the original investigation, not a guard
bug. Independently confirmed (not just trusted from the guard): (a) direct
`oshader --jit` compile attempts on minimal fixtures calling `shadername()`
and `option()` both fail exactly as the guard predicts (nonzero exit,
diagnostic naming the mnemonic, no `.slo` written); (b) grepping every
shipped shader (`shaders/*.sl`) for real calls to all 18 finds none (one
apparent hit, `phong` in `uberlight.sl`, is inside a comment, not a call).
- Q: Now that these 18 are confirmed real and unhandled, and confirmed to
  affect zero currently-shipped shaders, how should they be brought into
  this feature's scope? → A: Add them as a new User Story 5, via manual
  extension of `spec.md`/`research.md`/`tasks.md` (not a fresh
  `/speckit.specify` pass) — picking back up with `/speckit.analyze` before
  `/speckit.implement` proceeds into the new story, matching this feature's
  existing per-phase review process.



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

**Why this priority**: These are the only functions, of the 25 covered by
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
happened for `random()`/`urandom()` (issue #1) and the 25 functions this
feature inventories (issue #3). Independently, the project's automated test
suite also fails if a reachable builtin function lacks JIT handling, mirroring
the existing `OPCODE_`-only coverage guard's approach but extended to the
full `FUNCTION_`-family builtin set.

**Why this priority**: This is the actual root-cause fix for the defect
*class*, not just the functions this feature happens to name today — without
it, the exact same silent-failure pattern that produced two real,
independently-discovered bugs (`random()`/`urandom()`, then this feature's
25, then Story 5's further 18) can recur indefinitely. Sequenced after
Story 1 specifically: confirmed by direct inspection that, once Story 1's
six functions are implemented, zero currently-shipped shaders reference
any of the remaining 37 functions this feature also inventories (19 from
Stories 3/4, plus Story 5's 18, confirmed by the same direct-grep method
after Story 5's mid-implementation discovery) — so hardening the
compile-time gate at that point causes no build breakage, whereas
hardening it before Story 1 lands would break the build for every shipped
shader Story 1 fixes.

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

### User Story 5 - Previously-uninventoried builtin functions are available under the JIT (Priority: P3)

A shader author writes RSL using any of 18 builtin functions that GitHub
issue #3's original investigation never enumerated — because it only
inspected `giFunctions.h`, not the sibling `shaderFunctions.h` these all
live in. Discovered mid-implementation by actually running this feature's
own coverage guard (Story 2) rather than trusting its own code: `surface`,
`displacement`, `atmosphere`, `incident`, `opposite` (a shader-instance
parameter-query family, one shared mechanism); `attribute`, `option`,
`rendererinfo` (a scene/attribute-state parameter-query family, the same
underlying mechanism with a different accessor); `textureinfo` (texture
file metadata query); `texture3d`, `bake3d` (point-cloud read/write,
reusing this feature's own Story 1 `occlusion()`/`indirectdiffuse()`
point-cloud-cache architecture); `shadername` (current/named shader-type
lookup); `phong`, `specularbrdf` (BRDF lighting math, the same shape as
the already-JIT-handled `diffuse()`/`specular()`); `pnoise` (periodic
noise, reusing the already-JIT-handled `noise()`'s existing primitives);
`debug` (a four-overload no-op — the interpreter's own implementation
never even reads its argument); `Deriv` (general finite-difference
derivative of a caller-supplied expression pair); `clearlighting` (a
single-call state reset). Each silently no-ops under `--jit` exactly like
Stories 1/3/4's functions did. This story fixes all 18 so JIT output
matches the interpreter for shaders using them, and closes
`LibShader_OpcodeCoverage` (Story 2's guard) to fully green.

**Why this priority**: Same rationale as Stories 3/4 — confirmed, by direct
grep of every shipped `.sl` file, that nothing currently shipped depends on
any of these 18. Lower priority than Stories 1/2, but still real,
silently-wrong behavior today for any shader author who uses one of them,
and — unlike Stories 3/4 — leaving it undone means Story 2's own coverage
guard (`ctest -L libshader`'s `LibShader_OpcodeCoverage`) cannot reach a
passing state at this feature's close, since it now enumerates these 18
too.

**Independent Test**: For each function (or tightly-related group sharing
one probe shader, matching Stories 1/3/4's established pattern), render a
minimal shader exercising it, once via each backend, and confirm the two
match. After all 18 land, `ctest -L libshader`'s `LibShader_OpcodeCoverage`
passes with zero remaining gaps.

**Acceptance Scenarios**:

1. **Given** a shader that calls `surface()`, `displacement()`,
   `atmosphere()`, `incident()`, or `opposite()` to query a named parameter
   from the correspondingly-bound shader instance, in any of its four
   result-type forms (float/vector/string/matrix), **When** rendered with
   the JIT backend, **Then** the result matches the interpreter backend.
2. **Given** a shader that calls `attribute()`, `option()`, or
   `rendererinfo()` to query scene/attribute-level state, in any of its
   four result-type forms, **When** rendered with the JIT backend, **Then**
   the result matches the interpreter backend.
3. **Given** a shader that calls `textureinfo()` for any of its supported
   query strings (`"resolution"`, `"type"`, `"channels"`,
   `"viewingmatrix"`, `"projectionmatrix"`, `"exists"`), **When** rendered
   with the JIT backend, **Then** the result matches the interpreter
   backend.
4. **Given** a shader that calls `texture3d()` (read) or `bake3d()`
   (write), **When** rendered with the JIT backend, **Then** the result
   matches the interpreter backend — `bake3d()` additionally following
   FR-007's once-per-real-vertex-then-replicate discipline under
   derivative shading, since it is a `DEFSHORTFUNC` like Story 1's
   raytracing-tier functions.
5. **Given** a shader that calls `shadername()` (with or without its
   optional shader-type-string argument), **When** rendered with the JIT
   backend, **Then** the result matches the interpreter backend.
6. **Given** a shader that calls `phong()` or `specularbrdf()`, **When**
   rendered with the JIT backend, **Then** the result matches the
   interpreter backend, including `specularbrdf()`'s anti-parallel
   halfway-vector NaN guard (the same class of guard already applied to
   `specular()`).
7. **Given** a shader that calls `pnoise()` in any of its supported
   dimensionality/result-type overloads, **When** rendered with the JIT
   backend, **Then** the result matches the interpreter backend.
8. **Given** a shader that calls `debug()` (in any of its four overload
   forms), **When** rendered with the JIT backend, **Then** behavior
   matches the interpreter backend's own no-op implementation (no crash,
   no per-vertex output written) — not a "fixed" or more useful debug
   output, since the interpreter itself has none.
9. **Given** a shader that calls `Deriv()` to compute the finite-difference
   derivative of a caller-supplied expression pair, **When** rendered with
   the JIT backend, **Then** the result matches the interpreter backend,
   including producing an exact-zero result (not garbage from an
   out-of-bounds read) when either operand is uniform.
10. **Given** a shader that calls `clearlighting()`, **When** rendered with
    the JIT backend, **Then** behavior matches the interpreter backend.

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
- What happens with `lightsource()`'s existing JIT implementation
  (`op_lightsource_f`) as a template for Story 5's `surface`/`displacement`/
  `atmosphere`/`incident`/`opposite`/`attribute`/`option`/`rendererinfo`
  family, since it uses the exact same underlying `PARAMETEREXPR_PRE`/
  `PARAMETEREXPRF/V/S/M`/`PARAMETEREXPR_UPDATE` macro mechanism and is
  already JIT-handled? Confirmed by inspection that `op_lightsource_f` is
  an intentionally simplified, incomplete implementation — float-result-only,
  effectively uniform-only (always writes index 0 regardless of `n`), never
  exercising the vector/string/matrix branches or the `cVar`/stride logic
  `PARAMETEREXPR_PRE` actually provides for a genuinely varying result. It
  must not be copied as-is for Story 5's family; each of that family's four
  result-type forms needs a real, complete implementation.
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
- **FR-016**: Every fix delivered under FR-001 through FR-006, FR-011
  through FR-015, and FR-020 through FR-030 MUST compute its result by
  invoking the same underlying implementation the interpreter backend
  already uses for that function, not by re-implementing the function's
  logic independently for the JIT.
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
- **FR-020**: The JIT shading backend MUST produce output matching the
  interpreter backend for shaders calling `surface()`, `displacement()`,
  `atmosphere()`, `incident()`, or `opposite()`, in all four of their
  result-type forms (float/vector/string/matrix).
- **FR-021**: The JIT shading backend MUST produce output matching the
  interpreter backend for shaders calling `attribute()`, `option()`, or
  `rendererinfo()`, in all four of their result-type forms.
- **FR-022**: The JIT shading backend MUST produce output matching the
  interpreter backend for shaders calling `textureinfo()`, for all of its
  supported query strings.
- **FR-023**: The JIT shading backend MUST produce output matching the
  interpreter backend for shaders calling `texture3d()`.
- **FR-024**: The JIT shading backend MUST produce output matching the
  interpreter backend for shaders calling `bake3d()`, following FR-007's
  once-per-real-vertex-then-replicate discipline under derivative shading.
- **FR-025**: The JIT shading backend MUST produce output matching the
  interpreter backend for shaders calling `shadername()`, with or without
  its optional shader-type-string argument.
- **FR-026**: The JIT shading backend MUST produce output matching the
  interpreter backend for shaders calling `phong()` or `specularbrdf()`.
- **FR-027**: The JIT shading backend MUST produce output matching the
  interpreter backend for shaders calling `pnoise()`, in its supported
  dimensionality/result-type overloads.
- **FR-028**: The JIT shading backend MUST match the interpreter's own
  no-op behavior for shaders calling `debug()` (any of its four overload
  forms) — no crash, no per-vertex value written, since the interpreter's
  own implementation does not read its argument either.
- **FR-029**: The JIT shading backend MUST produce output matching the
  interpreter backend for shaders calling `Deriv()`, including producing
  an exact-zero (not out-of-bounds-garbage) result when either operand is
  uniform.
- **FR-030**: The JIT shading backend MUST match the interpreter backend
  for shaders calling `clearlighting()`.
- **FR-031**: Every fix delivered under FR-020 through FR-030 MUST follow
  FR-016's delegate-to-the-interpreter's-own-implementation requirement and
  FR-017's mirror-current-behavior-exactly requirement, and MUST have a
  persisted regression test per FR-018 — this story does not relax any of
  those three requirements, it only adds functions under them.

### Key Entities

- **Shading backend**: One of two interchangeable engines that execute a
  compiled shader per-point during rendering — the interpreter (`.rslo`,
  reference/ground-truth behavior) and the JIT (`.slo`, compiled native
  code). Selected per-scene or per-primitive.
- **RSL builtin function**: A named, callable RenderMan Shading Language
  function (as distinct from a bytecode-level operator) that the compiler
  lowers into an intermediate call instruction consumed by both backends —
  the unit this feature's inventory (43 functions: 25 from the original
  investigation plus 18 found mid-implementation by Story 5, see
  Clarifications) is organized around.
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
- **SC-005**: 100% of the 43 builtin functions this feature inventories
  (6 in Story 1, 5 in Story 3, 14 in Story 4, 18 in Story 5 — `comp`
  counted once, in Story 1) have a passing, persisted JIT-vs-interpreter
  equivalence regression test under `ctest -L visual`.
- **SC-006**: Zero instances remain, after this feature, of a JIT-side fix
  under this feature re-implementing math or trace logic that already
  exists in the interpreter's own implementation, rather than reusing it.
- **SC-007**: After Story 5 lands, `ctest -L libshader`'s
  `LibShader_OpcodeCoverage` passes with zero remaining gaps — Story 2's
  coverage guard, extended to include Story 5's 18 functions, reaches a
  fully green state.

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
- This feature's builtin-function inventory (43 functions across Stories 1,
  3, 4, and 5) is drawn from two sources: GitHub issue #3's original list
  minus `"XXX"` (confirmed a DSO-dispatch placeholder, not a real function
  — see Edge Cases) and minus `random`/`urandom` (already fixed by issue
  #1, merged prior to this feature) — 25 functions, Stories 1/3/4; plus 18
  more found by actually running this feature's own Story 2 coverage guard
  mid-implementation, confirmed real via direct `oshader --jit` compile
  tests and confirmed to affect zero shipped shaders via direct grep — see
  Clarifications' 2026-09-20 mid-implementation entry, Story 5.
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
