# Phase 0 Research: Blobby Implicit Surfaces

**Feature**: `015-blobby-implicit-surfaces` | **Date**: 2026-08-29

Primary-source material (opcode tables, field functions, the repeller
reference C) lives in [research-inputs.md](./research-inputs.md) and is not
repeated here. This document records **decisions** and the reasoning behind
them.

All `NEEDS CLARIFICATION` items from Technical Context are resolved below.

---

## Decision 1 — Where the surface is derived

**Decision**: Evaluate the field and extract the surface inside
`CRendererContext::RiBlobbyV()`, producing a `CPolygonMesh` that is handed to
`addObject()`. The blobby's own `CXform` is retained (geometry stays in the
primitive's object space); no baking to world space.

**Rationale**: `addObject()` (`rendererContext.cpp:499`) is the single
chokepoint through which every primitive reaches both the raytrace object
tree and the REYES rasterizer. Anything that arrives through it is
hider-agnostic *by construction* — which is how FR-022 and FR-023 get
satisfied without per-hider parity work. This mirrors spec 013's resolved
solid boundary rather than inventing a second pattern.

Retaining the `CXform` (rather than CSG's identity-xform + world-space
vertices) is the right call here because a blobby has exactly one coordinate
frame. `CSurface::sample()` applies `xform->from` unconditionally, so
object-space vertices are transformed on demand, which preserves instancing
and keeps transformation motion blur working through the existing xform path.
CSG needed identity xforms only because it merges operands declared in
*different* frames — a problem blobby does not have.

**Alternatives considered**:

- *Ray-march the field per ray, as `CImplicit` does.* Rejected outright: this
  is the hider-coupled shape the feature's core constraint forbids.
  `CImplicit::dice()` is an empty body (`implicitSurface.cpp:196`), which is
  exactly why that class renders only under the raytracer. The spec places it
  out of scope; it is neither extended nor reused.
- *Extract lazily on first dice/intersect.* Rejected. It would satisfy
  "hider-independent" in letter while reintroducing per-hider timing
  differences, and it collides with FR-023a's determinism requirement under
  multi-threaded bucket rendering.

---

## Decision 2 — Surface extraction algorithm

**Decision**: **Seeded continuation marching tetrahedra.** Place a seed cell
at each primitive field's centre, walk outward through only those cells the
surface actually crosses, tracking visited cells in a deterministically
ordered container. Each visited cube is decomposed into 6 tetrahedra;
per-tetrahedron cases are unambiguous.

**Rationale**: Two independent requirements force this shape.

*Cost must track the surface, not the volume* (SC-012). The published
480-segment toroidal spiral occupies a large bounding box whose surface fills
a small fraction of it. Sampling a dense grid over the bound is
O(resolution³) in a volume that is mostly empty. Continuation visits O(surface
area) cells instead. This was settled in clarification and is not
re-litigated here.

*The mesh must be watertight* (FR-027). A blobby has to work as a CSG
operand, and BSP-based boolean resolution over a mesh with holes does not
fail loudly — it produces subtly wrong solids. Marching cubes has
ambiguous-face configurations where adjacent cells can disagree about
connectivity, opening cracks. Marching tetrahedra has no such case: every
tetrahedron's sign configuration has one unambiguous triangulation, and
adjacent tetrahedra necessarily agree on their shared face because the
decision depends only on the shared vertices' signs.

The cost is roughly double the triangles for equal fidelity. Analytic
gradient normals (Decision 4) absorb most of that penalty, because shading
quality stops depending on facet density.

**Alternatives considered**:

- *Marching cubes with a disambiguation table.* Fewer triangles, but trades a
  structural correctness guarantee for table entries whose errors would
  surface as rare, hard-to-attribute holes — precisely the failure mode CSG
  operand use cannot tolerate.
- *Adaptive octree + dual contouring.* Curvature-adaptive density and sharp
  feature reproduction. Rejected as poorly matched: blobby fields are C²-smooth
  by construction, so the sharp-feature machinery is wasted, while the octree
  adds substantial design and test surface.
