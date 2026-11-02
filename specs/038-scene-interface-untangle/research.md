# Research: Scene Interface Untangle (spec 038)

**Branch**: `038-scene-interface-untangle` | **Date**: 2026-11-01 | **Base**: master `d6910bd0`
(spec 026 merged). The worktree is at `628d6747`; the coordinator merges master into the branch
before implementation (plan.md, Phase 0). Everything below was read from `d6910bd0` (the main
checkout) unless a line says otherwise. The files spec 026 changed and that this spec touches are
`src/libshader/compiler/llvmEmitter.cpp` (026's edits are at `:2063-2084` and `:2977-2983`, away
from every line cited here), `src/libshader/shading/{rendererServices.h,sloLoader.{h,cpp},rslOps.*}`,
`src/ri/render/rendererFiles.cpp` and `tests/architecture/allowlist.txt` (DB-001, DB-006 lines only).
The Scene Interface sources, `riHooks.h`, `delayed.h`, the preview, the data viewer, `precomp`,
`metrics-baseline.txt` and the registry entries DB-015, DB-019 and DB-024 are the same at both
commits.

Section M records measured facts. Section R records decisions (Decision / Rationale /
Alternatives). Section Q lists the questions the maintainer must answer; each has a recommendation.

Some numbers below come from the main checkout's build tree. That tree was built on 2026-11-01 at
03:59 and 11:22, before the 026 merge (22:53). Those numbers are marked "(pre-026 build)" and are
re-measured in Phase 0.

---

## M. Measured facts

### M1. Where the Scene Interface reaches the core (DB-015)

| Site | What it does | Allowlist (`d6910bd0`) |
|---|---|---|
| `src/ri/parse/rib.y:39-40` | includes `rendererContext.h`, `renderer.h` | `:300-301` |
| `src/ri/parse/ifexpr.y:28-29` | includes `renderer.h`, `rendererContext.h` | `:298-299` |
| `src/ri/parse/rib.y:222` | `CRenderer::retrieveVariable(par->name)` | `:305` |
| `src/ri/parse/rib.y:232-245` | dead `if (FALSE)` branch naming `CRenderer::context` (`:234`) | none (no link edge: dead code) |
| `src/ri/parse/rib.l:178` | `CRenderer::locateFile(location, fileName, archivePath, TRUE)` | `:304` |
| `src/ri/parse/ri.cpp:408,416` | `CRenderer::initDeclarations()` / `shutdownDeclarations()` around the `.orenderrc` parse in writer mode | `:306-307` |
| `src/ri/parse/ri.cpp:46`, `src/ri/output/ribOut.cpp:35` | include `src/ri/render/riHooks.h` | `:302-303` |
| `src/ri/parse/ifexpr.y:568` | defines `CRendererContext::ifParse`; reads `context->getAttributes(TRUE)` / `getOptions()` (`:203-224`); called only from `rendererContext.cpp:6399,6421` | none |

The DB-015 section is `allowlist.txt:297-307`: header at `:297` says "(11)", there are 10 lines
(`:298-307`). In the worktree base (`628d6747`) the same lines were `:302-312`; spec 026 removed 5
lines above them. The Scene Interface metric is 6 references / 6 distinct (`metrics-baseline.txt`,
unchanged by 026; TOTAL 612 / 94): `retrieveVariable`, `context`, `locateFile`, `initDeclarations`,
`shutdownDeclarations`, `ifParse`.

### M2. The declaration table today

`CRenderer::initDeclarations()` (`src/ri/render/rendererDeclarations.cpp:60-316`) does six things:
1. allocates the declared-variable trie, the global-variable array and the variable list
   (`:70-77`; the statics are `renderer.h:151-158`, defined in `rendererStatics.cpp:109-116`);
2. begins Graphics State's channel registry (`:81`);
3. resets the global identifiers (`:84-86`);
4. declares the standard options, attributes and globals in a fixed order (`:96-282`); the
   `assert(tmp->entry == VARIABLE_*)` lines depend on that order;
5. defines the named coordinate systems (`:292-309`);
6. installs Graphics State's two hooks (`installVariableLookup`, `installVariableDeclaration`,
   `:312-315`; `state/variableLookup.h`).

`shutdownDeclarations()` (`:324-344`) reverses it. `declareVariable` (`:391-458`) parses, applies
the redeclaration rules (the "st" rule at `:417-431`) and, for a `global` storage class, calls
`makeGlobalVariable` (`:449-451`). `makeGlobalVariable` exists twice: the real body numbers the
variable and notifies every live shading context (`render/rendererDeclarationsDispatch.cpp:70-84`);
the `ribVector` stub only numbers it (`vector/rendererDeclarationsDispatchStub.cpp:39-45`).
`retrieveVariable` is a trie lookup (`:474-484`).

Users of the table statics outside `rendererDeclarations*.cpp`: `rendererServicesImpl.h:41-42`
(`numGlobalVariables`, `globalVariable` for Shading). Callers of `CRenderer::retrieveVariable` /
`locateFile` outside the core: `geometry/blobby.cpp:286,410,411` and `geometry/blobbyRepeller.cpp:138`
(DB-004 lines `:49-50`), the RIB parser (DB-015). Shading reaches the table only through
`CRendererServices::retrieveVariable` (`rendererServices.h:395`).

### M3. Every sequence that sets up declarations today

