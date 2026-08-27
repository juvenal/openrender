# Tasks: JIT/Interpreter Parity Follow-ups (post-011)

**Branch**: `012-jit-parity-followups` | **Date**: 2026-08-25 | **Input**: Design documents from `specs/012-jit-parity-followups/`

**Prerequisites**: [plan.md](./plan.md), [spec.md](./spec.md), [research.md](./research.md), [data-model.md](./data-model.md), [contracts/](./contracts/), [quickstart.md](./quickstart.md)

**Worktree**: `/Volumes/Projects/Development/CLI/openrender-worktrees/012-jit-parity-followups` —
run every command from here. Do **not** `cd` to the main checkout.

**Tests are REQUIRED**: Constitution Principle III (Test-Driven Development) is
NON-NEGOTIABLE and [plan.md](./plan.md) carries a per-story Red/Green mapping.
Every story's Red artifact is a task in this file and precedes its Green tasks.

---

## Format: `[ID] [P?] [Story?] Description`

- **[P]**: Can run in parallel (different files, no dependency on an incomplete task)
- **[Story]**: `[US1]`, `[US2]`, `[US3]` — which user story the task serves
- Every task names an exact file path or an exact command target

## Standing rules (apply to every task below)

- **No automatic commits.** Commit only when the maintainer explicitly asks.
- **`.slo` staleness**: `stat` every `.slo` against both the `oshader` binary
  and the emitter/runtime sources before trusting any `-slo` result. A green
  `-slo` run after an emitter change proves nothing unless the bitcode
  postdates the edit and the rebuild.
- **No pre-existing reference image is regenerated** (SC-007, admits no
  exceptions). New coverage arrives only as new scenes with new references.
- **`ri` depends on `libshader_shading`, never the reverse.**

### `<render command>` — the literal expansion used by every task below

Tasks that render a scene directly (rather than through `ctest`) write
`<render command>`. It expands to exactly this, run from the worktree root:

```bash
SHADERS="$(pwd)/openrender/shaders" \
ORENDERHOME="$(pwd)/openrender" \
DISPLAYS="$(pwd)/openrender/displays" \
GEOMETRIES="$(pwd)/openrender/geometry" \
build/src/orender/orender <rib>
```

The `openrender/` deploy tree it points at **does not exist in a fresh
worktree** — it is gitignored and only produced by `cmake --install`. T003b
provisions it; no direct-render task can run before that.

### Baseline and evidence locations

Captured run logs go under `specs/012-jit-parity-followups/baselines/`, **not
`/tmp`**: a `/tmp` file cannot be diffed after a reboot and cannot be reviewed
alongside the change it justifies. Narrative measurements go in
`specs/012-jit-parity-followups/measurements.md`. The filenames used below are:

| File under `baselines/` | Captured by | Consumed by |
|---|---|---|
| `base-libshader.txt` / `base-visual.txt` | T004 / T005 | the per-stream "before" tasks T015a, T032a, T044a |
| `perf-var-1..5.txt` | T007 | T008, T037 |
| `perf-before.txt` | T008 | T037 |
| `us1-before-libshader.txt` / `us1-before-visual.txt` | T015a | T022 |
| `us2-before-libshader.txt` / `us2-before-visual.txt` | T032a | T036 |
| `us3-before-libshader.txt` / `us3-before-visual.txt` | T044a | T046 |
| `perf-after.txt` | T037 | T037's SC-004/SC-005/SC-006 reporting step, and T052 (whose performance sentence needs the measured figures) |

**Expected test-set delta — state it, never absorb it.** This feature adds
**exactly two** `add_visual_test` registrations, and each lands *after* some of
the baselines above are already captured. A `-L visual` diff against a baseline
taken before a registration therefore shows an added test entry, which is
expected; **any other** test-set difference — a third addition, a removal, a
rename — is a finding, not bookkeeping. Every reported comparison must name its
expected delta explicitly:

| Baseline | Registrations it predates | Expected added entries in a later run |
|---|---|---|
| `base-visual.txt` (T005) | T008b, T020 | 2 — the FR-006 discrimination scene and the `usfroma` probe scene |
| `us1-before-visual.txt` (T015a) | T020 only | 1 — the `usfroma` probe scene |
| `us2-before-visual.txt` (T032a) | none | 0 — diffs clean |
| `us3-before-visual.txt` (T044a) | none | 0 — diffs clean |

US1's before-pair unavoidably predates its own test registration: the probe
scene's reference image (T020) can only be generated from a build where the
crash is fixed, so the test cannot exist while the defect does. That is why
US1's expected delta is 1 and not 0.

## Hard serialization points — never mark these `[P]`

Reproduced from [plan.md](./plan.md) § Execution Model so they cannot be lost
in task-level optimism:

1. **Phase 2 (Stream 0) in full**, before anything lands.
2. **Every `perf-manual` timing run** — exclusive, quiescent machine (T007, T008, T037).
3. **`.slo` regeneration after any emitter or shading-runtime change** (T033, T045),
   with a `stat` check before every `-slo` verification.
4. **The US1 STOP** (T015), before any `.rslo` interpreter *or* compiler source
   edit; and within US2, the §2.2 callee audit (T023–T024) precedes any collapse.
5. **The conditional US3 STOP** (T044), triggered only if the shared entry point
   cannot preserve both backends' observable behaviour.
6. **The FR-006 active/inactive discrimination check** (T035) — it gates T036 and
   may itself have to author new coverage. "No scene covers it" is not a
   passing outcome.
7. **T003e → T003e1 → T003f → T003g**, which all edit the single file
   `.github/workflows/deploy-site.yml`. Same-file tasks are never `[P]` here
   regardless of how independent their *concerns* are; T003h (`release.yml`)
   is the only `[P]` task in Phase 1b.

---

## Phase 1: Setup (shared infrastructure)

**Purpose**: Make the worktree buildable and its shader artifacts trustworthy.
The worktree has no `build/` directory yet — nothing can be compiled or measured
until this phase completes.

> **Worktree reality check (verified 2026-08-25).** This worktree contains
> **zero** `.slo` and **zero** `.rslo` files — `shaders/` holds only `.sl`
> sources — and the gitignored `openrender/` deploy tree **does not exist**.
> Phase 1 therefore *generates* the artifact set and *provisions* the deploy
> tree. It is not the staleness audit the quickstart's P1 describes for a
> populated tree; that audit becomes meaningful only from T033/T045 onward,
> once artifacts exist and an emitter edit can make them stale.

