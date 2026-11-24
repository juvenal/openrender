# Contract: the closure ABI (tree, registry, BSDF table)

- **Headers**:
  - `src/libshader/closures/closureAbi.h`: strict C17 and C++20; it includes only `<stdint.h>`, `<stddef.h>` and `orplugin.h`;
  - `closureTree.h`, `closureRegistry.h` and `bsdf.hpp`: the C++ views.
- **Domain**: Shading, compiled into `libRSLRuntime`. **Not installed**: it is not in `OPENRENDER_PUBLIC_HEADERS`, and it stays internal until spec 051/052.
- **Requirements**: FR-001 to FR-012; research R2 to R7 and R14.

## Version

```c
#define OR_CLOSURE_ABI_VERSION OR_ABI_VERSION(1, 0, 0)
```

The header's version-history table:

| Version | Change |
|---|---|
| 1.0.0 | Spec 035. The node layout (component, multiply, add), the registry entry shape, the eight built-in closures and their parameter blocks, and the `OrBsdfTable` conventions. New in the path-tracing milestone; it starts at 1.0.0 (maintainer, 2026-11-23). The MAJOR-2 rule of `hiderAbi.h` and `kRendererServicesVersion` covers interfaces that predate the milestone, not this one. 1.x is unstable until the milestone closes: until then a breaking change steps MINOR and says so here; after it, only a real breaking change steps MAJOR, to 2.0.0. |

This version moves independently of the RSL language version and of `kRendererServicesVersion` (D22). It versions three things:

- the node structs;
- the registry entry and the built-in parameter blocks;
- the BSDF table's conventions.

A change to any of them is a step of this version.

## Nodes

```c
enum { OR_CLOSURE_MUL = -1, OR_CLOSURE_ADD = -2 }; /* id >= 0: a registered component */

typedef struct OrClosureNode { int32_t id; } OrClosureNode;

typedef struct OrClosureComponent {
    OR_ALIGNAS16 int32_t id; /* registered closure id; the member alignment makes the struct 16-aligned */
    float   w[3]; /* weight, three channels (D17, D18) */
    /* parameter block follows: (const char *)(c + 1) */
} OrClosureComponent;

typedef struct OrClosureMul { int32_t id; float weight[3]; const OrClosureNode *closure; } OrClosureMul;
typedef struct OrClosureAdd { int32_t id; const OrClosureNode *closureA; const OrClosureNode *closureB; } OrClosureAdd;
```

- `OR_ALIGNAS16` expands to `_Alignas(16)` in C and `alignas(16)` in C++. It is applied to the first member, because C17 does not allow an alignment specifier on a struct tag.
- **Pinned on LP64**, by `static_assert` in `closureAbi.h` and printed by `Closure_Layout`:

  | Struct | sizeof | alignof | offsets |
  |---|---|---|---|
  | `OrClosureComponent` | 16 | 16 | `id` 0, `w` 4 |
  | `OrClosureMul` | 24 | 8 | `id` 0, `weight` 4, `closure` 16 |
  | `OrClosureAdd` | 24 | 8 | `id` 0, `closureA` 8, `closureB` 16 |

- These numbers are what spec 049's layout test compares against `oslclosure.h` (`ClosureComponent`, `ClosureMul`, `ClosureAdd`, reference doc §2.4). 035 neither includes nor links OSL.
- **The compatibility figure is the parameter-block offset (16)** (analyze E13): OSL's component may carry a 16-aligned `mem[4]` member that would make its `sizeof` 32 where ours is 16, while the block offset would still match. This is not verified against `oslclosure.h` (plan R-2). `Closure_Layout` pins the block offset as the compatibility figure and prints `sizeof`, `alignof` and the other offsets separately for spec 049.

## Registry

