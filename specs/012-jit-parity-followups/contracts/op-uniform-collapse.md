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

**Audit status: NOT YET DISCHARGED.** This guarantee is asserted, not verified;
producing the exception list is an implementation task, and no instruction may
be collapsed at a family until its callee has been checked. A scan of
`rslOps.cpp` for uses of `n` outside a loop bound already names the candidates
that must be resolved:

| Site | Use of `n` | Disposition to establish |
|---|---|---|
| `rslOps.cpp:803` | `*numPassive = n;` | conditional/state op, not an arithmetic op — confirm it is outside FR-004's families |
| `rslOps.cpp:1093`, `1102`, `1112` | `op_area` / `op_calculatenormal` / `op_depth` forward `n` into a `ctx->jit*` method | derivative-dependent: a grid width is semantically required, so collapsing is likely **invalid** even if the operands classify uniform |
| `rslOps.cpp:836`, `842`, `888`, `894`, `903`, `914`, `920` | light/illuminate/solar ops forward `n` to context methods | `DEFLIGHTFUNC` family, already excluded by §4 — confirm none reaches a collapsible family |
| `rslOps.cpp:555`, `584`, `1094`, `1103` | `if (n > 0 && ACTIVE(tags, 0))` | reads element 0 explicitly; verify it is correct under `n == 1, tags == nullptr` |

An op that cannot honour the guarantee is not a blocker: it is simply excluded
from the collapse and recorded, the same way `DEFLIGHTFUNC` is in §4.

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
| Collapse is emitted where and only where the instruction is uniform | Inspect emitted LLVM IR for a uniform-dense shader (`array_ops_probe`): uniform instructions call with `i32 1` and `ptr null` |
| Output unchanged | Full `-slo` visual suite against the unchanged-binary baseline; differences must not exceed the noise floor (SC-007) |
| Active/inactive state preserved | A scene exercising a uniform instruction inside a conditional where early points are inactive — the case that discriminates `tags = nullptr` from live tags |
| Performance effect | `ctest -L perf-manual` on a quiescent machine, judged against the run-to-run variance baseline (SC-004, SC-006) |

**Known risk to Obligation 2**: today's JIT already differs from the
interpreter when *every* point in a block is inactive — the interpreter writes
once, the JIT writes nothing. This contract moves the JIT onto the
interpreter's semantics, so an observed difference is a JIT correction toward
the reference. Per SC-007 that is a maintainer STOP for disposition, never a
unilateral reference-image regeneration ([research.md D5](../research.md)).
