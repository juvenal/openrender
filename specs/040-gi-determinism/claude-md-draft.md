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