- *An external isosurface library.* Rejected under Principle V. Marching
  tetrahedra is a small, self-contained, well-understood algorithm; importing
  a dependency for it would not be justifiable.

---

## Decision 3 — Determinism of extraction

**Decision**: The visited-cell set is keyed by integer lattice coordinates
and iterated in a defined order; the propagation frontier is a FIFO queue
seeded in code-array order. Extraction is single-threaded. Vertex and
triangle emission order is a pure function of that traversal order.

**Rationale**: FR-023a is a hard requirement with a visible failure mode.
Because each server in a distributed render re-derives the surface from the
re-emitted `Blobby` declaration (spec Clarifications Q2) rather than
receiving finished geometry, any divergence between servers appears as a seam
where their buckets meet. Hash-container iteration order, thread scheduling,
and floating-point reduction order are all plausible sources of such
divergence, and all are avoided by construction here rather than tested for
afterwards.

Single-threaded extraction is not a performance concession worth arguing
about: it happens once per primitive at scene-build time, on the RIB parsing
thread, which is already serial. Parallelising it would introduce exactly the
ordering hazards this decision exists to eliminate, for no benefit the spec
asks for.

**Alternatives considered**: *Parallel extraction with a post-sort to restore
canonical order.* Rejected — it reintroduces floating-point summation-order
risk in the shared seam vertices and buys nothing, since extraction is not on
the critical path.

**Note for implementation**: openRender's own history contains the cautionary
case. `CStochastic::rasterBegin`'s `nullBucket` early-out assumed a
single-threaded invariant that did not hold, and composited stale fragments
for months before anyone traced it. Treat any "this is obviously order-
independent" reasoning in the polygonizer with the same suspicion.

---

## Decision 4 — Shading normals from the analytic gradient

**Decision**: Per-vertex normals are the normalized analytic gradient of the
combined field at the vertex position, evaluated directly rather than
differenced from neighbouring facets.

**Rationale**: FR-024 requires it, and the field makes it cheap: every
primitive field has a closed-form derivative, and the combining operations
differentiate by the ordinary rules (sum of gradients for add, the product
rule for multiply, the winning operand's gradient for max/min, negation for
negate). This is strictly better than the position spec 013 was forced into,
where subdivision-sourced fragments had no analytic normal available and had
to rely on tessellation density alone (013 `research.md` Decision 4b).

It is also what makes Decision 2's triangle-count penalty acceptable: with
gradient normals, shading smoothness is decoupled from facet density, so the
extra tetrahedral triangles cost memory rather than image quality.

**Gradient at a `max`/`min` seam**: the gradient is discontinuous exactly
where two operands are equal. This is correct — that seam is a genuine crease
in the surface (it is what makes unblended union look unblended), and the
appnote itself warns that "non-smooth surfaces can look nasty at their
creases". Pick the gradient of the operand selected by the same tie-break the
field evaluation used, so normals and geometry agree.

---

## Decision 5 — Depth-file access for the repelling ground plane

**Decision**: Load the whole depth image once, at `RiBlobby` time, into a flat
float array alongside its NDC and camera matrices, through a **context-free**
reader in `blobbyRepeller.cpp`. Do not use `CTexture::lookupz()`.

**Rationale**: This resolves a genuine integration blocker discovered during
research. The existing depth lookup path is bound to shading:
`CTexture::lookupz(s, t, z, context)` delegates to
`lookupPixel(res, si, ti, context)`, whose implementation dereferences
`context->thread` (`texture.cpp:667`) to index the per-thread tile cache.
Polygonization runs at `RiBlobby` time, where **no `CShadingContext`
exists** — passing `NULL` is a null-pointer dereference, not a graceful
degradation.

A one-shot full read is not merely a workaround, it is a better fit. The tile
cache exists to avoid faulting in a large texture for scattered shading
lookups. A repeller is sampled densely and exhaustively over the blob's whole
footprint during extraction, so tiling buys nothing and costs indirection. A
z-file is a single-channel depth image, typically small.

