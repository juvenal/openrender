# Feature Specification: Solid CSG Operations

**Feature Branch**: `013-solid-csg-operations`

**Created**: 2026-08-25

**Status**: Draft

**Input**: User description: "The RenderMan Spec 3.2 at @docs/references/RISpec3_2.pdf defines "Solids and Spatial Sets Operations" in chapter 5.9, it sets the ground layer for Constructive Solid Geometry. openRender should implement support for the complete set of SolidBegin/SolidEnd definitions with all possible operations "primitive", "union", "intersection", and "difference", as well as Interior and Exterior shaders support."

## Clarifications

### Session 2026-08-26

- Q: When a composite solid combines operands that each carry their own Interior/Exterior shader assignment, which shader governs each resulting boundary face? → A: Face keeps its operand's shader — each boundary face inherits the Interior/Exterior shader from whichever operand originally contributed it.
- Q: Should Interior/Exterior shaders take effect only inside SolidBegin/SolidEnd blocks, or also on ordinary non-CSG geometry that has them assigned? → A: Solids only — assigning Interior/Exterior outside a solid block remains a no-op exactly as it behaves today.
- Q: Should a SolidBegin "primitive" block be allowed to contain a nested SolidBegin/SolidEnd block, or should that be rejected? → A: Reject with a clear diagnostic — "primitive" is a true leaf marker.
- Q: What performance expectation should guide the CSG boundary-resolution algorithm's design? → A: Correctness-first, no explicit target for v1.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Compose shapes with boolean solid operations (Priority: P1)

A scene author builds a composite shape by combining two or more simple primitives with a boolean set operation (union, intersection, or difference) instead of modeling the composite shape by hand. They wrap the operand primitives in a solid block declaring the operation, and the rendered image shows a single, correct resulting surface — not the individual overlapping primitives.

**Why this priority**: This is the foundational capability of the entire feature. Without correct boolean composition, there is no CSG support at all — every other scenario builds on this one.

