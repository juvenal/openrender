# Feature Specification: Scene Interface Untangle

**Feature Branch**: `038-scene-interface-untangle`
**Created**: 2026-11-01
**Status**: Draft
**Input**: Constitution Principle VIII remediation, GitHub issue #43 (registry DB-015, DB-019, DB-024). The Scene Interface (RI entry points, the RIB grammar, the RIB writer) stops depending on the renderer core: the parser reaches variable declarations and file search through a Scene-owned service, `riHooks.h` and `CDelayedData` become Scene Interface files, the active RI context is installed explicitly instead of as a side effect of a constructor, and the preview back-end plugs in by self-registration like the renderer and the RIB writer. Around that, the RSL compiler becomes standalone (its own builtin and opcode tables, no shading-engine header), the LLVM JIT gains a call path for DSO shadeops so a `.slo` shader can call an operand, and the tools stop reaching into the internals this spec closes.

## Clarifications

### Session 2026-11-01

- Q: Is the standalone compiler in 038 or in 043? -> A: 038 (option A). It owns the compiler's own builtin, opcode and parameter-bit tables, the include path (no `src/ri/` directory and no `src/libshader/shading` directory) and the drift test (FR-017, FR-018). DB-019 moves to Resolved with spec 038. Spec 043 keeps only the public compiler library and C API (decision D25). The registry's Direction text for DB-019 (which put the standalone compiler in 043) is corrected by this spec's registry edit (FR-028). (maintainer)
- Q: How far does "ribVector compiles no renderer-core file" go? -> A: Option A. `ribOut` compiles zero Core files; `ribVector` drops every Core file the Scene Interface or the preview need. The residual Core files, which exist only because Geometry and Hiders link against `CRenderer` statics, are listed in the registry against DB-004 and DB-011 and pinned by a shrink-only check; specs 029 to 031 remove them (FR-015). (maintainer)
- Q: What does "DB-024 (the remainder)" close? -> A: Option A, narrow it. 038 retires the evidence it owns: `precomp`, and the preview's and the data viewer's declaration and context use (FR-023, FR-024). DB-024 stays Open for `sloinfo` and `oshader` (specs 033 and 043) and for the data viewer's Hiders and Texturing reach (spec 032). The registry discrepancies found at the specify stage (the `RiMake*` line, `riInterface.cpp:37` where the registry says :36, `scriptOpcodes.h` where it says `opcodes.h`, and the DB-015 allowlist header count of 11 for 10 lines) are corrected by the registry edit this spec plans (FR-028), not at the specify stage. (maintainer)

### Session 2026-11-02 (the plan stage's ten open decisions; the maintainer approved every recommendation)

- Q1 (`.slo` format record): a module that calls a DSO operand records format 2.1.0; every other module keeps the 2.0.0 record and its bytes (`SLO_FORMAT_MINOR_BASE`). This edits the approved test `LibShader_Format_slo_emitted` (TEST-APPROVAL). (maintainer)
- Q2 (a `.slo` whose DSO operand is missing at load): the load warns in the `.rslo` loader's form and returns `nullptr`; `getShader` then falls back to the `.rslo`, whose load prints the interpreter's own message. (maintainer)
- Q3 (the drift test cannot fail on the starting tree): it is registered DISABLED until the compiler's own table exists; its red evidence is its mutation self-check. FR-029 and SC-011 say so. (maintainer)
- Q4 (`precomp`): the tracked `stochasticPrimitives.h` is not raw tool output. The constants go to a new Hiders header, `src/ri/hiders/rasterFlags.h`; the criterion is "the output after equals the unmodified tool's output". US7-AS1, FR-023 and SC-009 are reworded. (maintainer)
- Q5 (construction-time reports): diagnostics raised while a `CRendererContext` is constructed no longer set `RiLastError`; their text, stream and severity are unchanged. Accepted behavior change, recorded in FR-005, SC-004 and the Assumptions, with a NEWS line. (maintainer)
- Q6: `ifexpr_generated.cpp` is compiled into all three targets (`ribRender`, `ribVector`, `ribOut`). (maintainer)
- Q7: the unreferenced `rendererClipping.cpp` and `resource.cpp` leave `ribVector`, with a NEWS line (the installed `libRIBVector` loses their exported symbols). (maintainer)
- Q8: `CRendererServices` goes from 2.0.0 to 2.0.1 for a comment-only change (`getDSO` also serves the `.slo` loader). (maintainer)
- Q9: a DSO call inside a parameter default (the init section) joins FR-022's parity matrix. (maintainer)
- Q10: the compiler's allowed include roots (US5-AS1, SC-007) also include `src/include`, `src/libshader/include` and the build directories (the generated parser, `config.h`). (maintainer)

## User Scenarios & Testing *(mandatory)*

The "users" are the maintainers who review changes against constitution Principle VIII, the authors of Scene Interface back-ends (the renderer context, the RIB writer, the preview, later a USD or OSL scene back-end), the authors of shadeop DSOs and RSL shaders, and the authors of the tools that sit on top (`orender-wire`, the data viewer, `precomp`).

### Measured starting point (2026-11-01, master `628d6747`; re-measured against master `d6910bd0` at the plan stage)

Every line below was read from the source at `628d6747`. The plan stage re-read the cited lines at `d6910bd0` (spec 026 merged), which this branch merges before implementation: the Scene Interface, preview, data viewer, `precomp` and registry citations are unchanged; the allowlist line numbers and the Shading-side locations that 026 moved are updated below and marked "(`d6910bd0`)".

**Scene Interface depends on the renderer core (DB-015, 10 allowlist lines at `tests/architecture/allowlist.txt:298-307` (`d6910bd0`; `:303-312` at `628d6747`, spec 026 removed 5 DB-001/DB-006 lines above them); the section header at `:297` says 11, the lines are 10).**

