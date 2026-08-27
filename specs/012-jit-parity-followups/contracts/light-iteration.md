# Contract: single shared light-iteration entry point

**Feature**: `specs/012-jit-parity-followups` (US3 / FR-009, FR-010, SC-008)
**Surface**: light-list traversal for the `illuminance` construct, reached from
both `execute.cpp` (interpreter) and `shading.cpp` (JIT)
**Status**: normative for this feature

---

## 1. The interface as it stands — two implementations

| | Macro form | Method form |
|---|---|---|
| Location | `execute.cpp:422-517`, `runLightsTemplate` + `CATEGORYLIGHT_PRE` / `CATEGORYLIGHT_CHECK` | `shading.cpp:1502-1558`, `CShadingContext::runLights` / `runCategoryLights` |
| Reached by | interpreter opcode bodies, via the `runLights` / `runCategoryLights` macro wrappers | JIT, via **five** live call sites, all through the no-category `runLights(...)` entry: `callDiffuse` (1615), `callSpecular` (1655), `prepareDiffuse` (1741), `setupIlluminance` (1754), `jitIlluminanceBegin` (2059-2122) |
| Category argument | yes (`runCategoryLights`) | **no** — every JIT call site passes `saveCat = 0` via the no-category entry |

**Correction (2026-08-26):** the original table above listed `jitIlluminanceBegin`
as the method form's only JIT caller. A call-site trace found four more —
`callDiffuse`, `callSpecular`, `prepareDiffuse`, `setupIlluminance` — all
reached from the `diffuse()`/`specular()`/`ambient()` builtin path, not the
`illuminance` construct. All five pass `saveCat = 0` (the plain `runLights`
wrapper, not `runCategoryLights`), so this does not add a new code path to
converge — it means §2.1's "exactly one implementation" / §5's retirement
grep cannot pass without repointing all five, not just one. T038/T041 are
scoped to cover all five call sites accordingly.

They are hand-synced copies and have drifted in exactly two places
([research.md D6](../research.md)):

1. **Cache-validity predicate.** Macro: `!*aTag & !*lTag` — any inactive point
   invalidates the cached lighting result and re-runs the lights. Method:
   `tags[i] != 0 || lightingTags[i] == 0` — the cache is kept for inactive
   points. The method's form is a strict improvement and output-neutral, but
   it is a divergence.
2. **Uncategorised light under `invertCatMatch`.** Macro: a light with
   `categories == NULL` **is** included. Method: it is **excluded**. Only one
   can be right.

---

## 2. The contract

### 2.1 Cardinality

After this feature, **exactly one** implementation of light iteration for
`illuminance` exists, and both backends reach it (SC-008). Neither backend
carries a copy, a wrapper that re-derives the logic, or a "kept in sync"
comment.

### 2.2 Semantics

The single implementation MUST reproduce the **macro form's** semantics —
both the cache-validity predicate and the category-matching rule — so that the
interpreter's behaviour is bit-unchanged (FR-011: the interpreter is the
reference). The JIT absorbs the difference, which is output-neutral on every
form it actually lowers (see §3).

The method form's better cache predicate is **not** adopted here. It may be
proposed afterwards as an interpreter change with its own empirical
justification and its own STOP, once a single implementation exists to change.

### 2.3 Signature

The converged entry point MUST retain a category parameter. The interpreter's
4-operand `illuminance` path (`IlluminationCat1`, `shaderOpcodes.h:92`)
genuinely uses it via `ILLUMINATION_RUNCATLIGHTS`. The no-category call is
expressed by passing the no-category value (`0`), exactly as the macro form's
`runLights` wrapper already does through `NORMALLIGHT_PRE`.

### 2.4 Preserved behaviour

- Per-light `enterLight` / `exitLight` sequencing and the `currentLight` walk
  are unchanged.
- `numActive` / `numPassive` bookkeeping is unchanged.
- `SHADERFLAGS_NONAMBIENT` handling is unchanged.
- The JIT's `costheta` construction and uniform-P/N stride-3 broadcast in
  `jitIlluminanceBegin` are JIT-side *argument preparation*, not light
  iteration, and stay where they are.

---

## 3. Why this is a refactor and not a behaviour change

