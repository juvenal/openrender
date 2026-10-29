# Contract: dlo 1.0.0, implicit 1.0.0, DynamicLoad generators, and the Geometric Procedures module (FR-001..FR-009, FR-022..FR-024, FR-032, FR-034, US5)

Geometric Procedures domain, `src/procedures/` (new; `domain-map.txt`:
`prefix src/procedures/ Procedures`; the two `file src/ri/geometry/{dlo,implicit}.h Procedures`
rules are deleted).

| File | Installed | Content |
|---|---|---|
| `dlo.h` | yes | moved with `git mv` from `src/ri/geometry/dlo.h` |
| `implicit.h` | yes | moved with `git mv` from `src/ri/geometry/implicit.h` |
| `proceduralModules.{h,cpp}` | no | the loader: locate-given-path, version check, resolve, reference-counted release |
| `CMakeLists.txt` | — | `openrenderprocedures_obj` (OBJECT), whose objects go into `ribRender` (shared and static) only |

## 1. `dlo.h` (public)

```c
#include "orplugin.h"
#define DLO_ABI_VERSION OR_ABI_VERSION(1, 0, 0)          /* + _MAJOR/_MINOR/_PATCH macros */
#define DLO_DECLARE_VERSION() OR_DECLARE_ABI_VERSION_SYMBOL(orDloAbiVersion, DLO_ABI_VERSION)

typedef void *(*dloInitFunction)(int, float *, float *);
typedef int   (*dloIntersectFunction)(void *, const float *, const float *, float *, float *);
typedef void  (*dloTiniFunction)(void *);

OR_EXTERN_C_BEGIN
OR_VISIBLE void *dloInit(int frame, float *bmin, float *bmax);
OR_VISIBLE int   dloIntersect(void *data, const float *from, const float *dir, float *t, float *N);
OR_VISIBLE void  dloTini(void *data);
OR_EXTERN_C_END

/* RenderMan DynamicLoad generators (ConvertParameters/Subdivide/Free are RI-defined and unchanged).
   Declaring a version is optional; an absent version is accepted silently (FR-009). Lives in dlo.h (Q3 resolved). */
#define OR_DYNAMICLOAD_ABI_VERSION OR_ABI_VERSION(1, 0, 0)
#define OR_DYNAMICLOAD_DECLARE_VERSION() \
    OR_DECLARE_ABI_VERSION_SYMBOL(orDynamicLoadAbiVersion, OR_DYNAMICLOAD_ABI_VERSION)
```

The stray `};` after today's `extern "C" { ... }` goes; `LIB_EXPORT` stays as an alias of
`OR_PLUGIN_EXPORT` for old sources.

## 2. `implicit.h` (public)

Same pattern: `IMPLICIT_ABI_VERSION` 1.0.0, `IMPLICIT_DECLARE_VERSION()` → `orImplicitAbiVersion`,
the four typedefs and declarations unchanged in signature.

Version histories (both): 1.0.0 = the Pixie-era interface; modules without the symbol are 1.0.0
and warn (rejected from 1.2.0). The 037 header changes (C guards, visibility) are binary-compatible
and part of 1.0.0.

## 3. Module API (`proceduralModules.h`, internal C++)

```cpp
class CProceduralModule : public CRefCounter {           // CRefCounter: libRIBCommon
    public:
        const char *path() const;
};
class CDloModule : public CProceduralModule {
    public: dloInitFunction init; dloIntersectFunction intersect; dloTiniFunction tini;
};
class CImplicitModule : public CProceduralModule {
    public: implicitInitFunction init; implicitEvalFunction eval;
            implicitEvalNormalFunction evalNormal; /* may be NULL */ implicitTiniFunction tini;
};
class CDynamicLoadModule : public CProceduralModule {
    public: TConvertParameters convertParameters; TSubdivide subdivide; TFree free; // each may be NULL
};

// Each: osLoadModule(path) -> checkPluginVersion(family) -> resolve -> attached reference, or
// NULL with today's diagnostics (missing symbol names the module as `displayName`). Thread-safe;
// one CProceduralModule per resolved path while referenced.
CDloModule        *acquireDloModule(const char *locatedPath, const char *displayName);
CImplicitModule   *acquireImplicitModule(const char *locatedPath, const char *displayName);
CDynamicLoadModule *acquireDynamicLoadModule(const char *generator);   // takes a frame reference

// Drops every frame reference taken by acquireDynamicLoadModule since the last call. Called by
// the renderer core right after root->destroy() in CRenderer::endFrame (FR-024, FR-034).
void releaseFrameProceduralModules();

// Test introspection (counts of loaded modules and of unloads), for FR-034/SC-008 tests.
struct SProceduralModuleStats { int loaded; int unloaded; };
SProceduralModuleStats proceduralModuleStats();
```

`detach()` of the last reference calls `osUnloadModule`.

