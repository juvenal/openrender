# Contract: edges, allowlist, metrics, registry and build targets at the end (FR-015, FR-025-FR-028, SC-002, SC-010)

Line numbers are `tests/architecture/allowlist.txt` at master `d6910bd0`. Each line is deleted in the
phase whose code removes the edge, so that phase's `ctest -L architecture` reports it `STALE` first.
**No allowlist line is added.**

## Deleted lines (all of DB-015)

| Line | Kind | From → to | Removed by |
|---|---|---|---|
| 297 | header | `# --- DB-015: ... (11) ---` | goes with the last line |
| 298, 299 | include | `@build/src/ri/ifexpr_generated.cpp` → `render/renderer.h`, `render/rendererContext.h` | the evaluator becomes a Scene function (R6) |
| 300, 301 | include | `@build/src/ri/rib_generated.cpp` → `render/renderer.h`, `render/rendererContext.h` | the grammar uses the services; the dead branch goes |
| 302 | include | `src/ri/output/ribOut.cpp` → `src/ri/render/riHooks.h` | `riHooks.h` moves to `parse/` |
| 303 | include | `src/ri/parse/ri.cpp` → `src/ri/render/riHooks.h` | same |
| 304 | link | `rib_generated.cpp` → `CRenderer::locateFile` | `riLocateFile` |
| 305 | link | `rib_generated.cpp` → `CRenderer::retrieveVariable` | `riRetrieveVariable` |
| 306 | link | `ri.cpp` → `CRenderer::initDeclarations` | `CRiStandardSession` in the writer branch |
| 307 | link | `ri.cpp` → `CRenderer::shutdownDeclarations` | same |

## Lines that stay (and why they stay valid)

- DB-004 `:49` `blobby.cpp -> CRenderer::retrieveVariable`, `:50` `blobbyRepeller.cpp ->
  CRenderer::locateFile`: the members stay as out-of-line forwarders (R4).
- DB-017 `:340` `object.cpp -> CRenderer::findCoordinateSystem`: the function and its stub stay.
- DB-004 includes of `riInterface.h` (`:23`, `:46`): unchanged.

## Edges added (all allowed by `domain-rules.txt`)

| From (class) | To (class) | Files |
|---|---|---|
| SceneInterface | GraphicsState | `parse/riServices.cpp` → `state/declarations.h`; `rib.y`/`ifexpr.y` → the State headers they used through `rendererContext.h` |
| SceneInterface | SceneInterface | `rib.y`, `rib.l`, `ri.cpp` → `riServices.h`, `delayedData.h`, `riCondition.h`, `riHooks.h` |
| GraphicsState | Infra / GraphicsState | `state/declarations.cpp` → `common/*`, `channelRegistry.h`, `variableLookup.h` |
| Core | SceneInterface | `rendererContext.cpp` → `parse/riHooks.h`, `parse/riCondition.h`; `rendererDeclarations.cpp` → `parse/riServices.h`; `rendererProcedures.cpp` → `parse/delayedData.h` |
| Core | GraphicsState | `rendererDeclarations.cpp`, `rendererServicesImpl.h` → `state/declarations.h` |
| Hiders | Hiders | `reyes.h` → `hiders/rasterFlags.h` |
| Shading | ShaderDSO | `rslOps.cpp` → `shadeop/shadeopModule.h` (the module already links it) |
| Consumer | SceneInterface | `previewContext.cpp` → `parse/riHooks.h`; `dataLoad.cpp` → `parse/riServices.h` (CONSUMER lines) |
| Consumer | Hiders | `precomp.cpp` → `hiders/rasterFlags.h` (replaces `reyes.h`) |

## Edges removed besides the allowlisted ones