- The generated parsers include the core. `ifexpr.y:28-29` includes `renderer.h` and `rendererContext.h`; `rib.y:39-40` includes `rendererContext.h` and `renderer.h`. Four link edges: `rib.l:178` calls `CRenderer::locateFile`; `rib.y:222` calls `CRenderer::retrieveVariable`; `ri.cpp:408` and `:416` call `CRenderer::initDeclarations` and `shutdownDeclarations` (the RIB-writer branch of `RiBegin` bootstraps declarations around the `.orenderrc` parse, because "CRibOut never goes through `CRenderer::beginRenderer()`", `ri.cpp:401-404`).
- `rib.y:234` names `CRenderer::context` inside an `if (FALSE)` branch (`rib.y:232-245`). The branch is dead, but it is compiled, and it is one of the reasons `renderer.h` stays included.
- `ifexpr.y:34,568` defines `CRendererContext::ifParse`: the `if (...)` expression grammar is a renderer-core method written in a Scene Interface file. It reads attributes and options through `context->getAttributes` and `getOptions` (`ifexpr.y:203-224`), the only callers are `RiIfBeginV` and `RiElseIfV` in `rendererContext.cpp:6399,6421`, and `ifexpr_generated.cpp` is excluded from `ribVector` (`src/ri/CMakeLists.txt:381`) and not part of `ribOut`.
- `riHooks.h` lives in `src/ri/render/` (class Core by `tests/architecture/domain-map.txt`'s `prefix src/ri/render/`) although only the Scene Interface's back-ends and `RiBegin` use it: included by `parse/ri.cpp:46` and `output/ribOut.cpp:35` (the two include edges), by `rendererContext.cpp:70` (the renderer registers its factory at `:6521-6524`), and by five test headers (`tests/unit/csg/csgTestUtils.h:45`, `blobby/test_surface_params.cpp:64`, `motion/test_frame_params_equal_core.cpp:43`, `sampling/scene_probe.h:45`, `motion/replay_capture.h:55`). `CRibOut` registers itself the same way (`ribOut.cpp:2175-2187`).
- `CDelayedData` (`geometry/delayed.h:37`, class Geometry) is a record of two strings and two bounds with no dependency on `CObject`, yet `delayed.h` includes `object.h` (`delayed.h:31`). It is created by `rib.y:2722`, read by `ri.cpp:1655,1662,1771,1783` and `render/rendererProcedures.cpp:28`, and built by two tests (`tests/unit/csg/test_procedural_operand.cpp:115`, `tests/unit/sampling/test_object_ids.cpp:185`). The Scene Interface includes `delayed.h` (`ri.cpp:40`, `rib.y:37`) only for it.
- **Active context.** `CRiInterface`'s constructor assigns the global (`parse/riInterface.cpp:37`, `renderMan = this`) and attaches diagnostics (`:39`); its destructor clears both when it is the active one (`:56-58`). Everything below depends on the constructor having done it: `RiBegin`'s factories (`ri.cpp:311-322`), the archive path (`rendererContext.cpp:6225-6226` saves `renderMan` through `extern` declarations at `:100-101` and assigns a new `CRibOut`; `ri.cpp:2139-2144` restores it and re-attaches diagnostics), `ribpreview_load` (`previewContext.cpp:196-204`, whose comment calls its own assignment "technically redundant"), the data viewer (`dataLoad.cpp:175`, `new CRiInterface()` as "a safe, silent sink", torn down at `:141-142`), and tests (`blobbyTestUtils.h:53`, `texture_tile/test_texture_system_stream_cursor.cpp:111`, `test_texture_system_null_resolver.cpp:115`, `preview/test_data_keys.cpp:51`). `RiContext` (`ri.cpp:287`) assigns it by hand.
- **Precedent.** Graphics State already has the same kind of hook for the variable table: `state/variableLookup.h` (`installVariableLookup`, `installVariableDeclaration`), installed by the core in `initDeclarations` (`render/rendererDeclarations.cpp:312`) and removed in `shutdownDeclarations` (`:326`). The constitution allows Scene Interface to depend on Graphics State and Geometry only (`tests/architecture/domain-rules.txt`).
- The architecture check's renderer-core metric for the Scene Interface is 6 references, 6 distinct members (`tests/architecture/metrics-baseline.txt`; unchanged by spec 026, TOTAL 612 / 94).
- **Writer mode has no `orender` flag** (plan stage): `orender.cpp` always passes a `#...` string to `RiBegin` (`:174`, `:265`, `:877`). The RIB writer is reached by `RiBegin(<plain file name>)` (C, Python and Lua clients and the writer tests), by `RiBegin(NULL)` under `OPENRENDER_RUNPROGRAM`, and by `ArchiveBegin` inside a renderer session. Where this spec says "the full renderer's `-ribout` mode" or "`orender -ribout`", read: a process that links the full renderer and calls `RiBegin` with a file name. Its declarations exist only around the `.orenderrc` parse (`ri.cpp:407-417`); the writer tests initialise them themselves (`test_ribout_shader_order.cpp:69-75`).
- **No test exercises `IfBegin`** (plan stage): no RIB under `tests/` or `examples/` and no test source uses it.

**The back-ends and the build targets.**

- `RiBegin` takes its implementation from two registered factories (`riHooks.h`; `ri.cpp:309-330`). The preview does not use them: `ribpreview_load` calls `CRenderer::initDeclarations()`, `RiBeginLite()`, assigns `renderMan` by hand and calls `ribParse` (`previewContext.cpp:189-210`). Constitution Principle VIII rule 4 says Scene Interface back-ends "(renderer, RIB writer, preview) MUST plug in through self-registration".
- `ribOut` compiles six renderer-core files: `render/rendererDeclarations.cpp`, `rendererFileSearch.cpp`, `rendererMutexes.cpp`, `rendererNetwork.cpp`, `rendererStatics.cpp` and `vector/rendererDeclarationsDispatchStub.cpp` (`src/ri/CMakeLists.txt`, `ribOut_sources`, from line 513). `ribVector` compiles seven files of `render/` (`rendererClipping`, `rendererDeclarations`, `rendererFileSearch`, `rendererMutexes`, `rendererNetwork`, `rendererStatics`, `resource`) and the four `vector/*Stub.cpp` files (`src/ri/CMakeLists.txt:363-406`); both classes are Core.
- `libribpreview` includes `renderer.h` (`previewContext.cpp`, `ribGeometryContext.cpp`) and `rendererContext.h` (`ribGeometryContext.cpp`); the only `CRenderer` names outside comments are the two declaration calls (`previewContext.cpp:191,209`).

**The compiler and the JIT (DB-019).**

- 0 include edges and 0 allowlist lines remain (spec 037). The compiler reads shadeop declarations through `shadeop/shadeopModule.h` (`libshader/compiler/rslo.cpp:44,1702-1718`).
- The compiler owns its builtin prototype table (`CScriptContext`, `rslo.cpp:786` and following `addBuiltInFunction` calls) and its opcode mnemonics (`compiler/opcodes.cpp`). But `llvmEmitter.cpp` builds two more tables by including the shading engine's headers inside local macro redefinitions: `kOpcodeParamTable` and `kAllFunctionMnemonics` come from `#include "scriptFunctions.h"` and `"scriptOpcodes.h"` (`llvmEmitter.cpp:159-211`). The registry says "`scriptFunctions.h`, `opcodes.h`"; the second header is the shading engine's `scriptOpcodes.h`, since `compiler/opcodes.h` is the compiler's own.
- The compiler's include path still reaches the Scene Interface and the core: `compiler/CMakeLists.txt:8-17` adds `src/ri`, `render`, `parse`, `geometry`, `output`, ... and `:82` adds `src/libshader/shading`. Nothing includes from them, but nothing prevents it.
- Spec 026 (`d6910bd0`) moved `parseSloShader` out of `rendererFiles.cpp` into Shading's `src/libshader/shading/sloLoader.{h,cpp}` (`sloLoader.cpp:139`), which is where `.slo` binding happens; `getShader` falls back to the `.rslo` whenever it returns `nullptr` (`rendererFiles.cpp:395-417`). The DSO lookup is `CRendererServices::getDSO` (`rendererServices.h:404`, unchanged in services 2.0.0), reached through `libshader::lookupDSO` (`serviceLookup.cpp:42-48`) and implemented by `CRenderer::getDSO` (`rendererFiles.cpp:560-584`). 026's `llvmEmitter.cpp` edits are at `:2063-2084` and `:2977-2983`; the lines cited here are unchanged.
- The compiler's sources include nothing under `src/ri` (their depfiles, plan stage); `logging.h` resolves to `src/include/` (Infra), and the generated parser and `config.h` come from the build directories, so the allowed include roots of US5 and SC-007 also need those (plan research R11).
- The JIT has no DSO call path. The two interpreter dispatcher rows are named `"XXX"` (`scriptFunctions.h:1162,1193`) and resolve their operand from the loaded module (`shadeopOperandData`, `shadeopOperandExec`) with the executing context's thread index (`DSO_THREAD_INDEX`, `:1130-1134`). The emitter rejects any function not in `kHandledOpcodes[]` with "JIT coverage gap" (`llvmEmitter.cpp:1107-1127`), and the coverage guard hand-excludes the `"XXX"` rows (`llvmEmitter.h:62-64`). Every shadeop render test in the tree uses `.rslo` shaders only (`tests/plugins/shadeop/CMakeLists.txt:56-71`, `probe.sl` compiled to `probe.rslo`).

**Tools (DB-024, the CONSUMER section, 36 include edges).** (Plan stage, on a build older than `d6910bd0`: the section lists 66 edges, 28 of them from `src/libshader/tests/*` and 38 from the tools; what the registry's 36 counts is re-measured in the plan's Phase 0.)

- `src/precomp/precomp.cpp:35-36` includes `ri/hiders/reyes.h` and `ri/render/renderer.h`; the tool writes `src/ri/hiders/stochasticPrimitives.h` (`precomp.cpp:380-430`, a tracked file) and needs `RASTER_*` and `DEPTH_MID` (`reyes.h:52`); the text `CRenderer::depthFilter` is only printed into the output (`:399`). It links `openrendercommon` only (`src/precomp/CMakeLists.txt`).
- **The tracked `stochasticPrimitives.h` is not the tool's raw output** (plan stage): run from a scratch tree, `precomp` writes 14,029 lines that differ from the tracked file (13,248 lines) at the first byte. The tracked file carries the license header and was reformatted by `aa3c684d` (for example `switch ((grid->flags` against the tool's `switch((grid->flags`). `DEPTH_MID` also appears only inside printed text. So "byte-identical to the tracked one" cannot hold on the starting tree; the maintainer decided the criterion on 2026-11-02 (Clarifications, Session 2026-11-02, Q4): the output after the change equals the unmodified tool's output.
- `src/oshader/oshader.cpp:32-36` includes the compiler's `opcodes.h`, `rslo.h` and `llvmEmitter.h`; `src/sloinfo/sloinfo.cpp:35-39` includes the runtime loader's `rslo.h`, `llvmJit.h` and `sloMetadata.h`. There is no public compiler header: `src/libshader/include/openrender/` holds `RSLShading.h` and `RSLShadingState.h` only.
- `libribpreview` includes Geometry primitives for its tessellators, Graphics State types and four Scene Interface headers (`riInterface.h`, `ri.h`, `rib.h`, `riDiagnostics.h`); `dataLoad.cpp` reaches Hiders and Texturing for its readers (DB-025, spec 032).

