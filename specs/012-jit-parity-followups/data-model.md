# Phase 1 Data Model: JIT/Interpreter Parity Follow-ups (post-011)

**Feature**: `specs/012-jit-parity-followups` | **Date**: 2026-08-24
**Source**: [spec.md](./spec.md) Key Entities, refined by [research.md](./research.md)

This feature persists no data. The "entities" below are the runtime and
compile-time concepts the requirements are written against; each is recorded
with the fields that matter to a requirement, the invariants that must hold
after the change, and — where applicable — its state transitions. Anything
not needed to verify a requirement is deliberately omitted.

---

## E1. Shading backend

The execution path a compiled shader takes. Two instances exist and both
ship; this feature adds none.

| Field | Interpreter | JIT |
|---|---|---|
| Artifact | `.rslo` bytecode | `.slo` LLVM bitcode |
| Executor | `execute.cpp` opcode dispatch loop | LLVM ORC `LLJIT`, code emitted by `llvmEmitter.cpp` |
| Selection | `Attribute "shade" "shaderformat"` > `Option "shaderformat"` > `OPENRENDER_DEFAULT_FORMAT` | same |
| Role in this feature | **Reference** (FR-011) | Subject of change (US2, US3) |

**Invariants**

- INV-1 — For every scene in the measurement set and the visual suite, the two
  backends produce output differing by no more than the same-configuration
  noise floor (FR-005, SC-007).
- INV-2 — Any change to the interpreter requires an empirical reproduction of
  a confirmed defect plus a maintainer STOP (FR-011). Behaviour-preserving
  refactors are exempt from the STOP, never from verification.
- INV-3 — The JIT never computes shading results itself; it calls the same
  final function the interpreter calls (FR-010).

**Relationship**: both backends read the *same* `op_*` / shared-function
surface. That surface is the join point this feature works on — E4 and E5.

---

## E2. Uniform classification (per instruction)

Whether every operand of one instruction is uniform, which authorizes running
that instruction once instead of once per shading point.

| Field | Interpreter representation | Emitter representation |
|---|---|---|
| Storage | `shader.h:73` — `unsigned char uniform` in the bytecode instruction | none stored; **derived** |
| Origin | `rslo.y` — `opcodeUniform` set TRUE per instruction, cleared by any non-uniform/non-constant operand | `getVar()` operand strides + `dstDesc.stride`, both compile-time `int` |
| Equivalent predicate | all operand `varyingStep == 0` | `dstStride == 0 && ∀ operand strides == 0` |
| Availability | at bytecode-emit time | **at dispatch-construction time** — resolved in [research.md D1](./research.md) |

**Invariants**

- INV-4 — The emitter's derived predicate agrees with the interpreter's stored
  flag for every instruction. They are two spellings of one definition
  ("all operands uniform"), not two analyses.
- INV-5 — The classification is *not* stored on `IRInstr`
  (`src/libshader/compiler/ir.h:90-97` has no such field) and this feature does
  not add one; the stride-derived route is the narrower change.

**State transitions** — per instruction, at compile time only, monotonic:

```
uniform=TRUE  ──(any operand is varying)──▶  uniform=FALSE   [terminal]
```

There is no run-time transition. A classification never changes after the
instruction is emitted.

---

## E3. Shading point count

How many points one instruction's execution covers. Two distinct counts exist
and **no single count generalizes** (FR-007).

| Count | Meaning | Used by |
|---|---|---|
| `numVertices` / `numVerts` | full grid width, including derivative duplicates | `DEFOPCODE`, `DEFFUNC` (interpreter); **every** JIT call site (all 54) |
| `numRealVertices` | real shading points only | `DEFSHORTOPCODE`, `DEFSHORTFUNC` (interpreter); spec 011's gather wrappers |

**Relationship**: in derivative-carrying shading contexts `numVertices` is a
multiple (up to 3×) of `numRealVertices` (`shading.cpp:771-927`).

**Invariants**

- INV-6 — A collapsed uniform call passes a count of exactly **1**, which is
  count-family-independent: one execution is one execution regardless of which
  of the two counts the varying path would have used. This is how FR-007 is
  satisfied at every family, including the SHORT one.
- INV-7 — The *varying*-path count asymmetry (the JIT dispatching
  `environment`/`shadow` at `numVerts` where the interpreter uses
  `numRealVertices`) is pre-existing, **out of scope**, and recorded in
  [research.md D4](./research.md). Correcting it would change JIT output and
  requires its own STOP.

---

## E4. Active-point state (tags)

Per-point active/inactive state carried through conditionals, expressed at the
`op_*` boundary as an `int*` tag array plus the count.

| Field | Type | Meaning |
|---|---|---|
| `tags` | `const int*` (nullable) | `tags[i] == 0` ⇒ point *i* active; `nullptr` ⇒ no filtering at all |
| `n` | `int` | number of points the call covers |
| guard | `ACTIVE(tags,i)` ≡ `!(tags) \|\| (tags)[i] == 0` (`rslOps.cpp:40-43`) | evaluated per element |
| companions | `numActive` / `numPassive` | maintained across conditional entry/exit |

**Invariants**

- INV-8 — A collapsed uniform call passes `tags = nullptr`. Passing `n = 1`
  with a live `tags` consults vertex 0's state and wrongly suppresses the
  write when vertex 0 is inactive, whereas the interpreter's uniform arm
  ignores tags entirely. See [contracts/op-uniform-collapse.md](./contracts/op-uniform-collapse.md).
- INV-9 — The collapse changes only the *width* and *tag filtering* of a call.
  `numActive`/`numPassive` bookkeeping and the conditional-nesting state
  machine are untouched (FR-006).

