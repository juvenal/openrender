# Contract: integrator interface and capability vocabulary (version 2.0.0)

**Domain**: Transport (`src/ri/transport/`, the tenth domain, D30). **Consumers**: the renderer core;
Hiders (the capability vocabulary only); tests; specs 042 and 050 (extend it); from spec 051 dlopened
integrator modules. **Research**: R3, R7, R9, R10, R11.

## Files

| File | Language | Content |
|---|---|---|
| `src/ri/transport/capability.h` | C17 + C++20 | `OrCapability`, the capability name constants, the reserved names. Includes only `orplugin.h`. |
| `src/ri/transport/capabilityCheck.{h,cpp}` | C++20 | `transportCheckPair()` (pure function), `SPairVerdict`. |
| `src/ri/transport/integratorAbi.h` | C17 + C++20 | `OR_INTEGRATOR_ABI_VERSION`, the table, settings, host and frame structs, the version-history table (which also versions the capability vocabulary). |
| `src/ri/transport/integratorAbi.hpp` | C++20 | `OR_REGISTER_INTEGRATOR(getter)`, which calls `orIntegratorAddPending(getter)`, defined once out of line in `integratorRegistry.cpp` (never a header-reached static, see contracts/hider-registration.md). Internal. |
| `src/ri/transport/integratorRegistry.{h,cpp}` | C++20 | `CIntegratorRegistry`, `integratorRegistry()`, `kIntegratorFamily`. |
| `src/ri/transport/builtinIntegrators.cpp` | C++20 | `registerBuiltinIntegrators()`: `classic`. |
| `src/ri/transport/classicIntegrator.{h,cpp}` | C++20 | `orIntegratorTableClassic()`, `transportClassicLifecycleCounts()` (tests). |

Transport includes Infra (`common/`, `include/`) only in this spec: no Graphics State, Geometry or Shading
header is needed yet (they are allowed by D30 for 042). It never includes a Hiders, Scene Interface or Core
header (the `transport-cycle` fixture proves a Transport → Hiders include fails).

## Registry API (internal C++; `integratorRegistry.h`)

`CIntegratorRegistry` **mirrors `CHiderRegistry`** (contracts/hider-registration.md "Registry API") with the integrator
table type; its API is pinned here so the tests can be written against it (maintainer approval of the Phase 2 test
drafts, 2026-11-09):

```cpp
class CIntegratorRegistry {
public:
    // Same rules as CHiderRegistry::add: validate, version check, store; false and one diagnostic when refused, the
    // first registration stays; a refusal is of the WHOLE table (a name or alias collision, an unnamed table, a version
    // or size refusal): nothing of it is registered.
    bool add(const OrIntegratorTable *table, const char *origin);
    struct SLookup { const OrIntegratorTable *table; const OrIntegratorAlias *alias; };  // alias == nullptr for a name
    SLookup find(const char *nameOrAlias) const;   // exact, case-sensitive; names and aliases both resolve
    int size() const;
    const OrIntegratorTable *at(int i) const;      // registration order
};
CIntegratorRegistry &integratorRegistry();         // process instance: classic + static registrars, once (std::call_once)
```

In a program that links no integrator other than the built-in (`Registry_IntegratorLookup`,
`Link_StaticConsumerRegistries`), `integratorRegistry().size()` is exactly **1** (`classic`). A test file's
`OR_REGISTER_INTEGRATOR` entries follow it in append order. A local `CIntegratorRegistry` instance is what the unit
tests use for collisions and rejections.

## Capability vocabulary (`capability.h`)

```c
typedef struct OrCapability {
    const char *name;     /* a name below; exact, case-sensitive */
    uint32_t version;     /* OR_ABI_VERSION encoding; a requirement is met when provided >= required */
} OrCapability;

#define OR_CAP_RASTERIZATION       "rasterization"       /* the hider rasterizes submitted objects   */
#define OR_CAP_DISPLAY_OUTPUT      "display-output"      /* the hider produces display output         */
#define OR_CAP_DISPLAY_CHANNELS    "display-channels"    /* arbitrary display channels / AOVs         */
#define OR_CAP_DEEP_SHADOW_OUTPUT  "deep-shadow-output"  /* deep shadow (TSM) output                  */
#define OR_CAP_ILLUMINATION_HOOK   "illumination-hook"   /* lights driven through the illumination hook */
/* Reserved, added by the spec that needs one (a MINOR step): ray-intersection, ray-occlusion,
   light-sampling, light-emission-sampling, surface-closures, volume-closures, reverse-pdf, derivatives,
   path-connections, batched-execution, spectral-transport, deep-output. */
```

