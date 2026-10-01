# Data Model: Deferred, On-Demand Evaluation of Solid (CSG) Geometry

**Feature**: 021-deferred-csg-evaluation | **Date**: 2026-09-30 | **Research**: [research.md](research.md)

Names below are proposed. The final C++ names follow the existing `C`-prefix
convention. All geometry is in camera space, the renderer's "world" space.

## Entities

### CSG capture node (`CSGTreeNode`, existing, trimmed)

A build-time node recorded while `SolidBegin`/`SolidEnd` blocks are open.

| Field | Meaning |
|---|---|
| `operation` | `CSG_PRIMITIVE`, `CSG_UNION`, `CSG_INTERSECTION` or `CSG_DIFFERENCE` |
| `parent` | Enclosing node |
| `operands` | Nested nodes, in declaration order |
| `leafObjects` | Primitives (including unexpanded procedurals) captured directly in this node |
| `outerXform` | Transform snapshot at the outermost `SolidBegin` |

**Validation**: the existing diagnostics are unchanged: bad operation name,
unmatched or improperly nested blocks, nesting inside `"primitive"`. The
procedural rejection is removed (FR-024).

**Lifetime**: discarded once converted into a CSG definition at the outermost
`SolidEnd`.

### CSG definition — `CCSGDefinition` (new; immutable, reference-counted)

The persistent, shared description of one solid tree. Built once at the
outermost `SolidEnd`; shared by every instance (FR-001, FR-017).

| Field | Meaning |
|---|---|
| `nodes[]` | Flattened tree: `{operation, childIndices[], operandIndex or -1}` |
| `operands[]` | One entry per operand: **template** member objects only (surfaces, mesh-type objects, unexpanded procedurals) and template bound. Each template leaf's xform is **rebased** into the outer frame (`outerXform⁻¹ · leafXform`), because captured leaves carry absolute transforms. Primitives placed directly in a boolean block form one operand, positioned first. Blocks nested inside `"primitive"` contribute no geometry |
| `leafFlip[]` | Per operand: static orientation parity (odd number of subtrahend positions ⇒ flipped) |
| `bound` | CSG-aware template bound in the outer frame's local space (each leaf's camera-space bound transformed by `outerXform⁻¹`): union = ∪ of operand bounds; intersection = ∩; difference = first operand's bound |

**Validation**:
- An empty tree yields no solid (spec 013 FR-016).
- An empty nested block stays in its position as an empty operand, the empty set
  (FR-025a). Spec 013's resolver dropped it instead (`csgTree.cpp:511`).
- A single-operand boolean node collapses to that operand, passthrough with no
  predicate (spec 013 FR-017). When the **whole** solid collapses to one operand,
  it renders as authored, with no implied caps (FR-019); a single-operand node
  nested in a boolean still takes part in it.
- The operand **kind** (single-quadric / multi-surface / empty / open) is **not**
  stored here. It is derived per instance after procedural expansion and mesh
  materialization (`CCSGInstanceState`). The definition provides the
  closedness helper (FR-021) used for that.

### CSG solid — `CCSGSolid` (new; replaces `CSolidObject`, a `CObject`)

The renderable handle registered with the scene and BVH, one per placement or
instance.

| Field | Meaning |
|---|---|
| `definition` | Shared CSG definition (attached / reference-counted) |
| `xform`, `attributes` | This placement's transform and attributes |
| `bmin`, `bmax` | The definition's local template bound transformed by this solid's `xform` |
| `state` | Per-instance state, or null until first use |
| `flags` | Not `OBJECT_DUMMY`, so `trace.cpp:332` still calls `intersect()` |

**Behavior**:
- `dice(CReyes*)` resolves the state, sets `children` to all instance leaves
  (mesh-type objects, surfaces, caps), then calls `CObject::dice(r)`
  (`geometryDispatch.cpp:76`). Each leaf is thereby clipped and queued per
  bucket via `drawObject`.
- `intersect()` resolves the state and populates `children`.
- `instantiate()` creates a new CSG solid over the same definition, with a
  composed transform.

### Per-instance state — `CCSGInstanceState` (new)

Built on first `dice()`/`intersect()` under `CRenderer::delayedMutex`, at most
once per instance.