**Alternatives considered**:

- *Fabricate a scratch `CShadingContext` at build time.* Reuses tested code
  but manufactures a shading object outside any shading, couples geometry
  construction to the shading subsystem, and would be fragile against future
  changes to what a context is assumed to contain.
- *Defer opcode 1003 to the refinement spec.* Would have removed the only
  external-file dependency from v1. Rejected because the user explicitly
  scoped 1003 into v1, and because the one-shot read proved tractable.

**Feasibility confirmed** (this was the one item still open when the decision
was first drafted; it is now closed, so no unknowns are carried into Phase 1).
A context-free read path already exists and needs no new file-format work:

- `CRenderer::locateFile(fn, name, path)` resolves a search-path name with no
  context (`texture.cpp:2137`).
- `TIFFOpen` / `TIFFGetField` / `TIFFReadScanline` are called directly at
  several context-free sites (`texture.cpp:2146`, `:2193`,
  `texmake.cpp:243`), guarded by the existing `tiffErrorHandler` so a bad
  file does not abort the process.
- The view matrices FR-010 needs are already recovered by the shadow loader:
  `readFloat32Array(in, header.toNDC, 16)` and the matching `toCamera` read
  (`texture.cpp:1292-1293`), with `CShadow` showing how they are applied
  (`texture.cpp:1179-1181`).

So `blobbyRepeller.cpp` composes existing pieces rather than introducing a
reader. Had this gone the other way — a new TIFF/depth reader — opcode 1003
would have grown materially, which is why User Story 7 is P4 and separable:
the fallback would have been to defer it to the refinement spec.

---

## Decision 6 — Per-blob value propagation

**Decision**: Blended primvar values are carried up the code array *alongside*
the field, in the same evaluation walk. Each combining operation blends its
operands' values the way it blends their fields: add and multiply apportion
proportionally by operand contribution; max and min pass through the winning
operand's value; a negated operand, and the subtrahend of a subtraction,
contribute nothing. Where an apportionment's denominator is zero, fall back
to an equal split among contributing operands (FR-019a).

**Rationale**: This is the reading settled in clarification, and it is the
only one under which value blending and shape blending agree everywhere. The
alternative — a flat weighted average over all leaves by field strength —
bleeds colour between blobs deliberately kept from merging, because two
fingers of the reference hand model overlap in *field* even though the `max`
that combines them means they do not overlap in *surface*. That would
directly contradict User Story 4's acceptance scenarios.

The structural consequence for implementation is that value propagation is
not a post-pass over a finished mesh. It is part of the evaluator's return
value, computed per evaluation point, which is why `blobbyField` returns
field, gradient, and blended values together rather than exposing three
separate entry points over the same tree walk.

**Storage**: blended values become `CONTAINER_VERTEX` `CPlParameter` entries
in the emitted `CPl`, following `csgBuildMeshForAttributeGroup`'s packing of
P and N (`csgTree.cpp:600-615`). No new plumbing is required to get them to
shaders.

---

## Decision 7 — Statistics and observability

**Decision**: Add house-style `CStats` counters — blobby primitives, total
primitive fields, field evaluations, cells visited, surface cells, triangles
emitted — incremented with `atomicIncrement`, plus a derived
**surface-cells-to-visited-cells percentage** printed by
`CStats::printStats()`.

**Rationale**: This closes the observability gap flagged as Outstanding in
the clarification coverage summary, and it follows the existing pattern
rather than inventing one: `CStats` already collects per-subsystem counters
(`stats.h:90-99` for the REYES rasterizer, `numSplits`/`numUsplits` for
`CPatch`) and already prints derived ratios under a level gate
(`stats.cpp:153-156` prints U/V/UV split percentages only when
`numSplits > 0`). The blobby ratio is modelled directly on those lines.