### User Story 1 - The RIB parser and the RIB writer need no renderer core (Priority: P1)

A maintainer builds `ribOut` (the `-ribout` writer) and expects it to be the RIB grammar, the RI entry points and the writer, and nothing else. The parser asks for a declared variable and for a file on the search path through a service the Scene Interface owns; the full renderer, the writer and the preview each supply the implementation they need.

**Why this priority**: it is DB-015's finding and the first acceptance line of issue #43. The other stories only add to it.

**Independent Test**: list the source files of the `ribOut` target; none is classified Core by `domain-map.txt`. Parse the same RIB through `ribOut` and through the full renderer's `-ribout` mode before and after: the output is byte-identical.

**Acceptance Scenarios**:

1. **Given** a RIB with declared and inline-declared parameters, `ReadArchive` of a relative name, an `if`/`else` block and a Procedural, **When** it is written back through the RIB writer, **Then** the text is identical to the pre-change output and the writer's target compiled no renderer-core file.
2. **Given** a parameter name that is neither declared nor an inline declaration, **When** the parser reaches it, **Then** the same "Parameter ... is not declared" error is reported at the same place, from the service's answer.
3. **Given** a bare `ReadArchive "name"` found only through the archive search path, **When** it is read by the renderer and by the writer, **Then** both locate the same file, and a name found only through a network file mapping behaves as it does today in the renderer.
4. **Given** the architecture check, **When** it runs, **Then** DB-015 has no allowlist line and the Scene Interface's metric row is 0 and 0.

