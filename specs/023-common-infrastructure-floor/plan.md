# Implementation Plan: Common Infrastructure Floor

**Branch**: `023-common-infrastructure-floor` | **Date**: 2026-10-08 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/023-common-infrastructure-floor/spec.md`

## Summary

Move generic infrastructure out of Scene Interface and the renderer core into `src/common`, which
becomes a **shared** `libRIBCommon` and the single owner of process-wide state. The static
`libRIBCommon.a` stays for fully static programs. What moves:

| Service | Change |
|---|---|
| Diagnostics | An installable sink in common, with a default that follows `RiErrorPrint`'s rules. Scene Interface installs an RI sink while a context exists |
| Vocabulary | `rendererc.h` moves to common. A new `riVocabulary.h` declares the RI types and tokens; the token definitions move to `riTokens.cpp`. The public `ri.h` is unchanged |
| Variable declarations | `CVariable`/`parseVariable` and the flex/bison grammar move to common |
| Scratch arena | `CArena globalArena` replaces `CRenderer::globalMemory` |
| Generic `src/ri/core` code | `refCounter`, `fileResource`, `memory`, `stats`, `random`, `atomic.h` move to common |

`pl` (primitive parameter lists) turns out to be Graphics State code. It moves to `src/ri/state/`,
and a Graphics State-owned lookup hook inverts its `CRenderer::retrieveVariable` call.

**Packaging and compatibility**:
- The three RI libraries re-export `libRIBCommon` on macOS and depend on it via `DT_NEEDED` on
  Linux, so existing binaries keep running.
- `ribRender_static` consumes the shared common library, so `libshader_shading` and a static
  consumer share one copy of the state.

**Results**:
- All 215 DB-016 allowlist lines and at least 12 stale DB-001 lines are deleted, and no line is added.
- DB-016 moves to Resolved.
- `libshader_shading` resolves its 17 infrastructure symbols from `libRIBCommon`.

Every decision is recorded in [research.md](research.md); R0 maps each DB-016 line to its fix.

## Technical Context

**Language/Version**: C++20 (`-Werror`), CMake ≥ 3.19, flex + bison (already mandatory).

**Primary Dependencies**: none new. Platform tools used by the tests:
- `nm`, `otool`/`dyld_info` (macOS);
- `readelf`/`LD_DEBUG` (Linux).

**Storage**: N/A. Edits to the version-controlled `tests/architecture/` data files and the registry.

**Testing**: ctest, TDD-first (research R10):
- a new label `common`: golden variable table, vocabulary consistency, state ownership,
  no-context diagnostics, cross-library sink (shared and static-package), arena semantics, shading
  binding;
- `-L architecture`;
- the full suite.

SC-006 and SC-007 are quickstart procedures that use baselines captured before any source change
(research R11, R12).

**Target Platform**: macOS (Apple Silicon, deployment target 13.3; vcpkg or Homebrew) and Ubuntu
24.04 (GCC 13, `-Werror`). Validation on Linux is run manually by the user, in `build-023`.

**Project Type**: C++ libraries plus CLI tools (renderer, shader compiler, texture tools) with a
CMake package.

**Performance Goals**: median render time ≤ +2% on the six perf-manual scenes, both `.rslo` and
`.slo` variants, measured interleaved A/B (FR-016, R12).

**Constraints**:
- byte-identical deterministic renders;
- no forwarding headers or aliases (FR-009);
- `ri.h` byte-identical;
- one definer per process-wide symbol (FR-008);
- binary compatibility for existing binaries (FR-014);
- builds and tests at most `nice -n 19 … -j4`; performance runs serial.

**Scale/Scope**:
- 215 DB-016 edges;
- about 135 `globalMemory` uses in 23 files;
- 45 `error.h` and 11 `ri.h` includers outside their domains;
- 87 token link references;
- ~13 moved source files;
- ~50 CMake link sites, which switch to the shared library through the unchanged target name.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Assessment | Status |
|---|---|---|
| I. Clean Code | Moves keep files whole (`git mv`, history preserved). New units are small: the sink, `CArena`, the lookup hook. No magic numbers: `ZONE_*`/`VARIABLE_NAME_LENGTH` move next to their only users | PASS |
| II. Language Standards | C++20 under `-Werror` on Clang and GCC 13. `-reexport_library` is a documented ld64 option, not a language extension. Identical typedef/macro redefinition between `ri.h` and `riVocabulary.h` is standard C++/C11 | PASS |
| III. TDD (non-negotiable) | Every `Common_*` test is written first: it fails, or for the golden/consistency tables passes on the old code, before its change lands (R10). Each story's tests-first block ends at a test-approval pause in tasks.md | PASS |
| IV. CLI | No CLI change. Tool output (stderr/stdout streams and exit status) is preserved, and is now also defined with no context | PASS |
| V. Minimal Dependencies | No new dependency. flex/bison are already mandatory; the platform binutils are already used by spec 022 | PASS |
| VI. Platform Targeting | macOS and Linux each get an explicit mechanism: re-export on macOS, `DT_NEEDED` and global scope on Linux. Each has its own checks in quickstart §3/§5; Linux is validated by the user | PASS |
| VII. Documentation and Site Management | Documentation lands in `DOMAIN_BOUNDARIES.md`, `CLAUDE.md` (gotcha), `DEVNOTES.md` and `INSTALL.md`/`COMPILING.txt` (R14). The `site/` folder that VII names still doesn't exist; that gap predates this spec and is unchanged | PASS (pre-existing gap noted) |
| VIII. Domain Separation | See the gate below | PASS |

**Principle VIII — Domain Separation and Modular Boundaries** (every plan):

- [x] Domains touched are named:
  - **Infra** (`src/common`, `src/includes/logging.hpp`, `src/ri/core/ri_config.h`): receives the
    moved code.
  - **Scene Interface** (`src/ri/parse/`): keeps `translate()`, the RI sink, `RiErrorPrint` and
    `ri.h`.
  - **Graphics State** (`src/ri/state/`): receives `pl`; owns the new lookup hook.
  - **Renderer core** (`src/ri/render/`): installs the hook; loses `globalMemory`.
  - **Geometry, Hiders, Shading, Texturing**: call sites switch to common headers.
  - Consumers (tools, tests, preview, bindings): linking changes only.
- [x] No new cross-domain dependency outside the table, and none on the renderer core.
  - Added edges are Domain → Infra (allowed) and Core → Graphics State (`installVariableLookup`,
    allowed).
  - `pl` moving into Graphics State turns its `attributes.h`/`xform.h`/`findParameter` edges into
    intra-domain edges; its includers are Geometry, Core and consumers, all allowed to reach
    Graphics State.
- [x] Every added or altered cross-domain interface is specified:
  - the Graphics State lookup hook and the removal of `CRendererServices::globalMemory()`:
    [contracts/variable-lookup.md](contracts/variable-lookup.md);
  - the common API: [contracts/common-api.md](contracts/common-api.md);
  - library/ABI: [contracts/library-abi.md](contracts/library-abi.md).
- [x] No hider-specific special case.
- [x] No plug-in interface changes. `shadeop.h`, `dlo.h`, `implicit.h`, `dsply.h`, `ptcapi.h` and
      `ri.h` are untouched, so there is no Semantic Version bump.
- [x] Registered violations are not extended:
  - DB-016's 215 lines are deleted, and DB-016 moves to **Resolved**.
  - DB-001 shrinks by at least 12 lines (30 → ≤ 18).
  - The allowlist only shrinks (686 → ≤ 459). The metric baseline is lowered: Infra 10 → 0, and
    every domain loses its `globalMemory` references.

**Post-design re-check (after Phase 1)**: all PASS.
- The design adds no allowlist line. The only new cross-domain interface is the Core → Graphics
  State hook, which is allowed and specified in contracts.
- The packaging change (shared `libRIBCommon`) is an Infra concern with no domain dependency.
- R0 shows every DB-016 line has a resolving decision, so FR-010 can't fail for lack of a design.

## Project Structure

### Documentation (this feature)

```text
specs/023-common-infrastructure-floor/
├── spec.md
├── plan.md               # this file
├── research.md           # R0–R14 decisions
├── data-model.md         # Diagnostic, sink, vocabulary, CVariable, CArena, lookup hook, packaging
├── quickstart.md         # validation procedures (baselines, checks, compat, perf, Linux)
├── contracts/
│   ├── common-api.md      # common headers: diagnostics, memory/arena, variable, vocabulary, log level
│   ├── variable-lookup.md # Graphics State hook + CRendererServices::globalMemory() removal
│   └── library-abi.md     # library set, single-definer invariant, binding, binary compat, package
├── tools/                # created in implement: compat-consumer/, perf_ab.sh
├── checklists/requirements.md
└── tasks.md              # /speckit-tasks
```

### Source Code (repository root)

```text
src/common/                        # Infra → shared libRIBCommon + static libRIBCommon.a
├── CMakeLists.txt                 # OBJECT lib → SHARED openrendercommon + STATIC openrendercommon_static;
│                                  #   flex_file/bison_file for variable.{l,y}
├── diagnostics.{h,cpp}            # NEW: EErrorCode, sink slot, default rules, error/warning/fatal/info
├── logging.cpp                    # NEW: the single definition of current_log_level
├── memory.{h,cpp}                 # MOVED from src/ri/core (+ CArena, globalArena, ZONE_*)
├── refCounter.{h,cpp} fileResource.{h,cpp} stats.{h,cpp} random.{h,cpp} atomic.h   # MOVED
├── variable.{h,y,l}               # MOVED from src/ri/parse (+ VARIABLE_NAME_LENGTH)
├── rendererc.h                    # MOVED from src/ri/render
├── riVocabulary.h                 # NEW: Rt* types, RI_* declarations, bases/filters
├── riTokens.cpp                   # NEW: RI_* definitions (from ri.cpp)
└── rslConstants.cpp colorSpace.h  # switch to riVocabulary.h / common/rendererc.h