The ratio earns its place beyond mere diagnostics: **it is the measurement
instrument for SC-012.** That criterion asserts extraction cost tracks the
surface rather than the bounding-box volume, and without a reported figure
the assertion could only be inspected by eye or inferred from wall-clock
noise. A continuation polygonizer that has silently degenerated into
sweeping the volume shows up immediately as a collapsed ratio.

**Alternatives considered**: *Counters only*, matching `numGprims`
granularity — consistent but leaves SC-012 unevidenced. *Adding per-primitive
wall-clock timing* — rejected because the spec sets no wall-clock target and
timing under a multi-threaded build is noisy enough that the number would be
reported but never acted on.

**Noted, not actioned**: constitution Principle IV states that CLI tools MUST
support machine-parseable output, and these counters print only through
`printStats()`'s human-readable report. This introduces no violation — the
feature adds no CLI surface, and `printStats()` was already human-only long
before it — but it does mean the figure that evidences SC-012 is currently
greppable prose rather than structured data. If machine-readable statistics
are ever added to `printStats()`, the blobby counters ride along for free;
introducing a blobby-only JSON path would be inconsistent with every other
counter in `CStats` and is deliberately not proposed here.

---

## Decision 8 — Motion blur representation

**Decision**: Extract the surface once at shutter-open time, then produce the
second sample by advecting each existing vertex onto the shutter-close field's
level set along the gradient (a short Newton iteration from its current
position). Emit both as `CPl::data0` and `CPl::data1`.

**Rationale**: The existing representation permits exactly two samples
(`pl.h:125`) and detects motion as `mesh->pl->data1 != NULL`
(`polygons.h:104`). Advection is what makes the second sample *compatible*
with the first: vertex count, ordering, and triangle connectivity are
identical by construction, which is the only way a two-sample vertex array
can represent motion at all. It also delivers FR-026's requirement to blur
genuine shape change — a growing or moving blob — rather than only rigid
displacement, because each vertex lands where that vertex's part of the
surface actually went.

**The iteration must be a fixed step count, not a convergence test.** This is
a determinism trap worth stating explicitly, because it interacts with
Decision 3. A loop that runs "until converged" makes the step count a
floating-point predicate — the same input can take a different number of
steps under different compiler flags or FMA contraction, landing the vertex in
a different place on different machines. That is precisely the cross-server
seam FR-023a exists to prevent, and it would strike hardest at vertices near a
topology change, where convergence is most marginal. A fixed step count makes
the advected position a deterministic function of its input. Decision 3's
determinism guarantee and its test must cover the motion path, not only the
initial extraction.

When topology changes within the shutter interval (lobes merging, a piece
vanishing), advection cannot be faithful — there is no correct destination for
a vertex whose surface ceased to exist. FR-026 and User Story 8 scenario 4
require only a bounded, non-crashing, documented result in that case, which
the fixed step count delivers by construction: a vertex that has not reached
the new level set after its allotted steps simply stops where it is,
degrading to locally unblurred rather than producing wild geometry.

**Alternatives considered**:

- *Independent extraction at both times.* Produces two meshes with different
  vertex counts, which the `data0`/`data1` format simply cannot represent.
- *Extract the field swept over the interval.* Robust to topology change, but
  fattens the surface even for slow motion and diverges from how every other
  primitive blurs — a violation of the standing rule that effects are
  implemented once, generically.

---

## Decision 9 — `mpoint` as a variable type

**Decision**: Add `TYPE_MPOINT` to `EVariableType` (`rendererc.h:43-53`),
following the `TYPE_QUAD` precedent.

**Rationale**: This was flagged as an unverified feasibility item during
specify; research resolves it. `TYPE_QUAD` exists in the enum annotated
`// For "Pw"` — a type whose RIB representation (4 floats) differs from what
the shader ultimately consumes. `mpoint` is the same shape of problem: 16
floats in RIB, a `point` in the shader. The switch sites that must gain a case
are enumerable and few — `pl.cpp:720`, `ribOut.cpp:1609` and `:1722`,
`rendererContext.cpp:1274` — which is what makes this a bounded change rather
than a new subsystem.

