# Contract: the declaration table and the Scene-owned services (FR-001-FR-003, FR-026)

Research R1-R4. Declarations below are the planned interfaces; names may be adjusted by the tasks
stage as long as the behaviour and the ownership hold.

## 1. Graphics State: `src/ri/state/declarations.{h,cpp}`

```cpp
class CVariable;

// The declared-variable table (moved from CRenderer's statics, spec 038). One per process at a time,
// reference counted. Single-threaded: acquired and released by the session's thread.
class CDeclarations {
    public:
        // First acquire: create the table; begin the channel registry; declare the standard set in
        // exactly today's order (rendererDeclarations.cpp:96-282, so every VARIABLE_* entry index is
        // unchanged); install installVariableLookup/installVariableDeclaration (variableLookup.h) on
        // this table. Later acquires only count.
        static void acquire();
        // Last release: uninstall the two hooks, end the channel registry, destroy the table. A
        // release with nothing acquired does nothing.
        static void release();
        static bool active();

        // CRenderer::declareVariable's semantics unchanged: parse, redeclaration diagnostics
        // ("Variable \"%s\" was previously defined differently\n", CODE_SYSTEM), the "st" rule, and for
        // global storage: number the variable (entry = count, storage = STORAGE_GLOBAL), append it to the
        // global array, then call the observer. nullptr if the declaration does not parse or no table.
        static CVariable *declare(const char *name, const char *type, int marker = 0);
        static CVariable *find(const char *name);           // nullptr if absent or no table

        static int numGlobals();
        static CVariable *global(int index);

        // Called after a global variable is added; the renderer core installs the notification of
        // its live shading contexts (today's makeGlobalVariable loop). nullptr uninstalls.
        using TGlobalObserver = void (*)(CVariable *);
        static void setGlobalObserver(TGlobalObserver observer);
};
```

Ownership: Graphics State. Dependencies: Infra only (`common/variable.h`, `common/containers.h`, RI
tokens from `common/riVocabulary.h`, `rendererc.h` for `PARAMETER_*`/`VARIABLE_*`), plus Graphics
State's own `channelRegistry.h` and `variableLookup.h`. Compiled into `ribRender`, `ribVector` and
`ribOut`.

## 2. Scene Interface: `src/ri/parse/riServices.{h,cpp}`

```cpp
class CVariable;
struct TSearchpath;

class CRiDeclarationService {
    public:
        virtual ~CRiDeclarationService() = default;
        virtual CVariable *retrieveVariable(const char *name) = 0;            // nullptr: not declared
        virtual CVariable *declareVariable(const char *name, const char *type) = 0;
};

class CRiFileSearchService {
    public:
        virtual ~CRiFileSearchService() = default;
        // TRUE and the path in result (OS_MAX_PATH_LENGTH) when found
        virtual int locateFile(char *result, const char *name, TSearchpath *searchpath, int tryNormalize) = 0;
};

// Install a service; nullptr uninstalls. Returns the service installed before, which the caller
// reinstalls at the end of its session (a nested session restores the outer one).
CRiDeclarationService *riInstallDeclarationService(CRiDeclarationService *service);
CRiFileSearchService *riInstallFileSearchService(CRiFileSearchService *service);

// What the grammar, the lexer and ri.cpp call.
CVariable *riRetrieveVariable(const char *name);
//   no service: nullptr (rib.y then reports "Parameter \"%s\" is not declared\n" as today)
int riLocateFile(char *result, const char *name, TSearchpath *searchpath, int tryNormalize);
//   no service: TRUE only for a name containing a directory separator that exists (no search, no
//   diagnostics beyond the ones that branch prints today)

// The Scene's standard implementation (writer, preview, data viewer): the Graphics State table, and
// the search of research M4 without the network. Scoped:
class CRiStandardSession {
    public:
        CRiStandardSession();   // CDeclarations::acquire(); install the standard services, keeping the previous ones
        ~CRiStandardSession();  // reinstall the previous services; CDeclarations::release()
        CRiStandardSession(const CRiStandardSession &) = delete;
        CRiStandardSession &operator=(const CRiStandardSession &) = delete;
};
```

The standard file search, in order (research M4, R3), all `info(CODE_RESOLUTION, ...)` texts as in
`rendererFileSearch.cpp`:
1. a name containing a directory separator: as given, if it exists;
2. otherwise each search-path directory, then the bare name relative to the working directory;
3. if `tryNormalize`: the same with `/` and `\` replaced by `_`, `tryNormalize` off;
4. `"\"%s\" -> ???\n"` and FALSE.

## 3. Renderer core

- `CRenderer::initDeclarations()`: `CDeclarations::acquire()`, the coordinate systems, the global
  identifiers, `CDeclarations::setGlobalObserver(&CRenderer::notifyGlobalVariable)`, and the
  installation of the core's `CRiDeclarationService` (`retrieveVariable` → `CDeclarations::find`,
  `declareVariable` → `CRenderer::declareVariable`) and `CRiFileSearchService` (→
  `CRenderer::locateFile`), keeping the previous ones. `shutdownDeclarations()` reverses it in the
  opposite order.
- `CRenderer::retrieveVariable`, `declareVariable(name, type, marker)` forward to `CDeclarations`;
  `locateFile` keeps its body (network mapping, `temporaryPath`, network fetch). Out of line, in
  their current files (research R4).
- `CRenderer::makeGlobalVariable` becomes `notifyGlobalVariable(CVariable *)`: the real body
  (`rendererDeclarationsDispatch.cpp`) keeps the context loop; the stub
  (`vector/rendererDeclarationsDispatchStub.cpp`) does nothing.
- The statics `declaredVariables`, `variables`, `globalVariables` leave `renderer.h:152,157,158` and
  `rendererStatics.cpp`; `rendererServicesImpl.h:41-42` reads `CDeclarations::numGlobals()/global(i)`.
- `definedCoordinateSystems`, `globalIdHash`, `numKnownGlobalIds` stay in the core.

## 4. The grammar and `ri.cpp`

| Site | Becomes |
|---|---|
| `rib.y:222` `CRenderer::retrieveVariable(par->name)` | `riRetrieveVariable(par->name)` |
| `rib.y:232-245` dead `if (FALSE)` branch | deleted; the `else` branch (the error) stays |
| `rib.l:178` `CRenderer::locateFile(...)` | `riLocateFile(...)`; and `rib.l:170` `renderMan->getOptions()`, unchecked today, becomes `renderMan ? renderMan->getOptions() : NULL` so a parse with no active context is safe (a test case in `SceneInterface_Services`) |
| `ifexpr.y:203-224` `cAttributes->find(...)` / `cOptions->find(...)` | a `nullptr` from `getAttributes(TRUE)`/`getOptions()` is "not found" (a test case) |
| `rib.y:39-40`, `ifexpr.y:28-29` core includes | deleted; `riServices.h`, `riCondition.h` and the State headers the code really needs |
| `ri.cpp:407-417` writer bootstrap | `{ CRiStandardSession session; globalArena.init(); ribParse(rcPath, NULL); globalArena.tini(); }` (same scope as today) |

## 5. Behaviour that must not change

- The declared set, the order, every entry index (`assert(tmp->entry == VARIABLE_*)` holds), the
  redeclaration diagnostics.
- Every "Parameter ... is not declared" error: same text, same place (US1-2).
- `ReadArchive` of a bare name: the same file is found by the renderer and the writer (US1-3); the
  network mapping only in the renderer.
- The writer tests that call `CRenderer::initDeclarations()` after `RiBegin` keep working unedited.