- [X] T001 Configure and build the unchanged binary in the worktree: `cmake -S . -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build --config Release`, per `COMPILING.txt`
- [X] T001a [P] Create the evidence scaffolding **before anything writes to it** — `specs/012-jit-parity-followups/baselines/` and the shared recording file `specs/012-jit-parity-followups/measurements.md`, with one section heading per phase. This task exists in Phase 1 rather than Phase 2 because five Phase 1 tasks (T002, T003, T003a, T003c, T003d) already record into `measurements.md`; creating it in Phase 2 would put its first write before its creation. **Every task that writes to `measurements.md`**, in order: T002, T003, T003a, T003c, T003d, T006, T006a, T007, T008, T008b, T012, T013, T018, T020a, T032, T034, T035, T037, T044, T047. (The before-pair captures T015a/T032a/T044a write to `baselines/`, not here; `measurements.md` records the *derived* comparisons. T053 fills [quickstart.md](./quickstart.md)'s Stage 4 table, not this file.) Independent of T001 — no build is required to create a directory and a Markdown file
- [X] T002 Inventory what exists before generating anything: `ls shaders/`, and record in `specs/012-jit-parity-followups/measurements.md` which `.sl` sources have no compiled counterpart. **Expected result in a fresh worktree: all of them** — there is nothing to audit for staleness yet, so the `stat` comparison in [quickstart.md](./quickstart.md) P1 is a no-op at this point and must not be mistaken for a clean bill of health
- [X] T003 [P] Generate the full artifact set from `shaders/*.sl` — for each source, `build/src/oshader/oshader -o shaders/<name>.rslo shaders/<name>.sl` and `build/src/oshader/oshader --jit -o shaders/<name>.slo shaders/<name>.sl`, using `SHADERS_INCLUDE="$(pwd)/shaders/includes"` for shaders that `#include` `.slh` headers (**not** `-I`, which mis-parses when combined with `-o` and a positional input). Record any source that fails to compile rather than skipping it silently
- [X] T003a [P] Reconcile the `perf-manual` harness with SC-005 **before** any timing run, in `tests/visual/CMakeLists.txt`: `add_perf_manual_test` (lines 218-261) defaults `MAX_RATIO` to `0.90` and `tests/visual/test_perf_compare.cpp:83` hard-fails above it. SC-005 states the 90% bar is *"a reported outcome, not a pass/fail gate"*, so as written the harness would fail every scene that has not yet met a stretch goal and would obscure the measurement this feature exists to take. Raise `MAX_RATIO` so the comparison reports rather than gates, and record the change and its justification in `specs/012-jit-parity-followups/measurements.md`. **This does not affect SC-003**: `perf-manual` is its own ctest label, outside both `-L libshader` and `-L visual`, so the regression baselines are untouched
- [X] T003b Provision the deploy tree the `<render command>` needs: run `cmake --install build --prefix "$(pwd)/openrender"` (see `INSTALL.md` for the non-sudo prefix workaround), then copy the T003 artifacts into `openrender/shaders/`. Confirm `openrender/{shaders,displays,geometry}` all exist — a direct `orender` invocation with any of them missing fails in a way that looks like a shader bug
- [X] T003c Smoke-test the provisioned tree by rendering one known-good existing scene with the `<render command>` and confirming normal exit, so that a later abnormal exit in T012 is attributable to the probe and not to a mis-provisioned environment
- [X] T003d Provide a working LLVM-IR dump path for `.slo` modules — the mechanism T006a, T034, and [contracts/op-uniform-collapse.md](./contracts/op-uniform-collapse.md) §5 all depend on, and which **does not exist today**. Verified 2026-08-25: `oshader` has no IR-dump flag, and `llvm-dis` is on neither `PATH` nor under `/opt/homebrew/opt/llvm/bin` (that prefix does not exist), so the usual "just run `llvm-dis`" fallback is unavailable on this machine. Resolve it in this order: (a) locate the LLVM installation the build actually links against — `grep -i LLVM_DIR build/CMakeCache.txt` — and check for `llvm-dis`/`llvm-bcanalyzer` in its `bin/`; if present, record the absolute path here and stop. (b) Otherwise add a debug flag to `src/oshader/` (e.g. `--emit-llvm` writing `Module::print()` textual IR alongside the bitcode), following the existing CLI conventions in `oshader --help`. Verify the mechanism end-to-end by dumping one existing shader and locating a literal `op_` call site in the output. **No collapse may be emitted and no emitted-form claim may be made until this task passes its own verification** — an unverifiable "expected: `i32 1` and `ptr null`" check is not evidence

### Phase 1b: CI compliance with Constitution Principle VII (independent of the build chain)

**Purpose**: Bring `.github/workflows/` into compliance with Principle VII's
"Site deployment MUST occur automatically on pushes to \[the default branch\]
and release tags", raised as `/speckit-analyze` finding **D1** and recorded as
Gap A / Gap B in [.specify/memory/constitution-v1.1.1-draft.md](../../.specify/memory/constitution-v1.1.1-draft.md) §5.

> **The bracketed substitution above is deliberate, and the amendment is
> *not* applied.** Principle VII's live text (`constitution.md:46`) literally
> reads "pushes to main branch"; the v1.1.1 draft that would correct it to the
> default branch (`master`) is held for review at the maintainer's direction and
> `constitution.md` remains at 1.1.0, unmodified. T003f therefore points the
> trigger at `master` while the constitution still says `main` — **this is
> intentional, not a violation.** No `main` branch has ever existed in this
> repository (`refs/remotes/origin/HEAD` → `refs/remotes/origin/master`), so a
> literal reading mandates deploying from a branch that cannot be pushed to.
> A future `/speckit-analyze` will likely flag this as a Principle VII conflict
> and score it CRITICAL, since constitution conflicts are auto-CRITICAL in that
> workflow. The disposition is recorded here so the finding can be resolved by
> ratifying the draft, not by repointing the trigger back at a nonexistent
> branch.

> **These tasks gate nothing.** They share no file, no binary, and no
> measurement state with the JIT work. T004 and everything after it may start
> before any of T003e-T003i is finished; they live in Phase 1 only because they
> are setup-class work, not because Phase 2 depends on them. **None of them
> writes to `measurements.md`**, so T001a's enumerated writer list is unaffected.

> **Consolidation decision (deviation from the literal instruction, stated so it
> is reviewable).** `docs-deploy.yml` already triggers on push to `master`
> filtered to `docs/site/**`. Simply repointing `deploy-site.yml` from `main` to
> `master` would make the two workflows *exact duplicate triggers* — both would
> build and both would deploy to Pages on every docs push. `concurrency:
> group: "pages"` serializes them but does **not** dedupe them, and
> `deploy-site.yml`'s build is inferior on four counts (`hugo-version: 'latest'`
> unpinned vs. pinned `0.152.2`, no Dart Sass, no `configure-pages`/baseURL, no
> `fetch-depth: 0` for `.Lastmod`), so the worse build could land last and
> clobber the good one. **Correction (2026-08-26):** an earlier revision of this
> callout also cited "no link validation" as a fifth count. That was wrong —
> `docs-deploy.yml`'s validator script has never existed and its step is a
> masked no-op, so neither workflow validates links today (see T003e1). The
> consolidation still holds on the four counts above.
> T003e-T003i therefore make `deploy-site.yml` the **single** implementation,
> carrying `docs-deploy.yml`'s build steps, and retire `docs-deploy.yml`. This
> satisfies every stated requirement — trigger on `master`, concurrency guard
> present, `release.yml` calls `deploy-site.yml` — without the duplicate deploy.

> **T003e → T003e1 → T003f → T003g are a serial sequence on one file.** All four
> edit `.github/workflows/deploy-site.yml` (51 lines), so none of them is `[P]` — the
> `[P]` contract in this file is *different files, no dependencies on incomplete
> tasks*, and three concurrent edits to one small YAML collide. They are kept as
> four tasks rather than one so each carries a single reviewable concern; run
> them in ID order (`T003e1` sorts between `T003e` and `T003f`). T003e1 also
> creates a new file, `docs/tools/link-validator.sh`, which nothing else touches. **T003h is the only `[P]` task here**: it is the sole task
> touching `release.yml`, and it may run concurrently with the whole
> `deploy-site.yml` sequence — but it cannot be *verified* until T003f has added
> the `workflow_call:` trigger it references.

