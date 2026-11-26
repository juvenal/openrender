# Contract: the Bxdf table, the Bxdf registry and the `Bxdf` statement

- **Headers**: `src/libshader/closures/bxdfAbi.h` (strict C17 and C++20; it includes `orplugin.h` and `closureAbi.h`), `bxdfRegistry.h` and `bxdfInstance.h` (C++).
- **Domain**: Shading. The statement itself lives in the Scene Interface (`ri.h`, `ri.cpp`, `rib.l`, `rib.y`, `ribOut`), the renderer core (`RiBxdfV`) and the bindings.
- **Not installed**: internal until spec 051, like `hiderAbi.h`.
- **Requirements**: FR-013 to FR-016 and FR-026; research R15, R16, R20 and R21.

## Version

```c
#define OR_BXDF_ABI_VERSION OR_ABI_VERSION(1, 0, 0)
```

| Version | Change |
|---|---|
| 1.0.0 | Spec 035. The table (name, typed parameters with ranges, create/destroy, build), the point record and the allocator. Built-ins register through it (D8). New in the path-tracing milestone; it starts at 1.0.0 (maintainer, 2026-11-23). The MAJOR-2 rule of `hiderAbi.h` and `kRendererServicesVersion` covers interfaces that predate the milestone, not this one. 1.x is unstable until the milestone closes: until then a breaking change steps MINOR and says so here; after it, only a real breaking change steps MAJOR, to 2.0.0. Internal until spec 051. |

## Table

```c
OR_DECLARE_HANDLE(OrBxdfData);

typedef enum OrBxdfParamType { OR_BXDF_FLOAT, OR_BXDF_INT, OR_BXDF_COLOR, OR_BXDF_STRING } OrBxdfParamType;
typedef enum OrBxdfRange { OR_BXDF_CLAMP = 0, OR_BXDF_REFUSE = 1 } OrBxdfRange;

typedef struct OrBxdfParam {
    const char *name; OrBxdfParamType type;
    float defaultValue[3];  /* for FLOAT/INT/COLOR */
    float minValue, maxValue; /* per channel; maxValue < minValue: no upper bound */
    OrBxdfRange onOutOfRange;
    const char *doc;
} OrBxdfParam;

/* RGB-3 until the spectral spec: the fixed float[3] fields (Cs, Os, defaultValue) carry no channel count (FR-005 note, analyze E5). Point inputs. Cs and Os are the point's colour and opacity (the primitive's, else the attributes'): RISpec §4.2.1 */
typedef struct OrBxdfPoint { uint32_t structSize; float P[3], N[3], Ng[3], dPdu[3], dPdv[3], I[3]; float u, v; float Cs[3], Os[3]; } OrBxdfPoint;

typedef struct OrBxdfDiagnostics { void *context; void (*report)(void *context, int code, const char *message); } OrBxdfDiagnostics;

typedef struct OrBxdfTable {
    OrTableHeader header; /* structSize = sizeof(OrBxdfTable), abiVersion = OR_BXDF_ABI_VERSION */
    const char *name;
    const OrBxdfParam *params; int numParams;
    /* Values are already typed, defaulted and range-handled by the host; return NULL to refuse */
    OrBxdfData *(*create)(const void *const *values, const OrBxdfDiagnostics *diag);
    void (*destroy)(OrBxdfData *data);
    /* Build the point's tree in alloc; declarative: no ray, no light, no illuminance/gather/occlusion (FR-015, D15) */
    const OrClosureNode *(*build)(const OrBxdfData *data, const OrBxdfPoint *point, const OrClosureAllocator *alloc);
} OrBxdfTable;
```

- **Ownership** follows `orplugin.h`:
  - the table and its strings are static;
  - `create` allocates and `destroy` frees;
  - `build` allocates only through `alloc`, the batch arena, so the tree's lifetime is the batch's.
- `build` is a pure function of `data` and `point`. Trees from equal inputs are bit-identical (FR-028).
- The shade step fills `OrBxdfPoint` from `varying`. `Cs` and `Os` are the primitive's values when it gives them, else the attributes' `surfaceColor`/`surfaceOpacity`. A consumer installed under a Bxdf adds `PARAMETER_CS | PARAMETER_OS` to the batch's `usedParameters`.

## Registry (`openrender::shading`)

- `bool registerBxdf(const OrBxdfTable *table, const char *origin)`:
  - checks the version through `checkPluginVersion(family, table->header.abiVersion, origin)`, the in-process overload of `common/pluginAbi.h`, and `structSize` through the table-size rule;
  - a refused table is **recorded with its reason**, so a later statement names that reason;
  - a duplicate name is refused, and the whole table with it, as the kind registries do (gotcha 38).