src/includes/logging.hpp           # inline current_log_level → extern
src/ri/core/ri_config.h            # loses ZONE_*, VARIABLE_NAME_LENGTH
src/ri/parse/
├── error.{h,cpp}                  # error.h REMOVED; error.cpp keeps translate() + CRiDiagnosticSink
├── ri.cpp                         # tokens removed; RiErrorPrint → diagnosticsPrintDefault; RiEnd restores the sink
├── riInterface.cpp                # ctor installs the RI sink
└── ri.h                           # UNCHANGED (public)
src/ri/state/
├── pl.{h,cpp}                     # MOVED from src/ri/core
└── variableLookup.{h,cpp}         # NEW Graphics State hook
src/ri/render/                     # renderer.h/rendererStatics.cpp drop globalMemory; rendererDeclarations.cpp
                                   #   installs the lookup; rendererServicesImpl.h drops globalMemory();
                                   #   init.cpp drops the threadMemory alias
src/ri/CMakeLists.txt              # sources/whitelists shrink; re-export option on RIBRender/Vector/Out;
                                   #   ribRender_static → shared common; installs/exports
src/libshader/{shading,compiler,runtime}/CMakeLists.txt   # link the shared library instead of embedding it
src/libshader/shading/rendererServices.h                  # globalMemory() removed
src/preview/orender-wire-macos/…   # bundle libRIBCommon into Contents/Frameworks
CMakeLists.txt, CMake/openRenderConfig.cmake.in           # export sets; Static component → shared RIBCommon
(≈60 call-site files across geometry/, hiders/, state/, texture/, libshader/, parse/, render/)

