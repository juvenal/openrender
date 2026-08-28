# Contract: RIB / RenderMan Interface Surface for Solid CSG

**Feature**: `013-solid-csg-operations` | **Date**: 2026-08-26

This feature's external interface is the RenderMan Interface (RIB tokens and
the corresponding `Ri*` C API), not a network/service API. This document is
the contract other tools (scene authors, RIB generators, the RIB parser
grammar) can rely on.

## `SolidBegin` / `SolidEnd`

```
SolidBegin <operation> [existing RIB block content...]
...
SolidEnd
```

| Element | Contract |
|---|---|
| `RiSolidBegin(RtToken operation)` | `operation` MUST be exactly one of `"primitive"`, `"union"`, `"intersection"`, `"difference"` (case-sensitive, matching existing RIB token conventions). Any other value → `error(CODE_BADTOKEN, ...)`, block treated as unrecognized and its contents skipped for CSG purposes (does not crash; matches FR-013). |
| `RiSolidEnd(void)` | Closes the most recently opened, still-open `SolidBegin`. An `RiSolidEnd` with no matching open block, or one that closes out of order relative to interleaved `AttributeBegin`/`TransformBegin`/etc., → `error(CODE_BADTOKEN, ...)` via the `check()` scope-mask machinery (requires closing the block-state push/pop gap identified in `research.md`), matching FR-014. |
| Nesting | `SolidBegin`/`SolidEnd` blocks MAY nest to arbitrary depth (FR-006), **except**: a `"primitive"` block MUST NOT contain a nested `SolidBegin` (FR-019) — rejected with `error(CODE_BADTOKEN, ...)` identifying the "primitive is a CSG leaf" reason. |
| Content of a `"primitive"` block | Any RenderMan geometric primitive (`Sphere`, `Cone`, `PolygonMesh`, `PointsGeneralPolygons`, subdivision surfaces, patches, etc. — FR-015) MAY appear. `RiProcedural`/delayed-generator primitives MUST NOT appear directly inside a `"primitive"` block — rejected with `error(CODE_BADTOKEN, ...)` (research.md, resolved open question). |
| Content of a boolean block (`"union"`/`"intersection"`/`"difference"`) | MUST contain only nested `SolidBegin`/`SolidEnd` blocks as its immediate children (each an operand). Zero operands → resolves to no geometry (FR-016, not an error). One operand → resolves to that operand unchanged (FR-017). Two or more → resolved per FR-003/FR-004/FR-005, in declaration order (order is significant for `"difference"`). |
| Scope interaction | A `SolidBegin`/`SolidEnd` pair MAY be declared inside `RiObjectBegin`/`RiObjectEnd` (instancing definition) or inside `RiTransformBegin`/`RiTransformEnd`/`RiAttributeBegin`/`RiAttributeEnd`, and behaves exactly as ordinary geometry declared at that scope would (no special-casing — research.md Decision 1). |
| Output | A closed, outermost `SolidBegin`/`SolidEnd` block becomes exactly one renderable primitive in the scene (a Resolved Solid Boundary, `data-model.md`), indistinguishable from any other primitive to every downstream hider (FR-007/FR-008). |

## `Attribute "solid"`

New attribute namespace, following the existing four-layer attribute
pattern (`CLAUDE.md` Attributes system: token constants → RIB parsing →
`CAttributes` storage → `initDeclarations()` pre-declaration).

| Parameter | Type | Default | Contract |
|---|---|---|---|
| `"float tessellationtolerance"` | float | implementation-defined, derived from the primitive's own object-space bound diagonal (research.md Decision 4) | Scoped/inherited the same way any other attribute is (nested-scope override, per RISpec attribute-inheritance rules). Only consulted when resolving a `"primitive"` leaf's geometry into a triangle mesh for CSG boolean combination; has no effect on non-CSG geometry. Maps directly onto the same chordal-deviation ("flatness") tolerance already used by `CTesselationPatch`'s raytrace grid-splitting (`surface.cpp:1858-1897`, research.md Decision 4) — a smaller value tightens the allowed deviation between a facet and the true curved surface, so refinement continues longer and concentrates density where curvature is highest (a NURBS fold, a sphere's silhouette) rather than spending triangles uniformly. The attribute exists specifically because operand tessellation happens once, at geometry-build time, independent of final camera distance or `ShadingRate` (research.md Decision 4) — scene authors needing a tighter boundary on a specific solid set this explicitly rather than relying on render-time adaptivity that does not apply here. Has no effect on a subdivision-surface leaf's normal smoothness (research.md Decision 4b: subdivision operands mitigate faceting via denser subdivision level only, not analytic limit normals) — only on its polygon density. |

Declared via `initDeclarations()` (`src/ri/rendererDeclarations.cpp`), parsed
in `RiAttributeV()` (`src/ri/rendererContext.cpp`), stored in `CAttributes`
(`src/ri/attributes.h/cpp`) — following the same four-layer pattern as every
other attribute token, per `CLAUDE.md`.

## `Attribute "identifier" "string interior"` / `"string exterior"`

Not new — this feature does not add these tokens (they are pre-existing
RenderMan Interior/Exterior shader-assignment attributes already parsed and
stored by the existing `CAttributes` machinery per `attributes.cpp:229-238`).
This feature's contract obligation is *consumption*, not introduction:

| Contract | Detail |
|---|---|
| FR-010/FR-011/FR-012 | The shading pipeline MUST consult `attributes->interior` when a ray/camera sample is inside a Resolved Solid Boundary fragment's volume, `attributes->exterior` when outside, and fall back to ordinary surface/atmosphere shading when neither is set — but **only** for fragments belonging to a Resolved Solid Boundary. |
| FR-020 | Interior/Exterior assigned to attribute state that never enters a `SolidBegin`/`SolidEnd` block MUST remain a no-op, exactly as today (unchanged behavior for all existing non-CSG scenes — also required by FR-018/SC-005). |
| FR-009 | A composite's boundary fragments each carry their *originating leaf operand's* Interior/Exterior assignment (`data-model.md`, Boundary Fragment `attributes`), not a single value forced across the whole composite. |

## Error/diagnostic contract

All rejection cases in this feature use the existing `error()`/`warning()`
machinery with `CODE_BADTOKEN` (the established convention for invalid RIB
enum tokens and malformed block usage elsewhere in `src/ri/*.cpp` — see
e.g. `rendererContext.cpp:927,1120` for precedent), producing a message that
identifies *which* rule was violated (invalid operation string, unmatched
`SolidEnd`, nested block inside a `"primitive"` leaf, procedural inside a
solid block) rather than a generic failure — required by SC-006.

## Non-goals of this contract

- No new network/wire protocol — this is a single-process, in-process
  scene-description API extension, consistent with every other RIB
  primitive.
- No new CLI surface on `orender`/`oshader`/etc. — Principle IV (CLI
  Interface) is already satisfied by the existing `orender <rib>` entry
  point; RIB scenes are the interface.
