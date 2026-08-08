# Phase 1 Data Model: NURBS Trim Curves (RiTrimCurve)

Entities below extend spec.md's Key Entities section with concrete field/type definitions and ownership, grounded in
the existing `CAttributes` (`src/ri/attributes.h`) / `CNURBSPatchMesh` (`src/ri/patches.h`) classes. No entity here
is a new top-level class unless stated — most are new fields on existing classes, per the additive-only constraint
(FR-004) and the four-layer attribute pattern (FR-007).

## Trim Loop

One or more homogeneous rational B-spline curves in `(u,v,w)` parameter space, connected head-to-tail into a single
closed boundary.

| Field | Type | Notes |
|---|---|---|
| `curveCount` | `int` | Number of curves composing this loop (`ncurves[i]` from `RiTrimCurve`) |
| `order[]` | `int[curveCount]` | B-spline order per curve |
| `knot[]` | `double[]` | Concatenated knot vectors, one run per curve |
| `min[]`, `max[]` | `double[curveCount]` | Parameter-range clamp per curve (per RISpec Appendix C.1's array form, not a single scalar pair) |
| `n[]` | `int[curveCount]` | Control-point count per curve |
| `u[]`, `v[]`, `w[]` | `double[]` | Concatenated homogeneous control points `(u, v, w)` across all curves in the loop |

**Validation rules** (applied once, at `CNURBSPatchMesh::create()` time — see R5):
- Every control point's `w` MUST be `> 0`; a loop containing any `w <= 0` control point is rejected in its entirety
  (FR-019) and excluded from the built Shared Trim Test.
- A loop whose curves are not already head-to-tail closed is implicitly closed by connecting its last flattened
  point back to its first (FR-017), and is otherwise still usable.
- Both rejection and implicit closure emit a diagnostic warning, deduplicated per R6 (see Shared Trim Test entity).

## Trim Curve Attribute State (new `CAttributes` field)

The renderer's current, possibly-empty set of trim loops for the active attribute scope — pending state produced by
`RiTrimCurve` and consumed by the next `RiNuPatchV` in scope.

| Field (on `CAttributes`) | Type | Notes |
|---|---|---|
| `pendingTrimLoops` | heap-owned array of Trim Loop (or `nullptr`) | Set by `RiTrimCurve`; read and left in place by `RiNuPatchV` (the pending state itself is attribute-scoped, not primitive-scoped, so it is NOT cleared by consumption — only by a subsequent `TrimCurve` call or `AttributeEnd`, per User Story 2) |
| `trimSense` | enum `{ Inside (default), Outside }` | Backing storage for `Attribute "trimcurve" "sense"` |

**State transitions**:
- `RiTrimCurve` with ≥1 loop → replaces `pendingTrimLoops` in the current scope (FR-003).
- `RiTrimCurve` with `ncurves` of length zero → clears `pendingTrimLoops` in the current scope (FR-003, User Story 2
  Acceptance Scenario 3).
- `AttributeBegin` → deep-copies `pendingTrimLoops`/`trimSense` into the new scope (existing `CAttributes` copy-ctor
  pattern, `attributes.cpp:156-211`); `AttributeEnd` → restores the enclosing scope's values, discarding the
  nested scope's copy (existing destructor pattern, `attributes.cpp:219-264`). **Both sites MUST be updated for the
  new heap-owned `pendingTrimLoops` field** — a missed deep-copy/free here is a use-after-free on `AttributeEnd`
  (FR-013), the same hazard already documented for every other heap-owned attribute field in this class.
- `RiNuPatchV` reads `pendingTrimLoops`/`trimSense` at mesh-construction time and passes them to
  `CNURBSPatchMesh::create()`; it does not mutate or clear attribute state (User Story 2 Acceptance Scenario 2:
  consecutive `NuPatch` calls with no intervening `TrimCurve`/attribute-scope change all see the same trim).

## Trim Sense

The `"trimcurve"/"sense"` attribute value determining whether the loop-enclosed region is discarded or kept.

| Value | RIB string | Effect |
|---|---|---|
| `Inside` (default) | `"inside"` | Region enclosed by trim loops is discarded (cut away) |
| `Outside` | `"outside"` | Region enclosed by trim loops is kept; everything outside is discarded |

Implemented via the existing four-layer pattern (FR-007): token constant (`ri.h`/`ri.cpp`), `RiAttributeV` parsing
(`rendererContext.cpp`, modeled on the `RI_SHADERFORMAT` block at `rendererContext.cpp:3336-3338`), `CAttributes`
storage/query (`CAttributes::find()`, modeled on `attributes.cpp:651-655`), and mandatory pre-declaration in
`initDeclarations()` (modeled on `rendererDeclarations.cpp:179`). See `contracts/attribute-contract.md`.

## Retained Region

Not a stored entity — a derived concept: the portion of a `NuPatch`'s `(u,v)` domain for which the Shared Trim Test
classifies a sampled point as "kept" (per the current `trimSense`). Computed on demand per sampled vertex; nothing
persists beyond the classification result used inline by `CPatch::dice()`/`CTesselationPatch`.

## Shared Trim Test (new field on `CNURBSPatchMesh`)

A single, hider-agnostic `(u,v)` inside/outside classification, owned by the mesh (spanning its full global knot
range) and consulted identically by every tessellation/sampling code path.

| Field (on `CNURBSPatchMesh`, `patches.h:167-186`) | Type | Notes |
|---|---|---|
| `trimTest` | owned object (or absent/`nullptr` when no trim state was set) | Built once in `CNURBSPatchMesh::create()` (`patches.cpp:1823-1891`) from the mesh's `pendingTrimLoops` snapshot; referenced (not copied) by every per-Bezier-span child constructed at `patches.cpp:1874` |
| `trimTest.loopPolylines[]` | array of flattened polylines, one per surviving (non-rejected) Trim Loop | Contiguous POD point arrays in `(u,v)` space (R2) — each loop's curves flattened once, amortized per FR-018 |
| `trimTest.sense` | `Inside` \| `Outside` | Snapshot of `CAttributes::trimSense` at mesh-construction time |
| `trimTest.warnedAlready` | implicit — see below | The mesh instance itself is the dedup key (R6): validation/warning emission happens exactly once, inline, during the single `create()` call that builds `trimTest`; no separate per-instance or per-render warning state is needed |

**Classification** (consulted from `CPatch::dice()`'s per-vertex probe loop, `surface.cpp:141+`, and from
`CTesselationPatch`'s `tesselate()`/`splitToChildren()`, `surface.cpp:1450,1914`):

1. If `trimTest` is absent (no `TrimCurve` was ever declared for this mesh) → skip entirely, single cheap
   pointer/flag check, no further work (FR-004).
2. Otherwise, for a sampled `(u, v)`: cast a ray in the parameter plane and count crossings against every polyline
   in `trimTest.loopPolylines[]` — an O(total polyline edges) test (FR-018). Odd crossing count → "enclosed by trim
   loops"; even → "not enclosed" (FR-005, odd-crossing-count rule authoritative over curve-orientation for
   malformed loops).
3. Combine with `trimTest.sense`: `Inside` sense discards enclosed points, `Outside` sense keeps only enclosed
   points (FR-006).
4. Multiple loops (User Story 5) compose naturally under the same odd-crossing-count rule — no special-casing for
   disjoint holes vs. nested islands is needed; the ray simply crosses more polyline segments.

**Forward-looking extension seams (documentation only — not built in this feature)**:
- **GPU** (R3): this classification step is the one place every hider's sampled vertices already funnel through
  before shading. A future GPU-backed batch classifier would intercept here — operating on many surfaces'
  `loopPolylines[]` and vertex grids at once — rather than needing a new interception point per hider.
- **CSG** (R4): a future Constructive Solid Geometry boolean-classification test needs the same property this test
  already has — one hider-agnostic classification consulted identically by both `CPatch::dice()` and
  `CTesselationPatch` before tessellated vertices are produced. This entity is the precedent to reuse, not
  generalize preemptively.

## Untrimmed-NuPatch Regression Baseline

Not a runtime entity — a test artifact. See `research.md` R7 and `quickstart.md`.

| Artifact | Path |
|---|---|
| Baseline scene | `examples/rib/tests/nupatch-vase-untrimmed.rib` |
| Baseline reference image | `examples/rib/tests/references/nupatch-vase-untrimmed.tif` (captured on unmodified `master`, before any trim code lands) |
| ctest registration | `tests/visual/CMakeLists.txt`, new `add_visual_test(...)` entry alongside the existing ~85 |