```c
typedef enum OrClosureCategory { OR_CLOSURE_BSDF = 0, OR_CLOSURE_EDF = 1, OR_CLOSURE_VDF = 2, OR_CLOSURE_SIGNAL = 3 } OrClosureCategory;
typedef enum OrClosureParamType { OR_CPARAM_FLOAT, OR_CPARAM_INT, OR_CPARAM_COLOR, OR_CPARAM_NORMAL, OR_CPARAM_VECTOR, OR_CPARAM_STRING } OrClosureParamType;
typedef struct OrClosureParam { const char *name; OrClosureParamType type; uint32_t offset; } OrClosureParam;

typedef struct OrClosureEntry {
    uint32_t structSize;
    int32_t id;                      /* assigned by the registry; ignored on registration */
    const char *name;
    OrClosureCategory category;
    const OrClosureParam *params; int numParams;
    uint32_t blockSize, blockAlign;
    const struct OrBsdfComponentOps *ops; /* BSDF: required; others: NULL */
} OrClosureEntry;
```

The C++ side (`closureRegistry.h`, namespace `openrender::shading`):

- `int registerClosure(const OrClosureEntry &)`:
  - returns the new id;
  - returns -1 with a diagnostic for a duplicate name, a BSDF entry without `ops`, or a param whose offset or size overruns the block.
- `const OrClosureEntry *findClosure(const char *name)`:
  - resolves the aliases. `emission` resolves to `uniform_edf`, with one deprecation message per process;
  - an unknown name returns `nullptr` with a diagnostic naming it.
- `const OrClosureEntry *closureEntry(int32_t id)`.
- `const char *internClosureString(const char *)`: interns a string parameter (the `microfacet` distribution).

**Built-ins, with fixed ids and the blocks of data-model.md**:

| Id | Name | Category |
|---|---|---|
| 0 | `diffuse` | BSDF |
| 1 | `oren_nayar` | BSDF |
| 2 | `microfacet` | BSDF; `"ggx"` and `"beckmann"`; `refract` 0, 1 or 2 |
| 3 | `reflection` | BSDF |
| 4 | `refraction` | BSDF |
| 5 | `transparent` | SIGNAL |
| 6 | `holdout` | SIGNAL |
| 7 | `uniform_edf` | EDF |

- The value 2 of `refract` is to be verified against `stdosl.h`.
- A `microfacet` with `eta = 0` and `refract` 0 has no Fresnel term (`F = 1`). The built-in `conductor` and `plastic` (at its default `ior` 0) rely on this. `eta = 0` with `refract` 1 or 2 is refused when the node is built, because a refraction needs an index.
- Registration is open: specs 036 and 049 call `registerClosure`, with no translation layer.

## Building, flattening, lifetime

`ClosureTreeBuilder` (`closureTree.h`), one per batch:

- `const OrClosureNode *component(int32_t id, const float w[3], const void *block, size_t blockSize)`. It returns `nullptr`, which drops the node, when:
  - the id is unknown;
  - `blockSize` is not the entry's;
  - a channel of `w` is negative or non-finite (one diagnostic per frame);
  - the budget is exceeded (one `CODE_LIMIT` per frame; the point's root becomes empty).
- `mul(const float w[3], const OrClosureNode *child)` and `add(a, b)`. A `nullptr` child collapses: `mul(w, nullptr)` is `nullptr`, and `add(a, nullptr)` is `a`.
- `void setRoot(int point, const OrClosureNode *root)`, for `point` in `[0, numRealVertices)`.
- **Memory**: everything comes from the batch's `threadMemory` page. A tree is valid until the `memEnd(threadMemory)` of the batch that built it. `closureTreeCopy(const OrClosureNode *, CMemPage *&)` deep-copies one. There is no per-node free.
- **Budget**: `numRealVertices * kClosureTreeBytesPerPoint`, with the constant at 1024 bytes (Q12, approved).

`ClosureLobeList closureFlatten(const OrClosureNode *tree, CMemPage *&arena)`:

- walks depth first, A before B, with an accumulated weight;
- drops a lobe whose accumulated weight is zero in every channel.

`closureEvaluateGeneric(tree, query)` recurses over the same nodes in the same order and skips the same zero weights. For any tree of the first registry, its answer is **bit-identical** to the lobe-list path. SC-005's 1e-6 bound is kept for later non-additive closures.

The accessors (FR-006) never require the consumer to walk the tree:

| Accessor | Returns |
|---|---|
| `float closureTreePresence(tree)` | the channel mean of the summed `transparent` weights; 0 when there is none (D16) |
| `Spectrum closureTreeEmission(tree)` | the summed EDF contributions (`uniform_edf`: `weight * emittance`) |
| `float closureTreeHoldout(tree)` | the channel mean of the summed `holdout` weights |

## BSDF table

```c
typedef enum OrTransportMode { OR_TRANSPORT_RADIANCE = 0, OR_TRANSPORT_IMPORTANCE = 1 } OrTransportMode;
enum { OR_BSDF_REFLECTION = 1u << 0, OR_BSDF_TRANSMISSION = 1u << 1, OR_BSDF_DIFFUSE = 1u << 2, OR_BSDF_GLOSSY = 1u << 3,
       OR_BSDF_SPECULAR = 1u << 4, OR_BSDF_DELTA = 1u << 5, OR_BSDF_HAS_DELTA = 1u << 6, OR_BSDF_NOT_SCATTERER = 1u << 7,
       OR_BSDF_FAILED = 1u << 8, OR_BSDF_ALL = 0x1Fu };
#define OR_SPECTRUM_MAX 4 /* storage bound of the C answer; numChannels says how many are used (3 now) */

typedef struct OrBsdfQuery  { uint32_t structSize; float wo[3], wi[3]; float Ng[3]; OrTransportMode mode; uint32_t requested; } OrBsdfQuery;
typedef struct OrBsdfEval   { float f[OR_SPECTRUM_MAX]; int32_t numChannels; float pdfForward, pdfReverse; uint32_t flags; } OrBsdfEval;
typedef struct OrBsdfSample { float f[OR_SPECTRUM_MAX]; int32_t numChannels; float pdfForward, pdfReverse; uint32_t flags; float wi[3]; float eta; } OrBsdfSample;

OR_DECLARE_HANDLE(OrBsdf);
typedef struct OrClosureAllocator { void *context; void *(*allocate)(void *context, size_t size, size_t align); } OrClosureAllocator;
typedef struct OrShadingFrame { float N[3]; float dPdu[3]; float I[3]; } OrShadingFrame;

typedef struct OrBsdfTable {
    OrTableHeader header; /* structSize = sizeof(OrBsdfTable), abiVersion = OR_CLOSURE_ABI_VERSION */
    OrStatus (*prepare)(const OrClosureNode *tree, const OrShadingFrame *frame, const OrClosureAllocator *alloc, OrBsdf **out);
    OrStatus (*evaluate)(const OrBsdf *bsdf, const OrBsdfQuery *q, OrBsdfEval *out);
    OrStatus (*sample)(const OrBsdf *bsdf, const OrBsdfQuery *q, float u1, float u2, float uLobe, OrBsdfSample *out);
    OrStatus (*pdf)(const OrBsdf *bsdf, const OrBsdfQuery *q, OrBsdfEval *out); /* f left 0 */
} OrBsdfTable;

typedef struct OrBsdfComponentOps { /* one per BSDF closure; block = the component's parameter block */
    void  (*evaluate)(const void *block, const float wo[3], const float wi[3], OrTransportMode, OrBsdfEval *out);
    void  (*sample)(const void *block, const float wo[3], float u1, float u2, OrTransportMode, OrBsdfSample *out);
    void  (*pdf)(const void *block, const float wo[3], const float wi[3], OrTransportMode, OrBsdfEval *out);
    float (*albedo)(const void *block, const float wo[3]); /* lobe-selection estimate in [0, 1] */
} OrBsdfComponentOps;

const OrBsdfTable *orBsdfTable(void); /* the host's table */
```

The component operations take directions already expressed in the component's local frame, which is built from its own `N` (R7).

**Conventions**, which `Bsdf_*` and `Closure_*` tests assert:

1. `wo` and `wi` point away from the surface (§2.6). `f` excludes `|cos theta_i|`. Densities are in sr^-1.
2. Every answer fills `pdfForward = p(wi | wo)` and `pdfReverse = p(wo | wi)` under the same `mode`. The reverse density equals the forward density of the swapped query (§7.4).
3. **Delta lobes**:
   - `evaluate` and `pdf` give them nothing, and set `OR_BSDF_HAS_DELTA` when the prepared BSDF has one;
   - `sample` may choose one. It then sets `OR_BSDF_DELTA`, `f` is the Fresnel-weighted delta value, and `pdfForward`/`pdfReverse` are the **discrete** lobe-choice probabilities, not densities.
4. **Mixture**: the lobe-choice probability is proportional to `mean(weight) * albedo(wo)`, renormalised over `requested`. For non-delta lobes, `pdf = sum_i p_i pdf_i`, and `evaluate`'s `f = sum_i weight_i f_i`.
5. **`IMPORTANCE` versus `RADIANCE`**: a transmissive lobe in `RADIANCE` mode multiplies `f` by `(eta_i/eta_t)^2`. Reflection is identical in both modes.
6. **Below the horizon or failed**: `f = 0`, densities 0, `OR_BSDF_FAILED` on a sample. No output is negative or non-finite.
7. **Not a scatterer**: a prepared BSDF with no BSDF lobe answers zero with `OR_BSDF_NOT_SCATTERER`. EDF and signal lobes never enter the mixture.
8. **Roughness floor**: a `microfacet` lobe with `alpha < kMicrofacetDeltaAlpha` (1e-4) behaves as `reflection`/`refraction`, and is flagged as a delta.
9. **Statelessness**: `prepare` writes only into `alloc`. `evaluate`, `sample` and `pdf` read only their arguments. The same inputs give bit-identical outputs on any thread (FR-008, US2 AS6). The table draws nothing; `u1`, `u2` and `uLobe` come from the caller.
10. **Status**: `OR_STATUS_OK`, or `OR_STATUS_BAD_ARGUMENT` for a null pointer or a `structSize` smaller than 1.0.0's. An answer with `OR_BSDF_FAILED` is still `OR_STATUS_OK`.

The C++ wrapper (`bsdf.hpp`) offers `Bsdf::prepare(tree, frame, arena)`, plus `evaluate`, `sample` and `pdf`, returning structs with `Spectrum f`.

## Numerical acceptance (SC-004)

These checks run per BSDF component and per prepared mixture. Reciprocity runs in both modes where the lobe is not symmetric. **Energy and the white furnace run in `IMPORTANCE` mode** (A1, maintainer 2026-11-23): in `RADIANCE` mode a transmissive lobe is multiplied by `(eta_i/eta_t)^2` (2.25 glass to air), so its albedo may exceed 1 by design; `Bsdf_ImportanceFactor` pins that factor itself. The tolerances are derived from each estimator's standard error, written in the test and never widened.

| Check | What is compared |
|---|---|
| reciprocity | `f(wo, wi) = f(wi, wo)`; for refraction, the documented factor |
| energy | directional albedo ≤ 1 + 3σ; white furnace within 3σ; in `IMPORTANCE` mode (A1) |
| density normalisation | ∫ pdf = 1 within 3σ, non-delta lobes |
| sample/density consistency | the sampled `f`/`pdf` equal the independent `evaluate`/`pdf`; chi-square of a 2-D histogram against the density at significance 0.001, with a fixed bin count |
| reverse density | the reverse density equals the forward density of the swapped query |

Random numbers come from spec 039's keyed stream (`common/sampling.h`), with test-local keys. 035 defines no call site in `sampling.h` (FR-008).
