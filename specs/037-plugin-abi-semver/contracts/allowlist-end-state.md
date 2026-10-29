# Contract: dependency edges, allowlist and registry end state (FR-029, SC-009)

Line numbers are those of `tests/architecture/allowlist.txt` at `486decd`. Each deletion happens in
the phase whose code removes the edge, so that phase's `ctest -L architecture` reports `STALE`
until the line is deleted. **No allowlist line is added.**

## Deleted lines

| Line | Entry | Kind | From → to | Removed by |
|---|---|---|---|---|
| 415 | DB-019 | include | `@build/src/libshader/compiler/dso_generated.cpp` → `src/ri/parse/dso.h` | the compiler's `dso.y` copy is gone (shadeop phase) |
| 416 | DB-019 | include | `@build/src/libshader/shading/rslo_generated.cpp` → `src/ri/parse/dso.h` | the unused include in `shading/rslo.y` is deleted |
| 417 | DB-019 | include | `src/libshader/compiler/rslo.cpp` → `src/ri/parse/dso.h` | `enumerateDso` calls the shadeop module |
| 420 | DB-021 | include | `src/ri/texture/texture3d.cpp` → `src/ri/render/renderer.h` | Texturing channel interface |
| 421 | DB-021 | include | `src/ri/texture/texture3d.cpp` → `src/ri/state/displayChannel.h` | same |
| 422 | DB-021 | link | `texture3d.cpp` → `_ZN9CRenderer22retrieveDisplayChannelEPKc` | same |

The DB-019 section header's count changes from "(4)" (already wrong: 3 lines) to "(0)" and the
section, now empty, is removed; DB-019 stays Open for its JIT and standalone-compiler items (specs
038, 043), with its Size updated. The DB-021 section header and blank line go.

## Edges added (all allowed by `domain-rules.txt`)

| From (class) | To (class) | Files | Rule |
|---|---|---|---|
| Display | Infra | `dsply.h` → `src/common/orplugin.h` | `* -> Infra` |
| ShaderDSO | Infra | `shadeop.h`, `shadeopModule.cpp` → `orplugin.h`, `pluginAbi.h`, `os.h`, diagnostics | `* -> Infra` |
| Procedures | Infra | `dlo.h`, `implicit.h`, `proceduralModules.cpp` → `orplugin.h`, `pluginAbi.h`, `os.h`, `refCounter.h`, diagnostics | `* -> Infra` |
| Shading | ShaderDSO | `dsoHandle.h`, `scriptFunctions.h` (via `execute.cpp`), `libshader/compiler/rslo.cpp` → `src/shadeop/*.h`; link refs to `shadeopOperandData`/`Exec`, `enumerateShadeopDeclarations` | `Shading -> ShaderDSO` |
| Geometry | Procedures | `dlobject.{h,cpp}`, `implicitSurface.{h,cpp}` → `src/procedures/*.h`; link refs to `CProceduralModule` | `Geometry -> Procedures` |
| Core | ShaderDSO, Procedures, Display, Scene, Texturing | `rendererFiles.cpp`, `rendererContext.cpp`, `renderer.cpp`, `rendererProcedures.cpp`, `displayHost.cpp`, `displayV1Adapter.cpp`, the channel-source implementation | `Core -> *` |
| Scene | Scene | `ri.cpp` → `procDynamicLoadHook.h` | same class |
| Texturing | Texturing | `texture3d.cpp` → `texture3dChannels.h` | same class |

## Edges removed besides the allowlisted ones

Core → Scene (`renderer.cpp`, `rendererFiles.cpp`, `rendererContext.cpp`, `rendererStatics.cpp`
→ `dso.h`); Scene → Infra `osLoadModule` (`ri.cpp`); Geometry → Infra `osLoadModule`
(`implicitSurface.cpp`, `dlobject.cpp`); Shading → ShaderDSO through the old path
`src/ri/render/shadeop.h` (replaced by `src/shadeop/shadeop.h`).

## `domain-map.txt` changes

```
- file    src/ri/geometry/dlo.h                        Procedures
- file    src/ri/geometry/implicit.h                   Procedures
- file    src/ri/render/shadeop.h                      ShaderDSO
- file    @build/src/ri/dso_generated.cpp              SceneInterface
- file    @build/src/ri/lex.dso.cpp                    SceneInterface
+ prefix  src/shadeop/                                 ShaderDSO
+ prefix  src/procedures/                              Procedures
```

`src/common/orplugin.h(pp)` and `pluginAbi.*` are covered by `prefix src/common/ Infra`; the new
core files by `prefix src/ri/render/ Core`.

## Metrics

`metrics-baseline.txt` may only go down. Expected: the Geometry `CRenderer::` count is unchanged
(the DLO and implicit code it touches does not name `CRenderer`); Texturing loses
`retrieveDisplayChannel` (1 reference, 1 member). Lower the baseline to the reported `NOTE` values.

## Registry (`DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`)

- DB-003 → Resolved, "spec 037, <date>": version macros, symbols, the one check, histories; rule
  text updated to constitution 1.3.0 ("a MAJOR the host does not support"); the preamble's "v1.2.0+"
  becomes "v1.3.0+".
- DB-014 → Resolved, "spec 037, <date>": display v2 + adapter, per-thread shadeop, one Procedures
  module, deferred unload.
- DB-019: Evidence and Size updated (0 lines; the compiler no longer `dlopen`s shadeops itself);
  Direction keeps the JIT DSO path (038) and the standalone compiler (043). Stays Open.
- DB-021 → Resolved, "spec 037, <date>".
- DB-024: the "one display ABI definition (037)" item is marked done in its History.
