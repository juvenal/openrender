# Research: Versioned Plug-in ABIs (spec 037)

**Branch**: `037-plugin-abi-semver` | **Date**: 2026-10-29 | **Base**: master `486decd`

Every finding below was checked against the tree at `486decd` (the worktree differs from it only
under `specs/` and `.specify/memory/constitution.md`). Each decision is written as
Decision / Rationale / Alternatives. Section M records the measured facts the decisions rest on,
including corrections to the notes the specify stage left.

---

## M. Measured facts (2026-10-29)

### M1. Loader sites

| Family | Site | What it does today |
|---|---|---|
| Display | `src/ri/render/rendererDisplay.cpp:829-880` (`computeDisplayData`) | `locateFileEx(deviceFile, outDevice, osModuleExtension, displayPath)`, `osLoadModule(deviceFile)`, resolves `displayStart`/`displayData`/`displayRawData`/`displayFinish`; sets the file-static `currentDisplay` (`:41`) just before `start`. |
| Display (custom) | same function, `outDevice == "custom"` | Casts `RtDisplayStartFunction`/`RtDisplayDataFunction`/`RtDisplayFinishFunction` (`riVocabulary.h:70-74`, int parameter type) into the `dsply.h` types (`ParameterType`). |
| Display unload | `CRenderer::dispatch` (`rendererDisplay.cpp:349-359`) | On `data == FALSE`: under `displayKillMutex`, `osUnloadModule`, `module = NULL`. Other threads test `datas[i].module` at `:321` outside the mutex. `endDisplays` (`:234-282`) calls `finish` and never unloads. |
| Shadeop (renderer) | `CRenderer::getDSO` + `dsoLoadCallback` (`rendererFiles.cpp:843-972`) | `osEnumerate("<dir>*.dsply")` over `proceduralPath`; `osResolve("<name>_shadeops")`; `dsoParse` each entry; on prototype match resolves `init`/`exec`/`cleanup`, tests `if (exec != NULL)` (`:884`), where `exec` is the **output array pointer** (`userData[3]`), always non-null (FR-033). One `init(0, NULL)` (`:960`); the result is shared by every thread. Modules are never unloaded; `shutdownFiles` (`:386-395`) only calls `cleanup`. |
| Shadeop (compiler) | `dsoEnumerateCallback` / `CScriptContext::enumerateDso` (`libshader/compiler/rslo.cpp:1698-1757`) | A second scan of `dsoPath` (`*.dsply`), second `dsoParse` call; registers every entry as a builtin prototype. |
| DLO | `CRendererContext::RiGeometryV` `"dlo"` branch (`rendererContext.cpp:5486-5560`) | Locates `name` on `proceduralPath` into `location`, then calls `osLoadModule(name)`: the **unresolved** name (FR-032). Resolves `dloInit`/`dloIntersect`/`dloTini`; `CDLObject` holds the module and `osUnloadModule`s it in its destructor (`dlobject.cpp:66-69`). |
| Implicit | `CImplicit::CImplicit` (`geometry/implicitSurface.cpp:43-80`) | `osLoadModule(name)` where `name` is already the located path (`rendererContext.cpp:5476-5478`); resolves four symbols; destructor unloads (`:87-95`). |
| DynamicLoad | `RiProcDynamicLoad` (`src/ri/parse/ri.cpp:1757-1793`) | `osLoadModule(delayed->generator)` per expansion; resolves `ConvertParameters`/`Subdivide`/`Free`; `osUnloadModule` is commented out (`:1788`). `ri.cpp` is compiled into **ribRender, ribVector and ribOut**. |

`osLoadModule` is `dlopen(name, RTLD_NOW)`, retried with `realpath(name)` (`common/os.cpp:96-106`).
`osModuleExtension` is `"dsply"` on every Unix platform (`os.h:327`), so DLO, implicit and
shadeop modules are also `.dsply` files.

### M2. Headers and includers

- 16 files include `dsply.h`/`shadeop.h`/`dlo.h`/`implicit.h` (sources, `tests/unit/state/test_state_vocabulary.cpp`,
  `examples/dsotest/dsotest.cpp`). `dso.h` is included by `parse/dso.y`, `render/renderer.cpp`,
  `render/rendererFiles.cpp`, `render/rendererContext.cpp`, `render/rendererStatics.cpp`,
  `libshader/shading/rslo.y` (included, `dsoParse` never called) and `libshader/compiler/rslo.cpp`.
- `dsply.h`, `dlo.h`, `implicit.h` open `extern "C" {` unguarded; `LIB_EXPORT` is plain `extern`
  (no visibility attribute). `shadeop.h`'s macros expand to `extern "C"`. `SHADEOP` defines
  `void f(void *, int, char *argv[])`; `dsoExecFunction` is `void (*)(void *, int, void *[])` (FR-031).
  `SHADEOP_SPEC` holds three `char *` (not `const`).
- `RtDisplay*Function` exist twice: `src/ri/parse/ri.h:76-80` and `src/common/riVocabulary.h:70-74`
  (spec 023 duplicated the vocabulary). `RtDisplayRawDataFunction` is used by nothing.
- `stateConversions.h` static-asserts `EDisplayParameterType` against `dsply.h`'s `ParameterType`;
  `State_VocabularyConsistency` (`tests/unit/state/test_state_vocabulary.cpp`) includes `dsply.h`.

### M3. The nine writable statics (DB-014)

