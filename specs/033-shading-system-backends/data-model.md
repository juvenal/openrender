# Data Model: Shading System Backends (spec 033)

All types are internal C++ in the Shading domain (`src/libshader/`) unless a line says otherwise. None is installed,
none is in `OPENRENDER_PUBLIC_HEADERS`, none carries a version of its own (FR-023). Decisions referenced as R*n* are in
[research.md](research.md); interfaces in [contracts/](contracts/).

## 1. Entities

### ShadingSystem (`shadingSystem.{h,cpp}`)

| Field / member | Meaning |
|---|---|
| `backends` | the entries of the backend registry at construction (key, extension, priority, backend or "unavailable" + its warning) |
| `cache` | `CTrie<CFileResource *>` of loaded descriptions, keyed by `desc->name` |
| `cacheMutex` | held across find, load and insert only (R10) |
| `bind(request, attributes, xform, host)` | find or load, type check, key, instantiate, light registration, initialise (R9) |
| `find(request)` | find or load a description; no instance (used by the `CRenderer::getShader` forwarder) |
| `resolvePreference(attr, option)` | `attr ? attr : option ? option : OPENRENDER_DEFAULT_FORMAT` (FR-006) |

Lifetime: created by the core in `CRenderer::initFiles()` (RiBegin), destroyed in `shutdownFiles()` (RiEnd); the cache
and every description die with it, as `globalFiles` did. One per renderer session; a test may build its own.

Validation: a name is never inserted twice (the cache is consulted first); a null description is never inserted.

### IShaderBackend (`shaderBackend.h`)

A stateless object per shader format, one singleton per implementation.

