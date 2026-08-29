# Phase 1 Data Model: Blobby Implicit Surfaces

**Feature**: `015-blobby-implicit-surfaces` | **Date**: 2026-08-29

Derived from the spec's Key Entities section and
[research.md](./research.md)'s decisions. Types below are design intent, not
final signatures.

---

## Lifetime overview

Everything in this model except the final `CPolygonMesh` is **build-time
only** — constructed inside `RiBlobbyV`, consumed during extraction, and
destroyed before the call returns. Nothing here survives into rendering,
which is what makes the hider-independence constraint (FR-022) structural
rather than a discipline to maintain.

```
Blobby statement (RIB / C API)
        │
        ▼
  CBlobbyProgram  ──validate──▶  diagnostics (FR-014, FR-017, FR-029)
        │
        ▼
  CBlobbyField (evaluator: field + gradient + blended values)
        │                    ▲
        ▼                    │ samples
  CBlobbyPolygonizer ────────┘
        │
        ▼
  CPolygonMesh (+ CPl with data0/data1)  ──▶ addObject()  ──▶ every hider
```

---

## 1. `CBlobbyProgram` — the validated code array

**Represents**: the parsed, validated instruction stream. Built once;
immutable thereafter.

| Field | Type | Notes |
|---|---|---|
| `instructions` | `CArray<CBlobbyInstruction>` | In declaration order. Index **is** the result reference. |
| `floats` | `float *`, `nfloats` | Owned copy of the RIB floats array. |
| `strings` | `char **`, `nstrings` | Owned copies. Depth-file names for opcode 1003. |
| `numLeaves` | `int` | Count of instructions with `opcode >= 1000`. Authoritative — see validation. |
| `declaredLeaves` | `int` | The `nleaf` the author wrote. Retained for diagnostics. |
| `opcodeOrder` | `EBlobbyOpcodeOrder` | `BLOBBY_ORDER_RISPEC` (default) or `BLOBBY_ORDER_APPNOTE`. Resolved once from the scene option (research Decision 10). |

### `CBlobbyInstruction`

| Field | Type | Notes |
|---|---|---|
| `opcode` | `int` | As written. |
| `leafIndex` | `int` | Ordinal among `opcode >= 1000` instructions, or `-1` for combining instructions. This is the index into per-blob parameter arrays (FR-016). |
| `operands` | `int *`, `numOperands` | For primitive fields: indices into `floats`/`strings`. For combining ops: indices of **earlier** instructions. |

### Validation rules (all produce a diagnostic, never a crash — FR-029)

| Rule | Requirement | Failure |
|---|---|---|
| Opcode is one of `0..7` or `1000..1003` | FR-012, FR-014 | Reject, naming opcode and position. Reserved `1004..1099` rejected here. |
| Instruction is complete within the array | Edge case | Reject on truncation. |
| Variable-arity count is `> 0` and fits | Edge case | Reject on zero, negative, or overrun. |
| Combining operand `< own index` | FR-006 | Reject self- and forward-references. |
| `floats` operand + field width `<= nfloats` | Edge case | Widths: 1000→1, 1001→16, 1002→23, 1003→4 floats + 1 string. |
| `strings` operand `< nstrings` | Edge case | — |
| `declaredLeaves == numLeaves` | FR-017 | **Diagnostic, then continue** using `numLeaves`. Pixar's own hand example violates this (declares 21, emits 22). Per-blob parameter reads clamp to the shorter of the two arrays. |
| At least one primitive field present | FR-030 | Not an error — yields no geometry. |

---

## 2. `CBlobbyField` — the evaluator

**Represents**: a pure function of position over a `CBlobbyProgram`. No
renderer state, no allocation per call. This purity is what makes the
constitution's TDD gate satisfiable (Principle III).

**Two entry points over the same tree walk**, split by cost:

```
evaluate      (point, time, /*out*/ field, /*out*/ gradient)
evaluateWeights(point, time, /*out*/ field, /*out*/ gradient, /*out*/ leafWeights)
```