- `SBxdfLookup findBxdf(const char *name)` returns `{table | nullptr, reason}`. The reasons:
  - `PrefixReserved`, for a name starting with `"sl:"` or `"osl:"` (A11, D9);
  - `Unknown`;
  - `VersionRefused`, which carries the declared MAJOR.
- **Built-ins**: the four built-ins register on the first use of the registry, through the call graph. There is no static registrar, so a static link needs no anchor (gotcha 38).

## Built-ins (A4; Q7 and Q-R3, approved 2026-11-23)

Where a Bxdf models a RISpec 3.2 standard shader (Table 4.8, Appendix A), it takes that shader's parameter names and defaults. The common rules:

- The base colour is the point's `Cs` and the opacity is `Oi = Os`, both point inputs (`OrBxdfPoint`; RISpec §4.2.1).
- **Tree weights are not premultiplied by `Os`** (analyze B1): RISpec Appendix A's `matte`, `metal` and `plastic` write `Ci = Os * (...)`, and `Oi = Os`. A Bxdf tree carries `Kd*Cs`, `Ks*specularcolor` and so on, with no `Os`; the **consumer** applies the opacity (`Oi = Os`, `Ci` premultiplied as Appendix A does). `OrBxdfPoint.Os` is there so the consumer's record and the substitute path agree, not so `build` folds it into the weights. `Bxdf_Tree_NoOsPremultiply` pins this.
- `Ka` is not a parameter: ambient is lighting work with no closure lobe, so `Ka` gets the unknown-parameter diagnostic.
- The GGX `alpha = roughness`, not squared, for all four built-ins, `dielectric` included (research R21).
- A `microfacet` lobe built with `eta = 0` has no Fresnel term (`F = 1`). `conductor` always builds it that way. `plastic` does too at its default `ior` 0, which matches the standard `plastic` shader.
- `color` parameters are read with the current `ColorSamples` count and converted to RGB with `COptions::convertColor`, as `RiColor`'s are (RISpec §4.1.3).

| Name (models) | Parameters (`type name` default; range; out of range) |
|---|---|
| `diffuse` (`matte`) | `float Kd` 1; [0, ∞); clamp. Extension: `float roughness` 0; [0, 1]; clamp (Oren-Nayar when > 0) |
| `plastic` (`plastic`) | `float Kd` .5; [0, ∞); clamp. `float Ks` .5; [0, ∞); clamp. `float roughness` .1; [0, 1]; clamp. `color specularcolor` 1; [0, ∞); clamp. Extension: `float ior` 0; [0, ∞); refuse (0: no Fresnel term; > 0: dielectric Fresnel) |
| `conductor` (`metal`) | `float Ks` 1; [0, ∞); clamp. `float roughness` .1; [0, 1]; clamp |
| `dielectric` (no RISpec counterpart) | `float ior` 1.5; (0, ∞); refuse. `float roughness` 0; [0, 1]; clamp. `color reflectColor` 1; [0, ∞); clamp. `color transmitColor` 1; [0, ∞); clamp |

- The trees are listed in research R21: `Kd*Cs * diffuse(N)`; `Kd*Cs * diffuse(N) + Ks*specularcolor * microfacet("ggx", N, roughness, ior, 0)`; `Ks*Cs * microfacet("ggx", N, roughness, 0, 0)`; and the `dielectric` pair, `reflectColor * microfacet("ggx", N, roughness, ior, 0) + transmitColor * microfacet("ggx", N, roughness, ior, 1)`.
- `N` in the point is the shading normal, `faceforward(normalize(N), I)` as in every Appendix A surface, in camera space.

## Instance

```cpp
class CBxdfInstance final : public CShaderGroupRefTarget {
  public:
    const OrBxdfTable *table; OrBxdfData *data; std::string handle;
    ShaderGroupRef substituteSurface; // A′ (Q-R1): the defaultsurface RiWorldBegin bound (set at bind, or lazily by RiWorldBegin for a Bxdf bound earlier), or empty for the no-shader path
};
CBxdfInstance *bxdfInstanceOf(const ShaderGroupRef &ref); // checked: dynamic_cast; nullptr for anything else
```

- Callers treat `nullptr` as "no Bxdf instance". A foreign object in the slot (a State double, an RSL instance in an old test) therefore never selects the Bxdf path.
- `shaderInstanceOf(attributes.bxdf())` must not be used; the header says so.
- Spec 036 and 049 Bxdfs register a table whose `build` runs their shader. They never put a `CShaderInstance` in the slot.

