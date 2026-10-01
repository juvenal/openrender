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

### CSG definition (new; immutable, reference-counted)

The persistent, shared description of one solid tree. Built once at the
outermost `SolidEnd`; shared by every instance (FR-001, FR-017).

| Field | Meaning |
|---|---|
| `nodes[]` | Flattened tree: `{operation, childIndices[], operandIndex or -1}` |
| `operands[]` | One entry per operand: template member objects, kind (single-quadric / multi-surface / has-procedurals), and bound |
| `leafFlip[]` | Per operand: static orientation parity (odd number of subtrahend positions ⇒ flipped) |
| `bound` | CSG-aware bound: union = ∪ of operand bounds; intersection = ∩; difference = first operand's bound |

**Validation**:
- An empty tree yields no solid (spec 013 FR-016).
- A single-operand boolean node collapses to that operand, passthrough with no
  predicate (spec 013 FR-017).
- An operand that is a lone zero-volume surface is empty.
- A lone open free-form operand emits a one-time "not closed" diagnostic.

### CSG solid (new; replaces `CSolidObject`, a `CObject`)

The renderable handle registered with the scene and BVH, one per placement or
instance.

| Field | Meaning |
|---|---|
| `definition` | Shared CSG definition (attached / reference-counted) |
| `xform`, `attributes` | This placement's transform and attributes |
| `bmin`, `bmax` | Definition bound under `xform` |
| `state` | Per-instance state, or null until first use |
| `flags` | Not `OBJECT_DUMMY`, so `trace.cpp:332` still calls `intersect()` |

**Behavior**:
- `dice(CReyes*)` resolves the state, then dices each leaf and cap.
- `intersect()` resolves the state and populates `children`.
- `instantiate()` creates a new CSG solid over the same definition, with a
  composed transform.

### Per-instance state (new)

Built on first `dice()`/`intersect()` under `CRenderer::delayedMutex`, at most
once per instance.

| Field | Meaning |
|---|---|
| `operandMembers[op][]` | This instance's leaf surfaces per operand: templates instantiated with the composed transform, plus procedural expansions |
| `caps[op][]` | Synthesized cap surfaces, single-quadric operands only (FR-021) |
| `regions[]` | One region adapter per leaf and cap (below) |
| `operandBounds[]` | Per-operand bounds for the uniform-patch shortcut |

**State transitions**: Unresolved → Resolving (lock held; procedurals expanded,
leaves instantiated, caps synthesized, flip applied to cloned attributes) →
Resolved (immutable, read concurrently by all threads).

### Region predicate (new generic interface; CSG is its only implementer)

Attached to a `CSurface` through a nullable pointer. It is a generic "spatial
trim", not a CSG concept.

| Operation | Meaning |
|---|---|
| `accepts(P)` | Does this surface point belong to the rendered surface? |
| `classifyBound(bmin, bmax)` | `ALL_IN`, `ALL_OUT` or `MIXED`, for a surface patch within this bound |

**The CSG region adapter** holds (instance state, operand index, leaf index) and
implements `accepts` by the boundary rule (research Decision 4). For
coincident-face ON cases it uses a two-sided evaluation at P ± ε·N.

**Invariants**:
- `accepts` is a pure function of P: deterministic across threads, buckets and
  query order (FR-008).
- A null region pointer means the surface behaves exactly as today (FR-020).

### Per-surface crossing query (new `CSurface` capability)

| Operation | Meaning |
|---|---|
| `countCrossings(from, dir) → int` | Signed crossings (+1 entering, −1 leaving) of the *undisplaced* surface along the ray |

This query ignores every rendering-side ray filter (visibility flags, LOD,
backface, displacement). Implementations:
- quadrics: analytic, all roots in the u/v range
- flat polygons: exact
- bilinear patches: analytic
- free-form surfaces: tessellation-seeded, Newton-refined

### Coverage mask (new generic `CRasterGrid` field)

| Field | Meaning |
|---|---|
| `coverage` | Per-micropolygon flag (null = all present). A quad is absent iff none of its 4 vertices is accepted by the surface's region predicate |

Consumed only where per-quad `bounds` are computed: an absent quad gets an empty
bound. Rasterizer loops are unchanged (FR-012).

### Statistics (`CStats`, extended)

`numCsgClassifications`, `numCsgSeamSplits`, `numCsgInstancesResolved`,
`numCsgProceduralExpansions` (SC-006).

## Relationships

```text
CSGTreeNode (capture, transient) ──SolidEnd──▶ CSG definition (shared, immutable)
                                                   ▲ 1
                                                   │ n
                     CSG solid (per placement/instance, CObject in scene + BVH)
                                                   │ 1
                                                   ▼ 0..1 (built on first use)
                                        Per-instance state
                                         ├── operandMembers ──▶ CSurface leaves ──▶ region adapter
                                         └── caps ────────────▶ CSurface caps   ──▶ region adapter
```

## Rules carried from the spec

| Rule | Where enforced |
|---|---|
| Composite surface only where and when needed (FR-002, FR-004) | Per-instance state built on first use; bucket and ray driven dicing and intersection |
| Seam accuracy tracks resolution (FR-007, FR-009) | Straddle split terminated by screen size, plus coverage mask |
| No CSG logic in hiders (FR-012) | Generic region predicate on `CSurface`; generic coverage mask on `CRasterGrid` |
| Faces keep operand shaders (FR-016) | Leaves are the operands' own surfaces with their own (cloned) attributes |
| Subtracted walls face outward (FR-011) | Static flip applied as `ATTRIBUTES_FLAGS_INSIDE` on cloned attributes |
| Interior/Exterior (spec 013 FR-009 to FR-012) | `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` set on each leaf's cloned attributes; `shading.cpp:1019` unchanged |
| Tolerance setting inert, with notice (FR-023) | Attribute still parsed; value unread; once-per-render notice |
