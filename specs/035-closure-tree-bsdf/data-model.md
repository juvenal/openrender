# Data Model: Closure Tree and BSDF Interface (spec 035)

Every entity below belongs to one domain, and the domain is named. Exact signatures are in `contracts/`. Research entries R1 to R23 give the reasons.

## Infra (`src/common/`)

### Spectrum (`spectrum.h`, header-only)

| Field / member | Type | Notes |
|---|---|---|
| `kSpectrumChannels` | `constexpr int` = 3 | the compile-time channel count (D18) |
| `c` | `float[kSpectrumChannels]` | storage. No `.r/.g/.b` names except in `fromRGB()`/`toRGB()` |
| arithmetic | `+ - *` (per channel), `* float`, `/ float`, `mean()`, `maxChannel()`, `isZero()`, `isFinite()`, `isNonNegative()` | constexpr where possible |
| C boundary | `const float *values` + `int numChannels` | a C table never takes a `Spectrum` by value (FR-005) |

- Node weights are **not** `Spectrum`. They stay `float[3]` in the C structs (D17, D18).
- The C++ view converts them with `Spectrum::fromArray3()`.

## Graphics State (`src/ri/state/`)

### EClosureMode (`stateVocabulary.h`)

`enum class EClosureMode : uint8_t { Unset = 0, Color = 1, Both = 2, Closures = 3 };`

- Shading mirrors it value for value as `openrender::shading::ClosureMode`.
- `State_ClosureModeVocabularyMirror` checks the mirror.

### CAttributes (existing, extended)

| Field | Type | Default | Copy / restore |
|---|---|---|---|
| `closureMode` | `EClosureMode` | `Unset` | copy constructor; `restore()` in the shading group, beside `shaderFormat` |
| `bxdfSlot` (existing) | `ShaderGroupRef` | empty | unchanged. `RiBxdfV` puts only a `CBxdfInstance` into it; anything else (a test double) is ignored by Shading |

- `find("closuremode", "shade")` answers the string, with `"color"` when unset.

### COptions (existing, extended)

| Field | Type | Default |
|---|---|---|
| `closureMode` | `EClosureMode` | `Unset` |

- It is copied by the copy constructor, and answered by the option query.

### CDeclarations

- One declaration: `closuremode` as `string`.

## Shading (`src/libshader/closures/`, compiled into `libRSLRuntime`)

### Closure nodes (`closureAbi.h`, C17, `OR_CLOSURE_ABI_VERSION` 1.0.0)

| Struct | Fields | Invariants |
|---|---|---|
| `OrClosureNode` | `int32_t id` | `id >= 0` component, `OR_CLOSURE_MUL` (-1), `OR_CLOSURE_ADD` (-2) |
| `OrClosureComponent` | `id`, `float w[3]`; then the parameter block | aligned 16; block at `(char *)(c + 1)`; `id` registered |
| `OrClosureMul` | `id = -1`, `float weight[3]`, `const OrClosureNode *closure` | `closure != NULL` |
| `OrClosureAdd` | `id = -2`, `const OrClosureNode *closureA`, `*closureB` | both `!= NULL` |

- **Build-time validation**: a weight with a negative or non-finite channel refuses the node. The node is dropped, and one diagnostic per frame is given (edge case).
- An empty tree is `NULL`, and it is valid.

### Parameter blocks (one per built-in closure)

| Closure (id) | Block | Size, alignment |
|---|---|---|
| `diffuse` (0) | `float N[3]` | 12, 4 |
| `oren_nayar` (1) | `float N[3]; float sigma` | 16, 4 |
| `microfacet` (2) | `const char *distribution; float N[3]; float alpha; float eta; int32_t refract` | 32, 8 (LP64) |
| `reflection` (3) | `float N[3]; float eta` | 16, 4 |
| `refraction` (4) | `float N[3]; float eta` | 16, 4 |
| `transparent` (5) | none | 0 |
| `holdout` (6) | none | 0 |
| `uniform_edf` (7) | `float emittance[3]` | 12, 4 |

