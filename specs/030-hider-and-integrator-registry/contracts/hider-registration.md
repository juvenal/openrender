# Contract: hider registration (version 2.0.0)

**Domain**: Hiders (`src/ri/hiders/`). **Consumers**: the renderer core (`render/rendererTransport.cpp`),
tests, and from spec 051 dlopened hider modules. **Research**: R1, R2, R5, R6, R10.

Registration level only (FR-011, clarify answer): the table says *what* a hider is and *how to build one*;
the hider's runtime (`CHider`, `renderingLoop()`, the tracer and producer wiring) stays internal C++ and is
documented as not external until spec 051.

## Files

| File | Language | Content |
|---|---|---|
| `src/ri/hiders/hiderAbi.h` | C17 + C++20 | `OR_HIDER_ABI_VERSION`, `OrHiderAlias`, `OrHiderDispatch`, `OrHiderCreateInfo`, `OrHiderTable`, opaque `OrHiderInstance`/`OrHiderFrame`, the version-history table. Includes only `orplugin.h` and `transport/capability.h`. |
| `src/ri/hiders/hiderAbi.hpp` | C++20 | `SHiderFrame`, `hiderOf()`, `frameOf()`, `OR_REGISTER_HIDER(getter)`. Internal. |
| `src/ri/hiders/hiderPeers.h` | C++20 | `CHiderPeers { CHider *const *members; int count; }`, header-only. |
| `src/ri/hiders/hiderRegistry.{h,cpp}` | C++20 | `CHiderRegistry`, `hiderRegistry()`, `kHiderFamily` (`SPluginFamily`). |
| `src/ri/hiders/builtinHiders.cpp` | C++20 | `registerBuiltinHiders(CHiderRegistry &)`: raytrace, reyes, zbuffer, photon, in that order. |
| `raytracer.cpp`, `stochastic.cpp`, `zbuffer.cpp`, `photon.cpp` | C++20 | each defines its own table and getter (`orHiderTableRaytrace()`, `orHiderTableReyes()`, `orHiderTableZbuffer()`, `orHiderTablePhoton()`). |

None of these headers is installed in this spec (not in `OPENRENDER_PUBLIC_HEADERS`).

## C table

```c
#include "orplugin.h"
#include "transport/capability.h"   /* OrCapability (Transport; Hiders -> Transport, D30) */

#define OR_HIDER_ABI_VERSION OR_ABI_VERSION(2, 0, 0)

OR_DECLARE_HANDLE(OrHiderInstance);   /* opaque: the host never dereferences it */
OR_DECLARE_HANDLE(OrHiderFrame);      /* opaque: host data valid for the create call */

typedef struct OrHiderAlias {
    const char *name;                 /* non-empty, unique across all names and aliases */
    const char *deprecationWarning;   /* NULL: plain alias; else the exact warning text, reported once per statement */
    const char *integrator;           /* FR-014, reserved: must be NULL in 2.0.0 (a non-NULL value is warned about and ignored) */
} OrHiderAlias;

typedef enum OrHiderDispatch {
    OR_HIDER_DISPATCH_BUCKETS = 0,    /* the core's bucket dispatcher (dispatchReyes) */
    OR_HIDER_DISPATCH_PHOTONS = 1     /* the core's photon-bundle dispatcher (dispatchPhoton) */
} OrHiderDispatch;

typedef struct OrHiderCreateInfo {
    uint32_t structSize;
    int thread;                       /* 0 .. numThreads-1 */
    int numThreads;
    const OrHiderFrame *frame;        /* valid until create returns */
} OrHiderCreateInfo;

typedef struct OrHiderTable {
    OrTableHeader header;             /* structSize = sizeof(OrHiderTable), abiVersion = OR_HIDER_ABI_VERSION */
    const char *name;                 /* canonical name, non-empty */
    const OrHiderAlias *aliases;      /* may be NULL when numAliases == 0 */
    int numAliases;
    const OrCapability *provides;     /* the capabilities this hider kind provides */
    int numProvides;
    OrHiderDispatch dispatch;
    OrStatus (*preDisplaySetup)(void);                         /* optional (NULL): run once per frame before displays are set up */
    OrHiderInstance *(*create)(const OrHiderCreateInfo *info); /* one per render thread; NULL on failure */
    void (*destroy)(OrHiderInstance *instance);                /* called by the core in endFrame, single-threaded */
} OrHiderTable;
```

Ownership (orplugin.h rules 1-6 apply): the table, its strings and arrays are static and outlive every
call; the host never frees them. `create` allocates, `destroy` frees. No C++ type crosses the C boundary;
for the built-in hiders `OrHiderInstance *` *is* a `CHider *`, an internal fact the C++ header encodes.

Errors: `create` returning `NULL` makes the core report `error(CODE_BUG, "Hider \"%s\" could not create its thread %d context")`
and **abort** the frame: the hiders created so far are destroyed at once, `contexts` becomes `NULL`,
`HIDER_BREAK` is set and `renderFrame()` spawns no thread (contracts/frame-transport.md). This failure happens after `beginDisplays()`, so the displays already started are finished
by `endDisplays()` as in today's "no active display" abort (a driver may write its file); it is therefore not
the pair-check refusal, whose "no output file" guarantee holds only because it runs before the displays
(contracts/frame-transport.md). Not reachable with the built-ins (they never return `NULL`).

