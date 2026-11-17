# CLAUDE.md draft for spec 040 (gotcha 37)

`CLAUDE.md` is not edited by the implementation; the maintainer applies this at the merge. Phase 4 drafts the draw-order paragraph; the
batch-scope paragraph follows in T079 and the commit and wire paragraph in T089.

## Old (CLAUDE.md line 892 on, spec 032)

> 37. **The GI stores are passive; one Shading function does the cache step; the draw order is a rule (spec 032).**
> `libshader::irradianceLookup` (`libshader/shading/irradianceCalls.cpp`) is the only place an `occlusion()`/`indirectdiffuse()`
> cache lookup, miss and insert are decided, for the interpreter (`IDEXPR`) and the JIT (`jitOcclusionBatch`). Its order is the old one and
> must stay: a point-based lookup draws nothing; otherwise `CIrradianceStore::lookupCached` calls its draw callback **once per record that
> passes the front test** (zero-weight records included, in traversal order), then a miss runs the sampler on the same cursor; a name that
> `getCache` answered `nullptr` for gives the "no joy" value and draws nothing.

(The rest of the paragraph, on the include boundary, the keys, the cache file order, `CPhotonMap`, #160 and #161, is revised by the later
phases of spec 040: #160 is fixed by Phase 3 and the key sentences stay true.)

## New (Phase 4)

> 37. **The GI stores are passive; one Shading function does the cache step; accept values are hashed, not drawn (specs 032, 040).**
> `libshader::irradianceLookup` (`libshader/shading/irradianceCalls.cpp`) is the only place an `occlusion()`/`indirectdiffuse()`
> cache lookup, miss and insert are decided, for the interpreter (`IDEXPR`) and the JIT (`jitOcclusionBatch`). It copies the bound
> sub-stream (`boundCall`) at its top. `CIrradianceStore::lookupCached(C, P, N, maxError, accept, acceptState, batch, Pc, Nc)` calls
> `accept(acceptState, recordIdentity)` **once per record that passes the front test** (zero-weight records included) and Shading answers
> `giAcceptValue(boundCall.seedHash, boundCall.indexBase, identity)` (`common/giHash.h`), a pure function in `[0, 1)`: a record's accept
> decision depends on the call and the record alone, never on the traversal order or on the other records, and **the cursor is not
> touched**. The identity is the record's key or, for a record read from a file (key 0), a hash of the bit patterns of `P` and `N`
> (`irradianceRecordIdentity`). A miss runs the sampler, which draws **by index from a copy of the bound sub-stream** (`rayDraw2D(copy,
> pair, k)`): with the cache off both `occlusion()` and `indirectdiffuse()` take `samples` keyed directions (pair 0 sample `k`, origin
> jitter pair 1 sample `k`); with the cache on the `nt x np` grid stays (the gradients need it) and cell `k = i * np + j` takes its
> in-cell offsets from pair 0 sample `k`; so neither the number of records visited nor a nested `bindDraws` can shift a draw. A point-based
> lookup draws and hashes nothing; a name that `getCache` answered `nullptr` for gives the "no joy" value and draws nothing. The batch
> view of `lookupCached` is a Shading concept (the points one `shade()` call evaluates): a difference between hiders in what the cache
> holds comes from the batch size they shade at, never from hider-specific code in the store. (Batch scope: T079; commit and wire: T089.)

## New (Phase 5, appended to gotcha 37: the batch scope and the canonical commit)

> **The cache is a fixed part, a batch and a commit (spec 040, M2).** During a frame the committed tree `F` is read-only and a lookup takes no lock:
> `lookupCached` walks `F` in canonical order (the tree's traversal, each node's list in file order), then the **batch view**, the records the
> earlier lookups of the same `shade()` call made (`CShadingState::irradianceBatch`, `irradianceBatch.h`, reset with the draw occurrences: a REYES or zbuffer
> grid is one batch, a raytrace shading group is the rays of one bundle that hit one object, in ray order, chunked at `maxGridSize`). A miss with `CACHE_SAMPLE` makes
> its record from the point's identity alone (key = `irradianceRecordKey(boundDraws())` copied at the top), calls `addPending(record, maxError)` with the **raw**
> radius, and adds it to the batch with the radius clamped against `F` (`clampAgainstFixed`) and against the batch (`clampAgainstBatch`: `min(raw, min over F (dP + l),
> min over earlier batch records (raw + l))`, each earlier one lowered by `raw_new + l`). Nothing else sees a record until the frame's `commitPending()`
> (`CIrradianceCache`, single-threaded, run by the destructor before the write and lazily by `forEachNode`/`forEachRecord`): dedupe by a non-zero `key` (**key 0 is never
> deduped: every key-0 record is kept**), sort by `(key, bytes, maxError)`, clamp every radius from raw or stored radii only (so the result does not depend on the order), place each
> record by `dP / K` at the tail of its node (fixed records first), and **drop the key** (a committed record is what a file holds, so a cache read back answers
> bit for bit as the one that wrote it). `readNode` appends, so a file's lists keep their order. Reyes and zbuffer make the same records for a scene;
> the raytracer's differ only by its batch size (M5): the cache has no hider-specific code. `insertRecord` and `clamp()` are gone; `insertReceived` (a `-P:N`
> client's placement) is unchanged until Phase 6.

## New (Phase 6, appended to gotcha 37: the `-P:N` wire)

> **A `-P:N` server sends the records it made, never the file's (spec 040, D4).** `CRemoteICacheChannel::sendRemoteFrame` sends the cache's **pending set**
> (`forEachPending`: deduplicated, sorted by `(key, bytes)`, the **raw** `dP`): one group of `int n` and `n` raw 240-byte `CCacheSample`, then `int 0`. The 8-byte `next` slot of a
> record carries the maxError of the call that made it (a float, then four zero bytes), so no pointer value leaves the process; `recvRemoteFrame` hands each record to
> `receivePending(record, maxError)` and the client commits the whole set once with `commitPending()`, each record placed with **its own** `K = 0.4 / maxError`
> (GitHub #161: `insertReceived`, which placed by raw `dP` with no `K`, is gone). The client needs the same fixed part as its servers to write the whole cache: the
> channel request's type word carries `CHANNELFLAG_READ` in its low byte when the server opened the cache from its file, and the client then creates its side with
> `getCache(name, "rw")` and reads the file itself (a server finds a file by its own search path and otherwise downloads it from the client by name,
> `NET_SEND_FILE`, so the client is the origin of every file a server reads). Not handled: a server with a local cache file the client does not have.

---

# FINAL TEXT (T100, Phase 8): the edits to apply to `CLAUDE.md` at merge preparation

Checked against the worktree's `CLAUDE.md` (master snapshot of 2026-11-12 plus 031/032): every "Old" text below is present verbatim. The merge-time rule: the literal
`kRendererServicesVersion` is re-checked against master then (specs 033 and 057 hold 2.4.0 and 2.5.0; this spec's entry is 2.6.0 and is renumbered to the next free MINOR if
either has not merged). `CLAUDE.md` itself is not edited by the implementation (M6).

## Edit 1: gotcha 37 (replaces the whole gotcha, from "37. **The GI stores are passive" to "`-L gi_split_threads` is opt-in).")

New text:

> 37. **The GI stores are passive; one Shading function does the cache step; accept values are hashed, the cache is a fixed part, a batch and a commit (specs 032, 040).**
>    `libshader::irradianceLookup` (`libshader/shading/irradianceCalls.cpp`) is the only place an `occlusion()`/`indirectdiffuse()` cache lookup, miss and insert are decided,
>    for the interpreter (`IDEXPR`) and the JIT (`jitOcclusionBatch`). It copies the bound sub-stream (`boundCall`) at its top. `CIrradianceStore::lookupCached(C, P, N, maxError,
>    accept, acceptState, batch, Pc, Nc)` calls `accept(acceptState, recordIdentity)` **once per record that passes the front test** (zero-weight records included) and Shading
>    answers `giAcceptValue(boundCall.seedHash, boundCall.indexBase, identity)` (`common/giHash.h`), a pure function in `[0, 1)`: a record's accept decision depends on the call and the
>    record alone, never on the traversal order or on the other records, and **the cursor is not touched** (the old "the draw order is a rule" sentence is gone). The identity is the
>    record's key, or a hash of the bit patterns of `P` and `N` for a key-0 record (`irradianceRecordIdentity`). A miss runs the sampler, which draws **by index from a copy of the
>    bound sub-stream** (`rayDraw2D(copy, pair, k)`): with the cache off both `occlusion()` and `indirectdiffuse()` take `samples` keyed directions, with the cache on the `nt x np`
>    grid stays (the gradients need it) and cell `k = i * np + j` takes its offsets from pair 0 sample `k`, its origin jitter from pair 1 sample `k`. A point-based lookup draws
>    and hashes nothing; a name `getCache` answered `nullptr` for gives the "no joy" value. **During a frame the committed tree `F` is read-only and a lookup takes no lock**:
>    `lookupCached` walks `F`, then the **batch view** (`CShadingState::irradianceBatch`, `irradianceBatch.h`: the records the earlier lookups of the same `shade()` call made, with
>    radii clamped against `F` and each other; a REYES or zbuffer grid is one batch, a raytrace shading group is the rays of one bundle that hit one object, in ray order, chunked
>    at `maxGridSize`). A miss with `CACHE_SAMPLE` makes its record from the point's identity alone (key = `irradianceRecordKey(boundDraws())`, copied at the top), calls
>    `addPending(record, maxError)` (the raw radius) and adds it to the batch. Nothing else sees a record until `commitPending()` (`CIrradianceCache`, single-threaded, run by the
>    destructor before the write and lazily by `forEachNode`/`forEachRecord`): dedupe by a non-zero key (**key 0 is never deduped**), sort by `(key, bytes, maxError)`, clamp every radius
>    from raw or stored radii only, place by `dP / K` at the tail of the node (after the fixed records), and **drop the key** (a committed record is what a file holds, so a cache
>    read back answers bit for bit as the one that wrote it). `readNode` appends, so a file keeps its order. The cache has no hider-specific code: reyes and zbuffer make the same
>    records, the raytracer's differ only by its batch size (M5). A `-P:N` server sends **the records it made, never the file's**: one group of raw 240-byte `CCacheSample`
>    (`forEachPending`, raw `dP`, the `next` slot carrying the call's maxError and four zero bytes), the client calls `receivePending` and commits once with each record's own
>    `K` (GitHub #161: `insertReceived` is gone); the channel request's type word carries `CHANNELFLAG_READ` when the server read the file, so the client reads it too (a server
>    finds a file by its own search path and otherwise downloads it from the client, so the client is the origin of every file; not handled: a server-local cache file the client
>    lacks). The photon store: the test, reset and first store of a frame are one claim under its lock (`storePhotonForFrame`, GitHub #164; `CMap::reset()` uses `delete[]`, #165),
>    `balance()` sorts the photons into the canonical order (key, then 38 bytes of `P`, `N`, `C`, `theta`, `phi`; `flags` and the padding are not part of it), every byte of a
>    `.gpm` is defined (#163), and a lookup is a pure function of the map and the query (the `PHOTON_LOOKUP_CACHE` memo is gone, which is also #160; the cost is GitHub #178's
>    follow-up). `maxPower` is the largest `|C|`, in memory and in the file. The cache and the photon store never include Shading, Hiders, Core or the viewer (`GiSplit_Boundary`,
>    `-L architecture`); a world bound or world matrices they need are passed to their constructors. `CPhotonMap` is header-only in Core (`render/photonMap.h`): `libRIBVector`
>    constructs it, and a `render/photonMap.cpp` would grow the residual list, which may only shrink. Keys are in memory only (the wire record carries one, no file does):
>    `CIrradianceRecord::key` (`CCacheSample` is 240 bytes on the wire, the key at offset 224) and a photon's key (`CPhotonStore::photonKey(i)`, a side array that `CMap::permuted()`
>    reorders when `balance()` runs). `GiRemote_*`, `GiFiles_Unchanged`, `GiKeys_*`, `GiCache_KindMismatch`, `GiDet_*` and `TextureOnly_GiStores` guard it (`ctest -L gi_split`,
>    `-L '^gi_determinism$'`; `-L gi_split_threads` and `-L '^gi_determinism_long$'` are opt-in). Under the interpreter `indirectdiffuse()` traces no ray (GitHub #173: the sampler
>    receives a denormal `samples`); the `.slo` shaders do. Cost: a cache reused at a fine shading rate costs about twice as much per later frame (the cache is 8 times as dense:
>    GitHub #179). Untouched: #158 (`jitOcclusionBatch`), #159, #166, #122.

## Edit 2: gotcha 28's version sentence

Old (the end of the sentence that starts "`getDSO` also serves the `.slo` loader"): "... It is 2.3.0 since spec 031 (a MINOR step that is breaking within 2.x: ... and `EShadingDim` moved to `common/shadingDim.h`)."

New: the same, followed by: " It is 2.6.0 since spec 040 (a MINOR step that is breaking within 2.x: `IPhotonStore`'s `resetPhotons`, `balancePhotons`, `isModifying`, `setModifying` and `storePhoton` are replaced by `storePhotonForFrame` and `writePhotons`; `CIrradianceStore::lookupCached` takes an accept function and a batch view in place of a draw callback, `insertRecord` is replaced by `addPending` and `clampAgainstFixed`)."

## Edit 3: the "GI stores (spec 032)" architecture paragraph

Old: "... and `CPhotonStore` (`photonStore.{h,cpp}`) hold records, answer lookups, write and read their files, and name no shading context, hider, renderer or data viewer." and its last sentence "Every record and photon carries a key for spec 040. See gotcha 37."

New (replace the last sentence): "Since spec 040 the cache is a read-only fixed part, a Shading batch scope and a canonical end-of-frame commit, a lookup of the photon map is a pure function, and both stores are byte-identical across runs, thread counts and `-P:N` (`common/giHash.h` supplies the hashes). See gotcha 37."

## Edit 4: the Testing block

Add after the `gi_split_threads` line:
```
ctest --test-dir build -L '^gi_determinism$' -j4         # spec 040: the photon store, the cache store, the accept rule, a miss's record, the batch clamp, the files (20 tests)
ctest --test-dir build -L '^gi_determinism_long$' -j2    # opt-in: the 30-run photon forms (3 tests)
```
Replace the everyday line: `ctest --test-dir build -LE 'determinism-sweep|gi_determinism_long' -j4   # the everyday full run: everything except the opt-in determinism sweep and the 30-run forms`.
Replace the sweep lines' counts: "default determinism set (156)" becomes "(164)", and "opt-in full sweep (582 entries" becomes "(616 entries".

## Edit 5: the Active Technologies bullet (after the 031 bullets)

```
- C++20, CMake ≥3.19, `-Werror`; no new dependencies: the photon store and the irradiance cache are deterministic (a claim under the store's lock, a canonical photon order, no lookup memo; a
  read-only fixed part, a Shading batch scope, a canonical commit, a pending-only `-P:N` wire), the accept value is a hash (`common/giHash.h`) and the sampler draws by index from a copy of the
  bound sub-stream; `kRendererServicesVersion` 2.6.0 (040-gi-determinism)
- Storage: N/A; memory only; the `tests/gi_split/reference` and `tests/gi_determinism` recordings are files (040-gi-determinism)
```

## Edit 6: Recent Changes (above 056's entry; the list is newest first)

```
- 040-gi-determinism: the photon map and the irradiance cache are deterministic: the same files and images across runs, at any thread count and under `-P:N`. Photon store: the claim and reset inside the
  store's lock (the intermittent `CPhotonHider` crash, #164), `delete[]` for `new[]` (#165), a canonical photon order and every byte of a `.gpm` defined (#163), no lookup memo (#160; the cost is #178's
  follow-up). Irradiance cache: the accept value is a hash of the call and the record, the sampler draws by index (`indirectdiffuse()` with the cache off now takes `samples` keyed directions), the cache
  is a read-only fixed part plus a Shading batch scope plus a canonical commit, and a `-P:N` server sends only the records it made, committed with each record's own K (#161). The thread pins of the GI
  scenes are removed, 34 GI sweep entries join the determinism sweep, the GI references were re-rendered once (the four images, the cache files, the loopback, the data scene, the trace replays, spec 039's
  occlusion golden). `kRendererServicesVersion` 2.6.0. Under the interpreter `indirectdiffuse()` traces no ray (#173); the M2 cache is 8 times as dense, so a cache reused at a fine shading rate costs
  about 2x per later frame (#179). No threshold widened; thresholds only lowered (see spec 040).
```
