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

### P1. Provision the artifact set — this worktree starts with none

**Verified 2026-08-25**: `shaders/` contains only `.sl` sources (65 files) plus
`includes/` — **zero `.slo`, zero `.rslo`** — and the `openrender/` deploy tree
**does not exist**. Nothing here is stale; nothing here exists. The first
version of this guide told you to audit staleness, which on a fresh worktree
silently audits an empty set and reports success.

```bash
cmake --build build --config Release
cmake --build build --target oshader

# Generate BOTH artifacts for every shader. Use SHADERS_INCLUDE, never -I:
# `oshader -I <path> -o <out> <one input>` misparses as "multiple input files".
for f in shaders/*.sl; do
  n="$(basename "$f" .sl)"
  SHADERS_INCLUDE="$(pwd)/shaders/includes" \
    build/src/oshader/oshader       -o "shaders/$n.rslo" "$f"
  SHADERS_INCLUDE="$(pwd)/shaders/includes" \
    build/src/oshader/oshader --jit -o "shaders/$n.slo"  "$f"
done

# Provision the deploy tree the render command reads from:
cmake --install build --prefix "$(pwd)/openrender"
# then copy the freshly generated artifacts into openrender/shaders/
```

Smoke-test before trusting anything: render `examples/rib/camera-dof.rib` with
the P2 command and confirm it writes an image.

**From the first regeneration onward** (US2's emitter change, US3's shading-runtime
change) the staleness audit becomes the real gate. Nothing in the build graph
regenerates `.slo` bitcode in either direction: an emitter edit does not rebuild
`oshader`, and rebuilding `oshader` does not recompile any `.slo`. A stale `.slo`
against changed runtime C++ is an ABI mismatch caught at neither build nor link
time — it reads garbage arguments at JIT call sites.

```bash
# Every tracked .slo, the oshader binary, and the emitter sources, newest last.
stat -f '%Sm %N' -t '%F %T' \
  shaders/*.slo \
  build/src/oshader/oshader \
  src/libshader/compiler/llvmEmitter.cpp \
  src/libshader/shading/rslOps.cpp | sort
```

Every `.slo` must postdate both `oshader` and the emitter/runtime sources, and
the deploy-tree copies must be refreshed alongside the tracked ones.

### P2. Render command

Requires the `openrender/` deploy tree from P1 — without it every path below
resolves to a directory that does not exist, and the renderer fails to find its
shaders and display drivers rather than reporting a missing tree.

```bash
SHADERS="$(pwd)/openrender/shaders" \
ORENDERHOME="$(pwd)/openrender" \
DISPLAYS="$(pwd)/openrender/displays" \
GEOMETRIES="$(pwd)/openrender/geometry" \
build/src/orender/orender <rib>
```

Wherever this guide writes `<render command>`, substitute that five-line block
verbatim.

### P3. LLVM IR dump path

Stage 2.3 needs to read the emitted IR, and **as of 2026-08-25 no mechanism
exists**: `oshader` has no IR-dump flag, and `llvm-dis` is on neither `PATH` nor
`/opt/homebrew/opt/llvm/bin` (`brew --prefix llvm` names that path, but the
directory is absent). Resolve this before US2 emits any collapse — resolution
order and the fallback of adding an `--emit-llvm` flag via `Module::print()` are
specified in `tasks.md` T003d. No emitted-form claim may be made until it works.

---

## Stage 0 — Baseline (serial; blocks all three stories)

Nothing may land before this completes. If any change lands first, its result
silently becomes the next story's "before" and SC-003 can no longer be
evaluated.

All baseline logs go under `specs/012-jit-parity-followups/baselines/` — a
version-tracked directory, **not** `/tmp`. A `/tmp` baseline cannot be diffed
after a reboot, and SC-003 is a before/after comparison that may span days.
Derived comparisons and figures go in `specs/012-jit-parity-followups/measurements.md`.

```bash
mkdir -p specs/012-jit-parity-followups/baselines
B=specs/012-jit-parity-followups/baselines

# 0.1  Regression baseline, unchanged binary
ctest --test-dir build -L libshader --output-on-failure  2>&1 | tee $B/base-libshader.txt
ctest --test-dir build -L visual    --output-on-failure  2>&1 | tee $B/base-visual.txt
```

**Expected**: record the exact pass/fail set. Pre-existing failures are part of
the baseline, not a blocker — but they must be *recorded* so a "newly failing"
test can be distinguished later.

**`$B` is re-assigned at the top of every block below that uses it.** The blocks
in this guide are meant to be copy-pasted individually, and a shell that never
ran block 0.1 would expand `$B` to the empty string and `tee` straight to `/`.

