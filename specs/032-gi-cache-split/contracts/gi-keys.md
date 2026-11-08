# Contract: deterministic keys for spec 040

**Requirements**: FR-017, FR-018, FR-019; US5; Q3 B. **Research**: R10.

## Definitions

Both keys are 64-bit. `mix` is `openrender::sampling::detail::mix` (SplitMix64, `common/sampling.h:118-123`). The tag
constants are new and fixed.

| Key | Function | Inputs | Where computed |
|---|---|---|---|
| record | `irradianceRecordKey(const SubstreamKey &k) = mix(0x1CAC4E5Aull, k.seedHash, k.indexBase)` | the sub-stream `bindDraws()` bound for the occlusion()/indirectdiffuse() call: a pure function of the shading point's key, the call site, the occurrence and the ray depth | `libshader::irradianceLookup`: `ctx.boundDraws()` is copied at entry, before the store lookup and the sampler, and the key is computed from that copy when a record is inserted |
| photon | `photonKey(uint64_t pointKeyFull, uint32_t bounce) = mix(0xF07011BEull, pointKeyFull, bounce)` | `currentShadingState->pointKeys[v].full` (light, index in the frame's photon sequence, frame, seed: `photon.cpp:207-212`); `bounce = numDiffuseBounces + numSpecularBounces` at the store | `CPhotonHider::tracePhoton`, at each `storePhoton` |

The helpers are inline:
- `irradianceRecordKey` in `irradianceCalls.h` (Shading);
- `photonKey` in `photonMapAccess.h` (Shading, which the photon hider includes).

Neither draws a value from a stream.

## Storage

- **Record key**: `CIrradianceRecord::key`, offset 224 (`next` moves to 232).
  - In memory and on the wire (`remote-channels.md`).
  - Not in cache files: `writeNode`/`readNode` stay field by field, and `readNode` sets `key = 0`.
- **Photon key**: `CPhotonStore`'s side array `keys`, index-aligned with `items`.
  - `store(P, N, I, C, key)` writes `keys[numItems]` after `CMap::store`, growing the array to `maxItems + 1` when
    `CMap` grew.
  - `store(P, N, I, C)` stores key 0.
  - `reset()` frees it.
  - `read()` allocates it zeroed (no file carries a key).
  - `balance()` on an empty map stores the dummy photon with key 0.

## `CMap` permutation hook (`src/ri/core/map.h`, Infra)

```cpp
protected:
    // Called by balance() once the balanced order is known: sorted[i] (i = 1..count) points into oldItems, the array
    // being replaced. A subclass holding data index-aligned with items reorders it here. Default: nothing.
    virtual void permuted(T *const *sorted, const T *oldItems, int count) {}
```

`balance()` calls `permuted(ar1, items, numItems)` after `finalitems` is filled and before `delete[] ar1; delete[]
items;`. Nothing else in `balance()` changes, so the item order and the tree are identical. `CPointCloud`,
`CPointHierarchy` and `CDepository` do not override it. `CPhotonStore::permuted` builds
`newKeys[i] = keys[sorted[i] - oldItems]`.

The hook adds one virtual slot to every `CMap<T>` vtable. Every `CMap` subclass is compiled together, so no layout
compatibility is at stake.

## Read access

- `CIrradianceCache::forEachRecord(f)` exposes `record.key`.
- `CPhotonStore::photonKey(int i)` exposes `keys[i]` for `1 <= i <= numItems`.

No rendering path reads either key (FR-018).

## Properties the key test checks (T-E)

1. **Photons.** Run `photon-parity`'s photon pass in process and dump `{(photonKey(i), photon fields)}` of each map:
   `P`, `N`, `flags`, `C`, `theta`, `phi`, never the raw bytes, which include padding (OQ-8).
   - The set is identical at `-t:1` twice.
   - At `-t:1` against `-t:4` it is identical in the opt-in `GiKeys_PhotonsThreads` only (OQ-3).
   - Keys are unique within a map.
   - The written `.pm` equals master's field by field (T-F).
2. **Irradiance records.** Run `sphere-occlusion-reyes` with a written cache at `-t:1` twice. The `{(key, record)}`
   sets are identical and keys are unique.
   - Across thread counts the record set itself is order-dependent (spec 040's subject), so T-E checks only that the
     records the two runs share have the same keys. US5's Independent Test was corrected to say so (OQ-3, maintainer
     answer 2026-11-07).
3. **No image changes** (the GI set, SC-003).
