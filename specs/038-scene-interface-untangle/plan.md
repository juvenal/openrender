# Implementation Plan: Scene Interface Untangle

**Branch**: `038-scene-interface-untangle` | **Date**: 2026-11-01 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/038-scene-interface-untangle/spec.md` (specified and
clarified 2026-11-01; constitution 1.3.0). Designed against master `d6910bd0` (spec 026 merged);
the coordinator merges master's tip `01501f10` (`d6910bd0` plus three doc-count corrections) into this
branch before implementation (Phase 0).

## Summary

The Scene Interface stops depending on the renderer core. The declared-variable table moves into
Graphics State (`CDeclarations`, reference counted, with a "global added" observer the core
installs). The Scene Interface owns a declaration service and a file-search service with a standard
implementation (no network) that the RIB writer, the preview and the data viewer use, while the full
renderer installs its own for its session. The grammar, the lexer and `RiBegin` call only those
services. The `if` evaluator becomes a Scene function that takes the context. `riHooks.h` and
`CDelayedData` become Scene Interface files.

The active RI context is installed and uninstalled explicitly (`riInstallContext` /
`riUninstallContext`, with the previous context kept in the installed one). The constructor no
longer assigns the global, and the archive path's `savedRenderMan` goes. The preview registers a
named back-end by a static initializer and opens a light session (`RiBeginBackEnd`). `ribOut`
compiles no Core file, and `ribVector` keeps only a pinned, shrink-only list of the Core files that
Geometry and Hiders still need.

On the shading side, the compiler owns its builtin and parameter-bit tables, checked against the
engine by a drift test, and its include path has nothing under `src/ri` or `src/libshader/shading`.
The JIT gains a DSO call: `op_dso_call` mirrors the interpreter's loop, and its operand slots are
recorded in the `.slo` and bound at every load through the same `lookupDSO`.

`precomp` takes its constants from a Hiders-owned constants header. DB-015 and DB-019 are resolved,
and DB-024 is narrowed. Every image, written RIB text, extracted preview scene, compiled shader and
diagnostic text stays identical.

## Technical Context

**Language/Version**: C++20, CMake ≥ 3.19, `-Werror`; flex and bison (mandatory, unchanged).

**Primary Dependencies**: none new. LLVM ≥ 15 for the JIT side (only `GlobalVariable`,
`NamedMDNode`, `alloca` and calls, all in LLVM 15); `libRIBShadeop` (spec 037) for operand data and
`exec`.

**Storage**: N/A, memory only. New files on disk are only test references (captured once) and the
pinned `ribVector` residual list.

**Testing**: CTest. Mechanical checks are `cmake -P` scripts under the label `architecture`
(`preview` for the preview one). In-process tests carry the labels `common`, `preview`,
`regression` and `libshader`. The `.rslo`/`.slo` parity renders run under `plugin_abi`. The
characterization references are written by a build target `scene-interface-capture-baselines`,
never by a ctest, which refuses to overwrite an existing reference unless `-DOR038_RECAPTURE=ON`. A
test that needs an API not yet written is registered `DISABLED` until the API lands, and the final
run has none left. [contracts/checks.md](contracts/checks.md) has the full list.

**Target Platform**: macOS 13.3+ (Apple Silicon, Clang) and Ubuntu 24.04 (GCC 13, LLVM 18).
`-DOPENRENDER_ENABLE_JIT=OFF` builds skip the `.slo` tests, as the `-slo` visual variants already do.

**Project Type**: renderer libraries (`libRIBRender`, `libRIBVector`, `libRIBOut`,
`libshader_*`) plus CLIs (`orender`, `oshader`, `precomp`) and the `orender-wire` preview.

**Performance Goals**: no measurable change. A parameter lookup in the parser gains one virtual
call. A JIT DSO call costs one C++ call per instruction with the interpreter's per-vertex loop
inside, and binding is one `lookupDSO` per slot per load.

**Constraints**: existing visual thresholds unchanged and green, a passing determinism sweep on both
hosts, and identical byte-pinned artifacts (written RIB text, preview extraction, shader sha256
manifests, `precomp` output) (FR-005, FR-019, SC-004). No allowlist line added (FR-025). No task depends on the merge of 038 (FR-030).
`CLAUDE.md` is never edited by an agent; its changes go into `claude-md-draft.md`, which the
maintainer applies. clang-format runs only on files that were clang-format clean before.

**Scale/Scope**: about 25 source files edited and 8 new (`state/declarations.{h,cpp}`,
`parse/riServices.{h,cpp}`, `parse/delayedData.h`, `parse/riCondition.h`, `hiders/rasterFlags.h`,
`compiler/builtinTables.cpp`), 1 file moved (`riHooks.h`), about 20 new checks and tests, 13
approved files edited (9 tests with code or include changes; 4 files whose comments only change:
`render.rib.in`, `render_1f.rib.in`, `scan.rib.in`, `check_versions_render.cmake`), and 10 allowlist
lines deleted.

No NEEDS CLARIFICATION remains. The ten design questions of the plan stage (research.md section Q)
were answered on 2026-11-02: the maintainer approved every recommendation, then changed Q8 (no version bump). The record is "Maintainer
decisions" below and research.md section D.

## Constitution Check

*GATE: checked before Phase 0 and re-checked after the design (below). Constitution 1.3.0.*

| Principle | Status | Notes |
|---|---|---|
| I. Clean code | PASS | Removes a constructor side effect, a dead `if (FALSE)` branch, the archive's private global and `RiBeginLite`; the standard declarations exist once, in Graphics State (the writer's own `declareDefaultVariables` table is its output state, not the parser's, and stays). |
| II. Language standards | PASS | C++20; no public C header changes (`ri.h`, `shadeop.h` unchanged). |
| III. TDD (non-negotiable) | PASS (planned) | Phase 1 writes every check and characterization before any change. Each phase starts with its tests and stops for TEST-APPROVAL. Characterizations are green on the starting tree by design; mechanical checks and the JIT parity tests are red. The drift test's red evidence is its mutation check (Q3). |
| IV. CLI | PASS | `orender`, `oshader` (`--help`, exit codes) and `precomp` behaviour unchanged; `precomp` output identical to the unmodified tool's. |
| V. Minimal dependencies | PASS | None added. |
| VI. Platform targeting | PASS | macOS and Linux through the same CMake; Linux validation at every phase checkpoint. |
| VII. Documentation/site | PASS (planned) | Registry, `DEVNOTES.md`, `PLUGIN_ABI.md` (`:154-155` "the JIT has no DSO call path"), the `src/ri/CMakeLists.txt` target comments, the site page `dso-shading.md` (a `.slo` can call an operand), `NEWS.md`/`ChangeLog.md`, and `claude-md-draft.md` for the maintainer. |
| VIII. Domain separation | PASS | Gate below. |

**Principle VIII — Domain Separation and Modular Boundaries**:

- [x] **Domains touched are named**: Scene Interface (`parse/`, `output/`, generated parsers, new
      `riServices`, `riCondition`, `delayedData`, moved `riHooks.h`), Graphics State (new
      `declarations`), Geometry (`delayed.h` loses a class; no other Geometry edit), Hiders (new
      `rasterFlags.h`, included by `reyes.h`), Shading (compiler tables and include path, emitter,
      `rslOps`, `sloLoader`, `llvmJit`), Shader DSO Operands (no change, consumed), renderer core
      (`rendererDeclarations*`, `rendererFileSearch`, `rendererContext`, `rendererServicesImpl.h`,
      `rendererStatics`, `renderer.h`, `rendererProcedures`, the CMake target lists), consumers
      (preview, data viewer, `precomp`, tests).
- [x] **No new cross-domain dependency outside the table, none on the core from a domain**: every
      added edge is in [contracts/allowlist-end-state.md](contracts/allowlist-end-state.md): Scene →
      Graphics State, Core → Scene/State, Shading → ShaderDSO, Hiders → Hiders, consumers → anything.
      Geometry never includes `delayedData.h`. The Scene Interface's Core edges go to zero.
- [x] **Interface changes specified before implementation**:
      [scene-services.md](contracts/scene-services.md), [active-context.md](contracts/active-context.md),
      [backend-registration.md](contracts/backend-registration.md),
      [compiler-tables.md](contracts/compiler-tables.md), [jit-dso-call.md](contracts/jit-dso-call.md),
      [tools.md](contracts/tools.md).
- [x] **No hider-specific special case**: none.
- [x] **Changed plug-in interfaces bump their SemVer**: no plug-in interface changes (`shadeop.h`
      stays 1.1.0). The `.slo` format gains MINOR 2.1 (written only by modules with a DSO call, Q1);
      `CRendererServices` stays 2.0.0 (Q8: a comment-only change gets no version bump; the maintainer's
      policy is MAJOR = breaking, MINOR = non-breaking addition, PATCH = bug fix only, and no MAJOR bump
      before spec 055); the `getDSO` wording change is recorded in `rendererServices.h`'s
      version-history comment. The `.slo` change is documented in `sloFormat.h`'s history.
- [x] **Registered violations not extended; reductions noted**: DB-015 resolved (10 lines deleted);
      DB-019 resolved (0 lines; its two remaining findings fixed); DB-024 narrowed; DB-004, DB-011
      and DB-017 lines unchanged (the forwarders stay, R4). Edges added under any entry: 0. Scene
      Interface metric 6/6 → 0/0; no metric goes up.

**Post-design re-check**: PASS. The contracts add no edge beyond the table. The table's owner is a
domain every consumer may depend on, and the only new Core knowledge is the core's own service
implementation.

## Project Structure

### Documentation (this feature)

```text
specs/038-scene-interface-untangle/
├── spec.md
├── plan.md                  # this file
├── research.md              # measured facts M1-M14, decisions R1-R16, questions Q1-Q10
├── data-model.md            # table, services, session, context, registration, record, compiler tables, DSO slot
├── quickstart.md            # validation steps (macOS and Linux)
├── contracts/
│   ├── scene-services.md        # CDeclarations, the two services, the standard session, the core's side
│   ├── active-context.md        # riInstallContext / riUninstallContext, call sites, rules
│   ├── backend-registration.md  # riHooks.h move, RiRegisterBackEnd, RiBeginBackEnd, the preview
│   ├── compiler-tables.md       # builtinTables.cpp, include path, include check, drift test
│   ├── jit-dso-call.md          # emitter, op_dso_call, binding, format 2.1, parity tests
│   ├── tools.md                 # precomp, data viewer, CDelayedData
│   ├── checks.md                # every new check and test, references, approved-test edits
│   └── allowlist-end-state.md   # deleted lines, edges, metrics, residual list, registry
├── checklists/requirements.md
├── tools/                   # (implementation) ribvector_core_sweep.sh, the nm/include sweeps
├── claude-md-draft.md       # (Phase 8) CLAUDE.md text for the maintainer to apply
├── validation.md            # (implementation) measurements and run records per phase
└── tasks.md                 # /speckit-tasks (not created here)
```

### Source Code (repository root)

```text
src/ri/state/
├── declarations.{h,cpp}     # NEW: CDeclarations (Graphics State)
└── (channelRegistry, variableLookup: unchanged; installed by CDeclarations)

