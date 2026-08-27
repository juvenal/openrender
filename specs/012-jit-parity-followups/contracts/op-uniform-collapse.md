# Contract: `op_*` uniform-collapse calling convention

**Feature**: `specs/012-jit-parity-followups` (US2 / FR-004, FR-005, FR-006, FR-007)
**Surface**: the C-linkage `op_*` / `rsl_*` functions in
`src/libshader/shading/rslOps.{h,cpp}`, called from JIT-emitted code
**Status**: normative for this feature

This is an *existing* interface. This contract does not change its signatures;
it fixes the meaning of an argument combination the JIT has never used, so
that the JIT can express what the interpreter already does.

---

## 1. The interface as it stands

Every batched op ends its parameter list with a shading-point count and a tag
pointer:

```c
void op_addff(float* dst, int sd,
              const float* a, int sa,
              const float* b, int sb,
              int n, const int* tags);
```

and guards each element with:

```c
#define IDX(base, str, i)  ((base) + (str) * (i))
#define ACTIVE(tags, i)    (!(tags) || (tags)[i] == 0)
```
(`rslOps.cpp:40-43`)

`ACTIVE` is already null-tolerant: **`tags == nullptr` means "no tag
filtering"**, for every op, today. No source change is needed to make the
collapsed form legal.

---

## 2. The contract

### 2.1 Caller obligations (the JIT emitter)

When, and only when, an instruction is classified uniform — that is, when
`dstStride == 0` and every operand stride returned by `getVar()` is `0`
(`llvmEmitter.cpp:442`, `598`; equivalence to the interpreter's
`code->uniform` established in [research.md D1](../research.md)) — the emitted
call MUST pass:

| Argument | Value | Rationale |
|---|---|---|
| `n` | `1` | one execution, matching the interpreter's uniform arm |
| `tags` | **`nullptr`** | the interpreter's uniform arm performs no tag test |

All other arguments are unchanged: same pointers, same strides, same function.

### 2.2 Callee guarantees (every `op_*`)

Given `n == 1` and `tags == nullptr`, an op MUST:

1. Execute its body exactly once.
2. Read operand element 0 and write destination element 0 — which, with
   stride 0, is the single uniform slot.
3. Perform no tag test (`ACTIVE` is unconditionally true).
4. Leave `numActive` / `numPassive` and all conditional-nesting state
   untouched, exactly as in the varying case (FR-006).

Ops that follow the `IDX`/`ACTIVE` idiom satisfy this without modification.
Any op that dereferences `tags` outside `ACTIVE`, or that assumes `n` equals a
grid width, violates the contract.

**Audit status: DISCHARGED (T023/T024, 2026-08-26).** Every candidate
surfaced by `grep -nE "\bn\b" src/libshader/shading/rslOps.cpp | grep -vE
"i < n|i<n|int n|\* n|n \*|n\)|numVerts"` has been read in context and
resolved below. Line numbers are current-file line numbers (they have
drifted a few lines from the numbers first cited when this contract was
authored, due to unrelated earlier edits in this same feature branch —
content, not position, is the identity of each candidate).