---

### User Story 2 - The Scene Interface owns its hooks and its records (Priority: P1)

A back-end author registers a back-end by including a Scene Interface header, and a Procedural's data record is a Scene Interface type. The `if` expression evaluator is a Scene Interface function that asks the active context for attributes and options, not a method of a core class.

**Why this priority**: it removes the two include edges `riHooks.h` causes and the last reason the grammars include the core.

**Independent Test**: the include scan of `src/ri/parse` and `src/ri/output` shows no file of class Core; the `if` grammar evaluates the same expressions to the same results.

**Acceptance Scenarios**:

1. **Given** the tree after this feature, **When** `riHooks.h` is searched for, **Then** it is in a Scene Interface directory and the 8 files that include it (two Scene Interface files, the renderer's registration and five test files, FR-007) still compile.
2. **Given** `RiIfBeginV` and `RiElseIfV` in the renderer context, **When** they evaluate a condition, **Then** they call the Scene Interface's evaluator, and every `if`/`elseif`/`else` scene in the suite gives the same result as before.
3. **Given** a Procedural of each kind (`DelayedReadArchive`, `RunProgram`, `DynamicLoad`), **When** it is parsed and expanded, **Then** the record it carries is the Scene Interface's and the expansion is unchanged.

---

### User Story 3 - The active RI context is installed explicitly (Priority: P1)

A back-end author, or a test, constructs a context without it silently becoming the process's active RI context. Installing and uninstalling is a Scene Interface call that also routes diagnostics. Nothing depends on a constructor having a global side effect.

**Why this priority**: the side effect is what lets the data viewer, the preview and four tests hand-assign the global, and it hides the order in which diagnostics attach. It is the third DB-015 direction item.

**Independent Test**: construct a context and check the active one is unchanged and diagnostics still go to the previous sink; install it and check both change; uninstall and check both are restored.

**Acceptance Scenarios**:

1. **Given** a constructed context that was not installed, **When** an error is reported, **Then** it reaches the sink that was in place before the construction.
2. **Given** `RiBegin` with any registered factory, **When** the context is created, **Then** it is the active context and diagnostics reach its error handler, as before.
3. **Given** an `ArchiveBegin` inside the renderer, **When** the archive context is created and `ArchiveEnd` runs, **Then** the outer context is active again with diagnostics re-attached (the behavior at `rendererContext.cpp:6225-6226` and `ri.cpp:2139-2144` today).
4. **Given** the data viewer and the preview load a file, **When** they finish, **Then** no active context is left behind, with no direct assignment of the global in either.

---

### User Story 4 - The preview back-end self-registers (Priority: P2)

`orender-wire` opens a RIB through the preview library. The preview registers itself with the Scene Interface the way the renderer and the RIB writer do, obtains declarations through the Scene-owned service, and names no renderer-core symbol.

**Why this priority**: constitution rule 4 names the preview as a back-end that must self-register; today it bypasses the dispatch layer.

**Independent Test**: build `libribpreview` and check it includes no Core header and no `CRenderer` symbol; load the preview scenes (`tests/preview`) and compare the extracted vertices and camera to the pre-change values.

**Acceptance Scenarios**:

1. **Given** `libribpreview` is linked into a program, **When** `ribpreview_load` is called, **Then** the preview context is created through the Scene Interface's registration, not by the library setting the global.
2. **Given** a preview of any RIB in the preview suite, **When** it is loaded, **Then** the extracted scene is identical to the pre-change one, and diagnostics during the parse reach the same sink.
3. **Given** a build that links the preview and not the renderer or the writer, **When** `RiBegin` is called for a mode whose back-end is not linked, **Then** the existing "not available in this build" diagnostic is printed.

---

### User Story 5 - The shader compiler is standalone (Priority: P2)

A shader author or a tool author builds the compiler on its own. It owns every table it uses and includes nothing of the shading engine, the Scene Interface or the core.

**Why this priority**: it is the open half of DB-019 and what lets the later compiler library (043) be written against a clean base.

**Independent Test**: a mechanical include check over `src/libshader/compiler` fails on the starting tree (it includes `scriptFunctions.h` and `scriptOpcodes.h`) and passes after; the compiler's target has no include directory under `src/ri` or `src/libshader/shading`.

**Acceptance Scenarios**:

1. **Given** the compiler sources, **When** their includes are collected, **Then** each resolves inside `src/libshader/compiler`, `src/common`, `src/shadeop`, `src/include`, `src/libshader/include`, the build directories (the generated parser, `config.h`), the standard library or LLVM.
2. **Given** the compiler's builtin-function, opcode and parameter-bit tables and the shading engine's, **When** the drift test runs, **Then** it compares them row by row and fails if either side gains, loses or changes a row without the other (the test is a consumer and may include both).
3. **Given** every shipped shader, **When** it is compiled for `.rslo` and for `.slo`, **Then** the output is byte-identical to the pre-change output.

---

### User Story 6 - A `.slo` shader can call a DSO shadeop (Priority: P2)

A shader author writes an RSL shader that calls a DSO shadeop and compiles it with `oshader --jit`. It compiles, loads and renders the same image as the interpreter's `.rslo` version, including a per-thread shadeop (interface 1.1).

**Why this priority**: today the JIT rejects the call, so the JIT format is not a drop-in for shaders that use a DSO.

**Independent Test**: the existing shadeop render scenes (`probe.sl`, `probe_two.sl`) rendered with `.rslo` and with `.slo` and compared; fails on the starting tree because the `.slo` compile is rejected.

**Acceptance Scenarios**:

1. **Given** a shader calling a value-returning DSO operand and one calling a void operand, **When** compiled with `oshader --jit` and rendered, **Then** the compile succeeds and the image equals the `.rslo` render pixel for pixel (the interpreter is the reference).
2. **Given** a per-thread shadeop (interface 1.1) executed on eight threads under the JIT, **When** it runs, **Then** each thread receives its own data pointer and the cleanups run as in the interpreter's test.
3. **Given** a varying operand, a uniform operand, a string operand and an operand under an `if`, **When** rendered under both backends, **Then** the results are equal, including points masked off by the condition. A DSO call inside a parameter default (the init section, thread index 0) is part of the same matrix.
4. **Given** a `.slo` that calls an operand whose module is absent at render time, **When** it loads, **Then** the `.slo` load warns in the `.rslo` loader's form and returns `nullptr` (no crash), `getShader` falls back to the `.rslo`, and that load prints the interpreter's own "Failed to locate DSO function" message.

---

### User Story 7 - Tools use public interfaces for what this spec closes (Priority: P3)

The tools that reach into the Scene Interface and the core get the same capability from Scene Interface or common entry points. DB-024 is narrowed, not closed (Clarifications, Q3).

**Why this priority**: it is the registry's last 038 item; the consumer report never fails, so it is the least urgent.

**Independent Test**: the architecture check's CONSUMER section no longer lists the retired edges, and the generated file and the tools' outputs are unchanged.

**Acceptance Scenarios**:

1. **Given** `precomp` is run, **When** it writes `stochasticPrimitives.h`, **Then** the file it writes is byte-identical to the one the unmodified tool writes (captured once before the change), and the tool includes neither `reyes.h` nor `renderer.h` (its constants come from the Hiders header `rasterFlags.h`).
2. **Given** the data viewer and the preview load a file, **When** they need declarations and a diagnostic sink, **Then** they use the Scene Interface's entry points (US1, US3), not `CRenderer::initDeclarations` or a hand-assigned global.

---

### Edge Cases

- **No service installed.** A parse with no declaration service (for example a unit test that links the grammar alone) reports "not declared" for every non-inline parameter and locates no file but an absolute or relative one that exists; it never dereferences null (`rib.l:171-178` already degrades this way for options).
- **Two back-ends in one process.** A process that links the renderer and the writer (`orender -ribout`) keeps both factories; the declaration service is installed once per session, by whichever back-end begins it, and the `RiBegin`/`RiEnd` pairing of `ri.cpp:306-460` is unchanged.
- **Declarations before `RiBegin`.** The preview and the data viewer initialise declarations without `RiBegin`; the service supports that and its teardown is idempotent.
- **A context destroyed while active.** Destroying an installed context uninstalls it and detaches diagnostics, as the destructor does now (`riInterface.cpp:56-58`); destroying an uninstalled one touches neither.
- **Nested contexts.** The archive context nests inside a renderer context; installing saves the previous one, and `RiArchiveEnd` restores it. A second nesting level is not introduced.
- **Static-library dead stripping.** The preview's registration lives in a static library: it must be reached from a symbol the program references (`ribpreview_load`), or the linker drops it silently, as the registry's gotcha 3 describes for JIT symbols.
- **A DSO operand with no module at JIT-load time.** `lookupDSO` finds nothing: the `.slo` load warns and returns `nullptr`, the loader falls back to the `.rslo`, and the `.rslo` load prints the interpreter's message (US6-4). With no `.rslo` the shader fails to load as any shader with no loadable file does.
- **`.slo` compiled before this feature.** A shader without DSO calls loads unchanged; a `.slo` whose format version is older than the one that introduces the DSO call is rejected or rebuilt per the existing stale-bitcode rule (registry gotcha: ABI mismatch between stale bitcode and runtime), never run with misread arguments.
- **Hostile table drift.** If the compiler's function table and the engine's differ, the drift test fails the build before a shader mis-compiles.
- **Racing registration.** Registration is by static initializer before `main` (not thread-safe, as `riHooks.h` already says); the explicit install/uninstall calls are made by the session's one thread.

## Requirements *(mandatory)*

### Functional Requirements

**Scene-owned services (US1)**

- **FR-001**: The Scene Interface MUST own a declaration service (resolve a declared variable by name, declare one, set up and tear down the standard declarations) and a file-search service (locate a file on a search path, with the network file mapping kept by the implementation). The RIB grammar, the lexer, the RI entry points and the RIB writer MUST use only these two services for what they used `CRenderer::retrieveVariable`, `locateFile`, `initDeclarations` and `shutdownDeclarations` for. The `ribOut` target MUST compile no source file of class Core (`domain-map.txt`). See Q2 for `ribVector`.
- **FR-002**: The service interfaces MUST follow the project's hook pattern (Graphics State's `variableLookup.h` is the precedent): owned by the Scene Interface, installed by the back-end that begins a session and removed at its end, answering "not found" with no service installed, and never requiring the grammar to name a core class. The full renderer MUST supply its present implementation; the writer and the preview MUST supply an implementation that compiles no Core file; which domain holds the shared declaration table is the plan's decision, and the declared set, the global-variable numbering and the notification of live shading contexts when a global is added MUST be exactly as today.
- **FR-003**: `rib.y` MUST no longer reference `CRenderer::context` (the dead branch at `rib.y:233-245` is removed or rewritten against the active context), and no generated parser MUST include `renderer.h` or `rendererContext.h`.
- **FR-004**: The `if` expression evaluator (`ifexpr.y`) MUST be a Scene Interface function, not a method of `CRendererContext`. It MUST read attributes and options through the active context's existing accessors (or the Graphics State types it already uses), `CRendererContext::RiIfBeginV` and `RiElseIfV` MUST call it, and the grammar file MUST include no core header. The evaluator's file (`ifexpr_generated.cpp`) is compiled into all three targets (`ribRender`, `ribVector`, `ribOut`; Session 2026-11-02, Q6); `if` blocks in the last two builds behave as they do now (nothing there calls it).
- **FR-005**: Behavior MUST NOT change: for every scene of the visual, parity, RIB-output and preview suites, the image or the written text is identical to the pre-change output, and every diagnostic is the same text at the same severity. One accepted exception (Session 2026-11-02, Q5): a diagnostic raised while a `CRendererContext` is being constructed (file and network setup) reaches the sink installed before `RiBegin`, so it no longer sets `RiLastError`; its text, stream and severity are unchanged. NEWS records it.

