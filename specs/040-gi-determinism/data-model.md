# Data Model: GI Determinism (spec 040)

All entities are in memory unless a file or wire form is named. "Canonical order" always means an order that is a function of
the scene, the options and the seed alone. Field names are the code's where the field exists today.

## 1. Photon (Texturing, `CPhoton` in `texture/photonStore.h`)

| Field | Bytes (offset in the 44-byte item) | Notes |
|---|---|---|
| `P` | 0-11 | world-space position at the hit |
| `N` | 12-23 | normal facing the incoming photon |
| `flags` | 24-25 | kd-tree splitting axis written by `CMap::balance()` |
| padding | 26-27 | **zero** (new: FR-004) |
| `C` | 28-39 | power |
| `theta`, `phi` | 40, 41 | packed incoming direction |
| padding | 42-43 | **zero** (new: FR-004) |

- **Photon key** (side array `CPhotonStore::keys`, index-aligned with the items, never in a file):
  `photonKey(pointKeyFull, bounce)` (`photonMapAccess.h`), where `pointKeyFull` is the cell key of (light index, index in the
  frame's photon sequence, frame, seed) (`photon.cpp`, spec 039 FR-023). A photon read from a file has key 0.
- **Item 0** (the unused slot before the 1-based heap) and the **dummy photon** of an empty map (`P = 0`, `N = (0,0,1)`,
  `C = 0`, `theta = phi = 0`) are all-zero bytes but for the dummy's defined fields.

### Canonical photon order (FR-002)

Before `balance()`, items `1..numItems` are sorted by the total order
`(key, then the 42 defined bytes of P, N, C, theta, phi compared as unsigned bytes)`; the keys are permuted with them.
Two photons equal under this order are byte-identical, so their relative order cannot change a byte of the map. `balance()`
then runs on that order; its median partition, the tie between equal coordinates and every `flags` value are functions of the
photon set alone.

### Map lifecycle in a photon frame (FR-003)

```
idle (modifying == FALSE)
  -- first storePhotonForFrame() of the frame, under the store's lock: reset, modifying = TRUE, returns "claimed" -->
filling (modifying == TRUE)        every later storePhotonForFrame() only stores (same lock)
  -- write() at the photon hider's destruction (single-threaded): canonical sort, balance, zero padding, write, modifying = FALSE -->
idle
```

Exactly one caller sees "claimed" per frame and pushes the map on its balance list; no store can run between the claim and
the reset because both happen inside the one critical section.

## 2. Photon lookup (Texturing, `CPhotonStore::lookup`)

A pure function of (map contents, `P`, `N`, `maxFound`): the kd-tree search, the sum and the normalisation. No memo
(`PHOTON_LOOKUP_CACHE`, `probe()`, `insert()`, `root`, `maxDepth` are removed if decision D2 is taken) and no member written
(`searchRadius` becomes a local). See contracts/photon-store.md.

## 3. Irradiance record (Texturing, `CIrradianceRecord` / `CCacheSample` in `texture/irradianceStore.h`, `irradianceCache.h`)

Unchanged fields: `P`, `N`, `irradiance`, `coverage`, `envdir`, `gP[21]`, `gR[21]`, `dP`, `key`. `CCacheSample` adds the list
link `next` (`static_assert(offsetof(next) == sizeof(CIrradianceRecord))`, 240 bytes, the wire record).

Meaning of `dP` by place:

| Place | `dP` holds |
|---|---|
| a pending record (made this frame, not committed) and the wire | the **raw** radius of validity the sampler returned (`IrradianceSample::dP`) |
| a committed record (in the tree, in a file) | the **clamped** radius (as today) |

New companion data, outside the 240-byte record so the record and the file keep their layout:

- **Pending entry** (`CIrradianceCache::CPending`, new): the record (raw `dP`), the creating call's `maxError` (its `K =
  0.4 / maxError` places the record), nothing else.
- **Record identity** (computed, not stored): `key` when non-zero, otherwise a hash of the bit patterns of `P` and `N`
  (records read from a file carry no key). Used only by the accept rule (§6).

## 4. Fixed part `F` and batch scope `B` (the visibility of FR-008)

- **Fixed part `F`**: the committed tree at frame begin: the records read from the file (`CACHE_READ`), in file order. During
  the frame it is **read-only**: no insert, no clamp, no list change. Lookups read it without a lock.
- **Shading batch**: the points one `shade()` call evaluates (a REYES or zbuffer grid; a raytrace shading group chunk, i.e. the
  rays of one bundle that hit one object, in ray order, at most `maxGridSize`). Its composition is a function of the scene and
  the options (research.md R4); the order in which batches run is not, and does not matter.
- **Batch record list `B`** (Shading, new, in `CShadingState`, allocated from the thread's memory and reset at the start of each
  `shade()`): the records made by earlier lookups of the same batch, in creation order (the order the shader executes the GI
  calls, then lane order within a call), each with the cache it belongs to and its batch-clamped radius.
- **Visible set of a query** = `F` plus the entries of `B` of the same cache made before the query.

Batch-clamped radius of a new batch record `r`: `min(raw_r, min over j in F (dP_j + l), min over earlier j in B (raw_j + l))`,
and each earlier batch record `j` of the same cache becomes `min(radius_j, raw_r + l)`; `l = |P_r - P_j|`, or `C_EPSILON` for
coincident points (as `clamp()` today). `F` is never changed during the frame.

## 5. End-of-frame commit (FR-007, FR-009, FR-013, FR-014, FR-015)

Input: `F` and the pending set `Pend` (the frame's records from every thread; on a `-P:N` client, the records every server sent).
Run once, single-threaded, before the cache is written, sent or destroyed (`commitPending()`, called lazily by the destructor's
write and by `forEachNode`/`forEachRecord`).

1. **Dedupe** `Pend` by `key`; equal keys keep the smallest record under the byte order (a difference is a determinism defect and is
   reported once in a debug build).
2. **Sort** `Pend` by `(key, bytes)`.
3. **Clamp, order-independently** (every value from the raw or stored radii, never from a value clamped in this step, so the
   result does not depend on the visiting order; `min` is exact in floating point):
   - new record `i`: `dP_i = min(raw_i, min over j in F (dP_j + l_ij), min over j in Pend, j != i (raw_j + l_ij))`;
   - fixed record `k`: `dP_k = min(dP_k, min over j in Pend (raw_j + l_kj))`.
   Only pairs with `l < the radius being clamped` can change it, so a bounded spatial search (the existing octree, read-only, plus
   a temporary grid over `Pend`) is exact.
4. **Place** each new record in sorted order by `dP_i / K_i` (the existing descent rule; `K_i` from its pending entry). Fixed records
   stay in their node.
5. **List order** in every node: fixed records first, in file order, then new records in sorted order (appended at the tail).

Tree shape (which nodes exist), every node's list and every radius are then functions of `F ∪ Pend` as sets.

## 6. Accept decision (FR-010)

The weighted-discard test of `lookupCached` keeps a record when `w > u * smallSampleWeight` (`smallSampleWeight` 0.1 with
`CACHE_SAMPLE`, else 0). Under decision D1 (recommended) `u` is a pure hash of the query's identity (the copy of the bound
sub-stream key taken at the top of `irradianceLookup`, `boundCall`) and the record's identity (§3), mapped to `[0, 1)`; nothing is
drawn from the cursor. Under D1-B (not recommended) `u` stays `drawNext()` in traversal order.

## 7. Sampler draws (FR-011, FR-012; `hiders/irradianceSampler.cpp`)

All draws come from a copy of the bound call's sub-stream taken when the sampler starts, by index, never by cursor position:

| Path | Directions | Origin jitter |
|---|---|---|
| `occlusion()`, cache off (`maxError == 0`) | `samples` directions, pair 0, sample `k` (as since spec 039 T076c) | pair 1, sample `k` |
| `indirectdiffuse()`, cache off | **new**: the same as occlusion (pair 0, sample `k`, `samples` rays) | pair 1, sample `k` |
| `occlusion()` / `indirectdiffuse()`, cache on | the `nt x np` grid kept (the gradients need it); the in-cell offsets of cell `k = i * np + j` from pair 0, sample `k` | pair 1, sample `k` |
| point-based lookup | nothing drawn | nothing drawn |

## 8. Files and wire (FR-014, FR-015, FR-022)

| Form | Layout | What changes |
|---|---|---|
| `.gpm` (photon map) | ropen header, `numItems`, `maxItems`, `numItems + 1` raw `CPhoton` items, bound, two matrices, `maxPower` | nothing in the layout; every byte defined; items in balanced order of the canonical photon order |
| `.icache` (irradiance cache) | `maxDepth`, then the node tree depth-first, each node: centre, side, count, records field by field, 8 child markers | nothing in the layout; record order canonical (§5); `readNode` appends, so a file reads back in its written order |
| wire (`CRemoteICacheChannel`) | per node with records: count, then raw 240-byte `CCacheSample`s; a 0 count ends | the server sends its **pending** records only, `dP` raw, and (decision D4) the 8-byte link slot carries the record's `maxError` as a float followed by 4 zero bytes; the client adds them to its pending set and commits canonically |

## 9. Validation rules (from the requirements)

- A map's `numItems` after a pass equals the number of `storePhotonForFrame` calls of the frame (FR-003).
- Two `.gpm` files of the same scene and seed are byte-identical at any `-t` (FR-001, FR-004).
- `lookupCached` and the commit give bit-identical results for any permutation of the pending set (FR-009, SC-004).
- The record a miss creates at a given point is bit-identical whatever `F` holds (FR-011, SC-003).
- A cache written and read back answers lookups bit-identically, and two reads of one file agree (FR-014, SC-005).
- The pending set of a `-P:N` render, committed on the client, equals the pending set of the same render in one process (FR-015).
