# Data Model: Full Subdivision Surface Support

**Feature**: `010-full-subdivision-support` | **Date**: 2026-08-11 | **Spec**: [spec.md](./spec.md)

Entities below mirror spec.md's Key Entities section, grounded in real class fields (existing and new) rather than
abstract descriptions. Per research.md R1/R2, the first entity is a **verification target**, not new state.

## Cross-Hider Motion-Blur Mechanism (verification target — no new fields)

| Field | Type | Owner | Notes |
|---|---|---|---|
| `moving` | `bool` (via `vertexData->moving`) | `CSubdivision` (`subdivision.h:45`, `moving()` override) | Already exists; drives whether `CTesselationPatch::sampleTesselation()` takes the two-time-sample path |
| begin/end tessellation grids | two grid buffers | `CTesselationPatch` (`surface.cpp:1393-1513`) | Already exists; populated once per moving object via `PARAMETER_BEGIN_SAMPLE`/`PARAMETER_END_SAMPLE`, LERP'd at `intersect()` time |

**Validation rules**: None new — this feature adds no fields here. Its only obligation (FR-001/FR-002) is
demonstrating, via test scenes, that the existing fields/flow already produce correct output for subdivision
surfaces specifically.

**State transitions**: N/A — pre-existing, unmodified control flow (per-object static→moving branch already
present in `sampleTesselation()`).

## Facevarying Corner Value (User Story 2, FR-004)

| Field | Type | Owner | Notes |
|---|---|---|---|
| `facevarying` (existing, being removed as a single-pointer field) | `float *` | `CSVertex` (`subdivisionCreator.cpp:157`) | **Removed** by this feature — replaced by the per-`CVertexFace` slot below |
| `facevarying` (new) | `float *` | `CSVertex::CVertexFace` (`subdivisionCreator.cpp:104-109`) | New field on the existing per-incident-face node; one value per `(vertex, incident face)` pair, populated in the assignment loop at `subdivisionCreator.cpp:1858-1860` using the already-in-scope `(faces[i], j)` pair |
| requesting-face parameter | `CSFace *` (or equivalent identity) | New parameter on `CSVertex::computeVarying()` (`subdivisionCreator.cpp:1237`), threaded from `CSFace::computeVarying()` (`1433`) and from the `sort()` call sites (e.g. `1614`) where `this` (the enclosing `CSFace`) is already available | Selects which `CVertexFace` node's `facevarying` slot to read; NULL/absent face-context falls back to the first available slot (matches today's single-value behavior for vertices with only one incident face, which is unaffected by this fix) |

**Validation rules**: A vertex with only one incident face has exactly one `CVertexFace` node and therefore exactly
one facevarying value — behavior for such vertices (including every boundary vertex and any vertex on a mesh with
no facevarying data at all, Acceptance Scenario 3 of User Story 2) is unchanged. A vertex with N incident faces
retains up to N distinct facevarying values, one per node.

**State transitions**: None — facevarying values are set once at mesh-creation time (`create()`) and read-only
thereafter through the recursive `computeVarying()` chain during subdivision.

## Subdivision Tag (User Story 3, FR-005)

| Field | Type | Owner | Notes |
|---|---|---|---|
| `RI_HOLE`, `RI_CREASE`, `RI_CORNER`, `RI_INTERPOLATEBOUNDARY` (existing) | `RtToken` (`const char *`) | `src/ri/ri.h:231-234` / `ri.cpp:160-163` | Unchanged |
| `RI_FACEVARYINGINTERPOLATEBOUNDARY` (new) | `RtToken` | `ri.h`/`ri.cpp`, alongside the existing four | Value: integer boundary-interpolation mode, same shape as `RI_INTERPOLATEBOUNDARY`'s existing value |
| `RI_FACEVARYINGPROPAGATECORNERS` (new) | `RtToken` | `ri.h`/`ri.cpp` | Value: boolean/integer flag |
| `RI_CREASEMETHOD` (new) | `RtToken` | `ri.h`/`ri.cpp` | Value: enumerated string/token selecting a crease-evaluation method (RISpec-defined) |
| new dispatch arms + flags | `unsigned int` bit flags on `CSubdivData` (alongside the existing `FACE_INTEPOLATEBOUNDARY`, `subdivisionCreator.cpp:47`) | `subdivisionCreator.cpp`'s tag-dispatch chain in `create()` | Replaces three `CODE_BADTOKEN` fall-throughs with tag-specific parsing + storage |

