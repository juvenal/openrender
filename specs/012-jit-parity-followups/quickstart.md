# Quickstart: validating JIT/Interpreter Parity Follow-ups

**Feature**: `specs/012-jit-parity-followups` | **Date**: 2026-08-24

A runnable validation guide. Every step below is a command you can execute in
this worktree, ordered so that each story's failing evidence is captured
*before* its fix exists (Constitution Principle III). Implementation details
belong in `tasks.md`; this file is how you tell whether the feature works.

**Worktree**: `/Volumes/Projects/Development/CLI/openrender-worktrees/012-jit-parity-followups`
— run everything from here. Do not `cd` to the main checkout.

---

## Prerequisites

```bash
cmake --build build --config Release
```

### P1. The `.slo` staleness check — do this before trusting any `-slo` result

Nothing in the build graph regenerates `.slo` bitcode, in either direction: an
emitter edit does not rebuild `oshader`, and rebuilding `oshader` does not
recompile any `.slo`. A stale `.slo` against changed runtime C++ is an ABI
mismatch caught at neither build nor link time — it reads garbage arguments at
JIT call sites.

```bash
# Every tracked .slo, the oshader binary, and the emitter sources, newest last.
stat -f '%Sm %N' -t '%F %T' \
  shaders/*.slo \
  build/src/oshader/oshader \
  src/libshader/compiler/llvmEmitter.cpp \
  src/libshader/shading/rslOps.cpp | sort
```

Every `.slo` must postdate both `oshader` and the emitter/runtime sources. To
regenerate:

```bash
cmake --build build --target oshader
build/src/oshader/oshader --jit -o shaders/<name>.slo shaders/<name>.sl
# shaders that #include .slh headers need the env var, NOT -I:
SHADERS_INCLUDE="$(pwd)/shaders/includes" \
  build/src/oshader/oshader --jit -o shaders/<name>.slo shaders/<name>.sl
```

Then refresh the deploy-tree copy under `openrender/shaders/` as well.

### P2. Render command

```bash
SHADERS="$(pwd)/openrender/shaders" \
ORENDERHOME="$(pwd)/openrender" \
DISPLAYS="$(pwd)/openrender/displays" \
GEOMETRIES="$(pwd)/openrender/geometry" \
build/src/orender/orender <rib>
```

---

## Stage 0 — Baseline (serial; blocks all three stories)

Nothing may land before this completes. If any change lands first, its result
silently becomes the next story's "before" and SC-003 can no longer be
evaluated.

```bash
# 0.1  Regression baseline, unchanged binary
ctest --test-dir build -L libshader --output-on-failure  2>&1 | tee /tmp/base-libshader.txt
ctest --test-dir build -L visual    --output-on-failure  2>&1 | tee /tmp/base-visual.txt
```

**Expected**: record the exact pass/fail set. Pre-existing failures are part of
the baseline, not a blocker — but they must be *recorded* so a "newly failing"
test can be distinguished later.

```bash
# 0.2  Run-to-run VARIANCE baseline — quiescent machine, nothing else running.
#      Repeat and record the spread. This does not exist yet; spec 011
#      reported single-run ratios only.
for i in 1 2 3 4 5; do
  ctest --test-dir build -L perf-manual --output-on-failure 2>&1 | tee /tmp/perf-var-$i.txt
done
```

**Expected**: per scene, a min/max/spread of the JIT-to-interpreter ratio.
SC-004 and SC-006 are both defined relative to this number; without it neither
can be judged.

```bash
# 0.3  Pre-change ratios — same quiescent session as 0.2
ctest --test-dir build -L perf-manual --output-on-failure 2>&1 | tee /tmp/perf-before.txt
```

Record, per scene, the ratio **and** its uniform-computation density (SC-004
requires the density recorded alongside, so classification is auditable rather
than retrofitted):