## 4. Callers

| Caller (class) | Change |
|---|---|
| `CRendererContext::RiGeometryV` `"dlo"` (Core) | `acquireDloModule(location, name)` — the **located** path (FR-023/032); `data = module->init(frame, bmin, bmax)`; on `NULL` data "DLO \"%s\" failed to initialize" naming `name`; passes the module to `CDLObject`. |
| `CRendererContext::RiGeometryV` `"implicit"` (Core) | `acquireImplicitModule(location, name)`; passes the module to `CImplicit`. |
| `CDLObject` (Geometry) | holds `CDloModule *` (attached); dtor: `tini(data)`, `detach()`. No `osUnloadModule`. |
| `CImplicit` (Geometry) | constructor takes `CImplicitModule *` instead of a path; calls `init`; on failure detaches and leaves `module == NULL` (today's `handle == NULL` path, same errors); dtor: `tini(data)`, `detach()`. |
| `RiProcDynamicLoad` (Scene Interface, `ri.cpp`) | calls `procDynamicLoadHook()` (§5). With no hook: `error(CODE_NOFILE, "The delayed module \"%s\" cannot be loaded: no renderer\n", generator)` and returns. |
| `rendererProcedures.cpp` (Core, new) | the hook: `acquireDynamicLoadModule(delayed->generator)`; if `NULL`, today's "The delayed module ... is not found" error; else `ConvertParameters`/`Subdivide`/`Free` exactly as today. |
| `CRenderer::endFrame` (Core) | `releaseFrameProceduralModules()` after `root->destroy()`. |

Geometry → Procedures (allowed). Core → Procedures, Scene (allowed). Scene Interface never names
Procedures.

## 5. Scene Interface hook (`src/ri/parse/procDynamicLoadHook.h`)

```cpp
typedef void (*TProcDynamicLoadHook)(void *data, float detail);  // data is the CDelayedData *
void setProcDynamicLoadHook(TProcDynamicLoadHook hook);          // nullptr uninstalls
TProcDynamicLoadHook procDynamicLoadHook();
```

Defined in `ri.cpp` (compiled into ribRender, ribVector, ribOut). Installed by
`CRenderer::beginRenderer()`, removed by `endRenderer()` (the services pattern, gotcha 23). The
preview (`CRibGeometryContext`) never expands procedurals, so it never reaches the hook.

## 6. Lifetime rules (FR-024)

- A DLO or implicit instance holds its module until the instance is destroyed (REYES consumption
  or `root->destroy()`).
- A DynamicLoad generator's module is held by a frame reference from its first expansion in the
  frame; every `RiProcedural` its `Subdivide` creates (with function pointers into the module)
  becomes a `CDelayedObject` of that frame and is destroyed by `root->destroy()` or earlier, so the
  release after `root->destroy()` is the first safe point.
- Generators expanded from an `ObjectBegin` definition are expanded during the frame and follow the
  same rule; a generator that called `RiObjectBegin` itself is outside what RISpec allows inside a
  procedural and is not supported (noted in the procedurals guide).

## 7. Tests

| Test | Covers |
|---|---|
| `ProceduresAbi_Versions` | dlo, implicit: good / newer / v2 / legacy (×2 instances: 1 warning) / hidden, through their RIB statements; dynamicload: plain (no warning), versioned, v2 (rejected) — SC-001, SC-002, US5-1, FR-009 |
| `ProceduresAbi_SameRoutine` | the diagnostics of the three kinds come from `checkPluginVersion` (identical format, one definer by `nm`) — US5-1, SC-008 |
| `ProceduresAbi_BareNameLocated` | `Geometry "dlo" "file" "bare_name"` with the module only on `Option "searchpath" "procedural"` and the working directory elsewhere: loads (fails on the base tree) — FR-032, US5-2 |
| `ProceduresAbi_DynamicLoadRelease` | `nested` generator: its nested procedural's free runs, then the module is unloaded at frame end (stats and the module's destructor log), no crash under ASan/TSan — FR-034, US5-3 |
| `ProceduresAbi_ConcurrentAcquire` | 8 threads acquire one module: one load, one version message (FR-044, SC-018) |
| `ProceduresAbi_ObjectAndFrames` | a generator expanded inside `ObjectBegin` and over two frames: one frame reference per frame, released after that frame's geometry, no call into an unloaded module (FR-044, SC-018) |
| `ProceduresAbi_MissingSymbol` | `missing_symbol` variants: today's error texts naming the module |
| `ProceduresAbi_NoLoadOutsideModule` | mechanical: `osLoadModule` call sites are only `proceduralModules.cpp`, `shadeopModule.cpp` and `displayHost.cpp` (SC-008) |
| `ProceduresAbi_Scenes` | small DLO, implicit and DynamicLoad scenes (no such scene exists in the tree today) render the same before and after (captured first) |
