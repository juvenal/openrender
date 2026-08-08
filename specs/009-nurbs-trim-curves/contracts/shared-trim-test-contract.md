# Contract: Shared Trim Test

Internal module-boundary contract between mesh construction (producer) and the two tessellation paths (consumers).
This is the single seam FR-010/FR-012 require every hider to go through — the contract exists precisely so no
hider-specific trim implementation can be added later without visibly breaking this shared interface.

## Producer: `CNURBSPatchMesh::create()`

- **File**: `src/ri/patches.cpp:1823-1891`
- **Input**: `CAttributes::pendingTrimLoops` (may be absent), `CAttributes::trimSense`, snapshotted at the moment a
  `NuPatch` is issued (existing call site already reads `CAttributes` here to build the mesh itself).
- **Behavior**:
  1. If `pendingTrimLoops` is absent → mesh's `trimTest` stays absent (`nullptr`). No further work. This is the
     path FR-004 requires to be byte-for-byte identical to pre-feature behavior.
  2. Otherwise, for each Trim Loop: validate weights (`w > 0` for every control point — reject the whole loop and
     warn once if violated, FR-019) and closure (implicitly close and warn once if the loop isn't already
     head-to-tail closed, FR-017), per R6's mesh-identity-based dedup (validation runs exactly once per mesh
     regardless of `ObjectInstance` reference count, satisfying FR-020 without extra bookkeeping).
  3. Flatten each surviving loop's curves into one polyline in `(u,v)` space, amortized once (FR-018).
  4. Store the resulting polyline set + `trimSense` snapshot as the mesh's owned `trimTest`.
- **Output**: `CNURBSPatchMesh::trimTest` — absent, or a built, immutable-for-the-render classification object.
- **Ownership**: Owned by the mesh; every per-Bezier-span child (`patches.cpp:1874`) references the parent mesh's
  `trimTest`, never builds or owns its own copy (FR-009).

## Consumers

Both consumers MUST call the identical classification entry point — no hider-specific variant is permitted
(FR-010, FR-012).

### `CPatch::dice()`

- **File**: `src/ri/surface.cpp:141+`
- **Call site**: the per-vertex probe / grid-fill stage, after existing `varying[VARIABLE_U/V/TIME]` values are
  available for a sampled point.
- **Contract**: for each sampled `(u, v)`, if the owning mesh's `trimTest` is absent, proceed exactly as today
  (FR-004). Otherwise, call the shared classification function; a "not retained" result excludes that vertex from
  the diced grid the same way an existing out-of-bounds/degenerate vertex is excluded — no new grid data structure
  is introduced, only a new exclusion predicate.
- **Consumers via this path**: reyes-family and z-buffer hiders (both built on `CReyes`/`CStochastic`/`CZbuffer`,
  which all consume `CPatch::dice()`'s output).

### `CTesselationPatch`

- **File**: `src/ri/surface.cpp` (ctor `:552`, `intersect()` `:635`, `splitToChildren()` `:1914`,
  `tesselate()` `:1450`), declared `src/ri/surface.h:61`
- **Contract**: same classification function, same "not retained → excluded" semantics, applied during on-demand
  tessellation for ray-surface intersection. `CTesselationPatch` does not share `CPatch::dice()`'s grid — it must
  call the same underlying classification function directly (not `dice()` itself), since its own
  `tesselationList` (`surface.cpp:539`) is a separate on-demand structure.
- **Consumers via this path**: ray-tracing hider (`CRaytracer`), and by extension any hider built on the same
  on-demand tessellation (photon-mapping, debug/visualization), per FR-012's "no hider-specific fallback" wording.

## Invariant this contract protects

Both consumers call the same classification function against the same mesh-owned `trimTest`. If a future change
needs hider-specific trim behavior, it must go through this contract's producer/consumer boundary (e.g. by changing
what `trimTest` contains), not by adding a second classification path — that would violate FR-010/FR-012 and is
exactly the outcome this explicit contract exists to make visible in review.

## Forward-looking note (documentation only)

See `data-model.md`'s Shared Trim Test entity for the GPU (R3) and CSG (R4) extension-seam notes. Neither is part
of this contract's current surface — this contract intentionally has one producer and two consumers, no more.
