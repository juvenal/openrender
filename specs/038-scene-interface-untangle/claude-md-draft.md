# CLAUDE.md draft for spec 038 (for the maintainer to apply; no task edits `CLAUDE.md`)

Written 2026-11-04 (Phase 8, T135). Every text below is what to add or replace in `/Volumes/Projects/Development/CLI/openrender/CLAUDE.md`
as it reads on master `01501f10` plus the merges since. SC-010 holds for `CLAUDE.md` only once this draft is applied.

Checked and not part of the draft: the sweep count `419 scenes x 5 configurations` in the Testing block already
arrived with the merge; `CLAUDE.md` never says "the JIT has no DSO call path" in so many words, so there is nothing to
remove (the corrections to gotchas 3 and 28 below are the places where the new path matters).

## 1. Architecture: add after "### Plug-in ABIs (spec 037)"

```markdown
### Scene Interface services and the active context (spec 038)
The Scene Interface (`src/ri/parse/`, `src/ri/output/`, the generated parsers) includes no renderer-core
header and references no `CRenderer`/`CRendererContext` symbol (`SceneInterface_NoCoreInclude`, `_RibOutNoCore`).
The declared variables are Graphics State's `CDeclarations` (`src/ri/state/declarations.{h,cpp}`, reference
counted, acquired by `CRiStandardSession` or by `CRenderer::initDeclarations()`); the grammar reaches variable
lookup and file search through two Scene-owned services (`CRiDeclarationService`, `CRiFileSearchService`,
`parse/riServices.h`) that the renderer core implements (`CCoreDeclarationService`/`CCoreFileSearchService`) and
that a program without a renderer gets from `CRiStandardSession`. The `if` evaluator is `riEvaluateCondition`
(`parse/riCondition.h`). `riHooks.h` lives in `src/ri/parse/`. The active RI context is installed explicitly
(`riInstallContext`, `riUninstallContext`, `riPreviousContext`); a back-end registers a named factory
(`RiRegisterBackEnd`) and a program starts it with `RiBeginBackEnd`/`RiEndBackEnd` (the preview registers
itself as `"preview"`). `libRIBVector` compiles only the Core files in
`tests/architecture/ribvector-core-residual.txt`, a list that only shrinks. See gotchas 29 to 33.
```

## 2. Architecture: "### Attributes system", layer 4 (stale path)

Before: `4. Pre-declaration — \`initDeclarations()\` in \`src/ri/rendererDeclarations.cpp\` (required, ...)`
After: `4. Pre-declaration — \`CRenderer::initDeclarations()\` in \`src/ri/render/rendererDeclarations.cpp\`, which fills Graphics State's \`CDeclarations\` (\`src/ri/state/declarations.cpp\`) (required, ...)`

## 3. Known gotchas: two corrections

Gotcha 3 (JIT symbol dead-stripping), append after "...wire up `addProcessSymbol()` for real then, rather than assuming it's already wired.":

```markdown
   `op_dso_call` (spec 038, a `.slo` shader's call of a DSO operand) is one more JIT-only entry resolved the
   same way; it adds no flat-namespace import.
```