| Sequence | Init | Parse | Teardown |
|---|---|---|---|
| Renderer session | `CRendererContext` ctor → `beginRenderer()` → `initDeclarations()` (`renderer.cpp:114`) | everything until `RiEnd` | `endRenderer()` → `shutdownDeclarations()` (`renderer.cpp:160`) |
| Writer mode `RiBegin(<plain name>)` / `OPENRENDER_RUNPROGRAM` | `ri.cpp:407-409`, only if `$ORENDERHOME/.orenderrc` exists | `.orenderrc` only | `ri.cpp:414-417` |
| Writer tests (`RibOut_ShaderStatementOrder`, `test_sampling_option_roundtrip.cpp:91-95`) | the test calls `CRenderer::initDeclarations()` after `RiBegin` | the test's RIB | the test calls `shutdownDeclarations()` before `RiEnd` |
| Preview `ribpreview_load` | `previewContext.cpp:191` | one RIB | `previewContext.cpp:209` |
| Data viewer `CDataDocument::open` | `dataLoad.cpp:165` | none (readers) | the document's destructor, `dataLoad.cpp:144` |

A writer-mode parse with no caller-provided declarations (a `ReadArchive` handled by `ribParse`
after `RiBegin(<plain name>)`) would reach `assert(declaredVariables != NULL)` with the table torn
down. No sequence in the tree does that: the writer re-serializes `ReadArchive` through
`CRibOut::RiReadArchiveV`. Two data documents open at once would destroy the table under the second
document; nothing does that either.

### M4. `locateFile` and what the writer and the preview see