**Hooks and records (US2)**

- **FR-006**: `riHooks.h` MUST move into the Scene Interface (`src/ri/parse/`), with its three registration functions unchanged in name and behavior (`RiSetContextFactory`, `RiRegisterDefaultContextFactory`, `RiRegisterRibOutFactory`). Its comment, which says the registrations keep a consumer from linking the whole renderer, MUST be kept accurate.
- **FR-007**: Every file that includes `riHooks.h` (listed in the starting point: `ri.cpp`, `ribOut.cpp`, `rendererContext.cpp` and the five test files) MUST compile against the new location, and the two DB-015 `riHooks.h` include lines MUST be gone.
- **FR-008**: `CDelayedData` MUST move into a Scene Interface header and MUST NOT include `object.h` or any Geometry header. `geometry/delayed.h` MUST keep `CDelayedObject` and `CDelayedInstance` and MUST keep compiling for its users. `rib.y`, `ri.cpp`, `rendererProcedures.cpp` and the two tests MUST use the moved type. The Scene Interface MUST include no `delayed.h`.

**Explicit active context (US3)**

- **FR-009**: Constructing a `CRiInterface` (or any derived context) MUST NOT assign the active-context global or attach diagnostics. A Scene Interface function installs a context (saving the previous one so a nesting can restore it) and attaches its diagnostics; another uninstalls it and restores both. Destroying an installed context MUST uninstall it. Destroying an uninstalled one MUST touch neither.
- **FR-010**: Every site that relied on the constructor MUST install explicitly: the three `RiBegin` branches, `RiContext`, the archive begin and end, `ribpreview_load`, the data viewer's document, and the tests that assign the global. After this feature no file outside the Scene Interface assigns the global directly (a mechanical check, failing on the starting tree), and the `extern` declarations of `renderMan` and `savedRenderMan` in `rendererContext.cpp:100-101` are replaced by the Scene Interface calls.
- **FR-011**: Diagnostics MUST keep routing as in spec 023: with a context active, errors reach its error handler; with none, they reach the sink installed before the first context; attaching twice or detaching with none MUST be harmless.

