# Contract: the checks, the characterization references and the approvals (FR-029, SC-011)

Every row is new or an edit of an approved test, so every row is a TEST-APPROVAL item: the phase
that adds it stops after its red evidence (or, for a characterization, its green evidence on the
unmodified tree) for the maintainer's approval, before any implementation task of that phase.
"Red" means it fails on master `d6910bd0` merged into the branch; a check that cannot fail does not
count (SC-011), except as stated for the drift test (Q3).

## Mechanical checks (`cmake -P`)

Each check that is red on the starting tree carries the temporary ctest label `or038_red` (and `preview` for `Preview_NoCoreReach`), so that `ctest -L architecture` passes at every checkpoint. The task that turns a check green changes its label to `architecture`: `SceneInterface_RibOutNoCore`, `_NoCoreInclude` and `_HooksAndRecords` in Phase 2, `_ActiveContextAssignments` in Phase 3, `_RibVectorResidual` and `Preview_NoCoreReach` in Phase 4, `Compiler_IncludeBoundary` in Phase 5, `Precomp_NoHiderCore` in Phase 7. Each checkpoint also runs `-L or038_red` and compares the result with the expected red set; Phase 9 verifies no test carries the label.

| Check | Fails when | On the starting tree |
|---|---|---|
| `SceneInterface_RibOutNoCore` | a `ribOut_obj` source (`$<TARGET_PROPERTY:ribOut_obj,SOURCES>`) is classified Core by `domain-map.txt` | red (6 files) |
| `SceneInterface_RibVectorResidual` | `ribVector_obj` compiles a Core file not in `tests/architecture/ribvector-core-residual.txt`, or the file lists one it no longer compiles (stale), or a "needed by" cell names a file under `parse/`, `output/` or `preview/` (the residual files exist only for Geometry and Hiders) | red (`rendererClipping.cpp`, `resource.cpp` not listed, Q7) |
| `SceneInterface_NoCoreInclude` | a source in `src/ri/parse/`, `src/ri/output/` or a generated parser (`rib_generated.cpp`, `ifexpr_generated.cpp`, `lex.*.cpp`) includes a file classified Core (resolved through the target's include path) | red (`rib.y`, `ifexpr.y`, `ri.cpp`, `ribOut.cpp`) |
| `SceneInterface_HooksAndRecords` | `riHooks.h` is not under `src/ri/parse/`; `CDelayedData` is defined outside the Scene Interface; a Scene source includes `geometry/delayed.h` | red |
| `SceneInterface_ActiveContextAssignments` | a source under `src/` or `tests/` outside `src/ri/parse/` (the check's own script excluded; `specs/` and the documentation are not scanned) matches `\brenderMan\s*=[^=]`, or one names `savedRenderMan` | red (M6) |
| `Preview_NoCoreReach` (label `preview`) | a `libribpreview` source includes a Core header, or an object has an undefined `CRenderer`/`CRendererContext` symbol (`nm`) | red |
| `Compiler_IncludeBoundary` | contracts/compiler-tables.md | red |
| `Precomp_NoHiderCore` | contracts/tools.md | red |

## In-process tests

| Test (label) | What | On the starting tree |
|---|---|---|
| `SceneInterface_ContextInstall` (`common`) | rule 1 of contracts/active-context.md (construct, do not install: the active context and the sink are unchanged); links `ribOut`; uses only today's API | red at run time (the constructor assigns) |
| `SceneInterface_ContextInstallRules` (`common`) | rules 2 to 6 (install, uninstall, delete, nest, attach twice); needs `riInstallContext`/`riUninstallContext`/`riPreviousContext` | `DISABLED`; its `EXCLUDE_FROM_ALL` executable fails to compile on the starting tree (red evidence, decision A) |
| `SceneInterface_Services` (`common`) | no service: `riRetrieveVariable` → `nullptr`, `riLocateFile` only finds a separator path; standard session and renderer session: the declared set equals the **declared-set reference** below (so both are pinned to the unmodified `CRenderer::initDeclarations()`, not to each other); the file search finds the same files with the same `info` lines as `CRenderer::locateFile` outside a network session, **in a process with no earlier renderer session** (these comparisons run before any renderer-session case of the executable); **after a renderer session in the same process** the standard search finds a bare name that exists only in the working directory (D4-a, accepted change; core defect GitHub #136; the test asserts only the standard search, never the core's `temporaryPath` behavior, so a later fix of #136 does not break it); a lexer archive lookup (`ReadArchive`) with **no active context** (`renderMan == NULL`) neither crashes nor finds a file by search path; `riEvaluateCondition` on a context whose `getAttributes`/`getOptions` answer `nullptr` reports "not found" without crashing; nested sessions restore the outer services | `DISABLED`; its `EXCLUDE_FROM_ALL` executable fails to compile on the starting tree (the API does not exist): that compile failure is its red evidence (decision A, precedent `tests/unit/state/CMakeLists.txt:92`) |
| `SceneInterface_IfCharacterization` (`regression`) | a RIB with `IfBegin`/`ElseIf`/`Else`/`IfEnd` over user attributes, options, comparison and string operators, nested; rendered through a capture context; the branch taken per block recorded and compared with a reference | green by design (M8); reference captured once |
| `Preview_SelfRegistration` (`preview`) | contracts/backend-registration.md, including the FR-014 case: a preview-only program's `RiBegin("#x")` prints the unchanged "Full-render mode is not available in this build" text and `RiBegin("file.rib")` keeps writing the file (`libRIBVector` registers the writer) | `DISABLED`; its `EXCLUDE_FROM_ALL` executable fails to compile on the starting tree (red evidence, decision A) |
| `LibShader_CompilerTableDrift` (`libshader`) | contracts/compiler-tables.md, mutation self-check | `DISABLED` until `builtinTables.cpp` exists (Q3) |
| `ShadeopAbi_Jit_*` (`plugin_abi`; registered under `if(OPENRENDER_ENABLE_JIT)`) | the seven rows of contracts/jit-dso-call.md plus the `LibShader_GateHardening` positive case (an approved-test edit, below); the 8-thread and render cases use a new RIB template `tests/plugins/shadeop/render_format.rib.in` with the shader format as a parameter (the approved `render.rib.in` hard-codes `"rslo"`; its scene is not edited, only its stale header comment, in Phase 6) | red (the `.slo` compile exits 3) |

## Characterization references (captured once, from the unmodified tree)

Captured by a build target `scene-interface-capture-baselines` (not a ctest), which writes a missing
reference and refuses to overwrite an existing one unless configured with `-DOR038_RECAPTURE=ON`
(037's pattern). The maintainer commits them with the phase that adds them.

| Reference | Compared by | Covers |
|---|---|---|
| the declared set after the unmodified `CRenderer::initDeclarations()`: every variable's name, type, class, storage, `entry` and usage marker, in list order, and the global count (written by a small capture program linking `ribRender`) | `SceneInterface_Services` | FR-002 "declared set, global numbering ... exactly as today" |
| `tests/RIB/writer/*.rib` → written text | `SceneInterface_WriterText` (`regression`) | US1-1: declared and inline parameters, `ReadArchive` of a relative name (search path), `IfBegin`/`Else`, each Procedural kind, written through `RiBegin(<file>)` |
| the `if` branch log | `SceneInterface_IfCharacterization` | US2-2 |
| `ribpreview_load` extraction per preview scene (vertices, colors, camera, bounds, as text) | `Preview_ExtractionUnchanged` (`preview`) | SC-006 |
| `precomp` raw output | `Precomp_OutputUnchanged` | SC-009 (Q4) |
| a sha256 manifest per platform (`reference/<platform>/shaders.sha256`) of the `.rslo` and `.slo` of every shipped shader and of `probe.sl`/`probe_two.sl` (`.rslo` only), and the `oshader --help` text | `Shaders_BytesUnchanged` (`libshader`) | FR-019 (with Q1(a), the `.slo` of shipped shaders keep their 2.0.0 record) |

## Edits of approved tests

| File | Edit | Why |
|---|---|---|
| `tests/unit/blobby/blobbyTestUtils.h:53` | `riInstallContext(iface)` once (a function-local install-once guard: installing an installed context is a `CODE_BUG`) instead of assigning | the constructor no longer attaches diagnostics |
| `tests/unit/texture_tile/test_texture_system_stream_cursor.cpp:110-111,228-229` | install / delete | FR-010 |
| `tests/unit/texture_tile/test_texture_system_null_resolver.cpp:114-115,164-165` (and its comment `:14`) | install / delete | FR-010 |
| `tests/preview/test_data_keys.cpp:50-57` | install / delete; the stale comment at `:48` (`RiBeginLite`, the constructor's global) rewritten | FR-010 |
| `tests/unit/csg/test_procedural_operand.cpp` (`:32` `#include "delayed.h"` replaced) and `tests/unit/sampling/test_object_ids.cpp` (no `delayed.h` include of its own: it gets `CDelayedData` through `scene_probe.h:40`, so a new `#include "delayedData.h"` is added next to `:26`) | `delayedData.h` | FR-008 |
| `src/libshader/tests/test_opcode_coverage.cpp` | explicit `DSO` handled check | FR-020 |
| `tests/plugins/shadeop/render_1f.rib.in:4`, `tests/plugins/shadeop/scan.rib.in:4`, `tests/plugins/shadeop/check_versions_render.cmake:5` (comment lines only) | the same stale sentence "(the JIT has no DSO call path)" replaced by the same wording as in `render.rib.in` | decision B (Phase 6) |
| `tests/plugins/shadeop/render.rib.in` (header comment, comment lines only) | the sentence "The shader is the interpreter bytecode (the JIT has no DSO call path)" is replaced by one saying the shader format is `.rslo` here and `render_format.rib.in` is the same scene with the format as a parameter; the scene lines are untouched | the comment is stale once the JIT has a DSO call path (decision B; Phase 6) |
| `src/libshader/tests/test_gate_hardening.cpp` | positive DSO compile case (builds a module with a `DSO` instruction, checks `op_dso_call` in the `.slo`; the synthetic unhandled mnemonic stays) | FR-020 |
| `src/libshader/tests/test_format_versions.cpp:276-300` (`LibShader_Format_slo_emitted`) | expect `SLO_FORMAT_MINOR_BASE` for its probe; a new case expects 2.1.0 from a probe with a DSO call | Q1(a) |
| (note) `test_opcode_coverage.cpp` | the row above is the only edit: the guard has no reverse check, so adding `"DSO"` to `kHandledOpcodes` trips nothing | FR-020 |
| the five `riHooks.h` includers | none expected (bare-name include); a CMake include-path line if one does not see `src/ri/parse` | FR-007 |

## Existing suites that must stay green, unchanged

visual (including every `-slo` variant), shading parity, state, common, texture, plug-in ABI,
preview, RIB output (`RibOut_ShaderStatementOrder`, the sampling-option round trip,
`Blobby_RibOutRoundTrip`), libshader, architecture, the determinism set and, before merge, the
determinism sweep. No threshold widened, no test disabled; at the end `0 Not Run (Disabled)`.