| Site | Use of `n` | Disposition |
|---|---|---|
| `rslOps.cpp:461` (`op_endif_update`) | `if (n > 0)` | **Cleared.** Guards a `log_debug(...)` diagnostic only; the functional loop above it is unaffected. Safe under `n==1, tags==nullptr`. |
| `rslOps.cpp:477,484,494` (`op_ambient_batch`/`op_diffuse_batch`/`op_specular_batch`) | `(void)n; (void)tags;` | **Excluded.** These are `DEFLIGHTFUNC`-adjacent batch wrappers — they ignore `n`/`tags` entirely and delegate straight to `ctx->callAmbient`/`callDiffuse`/`callSpecular`. Not a collapse-eligible dispatch site (already outside §4's applicable families). |
| `rslOps.cpp:~500` (`op_lightsource_f`) | `(void)n; (void)tags;` | **Excluded.** Lighting-query builtin; ignores `n`/`tags` entirely and always writes index 0. `DEFLIGHTFUNC`-adjacent, same reasoning as the batch wrappers above. |
| `rslOps.cpp:557` (`op_pfrom`), `586` (`op_ptransform`) | `if (n > 0 && ACTIVE(tags, 0))` | **Cleared.** Guards a `log_debug(...)` diagnostic printing element 0 only; the per-vertex transform loop below both sites already uses the `ACTIVE(tags,i)` idiom correctly and is unaffected. `ACTIVE(nullptr,0)` is `true` (§1), so the guard still fires exactly once under the collapsed form — cosmetic difference only (logs vertex 0 instead of being suppressed), no correctness impact. |
| `rslOps.cpp:805` (`op_forend`) | `*numPassive = n;` | **Excluded.** `op_forend`/`op_for_check`/`op_for_break` are `for`/`while` loop-scope-management opcodes (`FOR3EXPR_PRE`/`FOREND3EXPR_PRE` mirrors), not `DEFOPCODE`/`DEFFUNC`/`DEFSHORTOPCODE`/`DEFSHORTFUNC` arithmetic/builtin dispatch sites. They always operate over the full grid width to track per-vertex loop-tag state and are outside §4's applicable-families table entirely — never a target of the collapse regardless of any instruction's uniform classification. |
| `rslOps.cpp:837,844,890,896,905,916,922` (`op_illuminance_begin/next`, `op_gather_begin/else/end`, `op_illuminate_begin/end`, `op_illuminate3_begin`, `op_solar_begin/end`) | forward `n`/`tags` to `ctx->jit*` scope methods | **Excluded.** `DEFLIGHTFUNC`/loop-scaffolding constructs (illuminance/gather/illuminate/solar), already excluded by §4 — the interpreter treats a uniform classification here as `scripterror("Invalid uniform lighting call")`, so there is no run-once semantics to mirror. None reaches a collapsible family. |
| `rslOps.cpp:1101` (`op_area`), `1110` (`op_calculatenormal`), `1120` (`op_depth`) | forward `n` into `ctx->jitArea`/`jitCalculateNormal`/`jitDepth` | **Excluded — derivative-dependent.** These compute finite differences (`Du`/`Dv`) across neighboring grid points; `n` is a real grid-width requirement, not merely an iteration count. Collapsing to `n=1` would be semantically invalid even where every operand classifies uniform (in practice `P` is always varying, so these are never uniform-classified in the first place — the exclusion is defense-in-depth, not a live gap). |
| `rslOps.cpp:1102`, `1111` (`op_area`, `op_calculatenormal`) | `if (n > 0 && ACTIVE(tags, 0))` | **Cleared** (same reasoning as `op_pfrom`/`op_ptransform` above) — debug-log-only, moot anyway since these sites are already excluded above. |

An op that cannot honour the guarantee is not a blocker: it is simply excluded
from the collapse and recorded, the same way `DEFLIGHTFUNC` is in §4. No
candidate above requires an `rslOps.cpp` code change — every exclusion is
because the site is outside §4's applicable families (loop scaffolding,
`DEFLIGHTFUNC`, or genuinely derivative-dependent), not because it violates
the `IDX`/`ACTIVE` idiom.

**Extension to `shading.cpp` (2026-08-26, closing a gap in the initial
audit):** §4 names `environment`×2, `shadow`×2, `bake3d` as the
`DEFSHORTFUNC` family the collapse reaches. Their `op_*` wrappers
(`op_texture_f/c`, `op_environment_f/c`, `op_shadow_f`, `rslOps.cpp:1128-1163`)
are thin forwarders to `CShadingContext::jitTextureF/C`, `jitEnvironmentF/C`,
`jitShadowF` in `src/libshader/shading/shading.cpp:2451-2520` — a *different
translation unit* from `rslOps.cpp`, so `ACTIVE` (a `#define`d,
translation-unit-local macro) does not apply there and had to be checked
independently rather than inferred. Read in full:

| Site | Tag test | Disposition |
|---|---|---|
| `shading.cpp:2451` `jitTextureF`, `2466` `jitTextureC` | `if (!tags \|\| !tags[i])` | **Cleared.** Same null-tolerant idiom as `ACTIVE` — `tags==nullptr` skips the filter, safe under `n==1`. |
| `shading.cpp:2479` `jitEnvironmentF`, `2494` `jitEnvironmentC` | `if (!tags \|\| !tags[i])` | **Cleared.** Same idiom. |
| `shading.cpp:2507` `jitShadowF` | `if (!tags \|\| !tags[i])` | **Cleared.** Same idiom. |
| `shading.cpp:2440` `jitDepth` (already excluded above as derivative-dependent, listed here only because it uses the same idiom) | `if (!tags \|\| !tags[i])` | Idiom is safe; exclusion above stands for the independent derivative-dependence reason. |
| `bake3d` | — | **Moot.** No `jit*`/`op_*` JIT wrapper exists for `bake3d` anywhere in `rslOps.{h,cpp}` or `shading.{h,cpp}` — grepped, zero matches. §4 names it as collapse-eligible on paper, but there is currently nothing to collapse; not a live gap, and not something this feature needs to add (out of scope — FR-004 covers the collapse of existing dispatch, not authoring new JIT builtins). |