**Preview self-registration (US4)**

- **FR-012**: The preview back-end MUST register itself with the Scene Interface by a static initializer in its own translation unit, as the other two back-ends do, and `ribpreview_load` MUST begin its session through the Scene Interface (declarations through the FR-001 service, context through FR-009). The selection key by which a program asks for the preview mode, and whether `ribpreview_load` goes through `RiBegin` or a lighter entry like today's `RiBeginLite`, is the plan's decision; neither may make the dispatch layer name the preview.
- **FR-013**: `libribpreview` MUST include no renderer-core header and name no `CRenderer` or `CRendererContext` symbol. Whether the unused `renderer.h` and `rendererContext.h` includes in `ribGeometryContext.cpp` are removable is for the plan to confirm by building without them.
- **FR-014**: A program linking none of the renderer, writer or preview keeps the existing "not available in this build" diagnostics.

**Build targets (US1, US4)**

- **FR-015**: `ribOut` MUST compile no Core file. `ribVector` MUST compile no Core file that the Scene Interface or the preview needs (Clarifications, Q2). The Core files `ribVector` still compiles (those only Geometry and Hiders need) MUST be listed in the registry against DB-004 and DB-011, pinned by a shrink-only mechanical check that fails if the list grows, and removed by specs 029 to 031. `rendererClipping.cpp` and `resource.cpp`, which nothing in `ribVector` or the preview references, are removed from `ribVector` (Session 2026-11-02, Q7); NEWS records the removed exported symbols of the installed `libRIBVector`.
- **FR-016**: The build rules in `src/ri/CMakeLists.txt` for `ribOut` and `ribVector` MUST say what each excludes and why, and the comments now describing the real/stub pairs MUST be updated where a stub disappears.

**Standalone compiler (US5)**

- **FR-017**: The compiler MUST own the (text, parameter-bit) table and the builtin-function mnemonic list that `llvmEmitter.cpp` builds today by including `scriptFunctions.h` and `scriptOpcodes.h`, and MUST include no header of the shading engine. A drift test (a consumer, allowed to include both sides) MUST compare the compiler's tables with the engine's row by row, covering the function table, the opcode table and the parameter bits, and MUST fail on any difference. The existing coverage-guard and table-parity tests (`LibShader_OpcodeCoverage`, `LibShader_UsedParametersTable`) MUST keep their meaning and pass.
- **FR-018**: The compiler target MUST have no include directory under `src/ri/` and none under `src/libshader/shading`, and a mechanical include check over `src/libshader/compiler` (failing on the starting tree) MUST pass. The check's allowed include roots are the compiler's own directory, `src/common`, `src/shadeop`, `src/include`, `src/libshader/include`, the build directories (generated parser, `config.h`), the standard library and LLVM. The compiler MUST keep reading shadeop declarations only through the versioned shadeop module.
- **FR-019**: Every shipped shader MUST compile to byte-identical `.rslo` and `.slo` output (a shipped shader calls no DSO operand, so its `.slo` keeps the 2.0.0 format record; only a module that calls a DSO operand records 2.1.0, Session 2026-11-02, Q1), and `oshader --help` and its exit codes MUST not change. DB-019 moves to Resolved with this spec, once FR-017, FR-018 and US6 hold; spec 043 keeps only the public library and C API (Clarifications, Q1).

**JIT DSO call path (US6)**

