# Contract: the photon store (Texturing `texture/photonStore.{h,cpp}`, `core/map.h`; Shading `photonMapAccess.h`)

Owners: Texturing for `CPhotonStore`/`CMap`; Shading for the `IPhotonStore`/`IPhotonEstimator`/`IPhotonLookup` interfaces the
photon hider (Hiders) and the sampler use; Core's header-only `CPhotonMap` forwards. No edge is added.

## `IPhotonStore` (Shading, `photonMapAccess.h`)

Today: `storePhoton(P, N, I, C, key)`, `resetPhotons()`, `balancePhotons()`, `isModifying()`, `setModifying()`,
`writePhotons(world)`; the photon hider runs `if (!isModifying()) { setModifying(TRUE); resetPhotons(); balanceList.push(map); }`
outside any lock (`photon.cpp:633-651`).

After:

```cpp
class IPhotonStore {
    public:
        virtual ~IPhotonStore() = default;
        // Store one photon of the current photon frame. Under the store's lock: the first call of a frame resets the map
        // (items, keys, bound, maxPower) and marks it as being written, then stores; returns true for that first call only,
        // so exactly one caller puts the map on its balance list (FR-003).
        virtual bool storePhotonForFrame(const float *P, const float *N, const float *I, const float *C, uint64_t key) = 0;
        virtual void writePhotons(const CXform *world) = 0; // single-threaded; canonical sort, balance, zero padding, write
};
```

`IPhotonStore` is named by `rendererServices.h`'s 2.2.0 history as part of the versioned boundary, so this change is a breaking MINOR step of `kRendererServicesVersion` (task T039a). `resetPhotons`, `balancePhotons`, `isModifying` and `setModifying` leave the interface (their only callers are the lines
replaced). `photonKey()` is unchanged.

## `CPhotonStore` / `CMap`

| Member | Change | Requirement |
|---|---|---|
| `CMap::reset()` | `delete[] items` (was `delete`, GitHub #165) | FR-003 |
| `CPhotonStore::reset()` | also zeroes `maxPower` and the bound | FR-001, FR-006 (the stale `maxPower` found in R1) |
| allocations (`store` growth, `balance`'s `finalitems`, `read`) | value-initialised (`new T[n]()`) | FR-004 |
| `CPhotonStore::balance()` | sorts items `1..n` and their keys into the canonical photon order (data-model.md §1) before `CMap::balance()`; the empty-map dummy is stored with `theta = phi = 0` | FR-002 |
| `CPhotonStore::write()` | rebuilds every item in a zeroed buffer and copies it back with `memcpy` before writing (padding and item 0 zero, whatever the compiler's struct copy did) | FR-004 |
| `lookup(C, P, N, maxFound)`, `lookup(C, P, maxFound)` | no memo under decision D2: `probe()`/`insert()`/`root`/`maxDepth`/`PHOTON_LOOKUP_CACHE` removed; `searchRadius` a local | FR-005, #160 |
| `photonKey(i)` | unchanged; after the sort the keys are in balanced order | - |

The file layout does not change (contracts/files-and-wire.md).

## Guarantees and tests

| Guarantee | Test |
|---|---|
| `numItems` equals the number of stores of the frame, from any number of threads, one claim per frame | `GiDet_PhotonClaim` (unit: 8 threads x N stores on a `CPhotonStore`; exactly one `true`) |
| every byte of a written map defined; pad bytes, item 0 zero | `GiDet_PhotonFileBytes` (byte-level, not field by field) |
| a map is byte-identical across 30 separate processes at `-t:1`, and across `-t:1`, `-t:2`, `-t:4`, `-t:8` and the default | `GiDet_PhotonRepeat`, `GiDet_PhotonThreads` (opt-in label `gi_determinism_long` for the 30-run form), the sweep |
| a lookup is a function of map and query (twice in a row, on two threads, in two orders) | `GiDet_PhotonLookupPure` |
| a map filled in memory and never read can be queried (#160) | `GiDet_PhotonInMemoryLookup` |
| an in-memory store's lookups equal the reloaded store's, bit for bit (SC-005) | `GiDet_PhotonRoundTrip` |
| `Registry_IntegratorImageIdentity`'s photon rows (unchanged test) | run 30 times standalone per platform (validation) |