**Independent Test**: Render a scene containing two overlapping primitives (e.g., a box and a sphere) wrapped first in a "union" solid block, then in "intersection", then in "difference", producing three separate renders. Each render can be independently inspected against the geometrically expected boundary (union = outer envelope, intersection = only the overlap, difference = box with the sphere's overlap carved out) with no seams, gaps, or duplicated surfaces at the boundary.

**Acceptance Scenarios**:

1. **Given** two overlapping primitives each declared inside a "primitive" solid block, nested inside an enclosing "union" solid block, **When** the scene is rendered, **Then** the image shows one continuous surface covering the full extent of both primitives, with no internal surface visible where they overlapped.
2. **Given** the same two primitives nested inside an "intersection" solid block, **When** the scene is rendered, **Then** the image shows only the region common to both primitives, bounded by the correct trimmed surfaces from each operand.
3. **Given** the same two primitives nested inside a "difference" solid block, **When** the scene is rendered, **Then** the image shows the first-declared primitive with the volume of the second-declared primitive removed, including the newly exposed interior-facing surface created by the cut.
4. **Given** a solid block that closes without any nested geometry or solid blocks, **When** the scene is rendered, **Then** no geometry, error, or crash results from that block.

---

### User Story 2 - Nest solid operations to build complex shapes (Priority: P2)

A scene author models a shape too complex for a single boolean operation by nesting solid blocks inside each other (for example, unioning two primitives and then subtracting a third from the result). The renderer resolves the entire nested tree to one correct final shape regardless of how deep the nesting goes or in what order the operations are combined.

**Why this priority**: Real modeling use cases rarely stop at one operation; nesting is what makes CSG useful for anything beyond trivial two-primitive examples. It depends on User Story 1's boolean operations already working correctly.

**Independent Test**: Render a scene with a solid tree at least four levels deep combining all three operation types (e.g., `difference( union( primitive, primitive ), intersection( primitive, primitive ) )`) and compare the resulting silhouette and cross-section against the manually reasoned expected shape.

**Acceptance Scenarios**:

1. **Given** a solid block whose operands are themselves solid blocks (not raw primitives), **When** the scene is rendered, **Then** the composite boundary reflects evaluating the inner solid blocks first and combining their results per the outer block's operation.
2. **Given** a "difference" solid block with three or more operands, **When** the scene is rendered, **Then** the result equals the first operand with every subsequent operand's volume removed, in declaration order.
3. **Given** a boolean solid block containing only a single nested operand, **When** the scene is rendered, **Then** the result equals that operand's boundary unchanged.

---

### User Story 3 - Shade a solid's interior differently from its exterior (Priority: P3)

A scene author assigns an Interior shader and/or an Exterior shader to a solid (for example, to make a carved block look like solid colored glass, or to give a cut-away object a distinct interior surface treatment). When the camera or a traced ray is inside the solid's resolved volume, the interior shader governs its appearance; when outside, the exterior shader (or ordinary shading, if none is assigned) governs it.

**Why this priority**: This is the feature's visual payoff for realism (colored glass, hollow objects with visibly different inner surfaces) but is only meaningful once a correct composite boundary already exists from User Stories 1 and 2.

**Independent Test**: Render a "difference" solid (e.g., a sphere with a smaller sphere subtracted from its center, exposing a concave interior) with an Interior shader assigned, viewed from an angle that reveals the cut-away interior. The interior-facing surface should visibly differ in appearance from the exterior-facing surface, and a version of the same scene with the interior/exterior camera position swapped should show the shading swap accordingly.

**Acceptance Scenarios**:

1. **Given** a solid with an Interior shader assigned, **When** the camera or a traced ray is positioned or travels inside the solid's resolved volume, **Then** the interior shader's visual effect is applied to the surfaces seen from inside.
2. **Given** a solid with an Exterior shader assigned, **When** the camera or a traced ray views the solid from outside its resolved volume, **Then** the exterior shader's visual effect is applied.
3. **Given** a solid with neither Interior nor Exterior shaders assigned, **When** the scene is rendered, **Then** it appears exactly as it would have without this feature — ordinary surface shading, no regression.
4. **Given** Interior/Exterior shaders assigned in an enclosing attribute scope, **When** a nested solid block does not override them, **Then** the nested solid inherits the enclosing scope's Interior/Exterior shaders, consistent with how other shader attributes are inherited.

---

### Edge Cases

- A "primitive" solid block containing more than one raw geometric primitive: all geometry inside the block is treated as a single opaque CSG leaf operand (its internal combination is ordinary scene geometry, not itself a boolean operation).
- A solid block declares an operation type other than "primitive", "union", "intersection", or "difference": the scene is rejected with a clear, actionable diagnostic rather than silently ignored or misinterpreted as one of the valid types.
- A SolidEnd appears without a matching SolidBegin, or solid blocks close in the wrong order relative to how they were opened: the scene is rejected with a clear diagnostic, consistent with how other RIB block-mismatch errors are reported.
- An operand primitive is not a closed, manifold surface (for example, a single open patch used directly as a CSG leaf): the resolved boundary in that region is best-effort rather than rejected outright, since all primitive types are valid operands.
- The same composite solid is rendered with the raytrace, REYES, and Z-buffer hiders: all three produce the same resolved shape, with no hider-specific difference in the boundary.
- A solid block is declared inside an object definition used for instancing: each instance resolves its own copy of the solid tree the same way any other instanced geometry is resolved, with no special-casing.
- A "primitive" solid block contains a nested SolidBegin/SolidEnd block instead of raw geometry: the scene is rejected with a clear diagnostic, since "primitive" is a CSG leaf and cannot itself hold a sub-tree.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: System MUST allow scene authors to open a solid block via SolidBegin with an explicit operation type of "primitive", "union", "intersection", or "difference", and close it with a matching SolidEnd.
- **FR-002**: System MUST treat geometry declared directly inside a "primitive" solid block as a single CSG operand usable as input to an enclosing boolean solid block.
- **FR-003**: System MUST resolve a "union" solid block's two or more nested operands to the boundary of their combined volume (everything inside any operand).
- **FR-004**: System MUST resolve an "intersection" solid block's two or more nested operands to the boundary of their common volume (only regions inside every operand).
- **FR-005**: System MUST resolve a "difference" solid block's two or more nested operands to the boundary of the first operand's volume with every subsequent operand's volume removed, applied in declaration order.
- **FR-006**: System MUST support solid blocks nested to arbitrary depth, resolving the composite boundary of the full nested tree as a single coherent shape.
- **FR-007**: System MUST resolve a solid tree's composite boundary once, as ordinary renderable geometry, independent of which hider will render the frame — visibility and shading for the result MUST use the same code paths as any other primitive, with no hider-specific or shading-stage-specific CSG logic.
- **FR-008**: System MUST render the resolved composite solid boundary identically regardless of whether the raytrace, REYES, or Z-buffer hider renders the frame.
- **FR-009**: System MUST allow scene authors to assign an Interior shader and/or an Exterior shader to the attribute state associated with a solid block, inherited the same way other shader attributes are inherited by nested scope. When operands within a composite solid carry different Interior/Exterior assignments, each resulting boundary face MUST retain the Interior/Exterior shader inherited from the specific operand that contributed it, not a single shader forced across the whole composite.
- **FR-010**: When a solid with an assigned Interior shader is viewed such that the camera or a traced ray is inside its resolved volume, System MUST apply the interior shader's visual effect instead of the default/exterior appearance.
- **FR-011**: When a solid with an assigned Exterior shader is viewed from outside its resolved volume, System MUST apply the exterior shader's visual effect.
- **FR-012**: If no Interior or Exterior shader is assigned to a solid, System MUST render it using ordinary surface/atmosphere shading exactly as it would render without this feature.
- **FR-013**: System MUST reject a SolidBegin call whose operation type is not one of "primitive", "union", "intersection", or "difference" with a clear, actionable error.
- **FR-014**: System MUST reject an unmatched or improperly nested SolidBegin/SolidEnd pair with a clear diagnostic.
- **FR-015**: System MUST accept any RenderMan geometric primitive type as an operand inside a "primitive" solid block.
- **FR-016**: An empty solid block (no nested geometry or solid blocks) MUST resolve to no geometry rather than causing an error or crash.
- **FR-017**: A boolean solid block ("union"/"intersection"/"difference") containing only a single nested operand MUST resolve to that operand's boundary unchanged.
- **FR-018**: System MUST NOT change the rendered output of existing scenes that do not use SolidBegin/SolidEnd.
- **FR-019**: System MUST reject a "primitive" solid block that contains a nested SolidBegin/SolidEnd block with a clear, actionable error, since a "primitive" block is a CSG leaf and cannot itself contain a sub-tree.
- **FR-020**: Interior/Exterior shader assignments MUST govern rendered appearance only for solids resolved from a SolidBegin/SolidEnd block; assigning Interior/Exterior to attribute state outside any solid block MUST remain a no-op, unchanged from current behavior.

### Key Entities

- **Solid Block**: A region of a scene delimited by SolidBegin/SolidEnd carrying an operation type ("primitive", "union", "intersection", or "difference"); may contain nested geometry (if "primitive") or nested solid blocks (if a boolean type).
- **CSG Tree**: The hierarchy formed by nested solid blocks — leaf ("primitive") nodes hold geometry, internal nodes hold a boolean operation over their children.
- **Resolved Solid Boundary**: The single composite surface produced by evaluating a CSG tree, consumed by hiders and the shading pipeline exactly like any other primitive's geometry.
- **Interior/Exterior Shader Assignment**: Inheritable attribute state determining which shader governs a solid's appearance depending on whether it is viewed from inside or outside its resolved volume.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Scene authors can combine two or more primitives with union, intersection, or difference and see a single correct resulting surface, with no visible seams, gaps, or duplicate/overlapping surfaces at the boundary.
- **SC-002**: The same CSG scene renders to the same resolved shape across the raytrace, REYES, and Z-buffer hiders, with cross-hider comparison showing no shape differences attributable to hider choice.
- **SC-003**: Composite solids nested at least four operation levels deep resolve to the correct final shape, matching the manually reasoned expected boundary for representative test scenes.
- **SC-004**: Scenes using Interior and/or Exterior shaders show visibly distinct appearance between the inside and outside of a solid.
- **SC-005**: Every existing scene that does not use SolidBegin/SolidEnd renders identically before and after this feature, with zero regressions in the existing visual regression test suite.
- **SC-006**: A scene that misuses solid blocks (invalid operation type, unmatched SolidEnd) produces a clear error message identifying the problem, rather than an unexplained crash or silently wrong image.

## Assumptions

- CSG boundary resolution is a geometry-domain operation performed once per solid tree, independent of which hider ultimately renders the frame; hiders and the shading pipeline consume the resolved boundary the same way they consume any other primitive's geometry, with no hider-specific CSG evaluation logic.
- All RenderMan-defined geometric primitive types are valid CSG leaf operands. Primitives that are not closed/manifold surfaces may produce best-effort boundary results in the affected region rather than being rejected outright.
- Motion-blurred (time-varying) solid operands are out of scope for this feature; solid trees are resolved from a single static geometric sample per the existing per-frame geometry pipeline.
- A "difference" block with more than two operands subtracts every operand after the first, in declaration order (first − second − third − ...).
- Solid blocks declared inside an object definition used for instancing are resolved per instance the same way any other instanced geometry is, with no special-casing.
- Interior/Exterior shaders extend the existing inheritable, atmosphere-class shader attribute mechanism already used for other shader assignments, rather than introducing a new shader class or attribute layer.
- No explicit performance or scale target is set for CSG boundary resolution in this feature; the implementation prioritizes correctness over speed for v1, to be revisited later if profiling shows a real problem on representative scenes.