| Field | Meaning |
|---|---|
| `operandMembers[op][]` | This instance's leaf objects per operand: templates instantiated with a flagged attribute clone and the composed transform, plus procedural expansions. Captured through the context's `delayed` target, because `instantiate()` hands its output to `addObject()` |
| `resolved[op]` | A `CCSGResolvedOperand { members, surfaces, kind, bound }`: the operand's members (declared primitives, minus unsupported members per research Decision 3), every `CSurface` descendant of them after depth-first `materialize(ctx)` (the classification surfaces for parity), and the kind (single-quadric / multi-surface / empty / open) from the pure `deriveOperandKind(members, surfaces)`. Closedness is judged per member, because a mesh splits into many surfaces. An empty operand classifies every point OUT (FR-025a). Unit tests build these directly |
| `caps[op][]` | Synthesized cap surfaces, for operands resolved as single-quadric (FR-021) |
| `regions[]` | One `CCSGRegion` per leaf and cap (below) |
| `operandBounds[]` | Per-operand bounds for the uniform-patch shortcut |

**State transitions**: Unresolved → Resolving → Resolved.
- **Resolving** (lock held), in this order (research Decision 9):
  0. unsupported members (`honorsRegion()` false: implicit, DSO, points,
     curves, curve meshes, object instances; or not a surface, mesh or
     procedural)
     dropped, with one warning per offending operand per frame (FR-018)
  1. capture target set (`delayed` → per-operand holder)
  2. each template leaf instantiated with a clone of its attributes carrying
     `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` and the `leafFlip` toggle of
     `ATTRIBUTES_FLAGS_INSIDE`, and with the solid's xform (the leaves were
     rebased, so this reproduces their absolute placement); then every captured
     `CDelayedObject` replaced by its expansion through `expandIntoOperand()`,
     with a flagged clone as the current attributes, until none remains
  3. `region` set on every member
  4. depth-first materialization
  5. classification surfaces collected
  6. kinds derived, with the "not closed" warning once per offending operand per
     frame
  7. caps synthesized; the members of empty or open operands left out of
     `children` (they never render); and, if a single non-empty operand remains
     once empty subtrees are removed (emptiness propagates bottom-up), the
     instance is a passthrough with no caps, and the region already set on its
     members and their descendants is cleared (FR-019)

  Mesh children copy `attributes` and `region` when `create()` runs, so steps 2
  and 3 must precede step 4.
- **Resolved**: immutable, read concurrently by all threads.

### Region predicate — `CRegionPredicate` (new generic interface; CSG is its only implementer)

Attached to any `CObject` through the nullable `CObject::region` pointer. It is
a generic "spatial trim", not a CSG concept. `CObject::setChildren()` copies a
parent's `region` to children that have none, so the surfaces a mesh-type
object creates lazily inherit it (research Decision 15).

| Operation | Meaning |
|---|---|
| `accepts(ctx, P, Ng)` | Does this surface point belong to the rendered surface? `ctx` is the caller's `CShadingContext`; `Ng` is the accepting surface's undisplaced geometric normal at P in camera space, used only as the transverse axis of the two-sided ON rule (P ± ε·N̂g; sign irrelevant). The ON check counts every other surface within ε, including the other surfaces of the leaf's own operand. Among coincident leaves that all report a boundary, only the lowest-indexed operand's leaf accepts |
| `classifyBound(ctx, bmin, bmax)` | `ALL_IN`, `ALL_OUT` or `MIXED`, for a surface patch within this bound |

**Region support**: `virtual int CObject::honorsRegion() const` (default `TRUE`).
`CImplicit`, `CDLObject`, `CPoints`, `CCurve`, `CCurveMesh` and `CDelayedInstance`
return `FALSE`: their own rendering bypasses every trimming hook, or they
enclose no volume, so inside a solid they are dropped (FR-018).

**Mesh materialization**: `virtual void CObject::materialize(CShadingContext*)`
(default no-op). The five mesh types override it to create their children
(idempotent and mutex-guarded). Instance resolution calls it depth-first, which
reaches the nested `CPolygonMesh` that a `CLoopSubdivMesh` creates.

**Coverage hook**: `virtual void CObject::regionCoverage(CShadingContext*, int
udiv, int vdiv, unsigned char *coverage)` (default no-op in `object.cpp`;
`CSurface` override in the renderer-only `render/geometryDispatch.cpp`, with a
no-op stub in `vector/geometryDispatchStub.cpp`) fills a grid's coverage mask from **undisplaced** camera-space P at
the grid's u/v.

**The CSG region adapter, `CCSGRegion`**, holds (instance state, operand index, leaf index) and
implements `accepts` by the boundary rule (research Decision 4). For
coincident-face ON cases it uses a two-sided evaluation at P ± ε·N.

**Invariants**:
- `accepts` is a pure function of P: deterministic across threads, buckets and
  query order (FR-008).
- A null region pointer means the surface behaves exactly as today (FR-020).

### Per-surface crossing query (new `CSurface` capability)

| Operation | Meaning |
|---|---|
| `countCrossings(ctx, from, dir) → int` | **Unsigned** number of crossings of the *undisplaced* surface along the ray. Operands classify by parity (odd ⇒ IN), independent of how the author oriented each surface (research Decision 3) |