All five are at 1.0.0 in this spec. Absence means the hider is limited in that respect.

## Pair check

```cpp
struct SPairShortfall { const char *name; uint32_t required; uint32_t provided; bool missing; };
struct SPairVerdict  { bool accepted; std::vector<SPairShortfall> shortfalls; std::vector<const char *> absentOptional; };
SPairVerdict transportCheckPair(const OrCapability *provided, int numProvided,
                                const OrCapability *required, int numRequired,
                                const OrCapability *optional, int numOptional);
```

`accepted` is false when any required capability is missing or provided at a lower version; an absent or
older optional capability is listed in `absentOptional` and does not refuse the pair (US4 AS4).

## C table

```c
#define OR_INTEGRATOR_ABI_VERSION OR_ABI_VERSION(2, 0, 0)

OR_DECLARE_HANDLE(OrIntegratorInstance);
OR_DECLARE_HANDLE(OrTransportHost);   /* 2.0.0: structSize only; 042 appends the services facade (ref. §5.4, §5.7) */
OR_DECLARE_HANDLE(OrIntegratorFrame); /* 2.0.0: structSize only; 042 appends the frame view */

typedef struct OrIntegratorAlias { const char *name; const char *deprecationWarning; } OrIntegratorAlias;

typedef struct OrIntegratorParameter {
    const char *name;
    int type;            /* TYPE_FLOAT, TYPE_INTEGER, TYPE_STRING, TYPE_COLOR, TYPE_POINT, TYPE_VECTOR, TYPE_NORMAL, TYPE_MATRIX */
    int numItems;
    const void *data;    /* normalised at statement time: float[numItems] (TYPE_FLOAT), float[numItems*3] (color/point/vector/normal), float[numItems*16] (matrix), int[numItems] (TYPE_INTEGER), const char *[numItems] (TYPE_STRING); host memory, valid during configure */
} OrIntegratorParameter;

typedef struct OrIntegratorSettings {
    uint32_t structSize;
    const char *hiderName;               /* the resolved hider */
    const OrCapability *hiderProvides;   /* what that hider provides */
    int numHiderProvides;
    const OrIntegratorParameter *parameters;   /* the statement's list, without "handle" */
    int numParameters;
} OrIntegratorSettings;

typedef struct OrIntegratorTable {
    OrTableHeader header;
    const char *name;
    const OrIntegratorAlias *aliases;
    int numAliases;
    const OrCapability *required;   /* not "requires": a C++20 keyword, and this header is also compiled as C++20 */
    int numRequired;
    const OrCapability *optional;
    int numOptional;
    OrIntegratorInstance *(*create)(void);
    void (*destroy)(OrIntegratorInstance *);
    /* Lifecycle (ref. §5.1). Signatures pinned at 2.0.0; later versions add members to the structs they
       take and append slots to this table (structSize-checked), never change these. */
    OrStatus (*configure)(OrIntegratorInstance *, const OrIntegratorSettings *settings, int *acceptedParameters);
    OrStatus (*prepare)(OrIntegratorInstance *, const OrTransportHost *host, const OrIntegratorFrame *frame);
    void (*finish)(OrIntegratorInstance *, const OrTransportHost *host);
} OrIntegratorTable;
```

- `configure` sets `acceptedParameters[i]` to 1 for every parameter it defines and accepts, 0 otherwise;
  the host reports every 0 (research R9). Returning a status other than `OR_STATUS_OK` makes the host report
  `error(CODE_BADTOKEN, "Integrator \"%s\" could not be configured")` and **refuse** the frame through the
  same path as an unsupported pair: `configure` runs before `beginDisplays()`, so no display is started and
  no file is written. The instance `create` returned is **destroyed** (through the table's `destroy`, so
  `destroy` calls equal `create` calls); `prepare` and `finish` are never called. `classic` always returns OK.