- **FR-020**: `oshader --jit` MUST compile a call to a DSO shadeop (value-returning and void) found through the shadeop module, and the JIT MUST execute it with the interpreter's semantics: the operand's `exec` called per shading point with the operands' addresses and strides, the thread's data pointer from the shadeop module (the per-thread data of interface 1.1 included), uniform and varying operands, string operands, and points masked off by control flow not executed. The `"XXX"` placeholder rows stay excluded from the coverage guard's mnemonic universe, and the guard MUST gain a case that a DSO call is handled, so that "handled" does not mean "silently skipped" (the lesson of registry gotcha 12).
- **FR-021**: The `.slo` MUST record what the loader needs to bind the operand (name and prototype), the loader MUST resolve it through the same services lookup the `.rslo` loader uses, and a missing operand MUST make the `.slo` load warn in the `.rslo` loader's form (`CODE_BADFILE`, "Failed to locate DSO function", operand name and prototype) and return `nullptr`, so the loader falls back to the `.rslo`, which prints the interpreter's diagnostic (Session 2026-11-02, Q2). The `.slo` format gains MINOR 1 (2.1.0), recorded only by a module with a DSO call (Session 2026-11-02, Q1); an older loader rejects such a module, and an older `.slo` is rejected or rebuilt under the existing stale-bitcode rule, never misread. `getDSO` also serves the `.slo` loader, so `CRendererServices` goes to 2.0.1 (a comment-only PATCH; Session 2026-11-02, Q8).
- **FR-022**: Parity MUST be tested on both backends: the existing shadeop probe scenes rendered as `.rslo` and as `.slo`, a per-thread operand on eight threads, varying/uniform/string/masked arguments, a void operand, and a DSO call in a parameter default (the init section; Session 2026-11-02, Q9). The test MUST fail on the starting tree.

**Tools (US7)**

- **FR-023**: `precomp` MUST NOT include a hider or renderer-core header; the constants it needs come from the new Hiders-owned header `src/ri/hiders/rasterFlags.h` (no includes; `reyes.h` includes it), and the file it writes MUST equal, byte for byte, the file the unmodified tool writes (captured once before the change). The tracked `stochasticPrimitives.h` is a formatted copy with a license header, not raw output, and is not the reference (Session 2026-11-02, Q4).
- **FR-024**: The data viewer and the preview MUST use the Scene Interface's entry points for declarations and the active context (FR-001, FR-009); the Hiders and Texturing reach of `dataLoad.cpp` and the Geometry and Graphics State reach of `libribpreview` are not changed here (DB-025, spec 032; Clarifications, Q3). The tools' behavior and outputs MUST not change.

**Registry, allowlist, documentation**

- **FR-025**: The 10 DB-015 allowlist lines (`allowlist.txt:298-307` at `d6910bd0`; `:303-312` at `628d6747`) MUST be deleted and the section header removed with them; DB-015 MUST move to Resolved with this spec's number and the date. DB-019 MUST move to Resolved with this spec's number and the date (Clarifications, Q1). DB-024 MUST be narrowed and stay Open (Q3); an entry moves to Resolved only when it has no allowlist line and none of its evidence is left. No allowlist line is added.
- **FR-026**: DB-004's two link lines that name `CRenderer::retrieveVariable` (`blobby.cpp`) and `CRenderer::locateFile` (`blobbyRepeller.cpp`) and the equivalent Texturing callers MUST keep working; if the plan keeps `CRenderer`'s members as thin forwarders the lines are unchanged and still registered; if a symbol is removed, the stale lines are deleted. The lines are never moved to another registry entry to make a check pass.
- **FR-027**: The architecture check MUST pass with the Scene Interface's metric baseline lowered to 0 and 0 and the total lowered to the reported values; the baseline only goes down.
- **FR-028**: `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` MUST be updated: DB-015 to Resolved (with the corrected 10-line count and the `riInterface.cpp:37` citation, the registry says :36), DB-019 to Resolved (its Direction text no longer assigns the standalone compiler to 043), DB-024's evidence, size, direction and history narrowed as decided, and the corrected DB-024 `RiMake*` line, the `scriptOpcodes.h` name in DB-019 and the DB-015 allowlist header count (Clarifications, Q3). `DEVNOTES.md`, the RIB and plug-in guides that name `riHooks.h`'s old location or the constructor side effect, and `CLAUDE.md`'s architecture notes and Recent Changes MUST be consistent with the tree. New directories or files are classified in `tests/architecture/domain-map.txt`. `NEWS.md` and `ChangeLog.md` record the accepted `RiLastError` change (Session 2026-11-02, Q5) and the removed `libRIBVector` symbols (Session 2026-11-02, Q7). `CLAUDE.md` is never edited by an agent: its changes go into `specs/038-scene-interface-untangle/claude-md-draft.md` for the maintainer to apply.
- **FR-029**: Tests MUST be written before the change and fail on the starting tree: the `ribOut` source-class check, the include checks (parser, compiler, preview), the assigned-global check, the context-construction test, the preview self-registration test, the table-drift test, and the `.slo` shadeop parity tests. The table-drift test cannot fail on the starting tree (both tables come from one header): it is registered DISABLED until the compiler's own table exists, and its red evidence is its mutation self-check (Session 2026-11-02, Q3). The suites (visual, shading parity, state, common, plug-in ABI, preview, architecture) pass with no threshold widened and no test disabled.
- **FR-030**: No task and no requirement of this spec depends on the merge. Registry entries move to Resolved with this spec's number and the date; the issue closes through the line the maintainer writes in the merge commit.

### Key Entities