`findParameter` (`rendererDisplay.cpp:50-137`) returns non-`const` `void *` to `CRenderer::clipMin`,
`clipMax`, `fromWorld` (`Nl`), `worldToNDC` (`NP`), `toScreen` (`screen`), `gamma`, `gain`,
`depthQuantizer`/`depthQuantizer + 4`, `colorQuantizer`/`colorQuantizer + 4`; plus pointers into
`currentDisplay->quantizer` and into the display's own parameter data. `quantize` and `dither`
dereference `currentDisplay` with no null check (`:71`, `:86`) (FR-039). `type` returns the RIB
`outDevice` as written (`tiff`/`png`), not the remapped `file` module name; `file.cpp:38-41` relies
on it to choose PNG or TIFF. A display-line parameter wins only if name, item count and type all
match (`:55-62`); otherwise the renderer-provided value is returned.

### M4. Display parameter storage

`CRendererContext::RiDisplayV` (`rendererContext.cpp:1395-1462`) copies display parameters into
`COptions::CDisplay::parameters`. Strings keep only the first element as one `char *`
(`strdup(((char **)params[i])[0])`), so a v1 `STRING_PARAMETER` lookup returns `char *`, not
`char **`. **New finding:** for `TYPE_QUAD`/`TYPE_MPOINT`/`TYPE_DOUBLE` the entry's `name` and
`numItems` are set but `type` and `data` are left uninitialized while `j` is still incremented
(`:1430-1433`, `:1456`). A snapshot copy would dereference that garbage; fixed as FR-040 (R18).

### M5. Build placement

- `dso.l`/`dso.y` are generated and compiled into **four** libraries: `ribRender`, `ribVector`
  (both from `ri_sources`), `ribOut` (`ribOut_sources`) and `libshader_compiler`
  (`libshader/compiler/CMakeLists.txt:21-26`). The parser keeps its result in file statics
  (`prototypeBuffer[256]`, `currentPrototype`, `funName`, `numErrors`) and writes into the 256-byte
  buffer without a bound.
- `dso.l` ignores `output`, maps the eight type keywords, takes `[a-zA-Z0-9_]+` as a name, skips
  blanks, and (flex default) **echoes any other character to stdout and skips it**.
- Libraries follow one pattern: an OBJECT library packaged as SHARED + STATIC
  (`ribRender_obj`, `libshader_compiler_obj`, `openrendercommon_obj`), exported in
  `openRenderTargets`/`openRenderStaticTargets`. `libshader_shading` is SHARED only.
- The project enables C (`project(openRender ... LANGUAGES C CXX)`, version 1.0.0).

### M6. Procedurals and preview

- `CRibGeometryContext::RiProcedural` (orender-wire, `ribGeometryContext.cpp:745-753`) only
  records a bounding box; the preview never expands a procedural, so a renderer-installed hook
  changes nothing for it.
- `CRenderer::endFrame` (`renderer.cpp:834-`) order: contexts deleted, `endFrameResources()`
  (calls `CFrameResource::frameEnded()`), `root->destroy()`, `endDisplays()`, then remote
  channels, photon maps, files, texture system. A generator's `Subdivide` may call `RiProcedural`
  with subdivide/free functions that live in the generator module, so its module must outlive
  `root->destroy()`; `frameEnded()` is too early.

### M7. Texturing channel parse (DB-021)

`CTexture3d::defineChannels(const char *)` (`texture3d.cpp:80-140`) calls
`CRenderer::retrieveDisplayChannel`, which returns `nullptr` outside a session
(`channelRegistry()` is `nullptr` then). `fill` is **aliased** to the registry's array (not
copied; `~CTexture3d` does not free it). The `(n, names, types)` overload (ptcapi, no session)
uses `parseVariable` only. No test in the tree exercises either overload directly.

### M8. Tests

No test builds a plug-in module; `examples/dsotest` is not in the build; no example or test scene
uses `Geometry "dlo"`, `Geometry "implicit"` or `Procedural "DynamicLoad"`. `oshader`'s DSO search
path is its `-I` path (`oshader.cpp:330-347`). The JIT has no DSO call path (DB-019; the DSO rows
are the `"XXX"` dispatcher entries excluded from the coverage guard), so shadeop render tests run
the `.rslo` interpreter. `CShadingContext::thread` (`shading.h:511`) is the per-context thread
index. `CDiagnosticSink` / `installDiagnosticSink` (`common/diagnostics.h:86-96`) can capture
warnings in-process.

### M9. Corrections to the specify notes

| Note | Measured |
|---|---|
| "DLO loader near `rendererContext.cpp:5490-5560`" | `5486-5560`; it loads `name`, not `location`. |
| "`RiProcDynamicLoad` near `ri.cpp:1759-1790`" | `1757-1793`. |
| "9 writable statics, 4 driver modules, 16 includers" | Confirmed. |
| (implied) `dsoParse` is in two libraries | Four: ribRender, ribVector, ribOut, libshader_compiler. |
| DB-019 allowlist "(4)" | The section header says 4; it lists 3 lines. Fix the comment when they go. |
| DB-003 text | Still says "the host MUST reject a MAJOR mismatch" and the registry preamble cites constitution v1.2.0; both are updated when DB-003 is resolved (constitution 1.3.0). |

### M10. Further checks made during planning

- `CODE_VERSION` already exists in `src/common/diagnostics.h:40` (and `RIE_VERSION` in the RI
  vocabulary): the version diagnostics use it; no vocabulary change, `ri.h` untouched.
- `tests/framebuffer/` never `dlopen`s `framebuffer.dsply` and calls no `display*` symbol; those
  tests are unaffected by the v2 migration.
- The render worker threads are joined (`osWaitThread`, `renderer.cpp:1054`, `:1084`) before
  `endFrame` runs `endDisplays`. *Verify at tasks*: the network-server path and the `clear()` →
  `dispatch` calls made from the bucket code, which also reach the display host.