- `prepare`: after the per-thread hiders exist and the displays have started, before any render thread. A
  non-OK status (`error(CODE_BADTOKEN, "Integrator \"%s\" could not prepare the frame")`) **aborts** the
  frame: the hiders are destroyed at once (`contexts` becomes `NULL`), `HIDER_BREAK` is set,
  `renderFrame()` spawns no thread, `finish` is not called, and the started displays are finished by `endDisplays()` as in today's "no
  active display" abort (a driver may write its file). This is not the refusal, whose "no output file"
  guarantee holds only for failures detected before the displays. Everything created for the frame is
  **destroyed** at once: the per-thread hiders built so far (each through its table's `destroy`) and the integrator
  instance (through the table's `destroy`, without `finish`). `classic` always returns OK, so no built-in reaches it.
- `finish`: at the start of `endFrame()`, before the hiders are destroyed, once per frame for which
  `prepare` returned OK.
- Threading: all three are called on the thread that runs `RiWorldBegin`/`RiWorldEnd`, never concurrently.

## Core call sequence (per frame)

```text
beginFrame():  resolve hider (fallback reyes) -> resolve integrator (fallback classic, error lists names)
               -> transportCheckPair  --refused--> no displays, no hiders, no integrator (frame-transport.md)
               -> create -> configure (report unaccepted parameters)
               -> ... displays, FrameParams, per-thread hiders ...
               -> prepare
renderFrame(): (integrator not called in 2.0.0)
endFrame():    finish -> destroy -> delete hiders -> ... -> endDisplays -> clear SFrameTransport
```

## `classic`

| Field | Value |
|---|---|
| name | `classic` |
| aliases | none |
| required / optional | none / none |
| create / destroy | a static instance / nothing |
| configure | accepts no parameter; returns OK |
| prepare / finish | return at once |

Each lifecycle slot increments one atomic counter readable through `transportClassicLifecycleCounts()`
(declared in `classicIntegrator.h`, used only by tests; it changes no output). It returns **by value** a struct of three
`uint64_t`, one per slot:

```cpp
struct SClassicLifecycleCounts { uint64_t configure; uint64_t prepare; uint64_t finish; };
SClassicLifecycleCounts transportClassicLifecycleCounts();   // a snapshot of the three counters
```

## Diagnostics

| When | Code | Text |
|---|---|---|
| unknown integrator at frame begin | `CODE_BADTOKEN` (error) | `Integrator "<name>" unavailable; registered: <n1>, <n2>. Using "classic"` |
| unaccepted parameter | `CODE_BADTOKEN` (error; maintainer answer Q4: `orender` exits -1, like an unknown `Option`) | `Integrator "<name>" does not define parameter "<p>"; ignored` |
| unsupported pair | `CODE_LIMIT` (error) | `Hider "<h>" cannot run integrator "<i>": <shortfall>[; <shortfall>]...; the frame is not rendered`, where a `<shortfall>` is `missing "<c>" <v>` (the capability is not provided; `<v>` is the version required) or `"<c>" <v1> provided, <v2> required` (provided at a lower version), versions written `MAJOR.MINOR.PATCH` (e.g. `1.0.0`), shortfalls in the order of the integrator's `required` array, joined by `; `; one error per refused frame |
| alias without a name | `CODE_BADTOKEN` (error) | `Integrator "<n>" has an alias without a name; the registration is ignored (<origin>)`; the whole table is refused (added at Phase 5, approved by the maintainer 2026-11-10) |
| registration refusals | as the hider registry (contracts/hider-registration.md), with "Integrator"; a MAJOR mismatch and a `structSize` smaller than `sizeof(OrIntegratorTable)` both report `CODE_VERSION` and register nothing | |

## Version history (to be written in `integratorAbi.h`)

| Version | Change |
|---|---|
| 1 | None: no integrator existed; the `Surface` shader integrated itself. |
| 2.0.0 | Spec 030 (starting version confirmed by the maintainer, 2026-11-07, Q2): table (name, aliases, required and optional capabilities, create/destroy, `configure`/`prepare`/`finish`), settings/host/frame structs, the capability vocabulary `rasterization`, `display-output`, `display-channels`, `deep-shadow-output`, `illumination-hook` at 1.0.0, the reserved names. Until spec 055 MAJOR stays 2 and 2.x is unstable: a breaking change steps MINOR and says so here. Spec 042 is expected to append `render` and the host services as a MINOR step. |