Gotcha 28 (Shading's boundary), replace the sentence "A new flat-namespace import of `libshader_shading` must be added to ... (4 symbols today) and to the DB-001 allowlist." by itself plus, at the end of the gotcha:

```markdown
   `getDSO` also serves the `.slo` loader since spec 038 (`sloLoader.cpp` binds the DSO operands of a JIT module
   with `libshader::lookupDSO`, on every load); that is a wording change of the member, so
   `kRendererServicesVersion` stays 2.0.0.
```

## 4. Known gotchas: new, after gotcha 28

```markdown
29. **The active RI context is installed explicitly (spec 038).** `CRiInterface`'s constructor assigns nothing:
   not `renderMan`, not the diagnostic sink. `riInstallContext(ctx)` makes it the active one (and attaches the
   diagnostics), `riUninstallContext(ctx)` / the destructor of an installed context hands both back to the
   previous context, `riPreviousContext(ctx)` answers which one that is, and archive nesting uses it
   (`savedRenderMan` is gone). Installing an installed context is a `CODE_BUG` report. Nothing outside
   `src/ri/parse/` may assign `renderMan` (`SceneInterface_ActiveContextAssignments` scans `src/` and `tests/`).
   Reports raised while `CRendererContext` constructs happen before the install, so they reach the sink
   installed before `RiBegin` and never set `RiLastError` (an accepted behaviour change, NEWS).
30. **Declarations are Graphics State's, reference counted (spec 038).** `CDeclarations` holds the declared
   set and the global numbering; `CRenderer::retrieveVariable`/`declareVariable` and `initDeclarations()`
   are forwarders in the renderer core, and `notifyGlobalVariable` (the per-thread shading contexts' update) is
   the one thing the table still asks of the core, installed as an observer by `initDeclarations()`. A program
   without a renderer holds a `CRiStandardSession`: the standard declarations and a file search that finds a
   path as written or through the search path, with no network and no temporary directory. Never call
   `CRenderer::initDeclarations()` from the Scene Interface, the preview or the data viewer.
31. **Named back-ends register themselves (spec 038).** `RiRegisterBackEnd(key, factory)` is called by a static
   initializer in the back-end's own translation unit; `RiBeginBackEnd(key, arg)` starts a light session
   (`CRiStandardSession`, `RiInit`, the factory, `riInstallContext`; no `.orenderrc`, no network, no
   `RiTini`) and `RiEndBackEnd` ends it. An unknown key reports "Back-end ... is not available in this build" and
   returns `nullptr`. In a static archive the linker keeps the registering object only if something references it:
   `libribpreview` forces `ribpreview_load` into every link (`target_link_options(... -u ...)`), so keep the
   registrar in the translation unit that defines the entry point a consumer calls.
32. **The compiler owns its builtin tables (spec 038).** `src/libshader/compiler/builtinTables.cpp` defines
   `kOpcodeParamTable` and `kAllFunctionMnemonics`; it is generated by
   `test_libshader_compiler_table_drift --emit`, never edited by hand, and `LibShader_CompilerTableDrift` fails with
   the first differing row when the engine's `scriptFunctions.h`/`scriptOpcodes.h` change (regenerate the file).
   The compiler's include path holds no Scene Interface, renderer or shading-engine directory
   (`Compiler_IncludeBoundary`): `src/CMakeLists.txt` adds those directories to every directory below `src`, so
   `src/libshader/compiler/CMakeLists.txt` filters them out of its own directory property.
33. **A `.slo` shader can call a DSO operand (spec 038).** `oshader --jit` lowers a `DSO name ("proto") dest args`
   instruction to `op_dso_call` (decoded in one place, `decodeDsoCall`, before the destination prelude: the
   instruction's result field is the operand's name, not a destination). The module gets a slot array
   `@"<shader>.shadeops"` and the metadata `openrender.shader.shadeops`; the loader binds each slot on every
   load (`CLLVMJitEngine::bindShadeops`, resolved with `getDSO`) because the operands belong to the renderer
   session while the compiled module outlives it. Only such a module records the `.slo` format 2.1.0
   (`SLO_FORMAT_MINOR`); every other module records `SLO_FORMAT_MINOR_BASE` (2.0.0), so shipped `.slo` keep
   their bytes. A missing operand makes the loader warn and return `nullptr`, so `getShader()` falls back to the
   `.rslo`. The interpreter's void-DSO row (`DSO_VOID`) steps its argument pointers as `char **`: they are
   byte steps.
34. **`precomp` and the hiders share `rasterFlags.h` (spec 038).** The `RASTER_*` constants live in
   `src/ri/hiders/rasterFlags.h` (no includes); `reyes.h` includes it, `precomp` includes only it
   (`Precomp_NoHiderCore`, `Precomp_OutputUnchanged`).
```

## 5. Active Technologies: append

```markdown
- C++20, CMake ≥3.19, `-Werror`; no new dependencies — the Scene Interface owns its services and is built without the
  renderer core (`CDeclarations` in Graphics State, `CRiStandardSession`, explicit context installation, named
  back-ends), the shader compiler owns its builtin tables, and the JIT calls DSO shadeop operands
  (038-scene-interface-untangle)
- Storage: N/A — memory only; the characterization references under `tests/**/reference` and
  `src/libshader/tests/reference` are recorded files (038-scene-interface-untangle)
```

## 6. Recent Changes: add at the top of the list

```markdown
- 038-scene-interface-untangle: the Scene Interface no longer depends on the renderer core (DB-015 resolved):
  declarations are Graphics State's `CDeclarations`, file search and variable lookup are Scene-owned services
  the core implements and `CRiStandardSession` supplies, `riHooks.h` moved into `src/ri/parse/`, the active
  context is installed explicitly (`riInstallContext`; a constructor assigns nothing) and `RiBeginLite` became
  `RiBeginBackEnd` with named, self-registering back-ends (the preview). `libRIBVector` compiles only the Core
  files pinned in `ribvector-core-residual.txt`. The shader compiler owns its builtin tables
  (`builtinTables.cpp`, generated, drift-tested; DB-019 resolved) and a `.slo` shader can call a DSO operand
  (`op_dso_call`; format 2.1.0 only for such modules; a missing operand falls back to the `.rslo`).
  `precomp` includes `rasterFlags.h` instead of the hider. Fixed on the way: the interpreter's void DSO operands
  stepped varying arguments four times too far. Reports raised while the renderer context is constructed no
  longer set `RiLastError`, so a failed network connect no longer makes `orender` exit with -1. No image
  changed (see spec 038).
```
