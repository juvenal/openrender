# Contract: AOV/channel registry

**Owner**: Graphics State (`src/ri/state/channelRegistry.{h,cpp}`; `CDisplayChannel` stays in
`displayChannel.h`).
**Requirements**: FR-016 to FR-018, User Story 5, SC-008. Clarification Q1: a thin `CRenderer`
forwarder, `texture3d.cpp` untouched, DB-021 stays Open.

## State side

```cpp
class CChannelRegistry {
public:
    CChannelRegistry();                               // registers rgb(3,0) rgba(4,0) a(1,3) z(1,4) rgbaz(5,0)
    ~CChannelRegistry();                              // deletes each channel once, frees both containers
    CDisplayChannel *declare(const char *declaration);// from a declaration string
    CDisplayChannel *declare(CVariable *variable);    // from an already-declared variable
    CDisplayChannel *retrieve(const char *name) const;// nullptr if not found
    void resetUsage();                                // sampleStart = -1 for every variable-backed channel
    template <class F> void forEach(F &&) const;      // declaration order
};

void beginChannelRegistry();          // creates the session instance (initDeclarations)
void endChannelRegistry();            // destroys it (shutdownDeclarations)
CChannelRegistry *channelRegistry();  // nullptr outside a session
```

### `declare(const char *)` is today's `CRenderer::declareDisplayChannel(const char *)` (`rendererDeclarations.cpp:534-570`), unchanged in behaviour

1. `parseVariable(&tmp, NULL, declaration)` (infrastructure). On failure, return `nullptr` with no
   message.
2. If the name exists:
   - return it when `numSamples` matches and either the declaration is not global storage or
     `outType` matches;
   - otherwise call `error(CODE_SYSTEM, "Channel \"%s\" was previously defined differently\n",
     name)` and return `nullptr`.
3. Otherwise declare the variable with `declareVariableThroughHook(NULL, declaration)`.
   - On `nullptr`, call `error(CODE_SYSTEM, "Channel definition \"%s\" is ill formed\n",
     declaration)`. This also happens when no hook is installed.
   - Otherwise create `CDisplayChannel(var->name, var, var->numFloats, -1, global ? var->entry : -1)`,
     register it in both containers, and return it.

### `declare(CVariable *)` is today's overload (`:579-600`)

- If the name exists: return it when its variable is the same one; otherwise return `nullptr`
  with no message.
- Otherwise create and register the channel.

## Declared-variable hook (extends `variableLookup.h`)

```cpp
using TVariableDeclaration = CVariable *(*)(const char *name, const char *declaration);
void installVariableDeclaration(TVariableDeclaration);           // nullptr uninstalls
CVariable *declareVariableThroughHook(const char *name, const char *declaration); // nullptr if none
```

It is installed in `initDeclarations()` as a captureless static adapter, `declareVariableAdapter(name, declaration)` calling `CRenderer::declareVariable(name, declaration, 0)` (the core function has a third, defaulted `int um` parameter, so its address does not convert to `TVariableDeclaration`), and uninstalled at the
top of `shutdownDeclarations()`, beside `installVariableLookup`. The declared-variable table stays
in the renderer core (spec assumption).

## Renderer core

| Before | After |
|---|---|
| `static CTrie<CDisplayChannel*> *declaredChannels`, `static CArray<CDisplayChannel*> *displayChannels` | deleted |
| `declareDisplayChannel(const char*)`, `declareDisplayChannel(CVariable*)`, `resetDisplayChannelUsage()` | deleted; callers (`rendererContext.cpp:1483`, `rendererDisplay.cpp:563,618,671`) use `channelRegistry()->…` |
| `retrieveDisplayChannel(const char*)` | **kept**, declared in `renderer.h`, **defined out of line** in `rendererDeclarations.cpp` as `{ return channelRegistry() ? channelRegistry()->retrieve(name) : nullptr; }` |
| `initDeclarations()`: the two `new`s and five built-in registrations | `beginChannelRegistry()` |
| `shutdownDeclarations()`: `destroy()` + `delete` | `endChannelRegistry()`, at the same position (before the variables) |

The forwarder must not be made inline. If it were, `texture3d.o` would reference State symbols,
which would make DB-010's link line STALE and add a Texturing → State VIOLATION.

## Texturing

`CTexture3d::defineChannels` (`texture3d.cpp:119`) keeps calling
`CRenderer::retrieveDisplayChannel`, and the file has no diff. These allowlist lines are kept:
- DB-010: `link texture3d.cpp -> CRenderer::retrieveDisplayChannel`;
- DB-021: `include texture3d.cpp -> displayChannel.h`.

## Tests (`State_ChannelRegistry`)

- Fresh registry: the five built-ins have their samples and offsets, and an unknown name gives
  `nullptr` (US5.1).
- Declaring the same string twice gives the same pointer. A different shape gives `nullptr` and
  exactly one "previously defined differently" diagnostic, captured with an installed
  `CDiagnosticSink` (US5.2).
- The declare-from-string path runs through a test-double declaration hook. With no hook, it gives
  the "ill formed" diagnostic and `nullptr`.
- Out-of-order declarations across two "frames", then `resetUsage()`: every variable-backed
  channel's `sampleStart == -1`, and the `forEach` order is unchanged (US5.3).
- `begin`/`end`/`begin` gives a fresh instance with only the five built-ins (session reuse).
- US5.4 and US5.5 are checked by grep and diff in quickstart §3.