**State transitions** (unchanged by this feature; listed because FR-006 is
defined as preserving them):

```
        enter conditional              exit conditional
active ────────────────▶ inactive ─────────────────────▶ active
   (tags[i] set non-zero)          (tags[i] cleared to 0)
```

---

## E5. Light iteration

The traversal of the light list for one `illuminance` construct. Today it has
**two** implementations; after this feature it has one (SC-008).

| Field | Macro form (interpreter) | Method form (JIT) |
|---|---|---|
| Location | `execute.cpp:422-517`, `runLightsTemplate` | `shading.cpp:1502-1558`, `CShadingContext::runLights` / `runCategoryLights` |
| Entry from backend | `runLights` / `runCategoryLights` macro wrappers | `jitIlluminanceBegin` (`shading.cpp:2059-2122`) |
| Category parameter | present (`runCategoryLights` only) | **absent** at the JIT entry |
| Cache-validity predicate | `!*aTag & !*lTag` — any inactive point invalidates | `tags[i] != 0 \|\| lightingTags[i] == 0` — cache kept for inactive points |
| Uncategorised light under `invertCatMatch` | **included** | **excluded** |

**Related entity — `illuminance` arity** (`shaderOpcodes.h`), which determines
what the emitter can see:

| Opcode | Arity | Category operand | JIT lowering today |
|---|---|---|---|
| `Illumination1` (90) | 3 | — | **not lowered** (`size() >= 5` fails) |
| `IlluminationCat1` (92) | 4 | index 3 | **not lowered** |
| `Illumination2` (144) | 5 | — | lowered correctly |
| `IlluminationCat2` (146) | 6 | index 5 | lowered as the 5-operand form; category ignored — *as the interpreter also does* |

**Invariants**

- INV-10 — After convergence, exactly one implementation exists and both
  backends reach it (SC-008).
- INV-11 — The converged implementation reproduces the **macro form's**
  semantics, so the interpreter is bit-unchanged and the difference (if any)
  is absorbed by the JIT — which, per the table above, is output-neutral on
  every form the JIT actually lowers.
- INV-12 — The converged signature retains a category parameter: the
  interpreter's 4-operand path genuinely uses it. The JIT passes the
  no-category value.

**State transitions** per `illuminance` execution (preserved by the
convergence, not redefined by it):

```
begin ──▶ [light 0] ──enterLight──▶ body ──exitLight──▶ [light 1] ──▶ … ──▶ exit
   │                                                                        ▲
   └────────────────────── no lights / cache valid ─────────────────────────┘
```

---

## E6. Measurement set

The fixed apparatus SC-004/SC-005/SC-006 are evaluated against. Reused
unchanged from spec 011 per the spec's Assumptions; this feature defines no
new scene here and retires none.

| Field | Value |
|---|---|
| Scenes | `sphere-cfrom`, `sphere-ctransform`, `sphere-matrixops`, `sphere-comparisonlogic`, `sphere-arrayops`, `sphere-gather` |
| Registration | `add_perf_manual_test` (`tests/visual/CMakeLists.txt:236`), scenes at 1135–1157 |
| Label | `perf-manual` **only** — never runs in a default or CI `ctest` |
| Invocation | `ctest --test-dir build -L perf-manual -V` (**`-V`**, not `--output-on-failure` — `test_perf_compare.cpp:75,89` prints the ratio to stdout on the PASS path, which `--output-on-failure` discards) |
| Gate vs. report | `add_perf_manual_test` hard-fails any scene above `MAX_RATIO` (default `0.90`, `test_perf_compare.cpp:83`), while SC-005 specifies the 90% target as *reported, not gated*. Reconcile before relying on these runs (`tasks.md` T003a); either way the label sits outside `-L libshader` and `-L visual`, so SC-003's evidence is unaffected |
| Per-scene attributes | RIB pair (`-reyes.rib` / `-reyes-slo.rib`), surface shader, uniform-computation density, JIT/interpreter ratio, run-to-run variance |
| SC-006 comparison pair | `sphere-arrayops` (uniform-dominated) vs `sphere-cfrom` (near-zero uniform density), at identical scene scale |

**Invariants**

- INV-13 — Every recorded ratio carries its scene's uniform-computation
  density alongside it, so classification is auditable rather than
  retrofitted (SC-004).
- INV-14 — A variance figure exists per scene before any ratio is judged.
  It does not exist today; producing it is a prerequisite task, not setup
  ([research.md D7](./research.md)).
- INV-15 — All timing runs happen on a quiescent machine, one measurement
  session per comparison.

---

## E7. Coverage artifact (US1)

The new, executing regression coverage for the varying-index string-array
read. Modelled as an entity because SC-002 and SC-007 jointly constrain how it
may come into existence.

| Field | Value |
|---|---|
| Probe shader | new `.sl` under `shaders/` (plus its compiled `.rslo`/`.slo`) |
| Scene pair | new `-reyes.rib` + `-reyes-slo.rib` under `examples/rib/tests/` |
| Reference image | **one new** image under `tests/visual/reference/` |
| Registration | `add_visual_test(...)` in `tests/visual/CMakeLists.txt` |

**Invariants**

- INV-16 — No existing shader, scene, or reference image is modified. New
  coverage arrives only as new assets, so every existing scene remains a
  control (SC-007).
- INV-17 — The coverage must *execute* the read. The existing
  `LibShader_OpcodeCoverage` guard
  (`src/libshader/tests/test_opcode_coverage.cpp`) is reachability-only and
  does not satisfy SC-002.
- INV-18 — The artifact must fail if the defect returns (SC-002), which means
  it must be the reproduction case itself, not a weakened variant of it.