| Member | Contract |
|---|---|
| `key()` | the format's name in a `shaderformat` preference: `"rslo"`, `"slo"`, a test's `"tst"` |
| `extension()` | the extension passed to the resolver (the same strings) |
| `load(name, path)` | a description with this backend and its payload, or `nullptr` with the format's own diagnostics |
| `instantiate(desc, attributes, xform)` | a new instance with one reference (`CShaderInstance` ctor's `attach()`) |
| `initialise(instance)` | the bind-time init (R6); runs inside `arenaBegin/End(globalArena)`, set up by the facade |
| `execute(ctx, instance, locals)` | runs the instance for the context's current batch |

Registry: `registerShaderBackend(const IShaderBackend *, int priority)`, `registerUnavailableShaderFormat(key,
warningText)`, `unregisterShaderBackend(key)` (tests). Built-ins: rslo priority 0, slo 10 (or "unavailable" in a
JIT-less build).

### Shader description: `CShader` (`shader.h`, kept name)

| Field | Kept from today | Note |
|---|---|---|
| `name` | yes (`CFileResource`) | the cache key |
| `type` | yes | `SL_SURFACE` ... `SL_IMAGER` |
| `parameters` | yes | `CVariable` list with stored types and defaults (string defaults point into the payload's `strings`) |
| `usedParameters` | yes | |
| `flags`, `data` | yes | light flags and `CLightShaderData`, set by `analyse()` at load |
| `backend` | new | the backend that loaded it; set by the loader functions themselves (R3) |
| `payload` | new | owned; deleted by `~CShader` |

Removed from `CShader` (moved to the payload): `memory`, `codeArea`, `constantEntries`, `varyingSizes`, `strings`,
`numGlobals`, `numStrings`, `numVariables`, `codeEntryPoint`, `initEntryPoint`, `jitEntry`, `jitInitEntry`, and the
`friend parseShader` (the loader writes the payload through its own type).

### Shader payload (`shaderPayload.h`, backend-private)

```text
CShaderPayload            virtual ~CShaderPayload()
└── CRslPayload           numVariables, varyingSizes, numGlobals, strings, numStrings, memory   (RSL frame layout, owned storage)
    ├── CRsloPayload      codeArea, constantEntries, codeEntryPoint, initEntryPoint            (interpreter)
    └── CSloPayload       jitEntry, jitInitEntry   [only under OPENRENDER_HAVE_LLVM]            (JIT)
```

`~CRslPayload` frees `strings[i]` and `memory` exactly as `~CShader` does today. Accessors `rsloPayloadOf(const CShader &)`
and `sloPayloadOf(...)` are `static_cast`s used only inside the owning backend and by `CProgrammableShaderInstance::
prepare()` (`rslPayloadOf`).

### Shader instance (`shader.h`)

| Type | Change |
|---|---|
| `CShaderInstance` (backend-neutral base, the slot target) | gains `virtual const CShader *shaderDescription() const = 0` (the dispatcher's way to the backend); otherwise unchanged; still derives from `CShaderGroupRefTarget` |
| `CProgrammableShaderInstance` (RSL, both RSL backends) | loses `jitEntry`/`jitInitEntry`; ctor `(CShader *, CAttributes *, CXform *)` kept; `prepare()` reads `rslPayloadOf(*parent)` |

Lifetime: unchanged. Reference counted; created with one reference by the backend; the core adopts it into a slot
(`ShaderGroupRef::adopt`) or keeps it in `CRenderer::allLights`; freed by the last `detach()`.

### Shader group (one layer)

A group of one layer is its layer's `CShaderInstance` (R11). `ShaderGroupBuilder` holds the builder state: the
system, the first layer, and a `refused` flag. States:

```text
begun --shader() ok--> one-layer --end()--> bound (CShaderInstance *, one reference)
begun --shader() fails--> empty --end()--> nullptr
one-layer --shader() / connect()--> one-layer (refused, CODE_INCAPABLE "... before spec 034")
```

### ShaderRequest (`shaderBackend.h`)

| Field | Meaning |
|---|---|
| `name` | as the RIB statement gave it (may contain an extension or a path) |
| `type` | the expected `SL_*`, or `kAnyShaderType` for the `CRenderer::getShader` forwarder (no type check) |
| `attributeFormat` | `CAttributes::shaderFormat` (may be null) |
| `optionFormat` | `COptions::defaultShaderFormat` (may be null) |
| `resolver` | the core's `CShaderSearchPathResolver` over the current shader path |

### IShaderFileResolver, IShaderBindHost (`shaderBackend.h`, implemented by the core)

`locate(result, name, extension)` -> `CRenderer::locateFileEx` (R8). `takeInstanceKey()` -> `CRendererContext::
takeInstanceKey()`; `registerLight(instance)` -> `CRenderer::allLights->push(instance)` (R9).

### Renderer-services additions (2.4.0, approved P-Q1; spec 057 takes 2.5.0)

| Type | Addition |
|---|---|
| `MatrixRequest` | `EMatrixPhase phase = EMatrixPhase::Shade` |
| `MessageRequest` | `size_t destBytes = 0` |
| new `ShaderNameRequest` | `MessageSource source; ObjectHandle object; ShaderHandle light;` |
| new `ShaderNameAnswer` | `EQueryStatus status = Unavailable; const char *name = nullptr;` |
| `CRendererServices` | `virtual ShaderNameAnswer getShaderName(const ShaderNameRequest &) const { return {}; }` |

### Shader metadata (`libshader/include/openrender/ShaderMetadata.h`, library `libshader_metadata`)

| Field | `.rslo` source | `.slo` source |
|---|---|---|
| `status` | Found / NotFound ("Failed to find shader") | Found / NoMetadata / Unsupported (JIT-less) |
| `format` | `rslo` | `slo` |
| `typeName`, `name` | `TRSLObjectShader::type`, `name` | `SLOShaderInfo::typeName`, `name` |
| `formatVersion`, `languageVersion` | from the file's header when present, else empty | from the module metadata when present, else empty |
| `usedParameters` | not recorded by the inspection parser (empty) | `SLOShaderInfo::usedParameters` |
| `hasJitEntry` | n/a | `SLOShaderInfo::hasJitEntry` |
| `params[]` | name, type, container, `writable`, `numItems`, `space`, typed defaults | name, type name, storage text, `writable`, `arraySize`, default text |
| `locals[]` | not printed by `sloinfo` today; the API returns none for both formats in this spec | |

No field names an engine (no bytecode index, no LLVM type); spec 043 publishes from the same structure (FR-018).

## 2. Relationships

```text
CRenderer (core) ──owns──> ShadingSystem ──owns──> cache ──> CShader (description) ──owns──> CShaderPayload
                                │                              │ backend ─────────────────> IShaderBackend (singleton)
                                │ bind(request, host) ─────────┴──> CShaderInstance ──is──> CShaderGroupRefTarget
CRendererContext ──implements──> IShaderBindHost, IShaderFileResolver (per request)
CAttributes / COptions slots ──ShaderGroupRef──> CShaderInstance (one-layer group)
CProgrammableShaderInstance ──parent──> CShader ; execute() -> CShadingContext::execute() -> backend->execute()
(libshader_shading is renamed libRSLRuntime in Phase 8; libshader_runtime, the inspection reader, keeps its name)
sloinfo ──> describeShaderFile() ──> IShaderMetadataReader {rslo: libshader_runtime, slo: libshader_jitmeta}
```

## 3. Validation rules (from the requirements)

- The description and instance interfaces name no engine type outside the owning backend (FR-007, FR-003 scan).
- The format order is today's for every preference value (FR-006, R2); the build default is `OPENRENDER_DEFAULT_FORMAT`.
- A key is taken only after a successful load and type check (FR-008, FR-029).
- A group holds exactly one layer (FR-011).
- Every message builtin of the JIT asks the services; no JIT entry point reads a record, a bound shader or a light
  instance (FR-013, SC-005 scan).
- The metadata API loads and runs nothing and needs no renderer symbol (FR-016, FR-017).
