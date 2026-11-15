# Contract: the irradiance store (Texturing, `texture/irradianceStore.h`)

Owner: Texturing. Consumer: Shading (`libshader::irradianceLookup`, the only caller) and the renderer core's remote channel
(`render/remoteChannel.cpp`) and data viewer. Edges unchanged: Shading -> Texturing through this header (allowed); Texturing
includes only Texturing and Infra (`common/sampling.h` for the hash `mix`). No services member changes, so
`kRendererServicesVersion` **steps** (FR-021, maintainer decision 2026-11-15): `rendererServices.h`'s 2.2.0 history names `irradianceCalls.h` and `IPhotonStore` as part of the versioned boundary. The rule: the next free MINOR on master at the time of the edit (probably 2.6.0), written as a literal with a history entry saying it is breaking within 2.x (tasks T039a, T062a, T079).

## Today (master `278b0394`)

```cpp
using IrradianceDrawFn = float (*)(void *drawState);
virtual bool lookupCached(float *C, const float *P, const float *N, float maxError, IrradianceDrawFn draw, void *drawState, float *Pc, float *Nc) = 0;
virtual void insertRecord(const CIrradianceRecord &record, float maxError) = 0;   // copy, clamp(), place, under the lock
virtual bool samplesOnMiss() const = 0;
// CIrradianceCache only: forEachNode, forEachRecord, insertReceived(const CCacheSample &)  (no clamp, no K: #161)
```

## After spec 040

```cpp
// The accept value of a record for the query in progress, in [0, 1): a pure function of (query, record identity) under
// decision D1 (contracts/draw-rules.md). recordIdentity is the record's key, or a hash of its P and N bits when the key is 0.
using IrradianceAcceptFn = float (*)(const void *acceptState, uint64_t recordIdentity);

// Records the caller's shading batch made before this query, of this cache (data-model.md §4). Read-only for the store.
struct CIrradianceBatchView {
    const CIrradianceRecord *records; // dP holds the batch-clamped radius
    int count;
};

// Walk the fixed part F (read-only during a frame, no lock), then the batch view, in that order and in the canonical order
// of each (tree traversal with node lists in canonical order; the batch in creation order). Calls accept() once per record
// that passes the front test. A hit writes C[0..6] and returns true; a miss returns false and leaves C alone. Writes Pc/Nc.
virtual bool lookupCached(float *C, const float *P, const float *N, float maxError,
                          IrradianceAcceptFn accept, const void *acceptState,
                          const CIrradianceBatchView &batch, float *Pc, float *Nc) = 0;

// The batch-clamped radius of a record about to join a batch: min(raw, min over F (dP + l)), F read-only. The caller (Shading)
// applies the batch's own records to it and to them (data-model.md §4).
virtual float clampAgainstFixed(const CIrradianceRecord &record) const = 0;

// Add a record made this frame to the pending set (raw dP; the call's maxError kept for placement). Under the lock; touches
// neither F nor any list a lookup reads. A cache opened CACHE_RDONLY ignores it.
virtual void addPending(const CIrradianceRecord &record, float maxError) = 0;

virtual bool samplesOnMiss() const = 0;
```

`CIrradianceCache` (not the interface):

- `void commitPending()`: the canonical end-of-frame commit (data-model.md §5). Idempotent; runs single-threaded. Called by the
  destructor before the write, and by `forEachNode`/`forEachRecord` before they walk the tree. Never called while render threads
  run (the core destroys frame files and sends remote frames after the threads are joined).
- `template <class F> void forEachPending(F &&visit) const`: the pending set in sorted order, for the remote channel's send.
- `void receivePending(const CCacheSample &record, float maxError)`: a record a server sent; joins the pending set (replaces
  `insertReceived`, which placed by raw `dP` with no `K`: #161).
- `readNode` appends to a node's list (file order kept); `writeNode` is unchanged.
- `insertRecord` and `clamp()` are removed (no caller left).

## Behaviour guarantees (the tests that hold them)

| Guarantee | Requirement | Test |
|---|---|---|
| `lookupCached` reads only F and the view; no state written | FR-008 | `GiDet_LookupPure` (a lookup twice in a row, and on two threads, gives the same bits) |
| commit result independent of the pending order | FR-009, SC-004 | `GiDet_CommitOrder` (100 random permutations: lookups, placement, radii bit-identical) |
| equal keys commit once | FR-013 | `GiDet_CommitDedupe` |
| write then read gives the same lookups; two reads agree | FR-014, SC-005 | `GiDet_CacheRoundTrip` |
| old files load and answer | US3-4 | `GiFiles_Unchanged` (spec 032, unchanged test) plus `GiDet_CacheRoundTrip` on a file written by the starting tree (fixture copied from `tests/gi_split` references) |
| received records commit like local ones | FR-015 | `GiRemote_*` (spec 032) extended: loopback client commit equals a one-process commit |
