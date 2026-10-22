# Data Model: Sampling Foundation (spec 039)

These are conceptual shapes. The names are proposals in house style, and the C++ layout is decided at implement. "Owner" is the archcheck class of the code that defines the type.

## Entities

### SampleId — owner: Infra (`libRIBCommon`)

SampleId names one camera sample.

| Field | Type | Rule |
|---|---|---|
| `px`, `py` | `int32` | Absolute raster pixel of the subpixel sample: `floor(absSub / n)`. Margin samples can lie outside the image, so values may be negative (research §R3.1). |
| `sampleIndex` | `uint32` | `(absSubY mod ny)·nx + (absSubX mod nx)`, so `0 ≤ sampleIndex < nx·ny`. |
| `frame` | `uint32` | `CSamplingConfig.frame`. |
| `seed` | `uint32` | `Option "sampling" "int seed"`, reinterpreted as `uint32`. Negative values are valid. |

- **Invariant**: two hiders, two buckets or two processes that build the same subpixel produce equal SampleIds (FR-009).

### SampleKey — owner: Infra

SampleKey is the 64-bit hashed form of an identity. Rays and per-point shading state carry it.

| Field | Type | Rule |
|---|---|---|
| `full` | `uint64` | `mix(SampleId)` for camera samples. For cells it is `mix(stableId, bits(u), bits(v), elementId, frame, seed)`. For child rays it is `mix(parent.full, depth, callSite, occurrence, sampleIndex)`. |
| `path` | `uint64` | The same as `full`, but it never mixes in the camera `sampleIndex`: `mix(px, py, frame, seed)` for camera samples, `= full` for cells, and `mix(parent.path, …)` for children. Pixel-sample sub-streams seed from it (§R3.5). |
| `pixelSampleIndex` | `int32` | The camera `sampleIndex` this key descends from, or `-1` for cell keys. It drives the index mapping in §R3.5. |
| `kind` | `uint8` | `Camera`, `Cell` or `Child`. Debug and test only. It does not enter the hash. |

### Dimension cursor — owner: Infra

- **Camera slots**: fixed. Film is 2D dimension 0, lens is 2D dimension 1, time is 1D dimension 2 and importance is 1D dimension 3. The layout is the same in reyes and raytrace (FR-009).
- **Sub-streams**: each sub-stream starts its own cursor at 0. A call site consumes a fixed, documented number of slots, for example trace: 0 = origin jitter, 1 = direction jitter, 2 = time.

### SubstreamKey — owner: Infra

| Field | Type | Rule |
|---|---|---|
| `seedHash` | `uint64` | `mix(pointKey.path if pixelSampleIndex ≥ 0 else pointKey.full, shaderKey, callSite, occurrence, depth)` |
| `indexBase` | `uint32` | `pixelSampleIndex · N` for pixel-sample points, `0` for cell points (§R3.5). |

- **Draw**: `get2D(SubstreamKey, dim, indexBase + i)`.
- **Draw-independence invariant**: two different (callSite, occurrence) pairs never share a `seedHash`, except by a 64-bit collision.

### CSamplingConfig — owner: Infra (built by Core)

| Field | Type | Rule |
|---|---|---|
| `backend` | enum | `Sobol` (default after Phase 9), `PMJ02`, `Independent`, or `Legacy` (internal only, deleted in Phase 9). |
| `seed` | `uint32` | From the option. Default 0. |
| `frame` | `uint32` | The current frame. |
| `nx`, `ny` | `int` | Pixel sample counts. |
| `jitter` | `float` | Copied from the hider option, so the hiders stop reading `CRenderer::jitter` where possible. |

- **Lifetime**: one immutable instance per frame. Core builds it at `beginFrame` and hands it to every shading context (§R4).

### Stable object identity — owner: Geometry (field on `CObject`); assigned by Core and Geometry

| Field | Type | Rule |
|---|---|---|
| `stableId` | `uint64` | Assigned by the rules in §R3.2 below. |
| `elementId` | per-vertex `uint32` (only where needed) | Written by `sample()` for primitives whose vertices share (u, v) (§R3.3). |

How `stableId` is assigned:

| Object | `stableId` |
|---|---|
| Top-level object | `mix(scope, ordinal)` |
| Procedural or instance child | `mix(parent, k)` |
| Tessellation or mesh child | `mix(parent, childIndex)` |
| Split piece | inherits its parent's |
| `cluster()` dummy node | `mix(parent, 0 or 1)` |

- **Invariant**: identical across thread counts, process counts and runs. Two distinct instances never share an identity.

### Per-point shading identity — owner: Shading (`CShadingState`)

