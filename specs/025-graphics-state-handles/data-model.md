# Data Model: Graphics State Handles

**Plan**: [plan.md](plan.md) | **Research**: [research.md](research.md)

Every new State type lives in `src/ri/state/` and depends only on infrastructure (`src/common`) and
on State's own headers. The interfaces themselves are in [contracts/](contracts/).

## Entities

### `CShaderGroupRefTarget` (Graphics State)
- An empty base class deriving from `CRefCounter`, with a virtual destructor inherited.
- It is the base of everything a shader slot, the light list or the pattern table can refer to.
- Implemented by Shading: `CShaderInstance` changes its base to this class. State-only tests
  implement it with test doubles.
- Adds no data members, so `CShaderInstance`'s layout is unchanged.

### `CMapRefTarget` (Graphics State)
- An empty base class deriving from `CRefCounter`, the base of everything a map slot can refer to.
- Implemented by Hiders: in `CPhotonMap`'s base list, `public CRefCounter` becomes
  `public CMapRefTarget`.

### `TStateRef<T>`, `ShaderGroupRef`, `MapRef` (Graphics State, header-only)
- **Field**: `T *object`, which may be null. Empty means no object; it is distinguishable from a
  bound slot (FR-001).
- **Invariant**: a non-empty handle owns exactly one reference to `object`.
- **Operations**:
  - `adopt(T*)` takes over the caller's reference and adds no count;
  - `retain(T*)` adds one count;
  - copy adds one count;
  - move transfers the reference with no count change;
  - assignment retains the new object, then releases the old one;
  - `reset()` and the destructor release one count;
  - `get()`, `explicit operator bool`, and `==`/`!=` against another handle or a raw `T*`.
- `ShaderGroupRef = TStateRef<CShaderGroupRefTarget>` and `MapRef = TStateRef<CMapRefTarget>` are
  distinct types and do not convert to each other.

### `CActiveLight` (Graphics State, changed)
- `CShaderGroupRefTarget *light`: **non-owning**, as today. The renderer core's `allLights` owns
  the light.
- `CActiveLight *next`.
- Order rules (research R5, FR-025):
  - `addLight` appends and suppresses duplicates by pointer equality, as today;
  - `removeLight` unlinks the first match, as today;
  - the copy constructor and `restore(shading)` copy the list **in order** through
    `copyLightList()`. This is the FR-025 change, made in its own phase. Until that phase they keep
    today's prepend, which reverses the list.
- Invariant after FR-025: in every block, the list order equals the order in which the lights
  became active in the RIB stream.

### `EMaterialBinding` and the last-bound record (Graphics State)
- `enum class EMaterialBinding : unsigned char { None, Surface, Bxdf }`.
- `CAttributes::lastMaterialBinding` starts as `None`.
- **Transitions**:

| Event | Record afterwards |
|---|---|
| `bindSurface(h)`, whether `h` is empty or not | `Surface` |
| `bindBxdf(h)`, whether `h` is empty or not | `Bxdf` |
| `setDefaultSurface(h)` (`RiWorldBegin` only) | unchanged |
| the copy constructor (attribute push, copy-on-write clone) | copied from the source |
| `restore(other, shading = true)` | `other`'s value |
| `restore(…, shading = false)` | unchanged |

- **Query**: `governingMaterial()` returns the record. No consumer may infer the answer from which
  slot is empty (FR-011).

### `CPatternTable` (Graphics State)
- **Storage**: a reference-counted copy-on-write block holding a `std::vector` of entries.
  - Each entry is `{ std::string handle; ShaderGroupRef node; }`.
  - Entries are kept in first-definition order, with unique handles.
- **Operations**:
  - `define(handle, node)`: if the storage is shared, copy it first; then replace the entry with
    the same handle in place, or append a new one;
  - `lookup(handle) -> const ShaderGroupRef*`: `nullptr` when the handle is not found. State
    writes no diagnostic;
  - `forEach(fn)`: visits the entries in order;
  - `size()`.
- **Copying** the table (the attribute copy constructor, `restore(shading)`) shares the storage.
- **Scope** comes from value semantics: an inner block's definitions live in its own copy and
  disappear with it. A node bound into a slot stays alive through that slot's own reference.

### `CAttributes` (Graphics State, changed fields)

| Field | Before | After |
|---|---|---|
| `surface`, `displacement`, `atmosphere`, `interior`, `exterior` | public `CShaderInstance *` | private `ShaderGroupRef`, read through inline `const ShaderGroupRef &surface() const`, etc., written through `bind*()` |
| `bxdf` | — | private `ShaderGroupRef`; `bxdf()` and `bindBxdf()` |
| `lastMaterialBinding` | — | `EMaterialBinding` |
| `patterns` | — | `CPatternTable` |
| `globalMap`, `causticMap` | `CPhotonMap *` | public `MapRef` |
| `lightSources` | a list holding `CProgrammableShaderInstance *` | a list holding `CShaderGroupRefTarget *`, non-owning |
| `usedParameters` | `unsigned int`, set by `checkParameters()` | unchanged; `checkParameters()` now asks the installed hook (0 when none is installed) |
| `findParameter(name)` | walks the slots | forwards to the hook (`nullptr` when none is installed) |
| `selectVolumeShader()` | returns `CShaderInstance *` | returns `CShaderGroupRefTarget *` |