| Scene | Shader | Density | Ratio (before) | Variance |
|---|---|---|---|---|
| `sphere-cfrom` | `show_st_hsv` | near-zero — the control | | |
| `sphere-ctransform` | `show_ctransform` | low | | |
| `sphere-matrixops` | `matrix_ops_probe` | high | | |
| `sphere-comparisonlogic` | `comparison_logic_probe` | high | | |
| `sphere-arrayops` | `array_ops_probe` | highest | | |
| `sphere-gather` | `gather_named_probe` | mixed | | |

---

## Stage 1 — US1: the `usfroma` crash (P1)

### 1.1 Red — reproduce the crash (FR-002)

Write a minimal probe shader performing a varying-index read of a
`uniform string` array consumed directly in an expression — e.g. the shape
recorded at `specs/011-jit-opcode-parity/triage-results.md:85`,
`if (usarr[findex] == "a")`. Note the constraint that shapes it:
`rsloStringSpecifier` (`rslo.y:342-347`) forces `SLC_UNIFORM` onto a bare
`string`, so no RSL string *variable* can hold a varying value — the read must
be consumed inline.

```bash
build/src/oshader/oshader -o shaders/<probe>.rslo shaders/<probe>.sl
for i in 1 2 3 4 5; do
  <render command> examples/rib/tests/<probe>-reyes.rib ; echo "run $i exit=$?"
done
```

**Expected (before the fix)**: abnormal termination in at least 5 of 5 runs.
If it proves intermittent, record the observed rate — SC-001 admits any
non-zero pre-fix failure rate paired with a 100% post-fix pass rate.

**Gate**: this reproduction is the authorization for the fix. Do not edit any
source before it exists.

### 1.2 STOP — mandatory maintainer checkpoint

Present: the reproduction, the diagnosed root cause, whether it lies in the
interpreter or in the compiler, and the narrowest proposed change. If the fix
is compiler-side, the presentation must additionally state that it alters the
meaning of already-compiled artifacts for **both** backends. Wait for explicit
confirmation.

### 1.3 Green

```bash
cmake --build build --config Release
# regenerate BOTH artifacts for the probe, then:
for i in 1 2 3 4 5; do
  <render command> examples/rib/tests/<probe>-reyes.rib ; echo "run $i exit=$?"
done
ctest --test-dir build -R <probe> --output-on-failure
ctest --test-dir build -L libshader --output-on-failure
ctest --test-dir build -L visual    --output-on-failure
```

**Expected**: 5/5 normal completion (SC-001); the new scene passes and would
fail if the defect returned (SC-002); zero newly-failing tests versus
`/tmp/base-*.txt` (SC-003).

**Constraint**: coverage arrives as a **new** shader, a **new** scene pair, and
**one new** reference image. No existing reference image is modified — every
existing scene stays a control (SC-007).

---

## Stage 2 — US2: uniform-dispatch collapse (P2)

### 2.1 Red

Already captured as Stage 0.2 + 0.3. That *is* the failing evidence: the
JIT-to-interpreter ratio exceeds 1.0 on uniform-dense scenes.

### 2.2 Discharge the callee audit — before emitting any collapse

```bash
grep -nE "\bn\b" src/libshader/shading/rslOps.cpp \
  | grep -vE "i < n|i<n|int n|\* n|n \*|n\)|numVerts"
```

Every hit is an op that uses `n` for something other than a loop bound and must
be cleared or excluded before its family is collapsed — see
[contracts/op-uniform-collapse.md](./contracts/op-uniform-collapse.md) §2.2 for
the candidate list and the expected dispositions. `op_area`,
`op_calculatenormal`, and `op_depth` are derivative-dependent and are the ones
most likely to end up excluded rather than cleared.

### 2.3 Verify the emitted form (the detail most likely to be got wrong)

After implementing, inspect the emitted IR for a uniform-dense shader:

```bash
build/src/oshader/oshader --jit -o /tmp/probe.slo shaders/array_ops_probe.sl
# dump/inspect the module's op_* call sites
```

**Expected**: uniform-classified instructions call with `i32 1` **and**
`ptr null`. A call with `i32 1` and a live tag pointer is a contract violation
— see [contracts/op-uniform-collapse.md](./contracts/op-uniform-collapse.md)
§2.3. It produces correct output wherever vertex 0 is active and diverges only
inside conditionals, so it will pass casual testing.

