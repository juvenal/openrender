# Phase 0 Research: JIT/Interpreter Parity Follow-ups (post-011)

**Feature**: `specs/012-jit-parity-followups` | **Branch**: `012-jit-parity-followups`
**Date**: 2026-08-24 | **Spec**: [spec.md](./spec.md)

All findings below were established by direct source inspection in this
worktree. Line references are to the tree as of this branch's HEAD
(`3a8ca20`). Nothing here was assumed from spec 011's prose; where spec 011
is cited it is cited as a recorded *measurement*, not as an architectural
claim.

---

## D1. The spec's one open Assumption — RESOLVED (affirmative)

The spec carries exactly one unresolved question, recorded in Assumptions:

> "Whether the uniform classification of an instruction is already available
> at the JIT's dispatch-construction point is treated as an open question to
> be settled by investigation, not as an established fact. If it is not
> available, making it available is in scope as a prerequisite to FR-004."

**Decision**: The classification **is** available at the JIT's
dispatch-construction point, derivable with no new compiler plumbing. The
prerequisite branch of that Assumption does not apply, and FR-004 has no
blocking predecessor work.

**Rationale**: The interpreter's per-instruction uniform flag is not an
independent analysis — it is defined as *"every operand is uniform"*, and the
JIT emitter already computes the very same per-operand property under a
different name.

- Interpreter side: `src/libshader/shading/rslo.y` sets
  `currentData.opcodeUniform = TRUE` at the start of each instruction
  (lines 1834, 1885, 1903, 1921) and clears it the moment any operand is not
  uniform/constant (lines 480-481 and 501). In the same `else` branch it sets
  that operand's `varyingStep = 0`. The flag is stored into the bytecode at
  lines 551/628/642/1860 as
  `currentOpcodePlace->uniform = currentData.opcodeUniform`, declared in
  `src/libshader/shading/shader.h:73` as
  `unsigned char uniform; // TRUE if all the arguments are uniform`.
  So `uniform` ≡ *all operand `varyingStep` values are 0*.
- Emitter side: `src/libshader/compiler/llvmEmitter.cpp` already carries the
  identical per-operand quantity as a **compile-time `int` stride**:
  `getVar()` (line 442) returns `std::pair<llvm::Value*, int>` where the
  `int` is the operand stride, and the destination stride is a plain `int`
  at line 598 (`dstDesc.stride`), documented at line 129 as
  `int stride; // 0=uniform, 1=float, 3=vector, 16=matrix`.

The emitter-side predicate is therefore
`dstStride == 0 && every operand stride == 0`, computed from values the
emitter already holds before it constructs the call.

**Alternatives considered**:

- *Thread an explicit uniform flag through the IR.* `IRInstr`
  (`src/libshader/compiler/ir.h:90-97`) has no such field, so this means
  touching the IR struct, its producer, and its consumers. Rejected as
  strictly wider than the stride-derived predicate for identical
  information. Worth revisiting only if a future site needs the
  classification where strides are not in scope.
- *Recompute uniformity by re-analysing variable declarations in the
  emitter.* Rejected: duplicates the frontend's analysis, and would be a
  second source of truth that can disagree with `rslo.y` — the exact class of
  hand-synced duplication FR-009/FR-010 exist to eliminate.

---

## D2. The collapsed call form is `n = 1` **and** `tags = nullptr`

**Decision**: When an instruction is classified uniform, the emitted call
must pass a shading-point count of 1 **and a null tag pointer**. Passing
`n = 1` alone is incorrect.

**Rationale**: The `op_*` ABI guards each element with
`ACTIVE(tags, i)`, defined in `src/libshader/shading/rslOps.cpp:40-43` as:

```c
#define IDX(base, str, i) ((base) + (str) * (i))
#define ACTIVE(tags, i)   (!(tags) || (tags)[i] == 0)
```

A null `tags` means "no tag filtering". With `n = 1` and a live `tags`, the
op would consult `tags[0]` — vertex 0's active state — and skip the write
whenever vertex 0 happens to be inactive. The interpreter's uniform branch
does the opposite: in `src/libshader/shading/execute.cpp:610-718`, the
`if (code->uniform)` arm evaluates `expr` **once, with no tag test at all**,
while only the two varying arms iterate and test tags. Parity therefore
requires suppressing the tag test, which the ABI expresses as
`tags = nullptr`.