The category is very nearly unreachable through the JIT, and *partly*
unreachable through the interpreter:

| Opcode (`shaderOpcodes.h`) | Arity | Category operand | Interpreter light call | JIT lowering today |
|---|---|---|---|---|
| `Illumination1` (90) | 3 | — | `runLights` | **not lowered** — `ins.operands.size() >= 5` fails, `llvmEmitter.cpp:884-887` |
| `IlluminationCat1` (92) | 4 | index 3 | `runCategoryLights` | **not lowered** — same reason |
| `Illumination2` (144) | 5 | — | `runLights` | lowered correctly |
| `IlluminationCat2` (146) | 6 | index 5 | **`runLights`** — the category is declared and then ignored | lowered as the 5-operand form; operand 5 ignored |

The arity column is the emitter's `IRInstr.operands.size()`, verified — not
inferred from the interpreter's `DEFOPCODE` declaration. `CIlluminationLoop`
emits the IR line as `illuminance [P] [N] [angle] begin end [category]`
(`expression.cpp:2290-2400`), and `illuminance` is in `CIRBuilder`'s
`noResultOpcodes` list (`irBuilder.cpp:254-266`), so no token is consumed as a
result and every post-mnemonic token lands in `operands`
(`irBuilder.cpp:273-278`). The two counts coincide.

On the 5-operand form neither backend has a category. On the 6-operand form
**both** discard it — the interpreter by passing `ILLUMINATION_RUNLIGHTS`
where `ILLUMINATION_RUNCATLIGHTS` would be expected, the JIT by never reading
operand 5. So divergence 2 has no reachable effect on any form the JIT
lowers, and convergence changes no JIT output.

Divergence 1 (cache validity) affects only *how often* lights are re-run, not
what they compute; adopting the macro's stricter predicate makes the JIT
re-run lights in cases it currently skips, which is output-neutral by
construction.

US3 therefore proceeds under FR-009's refactor exemption: **no STOP required**,
full before/after verification still required.

---

## 4. Out of scope (recorded, not fixed)

Three defects surfaced while establishing this contract. None is actioned by
this feature; each is a spec-013 candidate and would need its own STOP.

1. `IlluminationCat2` (6-operand) discards the category in the **interpreter**
   — a latent interpreter defect that also fails FR-002's "empirical
   reproduction required" bar until someone renders it.
2. The JIT emitter drops the loop scaffolding entirely for the 3- and
   4-operand `illuminance` forms — a pre-existing opcode-coverage gap of
   exactly spec 011's kind.
3. The macro-vs-method cache-validity divergence, if the method's (better)
   form is ultimately preferred, is an interpreter change requiring FR-011
   process.

---

## 5. Verification

| Obligation | How it is checked |
|---|---|
| Exactly one implementation remains (SC-008) | Grep for the **retired symbols specifically** — `grep -rn "CShadingContext::runLights\|CShadingContext::runCategoryLights\|::runCategoryLights" src/` — and confirm no definition and no call site, plus their removal from `shading.h`. A bare `grep -rn runLights src/` is **not** valid evidence: §3 deliberately keeps `runLights`/`runCategoryLights` as the interpreter's macro *wrappers* in `execute.cpp:422-517`, so a bare grep returns live code by design. For the same reason the converged entry point must take a **different** name (e.g. `iterateLights`) — reusing either retired name makes this check unrunnable |
| Interpreter bit-unchanged | Full `-rslo` visual suite against **US3's own** before-pair (`baselines/us3-before-visual.txt`, captured immediately before the convergence rebuild — not the feature-level Stage 0 baseline, which by then may describe a tree US1/US2 have already changed): zero differences, not "within noise" |
| JIT unchanged | Full `-slo` visual suite against the same US3 before-pair, differences within the noise floor (SC-007) |
| Both backends reach it | An `illuminance`-using shader (the lit visual scenes) renders correctly under both `shaderformat` settings |

**Flip trigger.** If implementation shows the single entry point cannot
preserve both backends' observable behaviour simultaneously, the spec's edge
case applies — "cannot be done without changing observable behavior → FR-011
process" — and US3 stops for maintainer disposition rather than proceeding as
a refactor.