SceneInterface → Geometry (`rib.y`, `ri.cpp` → `geometry/delayed.h`; allowed, so never listed);
Consumer → Core (`previewContext.cpp`, `ribGeometryContext.cpp` → `renderer.h`, `rendererContext.h`;
`dataLoad.cpp` → `CRenderer::initDeclarations`/`shutdownDeclarations`; `precomp.cpp` → `renderer.h`);
Consumer → SceneInterface (`previewContext.cpp` → `riDiagnostics.h`); Shading → Shading
(`llvmEmitter.cpp` → `shading/scriptFunctions.h`, `scriptOpcodes.h`; same class, never listed, but
what US5 removes).

## Metrics (`metrics-baseline.txt`)

SceneInterface 6 / 6 → 0 / 0. TOTAL 612 / 94 → the values archcheck reports (expected 606 / 91:
`ifParse`, `initDeclarations` and `shutdownDeclarations` are referenced by no other measured class;
`retrieveVariable`, `locateFile` and `context` still are). Geometry 48 / 15 and Hiders 558 / 82
unchanged. The baseline only goes down; the comment block records spec 038's line.

## `ribVector` residual (`tests/architecture/ribvector-core-residual.txt`, new, shrink-only)

```text
# file                                       needed by
src/ri/render/rendererDeclarations.cpp      src/ri/geometry/blobby.cpp (CRenderer::retrieveVariable)
src/ri/render/rendererFileSearch.cpp        src/ri/geometry/blobbyRepeller.cpp (CRenderer::locateFile)
src/ri/render/rendererNetwork.cpp           src/ri/render/rendererFileSearch.cpp
src/ri/render/rendererMutexes.cpp           src/ri/render/rendererFileSearch.cpp
src/ri/render/rendererStatics.cpp           Geometry (object.cpp, blobbyRepeller.cpp), Hiders (compositor.cpp, irradiance.cpp, photonMap.cpp)
src/ri/vector/geometryDispatchStub.cpp      Geometry
src/ri/vector/irradianceDispatchStub.cpp    Hiders (irradiance.cpp)
src/ri/vector/rendererDeclarationsDispatchStub.cpp  src/ri/geometry/object.cpp (findCoordinateSystem)
src/ri/vector/surfaceCrossingDispatchStub.cpp       Geometry
```

Measured again by the Phase 4 `nm` sweep (specs/038-scene-interface-untangle/tools/ribvector_core_sweep.sh)
before the file is written; `rendererClipping.cpp` and `resource.cpp` leave `ribVector` (Q7).
Specs 029-031 delete lines as Geometry and Hiders stop needing them.

## Registry (`DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`)

- **DB-015 → Resolved**, "Spec 038, <date of the phase>": evidence restated with the corrected
  counts (10 lines, header said 11) and the `riInterface.cpp:37` citation (the entry said `:36`);
  history: what replaced each edge.
- **DB-019 → Resolved**, "Spec 038, <date>": the compiler owns its tables and includes nothing of
  the shading engine, the Scene Interface or the core; the JIT has a DSO call path. The second
  evidence bullet's header name corrected to `scriptOpcodes.h`; the Direction no longer assigns the
  standalone compiler to 043 (043 keeps the public library and C API, D25).
- **DB-024 stays Open, narrowed**: `precomp` (replaced by the constants header), the preview's and
  the data viewer's declaration and context use are done; remaining evidence `sloinfo` and `oshader`
  (specs 033, 043), the data viewer's Hiders/Texturing reach (spec 032), the preview's Geometry and
  Graphics State includes; Size restated as measured in Phase 0 with what it counts (M14); the
  Direction's "`RiMake*` for precomputation (038)" corrected to what 038 did.
- **DB-004 and DB-011**: a paragraph naming the `ribVector` residual file and its lines that belong
  to each entry; no allowlist change.
- **DB-017**: the `rendererDeclarations` pair now carries `findCoordinateSystem` and
  `notifyGlobalVariable` (the numbering moved to Graphics State); no allowlist change.

## Domain map

No new directory under `src/`. New files fall under existing prefixes: `src/ri/parse/`
(SceneInterface), `src/ri/state/` (GraphicsState), `src/ri/hiders/` (Hiders),
`src/libshader/compiler/` (Shading). `domain-map.txt` is unchanged.