```bash
B=specs/012-jit-parity-followups/baselines

# 0.2  Run-to-run VARIANCE baseline — quiescent machine, nothing else running.
#      Repeat and record the spread. This does not exist yet; spec 011
#      reported single-run ratios only.
#      -V, NOT --output-on-failure: test_perf_compare.cpp prints the ratio to
#      stdout on the PASS path too, and --output-on-failure discards stdout for
#      passing tests — i.e. it would throw away every number you came for.
for i in 1 2 3 4 5; do
  ctest --test-dir build -L perf-manual -V 2>&1 | tee $B/perf-var-$i.txt
done
```

**Expected**: per scene, a min/max/spread of the JIT-to-interpreter ratio.
SC-004 and SC-006 are both defined relative to this number; without it neither
can be judged. Use the **median** of these five runs as each scene's "before"
figure — not a single run.

Note that `add_perf_manual_test` hard-fails any scene whose ratio exceeds
`MAX_RATIO` (default `0.90`), while SC-005 says the 90% target is *reported, not
gated*. Reconcile before relying on these runs — see `tasks.md` T003a. The
`perf-manual` label is outside both `-L libshader` and `-L visual`, so however
it is reconciled, SC-003's regression evidence is unaffected.

```bash
B=specs/012-jit-parity-followups/baselines

# 0.3  Pre-change ratios — same quiescent session as 0.2
ctest --test-dir build -L perf-manual -V 2>&1 | tee $B/perf-before.txt
```

Record, per scene, the ratio **and** its uniform-computation density (SC-004
requires the density recorded alongside, so classification is auditable rather
than retrofitted). Density is **not** an impression: it is the count of
uniform-classified dispatch sites in a collapsible family, per
[contracts/op-uniform-collapse.md](./contracts/op-uniform-collapse.md) §4,
taken from pre-change evidence. SC-004 admits **exactly two** buckets — "has
meaningful uniform computation" or "does not" — and **no third 'mixed' bucket
exists**. Every scene lands in one of the two before any timing is compared.

| Scene | Shader | Uniform-site count | Bucket | Ratio (before, median of 0.2) | Variance |
|---|---|---|---|---|---|
| `sphere-cfrom` | `show_st_hsv` | | expected: *not* meaningful — the control | | |
| `sphere-ctransform` | `show_ctransform` | | | | |
| `sphere-matrixops` | `matrix_ops_probe` | | | | |
| `sphere-comparisonlogic` | `comparison_logic_probe` | | | | |
| `sphere-arrayops` | `array_ops_probe` | | expected: meaningful | | |
| `sphere-gather` | `gather_named_probe` | | **count it and assign one bucket** — do not record "mixed" | | |

```bash
# 0.4  Same-binary IMAGE NOISE FLOOR — SC-007's "within noise" is undefined
#      without it. Render the SAME UNEDITED BINARY TWICE per stochastic
#      raytrace scene and compare the two runs with the project's 8x8
#      block-averaged diff metric (the same metric tests/visual uses).
#      Record per scene the max block-avg and the mean.
```

**Expected**: the same order of magnitude as spec 011's reference figures for
the raytrace probe — max 39.375, mean 0.0193
(`specs/011-jit-opcode-parity/lessons-learned.md:392-396`). Any later
before/after diff at or below this pair is noise, not a change.

```bash
# 0.5  Pre-change EMITTED-FORM evidence and the FR-006 discrimination check —
#      both must exist against the UNCHANGED binary.
```

- Using the P3 dump path, record the `op_*` call sites emitted today for a
  uniform-dense shader: the callee symbol name and the `n`/`tags` arguments at
  each site. Stage 2.3's check is a *diff* against this, and cannot be run
  without it.
- Author the FR-006 discrimination probe now: a shader with a uniform
  instruction inside a conditional whose early points are inactive
  (`shaders/uniform_in_conditional_probe.sl`), its scene pair
  (`examples/rib/tests/sphere-uniform-conditional-reyes.rib` and
  `-reyes-slo.rib`), its reference image, and its `add_visual_test` entry.
  **Generated now, before the collapse exists.** A reference generated
  afterwards is a photograph of whatever the collapse produced and could never
  fail — including for the forbidden `n = 1` with live `tags` form this check
  exists to catch.

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
cp shaders/<probe>.rslo openrender/shaders/          # the render reads the deploy tree
for i in 1 2 3 4 5; do
  SHADERS="$(pwd)/openrender/shaders" \
  ORENDERHOME="$(pwd)/openrender" \
  DISPLAYS="$(pwd)/openrender/displays" \
  GEOMETRIES="$(pwd)/openrender/geometry" \
  build/src/orender/orender examples/rib/tests/<probe>-reyes.rib
  echo "run $i exit=$?"
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