This query ignores every rendering-side ray filter (visibility flags, LOD,
backface, displacement). Implementations:
- quadrics: analytic, all roots in the u/v range (NULL `ctx` allowed)
- flat polygons: exact (NULL `ctx` allowed)
- bilinear patches: analytic (NULL `ctx` allowed)
- free-form surfaces (`CBicubicPatch`, `CNURBSPatch`, `CPatchGrid`,
  `CBSplinePatchGrid`, `CSubdivision`): forward to the renderer-only
  `surfaceCrossingDispatch`, a pure Newton core seeded from an immutable
  per-surface evaluation grid. The ribVector build gets a stub.
- default (other `CSurface` types): 0, so they contribute nothing

### Coverage mask (new generic `CRasterGrid` field)

| Field | Meaning |
|---|---|
| `coverage` | Per-micropolygon flag (null = all present). A quad is absent iff none of its 4 vertices, taken undisplaced, is accepted by the object's region predicate |

Filled once by `CObject::regionCoverage()`, called from the P-only branch of
`CReyes::shadeGrid()` before `displace()`, and kept with the grid through its
later full shading. Consumed only where per-quad `bounds` are computed: an absent
quad gets the bound `{INT_MAX, INT_MIN, INT_MAX, INT_MIN}`, written both into
`bounds[]` and into the quad's per-time-stratum entries of
`grid->quadStratumBounds` (the motion-blurred xtreme path culls by those alone),
so the rasterizers' existing trivial rejects skip it. Rasterizer loops are
unchanged (FR-012).

### Statistics (`CStats`, extended)

`atomic_int32` counters `numCsgClassifications`, `numCsgSeamSplits`, `numCsgInstancesResolved`,
`numCsgProceduralExpansions` (SC-006, FR-003, FR-004).

They are printed at statistics level ≥ 2, zeros included, whenever the frame
contains a solid (a per-frame flag, reset at `RiWorldBegin`, set by every outermost `RiSolidEnd` inside
the world block, single-operand collapses included, and by every
`CCSGSolid::instantiate()`), with the labels `CSG classifications:`, `CSG seam splits:`,
`CSG instances resolved:` and `CSG procedural expansions:`.

## Relationships

```text
CSGTreeNode (capture, transient) ──SolidEnd──▶ CSG definition (shared, immutable)
                                                   ▲ 1
                                                   │ n
                     CSG solid (per placement/instance, CObject in scene + BVH)
                                                   │ 1
                                                   ▼ 0..1 (built on first use)
                                        Per-instance state
                                         ├── operandMembers ──▶ CSurface leaves ──▶ CCSGRegion
                                         └── caps ────────────▶ CSurface caps   ──▶ CCSGRegion
```

## Rules carried from the spec

| Rule | Where enforced |
|---|---|
| Composite surface only where and when needed (FR-002, FR-004) | Per-instance state built on first use; bucket and ray driven dicing and intersection |
| Seam accuracy tracks resolution (FR-007, FR-009) | Straddle split terminated by screen size, plus coverage mask |
| No CSG logic in hiders (FR-012) | Generic region predicate on `CObject` (inherited via `setChildren`); generic coverage mask on `CRasterGrid` |
| Every operand type keeps working (FR-018) | Mesh-type leaves are materialized, and their surfaces inherit the region via `setChildren` (research Decision 15) |
| Procedural operands behave as if written inline (FR-024) | Operand kind and caps are derived after expansion, per instance |
| Open operands (FR-021) | Closedness test per instance; single open quadric ⇒ implied closure and caps; lone open non-quadric ⇒ empty plus one warning per operand per frame |
| Single-operand solids (FR-019) | Whole-solid collapse is a passthrough: no region, no caps |
| Untrimmable operands (FR-018) | Unsupported members dropped with a deduplicated warning |
| Operands that contribute nothing (FR-021, FR-025a) | Members of empty or open operands never render; the coincident-face check counts the leaf's own operand's other surfaces |
| Empty operands (FR-025a) | Always OUT, so the tree gives the empty-set semantics; empty bound for culling; empty nested blocks kept in position |
| Faces keep operand shaders (FR-016) | Leaves are the operands' own surfaces with their own (cloned) attributes |
| Subtracted walls face outward (FR-011) | Static flip applied as `ATTRIBUTES_FLAGS_INSIDE` on cloned attributes |
| Interior/Exterior (spec 013 FR-009 to FR-012) | `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` set on each leaf's cloned attributes; `shading.cpp:1019` unchanged |
| Tolerance setting inert, with notice (FR-023) | Attribute still parsed; value unread; once-per-frame warning |