This is the single most consequential detail in the FR-004 change: `n = 1`
with a live tag pointer compiles, runs, and produces correct output on every
scene where vertex 0 is active — i.e. it passes casual testing and fails
inside conditionals. It is restated as a normative contract in
[contracts/op-uniform-collapse.md](./contracts/op-uniform-collapse.md).

**Alternatives considered**:

- *Pass `n = 1` and leave `tags` live.* Rejected — the failure above.
- *Add a dedicated `uniform` boolean parameter to every `op_*`.* Rejected:
  changes the C-linkage ABI of ~200 functions, and every already-compiled
  `.slo` in the tree would silently read garbage arguments at JIT call sites
  (CLAUDE.md gotcha: ABI mismatch is caught neither at build nor at link
  time). The `(n=1, tags=nullptr)` form needs no ABI change at all.
- *Emit a scalar fast path in LLVM IR instead of calling the op.* Rejected
  outright by FR-010 — that is reimplementation, not delegation.

---

## D3. Parity scope map for FR-004 (which interpreter sites short-circuit)

**Decision**: FR-004's "every site where the interpreter short-circuits" is
exactly the four opcode-dispatch macro families in
`src/libshader/shading/execute.cpp` that contain an `if (code->uniform)`
arm — `DEFOPCODE`, `DEFSHORTOPCODE`, `DEFFUNC`, `DEFSHORTFUNC`.
`DEFLIGHTFUNC` is an **explicit, documented exclusion**, not an omission.

**Rationale**: `DEFLIGHTFUNC` does not have a collapse arm to mirror; it
treats a uniform classification as a hard error —
`if (code->uniform) { scripterror("Invalid uniform lighting call"); }`. There
is no "run once" semantics for it to be brought into parity with. The spec's
Clarification requiring any unconvertible site to be "listed explicitly with
the reason" is satisfied by this entry.

Practical scope note: **`DEFSHORTOPCODE` has zero real uses** anywhere in the
tree — it appears only as macro definition/undef boilerplate in `rslo.y`
(29, 43, 105, 118, 126, 139), `rslo_code.h` (33, 53) and `execute.cpp`
(633, 736). The live SHORT family is therefore five `DEFSHORTFUNC` builtins
in `src/libshader/shading/shaderFunctions.h`: `environment` float (1918) and
color (1919), `shadow` float (2034) and color (2035), and `bake3d` (2217).

**Alternatives considered**:

- *Define scope as "the arithmetic opcodes only" (the `emitBin`/`emitUn`/
  `emitTern` helpers named in the seed description).* Rejected: it would
  leave `DEFFUNC`-family builtins dispatching at full width for no stated
  reason, and FR-004 asks for parity at every short-circuiting site, not at
  the three most convenient ones.
- *Define scope by enumerating opcodes.* Rejected as unmaintainable and
  drift-prone; the macro family is the interpreter's own organising
  principle and stays correct as opcodes are added.

---

## D4. SHORT-family shading-point count — a pre-existing asymmetry, recorded, NOT fixed here

**Decision**: Record the finding; keep it **out of scope** for this feature;
flag it as requiring a maintainer STOP if it is ever addressed.

**Finding**: The interpreter uses two different counts.

- `DEFOPCODE` / `DEFFUNC` iterate `numVertices`.
- `DEFSHORTOPCODE` / `DEFSHORTFUNC` (`execute.cpp:633` onward) iterate
  `currentShadingState->numRealVertices`.

The distinction is real and load-bearing: in derivative-carrying shading
contexts `numVertices` is a multiple (up to 3×) of `numRealVertices`
(`src/libshader/shading/shading.cpp:771-927`), which is why spec 011's
gather work had to introduce `numRealVertices`-bound `gatherElse`/`gatherEnd`
wrappers (`shading.cpp:2615-2638`) instead of reusing the `numVertices`-bound
`op_else_update`/`op_endif_update`.