**Immediately after approval and before touching any source**, capture US1's
own before-pair. The tree at this moment still carries the defect and none of
the fix, which is exactly what SC-003 wants recorded:

```bash
B=specs/012-jit-parity-followups/baselines

ctest --test-dir build -L libshader --output-on-failure 2>&1 | tee $B/us1-before-libshader.txt
ctest --test-dir build -L visual    --output-on-failure 2>&1 | tee $B/us1-before-visual.txt
```

Do this **here**, not after the fix. US1's fix goes live mid-sequence, so a
capture taken later in Stage 1 photographs the fixed tree and the closing
comparison would diff it against itself. (US2 and US3 rebuild at the *end* of
their stages, so their captures sit just before that rebuild — same rule,
different position. Do not copy their placement across.)

The Stage 0 `base-*.txt` is not a substitute: it describes a tree the other
stories may since have changed.

### 1.3 Green

```bash
cmake --build build --config Release
# Regenerate BOTH artifacts for the probe (and, if the fix was compiler-side,
# for EVERY shader — a compiler change alters the meaning of already-compiled
# artifacts for both backends), then refresh the deploy-tree copies.
#
# COMPILER-SIDE ONLY: the us1-before-* pair above predates this regeneration,
# so its -slo rows describe artifacts this change has just invalidated.
# Re-capture it here with the fix reverted and the artifacts rebuilt from the
# reverted compiler — reuse the same transient revert the SC-002 negative
# evidence below needs. Interpreter-side: no re-capture, the pair stands.

for i in 1 2 3 4 5; do
  SHADERS="$(pwd)/openrender/shaders" \
  ORENDERHOME="$(pwd)/openrender" \
  DISPLAYS="$(pwd)/openrender/displays" \
  GEOMETRIES="$(pwd)/openrender/geometry" \
  build/src/orender/orender examples/rib/tests/<probe>-reyes.rib
  echo "run $i exit=$?"
done
ctest --test-dir build -R <probe> --output-on-failure
ctest --test-dir build -L libshader --output-on-failure
ctest --test-dir build -L visual    --output-on-failure
```

**Expected**: 5/5 normal completion (SC-001); the new scene passes and would
fail if the defect returned (SC-002); zero newly-failing tests versus
`$B/us1-before-*.txt` (SC-003). The `-L visual` run legitimately shows **one**
test entry the before-pair lacks — the probe scene, whose reference image can
only be generated from a build where the crash is fixed. One added entry and no
other test-set difference is the pass condition; see the expected-delta table in
`tasks.md` § Standing rules.

**SC-002 needs negative evidence, not just a passing test.** Temporarily revert
the fix locally, confirm the new scene fails, then restore. Use a temporary WIP
commit or `git stash push -u -m "<unique-tag>"` + `git stash apply <sha>` —
**never bare `git stash` / `git stash pop`**: the stash stack is shared with the
main checkout and every other worktree.

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

After implementing, dump the emitted IR for a uniform-dense shader using the
**P3 dump path** — `llvm-dis` is not available in this environment, so a plain
`.slo` on disk is not inspectable without it:

```bash
build/src/oshader/oshader --jit -o /tmp/probe.slo shaders/array_ops_probe.sl
# then dump the module's op_* call sites via the P3 / T003d mechanism
```

**Expected — two independent checks against the Stage 0.5 pre-change dump:**

1. **Collapse form.** Uniform-classified instructions call with `i32 1` **and**
   `ptr null`. A call with `i32 1` and a live tag pointer is a contract
   violation — see
   [contracts/op-uniform-collapse.md](./contracts/op-uniform-collapse.md) §2.3.
   It produces correct output wherever vertex 0 is active and diverges only
   inside conditionals, so it will pass casual testing.
2. **Delegation (FR-010).** At each collapsed site, the callee named in the IR
   is the **same `op_*` symbol** the Stage 0.5 dump named at that site. Only the
   `n` and `tags` arguments differ. A changed callee means the collapse
   re-routed the computation rather than short-circuiting it, which is the
   reimplementation FR-010 forbids — and no image test would catch it as long
   as the substitute happens to agree.

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

Additionally **run** the FR-006 discrimination scene authored in Stage 0.5 — a
uniform instruction inside a conditional with early points inactive, the case
that discriminates `tags = nullptr` from a live tag pointer. US2 only runs it;
it must not be authored or re-referenced here, or it stops being a check.

### 2.5 Green — performance

```bash
B=specs/012-jit-parity-followups/baselines

# Exclusive, quiescent machine. Nothing else compiles or renders.
# -V again, for the same reason as 0.2: the ratio is printed on the PASS path.
ctest --test-dir build -L perf-manual -V 2>&1 | tee $B/perf-after.txt
```

**Expected** (all comparisons against the **median** of the 0.2 runs, not
against `perf-before.txt` as a single sample):
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

