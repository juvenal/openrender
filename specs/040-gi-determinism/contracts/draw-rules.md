# Contract: the draw rules of the irradiance cache step (FR-010, FR-011, FR-012; gotcha 37)

This replaces the "draw order is a rule" paragraph of gotcha 37 **only if the maintainer takes decision D1**. Until then it is the
proposed text.

## Gotcha 37 today

`CIrradianceStore::lookupCached` calls its draw callback once per record that passes the front test (zero-weight records
included, in traversal order), then a miss runs the sampler on the same cursor. A point-based lookup draws nothing; a name that
`getCache` answered `nullptr` for draws nothing.

## Proposed rule (D1-A, recommended)

1. `irradianceLookup` copies the bound sub-stream key at its top (`boundCall`, as today for the record key).
2. **Accept values are hashed, not drawn.** For each record that passes the front test, the store calls
   `accept(acceptState, recordIdentity)`; Shading answers `u = giAcceptValue(boundCall.seedHash, boundCall.indexBase, recordIdentity)`, a pure function in `[0, 1)`. `giAcceptValue` is a small public Infra helper in the **new** header `src/common/giHash.h` (it wraps `sampling::detail::mix` and `toUnitFloat(uint32_t)`, which keeps the top 24 bits, as the sampler's float conversion does; it does not edit spec 039's approved `sampling.h`, and neither Shading nor Texturing names `sampling::detail`).
   `recordIdentity` is the record's key, or a hash of its `P` and `N` bit patterns when the key is 0 (records read from a file).
   The cursor is not touched.
3. **The sampler draws by index from a copy of `boundCall`**: pair 0 sample `k` for the direction (or the in-cell offsets of grid
   cell `k`), pair 1 sample `k` for the origin jitter (data-model.md §7). The number of records visited or accepted changes no value
   the sampler draws, and no nested `bindDraws` during the sampler's tracing can change them.
4. Unchanged: a point-based lookup draws and hashes nothing; a `nullptr` cache gives the no-joy value and draws nothing; a cache
   with no `CACHE_SAMPLE` uses `smallSampleWeight` 0, so the accept value is never needed (it may be computed or not: no draw is
   consumed either way).

Consequences: the accept decision of a record for a query is the same on every thread and in every traversal order, and does not
depend on the other records (FR-010); the record a miss creates depends only on the point (FR-011). Every cached GI image changes
once (FR-018).

## Alternative (D1-B, not recommended)

Keep the cursor draw in traversal order. Under the batch-scoped visibility the traversal order is itself deterministic, so images
would be deterministic; but the accept value of a record would still depend on how many records passed the front test before it,
which FR-010 forbids as written, and the rule would remain a hazard for any later change of the tree's shape.

## Sampler change independent of D1 (FR-011, FR-012)

Moving the sampler to indexed draws is required by FR-011 whichever D1 option is taken, and moving `indirectdiffuse()` with the cache
off to `samples` keyed directions is FR-012 (spec 039's T076c gap). Both backends reach the one sampler through `irradianceLookup`,
so `.slo` and `.rslo` change together.