### `COptions` (Graphics State, changed fields)
- `imager`: from `CShaderInstance *` to `ShaderGroupRef`. Binding it adopts; a copy retains.
- `CDisplay::TDisplayParameter::type`: from `ParameterType` to `EDisplayParameterType`.
- `blobbyOpcodeOrder` stays `int`, with default `static_cast<int>(EBlobbyOrder::RiSpec)` (= 0).

### State vocabulary (`stateVocabulary.h`)

| State enumerator | Value | Must equal |
|---|---|---|
| `EDisplayParameterType::Float` | 0 | `FLOAT_PARAMETER` (`dsply.h`) |
| `EDisplayParameterType::Vector` | 1 | `VECTOR_PARAMETER` |
| `EDisplayParameterType::Matrix` | 2 | `MATRIX_PARAMETER` |
| `EDisplayParameterType::String` | 3 | `STRING_PARAMETER` |
| `EDisplayParameterType::Integer` | 4 | `INTEGER_PARAMETER` |
| `EBlobbyOrder::RiSpec` | 0 | `BLOBBY_ORDER_RISPEC` (`blobbyField.h`) |
| `EBlobbyOrder::AppNote` | 1 | `BLOBBY_ORDER_APPNOTE` |

### `CChannelRegistry` (Graphics State)
- **Fields**: `CTrie<CDisplayChannel*> byName` and `CArray<CDisplayChannel*> inOrder`. The
  registry owns each channel exactly once.
- **Construction** registers the built-ins in this order:

| Name | Samples | Offset |
|---|---|---|
| `rgb` | 3 | 0 |
| `rgba` | 4 | 0 |
| `a` | 1 | 3 |
| `z` | 1 | 4 |
| `rgbaz` | 5 | 0 |

- **Destruction** deletes each channel once, then frees both containers. This keeps today's
  `destroy()` plus `delete` ownership.
- **Lifetime**: the session instance is created in `initDeclarations()` and destroyed in
  `shutdownDeclarations()` before the declared variables. With no session instance,
  `channelRegistry()` returns `nullptr`.

### Hooks (Graphics State, installed by the renderer core)

| Hook | Signature | Installed in, removed in | Implemented by |
|---|---|---|---|
| variable lookup (exists) | `CVariable *(*)(const char *)` | `initDeclarations` / `shutdownDeclarations` | `CRenderer::retrieveVariable` |
| variable declaration (new) | `CVariable *(*)(const char *name, const char *decl)` | `initDeclarations` / `shutdownDeclarations` | `CRenderer::declareVariable` |
| shader parameters (new) | `SShaderParameterHooks { usedParameters, findParameter }` | `rendererShadingServicesInit` / `rendererShadingServicesShutdown` | `libshader::shaderParameterHooks()` (Shading) |

## Ownership mapping (today's calls next to their handle replacements)

| Site | Today | After |
|---|---|---|
| `Ri{Surface,Displacement,Atmosphere,Interior,Exterior}V` | `old->detach(); slot = cShader;` | `attributes->bindX(ShaderGroupRef::adopt(cShader))` |
| `RiWorldBegin` default surface | `attrs->surface = cShader;` (no attach) | `attrs->setDefaultSurface(ShaderGroupRef::adopt(cShader))` |
| `RiImagerV` | `old->detach(); options->imager = s;` | `options->imager = ShaderGroupRef::adopt(s)` |
| `CAttributes` copy constructor | `this[0] = a[0]` + 5 slot `attach()` + 2 map `attach()` | `this[0] = a[0]` (the handle copy-assignment retains); the 7 manual `attach()` lines are **deleted** |
| `~CAttributes` | 5 slot `detach()` + 2 map `detach()` | member destructors; the manual lines are deleted |
| `restore(shading)` | detach 5 + attach 5 | assignment of 5 slots + `bxdf`; the record and the pattern table are copied; maps are untouched, as today |
| `CRenderer::render()` lazy map | `map = getPhotonMap(n); map->attach();` | `cAttributes->globalMap = MapRef::retain(getPhotonMap(n))` |
| `COptions` copy constructor and destructor | `imager->attach()` / `imager->detach()` | handle copy / destructor; the manual lines are deleted |
| `addLight`, `removeLight`, light copies | no counts | no counts (non-owning, FR-001 as clarified 2026-10-26); copies keep order after FR-025 |

The State-only test asserts each of these against a counting test double (US1.3).