src/ri/parse/                # Scene Interface
├── riServices.{h,cpp}       # NEW: services, null-safe calls, CRiStandardSession
├── riCondition.h            # NEW: riEvaluateCondition
├── delayedData.h            # NEW: CDelayedData (from geometry/delayed.h)
├── riHooks.h                # git mv from src/ri/render/; + RiRegisterBackEnd
├── rib.h                    # + RiBeginBackEnd / RiEndBackEnd; - RiBeginLite
├── rib.y, rib.l             # services; dead branch removed; no core includes
├── ifexpr.y                 # riEvaluateCondition(context, expr); no core includes
├── ri.cpp                   # installs; standard session; savedRenderMan gone; back-end sessions
├── riInterface.{h,cpp}      # no constructor side effect; install/uninstall
└── error.cpp                # comments only (attach/detach callers)

src/ri/output/ribOut.cpp     # includes parse/riHooks.h (no other change)

src/ri/geometry/delayed.h    # CDelayedData removed
src/ri/hiders/rasterFlags.h  # NEW: RASTER_* constants; reyes.h includes it

src/ri/render/               # renderer core
├── rendererDeclarations.cpp # CDeclarations + coordinate systems + global IDs + core services
├── rendererDeclarationsDispatch.cpp, ../vector/rendererDeclarationsDispatchStub.cpp  # notifyGlobalVariable
├── rendererFileSearch.cpp   # unchanged body; core file-search service forwards to it
├── rendererContext.{h,cpp}  # ifParse gone; archive install; riHooks include path
├── renderer.h, rendererStatics.cpp, rendererServicesImpl.h  # table statics gone / read CDeclarations
└── rendererProcedures.cpp   # includes parse/delayedData.h
src/ri/CMakeLists.txt        # ribOut / ribVector lists and comments; ifexpr in all three