### 2.4 Green — output must not move

```bash
# staleness check FIRST (Prerequisites P1), then:
ctest --test-dir build -L visual --output-on-failure
```

**Expected**: zero differences above the noise floor versus the unchanged-binary
baseline (FR-005, SC-007).

**If a difference appears**: it is most likely the known corner — for a uniform
instruction inside an all-inactive block, today's JIT writes nothing while the
interpreter writes once. The change moves the JIT *onto* the reference, so the
difference is a JIT correction. **Stop and present it** for disposition. Never
regenerate a reference image unilaterally (SC-007 admits no exceptions).

Additionally verify a scene where a uniform instruction sits inside a
conditional with early points inactive — that is the case which discriminates
`tags = nullptr` from a live tag pointer.

### 2.5 Green — performance

```bash
# Exclusive, quiescent machine. Nothing else compiles or renders.
ctest --test-dir build -L perf-manual --output-on-failure 2>&1 | tee /tmp/perf-after.txt
```

**Expected**:
- SC-004: ratio improves by more than that scene's variance on 100% of scenes
  with meaningful uniform computation; zero scenes regress. `sphere-cfrom`
  showing no measurable change is a **conforming** outcome, not a failure.
- SC-006: the `sphere-arrayops` vs `sphere-cfrom` gap, at identical scale,
  narrows by more than the variance of that comparison. Pass/fail with no
  magnitude floor; report the magnitude either way.
- SC-005: report per scene whether the JIT reached ≤90% of the interpreter.
  Reported, not gated — where unmet, identify the residual dominant cost.

---

## Stage 3 — US3: light-iteration convergence (P3)

### 3.1 Red

The before/after render set. Capture the full `-rslo` and `-slo` visual suite
against the unchanged binary — Stage 0.1 already did this; reuse it.

### 3.2 Green

```bash
cmake --build build --config Release
# .slo staleness check (P1) — this change touches shading runtime
ctest --test-dir build -L visual    --output-on-failure
ctest --test-dir build -L libshader --output-on-failure
```

**Expected**:
- Interpreter (`-rslo` scenes): **bit-unchanged** — zero differences, not
  "within noise". The converged implementation reproduces the macro form's
  semantics precisely (FR-011).
- JIT (`-slo` scenes): within the noise floor.
- SC-008: exactly one implementation remains. Verify by grepping for the
  retired form's name and finding no definition and no call site.

**Flip trigger**: if the single entry point cannot preserve both backends'
observable behaviour, stop — the spec's edge case routes US3 into the FR-011
process instead of the refactor exemption.

---

## Stage 4 — Feature-level acceptance

| Criterion | Command / evidence |
|---|---|
| SC-001 | Stage 1.1 and 1.3 run logs (5 runs each) |
| SC-002 | `ctest -R <probe>` passes; removing the fix makes it fail |
| SC-003 | Three independent before/after pairs of `/tmp/base-*.txt` — one per change, not one for the feature |
| SC-004 / SC-005 / SC-006 | `/tmp/perf-var-*.txt`, `/tmp/perf-before.txt`, `/tmp/perf-after.txt` + the filled density table |
| SC-007 | Full visual suite after each change; `git status` shows **no** modified file under `tests/visual/reference/` — only the one new US1 image |
| SC-008 | Grep evidence that the retired light-iteration form is gone |
| FR-012 | `DEVNOTES_DETAILS/BUGS.md`, `DEVNOTES_DETAILS/OSHADER_UPDATES.md`, and spec 011's records updated. Hugo `site/` is **not** updated (exempt per the spec's Assumptions) |

### Standing rules during validation

- No automatic commits at any point. Commit only when explicitly asked.
- The US1 STOP is mandatory before any interpreter or compiler source edit.
- Every timing run is exclusive: nothing else compiles or renders on the
  machine.
- `stat` every `.slo` before trusting any `-slo` result. A green `-slo` run
  after an emitter change proves nothing unless the bitcode postdates both the
  emitter edit and the `oshader` rebuild.