| Field | Type | Rule |
|---|---|---|
| `pointKeys` | `SampleKey[numVertices]` | Filled for ray-hit points by Hiders (`trace.cpp`, from `CRay::key`). Filled for grid points by Shading (`shade()`, from the cell rule). Derivative-offset tail vertices copy their real vertex's key (§R3.10a). |
| `batchKey` | `uint64` | The invocation-level key for uniform `random()` (§R3.7): grid origin under reyes, (bucket, stableId, depth, chunkIndex) under raytrace, and derived from the parent `batchKey` for nested shading. |
| `callSiteOccurrence` | `int[numCallSites][numVertices]` | Lazily allocated in `threadMemory`. Incremented for active real lanes only. Tail vertices mirror their real vertex (§R3.10a). Reset per shader invocation. |
| `currentShaderKey` | `uint64` | `mix(shader name, instance ordinal)`. |

### CRay additions — owner: Infra (`src/ri/core/ray.h`)

| Field | Type | Rule |
|---|---|---|
| `key` | `SampleKey` | Primary rays get the camera key. Secondary rays get a child key, set at spawn. |
| `depth` | `uint16` | Primary rays are 0. A secondary ray's depth is its parent's plus 1. |
| `jimp` | existing | Primary rays use the camera importance slot, which `trace()` no longer overwrites. Secondary rays use a sub-stream draw. |

### CKeyedStream — owner: Infra

A local, non-sample stream for clustering, `getDisp`, the point hierarchy and the radiance cache. It is constructed from a key and returns `hash(key, counter++)`. It must never be stored beyond the call frame that owns it (§R3.10).

### Call-site ordinal — owner: Shading (compiler and runtime)

| Field | Type | Rule |
|---|---|---|
| `callSite` | `uint16` | Assigned by the compiler on the final IR, in IR order, for each stochastic builtin call. It is carried as a `.rslo` instruction operand and as a JIT `op_*` constant argument. It is identical on both backends (§R3.6). |

## Forward mapping to specs 026 and 028 (FR-011)

These are taken from the master-plan briefs. Neither spec is implemented yet; each must carry these fields unchanged.

| This spec | 026 `ShaderGlobals` (batch/grid-shaped) | 028 `ICamera` |
|---|---|---|
| `CShadingState.pointKeys[i]` | per-point `sampleKey` field in the batch | — |
| `CShadingState.batchKey` | per-batch `batchKey` | — |
| `CRay.depth` | `pathDepth`, which 026 already lists | — |
| `currentShaderKey`, `callSite`, `occurrence` | runtime-internal; not part of `ShaderGlobals` | — |
| camera slots Film2D/Lens2D/Time1D | — | `generateRay(x, y, t, lensUV)`: `x, y` from the film slot through the jitter mapping, `t` from Time1D, `lensUV` from Lens2D |
| Importance1D | `rayImportance` (`jimp`) | carried on the generated ray |
| `SampleId` | the identity of camera-ray points | the input that `generateRay` is evaluated for |

## Relationships

```
CSamplingConfig ──(per frame)──► CShadingContext (Hiders, Shading read their member)
SampleId ──mix──► SampleKey ──► CRay.key ──(trace.cpp)──► CShadingState.pointKeys
CObject.stableId + (u,v,elementId) ──mix──► SampleKey (Cell) ──► CShadingState.pointKeys
pointKey + shaderKey + callSite + occurrence + depth ──► SubstreamKey ──► get1D/get2D
SubstreamKey ──(child)──► CRay.key of a secondary ray
```

## Validation rules (from the spec)

- **Purity**: `get1D(k, d)` and `get2D(k, d)` depend only on the backend, `k` and `d` (FR-003).
- **Range**: every value lies in [0, 1 − 2⁻²⁴] (FR-004).
- **Padding**: any `d ≥ 0` and any index ≥ 0 is valid (FR-006, and the edge cases).
- **Draw independence**: the draws at (callSite A, occurrence n) are unchanged by adding draws at any other (callSite, occurrence) (FR-007).
- **Cross-hider consistency**: reyes and raytrace produce equal camera SampleIds and slot values for the same absolute subpixel (FR-009).
- **Jitter mapping**: `film = jitter·(u−0.5)+0.5001011` (clarify Q4). `jitter 0` gives today's fixed positions exactly.

## State transitions

- **`backend`**: `Legacy` (default, Phases 1–8) → `Sobol` (default from Phase 9). `Legacy` is deleted in Phase 9.
- **`callSiteOccurrence`**: zeroed when a shader invocation starts; each active lane at a call site increments it by one.