## C++ convenience (internal)

```cpp
struct SHiderFrame {
    const CHiderPeers *peers;                       // the frame's per-thread hiders, filled as they are constructed
    const openrender::ICamera *camera;              // frameCamera()
    CAttributes *(*acquireWorldAttributes)(void *host); // context->getAttributes(TRUE); call only if needed
    void *host;
};
inline const SHiderFrame &frameOf(const OrHiderFrame *f);   // reinterpret_cast, internal hiders only
inline CHider *hiderOf(OrHiderInstance *i);                 // reinterpret_cast, internal hiders only
inline OrHiderInstance *instanceOf(CHider *h);

// Registers a table getter (const OrHiderTable *(*)(void)) from the file it appears in. For hiders that are
// not built in (tests, programs). Expands to a file-local static object whose constructor calls
// orHiderAddPending(getter); registration happens when the registry is first used.
typedef const OrHiderTable *(*OrHiderTableGetter)(void);
void orHiderAddPending(OrHiderTableGetter getter);   // defined ONCE, out of line, in hiderRegistry.cpp
#define OR_REGISTER_HIDER(getter) ...

// The pending list lives inside hiderRegistry.cpp, never in a header-reached static: a program linked
// against the shared libRIBRender would otherwise register into its own copy (gotcha 22's class of defect).
```

## Registry API (internal C++)

```cpp
class CHiderRegistry {
public:
    // Validates (R2), checks the version (checkPluginVersion overload + checkPluginTable, R10), stores.
    // Returns false and reports one diagnostic when refused; the first registration stays.
    bool add(const OrHiderTable *table, const char *origin);

    // Exact, case-sensitive. Names and aliases both resolve.
    struct SLookup { const OrHiderTable *table; const OrHiderAlias *alias; }; // alias == nullptr for a name
    SLookup find(const char *nameOrAlias) const;

    // Enumeration for diagnostics and tests (FR-015), in registration order.
    int size() const;
    const OrHiderTable *at(int i) const;
};
CHiderRegistry &hiderRegistry();   // process instance; built-ins + static registrars, once (std::call_once)
const OrHiderTable *defaultHider(); // the reyes entry: the unknown-name fallback
bool hiderProvides(const OrHiderTable *, const char *capability, uint32_t *version); // nullptr-safe
```

A local `CHiderRegistry` instance (not the process one) is what unit tests use for duplicate and collision
refusals, so they do not change the process registry.

## Built-in registrations

| Name | Aliases | Provides (all 1.0.0) | Dispatch | create |
|---|---|---|---|---|
| `raytrace` | - | `display-output`, `display-channels` | BUCKETS | `new CRaytracer(thread, *frame.camera)` |
| `reyes` | `hidden` (plain), `stochastic` (deprecated: `Deprecated Hider "stochastic"; use "reyes" instead`) | `rasterization`, `display-output`, `display-channels`, `deep-shadow-output` | BUCKETS | `new CStochastic(thread, *frame.peers)` |
| `zbuffer` | - | `rasterization`, `display-output` | BUCKETS | `new CZbuffer(thread, *frame.peers)` |
| `photon` | - | `illumination-hook` | PHOTONS | `new CPhotonHider(thread, frame.acquireWorldAttributes(frame.host))` |

`preDisplaySetup` is `NULL` for all four (their bodies were only the flag writes, research R4).

`CReyes`, `CStochastic` and `CZbuffer` constructors gain `const CHiderPeers &peers`; `CReyes` keeps the
pointer for its sibling loop. `CHider` gains `virtual void drawObject(CObject *) {}`; `CReyes::drawObject`
overrides it.

## Diagnostics

| When | Code | Text |
|---|---|---|
| duplicate name or alias | `CODE_BADTOKEN` (error) | `Hider "<x>" is already registered (by "<first>"); the second registration is ignored` |
| table without a name | `CODE_BADTOKEN` (error) | `Hider registration without a name ignored (<origin>)` |
| non-NULL pair field | `CODE_BADTOKEN` (warning) | `Hider alias "<a>" names integrator "<i>"; pair aliases are not supported yet, the alias selects the hider only` |
| version or table size | `CODE_VERSION` | `checkPluginVersion` (the built-in overload taking the declared version, maintainer answer Q5) / `checkPluginTable` texts (spec 037) |
| deprecated alias used | `CODE_BADTOKEN` (warning) | the alias's `deprecationWarning` |
| unknown name at frame begin | `CODE_BADTOKEN` (error) | `Hider "<name>" unavailable` (today's text) |

## Version history (to be written in `hiderAbi.h`)

| Version | Change |
|---|---|
| 1 | None: the hider was chosen by `strcmp` chains in the renderer core (unversioned). |
| 2.0.0 | Spec 030 (starting version confirmed by the maintainer, 2026-11-07, Q2): registration table (name, aliases with optional deprecation and the reserved pair field, provided capabilities, dispatch kind, optional pre-display setup, create/destroy). Until spec 055 MAJOR stays 2 and 2.x is unstable: a breaking change steps MINOR and says so here. |