- **Declaration service**: the Scene-owned interface through which the grammar resolves and declares variables; installed per session.
- **File-search service**: the Scene-owned interface through which the lexer and the grammar locate archives.
- **Active context**: the one `CRiInterface` the RI entry points dispatch to; installed and uninstalled explicitly, with its diagnostics attachment.
- **Back-end registration**: a factory registered by static initializer: renderer, RIB writer, preview.
- **Procedural record** (`CDelayedData`): generator, helper and bounds a Procedural statement carries.
- **Compiler table set**: the compiler's function mnemonics, (text, parameter-bit) rows and opcode mnemonics; mirrored by a drift test.
- **DSO call**: the JIT's lowering of a shadeop operand call, bound at `.slo` load through the services lookup.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The `ribOut` target's source list has 0 files of class Core (6 at master `628d6747`); `ribVector`'s count of Core files the Scene Interface or the preview needs is 0, and the residual list is pinned and shrink-only (Q2).
- **SC-002**: The architecture check passes with DB-015's 10 allowlist lines deleted, no line added, and the Scene Interface's renderer-core metric 6/6 -> 0/0.
- **SC-003**: No generated parser, no file in `src/ri/parse` or `src/ri/output`, and no file in `libribpreview` includes a Core header (today: 2 generated parsers, plus `riHooks.h` through 2 files, plus the preview's 2).
- **SC-004**: For every scene in the visual (87+), RIB-output, preview and shading-parity suites, output is identical to the pre-change output on macOS and Linux, with no threshold widened; all diagnostics are unchanged text (the one accepted difference is that construction-time reports no longer set `RiLastError`, FR-005).
- **SC-005**: A context constructed and not installed changes neither the active context nor the diagnostic sink; installed, both change; uninstalled, both are restored. Zero files outside the Scene Interface assign the active-context global (mechanical check).
- **SC-006**: `ribpreview_load` on each scene of the preview suite returns vertices, colors, camera and bounds equal to the pre-change values, and the library names 0 `CRenderer` symbols.
- **SC-007**: The compiler's includes resolve only inside the compiler, `src/common`, `src/shadeop`, `src/include`, `src/libshader/include`, the build directories (generated parser, `config.h`), the standard library and LLVM; its target has 0 include directories under `src/ri` and `src/libshader/shading`; the drift test passes and fails when a row is changed in either table; all shipped shaders compile to byte-identical `.rslo` and `.slo`.
- **SC-008**: Each shadeop probe scene renders equal pixels under `.rslo` and `.slo` (0 differing pixels on deterministic scenes), a per-thread operand on 8 threads yields 8 distinct data pointers and 8 cleanups under the JIT, a DSO call in a parameter default renders equal pixels under both formats, and a missing operand makes the `.slo` load warn and return `nullptr` (no crash) with the `.rslo` fallback printing the interpreter's message.
- **SC-009**: `precomp` writes a `stochasticPrimitives.h` byte-identical to the unmodified tool's output, and includes neither `reyes.h` nor any core header (its one hider include is the constants header `rasterFlags.h`).
- **SC-010**: DB-015 under Resolved with spec 038 and the date; DB-019 under Resolved with spec 038 and the date, DB-024 narrowed and still Open, both consistent with the allowlist; `DEVNOTES.md`, the registry and the `CLAUDE.md` draft (`claude-md-draft.md`, applied by the maintainer) agree with the tree.
- **SC-011**: The checks of FR-029 each fail on the starting tree (master `d6910bd0` merged into the branch); a check that cannot fail does not count, except the table-drift test, whose red evidence is its mutation self-check (Session 2026-11-02, Q3).
- **SC-012**: The full suite (everything except the opt-in determinism sweep) passes on macOS and Linux; the Linux run uses GCC 13 with `-Werror`.

## Assumptions

- **Domains.** The two services and the hooks are Scene Interface (Principle VIII allows it to depend on Graphics State and Geometry only); the declaration table's owner, and whether the writer's and the preview's implementation is shared, is the plan's decision under FR-002. New files are classified in `domain-map.txt`.
- **Constitution, no amendment.** Rule 4's sentence about Scene Interface back-ends self-registering is already in version 1.3.0; this spec implements it.
- **No behavior change.** This is an ownership and wiring change. Apart from the JIT DSO path (a capability added) every output stays identical; no image, RIB text or diagnostic text changes. The one accepted difference is that diagnostics raised during `CRendererContext` construction no longer set `RiLastError` (Session 2026-11-02, Q5).
- **Clarified scope.** The maintainer's answers of 2026-11-01 (Clarifications) fix the compiler split with spec 043, the reach of the `ribVector` criterion and the extent of DB-024's closure. The preview's Geometry and Graphics State includes (tessellators) are consumer reach that the answers do not name for retirement; they are left as they are and stay recorded in the narrowed DB-024 entry.
- **Test and design discipline.** TDD (Principle III): the failing checks of FR-029 come first. No build or test is run during this stage; when the plan stage needs one it goes through `~/.claude/bin/cpujob`.
- **Out of scope.** Geometry's and Hiders' remaining `CRenderer` references (DB-004, DB-011; specs 029 to 031); the data viewer's Hiders and Texturing reach (DB-025, spec 032); the shader-metadata API for `sloinfo` (spec 033); the public compiler library and C API (043, decision D25); the OSL or RSL 2.0 front ends; Windows.
- **Spec 026 (shading contract v2) is merged** into master as `d6910bd0` (plan stage); this branch merges master before implementation (plan Phase 0), and nothing here depends on 038's own merge. What 026 changed where the two touch, as merged: (1) `parseSloShader` is in Shading's `sloLoader.{h,cpp}`, where US6's operand binding sits; (2) `CRendererServices` is versioned 2.0.0 (`getAttribute`, `getMessage`, `getMatrix`, `tracer()` added), with `getDSO` unchanged (`rendererServices.h:404`; this spec only edits its comment and takes the version to 2.0.1; Session 2026-11-02, Q8); `llvmEmitter.cpp` gained the `ctransform "c=SSc"` case and the `rendererinfo()` `resultKind` change, and `rslOps` `op_ctransform_ss`, all away from the lines US5 and US6 edit; (3) the registry and the allowlist: DB-001 8 → 5 edges (the three `trace`/`traceEx` link lines), DB-006 33 → 31 (the two `execute.cpp`/`executeMisc.cpp` → `surface.h` includes), DB-018's evidence rewritten, DB-015, DB-019 and DB-024 untouched; `metrics-baseline.txt` unchanged (TOTAL 612 / 94); `CLAUDE.md` gained gotcha 28 and its 026 entries; (4) `libshader_shading`'s flat-namespace imports went 6 → 4; this spec adds none (FR-020 adds only an `op_*` entry point that resolves in the shading library). No task of this spec reads an allowlist line, a registry text or a metric that only 026 changed.
- **Other merged work.** Specs 023 (`libRIBCommon`, diagnostics hooks) and 037 (plug-in ABI, shadeop module, `procDynamicLoadHook.h`) are merged and provide the pieces this spec reuses.