- `CDLObject::instantiate` and `CImplicit::instantiate` are empty (`dlobject.cpp:145`,
  `implicitSurface.cpp:218`): an instanced DLO or implicit renders nothing and copies no module
  handle, so there is no double `tini`/unload today and "one reference per instance" only concerns
  objects the core creates. (A functional gap, not in 037's scope; recorded here, not fixed.)
- Library placement for US7: `texture3d.cpp` and `delayed.cpp` are in ribVector's sources;
  `rendererShadingServices.cpp` is excluded from ribVector and absent from ribOut. So
  `texture3dChannels.cpp` (the registration pair) joins `ri_sources` (both ribRender and ribVector),
  and the core's channel-source implementation goes in `rendererShadingServices.cpp`. ribOut and
  ribVector still link after the `dso` grammar is removed (neither calls the parser).
- `numThreads` is per frame (`renderer.cpp:236`), see R10.

---

## R1. One version-check routine and the convention header live in infrastructure

**Decision.** The version encoding, the export macros, the status codes and the table header
(`src/include/orplugin.h`, C17, installed) and the C++ convenience header
(`src/include/orplugin.hpp`, installed) are infrastructure (`src/include/` is on every target's include path). The single host-side check routine
`checkPluginVersion()` and the table check `checkPluginTable()` live in
`src/common/pluginAbi.{h,cpp}` (internal, not installed), compiled into `libRIBCommon`. Each of the
four family headers includes `orplugin.h`.

**Rationale.** Three classes call the check (renderer core for displays, Shader DSO Operands,
Procedures); the only place all three may depend on is Infra (`* -> Infra` in
`domain-rules.txt`). FR-004 asks for one routine, not one per loader. The "warned once per module
per run" record is process-wide state, and gotcha 22 makes `libRIBCommon` its only legitimate
home. The family headers stay "their own published ABI header" plus Infra.

**Alternatives.** (a) One copy per family header as `static inline` code: violates FR-004.
(b) A new "plugin" utility library beside `libRIBCommon`: a second infrastructure library for
~200 lines, and it would need its own process-wide state. (c) Put the check in the renderer core:
Shader DSO Operands and Procedures may not depend on the core.

## R2. Version symbols: one per family, distinct names, read first

**Decision.** Encoding `MAJOR<<16 | MINOR<<8 | PATCH` (`OR_ABI_VERSION(M, m, p)`). Each header
defines a one-line macro that emits a C-linkage, default-visibility function:

| Family | Macro | Symbol | Host version | Host supports MAJOR |
|---|---|---|---|---|
| Display | `OR_DISPLAY_DECLARE_VERSION()` | `orDisplayAbiVersion` | 2.0.0 | 1 (adapter), 2 |
| Shadeop | `SHADEOP_DECLARE_VERSION()` | `orShadeopAbiVersion` | 1.1.0 | 1 |
| DLO | `DLO_DECLARE_VERSION()` | `orDloAbiVersion` | 1.0.0 | 1 |
| Implicit | `IMPLICIT_DECLARE_VERSION()` | `orImplicitAbiVersion` | 1.0.0 | 1 |
| DynamicLoad generator | `OR_DYNAMICLOAD_DECLARE_VERSION()` (in `dlo.h`, Q3 resolved) | `orDynamicLoadAbiVersion` | 1.0.0 | 1 |

The loader resolves the family's symbol and calls it before resolving or calling anything else.
Outcomes: MAJOR not supported → reject, diagnostic, no further call; same MAJOR, MINOR ≤ host →
silent; MINOR > host → one warning; symbol absent → treated as 1.0.0 with the legacy warning
(no warning for DynamicLoad generators, FR-009). A raw value whose MAJOR is not supported
(including `0xFFFFFFFF` or 0) is printed raw. The check is cached per (family, resolved path) for
the life of the process (R10), so the second load of a module prints nothing.

**Rationale.** Distinct names answer the "one module, several families" edge case; a function
(not a data symbol) keeps the C ABI trivially portable and lets a C plug-in declare it in one line.
The legacy sunset release lives in one constant, `OR_PLUGIN_UNVERSIONED_SUNSET "1.2.0"`, in
`pluginAbi.h` (FR-007), so the later rejection is a one-line change.

**Alternatives.** A data symbol (`const uint32_t orDisplayAbi`): equally simple, but harder to
make "garbage" testable and unlike the registration function of R3. One shared name for all
families: breaks the multi-family edge case.

## R3. The shared plug-in convention (FR-010)

**Decision.** `orplugin.h` defines:
- `OrTableHeader { uint32_t structSize; uint32_t abiVersion; }`, the first two fields of every table;
- the size rule: a table's **required size** is the size of the fields of its MAJOR's `.0`
  release. The host rejects `structSize < required` without reading past the header; fields added
  by later MINORs are read only when `structSize` covers them; bytes beyond what the host knows are
  ignored;
- `OrStatus` (`OR_STATUS_OK = 0`, `ERROR`, `UNSUPPORTED`, `BAD_ARGUMENT`, `OUT_OF_MEMORY`,
  `VERSION`, `TABLE_SIZE`); any non-OK return is failure;
- opaque handle declaration macro `OR_DECLARE_HANDLE(Name)` (`typedef struct Name Name;`);
- `OrHostInfo { structSize; abiVersion; const char *hostName; const char *hostVersion; }`, passed to
  a registration entry point;
- the ownership rules (contract `plugin-convention.md` §4): the side that allocates frees; host
  data handed to a plug-in is `const` and valid for a stated window; plug-in tables are static
  storage that outlives every call;
- `OR_PLUGIN_EXPORT` (`extern "C"` under C++, `__attribute__((visibility("default")))` on GCC/Clang,
  `__declspec(dllexport)` under `_WINDOWS`, kept as today) and the registration shape
  `OR_PLUGIN_EXPORT const <Type>Plugin *or<Type>Plugin(const OrHostInfo *host)`.

`orplugin.hpp` gives `orplugin::makeTable<T>(version)` (zero-filled, header set), a
`static_assert` that `T` starts with `OrTableHeader`, and `orplugin::Status` aliases. The host side
of the size rule is `checkPluginTable()` in `pluginAbi.cpp`. Display v2 (R5) is the first family
built on it; the three legacy families keep their named entry points (they cannot change shape
without a MAJOR) and say so in their headers (US6).

**Rationale.** It is decision D7 / reference §9 made concrete, and the spec's literal size rule
("rejects a smaller one") would reject every older-MINOR plug-in once a MINOR adds a field; the
"required size = MAJOR's .0 fields" reading keeps MINOR backward-compatible as Principle VIII
requires.

**Alternatives.** Host-allocated output tables (`register(host, out)` with the host's size): the
plug-in must then know how much it may write, which is the same check moved to the plug-in side and
untestable from the host. Vulkan-style `pNext` chains: more than any current family needs.

## R4. C-includability and the shadeop signature

**Decision.** All four headers: `#include <stdint.h>`, `OR_EXTERN_C_BEGIN/END` guards around
declarations, `OR_PLUGIN_EXPORT` on every entry point, no C++-only construct outside the guards.
`SHADEOP` becomes `void f(void *initdata, int argc, void *argv[])`, matching `dsoExecFunction`
(FR-031); `SHADEOP_SPEC`'s fields become `const char *`. Both are source-level changes with no
binary effect (same layout, same `extern "C"` names, same calling convention), so they are part of
shadeop 1.0.0. `LIB_EXPORT` stays defined (old plug-in sources use it) as an alias of
`OR_PLUGIN_EXPORT`.

**Rationale.** A C17 plug-in must be able to include any header (Principle II); hidden-visibility
builds (US2-3) need the attribute; the `argv` mismatch is the FR-031 defect. Code that reads
`argv[i]` with an explicit cast (`(float *)argv[0]`, as `examples/dsotest` does) compiles
unchanged; pointer arithmetic on `argv[i]` without a cast would not, and that is documented in the
shadeop history.

**Alternatives.** Change the typedef to `char *[]` instead: keeps old sources compiling but makes
the host's typed call an implicit pointer conversion; PRMan's own `shadeop.h` uses `void *`.

## R5. Display v2: a function table, a const snapshot, channel descriptors

**Decision.** Display 2.0.0 (contract `display-v2.md`): the module exports
`orDisplayAbiVersion()` and `const OrDisplayPlugin *orDisplayPlugin(const OrHostInfo *)`. The table
is `{OrTableHeader header; start; data; finish}`:

```c
OrStatus (*start)(OrDisplayContext *ctx, const OrDisplayInfo *info, void **instance);
OrStatus (*data)(OrDisplayContext *ctx, void *instance, int x, int y, int w, int h, float *pixels);
void     (*finish)(OrDisplayContext *ctx, void *instance);
```

`OrDisplayContext` is an opaque per-display host handle passed to every call. `OrDisplayInfo`
(`const`, host-owned, valid from `start` until `finish` returns) carries `name`, `type` (the RIB
display type as written, e.g. `tiff`), `mode` (the RIB mode string, informational), `width`,
`height`, `numSamples`, the channel descriptors and the parameters. Parameters are the RIB display
parameters followed by the renderer-provided ones (`near`, `far`, `Nl`, `NP`, `screen`, `gamma`,
`gain`, `quantize`, `dither`, `Software`, `type`), all copies. `orDisplayFindParameter(info, name,
type, numItems)` (`static inline` in `dsply.h`) returns the first entry matching name, type and
count, so a display-line value still overrides the renderer's exactly as `findParameter` did
(M3). Strings are one `const char *` (M4). `displayRawData` is gone (FR-019).

**Rationale.** It satisfies FR-014..FR-017 with the convention of R3, gives later display types
(and display filters, D7) the same shape, and keeps the in-tree drivers' migration mechanical: they
read the same names through the inline lookup. Keeping one lookup with the old precedence is what
makes SC-004's zero-pixel-difference reachable. `pixels` is deliberately not `const`: it is the
host's per-call, per-display packed copy (`CRenderer::dispatch` already builds one), and the
in-tree drivers modify it in place (`CDisplay::clampData` in the framebuffer,
`CFileOutputBase::applyColorPipeline` in `write`). Being scratch owned by the host for one call, it
is not renderer state, and drivers need no extra copy.

**Alternatives.** Keep the four named exports with new signatures: a v1 binary would be called
with v2 arguments if its version symbol were wrong or missing — distinct symbols make a v1/v2
confusion impossible. A host callback table for parameter lookup: a second call boundary for data
that can simply be handed over.

## R6. The confined v1 adapter, and its scoped thread-local

**Decision.** One renderer-core module, `src/ri/render/displayV1Adapter.{h,cpp}`, is the only
code that knows display MAJOR 1. It owns the v1 function types (moved out of `dsply.h` into its
private header `displayV1Abi.h`), wraps a v1 module, or an `RiCustomDisplay` callback triple,
in an `OrDisplayPlugin` table, and is selected in exactly one place in the display host's loader.
v2 code paths never see v1. The adapter:
- gives the v1 `findParameter` callback **per-display, mutable copies** made from the v2 snapshot
  (a v1 driver that writes through a returned pointer changes only its own copy; FR-006 "copies only");
- binds "the display being started" in a `thread_local` pointer **inside the adapter**, set
  immediately before the v1 `displayStart` and cleared immediately after it, on the calling thread
  only. A lookup with nothing bound (a driver that kept the callback and calls it later, or on
  another thread) returns `NULL` and logs once per display (FR-020, FR-039);
- exposes two distinct callbacks with the exact v1 types (`void *(*)(const char *, ParameterType,
  int)` for modules, `void *(*)(const char *, int, int)` for `RiCustomDisplay`), so neither path
  is a function-pointer cast (FR-019);
- serializes `data` calls per display with a mutex and stops forwarding after a failure
  (v1 drivers such as the old framebuffer delete themselves inside a failing `displayData`, so a
  concurrent in-flight call would use freed memory); after a v1 failure, the adapter's `finish`
  does not call the v1 `displayFinish` (the legacy contract has no finish after failure).

**Rationale.** A v1 callback carries no context, so some binding must exist; FR-006 allows it "for
the duration of that driver's `displayStart` call only". A thread-local scoped to that call, in the
one file that knows v1, is that binding and nothing more: it is not host-wide, not file-static
across calls, and not visible to v2. Serialization trades parallel `data` on legacy drivers (none
in tree after migration) for the absence of a use-after-free.

**Alternatives.** A process-wide "current display" with a mutex held across `start`: still a
global, and it serializes every start. Generating per-display trampolines (closures) so the
callback carries its display: needs runtime code generation or a fixed pool; disproportionate.
Leaving v1 `data` unserialized: keeps today's latent race for legacy drivers.

## R7. Display threading, disable and unload

**Decision.** The display host (R8) keeps per display an atomic `enabled` flag. `dispatch` tests
it before calling `data`; a non-OK `data` clears it (under `displayKillMutex` for the
`numActiveDisplays`/`HIDER_BREAK` bookkeeping) and **never unloads**. `endDisplays` calls `finish`
exactly once for every display whose `start` succeeded (enabled or not), then the shadow-map step
as today, then releases the module reference; a module shared by two displays is unloaded when the
last reference goes (FR-018, FR-035). The documented v2 contract: `start`/`finish` never concurrent
with any other call for that display and called once each; `data` may run concurrently for
disjoint tiles; a `data` call that started before the failure was observed may still arrive after
it, and the driver must treat it as a no-op; no call follows `finish`.

The in-tree framebuffer driver stops deleting its window inside a failing `data`; `finish` deletes
it (the v2 rule). `endDisplays` runs after the render threads have been joined, so "no host thread
is inside an entry point" holds when it releases modules.

**Rationale.** Removes the unload race (US3-4, SC-006) without a per-call reader count, because
the only unload point is after every render thread has returned.

**Alternatives.** Reader/writer lock around every `data` with unload on the failing call: correct
but adds a lock to every tile and keeps an unload in the hot path. Keep "no finish after failure"
for v2: leaks the driver's resources and contradicts FR-017 ("exactly once").

## R8. A display host class inside the renderer core

**Decision.** Move display loading, starting, dispatching to drivers, finishing and unloading out
of `CRenderer`'s statics into `CDisplayHost` (`src/ri/render/displayHost.{h,cpp}`, renderer core).
`CRenderer` keeps channel layout (`computeDisplayData`'s channel parsing, sample orders, TSM) and
hands `CDisplayHost` one `SDisplaySpec` per display (name, type, mode, resolution, channel
layout, parameter list, renderer-provided values, or a custom-display callback triple). The host
builds the snapshot and descriptors, loads (version check → v2 table or v1 adapter), and offers
`start()`, `dispatch(i, x, y, w, h, pixels)`, `finishAll()`. `CRenderer::dispatch` keeps exposure,
the imager and the per-display channel packing, and calls `CDisplayHost::dispatch`. The file-static
`currentDisplay` and the free function `findParameter` are deleted.

**Rationale.** SC-005 (100 randomized start orders) and SC-006 (1,000 start-dispatch-finish cycles
on ≥ 8 threads) are only practical against a unit a test can drive without a frame. It also gives
the one place where the v1 adapter is selected (FR-006).

**Alternatives.** Drive SC-005/006 with full renders: 1,000 renders is minutes of CPU per run and
cannot randomize start order. Leave everything in `CRenderer`: the statics stay the coupling DB-014
names.

## R9. One shadeop module, in its own library

**Decision.** A new Shader DSO Operands directory `src/shadeop/` holds `shadeop.h` (moved with
`git mv` from `src/ri/render/`), the declaration parser (`shadeopDeclaration.{h,cpp}`) and the
module (`shadeopModule.{h,cpp}`): directory scan (a plain `const char *const *dirs, int n`, not
either `TSearchpath`), `<name>_shadeops` lookup, version check, declaration parse, `exec`
resolution with the `exec[0]` test (FR-013/033), init, per-thread state and release. It builds as
`openrendershadeop_obj` → `libRIBShadeop` SHARED + STATIC, linked by `ribRender`,
`libshader_shading` and `libshader_compiler`. The compiler's `enumerateDso` and the renderer's
`getDSO` both call it; `dso.l`/`dso.y`/`dso.h` are deleted, removing the parser from all four
libraries (M5) and the three DB-019 `dso.h` edges.

The declaration parser is hand-written: `type name ( [type [name] {, type [name]}] )` with `output`
ignored, the same eight type letters and the same `<ret>=<params>` prototype string; it is
reentrant (all state on the caller's stack), bounds-checked, and returns `std::string`s. A
characterization test, written first against the old `dsoParse`, fixes the accepted language,
including inputs it rejects. The only intended difference: an unknown character is skipped
silently instead of being echoed to stdout by flex's default rule.

**Rationale.** FR-012/037 ask for one module, one parser and no shared file statics; a hand-written
parser for a grammar this small removes two generated sources from four libraries and the shared
buffer. A library of its own satisfies Principle VIII rule 5 and gives Shading a link target for
the per-thread lookup without an upward edge into `libRIBRender` (the DB-001 pattern).

**Alternatives.** (a) Keep bison, make it pure/reentrant, compile it once into the new library:
more build machinery for the same result. (b) Compile the module into `libRIBCommon`: places a
domain in the infrastructure library. (c) A static archive linked into `ribRender` and
`libshader_compiler`: two copies of the parser in two shared objects, which is what FR-037 removes.
Q1 resolved: the maintainer approved the new installed library.

## R10. Per-thread shadeop state (shadeop 1.1.0)

**Decision.** `SHADEOP_SPEC` cannot grow (it is an array; a new field changes its stride, a MAJOR
change). A 1.1 module therefore exports an optional parallel table
`<name>_shadeop_threads[]` (`SHADEOP_THREAD_SPEC { const char *threadInit; const char
*threadCleanup; }`, same order and length as `<name>_shadeops`, `""` for none), declared with
`SHADEOP_THREAD_TABLE(name)`, and read only when the module reports MINOR ≥ 1. Signatures:
`void *threadInit(void *initdata, int thread)` and `void threadCleanup(void *initdata, void
*threaddata)`. The operand keeps one slot per shading thread index (`CShadingContext::thread`).
`numThreads` is an option and can change from frame to frame (`CRenderer::numThreads =
o->numThreads` in `beginFrame`, `renderer.cpp:236`), so the registry's `beginFrame(numThreads)`,
called by the core in `CRenderer::beginFrame` before any shading thread exists, grows every
operand's slot array to the largest count seen (single-threaded point, no lock on the hot path).
The first `exec` on a thread calls `threadInit` on that thread and stores the result in that
thread's slot; `exec` then receives the slot instead of the process-wide data. At release
(`RiEnd`, `CRenderer::shutdownFiles`) the module calls `threadCleanup` for every initialized slot,
then `cleanup`, then unloads. A thread index outside the slot array (a context the core did not
size for) gets the process-wide data with a one-time diagnostic. Without the parallel table, an
operand behaves exactly as today. The header and guide state that `exec` may run concurrently on
several threads.

**Rationale.** MINOR-compatible (FR-011), no change to the legacy table, no lock on the hot path
(each thread writes only its slot), and cleanup runs for every init before unload.

**Alternatives.** Per-operand symbol naming conventions (`<init>_thread`): invisible in the
table, easy to misspell. A `thread_local` per operand: cleanup would have to run on each owning
thread before it exits, which the host does not control. Re-reading `init(ctx, texturectx)`'s
`ctx` as a thread index (PRMan's meaning): changes the meaning of an existing 1.0 entry point.

## R11. One Procedures module, and how each caller reaches it

**Decision.** `src/procedures/` (the Procedures domain) holds `dlo.h` and `implicit.h` (moved with
`git mv` from `src/ri/geometry/`) and `proceduralModules.{h,cpp}`: `acquireDloModule(path)`,
`acquireImplicitModule(path)`, `acquireDynamicLoadModule(path)` each return a reference-counted
`CProceduralModule` (base `CRefCounter`) holding the resolved entry points, after the version check
of R1. It is compiled as `openrenderprocedures_obj` into `ribRender` only (its only consumer;
ribVector and ribOut compile neither the DLO nor the implicit object nor the hook below).
- **Renderer core** (`rendererContext.cpp`): the `"dlo"` and `"implicit"` branches acquire the
  module for the **located** path (FR-023/032) and pass it to `CDLObject`/`CImplicit`, keeping their
  error texts (which name the RIB `name`).
- **Geometry**: `CDLObject` and `CImplicit` hold the reference and release it in their destructors
  (`Geometry -> Procedures` is allowed). `CImplicit`'s constructor no longer loads anything.
- **Scene Interface**: `RiProcDynamicLoad` (`ri.cpp`) calls a Scene-owned hook,
  `src/ri/parse/procDynamicLoadHook.h` (`setProcDynamicLoadHook()`), which the renderer core
  installs in `beginRenderer()` and removes in `endRenderer()`. With no hook installed (ribOut,
  ribVector), the function reports one error naming the generator and returns, as it does today
  for a module that cannot be found. The core's implementation acquires the generator module,
  registers a frame reference, and calls `ConvertParameters`/`Subdivide`/`Free`.
- **Release**: the frame references of DynamicLoad generators are dropped by
  `releaseFrameProceduralModules()`, called from `CRenderer::endFrame` immediately after
  `root->destroy()` (M6), so a nested procedural whose functions live in the generator is destroyed
  before its code is unmapped (FR-024/034).

**Rationale.** Scene Interface may depend only on Graphics State and Geometry, and `ri.cpp` is also
compiled into `ribOut`, which compiles no Geometry source: a route through Geometry would add
Geometry sources to `ribOut`. A Scene-owned hook implemented by the composition root is the
pattern spec 024/025 used, and adds no forbidden edge.

**Alternatives.** A Geometry-owned entry (`CGeometryHost::expandDynamicLoad`): needs
`geometryHost.cpp` in ribOut/ribVector or a stub pair (DB-017's pattern, which is being retired).
Loading in `CImplicit`'s constructor through Procedures: allowed, but keeps two acquisition sites
for the two shapes of the same thing; acquiring in the core matches the DLO site and keeps the
`implicitSurface.cpp` diff (which spec 028 also edits) small.

## R12. "Once per module per run"

**Decision.** "Run" is the process. `checkPluginVersion()` caches its verdict per (family,
`realpath` of the module) in a mutex-protected map in `libRIBCommon`; the second and later loads of
the same module reuse the verdict and print nothing. The record survives the module's unload.

**Rationale.** `orender` is one session per process; in-process multi-session tests
(`RibParse_SessionsReadArchives` style) would otherwise repeat warnings; ctest runs each test in its
own process, so tests do not interfere.

**Alternatives.** Per `RiBegin` session: needs the core to reset an infrastructure record and gives
the compiler (no session) different rules.

## R13. Texturing's channel interface (US7)

**Decision.** Texturing owns `src/ri/texture/texture3dChannels.h`:
`struct STexture3dChannelInfo { const char *name; int numSamples; int type; const float *fill; }`
and `class CTexture3dChannelSource { virtual bool find(const char *, STexture3dChannelInfo &) const
= 0; }` with `setTexture3dChannelSource()`/`texture3dChannelSource()`. The renderer core installs an
implementation backed by `channelRegistry()->retrieve()` next to the services registration
(`beginRenderer`/`endRenderer`). `defineChannels(const char *)` asks the installed source; with none
installed (no session) it reports the same "Unknown display channel" error as today. `fill` stays
aliased to the registry's array, as today (M7). `texture3d.cpp` drops `renderer.h` and
`displayChannel.h`; `CRenderer::retrieveDisplayChannel` loses its last caller and is deleted.

**Rationale.** It removes the three DB-021 edges with no behaviour change, in both session states,
following the services/hook pattern of 024/025/027.

**Alternatives.** Pass a channel table into `CTexture3d`'s constructor: changes four constructors
and the remote channel path for no gain. Move the registry into Texturing: wrong owner (spec 025
gave it to Graphics State).

## R14. Install, `file_base.h` and the guides (US8)

**Decision.** One CMake list, `OPENRENDER_PUBLIC_HEADERS` (root `CMakeLists.txt` or
`CMake/openRenderPublicHeaders.cmake`), names every installed header: `ri.h`, `ptcapi.h`,
`dsply.h`, `shadeop.h`, `dlo.h`, `implicit.h`, `orplugin.h`, `orplugin.hpp`. `install(FILES ...)`
uses it, into the layout's include directory (self-contained `<ORENDERHOME>/include`; FHS `<prefix>/include/openRender/`, maintainer's decision 2026-10-30; the package config follows); `file_base.h`'s `install()` line is removed. A test stages exactly that list into a
scratch include directory and compiles each header alone (C17 and C++20) plus the guide's example
display (`examples/display/simpledisplay.c`, a v2 driver in C) against it; a second script checks
that `INSTALL_ARTIFACTS.md` and `DISPLAY_PLUGIN_GUIDE.md` name exactly that list and that every
include or source path they mention exists. A real `DESTDIR` install is a quickstart step, not a
ctest (memory: a past `DESTDIR` test leaked into `/usr/local`; `--prefix` is GitHub #23).

**Rationale.** Mechanical checks for FR-026/036/038 and SC-011 that run on every build without
touching the system prefix.

**Alternatives.** A ctest that runs `cmake --install` into a temporary prefix: fails in
self-contained mode (#23) and is the step that leaked before.

## R15. Plug-in test infrastructure

**Decision.** `tests/plugins/` builds every test module from the tree's own CMake as `MODULE`
targets with `PREFIX ""` and `SUFFIX ".dsply"`, into per-purpose directories under
`${CMAKE_BINARY_DIR}/tests/plugins/<family>/<variant>/`, so search-path order (FR-033) is
controlled by directory, not by `osEnumerate`'s filesystem order. One source per family is compiled
into several variants with compile definitions (`TEST_ABI_MAJOR/MINOR`, `TEST_NO_VERSION`,
`TEST_BEHAVIOR=...`); the hidden-visibility variants set `C_VISIBILITY_PRESET hidden`. Plug-ins
record every entry-point call to a log file named by `OR_TEST_PLUGIN_LOG`, so "never entered"
(SC-001) is checked from the log. Test programs link `ribRender` and drive the real loaders
in-process with paths passed as arguments (`$<TARGET_FILE_DIR:...>`), capture diagnostics with an
installed `CDiagnosticSink`, and use `CDisplayHost` directly for SC-005/006. Render-level checks
(DLO, implicit, DynamicLoad, shadeop under `.rslo`) use small RIB scenes under
`tests/plugins/scenes/` with `Option "searchpath" "procedural"`/`"display"` set to the build-tree
directories. All tests carry the label `plugin_abi`. macOS `MODULE` targets are bundles (`dlopen`
works); Linux gets `.dsply` shared objects; both are covered by the same CMake.

**Rationale.** Nothing depends on the gitignored deploy tree or on compiled-in install paths;
everything is rebuilt by `cmake --build`.

**Alternatives.** Pre-built binary plug-ins checked into the tree: not reproducible, platform
specific. One source file per variant: dozens of near-identical files.

## R16. Pixel baselines for FR-021 / SC-004 / SC-013

**Decision.** The first implementation phase (before any driver or host change) adds
`tests/plugins/display_baseline/` with deterministic scenes (box filter, fixed seed, `PixelSamples
1 1`) for TIFF 8/16/float, PNG 8/16, OpenEXR half/float, RGBE, a depth (`z`) display and an AOV
display with several channels, plus the cases that exercise every lookup the migration touches:
- a **dithered** 8-bit output (spec 039 made the dither a hash of pixel and channel, so it is
  deterministic and comparable);
- a display-line `"float quantize"` override (the RIB-beats-renderer precedence);
- a `"string type" "tiff"` override on a `.png` name, and a `.png` name sent to `"file"` with no
  type (PNG/TIFF selection through `type`);
- a PNG with `Exposure` gamma ≠ 1, whose `gAMA` chunk is checked separately (gamma never reaches
  the pixels, so a decoded-pixel compare cannot see it).

It renders them with the unmodified drivers and stores **decoded pixels** (not bytes: TIFF/EXR
headers carry dates and software strings) as reference data, decoded with spec 018's
`CImageInput` (TIFF, PNG, OpenEXR, RGBE). The comparison test decodes the current output and
requires zero differing samples.

The capture is **not a ctest**: it is an explicit build target (`plugin-abi-capture-baselines`)
that refuses to overwrite an existing reference unless `-DOR037_RECAPTURE=ON` is passed at
configure time, so `ctest -L plugin_abi` (a regular expression) can never re-run it and turn a
post-change output into its own baseline.

The framebuffer driver is covered by its existing tests (`tests/framebuffer`; none of them
`dlopen`s `framebuffer.dsply` or calls a `display*` symbol, so the v2 migration does not touch
them) plus a new v2 smoke test through `CDisplayHost`. The "legacy driver built before this
spec" for SC-013 is built by the test's CMake from **frozen copies** of the base tree's
`file.cpp`, `file_tiff.*`, `file_png.*`, `file_base.*` and `dsply.h`, stored under
`tests/plugins/display/legacy_v1/` with the header renamed (`dsply_v1_frozen.h`), because
`ribRender`'s PUBLIC include directories contain `src/display/` and would otherwise hand the
fixture the new 2.0 `dsply.h`.

**Rationale.** SC-004 requires an exact comparison against pre-change output; capturing it first
is the only way to have it.

**Alternatives.** Compare against the visual suite's references: those use the block-average
tolerance metric, which SC-004 excludes.

## R17. TSan

**Decision.** An opt-in CMake option `OPENRENDER_TSAN` (adds `-fsanitize=thread` to C and C++
compile and link flags, forces `OPENRENDER_ENABLE_JIT=OFF` to keep LLVM out of the instrumented
build) used in a separate build tree `build-tsan`. Only `ctest -L plugin_abi` runs there, through
`cpujob` at `-j2`, as a validation step listed in quickstart, not in the everyday run.

**Rationale.** SC-006 asks for a TSan run of the display, shadeop and procedural loading paths;
the test plug-ins are built in the same tree and are therefore instrumented too.

**Alternatives.** TSan on the main build tree: would slow every everyday run and the JIT build.

---

## Maintainer decisions (open questions resolved 2026-10-29, plan approved)

All ten questions were answered; each answer adopts the recommendation.

| # | Question | Resolution |
|---|---|---|
| Q1 | New installed library `libRIBShadeop` (R9) | **Resolved: yes.** Shared + static, exported in the package targets. |
| Q2 | Lifetime of per-thread shadeop data (R10) | **Resolved: the session.** `threadCleanup` runs at `RiEnd` (`shutdownFiles`). |
| Q3 | Where the DynamicLoad version macro lives | **Resolved: `dlo.h`**, with its own 1.0.0 history section. |
| Q4 | `RiProcDynamicLoad` and the search path | **Resolved: unchanged.** The generator name is loaded exactly as written. |
| Q5 | `RtDisplayRawDataFunction` in `ri.h`/`riVocabulary.h` | **Resolved: left as is.** Both headers unchanged; FR-019 is met by removing the cast and the `dsply.h` duplicate. |
| Q6 | The v1 adapter's scoped `thread_local` (R6) | **Resolved: complies** with FR-006/FR-014. |
| Q7 | Display parameters of type quad/mpoint/double (M4) | **Resolved: fixed in 037 as FR-040** (spec.md), test `DisplayAbi_UnsupportedParamType` under SC-014; see R18. |
| Q8 | Serialized `data` for v1 drivers (R6) | **Resolved: yes**, per-display mutex in the adapter. |
| Q9 | Principle VII (Hugo site) | **Resolved: add a short `docs/site` page** for plug-in authors; see R19. |
| Q10 | Hand-written parser vs reentrant bison (R9) | **Resolved: hand-written**, fixed by the characterization test. |

## R18. FR-040: unsupported display parameter types

**Decision.** In `CRendererContext::RiDisplayV`, the `TYPE_QUAD`/`TYPE_MPOINT`/`TYPE_DOUBLE` case
frees the `strdup`'d name, prints one warning (`CODE_BADTOKEN`, "Display parameter \"%s\": type
not supported by display drivers, ignored") and does not increment `j`, so `numParameters` counts
only initialized entries. The snapshot builder still skips any type it does not know (defence in
depth). Test `DisplayAbi_UnsupportedParamType` (in-process, links `ribRender`): `RiDisplayV` with a
`"double dd"` (and a `"quad"`/`"mpoint"` token where `parseVariable` accepts them) plus a float
parameter; asserts the stored display has exactly the float parameter, the warning is printed once
per parameter, a `COptions` clone (the `CDisplay` copy constructor) and the frame's snapshot
(`v2_record`) see only that parameter, and the run is clean under ASan/TSan builds.

**Rationale.** The copy constructor (`options.cpp`) switches on the uninitialized `type`, and the
FR-015 snapshot copies `data`; both are undefined behaviour today. No driver consumes these types
(the existing comment says so), so dropping them changes no image.

**Alternatives.** Silently drop: hides a user error. Store them as float arrays: invents a meaning
no driver reads.

## R19. Plug-in author page on the site (Principle VII)

**Decision.** A new page `docs/site/content/manual/reference/plugin-abi.md` (Hugo front matter like
its siblings): interface versioning (encoding, what MAJOR/MINOR/PATCH mean, the per-family version
macro and symbol, what the host does with each outcome, the 1.2.0 sunset for unversioned
plug-ins), the shared convention in brief (table header, size rule, status codes, ownership), and
"migrating a display driver to 2.0" (entry points, snapshot lookup, descriptors, threading and
`finish` rules, with the `examples/display/simpledisplay.c` example). It links to the installed
headers and to `DEVNOTES_DETAILS/DISPLAY_PLUGIN_GUIDE.md` and `PLUGIN_ABI.md` for detail, and the
existing `display-drivers.md` and `dso-shading.md` pages link to it. `PluginAbi_DocPaths` also checks
the paths this page names.

**Rationale.** Principle VII keeps user-facing documentation in the Hugo site; third-party plug-in
authors are users of the installed headers.

**Alternatives.** DEVNOTES only: not published on the site.
