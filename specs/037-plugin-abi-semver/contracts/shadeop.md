# Contract: shadeop interface 1.1.0 and the shadeop module (FR-001..FR-005, FR-011..FR-013, FR-031, FR-033, FR-037, US4)

Shader DSO Operands domain, `src/shadeop/` (new; `domain-map.txt`: `prefix src/shadeop/ ShaderDSO`).

| File | Installed | Content |
|---|---|---|
| `shadeop.h` | yes | the ABI (moved with `git mv` from `src/ri/render/shadeop.h`) |
| `shadeopDeclaration.{h,cpp}` | no | the declaration parser (replaces `src/ri/parse/dso.{l,y,h}`) |
| `shadeopModule.{h,cpp}` | no | scan, table lookup, version check, operand acquisition, per-thread state, release |
| `CMakeLists.txt` | — | `openrendershadeop_obj` → `openrendershadeop` (SHARED, `libRIBShadeop`) + `openrendershadeop_static` |

## 1. `shadeop.h` (public, C17 / C++20)

```c
#include "orplugin.h"

#define SHADEOP_ABI_VERSION_MAJOR 1
#define SHADEOP_ABI_VERSION_MINOR 1
#define SHADEOP_ABI_VERSION_PATCH 0
#define SHADEOP_ABI_VERSION OR_ABI_VERSION(1, 1, 0)
#define SHADEOP_DECLARE_VERSION() OR_DECLARE_ABI_VERSION_SYMBOL(orShadeopAbiVersion, SHADEOP_ABI_VERSION)

typedef struct {
    const char *definition;  /* "float name(float, point)"; "" terminates the table */
    const char *init;        /* name of the SHADEOP_INIT function, or ""            */
    const char *cleanup;     /* name of the SHADEOP_CLEANUP function, or ""         */
} SHADEOP_SPEC;              /* layout unchanged from 1.0 (char * -> const char *)  */

typedef struct {             /* 1.1: optional, parallel to <name>_shadeops[]        */
    const char *threadInit;    /* "" = none */
    const char *threadCleanup; /* "" = none */
} SHADEOP_THREAD_SPEC;

#define SHADEOP_TABLE(n)        OR_PLUGIN_EXPORT SHADEOP_SPEC n##_shadeops[]; SHADEOP_SPEC n##_shadeops[]
#define SHADEOP_THREAD_TABLE(n) OR_PLUGIN_EXPORT SHADEOP_THREAD_SPEC n##_shadeop_threads[]; \
                                SHADEOP_THREAD_SPEC n##_shadeop_threads[]
#define SHADEOP_INIT(f)         OR_PLUGIN_EXPORT void *f(int ctx, void *texturectx)
#define SHADEOP(f)              OR_PLUGIN_EXPORT void f(void *initdata, int argc, void *argv[])
#define SHADEOP_CLEANUP(f)      OR_PLUGIN_EXPORT void f(void *initdata)
#define SHADEOP_THREAD_INIT(f)    OR_PLUGIN_EXPORT void *f(void *initdata, int thread)
#define SHADEOP_THREAD_CLEANUP(f) OR_PLUGIN_EXPORT void f(void *initdata, void *threaddata)

typedef void *(*dsoInitFunction)(int, void *);
typedef void  (*dsoExecFunction)(void *, int, void *[]);     /* == SHADEOP (FR-031) */
typedef void  (*dsoCleanupFunction)(void *);
typedef void *(*dsoThreadInitFunction)(void *, int);
typedef void  (*dsoThreadCleanupFunction)(void *, void *);
```

The `_WINDOWS` branch keeps its `__declspec(dllexport)` spelling through `OR_VISIBLE`.

Version history:

| Version | Change |
|---|---|
| 1.0.0 | The Pixie-era interface. Modules without `orShadeopAbiVersion` are 1.0.0. In the 1.0.0 header as shipped by 037: C-includable, default-visibility exports, `SHADEOP_SPEC` fields `const char *` (same layout), `SHADEOP`'s `argv` is `void *[]` to match `dsoExecFunction` (same binary; source using `argv[i]` without a cast to do pointer arithmetic must add one). |
| 1.1.0 | Optional `<name>_shadeop_threads[]` with per-thread init/cleanup. |