- [X] T003e Port the superior build into `.github/workflows/deploy-site.yml`, replacing its current `build` job steps with `docs-deploy.yml`'s: pinned `env: HUGO_VERSION: 0.152.2` installed from the GitHub releases `.deb` (not `peaceiris/actions-hugo@v3` with `hugo-version: 'latest'`), `sudo snap install dart-sass`, `actions/configure-pages@v5` (id `pages`), `hugo --minify --baseURL "${{ steps.pages.outputs.base_url }}/"` with `HUGO_ENVIRONMENT`/`HUGO_ENV: production`, then `actions/upload-pages-artifact@v3` on `docs/site/public`. **Do not port `docs-deploy.yml`'s `./link-validator.sh || true` step as-is — see T003e1**, which owns link validation. Also carry over `defaults: run: { shell: bash, working-directory: docs/site }` and the checkout's `fetch-depth: 0` (needed for Hugo's `.Lastmod` from git history). Do **not** change the trigger block in this task — T003f owns it
- [X] T003e1 **Author the link validator that does not exist, then wire it in.** Verified 2026-08-26: `docs-deploy.yml:66` runs `./link-validator.sh || true`, but **no such script exists anywhere** — absent from the worktree, from `master`, and from every commit on every branch (`git log --all -- '*link-validator*'` returns nothing). The `|| true` masks the resulting "no such file" failure, so this step has *always* been a silent no-op, and "link validation" was never a real capability of either workflow. Write `docs/tools/link-validator.sh` — placed outside `src/` and outside the Hugo tree at `docs/site/`, so Hugo never processes it and it cannot leak into `public/`. Requirements: (a) take the built-site directory as `$1`, defaulting to `docs/site/public`, so it is runnable locally against a plain `hugo` build; (b) walk the generated HTML and verify every **internal** `href`/`src` resolves to a file that exists in that directory, resolving `/foo/` to `foo/index.html`; (c) skip external `http(s)://`, `mailto:`, and bare-fragment links — validating outbound URLs makes the deploy depend on third-party uptime; (d) print each broken link as `<source-file>: <target>` and exit non-zero if any were found, exit 0 otherwise; (e) `set -euo pipefail`, `#!/usr/bin/env bash`, `chmod +x`. Wire it into `deploy-site.yml` after the Hugo build as `run: ../tools/link-validator.sh public` (the job's `working-directory` is `docs/site` per T003e) — **without `|| true`**, since suppressing the exit code is precisely what made the original meaningless. **Run it locally against a fresh `hugo --minify` build before wiring it in**: if the existing documentation already contains broken internal links the step will now fail the deploy, so fix them, or report the count back and get a decision. Do not neutralize the gate to make it pass
- [X] T003f Fix `.github/workflows/deploy-site.yml`'s trigger block: change `push: branches: [main]` to `[master]` (**no `main` branch has ever existed** in this repository — `refs/remotes/origin/HEAD` → `refs/remotes/origin/master`), keep the `paths: ['docs/site/**']` filter, add `.github/workflows/deploy-site.yml` to that path list so the workflow re-runs when it is itself edited, keep `workflow_dispatch:`, and **add `workflow_call:`** — without a `workflow_call` trigger, T003h's `uses:` reference from `release.yml` fails at workflow-parse time, not at run time
- [X] T003g Add the two missing safety guards to `.github/workflows/deploy-site.yml`. (a) A workflow-level `concurrency: { group: "pages", cancel-in-progress: false }` block — currently absent, so a manual dispatch concurrent with a `master` push is an unguarded double-deploy to production Pages. (b) An `if:` gate on the `deploy` job, which today has **none**, meaning any `workflow_dispatch` from any branch publishes to production. **Do not copy `docs-deploy.yml`'s gate verbatim** — its `github.event_name == 'push' && github.ref == 'refs/heads/master'` is *false for a tag push*, so a verbatim port would silently no-op the release-tag deployment T003h exists to add. The gate must admit three paths: a `master` push, a release-tag push, and the `workflow_call` invocation. **Inside a reusable workflow `github.event_name`/`github.ref` reflect the *caller's* event, not the callee's** — verify the final expression against an actual run (or `act`), not by reasoning about it
- [X] T003h [P] Add a site-deployment job to `.github/workflows/release.yml` that `uses: ./.github/workflows/deploy-site.yml`, satisfying Principle VII's release-tag clause (Gap A — verified by grep: `release.yml` currently contains **zero** occurrences of hugo, `docs/site`, or pages). The job **must declare `permissions: { contents: read, pages: write, id-token: write }` on itself**: `release.yml`'s workflow-level grant is `contents: write` and nothing else, a called workflow cannot exceed its caller's grant, and the resulting failure surfaces at `actions/deploy-pages` as a permissions error rather than at parse time. Place it `needs: create-release` so it runs only after the tag is confirmed to be on master (existing step, `release.yml:24`). **Note the interaction with T003g(a)**: the `pages` concurrency group is now shared between the docs-push path and the release path, and `cancel-in-progress: false` means a release cut during an in-flight docs deploy *queues* rather than cancelling — this is the intended behaviour, but it can look like a hung release job if it is not expected
- [X] T003i Delete `.github/workflows/docs-deploy.yml` **only after T003e-T003h are complete and verified**, since it is the live deployer and its build steps are the source T003e ports from. Removing it earlier leaves the repository with no working Pages deployment. Confirm before deleting that `deploy-site.yml` now carries every capability `docs-deploy.yml` had: pinned Hugo, Dart Sass, `configure-pages`/baseURL, `fetch-depth: 0`, the `pages` concurrency group, and a deploy gate that admits a `master` push. **Link validation is deliberately not on that list** — `docs-deploy.yml`'s `link-validator.sh` step never ran (the script does not exist; see T003e1), so there is no such capability to preserve. What must be confirmed instead is that T003e1's *replacement* validator is present and wired without `|| true`

**Checkpoint**: `build/src/orender/orender` and `build/src/oshader/oshader` exist; every `shaders/*.sl` has both a `.rslo` and a `.slo` postdating the `oshader` binary; `openrender/` is provisioned and the `<render command>` renders an existing scene successfully; `perf-manual` reports rather than gates. Independently, `.github/workflows/` contains exactly one Pages deployer, triggered on `master` pushes to `docs/site/**` and callable from `release.yml`, with both the concurrency guard and the deploy gate in place.

---

## Phase 2: Foundational — Stream 0 baseline (BLOCKING)

**Purpose**: Capture every "before" number that SC-003, SC-004, SC-006, and
SC-007 are defined against. If any story's change lands first, its result
silently becomes the next story's "before" and those criteria can no longer be
evaluated.

**⚠️ CRITICAL**: No task in Phase 3, 4, or 5 may start until this phase completes.
**No task in this phase is `[P]`** — T007 and T008 additionally require an
exclusive, quiescent machine.

- [X] T004 Capture the pristine compiler/unit baseline into the scaffolding T001a created: `ctest --test-dir build -L libshader --output-on-failure 2>&1 | tee specs/012-jit-parity-followups/baselines/base-libshader.txt`, and record the exact pass/fail set (pre-existing failures belong in the baseline, not the blocker list)
- [X] T005 Capture the pristine visual baseline: `ctest --test-dir build -L visual --output-on-failure 2>&1 | tee specs/012-jit-parity-followups/baselines/base-visual.txt`, recording the exact pass/fail set
- [X] T006 Record the same-configuration image noise floor into `specs/012-jit-parity-followups/measurements.md` — SC-007's "within noise" is undefined without it. Method, verbatim from `specs/011-jit-opcode-parity/tasks.md:222` (T048) and `lessons-learned.md:392-396`: render the *same unedited binary* twice for each stochastic raytrace scene, then compare the two runs with the project's 8×8 block-averaged diff metric (the same metric `tests/visual/CMakeLists.txt` uses); record per scene the **max block-avg** and **mean** of that same-binary pair. Spec 011's reference figure for the raytrace probe was max 39.375 / mean 0.0193 — expect the same order of magnitude, and treat any later before/after diff at or below the pair as noise
- [X] T006a Capture the **pre-change** emitted-form evidence that T008's bucket assignment depends on and that T034 later compares against: using the IR-dump mechanism from T003d, dump the `op_*` call sites of each of the six measurement shaders against the **unchanged** binary, and record per shader the count of uniform-classified sites in a collapsible family into `specs/012-jit-parity-followups/measurements.md`. Taking this before any emitter edit is what makes SC-004's classification auditable rather than retrofitted. **Sequenced here, ahead of T007/T008, for two reasons**: T008 cannot assign SC-004 buckets without this evidence, and dumping IR is machine load that would perturb the quiescent timing session T007–T008 must share
- [X] T007 Establish the run-to-run **variance** baseline (does not exist today; spec 011 reported single-run ratios only — [research.md D7](./research.md)) on a quiescent machine: `for i in 1 2 3 4 5; do ctest --test-dir build -L perf-manual -V 2>&1 | tee specs/012-jit-parity-followups/baselines/perf-var-$i.txt; done`, then record per-scene min/max/spread of the JIT-to-interpreter ratio in `specs/012-jit-parity-followups/measurements.md`. **Use `-V`, not `--output-on-failure`**: `tests/visual/test_perf_compare.cpp` prints the ratio to stdout (lines 75 and 89), and `--output-on-failure` suppresses stdout for every test that *passes* — which, after T003a stops the harness gating on 0.90, is all of them. A variance baseline captured with `--output-on-failure` would be empty
- [X] T008 Capture pre-change per-scene ratios in the **same quiescent session** as T007: `ctest --test-dir build -L perf-manual -V 2>&1 | tee specs/012-jit-parity-followups/baselines/perf-before.txt`, and fill the six-row density table from [quickstart.md](./quickstart.md) § 0.5 in `specs/012-jit-parity-followups/measurements.md`. **The "before" figure for each scene is the median of T007's five runs**, not this single run — this run exists to confirm the session is still quiescent and to catch drift between T007 and the density classification. Assign each scene to exactly one SC-004 bucket using the operational definition in spec.md SC-004 (count of uniform-classified dispatch sites in a collapsible family, from the T006a evidence): `sphere-cfrom`/`show_st_hsv`, `sphere-ctransform`/`show_ctransform`, `sphere-matrixops`/`matrix_ops_probe`, `sphere-comparisonlogic`/`comparison_logic_probe`, `sphere-arrayops`/`array_ops_probe`, `sphere-gather`/`gather_named_probe`. **`sphere-gather` was previously labelled "mixed"** — that is not a bucket; count its sites and place it in either "meaningful" or "near-zero control" here, before any after-measurement is taken
- [X] T008b Author the FR-006 discrimination coverage **against the unchanged binary** — this is US2's Red artifact for FR-006 and must exist before T025, not after. First check whether any existing visual-suite scene already exercises a uniform-classified instruction inside a conditional whose predicate is false for the *leading* shading points; **"no existing scene covers it" is not an acceptable stopping point.** If none does, author `shaders/uniform_in_conditional_probe.sl`, the scene pair `examples/rib/tests/sphere-uniform-conditional-reyes.rib` / `-reyes-slo.rib`, **one** new reference image under `tests/visual/reference/` generated from the unchanged binary, and an `add_visual_test` entry in `tests/visual/CMakeLists.txt`. Record the scene chosen or authored in `specs/012-jit-parity-followups/measurements.md`. Modify no existing reference image (SC-007). The reference generated here is the control T035 verifies against — generating it *after* the collapse would make the test unfalsifiable

**Checkpoint**: Baselines exist for regression (T004, T005), image noise (T006), pre-change emitted form (T006a), timing variance (T007), timing level (T008), and FR-006 discrimination coverage (T008b). All three stories may now proceed in parallel.

---

## Phase 3: User Story 1 — Varying-index `uniform string` array read renders instead of crashing (Priority: P1) 🎯 MVP

**Goal**: A shader reading a `uniform string` array element at a varying,
in-range index renders to completion under the interpreter backend with the
correct per-point element selection, instead of terminating the whole render
(FR-001, FR-002, FR-003).

**Independent Test**: Render the new probe scene with the interpreter backend.
Before the fix it terminates abnormally; after the fix it completes, the
selected-element behaviour matches a hand-computed expectation and the
uniform-indexed equivalent, and the new scene passes as an executing regression
test while every pre-existing reference image stays untouched.

### Red — reproduction (FR-002); this IS the authorization for the fix

- [X] T009 [P] [US1] Write the minimal probe shader `shaders/usfroma_probe.sl` performing a varying-index read of a fixed-length `uniform string` array consumed **inline** in an expression — the shape recorded at `specs/011-jit-opcode-parity/triage-results.md:85`, `if (usarr[findex] == "a")`. The read must be inline because `rsloStringSpecifier` (`src/libshader/compiler/rslo.y:342-347`) forces `SLC_UNIFORM` onto a bare `string`, so no RSL string *variable* can hold a varying value
- [X] T010 [P] [US1] Author the scene pair `examples/rib/tests/sphere-usfroma-reyes.rib` (pins `Option "shaderformat" "rslo"`) and `examples/rib/tests/sphere-usfroma-reyes-slo.rib` (pins `"slo"`), modelled on the existing `sphere-arrayops-*` scenes
- [X] T011 [US1] Compile the probe to bytecode: `build/src/oshader/oshader -o shaders/usfroma_probe.rslo shaders/usfroma_probe.sl`
- [X] T012 [US1] Reproduce the crash 5× using the `<render command>` expansion defined in Standing rules above (requires T003b's provisioned `openrender/` tree): `for i in 1 2 3 4 5; do SHADERS="$(pwd)/openrender/shaders" ORENDERHOME="$(pwd)/openrender" DISPLAYS="$(pwd)/openrender/displays" GEOMETRIES="$(pwd)/openrender/geometry" build/src/orender/orender examples/rib/tests/sphere-usfroma-reyes.rib; echo "run $i exit=$?"; done`, recording the exit status of each run in `specs/012-jit-parity-followups/measurements.md`
- [X] T013 [US1] Record the observed reproduction rate against SC-001 in `specs/012-jit-parity-followups/measurements.md` — 5/5 abnormal termination is the expected result; if intermittent, record the rate (SC-001 admits any non-zero pre-fix failure rate paired with a 100% post-fix pass rate)

### Root cause and approval gate

- [X] T014 [US1] Diagnose the root cause read-only across `src/libshader/shading/execute.cpp` (interpreter handling of the `usfroma` instruction) and `src/libshader/compiler/rslo.y` + `src/libshader/compiler/expression.cpp` (which instruction form the compiler chooses), and determine which side is at fault. **Change no source in this task**
- [X] T015 [US1] **🛑 MANDATORY STOP — maintainer approval required.** Present: the T012/T013 reproduction evidence recorded in `specs/012-jit-parity-followups/measurements.md`, the confirmed root cause, which side it lies on (interpreter — `src/libshader/shading/execute.cpp` — or compiler — `src/libshader/compiler/rslo.y` / `expression.cpp`), and the narrowest proposed change naming the exact file and line range. If the fix is compiler-side, the presentation MUST additionally state that it alters the meaning of shader artifacts already compiled by the previous compiler, for **both** backends at once (FR-002, FR-011). Wait for explicit confirmation. **Not `[P]` under any circumstance**
- [X] T015a [US1] **Capture US1's own "before" pair — after the STOP, before T016 makes the fix live.** SC-003 requires three *independent* before/after pairs, one per change, and Phase 2's pristine files are only valid as the "before" for whichever stream lands first. The tree here still contains the defect and none of US1's fix, so this is a genuine pre-change capture requiring no revert: `ctest --test-dir build -L libshader --output-on-failure 2>&1 | tee specs/012-jit-parity-followups/baselines/us1-before-libshader.txt` and `ctest --test-dir build -L visual --output-on-failure 2>&1 | tee specs/012-jit-parity-followups/baselines/us1-before-visual.txt`. **This task must not be moved after T016 or T017.** A capture taken once the fix is live photographs the post-change state, so T022 would diff the fixed tree against itself and could never fail — the same defect FR-006's discrimination reference exists to avoid. If US1 is the first stream to land, these files will be identical to `base-*.txt` apart from T008b's added entry, and that identity is itself worth recording

### Green — fix and permanent coverage

- [X] T016 [US1] Apply the approved narrowest change in the approved file (`src/libshader/shading/execute.cpp` if interpreter-side, `src/libshader/compiler/rslo.y` or `src/libshader/compiler/expression.cpp` if compiler-side). No incidental refactoring (FR-011). **FR-010 applies here too**: if the fix routes the varying-index read through an existing array-access path, it must *call* that path, not restate its indexing arithmetic at the crash site — a second copy of the stride math is exactly the drift FR-010 exists to prevent
- [X] T017 [US1] Rebuild (`cmake --build build --config Release`) and regenerate **both** probe artifacts: `build/src/oshader/oshader -o shaders/usfroma_probe.rslo shaders/usfroma_probe.sl` and `build/src/oshader/oshader --jit -o shaders/usfroma_probe.slo shaders/usfroma_probe.sl`; if the fix was compiler-side, regenerate **every** `shaders/*.slo` and `shaders/*.rslo` plus the deploy-tree copies, because the compiler change altered their meaning
- [X] T018 [US1] Re-run the reproduction 5× against `examples/rib/tests/sphere-usfroma-reyes.rib` with the same `<render command>` expansion as T012 and confirm 5/5 normal completion (SC-001), recording results in `specs/012-jit-parity-followups/measurements.md`
- [X] T019 [US1] Verify the per-point element selection in `shaders/usfroma_probe.sl` matches a hand-computed expectation and the uniform-indexed equivalent, and that `examples/rib/tests/sphere-usfroma-reyes-slo.rib`'s output matches `examples/rib/tests/sphere-usfroma-reyes.rib`'s within the visual-regression tolerance (spec Acceptance Scenarios 1 and 2)
- [X] T020 [US1] Generate **one new** reference image for the probe scene under `tests/visual/reference/` and register the scene pair via `add_visual_test` in `tests/visual/CMakeLists.txt`, following the existing `sphere-arrayops-*` entries. Modify no existing reference image (FR-003, SC-007)
- [X] T020a [US1] Produce SC-002's **negative** evidence — the half that distinguishes "a test exists" from "a test that would fail if the defect returned". Stash the T016 fix (a temporary local revert of the approved change only, never `git stash` — the stash stack is shared across worktrees), rebuild, regenerate the probe artifacts, and confirm `ctest --test-dir build -R usfroma --output-on-failure` **fails**. Restore the fix, rebuild, and confirm it passes again. Record both outcomes in `specs/012-jit-parity-followups/measurements.md`. Without this, SC-002's "would fail if the defect returned" clause is asserted, not demonstrated. **This revert is not a baseline capture and must not be used as one** — US1's before-pair is T015a's, taken before T016, and the revert here is a transient state that ends with the fix restored
- [X] T021 [US1] Add the FR-003 deliberate-omission note to the spec 011 diagnostic shader that had the construct removed (`shaders/array_ops_probe.sl`), recording that the omission is intentional and that coverage now lives in `shaders/usfroma_probe.sl` / `examples/rib/tests/sphere-usfroma-reyes.rib`, so the removal is not later mistaken for an oversight
- [X] T022 [US1] Run the full regression and diff against **US1's own before-pair** (`baselines/us1-before-libshader.txt`, `baselines/us1-before-visual.txt` from T015a — not the shared `base-*.txt`, which may already reflect another stream's landed change): `ctest --test-dir build -R usfroma --output-on-failure`, `ctest --test-dir build -L libshader --output-on-failure`, `ctest --test-dir build -L visual --output-on-failure`; confirm the new test passes (SC-002) and zero tests newly fail (SC-003), and confirm `git status` shows no modified file under `tests/visual/reference/` — only added ones (SC-007). Apply the expected-delta rule from Standing rules: `-L visual` gains exactly **one** entry (T020's probe scene) relative to `us1-before-visual.txt`, and `-L libshader` gains none

**Checkpoint**: US1 is complete and independently deliverable — SC-001, SC-002, and SC-003's first before/after pair are satisfied. **This is the MVP.**

---

## Phase 4: User Story 2 — Uniform-dominated shaders are not slower under the JIT (Priority: P2)

**Goal**: For an instruction the compiler classified uniform, the JIT performs
the computation once rather than once per shading point, matching the
interpreter, at every family where the interpreter short-circuits — with zero
output change (FR-004 … FR-008).

**Independent Test**: Measure JIT and interpreter wall-clock per measurement
scene before and after, under identical settings, and compare ratios against the
T007 variance; independently confirm the rendered images did not move.

**Red**: already captured as T007 + T008 — the JIT-to-interpreter ratio
exceeding 1.0 on uniform-dense scenes *is* the failing evidence.

### Prerequisite — discharge the callee audit (contract §2.2, NOT yet discharged)

**⚠️ Not `[P]`, and blocks every collapse task below.** No instruction may be
collapsed at a family until its callee has been checked.

- [X] T023 [US2] Scan `src/libshader/shading/rslOps.cpp` for every use of `n` outside a loop bound: `grep -nE "\bn\b" src/libshader/shading/rslOps.cpp | grep -vE "i < n|i<n|int n|\* n|n \*|n\)|numVerts"`, and resolve each candidate named in [contracts/op-uniform-collapse.md](./contracts/op-uniform-collapse.md) §2.2 — `rslOps.cpp:803` (`*numPassive = n;`), `rslOps.cpp:1093`/`1102`/`1112` (`op_area`/`op_calculatenormal`/`op_depth`, derivative-dependent and likely **excluded**), `rslOps.cpp:836`/`842`/`888`/`894`/`903`/`914`/`920` (`DEFLIGHTFUNC`, already excluded by §4), `rslOps.cpp:555`/`584`/`1094`/`1103` (`if (n > 0 && ACTIVE(tags, 0))`)
- [X] T024 [US2] Record the discharged audit in [contracts/op-uniform-collapse.md](./contracts/op-uniform-collapse.md) §2.2 — each candidate either **cleared** (honours the `n == 1, tags == nullptr` guarantee) or **excluded with its reason** — and flip the section's "Audit status: NOT YET DISCHARGED" line. An op that cannot honour the guarantee is not a blocker; it is excluded and recorded, the same way `DEFLIGHTFUNC` is (FR-004's explicit-exclusion requirement)

### Implementation

- [X] T025 [US2] Add the uniform-classification predicate to `src/libshader/compiler/llvmEmitter.cpp` — an instruction is uniform iff `dstDesc.stride == 0` (line 598) **and** every operand stride returned by `getVar()` (line 442) is `0`, the equivalence to the interpreter's `code->uniform` established in [research.md D1](./research.md)
- [X] T026 [US2] Add the collapsed-call helper in `src/libshader/compiler/llvmEmitter.cpp` that, when the predicate holds, emits `n = 1` **and `tags = ptr null`** — never `n = 1` with a live tag pointer, which is the forbidden combination of [contracts/op-uniform-collapse.md](./contracts/op-uniform-collapse.md) §2.3 (it renders correctly wherever vertex 0 is active and diverges only inside conditionals)
- [X] T027 [P] [US2] Apply the collapse in the `DEFOPCODE` arithmetic dispatch — `emitBin`/`emitUn`/`emitTern` in `src/libshader/compiler/llvmEmitter.cpp:459-509` — skipping any callee excluded by T024
- [X] T028 [P] [US2] Apply the collapse at the `DEFFUNC` builtin call sites in `src/libshader/compiler/llvmEmitter.cpp`, skipping any callee excluded by T024
- [X] T029 [P] [US2] Apply the collapse at the `DEFSHORTFUNC` call sites in `src/libshader/compiler/llvmEmitter.cpp` (`environment` ×2, `shadow` ×2, `bake3d` — [contracts/op-uniform-collapse.md](./contracts/op-uniform-collapse.md) §4), emitting the collapse **only** at sites whose callee T024 cleared. On a collapsed call the count is `1`, so there is no per-site count to derive; FR-007's bite here is "do not collapse where the grid width is semantically required", which is exactly T023/T024's disposition. Leave the *varying* path's existing count untouched — the pre-existing `numVerts`-vs-`numRealVertices` asymmetry is explicitly out of scope ([contracts/op-uniform-collapse.md](./contracts/op-uniform-collapse.md) §3, [research.md D4](./research.md)) and must not be "fixed" under cover of this task
- [X] T030 [P] [US2] Apply the collapse at the `DEFSHORTOPCODE` sites in `src/libshader/compiler/llvmEmitter.cpp`, or record that the family has zero real uses in the tree ([contracts/op-uniform-collapse.md](./contracts/op-uniform-collapse.md) §4)
- [X] T031 [US2] Confirm `DEFLIGHTFUNC` is left uncollapsed in `src/libshader/compiler/llvmEmitter.cpp` — the interpreter treats a uniform classification there as an error (`scripterror("Invalid uniform lighting call")`), so there is no run-once semantics to mirror
- [X] T032 [US2] Write the complete FR-004 exclusion list — every dispatch site not converted, each with its reason — into `specs/012-jit-parity-followups/measurements.md`. Silent omission is explicitly not an acceptable outcome

### Green — output must not move

- [X] T032a [US2] **Capture US2's own "before" pair, before T033 rebuilds anything** — SC-003 requires an independent before/after pair per change, and by now US1 and/or US3 may have landed, making the Phase 2 `base-*.txt` files stale as US2's reference point: `ctest --test-dir build -L libshader --output-on-failure 2>&1 | tee specs/012-jit-parity-followups/baselines/us2-before-libshader.txt` and `... -L visual ... | tee .../us2-before-visual.txt`. The emitter edits from T025-T032 are not yet in any built artifact at this point — T033 is what makes them live — so this run legitimately measures the pre-collapse tree
- [X] T033 [US2] **Rebuild and regenerate — not `[P]`.** `cmake --build build --target oshader && cmake --build build --config Release`, then regenerate **every** `shaders/*.slo` and its `openrender/shaders/` deploy-tree copy, then run the `stat` audit described in [quickstart.md](./quickstart.md) P1 to confirm all bitcode postdates both the emitter edit and the `oshader` rebuild. **This is the first point in the feature where that audit is meaningful** — T002 ran against an empty artifact set. **Coordination with T045**: T033 and T045 are the same physical operation. Whichever of US2/US3 reaches its rebuild step second **skips its own regeneration and instead re-runs both stories' verification tasks against the single combined regeneration** — do not regenerate twice, and do not treat a `.slo` produced before the other story's source edit as current. If US2 lands second, T045 has already regenerated: run the `stat` audit only, confirm the bitcode postdates *both* stories' edits, and proceed to T034
- [X] T034 [US2] Confirm the emitted form using the T003d IR-dump mechanism (**not** an unspecified "dump the call sites" step — no such capability existed before T003d): regenerate the module for a uniform-dense shader with `build/src/oshader/oshader --jit -o /tmp/probe.slo shaders/array_ops_probe.sl`, dump its textual IR, and grep the `op_*` call sites. Confirm two things. **(a) FR-006 / contract §2.1**: every uniform-classified instruction calls with `i32 1` **and** `ptr null` — a call with `i32 1` and a live tag pointer is the forbidden combination of [contracts/op-uniform-collapse.md](./contracts/op-uniform-collapse.md) §2.3, and it passes casual testing because it is only wrong inside conditionals. **(b) FR-010 / contract §3**: for each collapsed site, the callee named in the IR is the **same `op_*` symbol** the pre-change dump from T006a named at that site — only the `n` and `tags` arguments differ. A changed callee means the collapse re-routed the computation instead of narrowing its width, which is an FR-010 violation regardless of whether the image moves. Diff against T006a's dump and record both confirmations in `specs/012-jit-parity-followups/measurements.md`
- [X] T035 [US2] Verify FR-006 per-point active/inactive semantics against the coverage T008b authored **before** the change: run the `sphere-uniform-conditional-*` pair (or the pre-identified existing scene) and confirm it still matches the reference image T008b generated from the unchanged binary. This is the single case that discriminates `tags = nullptr` from a live tag pointer, and the failure mode [contracts/op-uniform-collapse.md](./contracts/op-uniform-collapse.md) §2.3 flags as "most likely to survive casual testing". Record the result in `specs/012-jit-parity-followups/measurements.md`. **Not `[P]`** — it gates T036. **The authoring half of this check has deliberately moved to Phase 2 (T008b)**: a discrimination reference generated after the collapse would be a photograph of whatever the collapse produced, and could not fail
- [X] T036 [US2] Run `ctest --test-dir build -L visual --output-on-failure` and confirm zero differences above the T006 noise floor versus **`baselines/us2-before-visual.txt`** (T032a's pair, not the shared `base-*.txt`) — FR-005, SC-007. **If a difference appears** it is most likely the known corner — for a uniform instruction inside an all-inactive block, today's JIT writes nothing while the interpreter writes once, so the change moves the JIT *onto* the reference. **Stop and present it for disposition**; never regenerate a reference image unilaterally. Also run `ctest --test-dir build -L libshader --output-on-failure` against `baselines/us2-before-libshader.txt` (SC-003's second before/after pair)

### Green — performance (exclusive machine)

- [X] T037 [US2] **Exclusive, quiescent machine — not `[P]`; nothing else compiles or renders.** Run `ctest --test-dir build -L perf-manual -V 2>&1 | tee specs/012-jit-parity-followups/baselines/perf-after.txt` (**`-V`, not `--output-on-failure`** — see T007: the ratio is printed to stdout and suppressed for passing tests), then record in `specs/012-jit-parity-followups/measurements.md`, comparing against the **median** of T007's runs: **SC-004** — ratio improves by more than that scene's T007 variance on 100% of scenes T008 placed in the "meaningful uniform computation" bucket, and zero scenes regress (`sphere-cfrom` showing no measurable change is a **conforming** outcome, not a failure); **SC-006** — the `sphere-arrayops` vs `sphere-cfrom` gap at identical scale narrows by more than the variance of that comparison (pass/fail, no magnitude floor; report the magnitude either way); **SC-005** — per scene, whether the JIT reached ≤90% of the interpreter, reported and not gated (T003a removed the harness's gate), and where unmet, the residual dominant cost

**Checkpoint**: US2 is complete and independently deliverable — FR-004 … FR-008, SC-004, SC-005, SC-006, and SC-003's second before/after pair are satisfied.

---

## Phase 5: User Story 3 — Light iteration has exactly one implementation (Priority: P3)

**Goal**: The interpreter's `runLightsTemplate` macro and the JIT's
`CShadingContext::runLights`/`runCategoryLights` methods — hand-synced copies
that have already drifted in two places ([research.md D6](./research.md)) —
converge onto one implementation reproducing the **macro's** semantics exactly,
so the interpreter stays bit-unchanged (FR-009, FR-010, SC-008).

**Independent Test**: Render `illuminance`-using shaders under both backends
before and after and confirm output does not move, while confirming by
inspection that only one implementation remains.

**Red**: the before/after render set, captured by **T044a** immediately before
T045 makes US3's edits live. **Do not reuse T004/T005's `base-*.txt`** — by the
time US3 rebuilds, US1 and/or US2 may already have landed, so the Phase 2
baseline describes a different tree than the one US3 is changing, and diffing
against it would attribute their deltas to this refactor. The Stage 0
`base-*.txt` is the feature-level control, not any story's "before"
([contracts/light-iteration.md](./contracts/light-iteration.md) §5,
[quickstart.md](./quickstart.md) § 3.1).

**Process note**: US3 proceeds under FR-009's refactor exemption — **no STOP**,
because both backends discard the category on every form the JIT actually lowers
([contracts/light-iteration.md](./contracts/light-iteration.md) §3), so
convergence changes no observable behaviour. Full before/after verification is
still required.

- [X] T038 [US3] Design the converged entry point in `src/libshader/shading/shading.cpp`, retaining a **category parameter** — the interpreter's 4-operand `illuminance` path (`IlluminationCat1`, `src/libshader/shading/shaderOpcodes.h:92`) genuinely uses it via `ILLUMINATION_RUNCATLIGHTS`; the no-category call passes the no-category value `0`, exactly as `NORMALLIGHT_PRE` already does ([contracts/light-iteration.md](./contracts/light-iteration.md) §2.3). **Give the converged function a name distinct from `runLights` / `runCategoryLights`** (e.g. `iterateLights`). Those two names survive T040 as the interpreter's macro *wrappers* in `src/libshader/shading/execute.cpp:422-517` — only the `CShadingContext` *methods* are retired — so reusing either name would make T047's SC-008 grep return live code and destroy the evidence that exactly one implementation remains. Done: `iterateLights` implemented as two overloads in `shading.h:412-417`/`shading.cpp:1508-1512` — a no-category convenience overload forwarding `saveCat=0` into the categorized one, matching the `NORMALLIGHT_PRE` semantics; confirmed via T047's grep that no `runLights`/`runCategoryLights` method declaration survives in `shading.h` (only a contextual comment referencing the retained macro-wrapper names).

  **Chosen name and signature (recorded here so T047 can grep for the retired symbols precisely):**
  `void CShadingContext::iterateLights(const float *lP, const float *lN, const float *lT, int numVertices, int *tags, int &numActive, int &numPassive, int saveCat, int inShadow, float **varying, CShaderInstance *cInstance)` — identical parameter shape to the retired `runCategoryLights` (`shading.cpp:1506`), since every call site already constructs these exact arguments today. A thin `iterateLights(lP, lN, lT, numVertices, tags, numActive, numPassive, inShadow, varying, cInstance)` no-category overload (or default `saveCat = 0`) replaces the retired no-category `runLights` method, matching all five call sites below.

  **All five live call sites must be repointed (T041), not just one** — corrected scope, see `contracts/light-iteration.md` "Correction (2026-08-26)": `callDiffuse` (`shading.cpp:1624`), `callSpecular` (`shading.cpp:1664`), `prepareDiffuse` (`shading.cpp:1747`), `setupIlluminance` (`shading.cpp:1759`), `jitIlluminanceBegin` (`shading.cpp:2085`) — all currently pass `saveCat = 0` via the plain `runLights` wrapper, so repointing every one is a mechanical, signature-preserving rename with no semantic change to any builtin.
- [X] T039 [US3] Implement the single light-iteration function in `src/libshader/shading/shading.cpp` reproducing the **macro form's** semantics: the macro's cache-validity predicate `!*aTag & !*lTag` (not the method's `tags[i] != 0 || lightingTags[i] == 0`) and the macro's category rule that a light with `categories == NULL` **is** included under `invertCatMatch`. The method's better cache predicate is deliberately **not** adopted here — it may be proposed afterwards as an interpreter change with its own STOP, once a single implementation exists to change ([contracts/light-iteration.md](./contracts/light-iteration.md) §2.2). Done: implemented at `shading.cpp:1512` (the categorized overload); T043's FR-010 diff and T046's bit-unchanged ctest re-run confirm the macro-form semantics (cache predicate + `categories == NULL` inclusion rule) were reproduced exactly, not the method's differing predicate.
- [X] T040 [US3] Route the interpreter's opcode bodies through the converged function by rewriting the `runLights` / `runCategoryLights` macro wrappers in `src/libshader/shading/execute.cpp:422-517` to call it, leaving `enterLight`/`exitLight` sequencing, the `currentLight` walk, `numActive`/`numPassive` bookkeeping, and `SHADERFLAGS_NONAMBIENT` handling unchanged ([contracts/light-iteration.md](./contracts/light-iteration.md) §2.4). Done: `runLightsTemplate` now resolves `saveCat` via `CATEGORYLIGHT_PRE`/`NORMALLIGHT_PRE` and delegates directly to `CShadingContext::iterateLights(...)`; the now-dead `CATEGORYLIGHT_CHECK` macro (its per-light category logic moved into `iterateLights` itself) was deleted outright, definition and `#undef` both, rather than left unused
- [X] T041 [US3] Route the JIT through the same function by rewriting **all five** live call sites of the retired `runLights`/`runCategoryLights` methods to call `iterateLights` instead: `callDiffuse` (`src/libshader/shading/shading.cpp:1624`), `callSpecular` (`shading.cpp:1664`), `prepareDiffuse` (`shading.cpp:1747`), `setupIlluminance` (`shading.cpp:1759`), and `jitIlluminanceBegin` (`shading.cpp:2059-2122`) — keeping `jitIlluminanceBegin`'s `costheta` construction and uniform-P/N stride-3 broadcast where they are, since those are JIT-side argument preparation, not light iteration. All five already pass `saveCat = 0` through the identical no-category path today, so this is a mechanical repoint, not a behavior change (see `contracts/light-iteration.md` "Correction (2026-08-26)" — the original wording here scoped this task to `jitIlluminanceBegin` alone, which is incompatible with T042's "leaving no copy"). Done: all five renamed `runLights(...)` → `iterateLights(...)`, no signature or argument changes
- [X] T042 [US3] Delete the retired duplicate implementation from `src/libshader/shading/shading.cpp` (the `runLights`/`runCategoryLights` method pair at 1502-1558), leaving no copy, no re-deriving wrapper, and no "kept in sync" comment (SC-008). Already satisfied by construction: T039 replaced the method pair's body in place with `iterateLights` rather than adding a duplicate alongside it, so there was never a second copy to delete here — confirmed via `grep -rn "CShadingContext::runLights\|CShadingContext::runCategoryLights\|::runCategoryLights" src/` returning empty (T047's exact evidence command, run early to confirm this task's completion)
- [X] T043 [US3] Verify no shading math was re-derived (FR-010) by diffing the converged entry point in `src/libshader/shading/shading.cpp` against the retired macro body in `src/libshader/shading/execute.cpp` (git history for lines 422-517): the converged function must be the same computation both backends already performed, reached from one place. Done: `git diff` on both files confirms it — `execute.cpp`'s macro body was deleted outright and replaced with a pure delegation call to `iterateLights`; `shading.cpp`'s `iterateLights` is byte-identical to the pre-existing `runLights`/`runCategoryLights` method body except for exactly the two documented divergence fixes (cache-validity predicate, NULL-category-under-`invertCatMatch`). No other line changed; `shading.h` shows only the two signature renames
- [X] T044 [US3] **🛑 CONDITIONAL STOP — only if triggered.** If T039–T042 show the single entry point cannot preserve both backends' observable behaviour simultaneously, stop: the spec's edge case routes US3 out of the refactor exemption and into the FR-011 process (empirical reproduction, maintainer approval, narrowest change). The affected sources are `src/libshader/shading/shading.cpp` and `src/libshader/shading/execute.cpp`. Absent the trigger, this task is a no-op — record that it did not fire in `specs/012-jit-parity-followups/measurements.md`. **Not `[P]`**. Done: did not fire — T043's diff confirms the single entry point preserves both backends' behavior (interpreter reference semantics adopted per FR-011, JIT output-neutral per contract §3); recorded as a no-op below in this update pending `measurements.md` entry
- [X] T044a [US3] **Capture US3's own "before" pair, before T045 rebuilds anything** — SC-003's third independent pair. By this point US1 and/or US2 may have landed, so the Phase 2 `base-*.txt` files no longer describe the tree US3 is changing: `ctest --test-dir build -L libshader --output-on-failure 2>&1 | tee specs/012-jit-parity-followups/baselines/us3-before-libshader.txt` and `... -L visual ... | tee .../us3-before-visual.txt`. T038-T043's source edits are not yet in any built artifact — T045 is what makes them live — so this run legitimately measures the pre-convergence tree. Done: both baseline files captured, both suites passed at capture time (recorded in measurements.md).
- [X] T045 [US3] **Rebuild and regenerate — not `[P]`, and the same physical operation as T033.** `cmake --build build --config Release`, then regenerate every `shaders/*.slo` and its `openrender/shaders/` deploy-tree copy (this change touches the shading runtime, so every `.slo`'s ABI assumptions are in question), then run the [quickstart.md](./quickstart.md) P1 `stat` audit. **Coordination rule, stated once and identically in T033**: T033 and T045 must never both run. Whichever story reaches its rebuild step **first** performs the regeneration; the one that reaches it **second** skips regeneration entirely and instead (a) runs the `stat` audit to confirm every `.slo` postdates *both* stories' source edits — if it does not, the first story regenerated too early and the regeneration must be redone once, now — and (b) re-runs **both** stories' verification tasks (T034/T035/T036 *and* T046/T047) against that single combined artifact set. Neither story's `-slo` evidence is valid against bitcode that predates the other story's edit. Done: T033 (US2) reached the rebuild step first and already regenerated all `.slo`/`.rslo` artifacts; T045 ran only the stat audit — three `find ... ! -newer` checks against `shading.cpp`/`shading.h`/`execute.cpp` across `shaders/*.slo`, `openrender/shaders/*.slo`, and `shaders/*.rslo`, all empty (pass) — confirming no re-regeneration was needed. Full results in measurements.md.
- [X] T046 [US3] Verify the interpreter is **bit-unchanged** — zero differences, not "within noise" — across the `-rslo` visual scenes versus **`baselines/us3-before-visual.txt`** (T044a's pair, not the shared `base-*.txt`), and the JIT within the T006 noise floor across the `-slo` scenes: `ctest --test-dir build -L visual --output-on-failure` plus `ctest --test-dir build -L libshader --output-on-failure` against `baselines/us3-before-libshader.txt` (SC-003's third before/after pair, FR-009, FR-011). Done: `ctest -L visual` 91/91 passed (100%, includes the previously-flaky `Visual_subdiv-loop-photon`, which is a confirmed pre-existing intermittent unrelated to this change — see BUGS.md); `ctest -L libshader` 2/2 passed (100%). Combined re-run also satisfies US2's T036 re-check per T045's coordination rule. Full output in measurements.md.
- [X] T047 [US3] Produce the SC-008 evidence with a grep precise enough to mean something: search `src/` for the **retired method symbols specifically** — `grep -rn "CShadingContext::runLights\|CShadingContext::runCategoryLights\|::runCategoryLights" src/` — and confirm no remaining definition and no remaining call site, then confirm the method declarations are gone from `src/libshader/shading/shading.h`. **A bare `grep -rn runLights src/` is not valid evidence**: T040 deliberately keeps `runLights`/`runCategoryLights` as the interpreter's macro wrappers in `execute.cpp:422-517`, so a bare grep returns live code by design and would read as a failed removal. Also confirm the converged entry point named in T038 has exactly one definition. Then confirm an `illuminance`-using shader renders correctly under both `shaderformat` settings. Record the exact grep commands and their output in `specs/012-jit-parity-followups/measurements.md`. Done: all three greps run — retired-symbol grep empty; `shading.h` grep shows only a contextual comment, no declaration; `iterateLights` shows one substantive definition (`shading.cpp:1512`) plus one intentional forwarding overload (`shading.cpp:1508`, `saveCat=0`), not a duplicate. Dual-shaderformat check: `matte.sl` (via `diffuse()` → `iterateLights`) rendered against a `bunny-reyes.rib`-derived scene under both `rslo` and `slo`, both exit 0, both non-degenerate output files. Full evidence in measurements.md.

**Checkpoint**: US3 is complete and independently deliverable — FR-009, FR-010, SC-008, and SC-003's third before/after pair are satisfied.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: FR-012 documentation reconciliation and feature-level acceptance.
T048–T052 are `[P]` against each other (five distinct files) and may be drafted
concurrently with Phases 3–5, but are finalized only once their stories land —
T052 in particular needs T037's measured figures before its performance sentence
can be written.

- [X] T048 [P] Update `DEVNOTES_DETAILS/BUGS.md` — move the `usfroma` interpreter-crash entry to Resolved with its root cause, the side it landed on, and the new probe scene that now covers it (FR-012). Done: moved both the `usfroma` crash entry and the `illuminance`/`runLights` duplication entry to Resolved Bugs (the latter genuinely closed by US3's `iterateLights` convergence, confirmed via T047's zero-remaining-references grep); appended an accurate addendum to the `numVerts`-tax Open Issue noting US2 implemented and verified the fix but it did not close the measured wall-clock gap (SC-004/005/006 still unmet per T037) — left open, not moved.
- [X] T049 [P] Update `DEVNOTES_DETAILS/OSHADER_UPDATES.md` — record the uniform-dispatch collapse (predicate, the `n=1`/`tags=nullptr` convention, the exclusion list from T032) and the light-iteration convergence (FR-012). Done: extended the "Known performance gap" paragraph with an accurate US2 outcome (fix implemented, verified correct at IR + rendered-output levels, did not close the measured wall-clock gap) and added a new "Light-iteration convergence" subsection documenting `iterateLights`, the two resolved semantic divergences, and the two spec-013 candidates left open.
- [X] T050 [P] Update `specs/011-jit-opcode-parity/lessons-learned.md` and its records to reflect SC-006's outcome under this feature — spec 011's unmet performance criterion and what this feature measured against it (FR-012). Done: appended a 2026-08-27 addendum to the "Phase 10: SC-006 not met" section stating that `specs/012-jit-parity-followups` (US2/T023-T037) implemented the identified calling-convention fix (`collapseArgs`, `n=1`/`tags=nullptr`), verified it correct at the IR level (23/84 sites) and rendered-output level (91/91 visual, FR-006 discrimination scene flip), but that it did **not** close the wall-clock gap — `ctest -L perf-manual` still shows 1.06-1.45x ratios, statistically indistinguishable from the original 1.048-1.464x — with the diagnosed reason (collapse only reaches uniform-classified prologue sites, not the dominant varying-body cost).
- [X] T051 [P] Update `DEVNOTES.md` — mark the three follow-up items resolved in `## Todos`, and reconcile the "Review in next steps — shading interpreter and LLVM JIT" subsection under `## Open Issues`, striking any item this feature closed and leaving the spec-013 candidates ([contracts/light-iteration.md](./contracts/light-iteration.md) §4) in place. Done: reconciled the `numVerts`-tax Open Issues bullet (US2 implemented/verified but did not close the wall-clock gap — still open); struck the "op_* callee guarantee...not audited" Review-in-next-steps item as discharged (US2's `collapseArgs` exclusion list + IR-level audit, 23/84 sites, zero forbidden combinations); marked the `## Todos` follow-up-spec bullet `[x]` with a two-of-three-closed nuance (usfroma crash and illuminance/runLights duplication fixed; numVerts tax still open, SC-004/005/006 unmet). Left Review-in-next-steps items 1, 2, 4 and the spec-013-candidates Todos line untouched — genuine spec-013 candidates per contracts/light-iteration.md §4.
- [X] T052 [P] **Satisfy Constitution Principle VII with an actual site edit** — add an entry to `docs/site/content/development/releases.md` (the correct path; the tree is `docs/site/`, not `site/`) covering the two user-observable outcomes of this feature: (a) the `usfroma` crash a shader author can trigger from RSL source is fixed, with the RSL shape that used to crash, and (b) JIT shader execution performance changed, with the measured direction and magnitude from T037. Keep it proportionate — a release-note entry, not a design document; internal mechanics (the collapse predicate, the light-iteration convergence) stay in `DEVNOTES_DETAILS/`. **No Principle VII exemption is claimed by this feature** — the plan's Constitution Check records VII as PASS discharged by this task. The existing `.github/workflows` site-deployment automation is unchanged and needs no task. Done: added a "Development build (2026-08-27)" section at the top of `docs/site/content/development/releases.md` covering (a) the `usfroma` crash fix with the exact triggering RSL shape (`usarr[findex] == "a"`, `findex` varying), and (b) the numVerts-tax investigation outcome per T037's measured figures — fix verified correct but no measurable wall-clock improvement (~1.05x-1.4x unchanged within noise). Internal mechanics kept out per the task's proportionality note.
- [X] T053 Fill the Stage 4 feature-level acceptance table in [quickstart.md](./quickstart.md) with the evidence produced by Phases 2–5, mapping SC-001 … SC-008 and FR-012 to their recorded artifacts. Verify T052 landed: `git status --short docs/site/content/development/releases.md` must show the file **modified** (the inverse of the old, incorrect check), and the entry must name both the crash fix and the performance change. Done: added a third `Result` column to the Stage 4 table with concrete recorded outcomes drawn from `measurements.md` — SC-001 met (T012/T013, 5/5 pre-fix crashes, 5/5 post-fix passes); SC-002 met (T020a revert/restore cycle: revert reproduces MaxBlockAvgDiff 116.34 vs. original 116.81, restore returns to 100% pass); SC-003 met (three independent before/after pairs captured and diffed); SC-004/SC-006 **not met** (T037: zero of five scenes show confirmed improvement, arrayops/cfrom gap widened fractionally +0.004), SC-005 reported not gated; SC-007 met (T036 91/91 then T046 91/91 final, zero modified references); SC-008 met (T047's zero-remaining-`runLights`/`runCategoryLights` grep, one converged `iterateLights` implementation); FR-012 met (all four internal docs updated T048-T051 plus the T052 `releases.md` entry, confirmed modified via `git status`). Verified T052 landed: `git status --short docs/site/content/development/releases.md` → ` M docs/site/content/development/releases.md`.
- [X] T054 Final verification sweep: `ctest --test-dir build -L libshader --output-on-failure` and `ctest --test-dir build -L visual --output-on-failure` clean versus the Phase 2 baseline **allowing for exactly the two added test entries the Standing-rules delta table predicts for `base-visual.txt`** (T008b's discrimination scene and T020's `usfroma` probe scene) and nothing else, and `git status` shows **zero modified** files under `tests/visual/reference/` (SC-007, which admits no exceptions). Added files there are expected and permitted: **exactly two** — one for US1's probe scene (generated at T020) and one for the FR-006 uniform-in-conditional discrimination scene (T008b). Any third addition, any removal, or any modification fails this task. Done: `ctest -L libshader` 2/2 passed (0.08s); `ctest -L visual` 91/91 passed (60.07s) — matches T046's final count exactly, no regressions. Note: the task text's `tests/visual/reference/` path doesn't exist in this repo — the actual reference-image directory is `examples/rib/tests/references/` (per `tests/visual/CMakeLists.txt`); `git status --short` against the real path shows exactly the two permitted additions (`sphere-uniform-conditional-reyes.tif`, `sphere-usfroma-reyes.tif`) and **zero modified** files — SC-007 satisfied with no exceptions. Spec 012 is now fully complete: all Phase 6 Polish tasks (T048-T054) closed.

---

## Dependencies & Execution Order

### Phase dependencies

- **Phase 1 (Setup, T001-T003d)** → no dependencies; the worktree has no `build/` yet, so nothing precedes it
- **Phase 1b (CI compliance, T003e-T003i)** → no dependencies, and **blocks nothing**. It touches only `.github/workflows/` plus one new file under `docs/tools/`, shares no file or measurement state with the JIT work, and needs no *openRender* build (T003e1 does want a local `hugo --minify` run to trial its validator, which is unrelated to the C++ build and to any timing state). Internally: T003e → T003e1 → T003f → T003g are **serial** (one file, `deploy-site.yml`; T003e1 additionally creates `docs/tools/link-validator.sh`, which nothing else touches); T003h is `[P]` against that sequence (it edits `release.yml` only) but cannot be verified until T003f adds `workflow_call:`; T003i is last, since it deletes the live deployer whose steps T003e ports from. Run it whenever convenient — it is *not* a prerequisite for Phase 2
- **Phase 2 (Foundational)** → depends on Phase 1 (T001-T003d) **only**, not on Phase 1b. **BLOCKS Phases 3, 4, 5**
- **Phase 3 (US1, P1)** → depends on Phase 2; independent of Phases 4 and 5
- **Phase 4 (US2, P2)** → depends on Phase 2; independent of Phases 3 and 5
- **Phase 5 (US3, P3)** → depends on Phase 2; independent of Phases 3 and 4
- **Phase 6 (Polish)** → each task depends on its story landing; T053/T054 depend on all three

### User story dependencies

None. The spec's Assumptions state it outright: *"none blocks another, and each
is separately deliverable and separately verifiable."* The couplings that exist
are environmental, not logical:

- **T033 ↔ T045**: US2 and US3 both invalidate every `.slo`. They are the same
  physical operation and must never both run: the story that reaches its
  rebuild step **first** regenerates; the one that reaches it **second** skips
  regeneration, runs the `stat` audit, and re-runs **both** stories'
  verification tails (T034/T035/T036 and T046/T047) against the single combined
  artifact set. The full rule is stated identically in T033 and T045.
- **T007/T008/T037**: every `perf-manual` run needs the machine idle, so no
  other story may build or render during them.
- **US1 landing before US2/US3 finish**: each stream verifies against its **own**
  before-pair — T015a for US1, T032a for US2, T044a for US3, all under
  `specs/012-jit-parity-followups/baselines/` — not against the shared Phase 2
  `base-*.txt`. That is what makes a landed US1 fix harmless to the other two.
  But a **compiler-side** US1 fix (T016) forces the full-tree artifact
  regeneration in T017, which every later `-slo` check must postdate. T015a is
  by construction captured before T017, so in the compiler-side case its
  `-slo` rows describe artifacts the fix has since invalidated. Resolution: if
  and only if T015 approves a compiler-side fix, **re-capture** `us1-before-*`
  immediately after T017 *with the fix reverted* and the artifacts regenerated
  from the reverted compiler — the same transient revert T020a performs, done
  once and used for both. In the interpreter-side case (the expected one) no
  re-capture is needed and T015a's files stand.

### Within each user story

- **US1**: T009/T010 in parallel → T011 → T012 → T013 → T014 → **T015 STOP** → **T015a** (capture US1's before-pair — the tree still carries the defect and none of the fix) → T016 → T017 → T018 → T019 → T020 → **T020a** (negative evidence, transient local revert; not a baseline) → T021 → T022
- **US2**: T023 → T024 → T025 → T026 → T027/T028/T029/T030 in parallel → T031 → T032 → **T032a** (capture US2's before-pair, while the tree is still pre-collapse) → T033 → T034 → T035 → T036 → T037
- **US3**: T038 → T039 → T040 → T041 → T042 → T043 → T044 → **T044a** (capture US3's before-pair, while the tree is still pre-convergence) → T045 → T046 → T047

Each story's before-pair capture (T015a / T032a / T044a) is the last step
**before that story's own change becomes live** — which is a different position
in each sequence, so read them individually rather than by analogy. US2 and US3
carry their rebuild at the tail (T033, T045), so their captures sit immediately
ahead of it. US1's fix goes live mid-sequence at T016/T017, so T015a sits
immediately after the STOP, **not** near T022. Capturing after the change is
live would measure the change against itself and make SC-003 unfalsifiable.

### Parallel opportunities

- **Phase 1b (T003e–T003i), against literally anything**: the widest parallel
  window in this file. It needs no openRender build, no render artifacts and no
  quiescent machine, and writes only to `.github/workflows/` plus one new file
  under `docs/tools/`, so it can run alongside Phase 1, the
  Phase 2 baselines, or any story — including during the exclusive-machine
  timing runs, which it cannot perturb. (T003e1 runs `hugo` locally to trial its
  validator; a Hugo build is not a timing-sensitive workload and touches nothing
  the measurements read.) Note this is parallelism *of the phase
  against other phases*: internally T003e → T003e1 → T003f → T003g are serial on
  one file, and only T003h carries `[P]`.
- **Across stories, after Phase 2**: all of Phase 3, Phase 4's T023–T032, and
  Phase 5's T038–T043 may proceed concurrently — authoring and unit verification
  only. Their rebuild/regenerate/measure tails (T033, T037, T045) serialize, and
  so do the three before-pair captures (T015a, T032a, T044a): each is a full
  `ctest` sweep, so running two at once corrupts both and would also disturb any
  concurrent `perf-manual` timing.
- **Within US1**: T009 (probe shader) and T010 (scene pair) touch different files.
- **Within US2**: T027, T028, T029, T030 are one independent edit per instruction
  family against the same predicate.
- **Within Polish**: T048, T049, T050, T051, T052 touch five different
  documentation files — four internal, one under `docs/site/` — and may be
  drafted concurrently, alongside any phase.

---

## Parallel Example: User Story 2

```text
# After T023–T026 (audit discharged, predicate and helper in place),
# launch the four family edits together — same file region, distinct
# dispatch sites, one shared predicate:
T027 [P] [US2] Collapse DEFOPCODE arithmetic — emitBin/emitUn/emitTern, llvmEmitter.cpp:459-509
T028 [P] [US2] Collapse DEFFUNC builtin call sites in llvmEmitter.cpp
T029 [P] [US2] Collapse DEFSHORTFUNC sites (environment ×2, shadow ×2, bake3d)
T030 [P] [US2] Collapse DEFSHORTOPCODE sites, or record zero real uses

# Then serialize: T031 (confirm DEFLIGHTFUNC excluded) → T032 (write exclusion
# list) → T033 (rebuild + regenerate ALL .slo) → T034 → T035 → T036 → T037.
```

---

## Implementation Strategy

### MVP first (User Story 1 only)

1. Phase 1 (T001–T003d, including T001a and T003a–T003d) — make the worktree buildable and its artifacts trustworthy
2. Phase 2 (T004–T008b, including T006a) — capture every "before"
3. Phase 3 (T009–T022) — reproduce, STOP, fix, cover
4. **STOP and validate**: the crash is gone in 5/5 runs, a new executing test covers it, no existing reference image moved

US1 alone is a shippable increment: it is the only one of the three that is an
outright failure a shader author can hit today with no workaround.

**Phase 1b (T003e–T003i) is deliberately absent from this sequence** and from
the incremental-delivery sequence below. It is CI-compliance work that shares
nothing with the JIT chain; it neither blocks nor is blocked by any step here.
Slot it in wherever convenient — see § Phase dependencies.

### Incremental delivery

1. Phases 1–2 → shared verification foundation
2. Phase 3 → US1 → validate → deliverable (SC-001, SC-002)
3. Phase 4 → US2 → validate → deliverable (SC-004, SC-005, SC-006)
4. Phase 5 → US3 → validate → deliverable (SC-008)
5. Phase 6 → documentation reconciliation and feature-level acceptance

Each story adds value without breaking the previous one, and each carries its
own before/after regression pair per SC-003.

### Parallel team strategy

With Phase 2 complete, three developers can work simultaneously:

- **Developer A** → US1 (`execute.cpp` or `rslo.y`, plus new shader/scene/reference)
- **Developer B** → US2 (`llvmEmitter.cpp`, plus the `rslOps.cpp` audit)
- **Developer C** → US3 (`shading.cpp`, `execute.cpp` light-iteration macros)

They coordinate at exactly three points: the shared `.slo` regeneration (T033/T045),
the exclusive-machine timing run (T037), and the US1 STOP (T015), which gates
only Developer A. Note that A and C both touch `src/libshader/shading/execute.cpp`
— A at the array-access opcode, C at the light-iteration macros — so their edits
must be coordinated even though they are logically independent.

Phase 1b is assignable to any of the three (or a fourth person) at any time,
including before Phase 2 completes: it touches only `.github/workflows/` plus
one new file under `docs/tools/`, and coordinates with nobody. Its own internal
order is serial — T003e → T003e1 → T003f → T003g on `deploy-site.yml`, T003h on
`release.yml` alongside them, T003i last.

---

## Notes

- `[P]` marks tasks touching different files with no dependency on an incomplete task
- `[Story]` labels map every implementation task to its user story for traceability
- Each user story is independently completable and testable
- Verify a story's independent test criteria before moving to the next
- **Never commit automatically** — commit only when the maintainer explicitly asks
- Stop at any checkpoint to validate before proceeding