`CRenderer::locateFile` (`rendererFileSearch.cpp:72-148`): a network file mapping when
`netClient != INVALID_SOCKET`; a name with a directory separator is checked as given; otherwise
the search path, then `temporaryPath + name`; then the network (`getFile`, under `networkMutex`);
then, if `tryNormalize`, the same search with `/` and `\` replaced by `_`; `info(CODE_RESOLUTION,
...)` on every outcome (`"\"%s\" -> \"%s\"\n"`, `"\"%s\" -> ???\n"`). `temporaryPath` is set only in
`beginRenderer()` (`renderer.cpp:135`); in writer and preview mode it is the zero-initialised array
(`rendererStatics.cpp:122`), so the fallback looks for the bare name in the working directory, and
`netClient` is `INVALID_SOCKET` (`rendererStatics.cpp:119`), so the network steps never run.

That is why `ribOut` compiles `rendererNetwork.cpp`, `rendererMutexes.cpp` and `rendererStatics.cpp`
(`src/ri/CMakeLists.txt:515-530`): `rendererFileSearch.cpp` references `netClient`,
`netFileMappings`, `temporaryPath`, `networkMutex` and `getFile`.

### M5. The `ribVector` Core files and who needs them (nm sweep, pre-026 build)

`ribVector` compiles `render/{rendererClipping,rendererDeclarations,rendererFileSearch,
rendererMutexes,rendererNetwork,rendererStatics,resource}.cpp` and the four `vector/*Stub.cpp`
(`src/ri/CMakeLists.txt:364-408`, comment `:283-363`). Undefined symbols of every non-Core `ribVector` object and of
every `libribpreview` object, joined with the symbols the Core objects define:

| Core file | Referenced by | Needed by Scene/preview? |
|---|---|---|
| `rendererDeclarations.cpp` | `parse/ri.cpp` (init/shutdown), `rib_generated.cpp` (`retrieveVariable`), `dataviewer/dataLoad.cpp` (init/shutdown), `previewContext.cpp` (init/shutdown); `geometry/blobby.cpp` (`retrieveVariable`) | yes today; after this spec only `blobby.cpp` (Geometry) |
| `rendererFileSearch.cpp` | `rib_generated.cpp`, `geometry/blobbyRepeller.cpp` | yes today; after: only `blobbyRepeller.cpp` |
| `rendererStatics.cpp` | Geometry (`object.cpp`, `blobbyRepeller.cpp`), Hiders (`compositor.cpp`, `irradiance.cpp`, `photonMap.cpp`), `dataLoad.cpp` | no (Geometry, Hiders, data viewer) |
| `rendererMutexes.cpp`, `rendererNetwork.cpp` | `rendererFileSearch.cpp` only | no |
| `rendererClipping.cpp`, `resource.cpp` | nothing in `ribVector` or `libribpreview` | no |
| `vector/geometryDispatchStub.cpp`, `surfaceCrossingDispatchStub.cpp` | Geometry | no |
| `vector/irradianceDispatchStub.cpp` | Hiders (`irradiance.cpp`) | no |
| `vector/rendererDeclarationsDispatchStub.cpp` | `geometry/object.cpp` (`findCoordinateSystem`; DB-017 line `:340`), `rendererDeclarations.cpp` (`makeGlobalVariable`) | no |

The only Core symbols `libribpreview` references are `CRenderer::initDeclarations` and
`shutdownDeclarations`, both from `previewContext.cpp.o`; `ribGeometryContext.cpp.o` references
none, although it includes `renderer.h` and `rendererContext.h`.

### M6. The active-context global

Assignments of `renderMan` outside `src/ri/parse/`: `render/rendererContext.cpp:6225-6226`
(through the `extern`s at `:100-101`), `dataviewer/dataLoad.cpp:141-142,175`,
`preview/libribpreview/previewContext.cpp:200,202`, and four tests
(`tests/unit/blobby/blobbyTestUtils.h:53`, `tests/unit/texture_tile/test_texture_system_stream_cursor.cpp:111,229`,
`test_texture_system_null_resolver.cpp:115,165`, `tests/preview/test_data_keys.cpp:51,57`).
`blobbyTestUtils.h` depends on the constructor's diagnostics attachment: it assigns the global and
then records `RiError` calls, which reach it only because `riDiagnosticsAttach()` ran in the
constructor.

Diagnostics: `CRiDiagnosticSink::report` reads `renderMan` on each report (`parse/error.cpp:182-188`);
`riDiagnosticsAttach`/`Detach` are idempotent (`:202-221`). `RiErrorPrint` (`ri.cpp:2017-2031`) prints
exactly what `diagnosticsPrintDefault` prints and also sets `RiLastError` for errors.

Construction-time reports: `CRendererContext`'s constructor runs `beginRenderer()`
(`rendererContext.cpp:112`), which can report from `initFiles`, `initNetwork` and the declarations.
Today those reports reach the new context's `RiErrorPrint` (the base constructor already assigned
the global). `CRibOut`'s constructors report nothing.

The archive path: `CRendererContext::RiArchiveBeginV` saves the global into `savedRenderMan` and
assigns a new `CRibOut` (`rendererContext.cpp:6224-6226`); `RiArchiveEnd` (`ri.cpp:2136-2146`), when
`archiveNesting` reaches 0 and `savedRenderMan != NULL`, deletes the writer (whose base destructor
clears the global and detaches), restores the saved context and re-attaches; the renderer's
`RiArchiveEnd` clears `savedRenderMan` (`rendererContext.cpp:6231-6233`).

### M7. The writer mode has no `-ribout` flag

`src/orender/orender.cpp` always passes a `#...` string to `RiBegin` (`:174`, `:265`, `:877`), so
`orender` never enters writer mode. The writer is reached by `RiBegin(<plain file name>)` (C, Python,
Lua clients and the writer tests), by `RiBegin(NULL)` under `OPENRENDER_RUNPROGRAM`, and by
`ArchiveBegin` inside a renderer session. The spec's "`orender -ribout`" wording (US1 independent
test, edge case "Two back-ends") is corrected in the spec's starting point; the requirement is
unchanged.

### M8. Nothing tests `IfBegin`

No RIB under `tests/` or `examples/` and no test source uses `IfBegin`/`ElseIf` (grep). The site page
`docs/site/content/manual/reference/conditional-rib.md` is a 7-line stub. US2-2 therefore needs a
characterization test written first, green on the starting tree by design.

### M9. `CDelayedData`

`geometry/delayed.h:34-55`; `delayed.h:31` includes `object.h` for `CDelayedObject`/
`CDelayedInstance`, not for `CDelayedData`. Users: `rib.y:2722`, `ri.cpp:1655,1662,1771,1783`,
`render/rendererProcedures.cpp:28`, `parse/procDynamicLoadHook.h:13` (comment),
`tests/unit/csg/test_procedural_operand.cpp:115`, `tests/unit/sampling/test_object_ids.cpp:185`.
Geometry itself never names it.

### M10. The compiler's borrowed tables

`llvmEmitter.cpp:159-211` builds `kOpcodeParamTable` (text, parameter bits) by including
`scriptFunctions.h` and `scriptOpcodes.h` under local `DEF*` macros, and `kAllFunctionMnemonics`
(text) from `scriptFunctions.h` alone. The include chain is `scriptFunctions.h` → `shaderFunctions.h`
→ `giFunctions.h` and `scriptOpcodes.h` → `shaderOpcodes.h` → `giOpcodes.h`: 321 function rows
(118 + 191 + 12, two of them the `"XXX"` DSO rows at `scriptFunctions.h:1162,1193`) and 96 opcode
rows (78 + 15 + 3). `giFunctions.h` has `#ifndef INIT_SHADING` blocks around macro bodies; the
emitter sees the non-`INIT_SHADING` expansion.

Depfiles of the compiler's objects (pre-026 build): the only headers resolved under
`src/libshader/shading` are those six, all from `llvmEmitter.cpp.o.d`; nothing resolves under
`src/ri`. (A stale `dso_generated.cpp.o.d` from before spec 037 also lists `src/ri/parse/dso.h` and
`src/ri/core/ri_config.h`; that object is no longer built.) The include path still lists `src/ri`,
eight of its subdirectories (`compiler/CMakeLists.txt:8-17`, directory-scoped), `src/ri` again
(`:74`) and `src/libshader/shading` (`:82`). `logging.h` resolves to `src/include/` (Infra).

The two existing table tests: `LibShader_UsedParametersTable` (`src/libshader/tests/test_used_parameters_table.cpp`)
compares `kOpcodeParamTable` with its own expansion of the same two headers (`:49-50`), so today it
cannot fail on drift; `LibShader_OpcodeCoverage` (`test_opcode_coverage.cpp:78-90`) checks that
`kHandledOpcodes` covers `kAllFunctionMnemonics` minus `"XXX"`.

### M11. How a DSO call reaches the JIT today

Measured with the existing `oshader` (pre-026 build; the compiler front end is unchanged by 026):

```text
probe.sl      ->  DSO  probe_add ("f=ff") temporary_0 0.25 0.5        (uniform call)
probe_vary.sl ->  vufloat  temporary_0 k
                  DSO  probe_add ("f=ff") v_1 s temporary_0           (varying call)
                  ...
                  if temporary_0 #!Label3
                  vufloat  temporary_1 0.25
                  DSO  probe_add ("f=ff") w t temporary_1             (varying call under an if)
```

`oshader --jit` on `probe.sl` exits 3 with `llvmEmitter: JIT coverage gap for 'probe': builtin 'DSO'
has no emitFunction() case`. So:
- the compiler writes one text form for both formats (`expression.cpp:1559-1563`), and `.rslo` output
  also goes through the IR builder and the passes (`rslo.cpp:1668-1694`);
- `CIRBuilder::parseLine` (`irBuilder.cpp:258-311`) reads that line as opcode `DSO`, result
  `probe_add`, and operands `("f=ff")`, destination, arguments;
- a uniform argument of a varying call is copied into a varying temporary before the call
  (`vufloat temporary_0 k`; the promotion is `expression.cpp:1534-1557`), so every operand of a
  varying DSO call is varying, and the interpreter's step (`numItems * bytesPerItem`,
  `scriptFunctions.h:1145-1148`) is the per-vertex size. An operand whose prototype letter is upper
  case stays uniform (`expression.cpp:1538-1542`); whether a shadeop prototype can contain one is
  checked in R14.

The interpreter runs a DSO row like every `DEFFUNC` (`execute.cpp:601-625`): once when the
instruction is uniform; per vertex otherwise, skipping vertices whose tag is set when `numPassive != 0`,
and advancing every operand for every vertex. The thread index is the context's `thread`
(`scriptFunctions.h:1132-1134`), and `0` in the init-block executor (`render/init.cpp:31`).

`VarDesc` in the emitter already carries the per-vertex byte size of a variable whether uniform or
varying (`llvmEmitter.cpp:379-383`, used for parameter lists, GitHub #55), and the JIT reaches the
executing context through `libshader::activeContext()` (`shading/activeContext.h`).

### M12. Binding at load

`.rslo`: the loader resolves a DSO by `libshader::lookupDSO(name, prototype)` (`shading/rslo.y:648,1865`),
which calls `CRendererServices::getDSO` (`serviceLookup.cpp:42-48`; `rendererServices.h:404`, unchanged
in services 2.0.0) → `CRenderer::getDSO` (`rendererFiles.cpp:560-584`); the operand lives until
`RiEnd`. A missing operand is `rsloerror("Failed to locate DSO function\n")` (`rslo.y:1876`), printed
as `warning(CODE_BADFILE, "Error in shader \"%s\" (%d) (\"%s\") (v%d.%d.%d, .rslo format %d.%d.%d)\n", ...)`
(`rslo.y:2097-2100`).

`.slo`: `parseSloShader` (`shading/sloLoader.cpp:139`, moved there by spec 026) builds the `CShader`
from the module's metadata and calls `CLLVMJitEngine::compileShader` (`llvmJit.cpp:132-241`), which
caches the compiled module by shader name for the process and checks the format record first
(`:163-177`). `getShader` falls back to the `.rslo` whenever `parseSloShader` returns `nullptr`
(`rendererFiles.cpp:395-417`).

`.slo` format: `common/sloFormat.h`, 2.0.0, named metadata `openrender.slo.format`; a loader reads a
module with the same MAJOR and a MINOR not greater than its own.

### M13. `precomp`

`src/precomp/precomp.cpp:35-36` includes `ri/hiders/reyes.h` and `ri/render/renderer.h`. It uses the
`RASTER_*` constants (`reyes.h:42-55`); `CRenderer::depthFilter` and `DEPTH_MID` appear only inside
printed strings (`:399`). It writes `../ri/hiders/stochasticPrimitives.h` relative to the working
directory (`:384-388`).

Run from a scratch tree (pre-026 build), it writes 14,029 lines that differ from the tracked
`src/ri/hiders/stochasticPrimitives.h` (13,248 lines) from the first byte: the tracked file carries
the project's license header and was reformatted by `aa3c684d` ("style: apply repo .clang-format to
all tracked C/C++ sources"), e.g. `switch ((grid->flags` against precomp's `switch((grid->flags`.
So "reproduces the tracked file byte for byte" (FR-023, US7-1, SC-009) is false on the starting
tree (Q4).

### M14. DB-024's count

The check's CONSUMER section lists 66 edges on the pre-026 build: 28 from `src/libshader/tests/*`,
and 38 from the tools (`oshader.cpp` 3, `sloinfo.cpp` 3, `precomp.cpp` 1, `orender.cpp` 1,
`otexmake.cpp` 1, `previewContext.cpp` 9, `ribGeometryContext.cpp` 10, `ribGeometryContext.h` 5,
`dataLoad.cpp` 5). The registry says 36. The two `ri.h` edges of `orender.cpp`/`otexmake.cpp` would
account for the difference, but that is a guess; Phase 0 re-measures on a fresh build and the
narrowed entry states what it counts. Consumer includes of Core headers (the preview's `renderer.h`)
are not CONSUMER lines.

---

## R. Decisions

### R1. The declaration table moves into Graphics State

- **Decision**: a new Graphics State unit `src/ri/state/declarations.{h,cpp}` (`CDeclarations`,
  static interface, [contracts/scene-services.md](contracts/scene-services.md)) owns what M2 items 1,
  2, 4 and 6 are: the declared-variable trie, the variable list, the global-variable array and its
  numbering, the standard declared set in today's order, the channel registry's begin/end, and the
  installation of Graphics State's two hooks. It is reference counted: the first `acquire()` builds
  it, the last `release()` destroys it, an extra `release()` does nothing. A "global variable added"
  observer, installed by the core, replaces the shading-context notification inside
  `makeGlobalVariable`. The core keeps the coordinate systems and the global identifiers (M2 items 3
  and 5).
- **Rationale**: every consumer of the table may depend on Graphics State: the Scene Interface,
  Geometry (`blobby.cpp`), Shading (today through the services) and Graphics State's own parameter
  lists. A Scene-owned table would be reachable only by the Scene Interface and the core. The
  standard set exists once, so the renderer, the writer and the preview cannot drift. The reference
  count makes the teardown idempotent (spec edge case "Declarations before `RiBegin`") and changes
  behaviour only in the two sequences of M3 that would use freed memory today.
- **Alternatives**: keep the table in the core and give the writer and preview their own copy (two
  standard sets that can drift; the copy would still have to number globals identically); put it in
  the Scene Interface (Geometry and Shading could never reach it without the core).

### R2. The Scene-owned services, and who installs them

- **Decision**: `src/ri/parse/riServices.{h,cpp}` declares `CRiDeclarationService` (retrieve,
  declare) and `CRiFileSearchService` (locate), the null-safe calls the grammar, the lexer and
  `ri.cpp` make (`riRetrieveVariable`, `riLocateFile`), install functions that return the previous
  service so a nested session restores it, and the Scene's own standard implementations with a
  scoped `CRiStandardSession` (acquire the State table, install the standard services; the end
  restores what was there). The full renderer installs its implementation in
  `CRenderer::initDeclarations()` and restores the previous one in `shutdownDeclarations()`, the
  precedent of `variableLookup.h` (`rendererDeclarations.cpp:312-315`). The writer branch of
  `RiBegin`, the preview and the data viewer use `CRiStandardSession`. Each sequence of M3 keeps its
  scope exactly: the writer still has declarations only around `.orenderrc`, and the writer tests'
  own `CRenderer::initDeclarations()` calls keep working unchanged.
- **Rationale**: FR-001/FR-002. One implementation shared by the writer and the preview (FR-002
  leaves that open): the standard declarations plus the search-path walk with no network. With no
  service installed, `riRetrieveVariable` answers `nullptr` (the parser then reports "Parameter ...
  is not declared") and `riLocateFile` finds only a name with a directory separator that exists.
- **Alternatives**: a declaration service per back-end, registered next to its factory (more
  registration surface, no second implementation to justify it); `ri.cpp` calling Graphics State
  directly with no service (fails FR-001's Scene-owned service).

### R3. The standard file search reproduces the writer's behaviour exactly

- **Decision**: `CRiStandardFileSearch::locate` is M4's algorithm without the two network steps,
  with the bare-name fallback in the working directory that the empty `temporaryPath` gives today,
  the same `info(CODE_RESOLUTION, ...)` texts and the normalized retry. The renderer's
  implementation forwards to `CRenderer::locateFile`, so the network mapping stays the renderer's.
- **Rationale**: US1-3; byte-identical diagnostics (FR-005). The network is reached only by the
  core's implementation.
- **Alternatives**: a network hook inside the Scene implementation (nothing outside the renderer
  has a network).

### R4. `CRenderer`'s members stay as out-of-line forwarders (FR-026)

- **Decision**: `CRenderer::retrieveVariable`, `declareVariable` and `locateFile` stay, defined in
  their Core files, forwarding to `CDeclarations` (and, for `locateFile`, keeping its own body).
  `initDeclarations`/`shutdownDeclarations` stay as the renderer's session entry. `makeGlobalVariable`
  becomes the observer (`notifyGlobalVariable`): the numbering moves to Graphics State, the real
  body keeps only the context loop, the stub becomes a no-op. The DB-004 lines `:49-50` stay valid
  and registered; the DB-017 line `:340` (`object.cpp -> findCoordinateSystem`) is untouched.
- **Rationale**: Geometry's and Shading's calls do not change, so no Geometry file is edited and
  the Geometry metric stays 48/15. An inline forwarder would turn the two DB-004 link lines into
  Geometry → Graphics State edges (allowed) and make them STALE; that is Geometry's remediation
  (specs 029-031), not this spec's.
- **Alternatives**: inline forwarders (deletes two DB-004 lines; touches `renderer.h`'s consumers'
  link edges); calling `CDeclarations` from `blobby.cpp` (a Geometry edit, out of scope).

### R5. `riHooks.h` and `CDelayedData` move into the Scene Interface

- **Decision**: `git mv src/ri/render/riHooks.h src/ri/parse/riHooks.h`, text unchanged except the
  comment that names its location; the three registration functions keep their names and behaviour.
  `CDelayedData` moves to a new `src/ri/parse/delayedData.h` that includes only `common/global.h`
  (and `common/algebra.h` if `vector` needs it); `geometry/delayed.h` drops the class and keeps
  `object.h`. `rib.y`, `ri.cpp`, `rendererProcedures.cpp`, `procDynamicLoadHook.h`'s comment and the
  two tests use the new header; `delayed.h` must not include it (Geometry → Scene is not allowed).
  Whether `rib.y` or `ri.cpp` needed `object.h` through `delayed.h` is settled by the build; any type
  they still need comes from a State or Geometry header named directly (Scene may depend on both).
- **Rationale**: FR-006-FR-008. The five test files that include `"riHooks.h"` by bare name
  (`tests/unit/csg/csgTestUtils.h:45` and four more, M6 list) compile unchanged if their include
  path contains `src/ri/parse`; `ribRender`'s public include directories do. If one does not, its
  CMake include line is the edit (TEST-APPROVAL).
- **Alternatives**: a forwarding `render/riHooks.h` shim (keeps a Core-classified file the Scene
  depends on).

### R6. The `if` evaluator becomes a Scene function taking the context

- **Decision**: `ifexpr.y` defines `int riEvaluateCondition(CRiInterface *context, const char *expr)`
  (declared in a new `src/ri/parse/riCondition.h`), using `context->getAttributes(TRUE)` and
  `context->getOptions()` exactly where it uses them today, including the copy-on-write side effect
  of `getAttributes(TRUE)`, and treating a `nullptr` answer as "not found". `CRendererContext::ifParse`
  and its declaration (`rendererContext.h:274`) are deleted; `RiIfBeginV`/`RiElseIfV` call the new
  function with `this`. The generated parser includes no core header. `ifexpr_generated.cpp` joins
  `ribOut` and `ribVector` (Q6); nothing in those builds calls it, so `if` blocks behave there as now.
- **Rationale**: FR-004. Passing the context avoids a new dependency on the global.
- **Alternatives**: read the active-context global inside the evaluator (an implicit dependency the
  spec is removing elsewhere).

### R7. Explicit installation of the active context

- **Decision** ([contracts/active-context.md](contracts/active-context.md)): `CRiInterface`'s
  constructor stops assigning the global and attaching diagnostics. `riInstallContext(ctx)` saves
  the current active context in `ctx`, makes `ctx` active and attaches; `riUninstallContext(ctx)`,
  when `ctx` is active, restores the saved one and re-attaches (or detaches when it is `nullptr`);
  when `ctx` is installed but not active it only clears the mark. The base destructor uninstalls an
  installed context, after the derived destructor has reported (end-of-render statistics still reach
  the context's handler). `RiBegin`'s three branches, `RiArchiveBegin` in the renderer (one call
  replacing `savedRenderMan`), the preview session, the data viewer and the tests install
  explicitly. `RiArchiveEnd` deletes the archive context when the active context has a saved
  previous one (today `savedRenderMan != NULL`). `RiContext` keeps its body. `savedRenderMan` and
  the two `extern`s in `rendererContext.cpp` go.
- **Rationale**: FR-009-FR-011, US3. The previous-context link replaces the archive's private save.
- **The construction window** (M6): reports raised while `CRendererContext` constructs (only the
  network and file setup can report) reach the sink installed before `RiBegin`. The text, stream and
  severity are the same (`RiErrorPrint` prints through `diagnosticsPrintDefault`); `RiLastError` is
  not set for them. Q5 asks whether that is acceptable.
- **Alternatives**: a two-phase begin (`CRiInterface::begin()` after install) moves
  `beginRenderer()` out of the constructor and changes every capture context in the tests; a
  thread-local "constructing" pointer reintroduces a hidden global.

### R8. The preview registers a named back-end and begins a light session

- **Decision** ([contracts/backend-registration.md](contracts/backend-registration.md)): `riHooks.h`
  gains `RiRegisterBackEnd(const char *key, TRiBackEndFactory)`, and `rib.h` gains
  `RiBeginBackEnd(key, argument)` / `RiEndBackEnd(context)`: a standard session, `RiInit` (what
  `RiBeginLite` does today, which is then deleted), the registered factory, and `riInstallContext`;
  the end deletes the context (which uninstalls it) and ends the standard session. No `.orenderrc`,
  no network, no `RiTini` (today's preview never calls it). The preview's registrar is a static
  initializer in `previewContext.cpp`, the translation unit of `ribpreview_load`, so linking the
  entry point links the registration. An unknown key reports `Back-end "<key>" is not available in
  this build (none registered)` and returns `nullptr`. `ri.cpp` contains no back-end name.
- **Rationale**: FR-012-FR-014, rule 4. Going through `RiBegin` would read `.orenderrc` and `RiEnd`
  would run `RiTini` and the full teardown, both new behaviour for the preview.
- **Alternatives**: a third fixed slot `RiRegisterPreviewFactory` (the dispatch layer would name the
  preview); a `#preview` key in `RiBegin`'s name string (pulls in `.orenderrc`).

### R9. Build targets

- **Decision**: `ribOut_sources` loses the six Core files and gains `parse/riServices.cpp`,
  `state/declarations.cpp` and `ifexpr_generated.cpp`. `ribVector` keeps only the Core files
  Geometry and Hiders need (M5): `rendererDeclarations.cpp` (forwarders for `blobby.cpp`),
  `rendererFileSearch.cpp` with `rendererNetwork.cpp` and `rendererMutexes.cpp` (for
  `blobbyRepeller.cpp`), `rendererStatics.cpp`, and the four stubs. `rendererClipping.cpp` and
  `resource.cpp`, which nothing in `ribVector` or the preview references, leave it (Q7). The list is
  pinned in `tests/architecture/ribvector-core-residual.txt` with the referencing Geometry/Hiders
  file of each entry; a check fails if `ribVector` compiles a Core file not in the list, or if a
  listed file is no longer compiled (stale, so the list only shrinks). The comments of
  `src/ri/CMakeLists.txt:283-363` and `:477-514` are rewritten (FR-016). The registry lists the residual
  files under DB-004 and DB-011.
- **Rationale**: FR-015, SC-001, Q2 of the clarification.

### R10. The compiler owns its tables

- **Decision** ([contracts/compiler-tables.md](contracts/compiler-tables.md)): a compiler-owned
  `src/libshader/compiler/builtinTables.cpp` defines `kOpcodeParamTable` and
  `kAllFunctionMnemonics` with the same rows, in the same order, both `"XXX"` rows included, written
  with the parameter expressions spelled as in the engine (`PARAMETER_* | ...`, from
  `common/rendererc.h`). It is generated once by the drift test's `--emit` mode (which stringifies
  each row's `par` argument) and never edited by hand; its header comment says how to regenerate.
  `llvmEmitter.cpp` loses both table blocks and its two engine includes. The compiler's
  `CMakeLists.txt` loses every include directory under `src/ri` and `src/libshader/shading`.
- **The drift test** (`LibShader_CompilerTableDrift`, consumer, includes both sides) compares row by
  row: `kOpcodeParamTable` against the engine's `scriptFunctions.h` + `scriptOpcodes.h` expansion,
  `kAllFunctionMnemonics` against the `scriptFunctions.h` expansion, and the compiler's
  `kAllOpcodeMnemonics` (`compiler/opcodes.cpp:191`) against the engine's opcode texts under the
  relation R10a measures first. A mutation self-check runs the same comparator on a copy with one
  changed row and expects a failure. Because both tables come from one header on the starting tree,
  the test cannot be red there; it is registered `DISABLED` until `builtinTables.cpp` exists (037's
  pattern), and its red evidence is the mutation check (Q3). `LibShader_UsedParametersTable` keeps
  its meaning and, from then on, compares two independent tables.
- **R10a (Phase 1 measurement)**: whether `kAllOpcodeMnemonics` equals the engine's opcode text set
  exactly or with a known difference (control-flow mnemonics, dead opcodes); the comparator encodes
  the measured relation with every exception named.
- **Rationale**: FR-017, FR-018, Clarification Q1. A table generated by the build from the engine
  headers would keep the compiler's build dependent on the engine's sources.
- **Alternatives**: split the engine's `DEF*` rows into a shared metadata header both include (a
  rewrite of every row of six engine headers); generate the table at build time.

### R11. The mechanical include check for the compiler

- **Decision**: a `cmake -P` check (`Compiler_IncludeBoundary`, label `architecture`) receives the
  compiler target's `INCLUDE_DIRECTORIES` through a generator expression and the list of its
  sources, fails if any directory is under `src/ri` or `src/libshader/shading`, and resolves each
  `#include "..."` of the compiler's sources and generated parser against that path (compiler
  directory first), failing unless it lands in `src/libshader/compiler`, the compiler's build
  directory, `src/common`, `src/include`, `src/shadeop`, `src/libshader/include`, the top-level
  build directory (`config.h`) or LLVM. Red on the starting tree (M10).
- **Rationale**: SC-007. `src/include/` (`logging.h`) and the generated files must be in the allowed
  list; the spec's list omits them (corrected in its starting point).

### R12. The JIT DSO call

- **Decision** ([contracts/jit-dso-call.md](contracts/jit-dso-call.md)):
  - The IR is not changed: the emitter decodes the existing `DSO` form (result field = operand name,
    first operand = quoted prototype, then the destination unless the prototype starts with `o`,
    then the arguments) in one helper, so the passes and the `.rslo` text stay as they are.
    The `DSO` case runs before `emitFunction`'s destination prelude (`llvmEmitter.cpp:1101-1105`),
    which would read the operand name as the destination and fall back to stride 3.
  - The emitter numbers each distinct (name, prototype) of the function, emits a module global
    `@"<shader>.shadeops" = [N x ptr] zeroinitializer` and the named metadata
    `openrender.shader.shadeops` (one `!{name, prototype}` node per slot, in first-use order), only
    when the module has a DSO call.
  - Each call is `op_dso_call(slot, isVoid, inInit, argc, argv, steps, n, tags)`: `argv`/`steps`
    are stack arrays of the operands' pointers and per-vertex byte sizes (`VarDesc` item bytes,
    M11), `n`/`tags` come from `collapseArgs` as for every other call, `inInit` is 1 in the init
    function.
  - `op_dso_call` (Shading, `rslOps.cpp`) mirrors `DEFFUNC` (M11): once if uniform; otherwise per
    vertex, skipping tagged vertices, advancing every operand per vertex; `exec(data, argc, argv)`
    with `argv[0]` the destination (or unused for a void operand) and `data =
    shadeopOperandData(operand, inInit ? 0 : activeContext()->thread)`.
  - Binding: `CLLVMJitEngine` keeps each module's slot list and table address from `compileShader`;
    `parseSloShader` binds on every load (the module cache outlives a session, the operands do not)
    by calling `libshader::lookupDSO(name, prototype)` per slot in slot order, before the shader is
    returned. A missing operand fails the load (Q2). `getShader` consults the session's
    `globalFiles` cache before loading (`rendererFiles.cpp:386-388`), so a shader is bound once per
    session, before any of its instances exist in that session; no render thread executes the
    module while its slots are written.
  - `kHandledOpcodes` gains `"DSO"`; `LibShader_OpcodeCoverage` gains an explicit check that `DSO`
    is handled and `LibShader_GateHardening` a positive case that `probe.sl` compiles with `--jit`
    and the module declares `op_dso_call` (FR-020).
  - `CRendererServices` is unchanged (still 2.0.0); only `getDSO`'s comment gains "and the `.slo`
    loader", a PATCH under its own rule (Q8). No flat-namespace import is added: `op_dso_call` is
    defined in `libshader_shading` and calls `libRIBShadeop`, which it links.
- **Rationale**: FR-020-FR-022. Decoding the existing form keeps `.rslo` bytes for every shader;
  binding by slot at load satisfies FR-021 and keeps `lookupDSO` a load-time call (its contract).
- **Alternatives**: restructure `IRInstr` for DSO lines (risks the passes and the `.rslo` text);
  resolve lazily at the first call (breaks FR-021 and calls `getDSO` from render threads).

### R13. `.slo` format version

- **Decision** (Q1): the loader's `SLO_FORMAT_MINOR` becomes 1; the emitter writes 2.1.0 only in a
  module that calls a DSO operand and 2.0.0 otherwise. A 2.0 loader rejects a module with DSO calls
  before linking it; every module without one keeps its bytes (FR-019) and loads in both.
- **Rationale**: the format rule (`sloFormat.h`): a MINOR is a backward-compatible addition the
  older loader must refuse.
- **What it costs** (measured): the emitter writes `SLO_FORMAT_MAJOR/MINOR/PATCH` directly
  (`llvmEmitter.cpp:502-503`), and the approved test `LibShader_Format_slo_emitted`
  (`src/libshader/tests/test_format_versions.cpp:276-300`) compiles a probe without DSO calls and
  expects exactly `{SLO_FORMAT_MAJOR, SLO_FORMAT_MINOR, SLO_FORMAT_PATCH}`; its `newer_minor` case
  (`:318`) builds `SLO_FORMAT_MINOR + 1`. With (a) the record becomes "the lowest reader version the
  module needs", not the writer's version. So (a) needs: a second constant in `sloFormat.h`
  (`SLO_FORMAT_MINOR_BASE 0`, what a module without DSO calls records) with the comment's meaning
  rewritten; the emitter choosing between the two; and an edit of `slo_emitted` (expect the base
  record for its probe) plus a new case (a probe with a DSO call records 2.1.0), both TEST-APPROVAL.
  `test_stale_shaders.cpp` names no format constant. Option (b) keeps that test unedited but rewrites
  the record of every shipped `.slo` (FR-019).

### R14. Checks the DSO tests must make before calling the interpreter the reference

- **R14a**: two identical DSO calls in a row, a value-returning call whose result is unused, and a
  call followed by a read of its destination, compiled to `.rslo`: check that the passes (ConstFold,
  UniformLifting, CSE, DCE, CallSites) neither merge, drop nor reorder a DSO line (the IR records the
  operand name as the result and the destination as a read). If one does, that is a defect of the
  `.rslo` path today; it is reported to the maintainer, not reproduced silently or fixed in passing.
- **R14b**: whether a shadeop prototype (`shadeopDeclaration.cpp`) can produce an upper-case letter.
  If it can, a uniform operand of a varying call is advanced per vertex by the interpreter, past its
  single value; the same reporting rule applies.
- **R14c**: the JIT's uniform decision for a DSO call (all operand strides 0, through `collapseArgs`)
  equals the `.rslo` loader's `opcodeUniform` (`shading/rslo.y`); `probe.sl` (a uniform call) and
  `probe_vary.sl` cover both answers.

### R15. Tools

- **`precomp`** ([contracts/tools.md](contracts/tools.md)): the `RASTER_*` constants (`reyes.h:42-55`)
  move to a Hiders-owned constants header `src/ri/hiders/rasterFlags.h` with no includes, which
  `reyes.h` includes; `precomp.cpp` includes it instead of `reyes.h` and `renderer.h` (Q4 for the
  location and the criterion). Its output is checked against the unmodified tool's output captured in
  Phase 1, run from a scratch tree.
- **Data viewer**: `CDataDocument` holds a `CRiStandardSession` and an installed `CRiInterface` it
  owns; the destructor deletes the context (which uninstalls it) and ends the session. Its readers'
  reach into Hiders and Texturing is unchanged (DB-025).

### R16. Test discipline and approvals

Principle III and the project's approval rule: every new test, and every edit of an approved test or
reference, stops for the maintainer's approval (TEST-APPROVAL) before the phase's implementation
tasks. Characterization tests (the `if` evaluator, the writer's text, the preview's extraction, the
`precomp` output, the `.rslo`/`.slo` byte snapshot) are green on the starting tree by design and
pinned before any change; their references are captured once, from the unmodified tree, by a build
target that refuses to overwrite (037's `plugin-abi-capture-baselines` pattern), never by a ctest.
The full list is in [contracts/checks.md](contracts/checks.md).

---

## Q. Questions for the maintainer

| # | Question | Options | Recommendation |
|---|---|---|---|
| Q1 | `.slo` format record (FR-019 says every shipped `.slo` stays byte-identical) | (a) write 2.1.0 only in modules with a DSO call, through a second constant `SLO_FORMAT_MINOR_BASE` (R13), editing the approved `LibShader_Format_slo_emitted` case and adding a DSO case; (b) bump every module to 2.1.0 and except the record from FR-019 (no test edit) | **(a)** (keeps FR-019; one approved-test edit) |
| Q2 | A `.slo` whose operand is missing at load | (a) warn in the `.rslo` loader's form (`CODE_BADFILE`, "Failed to locate DSO function", operand name and prototype) and return `nullptr`, so `getShader` falls back to the `.rslo` as for any failed `.slo` load and the interpreter then prints its own message; (b) hard failure, no fallback | **(a)**: one load-failure rule for `.slo`; SC-008's "interpreter's message" is then literally printed when the `.rslo` exists |
| Q3 | The drift test cannot fail on the starting tree (both tables from one header): FR-029/SC-011 | (a) red evidence = the mutation self-check; registered `DISABLED` until `builtinTables.cpp` exists; the include check (R11) is US5's red test; (b) leave FR-029 as written | **(a)**, with FR-029/SC-011 amended to say so |
| Q4 | `precomp`: the tracked header is not raw tool output (M13), and SC-009 forbids "hider headers" while FR-023 allows a Hiders-owned constants header | location: (a) `src/ri/hiders/rasterFlags.h`; (b) `src/common/rasterFlags.h`; (c) local copies in `precomp.cpp` plus a consumer drift check. Criterion: (i) output after = output before; (ii) after formatting and the license header, equals the tracked file | **(a) + (i)**; SC-009 and US7-1 reworded to "no `reyes.h`, no core header; output identical to the unmodified tool's" |
| Q5 | Reports raised while `CRendererContext` constructs no longer set `RiLastError` (same text, stream, severity) | (a) accept and document; (b) a two-phase begin | **(a)** |
| Q6 | `ifexpr_generated.cpp` in `ribOut`/`ribVector` | (a) compile it in all three (a Scene file with no core dependency, unused there); (b) keep the exclusion | **(a)** |
| Q7 | `rendererClipping.cpp`, `resource.cpp` in `ribVector` (unreferenced; removing them removes exported symbols of the installed `libRIBVector`) | (a) remove, note in NEWS; (b) pin as residual | **(a)** |
| Q8 | `getDSO`'s comment now covers the `.slo` loader | (a) PATCH, services 2.0.1, with a history line; (b) no version change | **(a)**, the services header's own rule |
| Q9 | Extra parity case: a DSO call in a parameter default (the init section, thread index 0) | (a) add to FR-022's matrix; (b) not in this spec | **(a)**, one more shader in the same test |
| Q10 | The allowed include roots of US5-AS1 and SC-007 omit `src/include/` (`logging.h`), `src/libshader/include/` and the build directories (generated parser, `config.h`), which the compiler needs (M10, R11) | (a) amend US5-AS1 and SC-007 to R11's list; (b) keep them as written (the check would fail on a correct tree) | **(a)** |
