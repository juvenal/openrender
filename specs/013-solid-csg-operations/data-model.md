# Data Model: Solid CSG Operations

**Feature**: `013-solid-csg-operations` | **Date**: 2026-08-26

This maps the spec's Key Entities onto concrete `src/ri/` types, per the
architecture settled in `research.md` (Decisions 1-2, 5). All entities live
purely in the geometry domain — none are hider-specific.

## CSG Tree (build-time state)

Exists only while a solid block is open; discarded once resolved into a
Resolved Solid Boundary at the outermost `RiSolidEnd`.

| Field | Type | Notes |
|---|---|---|
| `operation` | enum `{ Primitive, Union, Intersection, Difference }` | Set from the `SolidBegin` operation-type string (FR-001); invalid strings rejected per FR-013 before a node is created. |
| `operands` | ordered list of `CSGTreeNode*` | For `Primitive`: not used (leaves hold captured `CObject*` instead, see below). For boolean nodes: children in declaration order — order matters for `Difference` (FR-005: first operand minus every subsequent one, in declaration order). |
| `leafObjects` | linked list of `CObject*` (only on `Primitive` nodes) | Captured via the diverted `addObject()` path (research.md Decision 1) instead of dispatching to `CRenderer::render()`. Each retains its own `CAttributes*` untouched — this is what makes per-leaf shader provenance free. |
| `outerXform` | `CXform*` | Captured once, at the outermost `SolidBegin`; all leaf geometry is later brought into this local frame before classification (research.md Decision 5). Only meaningful on the root node. |
| `parent` | `CSGTreeNode*` or null | Enables the nested-block validity checks (FR-006, FR-014, FR-019). |

