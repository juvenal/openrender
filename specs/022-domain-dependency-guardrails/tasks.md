---

description: "Task list for spec 022: domain-dependency guardrails"
---

# Tasks: Domain-Dependency Guardrails

**Input**: Design documents from `specs/022-domain-dependency-guardrails/`

**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/archcheck-cli.md, contracts/data-files.md, quickstart.md

**Tests**: REQUIRED. Constitution III (TDD, non-negotiable) applies: in every phase, the fixture scenarios and unit tests are written first and must fail before the implementation tasks that make them pass.

**Organization**: tasks are grouped by user story. **Every phase ends in a review/commit pause:** ctest results are reported, changes are staged, and work waits for the maintainer's commit. The agent never commits.

**Working directory**: `/Volumes/Projects/Development/CLI/openrender-worktrees/022-domain-dependency-guardrails`, build tree `build/` inside it. All paths below are repo-relative.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story the task belongs to (US1–US4)

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: the `tests/architecture/` scaffold and build wiring. No behavior yet.

- [X] T001 Create directory layout `tests/architecture/{archcheck,unit,fixtures}/` per plan.md › Project Structure
- [X] T002 Create `tests/architecture/CMakeLists.txt`:
  - build an `archcheck` executable from `tests/architecture/archcheck/*.cpp` (C++20, the project's warning flags via `openrender_common_flags`; no link to renderer libraries);
  - build a `test_archcheck_parsers` executable from `tests/architecture/unit/test_archcheck_parsers.cpp`, registered as ctest `Architecture_Parsers` with `LABELS "architecture;unit"`;
  - add `add_subdirectory(architecture)` to `tests/CMakeLists.txt`.
- [X] T003 [P] Create a minimal `tests/architecture/archcheck/main.cpp` that parses the contract options (`--source-dir`, `--build-dir`, `--config`, `--registry`, `--platform`, `--format`, `--emit-allowlist`, `--quiet`, `--help`) and exits 2 with "not implemented", so the target links and every fixture fails before implementation (TDD red)
- [X] T004 [P] Create the four data files with header comments only, in the formats of contracts/data-files.md:
  - `tests/architecture/domain-map.txt`
  - `tests/architecture/domain-rules.txt`
  - `tests/architecture/allowlist.txt`
  - `tests/architecture/metrics-baseline.txt`
- [X] T005 Add a CMake helper `archcheck_add_fixture(<name> <expected-exit> <pass-regex>)` in `tests/architecture/CMakeLists.txt`. It must:
  - compile `tests/architecture/fixtures/<name>/src/**` as an OBJECT library, so real `.o` and `.o.d` files exist;
  - register ctest `ArchFixture_<name>`, with every `-` in the fixture directory name replaced by `_` so names stay regex-friendly (e.g. `fixtures/metric-increase/` → `ArchFixture_metric_increase`), label `architecture`, running `archcheck` with `--source-dir`/`--config`/`--registry` pointed at the fixture and `--build-dir ${CMAKE_BINARY_DIR}`;
  - assert the exit code and `PASS_REGULAR_EXPRESSION`.
  - **Isolation is by `--source-dir` scoping** (research R10): only sources under `<source-dir>/src/` (plus mapped `@build/` sources) are analysed, so a fixture sees only its own objects in the shared build tree. No new option.
- [X] T006 Configure and build: `cmake -B build -S . && cmake --build build -j`. `archcheck` and `test_archcheck_parsers` build under `-Werror`.

**Checkpoint (PAUSE 1)**: the scaffold builds; the full existing `ctest` suite is still green. Stage, summarize, wait for review and commit.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: data-file parsing, the domain map and the rule table, which every story needs. Tests first.

**⚠️ CRITICAL**: no user-story work begins until this phase is complete.

### Tests first (must fail before T008–T016)

- [X] T007 Write parser unit tests in `tests/architecture/unit/test_archcheck_parsers.cpp`, covering:
  - (a) domain-map rules: a `file` rule beats a `prefix` rule; the longest prefix wins; a duplicate pattern is an error; an unknown class is an error; `@build/` paths;
  - (b) rule table: wildcards (`Core -> *`, `Consumer -> *`, `* -> Infra`); "same-class edges are always allowed"; a cyclic domain+Infra subgraph is rejected; an `Infra -> X` entry is rejected;
  - (c) allowlist lines: `<id>` must be "`DB-` + ≥3 digits"; `<kind>` is `include` | `link`; optional `macos` | `linux` tag; a trailing `#` comment is ignored; duplicate `(kind, from, to, platform)` is an error; the same `(kind, from, to)` both tagged and untagged is an error;
  - (d) registry headings `^### (DB-\d{3,})\b`: Open before `## Resolved`, Resolved after it;
  - (e) metrics-baseline lines;
  - (f) depfile parsing: backslash continuations, escaped spaces, multiple targets;
  - (g) `#include` directive extraction: quoted vs angle, comments, and preprocessor conditionals left to depfile resolution.

  Errors carry `file:line`.

**Checkpoint (TEST-APPROVAL 2a)**: the parser unit tests are written and failing (red). Stage, show the failing output, and wait for the maintainer's approval of the tests before T008.

### Implementation

- [X] T008 Define the core types in `tests/architecture/archcheck/config.h`, exactly as in data-model.md:
  - `Class` = `SceneInterface | GraphicsState | Geometry | Hiders | Shading | Texturing | ShaderDSO | Procedures | Display | Core | Infra | Consumer`;
  - `Rule`, `Allow`, `AllowlistEntry` with identity `(kind, from_file, to, platform)`;
  - `RegistryEntry` with `status` Open/Resolved;
  - `MetricBaseline`.
- [X] T009 Implement the domain-map loader and resolver in `tests/architecture/archcheck/config.cpp`. A `file` rule beats a `prefix` rule; among prefixes the longest match wins; duplicate patterns are an error. Unresolved scanned files are reported as `unclassified: <path>` (exit 2).
- [X] T010 Implement the rule-table loader in `tests/architecture/archcheck/config.cpp`:
  - wildcards, plus "same-class edges are always allowed";
  - at load, "the graph restricted to domains plus Infra MUST be acyclic, and `Infra -> <domain>` MUST NOT appear", otherwise exit 2.
- [X] T011 Implement the allowlist, registry and baseline loaders in `tests/architecture/archcheck/config.cpp`, with the validation rules of contracts/data-files.md. Parse errors are exit 2 with `file:line`.
- [X] T012 Implement path canonicalization (`std::filesystem::weakly_canonical`) and repo-relative/`@build/` path mapping in `tests/architecture/archcheck/config.cpp`. Research R2 requires this because depfiles may record `/Users/...` while the checkout is reached through `/Volumes/...`.
- [X] T013 Fill `tests/architecture/domain-rules.txt` as a verbatim mirror of the constitution v1.2.0 table, per contracts/data-files.md: `SceneInterface -> GraphicsState Geometry`; `Geometry -> GraphicsState Procedures`; `Shading -> GraphicsState Texturing ShaderDSO`; `Hiders -> GraphicsState Geometry Shading Display`; empty rows for the other domains; `Core -> *`; `Consumer -> *`; `* -> Infra`. Cite constitution v1.2.0 in a header comment.
- [X] T014 Fill `tests/architecture/domain-map.txt` for the real tree, per research R4:
  - **Infra:** `src/common/`, `src/includes/`, `src/ri/core/`;
  - **Shading** overrides for the `src/ri/core` shims: `shader.h`, `shading.h`, `shaderPl.h`, `noise.h`;
  - **SceneInterface:** `src/ri/parse/`, `src/ri/output/`;
  - **GraphicsState:** `src/ri/state/`;
  - **Geometry:** `src/ri/geometry/`, with `dlo.h` and `implicit.h` → **Procedures**;
  - **Hiders:** `src/ri/hiders/`;
  - **Texturing:** `src/ri/texture/`;
  - **Shading:** `src/libshader/`;
  - **Display:** `src/display/`;
  - `src/ri/render/shadeop.h` → **ShaderDSO**;
  - **Core:** `src/ri/render/`, `src/ri/vector/`, `src/ri/hcshader.*`;
  - **Consumer:** `src/ri/dataviewer/`, `src/orender/`, `src/oshader/`, `src/otexmake/`, `src/precomp/`, `src/preview/`, `src/sloinfo/`, `src/rslo/`, `src/python/`, `src/lua/`;
  - **generated sources:** `@build/` rules sending `rib`, `dso`, `variable` and `ifexpr` to SceneInterface, and the `rslo` grammars to Shading.

  Then enumerate the scanned files (`src/**/*.{c,cc,cpp,h,hpp,y,l}`, FR-002) and confirm every one resolves. Other files (e.g. `src/CMakeLists.txt`, `src/ri/CMakeLists.txt`, Swift/Python/Lua, assets) are ignored and need no rule.
- [X] T015 Implement the report model and writers in `tests/architecture/archcheck/report.{h,cpp}`:
  - text lines `VIOLATION`/`STALE`/`BADID`/`CYCLE`/`ALLOWED`/`CONSUMER`/`NOTBUILT`/`UNRESOLVED`/`METRIC`/`WARNING`/`NOTE`/`RESULT`;
  - JSON with the contract's keys;
  - deterministic ordering, "sorted by class, then file, then target" (SC-007);
  - exit codes: 0 pass, 1 check failed, 2 configuration/environment error.
- [X] T016 Run `ctest --test-dir build -R Architecture_Parsers`. All parser tests pass, and the full existing suite stays green.

**Checkpoint (PAUSE 2)**: the configuration layer is complete and unit-tested. Stage, summarize, wait for review and commit.

---

## Phase 3: User Story 1 — New boundary violations are caught automatically (Priority: P1) 🎯 MVP

**Goal**: forbidden include and link edges, and new cycles, fail with actionable messages; allowed edges and unclassified files behave per spec.

**Independent Test**: the fixture scenarios below pass (each asserts exit code and message), without touching production sources (spec US1, SC-001).

### Fixtures first (must fail until T024–T028 land)

- [X] T017 [P] [US1] Fixture `tests/architecture/fixtures/clean/`: a tiny tree with Geometry, GraphicsState, Hiders and Infra files using only allowed edges (e.g. Hiders → Geometry include and link). Expect exit 0 and `RESULT PASS` (US1 #5).
- [X] T018 [P] [US1] Fixture `tests/architecture/fixtures/forbidden-include/`: a GraphicsState file including a Shading header. Expect exit 1 and the regex `VIOLATION include .*\[GraphicsState -> Shading\] rule:` (US1 #2).
- [X] T019 [P] [US1] Fixture `tests/architecture/fixtures/forbidden-link/`: a Texturing `.cpp` calling a function defined only in a Hiders `.cpp`, with no header include of a Hiders header. Expect exit 1 and the regex `VIOLATION link .*\[Texturing -> Hiders\] defined in:` (US1 #3).
- [X] T020 [P] [US1] Fixture `tests/architecture/fixtures/new-cycle/`: Geometry → Shading and Shading → Geometry edges, neither allowlisted. Expect exit 1 and a `CYCLE` line without `[allowlisted]` (US1 #4). Add a second fixture `new-cycle-allowlisted/` where both edges are allowlisted against an Open registry ID: expect exit 0 and `CYCLE … [allowlisted]`.
- [X] T021 [P] [US1] Fixture `tests/architecture/fixtures/unclassified/`: a source file outside every map rule. Expect exit 2 and `unclassified: ` (US1 #6). The unclassified file must be a scanned type (e.g. a `.cpp`). Add a non-scanned file (e.g. `notes.txt`) outside every rule to the same fixture, and assert that it does **not** appear in the output.
- [X] T022 [P] [US1] Fixture `tests/architecture/fixtures/forwarding-shim/`: an Infra-located forwarding header mapped to Shading by a `file` rule, included from GraphicsState. Expect exit 1 with `[GraphicsState -> Shading]` (spec Edge Cases: forwarding can't hide an edge).
- [X] T023 [P] [US1] Fixture `tests/architecture/fixtures/incomplete-build/`: run with `--build-dir` pointing at an empty directory. Expect exit 2 and a message naming the missing build data (US1 #7, FR-001).

**Checkpoint (TEST-APPROVAL 3a)**: the US1 fixtures are written and failing (exit 2, "not implemented", via the T003 stub). Stage, show the failing output, and wait for the maintainer's approval of the tests before T024.

### Implementation

- [X] T024 [US1] Implement direct-include extraction plus depfile resolution in `tests/architecture/archcheck/includes.{h,cpp}`, per research R2 and FR-003:
  - parse each scanned file's direct `#include`s;
  - resolve each against the depfile of a translation unit that compiled the file; count an include only if the resolved file is in that TU's depfile (inactive conditional branches drop out);
  - headers are resolved through any TU that includes them;
  - headers no TU includes are listed `UNRESOLVED` and never fail;
  - read Makefiles `<obj>.o.d`, and Ninja via `ninja -t deps`;
  - any other generator, or missing deps data, is exit 2.
- [X] T025 [US1] Implement batched `nm -g -A` collection and symbol tables in `tests/architecture/archcheck/symbols.{h,cpp}`, per research R3 and FR-004:
  - scan only objects whose source is under `<source-dir>/src/` or `@build/`-mapped (non-test code; research R10 scan scope);
  - normalize by stripping the macOS leading `_`;
  - demangle with `abi::__cxa_demangle` for display only (the mangled name stays the key);
  - de-duplicate sources compiled into several object libraries;
  - missing `nm` is exit 2.
- [X] T026 [US1] Implement link-edge attribution in `tests/architecture/archcheck/symbols.cpp`:
  - an edge is "referencing source file → symbol";
  - ignore it if any definer is in the referencing class or Infra;
  - ignore it if the project defines the symbol nowhere;
  - otherwise attribute it to every remaining definer class (covers the substitution pairs).
- [X] T027 [US1] Implement edge classification in `tests/architecture/archcheck/graph.{h,cpp}`:
  - statuses `allowed` / `allowlisted` / `violation` / `consumer-reach`, per data-model.md › Edge;
  - consumers never fail;
  - SCC-based cycle detection, reporting each cycle's path and edges; a cycle fails only if it contains a non-allowlisted edge (FR-006).
- [X] T028 [US1] Wire `tests/architecture/archcheck/main.cpp`: load the configuration, extract edges, classify, report, exit code. `--platform` defaults to the detected host. Every `VIOLATION` line names the referencing file, the header or symbol (raw + demangled + defined-in), both classes and the rule (FR-007). `--help` documents every option, exit code and data file, per contracts/archcheck-cli.md (constitution IV).
- [X] T029 [US1] Register fixtures T017–T023 via `archcheck_add_fixture` in `tests/architecture/CMakeLists.txt`, then run `ctest --test-dir build -L architecture`. All US1 fixtures pass, and the full existing suite is unchanged.

**Checkpoint (PAUSE 3, MVP)**: detection works end to end on fixtures. Stage, summarize, wait for review and commit.

---

## Phase 4: User Story 2 — Every known violation is registered and the allowlist can only shrink (Priority: P1)

**Goal**: the allowlist/registry integrity rules, the bootstrap, a fully registered real tree, and the real-tree test passing.

**Independent Test**: the allowlist fixtures below pass; `Architecture_DomainBoundaries` passes on the real tree; `registry-reconciliation.md` accounts for V1–V21 (SC-002, SC-003).

### Fixtures first (must fail until T035–T037 land)

- [X] T030 [P] [US2] Fixture `tests/architecture/fixtures/stale-entry/`: an allowlist entry whose `from` file *was* compiled but whose edge doesn't occur. Expect exit 1 and `STALE ` (US2 #3). Add a variant `stale-now-allowed/`: the entry's edge exists but is allowed by the fixture rules. Expect exit 1 and `STALE` (US2 #7).
- [X] T031 [P] [US2] Fixtures `tests/architecture/fixtures/unknown-id/` and `tests/architecture/fixtures/resolved-id/`: entries referencing `DB-099` (absent) and an ID listed after `## Resolved` in the fixture's `REGISTRY.md`. Expect exit 1 and `BADID` (US2 #2, FR-009).
- [X] T032 [P] [US2] Fixture `tests/architecture/fixtures/other-platform-tag/`: two entries, one tagged for the non-host platform whose edge doesn't occur (expect it ignored), and one tagged for the host whose edge doesn't occur (expect `STALE`). Pass `--platform` explicitly so the test is host-independent (US2 #5, FR-010a).
- [X] T033 [P] [US2] Fixture `tests/architecture/fixtures/not-built/`: an entry whose `from` file exists in the fixture tree but is excluded from the fixture's OBJECT library. Expect exit 0 and `NOTBUILT` (US2 #6, FR-010).
- [X] T034 [P] [US2] Fixture `tests/architecture/fixtures/tagged-untagged-dup/`: the same edge listed both untagged and tagged `macos`. Expect exit 2 with the file:line of the duplicate (FR-010a, contracts/data-files.md).

**Checkpoint (TEST-APPROVAL 4a)**: the US2 fixtures are written and failing. Stage, show the failing output, and wait for the maintainer's approval of the tests before T035.

### Implementation

- [X] T035 [US2] Implement allowlist matching in `tests/architecture/archcheck/graph.cpp`:
  - an entry applies when its platform tag matches `--platform`, or it has none;
  - `allowlisted` = an applying entry with the exact `(kind, from, to)` edge;
  - `NOTBUILT` = an applying entry whose `from_file` wasn't compiled (`include`: absent from every TU depfile; `link`: not compiled into an object);
  - `STALE` = an applying, compiled entry whose edge is absent or now allowed;
  - `BADID` = an ID that is unknown, or Resolved in the registry.
- [X] T036 [US2] Implement `--emit-allowlist` in `tests/architecture/archcheck/main.cpp`: print every current non-allowed edge in allowlist format with `DB-???` IDs, sorted, then exit 0 (research R7).
- [X] T037 [US2] Register fixtures T030–T034 in `tests/architecture/CMakeLists.txt`. All US2 fixtures pass.
- [X] T038 [US2] Bootstrap the real tree: run `build/tests/architecture/archcheck --source-dir . --build-dir build --config tests/architecture --emit-allowlist`, and save the output (not committed) to `build/arch-bootstrap.txt`. Group the edges by `(from class → to class)` and by file, to drive the re-measurement.
- [X] T039 [US2] Re-measure V1–V21 on the current tree. Use the bootstrap groups for the edge-based findings, and targeted greps for the non-edge findings:
  - V6: duplicated motion-blur/DOF code;
  - V9: the JIT illumination-hook mismatch;
  - V10: the hider `strcmp` chains and `hcshader` fallback;
  - V11: the plug-in ABI defects;
  - V15: message passing;
  - V16: the compiler's core dependencies;
  - V17: friends;
  - V20: build structure;
  - V21: consumer reach.

  Write `specs/022-domain-dependency-guardrails/registry-reconciliation.md`, mapping each of V1–V21 to exactly one outcome: a new DB-NNN, "see DB-004", or "no longer present" with the reason (FR-011, SC-003).
- [X] T040 [US2] Write the new registry entries DB-005… in `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`, above `## Resolved`, in the existing DB-001…DB-004 shape. Each gives:
  - the rule;
  - evidence re-measured on the current tree (file:line or symbol);
  - size ("the number of files and sites … as measured by the check");
  - a direction;
  - tracking.

  Until the issues exist, tracking is "remediation spec NNN (issue pending approval)". Overlaps with DB-004 say "see DB-004" (FR-011).
- [X] T041 [US2] Seed `tests/architecture/allowlist.txt` from `build/arch-bootstrap.txt`, replacing every `DB-???` with the registry ID T040 assigned. Keep one line per concrete edge, with the demangled `#` comment for link entries. No line may reference DB-001…DB-004 unless the edge belongs to that entry.
- [X] T042 [US2] Register the real-tree test `Architecture_DomainBoundaries` in `tests/architecture/CMakeLists.txt`: label `architecture`, `TIMEOUT 120`, command `archcheck --source-dir ${CMAKE_SOURCE_DIR} --build-dir ${CMAKE_BINARY_DIR} --config ${CMAKE_SOURCE_DIR}/tests/architecture`. It passes on macOS with `RESULT PASS`, and the `ALLOWED` lines cover every new DB ID (SC-002).
- [X] T043 [US2] Linux validation through the existing path (rsync + Maestri Linux/GCC 13, project memory):
  - build;
  - run `ctest -L architecture`;
  - add `linux`/`macos` tags in `tests/architecture/allowlist.txt` for the edges that exist on one platform only;
  - re-run on both platforms.

  Linux `ctest -L architecture` passing is a **merge prerequisite**, because the release Ubuntu job runs `ctest` (spec FR-010a). If the Linux path is unavailable at this pause, the phase stays open (not ready to merge) until it is. Record the results in `specs/022-domain-dependency-guardrails/validation.md`.
- [X] T044 [US2] Draft the 16 tracking-issue texts, one per remediation spec 023–038, in `specs/022-domain-dependency-guardrails/tracking-issues.md`. Each gives: title, label (`enhancement`), the scope from the remediation plan, the DB entries it resolves, and acceptance. **Do not file.**

**Checkpoint (PAUSE 4)**: the real tree is fully registered and the check passes. **The maintainer approves filing the 16 issues** (an outward-facing action) and commits.

- [X] T045 [US2] **After the maintainer's explicit approval at PAUSE 4:** file the 16 issues from `tracking-issues.md` with `gh issue create`, then replace each "issue pending approval" in `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` with the issue link(s) (SC-003). Re-run `Architecture_DomainBoundaries`.

---

## Phase 5: User Story 3 — Progress toward a smaller renderer core is measurable (Priority: P2)

**Goal**: a per-class renderer-core metric (references + distinct members) with baseline comparison, and a deterministic report.

**Independent Test**: the metric fixtures pass, and two runs produce byte-identical reports (US3, SC-007).

### Fixtures first

- [X] T046 [P] [US3] Fixture `tests/architecture/fixtures/metric-increase/`: a Geometry file with `CRenderer::a` used twice and `CRendererContext::b` once, plus one reference inside a comment and one inside a string literal (neither counted). Its baseline is `Geometry 2 1`. Expect exit 0, `WARNING metric Geometry refs 3 > baseline 2 (+1)`, and `distinct 2 > baseline 1 (+1)` (US3 #2, FR-012).
- [X] T047 [P] [US3] Fixture `tests/architecture/fixtures/metric-decrease/`: the same tree with baseline `Geometry 5 3`. Expect exit 0 and `NOTE metric Geometry refs 3 < baseline 5` (US3 #3).

**Checkpoint (TEST-APPROVAL 5a)**: the US3 fixtures are written and failing. Stage, show the failing output, and wait for the maintainer's approval of the tests before T048.

### Implementation

- [X] T048 [US3] Implement the scanner in `tests/architecture/archcheck/metrics.{h,cpp}`, per FR-012 / research R8:
  - strip comments and string/char literals;
  - match `CRenderer::<member>` and `CRendererContext::<member>`, with definitions counted;
  - scan every scanned file outside Core and Consumer, grouped by class, plus `TOTAL`;
  - count generated sources through their `.y`/`.l` grammars only.
- [X] T049 [US3] Implement the baseline comparison and `METRIC`/`WARNING`/`NOTE` output in `tests/architecture/archcheck/report.cpp`: an increase is a warning, a decrease is a note, neither fails. Register fixtures T046–T047.
- [X] T050 [US3] Seed `tests/architecture/metrics-baseline.txt` from a real-tree run. Add ctest `Architecture_Determinism` (label `architecture`): run `archcheck --format json` twice into `${CMAKE_BINARY_DIR}` and compare with `${CMAKE_COMMAND} -E compare_files` (SC-007; no Python).

**Checkpoint (PAUSE 5)**: the metric is reported, compared and deterministic. Stage, summarize, wait for review and commit.

---

## Phase 6: User Story 4 — Dead cross-domain includes are removed (Priority: P3)

**Goal**: remove the vestigial includes listed in FR-013, each proven harmless.

**Independent Test**: each removal builds with `-Werror` and no new warnings; the full suite gives identical results; the corresponding allowlist lines are deleted (otherwise `STALE` fires); the check passes (US4 #1).

For each task:
1. delete the include;
2. `cmake --build build -j` (warnings-as-errors);
3. full `ctest --test-dir build`;
4. delete the matching allowlist line(s);
5. re-run `ctest -L architecture`.

If the build breaks, restore the include, keep its allowlist line, and note it in `registry-reconciliation.md` as a real violation.

- [X] T051 [US4] Remove `#include "texture.h"` from `src/ri/state/options.cpp`
- [X] T052 [US4] Remove `#include "irradiance.h"` from `src/ri/state/attributes.cpp`
- [X] T053 [US4] Remove `raytracer.h`, `remoteChannel.h`, `brickmap.h`, `pointCloud.h` and `points.h` includes from `src/libshader/shading/shading.cpp`, one include at a time, each verified
- [X] T054 [US4] Remove `object.h` and `raytracer.h` includes from `src/ri/texture/pointCloud.cpp`, one at a time, each verified
- [X] T055 [US4] Remove `#include "points.h"` from `src/ri/hiders/reyes.cpp`
- [X] T056 [US4] From the T038 bootstrap, list any further cross-domain include whose target header provides no symbol used by the including file. Attempt each removal with the same procedure, and record the outcomes in `specs/022-domain-dependency-guardrails/registry-reconciliation.md` (V19 row).
- [X] T057 [US4] Linux validation of all removals (GCC 13, `-Werror`) via the existing Linux path. This is a **merge prerequisite**: if the Linux path is unavailable, the phase stays open (not ready to merge) until it is. Record the results in `specs/022-domain-dependency-guardrails/validation.md` (US4 #1).

**Checkpoint (PAUSE 6)**: removals complete, full suite identical. Stage, summarize, wait for review and commit.

---

## Phase 7: Polish & Cross-Cutting Concerns

- [X] T058 [P] Write the "Architecture check" section in `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` (FR-014):
  - purpose; how to run it (`ctest -L architecture`, direct invocation); how to read each report line type; the data files;
  - **two worked examples of at most 5 steps each:** classifying a new file, and retiring a fixed violation (delete the allowlist line(s), move the registry entry to Resolved with commit and date, lower the metric baseline if applicable) (SC-006);
  - the known limitation that the release workflow runs only on releases.

  Verify that each worked example has at most 5 steps (SC-006).
- [X] T059 [P] Add a pointer to the check and its documentation in `CLAUDE.md`, under the dev-workflow notes (FR-014).
- [X] T060 Move registry entry DB-002 to `## Resolved` in `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`, recording "resolved by spec 022" and the date; the commit hash is added after the maintainer's merge. Prepare the closing comment text for issue #25 in `specs/022-domain-dependency-guardrails/tracking-issues.md`; it is closed **only after the maintainer merges** (FR-016).
- [X] T061 Measure the runtime budget. Time `ctest --test-dir build -L architecture` on a Release build, on the Apple Silicon machine and on Linux. Both measurements are required (SC-005); like T043/T057, the phase stays open until the Linux timing exists. Record the results in `specs/022-domain-dependency-guardrails/validation.md`; each must be at most 60 s (SC-005).
- [X] T062 Run the quickstart.md validation scenarios 1–7 and record the results in `specs/022-domain-dependency-guardrails/validation.md`, including:
  - the manual injection spot check on the real tree, reverted afterwards and never staged;
  - CI coverage, i.e. confirming `release.yml` runs plain `ctest` in all three jobs.
- [X] T063 Format all new sources under `tests/architecture/` with the repo's `.clang-format` (`clang-format -i`), rebuild, and confirm a second run produces no diff (constitution › Code Quality).
- [X] T064 Run the full `ctest --test-dir build`. It must equal the pre-feature baseline (FR-015, SC-004), apart from the new `architecture` tests, which all pass.

**Checkpoint (PAUSE 7, final)**: the feature is complete. Stage, summarize, wait for review and commit; then `/speckit-converge`.

---

## Dependencies & Execution Order

### Phase dependencies

- **Setup (Phase 1)** → **Foundational (Phase 2)** → **US1 (Phase 3)**.
- **US2 (Phase 4)** depends on US1: allowlist matching extends classification, and the real-tree run needs edge extraction.
- **US3 (Phase 5)** depends only on Phase 2 plus the report writer (T015). It could run in parallel with US1/US2, but is sequenced after US2 for review clarity.
- **US4 (Phase 6)** depends on US2: removals must delete their seeded allowlist lines.
- **Polish (Phase 7)** depends on all stories.

### Within each phase

- Fixtures/tests are written first and fail (TDD), then implementation, then registration and run.
- T045 (filing issues) requires the maintainer's explicit approval at PAUSE 4.
- T060's issue closure waits for the maintainer's merge.

### Parallel opportunities

- **Phase 1:** T003 ∥ T004.
- **Phase 3:** fixtures T017–T023 are all [P] (separate directories).
- **Phase 4:** fixtures T030–T034 are all [P].
- **Phase 5:** T046 ∥ T047.
- **Phase 7:** T058 ∥ T059.

Implementation tasks inside a story touch shared files (`graph.cpp`, `main.cpp`, `CMakeLists.txt`), so they stay sequential.

## Parallel Example: User Story 1

```text
# Write all US1 fixtures together (independent directories):
T017 fixtures/clean/    T018 fixtures/forbidden-include/    T019 fixtures/forbidden-link/
T020 fixtures/new-cycle/ + new-cycle-allowlisted/    T021 fixtures/unclassified/
T022 fixtures/forwarding-shim/    T023 fixtures/incomplete-build/
# then implement T024 → T025 → T026 → T027 → T028, register in T029
```

## Implementation Strategy

- **MVP = Phases 1–3 (US1):** a working detector proven on fixtures. It can't yet gate the real tree, because the allowlist isn't seeded.
- **Phase 4 (US2)** turns it into the real guardrail: the registry and allowlist are complete, the real-tree test is green, and rule 7 + rule 8 are satisfied (after issue filing).
- **Phase 5 (US3)** adds the progress metric that later specs use.
- **Phase 6 (US4)** is the low-risk cleanup.
- **Phase 7** adds the documentation and closes DB-002.

Each phase is a commit point for the maintainer, and no phase leaves the full suite red.

---

## Phase 8: Convergence

- [ ] T065 Name the broken rule on link `VIOLATION` lines in the text report (`tests/architecture/archcheck/report.cpp`): today they end with `defined in: …` and only the JSON carries the rule; add `rule: <allowed-dependency text>` like include violations, and extend `ArchFixture_forbidden_link`'s pass regex to require it, per FR-007 (partial)
- [ ] T066 Report the line of the offending `#include` on include `VIOLATION` lines (`file:line`), from the directive's line already extracted in `includes.cpp`, so a contributor can go straight to it; carry it in the JSON too and extend `ArchFixture_forbidden_include`'s regex, per SC-001 (partial)
- [ ] T067 Stop build-option-dependent code from producing false `STALE` failures: an allowlisted include whose directive is present in the source but inactive in this build (e.g. `src/ri/texture/imageInputExr.cpp` → `src/ri/parse/error.h`, inside `#ifdef HAVE_OPENEXR`) must be reported as not built rather than stale; decide and document the equivalent rule for link entries in files with `HAVE_OPENEXR` / `HAVE_ZLIB` / `OPENRENDER_HAVE_LLVM` blocks; add a fixture with a conditional include compiled with the option off, per spec Edge Cases "Build options that remove code" and FR-010 (partial)
- [ ] T068 Exclude test code under `src/` from the analysis: `src/libshader/tests/` is test code but is classified Shading, scanned, and contributes `DB-016 include src/libshader/tests/test_used_parameters_table.cpp -> src/ri/render/rendererc.h`; classify it as Consumer (or exclude it) in `tests/architecture/domain-map.txt`, delete the resulting stale allowlist line, and re-check the metric baseline, per FR-004 "non-test code" (contradicts)
- [ ] T069 Validate the check under the CI build configurations it will first run in: `release.yml` builds `INSTALL_SELFCONTAINED=ON` (vcpkg) and `OFF` (Homebrew/system) variants on `macos-13` (Intel), `macos-14` and `ubuntu-24.04`, none of which was measured; run `ctest -L architecture` in at least one `INSTALL_SELFCONTAINED=OFF` macOS build and record the results (and any extra `macos`/`linux`-tagged lines) in `validation.md`, per FR-001 and FR-010a (partial)
- [ ] T070 Either report every elementary domain cycle or record the chosen behavior: `findCycles()` reports one shortest closing cycle per non-allowed class-to-class hop, so a second cycle through the same hops is not listed separately; if kept, document it under research R5 and in the "Architecture check" section of `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`, per FR-006 "report each cycle with its full path" (partial)
- [ ] T071 Exercise the Ninja code path (`ninja -t commands` / `ninja -t deps` in `includes.cpp`), which no run has executed: configure a scratch `-G Ninja` build on the Linux host (or install ninja locally), run `archcheck` against it, compare its edge counts and `RESULT` with the Makefiles tree, and record the outcome in `validation.md`, per plan: research R2 (partial)
- [ ] T072 After the maintainer merges spec 022: add the merge commit hash to DB-002's Resolved note in `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` and close issue #25 with the prepared text in `tracking-issues.md` (`<merge commit>` filled in), per FR-016 (missing)