The per-blob matrix is inverted once at build time and composed with the
blob's own inverse transform, so per-evaluation cost is a single matrix-vector
multiply, and the result blends through Decision 6's mechanism like any other
per-blob value.

---

## Decision 10 — Opcode 4/5 dual interpretation

**Decision**: The evaluator takes the operand order as a parameter resolved
once at construction from the scene option; it is not a runtime branch inside
the per-point evaluation loop.

**Rationale**: FR-013 requires both readings with the RISpec order as
default. Resolving the mapping once, at construction, keeps the hot evaluation
path free of a branch that cannot change mid-primitive, and it makes the two
orders trivially unit-testable in isolation (SC-002) by constructing the
evaluator both ways over the same code array.

The erratum itself — RISpec 3.2 Table 5.3 versus AppNote #31, verified
verbatim against both raw sources — is documented in
[research-inputs.md](./research-inputs.md) and must be reproduced in the Hugo
site documentation per FR-032, so scene authors encountering wrong-looking
subtraction have something to find.

---

## Decision 11 — Surface threshold calibration

**Decision**: Do not hard-code the commonly cited 0.5. Derive the threshold
from the published scenes' documented behaviour, assert the derivation in a
unit test, and record the resulting value with its justification.

**Rationale**: Settled in clarification (FR-015). Two published constraints
bracket the value and are directly testable:

- The six-blob coloured octahedron places unit-sphere fields at ±0.89 on each
  axis and **must** resolve to one connected surface.
- The appnote's unblended sphere-cluster pair **must** resolve to separate
  surfaces.

Encoding both as connectivity assertions turns a folklore constant into a
verified one, and — importantly — a test that fails loudly if the field
function or the threshold is ever changed incorrectly. Note that the analytic
ground-truth cases of SC-003 all depend on the threshold, so this must be
settled before those tests can assert absolute radii.

---

## Decision 12 — Blobby as a CSG operand requires no implementation

**Decision**: Ship FR-027 as a regression test only. Write no integration
code.

**Rationale**: Research shows the path already works. `addObject()` chains any
incoming object into `currentSolid->leafObjects` when a solid block is open
(`rendererContext.cpp:504-506`), and `csgTessellateOperand` already has a
`CPolygonMesh` branch (`csgTree.cpp:316`) that triangulates a mesh leaf via
`csgTessellatePolygonMeshOperand`. The `csgValidateProceduralCapture` guard
that rejects delayed and procedural primitives is called from the procedural
paths only — never from `addObject()` — so a blobby is never subject to it.

This was listed as a feasibility risk during specify; it resolves in the
favourable direction. The remaining risk is the *watertightness* of the
emitted mesh, which is why Decision 2 chose tetrahedra — and that is worth a
targeted test, since a leaky mesh corrupts boolean results silently rather
than erroring.

---

## Resolved unknowns summary

| Unknown from Technical Context | Resolution |
|---|---|
| Extraction algorithm | Decision 2 — seeded continuation marching tetrahedra |
| How determinism is guaranteed | Decision 3 — ordered integer-keyed visited set, FIFO frontier, single-threaded |
| Depth-file access without a shading context | Decision 5 — context-free one-shot full read; `lookupz` is unusable, but `locateFile` + direct TIFF reads + the shadow loader's matrix recovery already exist |
| `mpoint` feasibility | Decision 9 — `TYPE_QUAD` precedent; four enumerable switch sites |
| CSG leaf-capture shape | Decision 12 — already works; test-only |
| Motion sample generation | Decision 8 — extract at open, advect to close; two samples is a format-wide limit |
| Threshold value | Decision 11 — calibrated and asserted, not assumed |
| Statistics scope | Decision 7 — counters plus surface-cell ratio, instrumenting SC-012 |

No `NEEDS CLARIFICATION` items remain.
