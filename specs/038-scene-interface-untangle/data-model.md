# Data Model: Scene Interface Untangle (spec 038)

The spec's Key Entities, with their owner, fields, rules and life cycle. The C++ declarations are in
[contracts/](contracts/); the reasons in [research.md](research.md).

## 1. Declaration table (Graphics State, `CDeclarations`)

| Field | Meaning |
|---|---|
| declared variables | trie name → `CVariable *` (moved from `CRenderer::declaredVariables`) |
| variable list | every declared `CVariable`, newest first (from `CRenderer::variables`) |
| global variables | array, index = `CVariable::entry` for `STORAGE_GLOBAL` (from `CRenderer::globalVariables`) |
| reference count | acquirers alive |
| global observer | function called after a global is added (the core: notify shading contexts) |

Rules: the standard set is declared on the first acquire, in today's order, so every `VARIABLE_*`
index is unchanged; redeclaration rules and diagnostics as `CRenderer::declareVariable` today; a
global's entry is its index in the array. Life cycle: `none --acquire--> active(1) --acquire-->
active(n+1) --release--> active(n) ... active(1) --release--> none`; a release in `none` does nothing.
While active it has the channel registry begun and Graphics State's two hooks pointing at it.

Not in it (stay in the core): named coordinate systems, global identifiers.

## 2. Declaration service and file-search service (Scene Interface)

| Service | Operations | Implementations |
|---|---|---|
| `CRiDeclarationService` | retrieve(name), declare(name, type) | the core's (renderer session); the standard (`CDeclarations`) |
| `CRiFileSearchService` | locate(result, name, searchpath, tryNormalize) | the core's (`CRenderer::locateFile`, with the network); the standard (no network, research R3) |

At most one of each installed; installing returns the previous one, which the installer restores at
its end. With none installed: retrieve → not declared; locate → only an existing path with a
directory separator.

## 3. Standard session (Scene Interface, `CRiStandardSession`)

Scoped object: acquire the table, install the two standard services; destruction restores the
previous services and releases the table. Used by: `RiBegin`'s writer branch (around `.orenderrc`),
`RiBeginBackEnd`/`RiEndBackEnd` (the preview), `CDataDocument` (the data viewer).

## 4. Active context (Scene Interface)

| Field | Meaning |
|---|---|
| `renderMan` | the active `CRiInterface *` (unchanged global, assigned only inside `src/ri/parse/`) |
| `CRiInterface::riPrevious` | the context active when this one was installed |
| `CRiInterface::riInstalled` | installed mark |
| diagnostics attachment | `riDiagnosticsAttach`/`Detach` (spec 023), idempotent |

Life cycle of one context: `constructed --riInstallContext--> installed+active --(RiContext to
another)--> installed+inactive --riUninstallContext or delete--> uninstalled --delete--> gone`.
Uninstalling an active context reactivates `riPrevious` (attach if non-null, detach if null).
Nesting depth used: 2 (renderer, archive writer).

## 5. Back-end registration (Scene Interface)

| Back-end | Registered by | Through | Selected by |
|---|---|---|---|
| renderer | `rendererContext.cpp` static initializer | `RiRegisterDefaultContextFactory` (unchanged) | `RiBegin("#...")` / `RiBegin(NULL)` |
| RIB writer | `ribOut.cpp` static initializer | `RiRegisterRibOutFactory` (unchanged) | `RiBegin(<file>)`, `OPENRENDER_RUNPROGRAM` |
| preview | `previewContext.cpp` static initializer | `RiRegisterBackEnd("preview", f)` (new) | `RiBeginBackEnd("preview", arg)` |
| test override | a test | `RiSetContextFactory` (unchanged) | `RiBegin` full-render mode |

Rules: registration before `main` (not thread-safe, as today); the dispatch layer names no back-end.

## 6. Procedural record (Scene Interface, `CDelayedData`)

`generator` (malloc'd string), `helper` (malloc'd string), `bmin`, `bmax`. Created by the grammar's
`Procedural` action, read by `RiProcDelayedReadArchive`/`RiProcRunProgram`/`RiProcDynamicLoad` and
the core's DynamicLoad hook; freed by its destructor. No Geometry dependency.

## 7. Compiler table set (Shading, compiler)

| Table | Rows | Source after |
|---|---|---|
| `kOpcodeParamTable` | (text, PARAMETER bits), 321 function rows then 96 opcode rows, `{nullptr, 0}` | `compiler/builtinTables.cpp` |
| `kAllFunctionMnemonics` | 321 texts (two `"XXX"`), `nullptr` | `compiler/builtinTables.cpp` |
| `kAllOpcodeMnemonics` | the compiler's opcode texts | `compiler/opcodes.cpp` (unchanged) |

Rule: row-for-row equal to the engine's expansion (drift test); regenerated, never hand-edited.

## 8. DSO call slot (`.slo` module)

| Field | Where |
|---|---|
| (name, prototype) per slot | named metadata `openrender.shader.shadeops`, slot order |
| bound `CDSO *` per slot | `@"<shader>.shadeops"[k]`, written at every load |
| call site | `op_dso_call(slot entry, isVoid, inInit, argc, argv, steps, n, tags)` |

Life cycle: compiled once per process (module cache); bound at every `parseSloShader` of that
shader (the operands live until `RiEnd`); a missing operand fails the load.

## 9. `.slo` format record

`openrender.slo.format` = 2.0.0 for a module without DSO calls (unchanged bytes;
`SLO_FORMAT_MINOR_BASE`), 2.1.0 with one (`SLO_FORMAT_MINOR`). The record means the lowest reader
version the module needs. Loader reads MAJOR 2, MINOR ≤ 1.

## 10. `ribVector` Core residual list

`tests/architecture/ribvector-core-residual.txt`: one line per Core file `ribVector` may still
compile, with the Geometry/Hiders file that needs it. Shrink-only: a new Core file fails, a stale line
fails.