The JIT, however, passes `numVerts` at **every** call site without exception
— all 54 of them: lines 403, 489, 497, 508, 538, 565, 616, 623, 628, 809,
816, 825, 843, 851, 897, 954, 975, 1093, 1099, 1105, 1111, 1141, 1161, 1176,
1186, 1198, 1227, 1237, 1280, 1312, 1323, 1332, 1343, 1387, 1424, 1443,
1467, 1475, 1513, 1529, 1541, 1549, 1557, 1597, 1608, 1640, 1649, 1674,
1694, 1716, 1755, 1782, 1804, 1836 of `llvmEmitter.cpp`. The identifier
`numRealVertices` appears **nowhere** in `llvmEmitter.cpp`, and
`grep -c numRealVertices src/libshader/shading/rslOps.cpp` returns **0** — no
`op_*` wrapper clamps to it. Concretely, `environment` (1640, 1649) and
`shadow` (1674) are dispatched at `numVerts` while their interpreter
counterparts run at `numRealVertices`.

**Why it is nonetheless out of scope**: this asymmetry lives entirely in the
*varying* path and predates this feature. FR-004's change concerns the
*uniform* path, and the collapsed form (`n = 1`, `tags = nullptr`) is
count-independent — one execution is one execution regardless of which count
the varying path would have used. FR-007 ("must use the shading-point count
appropriate to that site; no single count generalizes") is therefore
satisfied by the collapse itself at every family including the SHORT one.

Correcting the varying-path count would change JIT output on
derivative-carrying shading contexts, which collides head-on with FR-005 and
SC-007 (zero output difference, no reference image regenerated). It is
recorded here as a spec-013 candidate and must go through the FR-011 process
if ever picked up.

**Alternatives considered**: folding the count fix into FR-004 as "while we
are here". Rejected: it would make an output-neutral change
output-affecting, destroying the clean SC-007 control that makes the FR-004
change verifiable at all.

---

## D5. FR-005 (zero output change) is a *hypothesis to verify*, not a given

**Decision**: Treat FR-005 as an empirical claim to be measured against an
unchanged-binary baseline, and pre-plan the response if it fails.

**Rationale**: The two backends **already differ today** in one corner. For a
uniform-classified instruction inside a block where every point is inactive:

- Interpreter: the `if (code->uniform)` arm runs `expr` once, ignoring tags —
  the destination **is** written.
- JIT today: `n = numVerts` with a live `tags`, and `ACTIVE()` rejects every
  element — the destination is **not** written.

The FR-004 change moves the JIT onto the interpreter's semantics, so any
output difference it produces is by construction a *JIT correction toward the
reference*, not a regression. But it is still an observable difference, and
SC-007 admits no exceptions and forbids regenerating any existing reference
image.

**Planned response if a difference appears**: stop and present it to the
maintainer as a defect-disposition decision (which backend was right, and
whether the affected reference image was capturing the JIT's error). Do not
regenerate a reference image unilaterally. Whether such a block is reachable
in the six measurement scenes plus the visual suite is unknown until
measured; the baseline capture task exists precisely to answer it.

**Alternatives considered**: asserting output-neutrality from the code
reading above and skipping the before/after image comparison. Rejected —
this is exactly the reasoning FR-002/FR-011 forbid (code-reading-derived
conclusions standing in for empirical evidence).

---

## D6. Light iteration: the two implementations, and their two divergences

**Decision**: Converge on a single implementation that reproduces the
**macro form's** semantics exactly, so the interpreter is bit-unchanged and
the JIT absorbs the (output-neutral) difference.

**The two implementations**:

| | Interpreter | JIT |
|---|---|---|
| Form | `runLightsTemplate` macro, `execute.cpp:422-517` | `CShadingContext::runCategoryLights` method, `shading.cpp:1502-1558` |
| Entry | `runLights` / `runCategoryLights` macro wrappers | `runLights(...)` called from `jitIlluminanceBegin`, `shading.cpp:2059-2122` |

**Divergence 1 — cache-validity predicate.** The macro invalidates its
cached lighting result if *any* point is inactive:
`curLightingValid = curLightingValid && (!*aTag++ & !*lTag++)`. The method
keeps the cache for inactive points:
`curLightingValid && (tags[i] != 0 || ss->lightingTags[i] == 0)`. The method's
form is a strict improvement (it re-runs lights less often) and is
output-neutral, but it is a divergence — the two were hand-synced, and drifted.

**Divergence 2 — category matching for uncategorised lights.** The macro's
`CATEGORYLIGHT_CHECK` ends `} else if (!invertCatMatch) { continue; }`, so a
light with `categories == NULL` **is** included when `invertCatMatch` is set.
The method computes `validLight` and inverts it, excluding that light. Only
one of these can be right.

**Reachability finding that de-risks Divergence 2 substantially**: the
category is very nearly unreachable from the JIT today, and *partly*
unreachable from the interpreter too.

`illuminance` has four interpreter arities
(`src/libshader/shading/shaderOpcodes.h`):

| Opcode | Arity | Operands | Light call |
|---|---|---|---|
| `Illumination1` (90) | 3 | P, begin, end | `runLights` |
| `IlluminationCat1` (92) | 4 | P, begin, end, **category@3** | `runCategoryLights` |
| `Illumination2` (144) | 5 | P, N, angle, begin, end | `runLights` |
| `IlluminationCat2` (146) | 6 | P, N, angle, begin, end, **category@5** | **`runLights`** |

Note line 146: the 6-operand category form declares `lightCat` via
`ILLUMINATION2RUNLIGHT_PRE` and then passes `ILLUMINATION_RUNLIGHTS` — the
*non*-category macro. **The interpreter itself discards the category in the
`illuminance("cat", P, axis, angle)` form.** That is a latent interpreter
defect, out of scope here (FR-011 process, and it fails FR-002's "empirical
reproduction required" bar until someone renders it), but it means JIT and
interpreter currently *agree* on that form.

The JIT emitter (`llvmEmitter.cpp:861-906`) reads operands `[P, N, angle,
bodyLabel, exitLabel]` and requires `ins.operands.size() >= 5`.

**The emitter's `IRInstr.operands` count is verified to equal the interpreter's
`DEFOPCODE` arity**, so the table above is directly applicable to the emitter
and is not an inference from the interpreter's declaration:

- `CIlluminationLoop::getCode()` (`expression.cpp:2354-2400`) emits the IR line
  as `illuminance [P] [N] [angle] beginLabel endLabel [category]`, each optional
  operand printed only when its `CExpression*` is non-null. The constructor
  (`expression.cpp:2290-2325`) sets exactly which are non-null from the RSL
  arity: 1 core param → `P`; 2 → `category`+`P`; 3 → `P`,`N`,`angle`;
  4 → all four. That yields token counts of 3 / 4 / 5 / 6 respectively.
- `CIRBuilder` lists `"illuminance"` in `noResultOpcodes`
  (`irBuilder.cpp:254-266`), so **no** token is consumed as a result and every
  post-mnemonic token becomes an entry of `operands`
  (`irBuilder.cpp:273-278`).

So `operands.size()` is 3 / 4 / 5 / 6, with the ordering above. So:

- 5-operand form: lowered correctly.
- 6-operand form: lowered as if it were the 5-operand form; operand 5
  (category) ignored — **matching the interpreter's own behaviour** for that
  form.
- 3- and 4-operand forms (`illuminance(P)` and `illuminance("cat", P)`):
  `size() >= 5` is false, `exitLabel` stays empty, and the emitter hits the
  `continue` at line 886. The loop scaffolding is never emitted. This is a
  **pre-existing JIT opcode-coverage gap of exactly spec 011's kind**, not a
  duplication problem — recorded here, out of scope for US3, and a spec-013
  candidate.

**Consequence for US3**: on every form the JIT actually lowers, converging
onto macro semantics changes no JIT output either (both ignore the category
there). US3 therefore remains a behaviour-preserving refactor and stays under
the FR-009/FR-011 refactor exemption — **no STOP required**, full before/after
verification still required. The shared implementation must still accept a
category parameter, because the interpreter's 4-operand path genuinely uses
it; the JIT passes the no-category value.

**Trigger that would flip this to a STOP**: if implementation shows the
shared entry point cannot preserve *both* divergence-1 behaviours
simultaneously without an observable difference on some scene. In that case
the spec's own edge case applies ("cannot be done without changing observable
behavior → FR-011 process"), and US3 stops for maintainer disposition rather
than proceeding.

**Alternatives considered**:

- *Converge on the method form's semantics* (keep its better cache
  predicate and its category handling). Rejected as the default: it changes
  interpreter behaviour, and the interpreter is the reference (FR-011). The
  cache-predicate improvement can be proposed separately, as an interpreter
  change with its own STOP, once parity exists.
- *Keep both and add a drift test.* Rejected by FR-009 and SC-008 outright
  — the requirement is one implementation, not two verified copies.

---

## D7. Measurement apparatus (reused unchanged from spec 011)

**Decision**: Reuse spec 011's six `perf-manual` scenes and its 8×8
block-averaged image comparison verbatim; add no new measurement machinery.
For SC-006's gap comparison, the pair is **`sphere-arrayops` (uniform-dominated,
ratio 1.15–1.46 in spec 011) vs `sphere-cfrom` (near-zero uniform density,
ratio ~1.03)**.

**Rationale**: the spec's Assumptions mandate this reuse. Verified in
`tests/visual/CMakeLists.txt`: `add_perf_manual_test` (macro at line 236,
`LABELS "perf-manual"` at 257) registers exactly six scenes — `sphere-cfrom`
(1135), `sphere-ctransform` (1139), `sphere-matrixops` (1143),
`sphere-comparisonlogic` (1147), `sphere-arrayops` (1151), `sphere-gather`
(1155) — carrying **only** the `perf-manual` label (comment at 224-227), so
they never run in a default or CI `ctest` invocation. Run via
`ctest --test-dir build -L perf-manual --output-on-failure`.

Scene→shader mapping, which fixes each scene's uniform-computation density
for SC-004's auditability requirement:

| Scene | Surface shader | Uniform density (spec 011 ratio) |
|---|---|---|
| `sphere-cfrom` | `show_st_hsv` | near-zero (~1.03) — the control |
| `sphere-ctransform` | `show_ctransform` | low |
| `sphere-matrixops` | `matrix_ops_probe` | high |
| `sphere-comparisonlogic` | `comparison_logic_probe` | high |
| `sphere-arrayops` | `array_ops_probe` | highest (1.15–1.46) |
| `sphere-gather` | `gather_named_probe` | mixed (gather-dominated) |

**Gap in the apparatus that this feature must close**: SC-004 and SC-006 both
key on "measured run-to-run variance", and **no variance baseline exists** —
spec 011 reported single-run ratios (`tasks.md` T051, `lessons-learned.md`
470-479). Establishing it (repeated runs per scene on an otherwise-idle
machine, recorded spread) is a prerequisite task for US2, not an
implementation detail.

**Alternatives considered**: adding new, more uniform-dense measurement
scenes to make the improvement easier to see. Rejected — it would let the
result be tuned by scene selection, and the spec explicitly fixes the
measurement set.

---

## D8. Serialization constraints that bound "maximum parallelism"

**Decision**: Three constraints serialize work that would otherwise be
parallel. They are recorded here because they are properties of the build and
measurement environment, not of the task decomposition, and no amount of task
re-ordering removes them.

1. **One pristine baseline, captured once, before any stream lands.** SC-003
   requires before/after verification "independently before and after each of
   the three changes". If streams land concurrently, stream A's landed change
   silently becomes stream B's "before". Baseline capture is a serial
   prerequisite of all three streams.
2. **Performance measurement requires exclusive machine access.** Timing
   `perf-manual` while another stream compiles or renders on the same host
   produces noise larger than the effect being measured — and SC-004/SC-006
   are both variance-relative. Every timing run (variance baseline, pre-change,
   post-change) is a quiescent-machine serialization point.
3. **`.slo` regeneration is a shared, serialized resource.** Per CLAUDE.md:
   nothing in the build graph regenerates `.slo` bitcode in either the tracked
   `shaders/` tree or the deploy tree, in either direction; an emitter change
   plus a stale `.slo` is an ABI mismatch that is caught at neither build nor
   link time and surfaces as garbage arguments at JIT call sites. Both US2
   (emitter dispatch) and US3 (light iteration) touch code that invalidates
   every `.slo`. They cannot independently regenerate and verify. Every
   `-slo` verification must be preceded by `stat`-ing each `.slo` against
   both the emitter source mtime and the `oshader` binary mtime.

**Alternatives considered**: giving each stream its own worktree and build
directory to parallelise the builds. Rejected for the timing runs
specifically (constraint 2 is about the machine, not the directory); it
remains available for the non-timing portions of US1 and US3 if wall-clock
pressure warrants, at the cost of a second full build tree.

---

## D9. `usfroma` crash — what is known, and what is deliberately *not* concluded

**Decision**: Enter US1 with **no** root-cause hypothesis committed. The
first task is empirical reproduction (FR-002), and the root-cause location
(interpreter vs. compiler) is an output of that task, not an input.

**What is established** (from spec 011's records, not from new analysis):

- `specs/011-jit-opcode-parity/triage-results.md:85` records the shape:
  a varying-indexed read of a `uniform string` array consumed directly in an
  expression, e.g. `if (usarr[findex] == "a")`.
- The read is not out of bounds.
- `rsloStringSpecifier` (`rslo.y:342-347`) unconditionally ORs `SLC_UNIFORM`
  into a bare `string`, so no RSL string *variable* can hold a varying value.
  `sfroma`/`usfroma` are therefore reachable only as directly-consumed
  expressions — which constrains what the reproduction shader can look like.
- The exercise was removed from `shaders/array_ops_probe.sl` as spec 011's
  workaround; today the opcode is covered only by the reachability-only
  `LibShader_OpcodeCoverage` build-time guard, which does not execute it.
- `op_sfroma` (`rslOps.cpp:1273`) and its emitter dispatch
  (`llvmEmitter.cpp:1767/1769/1779`) exist, so the JIT path is present; the
  crash is on the interpreter path.

**What is deliberately not concluded**: whether the defect is in the
interpreter's opcode implementation or in the bytecode the compiler emits for
it. The spec's Assumptions leave both open, and the answer determines which
STOP gate applies — an interpreter edit and a compiler edit have different
blast radii (a compiler change alters the meaning of already-compiled
artifacts for *both* backends at once, and the STOP presentation must state
that impact).

**Alternatives considered**: reading the code to a root cause first and using
the reproduction to confirm it. Rejected: FR-002 requires the empirical
reproduction to *precede* and *authorize* the change, and a pre-committed
hypothesis is precisely what makes a confirmation run non-diagnostic.

---

## Summary of dispositions

| ID | Finding | Disposition |
|---|---|---|
| D1 | Uniform classification available from emitter strides | Resolves spec Assumption; FR-004 unblocked |
| D2 | Collapse form is `n=1` **and** `tags=nullptr` | Normative — see `contracts/op-uniform-collapse.md` |
| D3 | 4 short-circuiting macro families; `DEFLIGHTFUNC` excluded | FR-004 scope, with its required explicit exclusion |
| D4 | JIT passes `numVerts` at SHORT-family sites too | Recorded, **out of scope**, spec-013 candidate + STOP |
| D5 | FR-005 neutrality is a hypothesis | Must be measured; failure → maintainer disposition, never a reference regeneration |
| D6 | Two light-iteration divergences; category near-unreachable in JIT | US3 stays a refactor (no STOP); named flip trigger recorded |
| D7 | Six `perf-manual` scenes reused; SC-006 pair fixed | Variance baseline is a new prerequisite task |
| D8 | Baseline / exclusive-machine / `.slo` serialization | Bounds the parallel plan |
| D9 | `usfroma` root cause left open | Reproduction precedes and authorizes any edit |

**Out-of-scope items discovered during Phase 0** (recorded for a future spec,
none actioned here):

1. JIT dispatches `environment`/`shadow` at `numVerts`, interpreter at
   `numRealVertices` (D4).
2. Interpreter's 6-operand `illuminance` category form silently discards the
   category (D6).
3. JIT emitter drops the loop scaffolding for the 3- and 4-operand
   `illuminance` forms (D6).
4. The macro-vs-method light-cache-validity divergence, if the method's form
   is preferred, requires an interpreter change with its own STOP (D6).