**Validation rules**: A recognized tag (one of the three new names) with an out-of-range/unrecognized value MUST
report a diagnostic naming the tag and value (FR-005, Acceptance Scenario 2) rather than a hard error or silent
ignore. An unrecognized tag name (anything outside all seven now-supported names) continues to hit the existing
`CODE_BADTOKEN` path unchanged.

**Classification**: `facevaryinginterpolateboundary` and `facevaryingpropagatecorners` are facevarying-domain tags
(interact with the Facevarying Corner Value entity above); `creasemethod` is a crease-domain tag (interacts with
the crease-quality investigation, User Story 4). All three, like the existing four, are per-mesh tags parsed from
`RiSubdivisionMesh`'s tag/args string array — not `CAttributes` push/pop state (see research.md R4's rejected
alternative).

## Hierarchical Subdivision Edit (User Story 5, FR-007/FR-008/FR-009)

| Field | Type | Owner | Notes |
|---|---|---|---|
| override list | new type, e.g. `struct CHierarchicalOverride { int faceIndex; int level; RtToken tagName; /* value */ CHierarchicalOverride *next; }` | New (geometry layer; e.g. `subdivisionHierarchical.h`) | One override = one `(face, level, tag, value)` tuple |
| base mesh reference | `CSubdivMesh *` | New hierarchical-mesh wrapper type or a new field on `CSubdivMesh` itself | Overrides are layered on top of a base mesh's default tags, never replacing the base mesh's own storage |
| resolved-tag cache (optional) | per-face/per-level lookup | Geometry layer only | An implementation detail for override resolution during subdivision; not exposed to any hider |

**Validation rules**: An override whose `faceIndex` or `level` does not exist on the base mesh is invalid (FR-009)
— it is skipped individually with a diagnostic naming the affected face/level; the rest of the mesh (and the rest
of the override list) renders normally. An override targeting a face/level combination that exists but is never
reached at a given render's effective subdivision depth is **not** an error (Edge Cases) — it simply has no visible
effect for that render.

**State transitions**: Override resolution happens once per mesh at subdivision-evaluation time, consulting the
override list against the currently-evaluated face/level; it does not mutate the base mesh's own default tags
(Acceptance Scenario 1 of User Story 5 — "without affecting the base mesh's default tags").

**Precedence rule**: When a new User-Story-3 tag and a hierarchical override target the same face, the override's
value takes precedence at its targeted face/level (Edge Cases, spec.md), consistent with RISpec's hierarchical-edit
model of layering overrides on top of base tags.

## Loop Scheme (User Story 6, FR-010/FR-011)

| Field | Type | Owner | Notes |
|---|---|---|---|
| `"loop"` scheme string | existing `const char *scheme` parameter | `RiSubdivisionMeshV` (`rendererContext.cpp:5348`) | Currently rejected at `rendererContext.cpp:5364-5366`; this feature adds it as a second accepted value alongside `"catmullclark"` |
| Loop algorithm state | new type(s), e.g. `CLoopSubdivMesh : public CObject` | New sibling file(s) (e.g. `subdivisionLoop.{h,cpp}`) | Implements the same `CObject`/`CSurface` contract (`intersect()`, `dice()`, `instantiate()`, `sample()`, `interpolate()`) that `CSubdivMesh`/`CSubdivision` already implement for Catmull-Clark |

**Validation rules**: An all-triangle face topology is required for Loop subdivision; a mixed triangle/non-triangle
topology under `scheme="loop"` MUST be rejected with a diagnostic identifying the mesh as unsuitable, not a crash
or silently-degenerate geometry (Edge Cases).

**Forward-looking extension seam (documentation only — not built in this feature)**: Because Loop shares the exact
`CObject`/`CSurface` seam Catmull-Clark uses, a future third scheme (or a future PathTrace hider) needs zero new
integration work to support Loop-scheme geometry — it already reaches it through the same virtual dispatch every
other primitive uses.

## Crease-Quality Reproducer (User Story 4, FR-006)

| Field | Type | Owner | Notes |
|---|---|---|---|
| test scene | RIB file | `examples/rib/tests/` (new) | Heavily-creased mesh, multiple crease sharpness values converging at one shared vertex |
| root-cause finding | prose, not code | `HIDER_PARITY.md` subdivision-surfaces section (new, R9) or a dedicated note if deferred | Documents reproduced/not-reproduced, root cause if found, and fix-or-deferred decision |

**Classification**: Not a code entity — a documentation/verification artifact. No renderer data model changes are
committed here unless and until root-causing identifies a specific, scoped fix (per FR-006's explicit gate).