- `distribution` points at a registry-interned string, `"ggx"` or `"beckmann"`.
- A component's total size is `16 + block size`, rounded up to 16.
- The blocks are the structs of `closureBuiltins.h` (`ClosureDiffuseBlock`, `ClosureOrenNayarBlock`, `ClosureMicrofacetBlock`, `ClosureReflectionBlock`, `ClosureRefractionBlock`, `ClosureUniformEdfBlock`): the one definition the registry, the BSDF components and the tests share.

### Closure registry entry (`closureRegistry.h`)

| Field | Type | Notes |
|---|---|---|
| `id` | `int32_t` | the built-ins are fixed at 0 to 7; later registrations take the next id |
| `name` | `const char *` | unique; an alias maps to an id (`emission` → 7, deprecated, reported once per process) |
| `category` | `OrClosureCategory` | `BSDF`, `EDF`, `VDF` or `SIGNAL` |
| `params` | `const OrClosureParam *`, `numParams` | `{name, type (OR_CPARAM_FLOAT/INT/COLOR/NORMAL/VECTOR/STRING), offset}` |
| `blockSize`, `blockAlign` | `uint32_t` | checked when a node is built |
| `ops` | `const OrBsdfComponentOps *` | BSDF only: `evaluate`, `sample`, `pdf`, `albedo`; `NULL` for the other categories |

- **State**: the registry is process-wide. Registration is under a mutex; lookup is lock-free once the table is published. It fills itself with the built-ins on first use.

### Closure tree view and builder (`closureTree.h`)

| Entity | Fields | Notes |
|---|---|---|
| `ClosureTreeBuilder` | the arena (`CMemPage *&`), the batch's byte budget, the bytes used, an overflow flag, the roots table, a pointer to the frame's `IClosureDiagnostics` (null allowed) | `component(id, weight, block, blockSize)`, `mul(weight, child)`, `add(a, b)`, `setRoot(point, node)` (after an overflow the next one stores an empty root), `root(point)`, `bytesUsed()`, `budget()` |
| `ClosureDiagKind`, `IClosureDiagnostics`, `ClosureFrameDiagnostics` | the kinds `BadWeight`, `BudgetExceeded`, `UnknownClosure`, `BadBlockSize`, `BadRefractionIndex`, `PointOutOfRange`; a per-kind atomic count | one report per kind per frame; `CODE_LIMIT` for the budget, `CODE_RANGE` for a bad weight |
| accumulated weight | a `Spectrum` | root 1; child = parent's accumulated weight times the parent's own weight, one multiplication per node; lobe weight = that times the component's `w` (closure-abi.md) |
| `ClosureLobe` | `int32_t id; Spectrum weight; const void *params` | the flattened form (FR-003) |
| `ClosureLobeList` | `ClosureLobe *lobes; int count; bool hasDelta` | in the arena |
| accessors | `closureTreePresence(tree) → float`, `closureTreeEmission(tree) → Spectrum`, `closureTreeHoldout(tree) → float` | presence is the summed `transparent` weight as a scalar (its channel mean) (D16) |

- **Lifetime**: valid until the batch's `memEnd(threadMemory)`. `closureTreeCopy(tree, arena)` keeps one longer (FR-002).

### BSDF service (`closureAbi.h`'s `OrBsdfTable`, C++ wrapper `bsdf.hpp`)

| Entity | Fields |
|---|---|
| `OrBsdfQuery` | `float wo[3], wi[3]`; `OrTransportMode mode` (`RADIANCE`, `IMPORTANCE`); `uint32_t requested` (lobe-kind mask); `float Ng[3]` (geometric normal, reported side only) |
| `OrBsdfEval` | `float f[OR_SPECTRUM_MAX]`, `int numChannels`; `float pdfForward, pdfReverse`; `uint32_t flags` |
| `OrBsdfSample` | the `OrBsdfEval` fields plus `float wi[3]`, `float eta` |
| `OrBsdf` | opaque: the flattened lobes, the selection probabilities, the shading frame, in the caller's arena |
| flags | `OR_BSDF_REFLECTION`, `_TRANSMISSION`, `_DIFFUSE`, `_GLOSSY`, `_SPECULAR`, `_DELTA`, `_HAS_DELTA`, `_NOT_SCATTERER`, `_FAILED` |

- **State**: none. Every call is a pure function of its arguments (FR-008).
- **Delta lobes**: with `OR_BSDF_DELTA` set, `pdfForward` and `pdfReverse` hold the discrete lobe-choice probabilities, not densities.