Documented threading rule (header and guide): `exec` may be called concurrently from several
shading threads. Without per-thread entry points every call receives the same `initdata`; with
them, each thread receives its own data. `threadCleanup` may run on a different thread from the
matching `threadInit`, after every `exec` using that data has returned.

## 2. Declaration parser (`shadeopDeclaration.h`)

```cpp
struct SShadeopDeclaration { std::string name; std::string prototype; };
// Parses "type name ( [type [name] {, type [name]}] )". Returns false on any syntax error.
// Reentrant: no static state. Accepts exactly what dso.l/dso.y accepted (types void float vector
// normal point color matrix string; 'output' ignored anywhere; names [A-Za-z0-9_]+; blanks, tabs,
// newlines skipped); any other character is skipped silently (flex echoed it to stdout).
bool parseShadeopDeclaration(std::string_view decl, SShadeopDeclaration &out);
```

Prototype letters: `o f v n p c m s`; format `<ret>=<p1><p2>...` (e.g. `"float f(point, color)"` →
`"f=pc"`). No length limit (the old 256-byte buffer had no bound).

## 3. Module API (`shadeopModule.h`, internal C++)

```cpp
struct SShadeopSearchPath { const char *const *directories; int count; }; // caller converts its TSearchpath; each entry ends with a separator, as a TSearchpath holds it

// Compiler: every declaration of every "<name>_shadeops" found on the path, in scan order
// (directories in search-path order; within a directory, module file names in lexicographic order, not osEnumerate order: FR-013). Version-checked; rejected modules
// are skipped with their diagnostic. Loads and unloads each module.
void enumerateShadeopDeclarations(const SShadeopSearchPath &, const char *name,
                                  const std::function<void(const SShadeopDeclaration &)> &);

class CShadeopOperand;   // Shading keeps seeing it through CDSO (dsoHandle.h), see §4

class CShadeopRegistry { // owned by the caller (the renderer core: one per RiBegin..RiEnd)
    public:
        CShadeopRegistry();
        ~CShadeopRegistry();  // releases every operand: threadCleanup per slot, cleanup, unload
        // Called by the core in CRenderer::beginFrame, before any shading thread exists: grows
        // every operand's slot array to max(numThreads seen) (numThreads is a per-frame option).
        void beginFrame(int numThreads);
        // The first operand whose definition parses to (name, prototype) and whose exec symbol
        // resolves (FR-013/033); NULL if none. Cached by (name, prototype).
        CShadeopOperand *acquire(const SShadeopSearchPath &, const char *name, const char *prototype);
};

// Called by the interpreter for every DSO call (the DSOEXEC_PRE macros):
void *shadeopOperandData(CShadeopOperand *, int thread);  // per-thread slot (lazy threadInit) or initData
dsoExecFunction shadeopOperandExec(const CShadeopOperand *);
```

Scan rule per module: `checkPluginVersion(shadeop)` first; skip on Reject. For each table entry:
parse; on a prototype match resolve `exec` **into the entry's own variable** and test that
(`exec[0]`), not the output array (FR-033). An unresolved `exec` prints "shadeop `<module>`:
`<symbol>` not found" once and the scan continues, to later entries and later modules. Modules that
supply nothing are unloaded at once.

## 4. Shading side

`src/libshader/shading/dsoHandle.h` keeps `CDSO` as a **real class** (it is forward-declared as
`class CDSO;` in `shader.h:53`, `serviceLookup.h:32`, `rendererServices.h:36` and `renderer.h:99`,
which a typedef cannot satisfy). It holds a `CShadeopOperand *` (plus `name`/`prototype` for the
existing cache comparisons) and includes `shadeop/shadeopModule.h`; `scriptFunctions.h`'s
`DSOEXEC_PRE`/`DSOVOIDEXEC_PRE` read `shadeopOperandData(code->dso->operand, thread)` and
`shadeopOperandExec(code->dso->operand)` instead of the `handle`/`exec` fields. The renderer's
registry owns the `CDSO` objects as it owns the operands. `libshader_shading` links `openrendershadeop` (Shading → ShaderDSO is
allowed). `CRendererServices::getDSO` keeps its signature. The JIT has no DSO path (DB-019 / spec
038); nothing changes there.