`leafWeights[i]` is leaf *i*'s normalized share of the result at that point —
the input to per-blob value blending. Returning weights rather than blended
values keeps the evaluator independent of *which* primvars a given blobby
carries.

**Why the split matters.** Producing `leafWeights` costs an O(numLeaves)
write per call. The continuation walk evaluates the field at every corner of
every cell it examines — millions of evaluations — but weights are only ever
needed at the comparatively few positions where a vertex is actually emitted.
On the 500-field spiral (SC-012), collapsing these into one entry point would
mean a 500-float write on every one of those millions of calls, for a result
almost all of them discard. Since SC-012 is the one scale criterion this
feature commits to, the traversal must use the cheap form and only vertex
emission the expensive one.

### Per-instruction semantics

| Opcode | Field | Gradient | Weight propagation |
|---|---|---|---|
| 1000 constant | the float | zero | own leaf gets weight 1 |
| 1001 ellipsoid | `(1-R²)³` for `R<=1` else 0, `R` in unit-sphere space via the inverse 4×4 | chain rule through the inverse matrix | own leaf |
| 1002 segment | convolution of segment impulse with the same bump | ditto | own leaf |
| 1003 repeller | `repulsion(z,A,B,C,D)` — see [research-inputs.md](./research-inputs.md) | numeric along the depth-map normal | own leaf |
| 0 add | `Σ` | `Σ` | proportional to each operand's contribution |
| 1 multiply | `Π` | product rule | proportional |
| 2 max / 3 min | winner | winner's gradient | winner takes all |
| 4/5 subtract | `a - b` | `∇a - ∇b` | `a`'s weights only; `b` contributes none |
| 4/5 divide | `a / b` | quotient rule | `a`'s weights only |
| 6 negate | `-a` | `-∇a` | none |
| 7 identity | `a` | `∇a` | `a`'s weights unchanged |

**Degenerate guards**: divide by zero, an apportionment whose denominator is
zero (FR-019a → equal split among contributing operands), a singular ellipsoid
matrix (edge case → treat as contributing no field), and a zero-length segment
(→ behaves as a sphere, US3 scenario 3). None may emit an invalid value.

**Gradient at a max/min seam** is discontinuous by design — that crease is
what makes an unblended union look unblended. The tie-break must match the
field evaluation's so normals and geometry agree (research Decision 4).

---

## 3. `CBlobbyRepeller` — build-time depth map

**Represents**: one loaded z-file plus its shaping parameters. Constructed
during validation, one per opcode-1003 instruction.

| Field | Type | Notes |
|---|---|---|
| `depth` | `float *`, `width`, `height` | Whole image, read once (research Decision 5). Never uses `CTexture::lookupz` — that path dereferences `context->thread` and no shading context exists at build time. |
| `toNDC`, `toCamera` | `matrix` | Recovers "the view direction in which the z-file was generated" (FR-010). Shape mirrors `CDeepShadowHeader` (`texture.h:148`). |
| `A`, `B`, `C`, `D` | `float` | Cut-off height, barrier sharpness, bulge position, bulge height. |
| `valid` | `int` | False when the file is missing or unreadable — diagnostic issued, field contributes zero, scene continues (US7 scenario 4). |

---

## 4. `CBlobbyPolygonizer` — extraction state

**Represents**: transient state of one seeded continuation walk. Destroyed
when extraction completes.

| Field | Type | Notes |
|---|---|---|
| `cellSize` | `float` | From the fidelity attribute, defaulted from the primitive's extent (FR-025). |
| `visited` | ordered set keyed by `(i,j,k)` | **Ordered, not hashed** — iteration order is part of FR-023a's determinism guarantee (research Decision 3). |
| `frontier` | FIFO queue of `(i,j,k)` | Seeded in code-array order. FIFO, not LIFO, so traversal order is reproducible. |
| `vertices` | `CArray<vertex>` | Position, analytic normal, blended values. Emission order is a pure function of traversal order. |
| `triangles` | `CArray<int[3]>` | — |
| Motion advection | fixed step count | The second sample moves each vertex onto the shutter-close level set in a **fixed** number of gradient steps — never "until converged", which would make the step count a floating-point predicate and reintroduce the cross-machine divergence FR-023a forbids (research Decision 8). |
| `edgeCache` | map from edge key to vertex index | Shared vertices between adjacent tetrahedra — this is what makes the mesh watertight rather than a triangle soup. |

