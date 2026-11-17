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