## `RiBxdf` / `RiBxdfV` (Scene Interface)

```c
RtVoid RiBxdf(RtToken name, RtToken handle, ...);
RtVoid RiBxdfV(RtToken name, RtToken handle, RtInt n, RtToken tokens[], RtPointer params[]);
```

- `handle` may be `RI_NULL`.
- It is checked with its own mask, `VALID_BXDF_BLOCKS` = `VALID_ATTRIBUTE_BLOCKS` without `RENDERMAN_OBJECT_BLOCK` (`ri.cpp`; Q-R4). Inside an `ObjectBegin` block `check()` refuses it as any out-of-block request, because RISpec §5.10 allows no shader or attribute assignment there.
- `RiSurface` and `RiAttribute` keep `VALID_ATTRIBUTE_BLOCKS`. That older deviation is filed by the coordinator as its own issue and is not 035 work.
- `CRiInterface::RiBxdfV` ignores it by default, so the preview and other back-ends need no edit.

**RIB grammar** (`rib.l`: `Bxdf` returns `RIB_BXDF`):

```text
RIB_BXDF RIB_TEXT RIB_TEXT ribPL   -> RiBxdfV($2, $3, ...)   // with a handle
RIB_BXDF RIB_TEXT ribPL            -> RiBxdfV($2, NULL, ...) // without
```

- This is the `LightSource` precedent: a second string is the handle. Without a handle, a first parameter whose value is a bare string must be written in brackets (`"string x" ["y"]`). The RIB guide says so (Q9, approved).
- This is the grammar spec 034 extends with `"reference <type> name"` parameters (FR-013).

**`CRendererContext::RiBxdfV`** (renderer core):

1. If `CRenderer::netNumServers > 0`, return. A `-P:N` client never shades; the servers parse the RIB (R12).
2. Look the name up with `findBxdf`. On a refusal:
   - report `CODE_BADTOKEN`. The text names the statement, the name and the reason, and adds "reserved for RSL 2.0 / OSL Bxdfs (specs 036, 049)" for a prefix;
   - return without touching the attributes or `pendingInstanceKey`.
3. Type the parameters against `table->params`:
   - an unknown parameter gives `CODE_BADTOKEN`, naming it, and the parameter is skipped;
   - a parameter out of range is clamped with `CODE_RANGE` naming it, or the whole statement is refused with `CODE_RANGE`, according to its `onOutOfRange`;
   - a missing parameter takes its default;
   - a `color` parameter is converted from the current `ColorSamples` count with `COptions::convertColor` (RISpec §4.1.3).
4. Call `table->create`. `NULL` refuses the statement and leaves the binding unchanged.
5. Make the `CBxdfInstance`, with the handle and the FR-026 substitute surface:
   - A′ (Q-R1, RISpec §4.2.4): the `defaultsurface` handle that the core captured at `RiWorldBegin`, **only when `setDefaultSurface()` bound it**;
   - empty otherwise: a pre-world `Surface` was given or `defaultsurface` could not be loaded. An empty substitute means the built-in no-shader path (`Cs*|I.N|`, `Os`);
   - **lazy, for a Bxdf bound before `WorldBegin`** (analyze B2, order per R1): the substitute starts empty. In the `RiWorldBegin` block, when `defaultsurface` is about to be bound, the core **first** hands the loaded `cShader` to the `CBxdfInstance` already in the current attributes' bxdf slot (`bxdfInstanceOf`) and **then** calls `setDefaultSurface()`, whose `checkParameters()` stores `usedParameters` from the substitute; the object then renders the initial default of §4.2.4;
   - never a user-bound `Surface`.
6. Call `getAttributes(TRUE)->bindBxdf(ShaderGroupRef::adopt(instance))`. That sets the last-bound record to Bxdf (D5, spec 025).
7. **Never** call `takeInstanceKey()`. A Bxdf draws nothing, and no key or random stream of the scene changes (FR-016).

**`CRibOut::RiBxdfV`**: writes `Bxdf "name" "handle" <pl>`, or without the handle when it is null, at the call's position. A `Surface`/`Bxdf` order therefore survives a round trip (D5, SC-008).

**Bindings**:

- `prman.py`: `def Bxdf(self, name, handle=None, parameterlist=None)`.
- `prman.lua`: `function Ri:Bxdf(name_str, handle_str, parameterlist)`.
- Both emit the same text as `ribOut` (SC-008).