### Bxdf table and registry (`bxdfAbi.h` C17, `OR_BXDF_ABI_VERSION` 1.0.0; `bxdfRegistry.h`)

| Entity | Fields |
|---|---|
| `OrBxdfParam` | `name`, `type`, default value, `min`, `max`, `onOutOfRange` (`CLAMP` or `REFUSE`), `doc` |
| `OrBxdfTable` | `OrTableHeader header`; `name`; `params`, `numParams`; `create(const void *const *values, const OrBxdfDiagnostics *diag) → OrBxdfData *` (the values are already typed, defaulted and range-handled by the host; `NULL` refuses); `destroy(data)`; `build(const OrBxdfData *, const OrBxdfPoint *, const OrClosureAllocator *) → const OrClosureNode *` |
| `OrBxdfPoint` | `P, N, Ng, dPdu, dPdv, I` (`float[3]`), `u, v` (camera space; `N` faceforwarded); `Cs, Os` (`float[3]`, the point's colour and opacity: the primitive's, else the attributes'; RISpec §4.2.1, Q-R3) |
| registry entry | the table, or a refusal reason (a MAJOR the host does not support), the name |

- `"sl:"` and `"osl:"` are refused before the lookup (A11).
- **Built-in parameters** (Q-R3): `diffuse` (matte) `Kd`; `plastic` `Kd`, `Ks`, `roughness`, `specularcolor`, `ior`; `conductor` (metal) `Ks`, `roughness`; `dielectric` `ior`, `roughness`, `reflectColor`, `transmitColor`. The defaults are in contracts/bxdf.md. The base colour is `Cs`, `Oi = Os`, and there is no `Ka`, for the three RISpec-modelled Bxdfs. `alpha = roughness` for all four. `plastic`'s `ior` defaults to 0, meaning no Fresnel term.
- **Colour parameters** are converted from `ColorSamples` with `COptions::convertColor` (RISpec §4.1.3).
- **Validity**: `RiBxdf` is refused inside an `ObjectBegin` block (`VALID_BXDF_BLOCKS`; RISpec §5.10, Q-R4).
- **State**: process-wide. The four built-ins register through the call graph on first use.

### CBxdfInstance (`bxdfInstance.h`)

| Field | Type | Notes |
|---|---|---|
| (base) | `CShaderGroupRefTarget` | reference counted; held by `CAttributes::bxdfSlot` |
| `table` | `const OrBxdfTable *` | |
| `data` | `OrBxdfData *` | owned; `destroy` runs in the destructor |
| `handle` | `std::string` | as the statement gave it (for spec 034) |
| `substituteSurface` | `ShaderGroupRef` | the FR-026 substitute, A′ (Q-R1, RISpec §4.2.4): the `defaultsurface` handle `RiWorldBegin` bound through `setDefaultSurface()`, or empty for the no-shader path. Never a user-bound `Surface`. A Bxdf bound before `WorldBegin` receives the handle lazily from `RiWorldBegin` (analyze B2).|

- Accessor: `bxdfInstanceOf(const ShaderGroupRef &)`, a checked `dynamic_cast` (`CRefCounter` is polymorphic). It returns `nullptr` for a foreign object, which the callers treat as "no Bxdf instance" (R15).

### Closure consumer (`closureConsumer.h`)

| Entity | Fields |
|---|---|
| `ClosureBatch` | `numPoints` (real points), `const OrClosureNode *const *trees`; strided `P, N, Ng, dPdu, dPdv, I` (`const float *`, stride 3), `u, v`; `ObjectHandle object`; `const SampleKey *keys`; `RayType rayType`; `int pathDepth`; `BatchShape shape`; `ClosureMode mode`; `bool fromBxdf` |
| `IClosureConsumer` | `virtual void consume(CShadingContext &, const ClosureBatch &) = 0` |

- **Installed** on a context (`setClosureConsumer`), with a process default for tests (`setDefaultClosureConsumer`). The context's own consumer wins.
- **Lifetime**: the batch and its trees are valid only during `consume` (FR-021).

### ClosureFrame (`closureFrame.h`)

| Field | Type | Notes |
|---|---|---|
| `sceneMode` | `ClosureMode` | from `COptions::closureMode` |
| `once` | mutex + `std::set<std::string>` | "once per frame" keys, shared by every thread's context |

- **Lifecycle**: created by the core in `beginFrame()` and set on each context next to `setSamplingConfig`. Deleted in `endFrame()` after the threads are joined. With none set: scene mode `Unset`, dedup once per process.

### Surface step plan (`closureStep.h`)

| Field | Type | Notes |
|---|---|---|
| `governing` | `Surface` or `Bxdf` | from `CAttributes::governingMaterial()` and the slot |
| `mode` | `ClosureMode` | attribute, else the frame's scene mode, else `Color` (FR-017) |
| `capable` | bool | the instance's backend `emitsClosures()` |
| `consumer` | `IClosureConsumer *` | the context's, else the process default, else `nullptr` |
| `rule` | `ClosureInterimRule` | `None`, `IR017`, `IR018` or `IR026` (R17) |
| `buildTrees` | bool | true only with a consumer and (a capable Surface under `both`/`closures`, or a Bxdf) |
| `suppressLighting` | bool | true only for a capable Surface under `closures` |
| `surface` | `CShaderInstance *` | the instance to run (`nullptr` = no-shader path); under IR-026 the substitute |

**State transitions of a batch** (per object, once per `shade()` call with `displaceOnly == FALSE`):

```text
governing == Bxdf ─┬─ consumer ─────────► build Bxdf trees, Ci=0, Oi=Os, deliver
                   └─ no consumer ──────► IR-026: run substitute surface (A′: defaultsurface or no-shader path), diag per Bxdf name per frame
governing == Surface ─ mode == color ───► today's path, byte for byte
                     ─ mode both/closures, !capable ─► IR-017: color path, warn per (shader, mode) per frame
                     ─ both, capable ─┬─ consumer ──► executeClosures (lighting on), deliver trees, Ci/Oi as color
                                      └─ none ──────► color path (trees dropped; Q5b: silent)
                     ─ closures, capable ─┬─ consumer ► executeClosures (lighting gated), Ci=0, Oi=shader's own, deliver
                                          └─ none ───► IR-018: executeClosures gated, Ci=0, Oi=shader's own, warn once per frame
no surface instance and no Bxdf ─────────► today's no-shader path; no diagnostic
```

**Opacity:**
- A capable Surface under `closures` keeps the `Oi` its shader computed (RISpec §12.1, Q-R5).
- `Os` is written only where no shader runs: a Bxdf with a consumer.

**Instancing:** an instance takes the mode and the Bxdf binding current at `ObjectInstance` (RISpec §5.10, Q-R2).

### CShadingState (existing, extended)

| Field | Type | Notes |
|---|---|---|
| `lightingSuppressed` | `int` | set by `runSurfaceStep` around a gated execution only; read by the four funnels (R9; the ambient funnel is three sites calling one shared inline check) |

## Scene Interface

### RiBxdf / RiBxdfV

- `RiBxdfV(RtToken name, RtToken handle, RtInt n, RtToken tokens[], RtPointer params[])`, with `handle` possibly `NULL`.
- It is valid in attribute blocks. `CRiInterface`'s default body ignores it (the preview, other back-ends).
- `ribOut` writes it at its position.

## Validation rules (from the requirements)

| Rule | Where | FR |
|---|---|---|
| A `closuremode` value outside `color`/`both`/`closures` gives `CODE_RANGE`, naming the value and the three spellings, and keeps the old value | `RiAttributeV`, `RiOptionV` | FR-020 |
| An unknown token under `Option "shade"`/`Attribute "shade"` gives `CODE_BADTOKEN` | same | FR-020 |
| A Bxdf name with `sl:`/`osl:`, an unknown name or a refused MAJOR gives a diagnostic with the reason; the binding is unchanged; no key is pending | `RiBxdfV` | FR-014 |
| A Bxdf parameter out of range is clamped or refused per its table, with a diagnostic naming the parameter | `create` | FR-015 |
| A negative or non-finite node weight drops the node, with one diagnostic per frame | builder | edge case |
| A tree over the batch budget gives an empty tree, with one `CODE_LIMIT` per frame | builder | edge case |
| An unregistered closure name gives a diagnostic naming it | registry lookup | FR-004 |