Capture the full `-rslo` and `-slo` visual suite into `$B/us3-before-*.txt`
**immediately before** the rebuild that makes US3's source edits live. Do not
reuse Stage 0.1: by this point US1 and/or US2 may have landed, so `base-*.txt`
describes a different tree than the one US3 is changing.

### 3.2 Green

```bash
cmake --build build --config Release
# Regenerate every .slo + its deploy-tree copy, then run the P1 stat audit —
# this change touches the shading runtime, so every .slo's ABI is in question.
# NOTE: this regeneration and US2's (Stage 2.4) are the SAME operation.
# Whichever story reaches it first regenerates; the second skips regeneration
# and re-runs BOTH stories' verification steps against the one combined set.
ctest --test-dir build -L visual    --output-on-failure
ctest --test-dir build -L libshader --output-on-failure
```

**Expected**:
- Interpreter (`-rslo` scenes): **bit-unchanged** — zero differences, not
  "within noise", versus `$B/us3-before-visual.txt`. The converged
  implementation reproduces the macro form's semantics precisely (FR-011).
- JIT (`-slo` scenes): within the noise floor.
- SC-008: exactly one implementation remains. Grep for the **retired method
  symbols specifically**:

  ```bash
  grep -rn "CShadingContext::runLights\|CShadingContext::runCategoryLights\|::runCategoryLights" src/
  ```

  A bare `grep -rn runLights src/` is **not** valid evidence: the interpreter
  keeps `runLights`/`runCategoryLights` as macro *wrappers* in
  `src/libshader/shading/execute.cpp:422-517` by design, so a bare grep returns
  live code and reads as a failed removal. This is also why the converged entry
  point must be given a *different* name (e.g. `iterateLights`) — see
  [contracts/light-iteration.md](./contracts/light-iteration.md).

**Flip trigger**: if the single entry point cannot preserve both backends'
observable behaviour, stop — the spec's edge case routes US3 into the FR-011
process instead of the refactor exemption.

---

## Stage 4 — Feature-level acceptance

All evidence lives in `specs/012-jit-parity-followups/` — run logs under
`baselines/`, derived figures and comparisons in `measurements.md`. Nothing is
accepted from `/tmp`.

| Criterion | Command / evidence |
|---|---|
| SC-001 | Stage 1.1 and 1.3 run logs (5 runs each), recorded in `measurements.md` |
| SC-002 | `ctest -R <probe>` passes **and** the temporary-revert run in Stage 1.3 shows it failing without the fix |
| SC-003 | Three independent before/after pairs — `baselines/us1-before-*.txt`, `baselines/us2-before-*.txt`, `baselines/us3-before-*.txt`, each captured immediately before its own story's change goes live — which for US1 is right after the STOP (mid-stage), and for US2/US3 is just before their end-of-stage rebuild. One per change, not one for the feature; the Stage 0 `base-*.txt` is the feature-level control, not any story's "before" |
| SC-004 / SC-005 / SC-006 | `baselines/perf-var-1..5.txt` (median = the "before" figure), `baselines/perf-before.txt`, `baselines/perf-after.txt` + the filled density table with every scene in one of exactly two buckets. SC-005 reported, not gated |
| SC-007 | Full visual suite after each change, compared against the Stage 0.4 noise floor; `git status` shows **zero modified** files under `tests/visual/reference/` and **exactly two added** — the US1 probe image and the FR-006 discrimination image |
| SC-008 | The specific-symbol grep from Stage 3.2, its output recorded verbatim in `measurements.md` |
| FR-012 | `DEVNOTES_DETAILS/BUGS.md`, `DEVNOTES_DETAILS/OSHADER_UPDATES.md`, `DEVNOTES.md`, and spec 011's records updated — **and** `docs/site/content/development/releases.md` gains an entry covering the crash fix and the measured performance change. Constitution Principle VII is satisfied by that edit; **no exemption is claimed** |

### Standing rules during validation

- No automatic commits at any point. Commit only when explicitly asked.
- The US1 STOP is mandatory before any interpreter or compiler source edit.
- Every timing run is exclusive: nothing else compiles or renders on the
  machine.
- `stat` every `.slo` before trusting any `-slo` result. A green `-slo` run
  after an emitter change proves nothing unless the bitcode postdates both the
  emitter edit and the `oshader` rebuild.
- Never bare `git stash` / `git stash pop` — the stash stack is shared with the
  main checkout and every other worktree, so a pop can take someone else's work.
  Use a temporary WIP commit, or `git stash push -u -m "<unique-tag>"` +
  `git stash apply <sha>` + an explicit drop.
- All evidence is written under `specs/012-jit-parity-followups/`, never `/tmp`.