src/libshader/compiler/
├── builtinTables.cpp        # NEW: the compiler's kOpcodeParamTable / kAllFunctionMnemonics
├── llvmEmitter.{h,cpp}      # tables gone; DSO lowering; shadeop slots + metadata; "DSO" handled
└── CMakeLists.txt           # no src/ri*, no src/libshader/shading include directory
src/libshader/shading/
├── rslOps.{h,cpp}           # op_dso_call
├── llvmJit.{h,cpp}          # slot list + table address per module; bindShadeops
├── sloLoader.cpp            # binds on every load; missing-operand failure
└── rendererServices.h       # getDSO comment and version-history comment only; stays 2.0.0 (Q8)
src/common/sloFormat.h       # MINOR 1 (Q1)

src/preview/libribpreview/   # previewContext.cpp registrar + light session; no core includes
src/ri/dataviewer/dataLoad.{h,cpp}   # standard session, installed sink
src/precomp/precomp.cpp      # includes hiders/rasterFlags.h only

tests/architecture/          # new cmake -P checks, ribvector-core-residual.txt, allowlist, metrics
tests/RIB/writer/, tests/precomp/, tests/preview/, tests/plugins/shadeop/  # new tests and references
src/libshader/tests/         # drift test; coverage and gate-hardening additions
DEVNOTES.md, DEVNOTES_DETAILS/{DOMAIN_BOUNDARIES,PLUGIN_ABI}.md, docs/site/.../dso-shading.md, NEWS.md, ChangeLog.md
```

**Structure Decision**: no new directory under `src/`. Each new file goes into the directory of the
domain that owns it, so `domain-map.txt` needs no line. The one moved header (`riHooks.h`) is moved,
not shimmed.

## Phase outline for `/speckit-tasks`

Every phase starts with its test tasks and a **TEST-APPROVAL** stop (new tests, edits of approved
tests and references are shown to the maintainer before any implementation task of the phase).
Every phase ends with this **checkpoint**, and reports before the next phase starts:

1. `~/.claude/bin/cpujob 038-pN-build cmake --build build --config Release -j4`
2. `~/.claude/bin/cpujob 038-pN-all ctest --test-dir build -LE determinism-sweep -j4 --output-on-failure`
3. `~/.claude/bin/cpujob 038-pN-arch ctest --test-dir build -L architecture --output-on-failure`
4. Linux validation on `juvenal@ubuntu-2404-desktop.v2-labs.io`, tree
   `~/Projects/Development/CLI/openrender-038` (GCC 13, `-Werror`): sync and verify, then build at
   `-j2`, `ctest -LE determinism-sweep -j2` and `ctest -L architecture` through the remote `cpujob`
   (quickstart.md §3).
5. A record in `validation.md`: the commands, the counts, what is still red or disabled and why.

The determinism sweep (`-L '^determinism-sweep$' -j2`) runs in Phase 9, before merge, on both
platforms. Phases 5-6 depend only on Phase 1 and may run before or between Phases 2-4. Phase 7's
`precomp` part depends only on Phase 1. The data-viewer part of US7 is in Phase 4 (it needs Phases 2-3).

Labels: the new `cmake -P` checks that are red on the starting tree carry the temporary ctest label
`architecture_038` and move to `architecture` in the phase that turns them green, so that every
checkpoint's `ctest -L architecture` passes; the checkpoint also runs `-L architecture_038` and
compares it with the expected red set. Phase 9 verifies that no `architecture_038` label is left.
The `.slo` tests (`ShadeopAbi_Jit_*`, the `.slo` half of `Shaders_BytesUnchanged`, the new libshader
JIT cases) are registered under `if(OPENRENDER_ENABLE_JIT)`.

### Phase 0 — Setup (precondition for the maintainer and the coordinator; no implementation)

- The maintainer commits the spec stage. The coordinator merges master `d6910bd0` into the branch
  (spec 026); 038's plan assumes it. No 038 task depends on the merge of 038 itself.
- Configure `build/` in the worktree. Re-measure on the merged tree and record in `validation.md` §0:
  the archcheck report (DB-015's 10 lines at `:298-307`, Scene metric 6/6, TOTAL 612/94, the CONSUMER
  count and what DB-024's "36" counts, M14), the `ribOut`/`ribVector` source lists, and the `nm`
  sweep of research M5.
- Create the Linux tree `openrender-038` (moved here from Phase 1, so that every phase, this one
  included, closes with a Linux run), configure it, build it and record the Linux baseline.
- Master's tip `01501f10` already contains three doc fixes, which arrive with the merge (the DB-015
  allowlist header "(11)" to "(10)", the sweep count 413 to 419 in `CLAUDE.md`, the DB-011 evidence
  611 to 558); no 038 task repeats them, and Phases 8 and 9 verify they arrived.

### Phase 1 — Tests first, references, Linux gate (no behaviour change)

- The mechanical checks of contracts/checks.md (red), the characterizations (`if` evaluator, writer
  text, preview extraction, `precomp` output, shader byte snapshot as a sha256 manifest per platform) with their capture target (green),
  the `DISABLED` API tests, and the `.slo` parity tests (red: compile exits 3), with their new
  fixture operands (void, string) and shaders (`probe_args`, `probe_void`, `probe_init`, Q9), all
  written here so that every new test exists before any implementation (the tasks stage fixed this;
  the Phase 6 text used to put the fixtures there).
- Research R10a (the opcode mnemonic relation) and R14a-c (pass behaviour on DSO lines, upper-case
  prototype letters, the uniform decision), measured and recorded in validation.md §1 (research.md keeps the design question). A defect found
  stops for the maintainer.
- Linux gate: build the Phase 1 tree on the Linux tree (created in Phase 0), run the red set and the
  characterizations there; capture `reference/linux/` only where a reference differs.
- **TEST-APPROVAL**. Checkpoint: everything green except the listed red checks and the disabled tests.

### Phase 2 — US1 + US2: services, declaration table, hooks and records (P1)

- `CDeclarations` (Graphics State); `riServices` (Scene) with the standard implementations; the
  core's `initDeclarations`/`shutdownDeclarations` on `CDeclarations` plus its own services;
  `notifyGlobalVariable` replacing `makeGlobalVariable`; the table statics out of `renderer.h`.
- `rib.y`, `rib.l`, `ri.cpp` on the services; the dead branch deleted; the writer branch's standard
  session; `ifexpr.y` → `riEvaluateCondition`; `CRendererContext::ifParse` deleted.
- `git mv` of `riHooks.h`; `delayedData.h`; the two `CDelayedData` test includes (approved at this
  phase's stop).
- `ribOut_sources` without Core files; `ifexpr_generated.cpp` in the three targets (Q6).
- Allowlist `:297-307` deleted as each line goes STALE; Scene metric lowered to 0/0 and TOTAL to
  archcheck's figure.
- Green: `SceneInterface_RibOutNoCore`, `NoCoreInclude`, `HooksAndRecords`, `Services` (enabled),
  the writer text and `if` characterizations, every RIB output test.

### Phase 3 — US3: explicit active context (P1)

- `riInstallContext`/`riUninstallContext`/`riPreviousContext`; the constructor and destructor;
  `RiBegin`'s three branches; the archive path; `savedRenderMan` and the `extern`s deleted.
- The four tests that assign the global, edited (approved at this phase's stop); the data viewer and
  the preview move here only as far as needed to compile (their sessions are Phase 4).
- Green: `SceneInterface_ContextInstall` (rule 1, red since Phase 1), `SceneInterface_ContextInstallRules`
  (enabled; its executable did not compile before the API existed), `SceneInterface_ActiveContextAssignments`.

### Phase 4 — US4 + FR-015/016/024: preview back-end, data viewer, `ribVector` (P2)

- `RiRegisterBackEnd`, `RiBeginBackEnd`/`RiEndBackEnd`; `RiBeginLite` deleted; the preview's
  registrar and session; the preview's core includes removed. `CDataDocument` on the standard session
  with an installed sink (US7-AS2; tagged [US7] in tasks.md).
- The `ribVector` `nm` sweep (`tools/ribvector_core_sweep.sh`, first run as the closing item of Phase 3 and
  re-run after the CMake change as the confirmation); `ribvector-core-residual.txt`;
  `rendererClipping.cpp` and `resource.cpp` out of `ribVector` (Q7); the CMake comments rewritten.
- Green: `Preview_SelfRegistration` (enabled), `Preview_NoCoreReach`, `Preview_ExtractionUnchanged`,
  `SceneInterface_RibVectorResidual`, the preview suite.

### Phase 5 — US5: standalone compiler (P2)

- `builtinTables.cpp` generated by the drift test's `--emit`; the emitter's two table blocks and
  engine includes deleted; the compiler's include directories pruned.
- Green: `Compiler_IncludeBoundary`, `LibShader_CompilerTableDrift` (enabled, with its mutation
  check), `LibShader_UsedParametersTable`, `LibShader_OpcodeCoverage`, `Shaders_BytesUnchanged`.

### Phase 6 — US6: JIT DSO call path (P2)

- The emitter's DSO decoding and lowering (dispatched before the destination prelude, contracts/jit-dso-call.md), shadeop slots, metadata and the 2.1.0 record (Q1, with `SLO_FORMAT_MINOR_BASE` and the `LibShader_Format_slo_emitted` edit);
  `op_dso_call`; `bindShadeops` and the binding in `parseSloShader` with the missing-operand rule
  (Q2); `"DSO"` in `kHandledOpcodes`; `sloFormat.h` MINOR 1; the services comments (Q8, no version bump); the stale header comment of the approved `render.rib.in`.
- The edits of the three approved libshader tests (`test_format_versions.cpp`, `test_opcode_coverage.cpp`,
  `test_gate_hardening.cpp`) and the stale header comment of the approved
  `tests/plugins/shadeop/render.rib.in` ("the JIT has no DSO call path"; comment lines only), approved
  at this phase's stop; the fixtures and parity tests were
  approved in Phase 1.
- Green: every `ShadeopAbi_Jit_*` test, the coverage and gate-hardening additions,
  `Shaders_BytesUnchanged` (shipped `.slo` still 2.0.0), the `-slo` visual variants.

### Phase 7 — US7: tools (P3)

- `hiders/rasterFlags.h`, `reyes.h` including it, `precomp.cpp` on it (Q4).
- Green: `Precomp_NoHiderCore`, `Precomp_OutputUnchanged`. The CONSUMER report re-measured.

### Phase 8 — Registry, allowlist, documentation (FR-025-FR-028)

- `DOMAIN_BOUNDARIES.md`: DB-015 and DB-019 to Resolved ("Spec 038", the implementation date),
  DB-024 narrowed, the DB-004/DB-011/DB-017 notes (contracts/allowlist-end-state.md); `DEVNOTES.md`
  status; `PLUGIN_ABI.md`; site `dso-shading.md`; `NEWS.md`/`ChangeLog.md` (the `libRIBVector`
  symbol removal of Q7, the `RiLastError` difference of Q5); the `metrics-baseline.txt` comment; `claude-md-draft.md` (Architecture,
  gotchas for `riInstallContext` and `CDeclarations`, Recent Changes).

### Phase 9 — Validation

- macOS: the full run (`-LE determinism-sweep -j4`), `-L architecture`, the determinism sweep at
  `-j2`, `0 Not Run (Disabled)`.
- Linux: the same three at `-j2` in `openrender-038`.
- `validation.md`: every SC with its measured value (SC-001 ... SC-012), the before/after metric and
  allowlist counts, the CONSUMER count.

## Maintainer decisions (research.md section Q; approved 2026-11-02, recorded in research.md section D)

Every row below is decided as recommended. spec.md changed where a requirement or a success
criterion stated the opposite (spec.md, Clarifications, Session 2026-11-02): FR-004, FR-005, FR-015,
FR-018, FR-019, FR-021, FR-022, FR-023, FR-028, FR-029, US5-AS1, US6-AS3, US6-AS4, US7-AS1, SC-004,
SC-007 to SC-011.

| # | Decision | Approved | Blocks |
|---|---|---|---|
| Q1 | `.slo` format record: 2.1.0 only in modules with a DSO call (a second constant `SLO_FORMAT_MINOR_BASE`, an edit of the approved `LibShader_Format_slo_emitted`, TEST-APPROVAL); every other shipped `.slo` keeps its bytes | approved | Phase 6 |
| Q2 | missing operand at `.slo` load: warn and return `nullptr`; the loader falls back to the `.rslo` | approved | Phase 6 |
| Q3 | drift test registered DISABLED until `builtinTables.cpp` exists; its red evidence is the mutation check; FR-029/SC-011 amended | approved | Phase 1 |
| Q4 | `precomp` constants in `hiders/rasterFlags.h`; criterion: output after equals the unmodified tool's; SC-009/US7-1/FR-023 reworded | approved | Phase 1, 7 |
| Q5 | construction-time reports no longer set `RiLastError` (text, stream, severity unchanged): accepted behavior change, NEWS line | approved | Phase 3 |
| Q6 | `ifexpr_generated.cpp` in all three targets | approved | Phase 2 |
| Q7 | `rendererClipping.cpp`, `resource.cpp` out of `ribVector` (exported symbols of an installed library), NEWS line | approved | Phase 4 |
| Q8 | `CRendererServices` stays 2.0.0: the `getDSO` wording change goes only into the version-history comment of `rendererServices.h` (changed by the maintainer from the first approval, a PATCH bump) | approved (changed) | Phase 6 |
| Q9 | an init-section DSO case in FR-022's matrix | approved | Phase 1, 6 |
| Q10 | the compiler's allowed include roots (US5-AS1, SC-007) gain `src/include`, `src/libshader/include` and the build directories | approved | Phase 1 |
| D4-a | the standard file search matches the writer's lookups only in a process with no earlier renderer session; after one, a bare name that exists only in the working directory is now found (core defect GitHub #136, `temporaryPath` never cleared at `RiEnd`, not fixed by 038): accepted behavior change, a `SceneInterface_Services` case, NEWS line | approved | Phase 1, 2, 8 |

## Risks

| Risk | Mitigation |
|---|---|
| A DSO line's IR form (operand name as the result, destination as a read) lets a pass merge, drop or reorder a DSO call in `.rslo` today | Measured first (R14a) on the `.rslo` path. The JIT consumes the same instruction stream, so parity holds either way. A defect is reported, not silently fixed. |
| The interpreter advances a uniform operand per vertex if a shadeop prototype can carry an upper-case letter | Measured first (R14b); the measured promotion (M11) covers lower-case prototypes. |
| Installing the context after construction changes where construction-time reports go | Only `RiLastError` differs; same text, stream, severity (Q5). Measured list of reachable reports in Phase 3. |
| The reference-counted table changes a sequence that relied on re-initialisation | Only the two sequences of M3 that use freed memory today behave differently; the five real sequences are characterised by tests. |
| The static `libribpreview` drops the registrar | The registrar is in `previewContext.cpp`, the translation unit of `ribpreview_load`; `Preview_SelfRegistration` links the static library. |
| The module cache outlives the operands | Binding at every `parseSloShader`, which runs once per shader per session (`globalFiles` cache), before any instance exists, so no render thread sees a rebinding. |
| GCC 13 `-Werror` rejects code Clang accepts | The Linux build at every checkpoint. |
| Overlap with in-flight spec 029 (`029-hider-shading-separation`, at `d6910bd0`): Hiders/Shading files, possibly `rendererDeclarationsDispatch.cpp` (the shading-context loop) and `rslOps` | 038 keeps those edits small and self-contained; the second to land rebases; no task reads a line only 029 changes. |
| Removing two files from `libRIBVector` removes exported symbols | Q7 (approved); NEWS line. |
| Construction-time reports no longer set `RiLastError` | Q5 (approved); NEWS line; the reachable reports are listed in Phase 3. |
| After a renderer session a writer, preview or data-viewer lookup now finds a bare name that exists only in the working directory | D4-a (approved); core defect GitHub #136 (`temporaryPath` never cleared at `RiEnd`), not fixed here; a `SceneInterface_Services` case; NEWS line. |

## Complexity Tracking

No constitution violation needs justification. Two additions are larger than the minimum and are
recorded for review:

| Addition | Why needed | Simpler alternative rejected because |
|---|---|---|
| A Scene-owned service layer over a Graphics State table (two classes and a session object) | FR-001/FR-002 require Scene-owned services installed per session; the renderer's network-aware file search differs from the writer's | Calling Graphics State directly from the grammar satisfies neither FR-001 nor the network rule |
| Per-module DSO slot table and metadata in the `.slo` | FR-021: bind at load through the same lookup, fail the load on a missing operand | Lazy resolution at the first call runs `getDSO` on render threads and cannot fail the load |