**Validation rules** (enforced at `SolidBegin`/`SolidEnd`, mapped to FRs):
- Operation type not one of the four recognized strings → reject (FR-013).
- `SolidEnd` with no matching open `SolidBegin`, or scope-mismatched close →
  reject (FR-014); requires the `RENDERMAN_SOLID_PRIMITIVE_BLOCK` push/pop
  gap in `src/ri/ri.cpp` to be closed first (research.md, "RIB block-state
  enforcement gap").
- A `Primitive` node whose body contains a nested `SolidBegin` → reject
  (FR-019) — a leaf cannot itself hold a sub-tree.
- Any primitive captured while `delayed` (an `RiProcedural` expansion) is
  active as the immediate context → reject (research.md, "Delayed/procedural
  primitives... resolved as rejected").
- A `Primitive` node with zero captured leaves, or a boolean node with zero
  operands → resolves to no geometry, not an error (FR-016).
- A boolean node with exactly one operand → resolves to that operand's
  boundary unchanged, skipping the BSP combination step entirely (FR-017).

**State transition**: `Open (capturing)` → `Closed (resolved)`. A node
transitions to `Closed` only when its own `SolidEnd` fires *and* all of its
children are already `Closed` (children always close before their parent,
since blocks nest). The root node's transition to `Closed` is the trigger
that produces the Resolved Solid Boundary and re-enters `addObject()`
(research.md Decision 1).

## Resolved Solid Boundary (render-time state)

The permanent, renderable output of resolving a CSG Tree. Modeled as a new
`CObject`-derived container type (working name `CSolidObject`, alongside
`CPolygonMesh`, `CSurface` etc. in `src/ri/object.h`'s hierarchy) whose sole
job is to own a flat list of boundary-fragment children and present them to
every hider through the existing generic `CObject` dispatch — no new virtual
methods beyond what `intersect()`/`dice()`/`instantiate()` already require,
following the `CLoopSubdivMesh` → `CPolygonMesh`-children precedent
(`subdivisionLoop.h:26-30`).

| Field | Type | Notes |
|---|---|---|
| `attributes` | `CAttributes*` | Inherited `CObject` field. Set to the attribute state active at the outermost `SolidBegin`, so ordinary (non-Interior/Exterior) attribute lookups — e.g. a surface shader applied outside any leaf override — fall back exactly as they would for any other primitive at that scope. |
| `xform` | `CXform*` | Inherited `CObject` field. Set to `outerXform` from the CSG Tree root (research.md Decision 5) — the resolved boundary is defined in the outermost solid block's local frame, correctly re-transformable per-instance. |
| `children` | linked list of `CPolygonMesh*` (via inherited `CObject::children`/`sibling`) | One or more boundary fragments (see below). Never a single merged mesh (research.md Decision 2). |
| `bmin`, `bmax` | inherited `CObject` bbox fields | Computed as the union of all fragment bounds, same convention as every other composite `CObject`. |

**Boundary Fragment** (each `CPolygonMesh` child of a Resolved Solid
Boundary):

| Field | Type | Notes |
|---|---|---|
| `attributes` | `CAttributes*` | **Not** the container's attributes — this is the *originating leaf's* `CAttributes*`, carried through unchanged from the `leafObjects` capture. This is the entire mechanism behind FR-009's "face keeps its operand's shader": no new attribute-provenance bookkeeping is needed because `CAttributes*` was already durably per-leaf at capture time. |
| `vertices` / topology | standard `CPolygonMesh` fields | Triangulated fragments produced by BSP classify/clip (research.md Decision 3) over an operand mesh tessellated to the flatness/chordal-deviation tolerance from `Attribute "solid" "tessellationtolerance"` (research.md Decision 4) — curvature-adaptive for NURBS/quadric operands, subdivision-level-based for subdivision-surface operands. |
| `windingReversed` | implicit in vertex order, not a stored flag | For `Difference`, faces retained from a subtracted operand have reversed winding/normals relative to that operand's original orientation, since the visible cut surface is the *inward*-facing side of the removed volume (research.md Decision 3). This is baked into the fragment's vertex order at construction time, not tracked as separate state. |
| `"N"` vertex primvar | `CPl` entry, `CONTAINER_VERTEX` | Present, with analytic per-vertex shading normals (`crossvv(dPdu, dPdv)` at each vertex's original surface parametric coordinates), only for fragments sourced from a NURBS or quadric leaf — interpolated automatically by `CPolygonTriangle::sample`/`interpolate` since `CPolygonMesh` already handles any supplied `"N"` primvar generically (research.md Decision 4b). Absent for fragments sourced from polygon-mesh or subdivision-surface leaves, which fall back to the always-computed flat `VARIABLE_NG` per-facet normal — a documented, accepted v1 asymmetry (research.md Decision 4b). |

**Relationship to Interior/Exterior Shader Assignment**: a Boundary
Fragment's `attributes->interior` / `attributes->exterior` pointers (already
present on `CAttributes`, `attributes.h`) are exactly the Interior/Exterior
Shader Assignment entity from the spec — reused as-is, not duplicated. FR-020
("Interior/Exterior is a no-op outside a solid block") requires no new model
state either: it's an assertion about the *shading pipeline's* consumption of
`attributes->interior`/`exterior` (only consulted for objects that are
Resolved Solid Boundaries / their fragments), not about attribute storage
itself.

## Interior/Exterior Shader Assignment

Not a new entity — an existing `CAttributes` field pair (`interior`,
`exterior`, per `attributes.cpp:229-238`'s clone/attach handling). This
feature's only addition is *consuming* those fields at shading time when the
shaded primitive is a Boundary Fragment of a Resolved Solid Boundary, per
FR-010/FR-011/FR-012/FR-020. No schema change to `CAttributes` itself.

## Entity relationship summary

```
SolidBegin/SolidEnd (RIB) → CSG Tree (build-time, discarded after resolution)
                              ├─ Primitive node → leafObjects: CObject* (each with its own CAttributes*)
                              └─ Union/Intersection/Difference node → operands: CSGTreeNode*[]

CSG Tree (root, Closed) → [BSP boolean resolution, research.md Decision 3]
                         → Resolved Solid Boundary (CSolidObject : CObject)
                             └─ children: CPolygonMesh* (Boundary Fragments)
                                  attributes → (reused, unchanged) originating leaf's CAttributes*
                                                 ├─ interior  → Interior Shader
                                                 └─ exterior  → Exterior Shader

Resolved Solid Boundary → addObject() → CRenderer::render()
                              ├─ root->children (raytrace/photon intersect path)
                              └─ reyesContext->drawObject() (REYES/z-buffer dice path)
```

Both consumption paths reach the *same* `CSolidObject`/`CPolygonMesh`
instances — satisfying FR-007/FR-008/SC-002 by construction, not by
per-hider parity effort.