## 5. Renderer and compiler

- `CRenderer::getDSO` builds `SShadeopSearchPath` from `proceduralPath` and calls
  `registry->acquire`. `CRenderer::dsos` is replaced by `CRenderer::shadeops` (a
  `CShadeopRegistry *`, created in `initFiles`, sized per frame by `beginFrame(numThreads)`,
  deleted in `shutdownFiles`). An operand acquired mid-frame is sized at acquisition from the
  current frame's count.
  `rendererStatics.cpp` (also in ribOut) only defines the null pointer.
- `CScriptContext::enumerateDso` builds the path from `dsoPath` and calls
  `enumerateShadeopDeclarations`; `rslo.cpp`'s copy of the scan and both `dsoParse` copies are
  deleted. `libshader_compiler` links `openrendershadeop`.
- `src/ri/parse/dso.{l,y,h}`, their `flex_file`/`bison_file` lines in `src/ri/CMakeLists.txt` and
  `src/libshader/compiler/CMakeLists.txt`, `dso_generated.cpp` in `ribOut_sources`, and the unused
  `dso.h` includes (`renderer.cpp`, `rendererContext.cpp`, `rendererStatics.cpp`, `shading/rslo.y`)
  are removed.

## 6. Tests

| Test | Covers |
|---|---|
| `ShadeopAbi_DeclarationCharacterization` | written **first** against the old `dsoParse`: a table of ~40 declarations (all types, `output`, names, blanks, missing name, syntax errors, unknown characters, 300-character declarations) and their outputs; then re-pointed at `parseShadeopDeclaration` with identical expectations (except stdout echo) |
| `ShadeopAbi_ParserReentrant` | 8 threads × 10,000 parses of different declarations, all correct; part of the TSan run (FR-037) |
| `ShadeopAbi_ParserSingleDefiner` | `nm`: `parseShadeopDeclaration` defined only in `libRIBShadeop`; no `dsoParse`/`dsoparse`/`dso_scan_string` symbol in any library (FR-037, SC-008) |
| `ShadeopAbi_Versions` | good, newer (1 warning), v2 (rejected; its init/exec never logged), legacy (1 warning across 2 operands) through `CShadeopRegistry` and through a `.rslo` render (SC-001, SC-002) |
| `ShadeopAbi_MissingExec` | `missing_exec` in directory A, `has_exec` in directory B: B selected, diagnostic names the symbol (FR-013, FR-033, US4-4) |
| `ShadeopAbi_PerThread` | `good` on N = 8 threads through `shadeopOperandData`: N distinct data, N cleanups after release, cleanup before unload (SC-007, US4-1) |
| `ShadeopAbi_ThreadLifetime` | threadInit lazily once per thread index, concurrent first calls on 8 threads; NULL init result passed through; slots grow when `numThreads` rises between frames with existing data kept; threadCleanup runs at `RiEnd` (not frame end), on a thread other than the init thread, after the last `exec` and before unload (FR-011, SC-007) |
| `ShadeopAbi_ScanOrder` | two directories and two modules in one directory defining one prototype: the same module is chosen by the renderer and by `enumerateShadeopDeclarations`, in the order of FR-013 (SC-007) |
| `ShadeopAbi_LegacyShared` | `unaware`: one data pointer for all threads, render output unchanged (US4-2) |
| `ShadeopAbi_CompilerAndRenderer` | `oshader -I <dir>` resolves a test operand; the render resolves the same operand; neither links a `dso.h` symbol (US4-3) |
| `ShadeopAbi_SignatureMatches` | C TU: `dsoExecFunction p = f;` for a `SHADEOP(f)` under `-Werror`; C++ TU: `static_assert(std::is_same_v<decltype(&f), dsoExecFunction>)` (FR-031, US2-4) |
| `ShadeopAbi_ExampleBuilds` | `examples/dsotest` built as a test module with `SHADEOP_DECLARE_VERSION()` (FR-008) |