**Seeding**: one cell at each primitive field's centre. A field whose surface
does not enclose its own centre (a repeller, a negated blob) contributes no
seed; the surface is still reached by continuation from a neighbouring blob's
seed. A blobby whose combined field never crosses the threshold yields zero
triangles and no error (FR-030). Its dual — a field at or above the threshold
*everywhere* — must terminate promptly with a diagnostic rather than walking
outward forever (edge case).

**Cell decomposition**: each visited cube splits into 6 tetrahedra. Every
tetrahedral sign configuration has one unambiguous triangulation, and adjacent
tetrahedra agree on shared faces because the decision depends only on the
shared vertices' signs — this is the watertightness argument FR-027 rests on.

---

## 5. Emitted geometry

**Represents**: the single artifact that survives into rendering. Nothing
blobby-specific reaches any hider.

| Element | Value |
|---|---|
| Object | `CPolygonMesh(attributes, xform, pl, npoly, nholes, nvertices, vertices)` |
| `xform` | The blobby's own `CXform` — **not** identity. Vertices stay in object space; `CSurface::sample()` applies `xform->from`. Preserves instancing and transformation motion blur. |
| `pl` | `CPl(dataSize, numParams, plParams, data0, data1)` |
| `plParams[0]` | `P`, `CONTAINER_VERTEX` |
| `plParams[1]` | `N`, `CONTAINER_VERTEX` — analytic gradient (FR-024) |
| `plParams[2..]` | One per author-declared varying/vertex primvar, blended (FR-019). Packing follows `csgBuildMeshForAttributeGroup` (`csgTree.cpp:600-615`). |
| `data1` | Second motion sample, or `NULL`. `CPolygonTriangle::moving()` is exactly `pl->data1 != NULL` (`polygons.h:104`) — no hider changes needed. |

**`u`/`v`/`s`/`t`** (FR-021): a blobby has no global parameterisation, as
RISpec states. These take the same values subdivision surfaces give, since
subdivs share the limitation — to be read off the subdiv path and matched, not
invented.

---

## 6. Attribute and option

| Entity | Scope | Notes |
|---|---|---|
| Fidelity attribute | `CAttributes`, inheritable | FR-025. Requires all four layers: token constant (`ri.h`/`ri.cpp`), parsing (`RiAttributeV`), storage/query (`CAttributes::find`), **and pre-declaration** (`initDeclarations()`) — omitting the last makes the RIB parser reject it before parsing is reached. Zero, negative, or absurd values → diagnostic plus fallback (US6 scenario 4). |
| Opcode-order option | `COptions`, scene-wide | FR-013. Same four layers. Resolved once into `CBlobbyProgram::opcodeOrder` at construction, never branched on per point. |

---

## 7. Statistics

Added to `CStats` (`stats.h`), incremented with `atomicIncrement`, printed by
`printStats()` under the existing level gate.

| Counter | Meaning |
|---|---|
| `numBlobbies` | Blobby primitives constructed |
| `numBlobbyLeaves` | Total primitive fields across all blobbies |
| `numBlobbyFieldEvals` | Field evaluations performed |
| `numBlobbyCellsVisited` | Cells the continuation walk examined |
| `numBlobbySurfaceCells` | Cells that actually straddled the surface |
| `numBlobbyTriangles` | Triangles emitted |

Printed derived ratio: `numBlobbySurfaceCells / numBlobbyCellsVisited` as a
percentage, guarded by `> 0` exactly as the U/V split ratios are
(`stats.cpp:153-156`). This ratio is the measurement instrument for SC-012 —
a continuation walk that has degenerated into sweeping the volume shows up as
a collapsed percentage rather than only as a slow test.