tests/
├── unit/common/                   # NEW: test_common_{variables,diagnostics,arena,crosslib,crosslib_static}
│   └── CMakeLists.txt             #   + cmake -P runners: state ownership, shading binding, vocabulary
└── architecture/                  # allowlist (at least 227 lines removed), domain-map (+@build/src/common), metrics-baseline
```

**Structure Decision**: the existing tree is kept. Infrastructure lands in `src/common`, which the
domain map already classifies as Infra. New tests go in a new `tests/unit/common/` directory with
the ctest label `common`.

## Implementation sequence (input for /speckit-tasks)

Each step keeps the full suite green and is a review/commit pause point.

1. **Setup (before any source edit)**: R11 baselines.
   - the frozen DESTDIR install and the SC-006 consumer binary;
   - Linux "before" commands handed to the user;
   - the golden variable table written and passing on the old code.
2. **Foundational: packaging only** (no symbol moves):
   - shared `libRIBCommon` from the OBJECT library, linked with no undefined references (R1); the
     static archive renamed as a target;
   - the re-export spike (R1);
   - no shared image embeds the archive;
   - `ribRender_static` → shared common; display plug-in rpaths; the bundle, install and export
     sets;
   - `Common_StateOwnership` written first, with the Linux copy-relocation rule (R2). It fails on
     state symbols until the moves land and passes for the already-common utilities.
3. **US3: diagnostics core first** (tests first: `DiagnosticsNoContext_*`,
   `DiagnosticsCrossLibrary{,Static}`, including the attach/detach restore case).
   - This must precede step 4: `stats.cpp` and `fileResource.cpp` call `info()`/`error()`, and
     `libRIBCommon` may not reference upward.
   - Work:
     - `common/diagnostics`;
     - `common/logging.cpp` (the single `current_log_level`);
     - the idempotent RI sink attach/detach;
     - `RiErrorPrint` → `diagnosticsPrintDefault`;
     - switch the 45+ `error.h` includers; remove `parse/error.h`.
4. **US1/US2: generic core and arena** (tests first: `Common_ArenaSemantics`,
   `Common_ShadingInfraBinding`):
   - move `refCounter`/`fileResource`/`memory`/`stats`/`random`/`atomic.h`;
   - split `ri_config.h`;
   - add `CArena`/`globalArena`; rewrite every `globalMemory` use; remove the static, the services
     accessor and the `threadMemory` alias.
5. **US1: vocabulary and variables** (test first: `Common_VocabularyConsistency`):
   - `rendererc.h` → common; `riVocabulary.h` + `riTokens.cpp`;
   - switch the includers;
   - move the variable grammar (golden table stays green); update the domain map.
6. **US1: `pl` → Graphics State** with the lookup hook (contracts/variable-lookup.md).
7. **US4 + allowlist**:
   - prune the allowlist (all DB-016 lines and the stale DB-001 lines);
   - lower the metric;
   - run the quickstart §3 and §5 checks on macOS.
8. **Polish**: registry (R14), docs, the performance A/B (R12), Linux validation package for the
   user, full suite.

## Complexity Tracking

No constitution violations to justify.

One deliberate duplication is recorded: `ri.h` and `riVocabulary.h` both declare the RI tokens.
- **Why**: FR-004/FR-009 keep the public header byte-identical.
- **Guard**: `Common_VocabularyConsistency`.
- **Alternative rejected**: making `ri.h` include an internal header changes the installed header
  set (R4).