Audit is now complete across both translation units the DEFSHORTFUNC
collapse touches. No code change required in either file.

### 2.3 The prohibition

**`n = 1` with a non-null `tags` is forbidden.**

This is the failure mode most likely to survive casual testing. With a live
`tags`, the op evaluates `ACTIVE(tags, 0)` — vertex 0's active state — and
skips the write whenever vertex 0 is inactive. The interpreter, in the same
situation, writes anyway:

```c
if (code->uniform) {
    expr;                     /* once, NO tag test */
} else if (numPassive != 0) {
    for (...) { if (*tags == 0) { expr; } expr_update; }
} else {
    for (...) { expr; expr_update; }
}
```
(`execute.cpp:610-718`)

A build using `n = 1` with live tags produces correct output on every scene
where vertex 0 happens to be active, and diverges only inside conditionals.

---

## 3. What this contract does NOT change

- **No signature changes.** Adding a `uniform` parameter to the `op_*` family
  would alter the C-linkage ABI of every op. Because nothing in the build
  graph regenerates `.slo` bitcode, every already-compiled `.slo` would read
  garbage arguments at JIT call sites — caught at neither build nor link time.
  This contract exists specifically to avoid that.
- **No new symbols.** No `op_*` is added, so macOS JIT symbol resolution
  (`DynamicLibrarySearchGenerator::GetForCurrentProcess()`) is unaffected.
- **No math.** The collapse changes call width only; the function called and
  the arithmetic it performs are identical (FR-010).
- **No varying-path behaviour.** Non-uniform instructions dispatch exactly as
  before, including the pre-existing `numVerts`-vs-`numRealVertices`
  asymmetry at SHORT-family sites, which stays out of scope
  ([research.md D4](../research.md)).

---

## 4. Applicability (FR-004 scope)

The collapse applies at every family where the interpreter short-circuits:

| Interpreter family | Short-circuits? | Collapse applies |
|---|---|---|
| `DEFOPCODE` | yes | yes |
| `DEFFUNC` | yes | yes |
| `DEFSHORTOPCODE` | yes | yes — but the family has **zero real uses** in the tree |
| `DEFSHORTFUNC` | yes | yes — `environment` ×2, `shadow` ×2, `bake3d` |
| `DEFLIGHTFUNC` | **no** | **excluded** — treats a uniform classification as an error: `scripterror("Invalid uniform lighting call")`. There is no run-once semantics to mirror. This is FR-004's required explicit exclusion, with its reason |

---

## 5. Verification

| Obligation | How it is checked |
|---|---|
| §2.2 callee audit discharged | The `rslOps.cpp` exception list in §2.2 is resolved, each candidate either cleared or excluded and recorded, **before** any collapse is emitted at its family |
| Collapse is emitted where and only where the instruction is uniform | Dump the emitted LLVM IR for a uniform-dense shader (`array_ops_probe`) using the IR-dump mechanism established in `tasks.md` T003d — `oshader` has no IR-dump flag and `llvm-dis` is not installed in this environment, so this obligation has no tool until T003d passes. Uniform instructions must call with `i32 1` **and** `ptr null` |
| Delegation preserved (FR-010) | In the same dump, the callee at each collapsed site is the **same `op_*` symbol** the pre-change dump (T006a) named at that site; only `n` and `tags` differ. A changed callee is a reimplementation, and no image test detects it while the substitute agrees |
| Output unchanged | Full `-slo` visual suite against the unchanged-binary baseline; differences must not exceed the noise floor (SC-007) |
| Active/inactive state preserved | The FR-006 discrimination scene — a uniform instruction inside a conditional where early points are inactive, the case that discriminates `tags = nullptr` from live tags. Its shader, scene pair, and reference image are authored **before** the collapse exists (`tasks.md` T008b) and only *run* afterwards (T035); a reference generated after the collapse records the collapse's own output and can never fail |
| Performance effect | `ctest -L perf-manual` on a quiescent machine, judged against the run-to-run variance baseline (SC-004, SC-006) |

**Known risk to Obligation 2**: today's JIT already differs from the
interpreter when *every* point in a block is inactive — the interpreter writes
once, the JIT writes nothing. This contract moves the JIT onto the
interpreter's semantics, so an observed difference is a JIT correction toward
the reference. Per SC-007 that is a maintainer STOP for disposition, never a
unilateral reference-image regeneration ([research.md D5](../research.md)).
