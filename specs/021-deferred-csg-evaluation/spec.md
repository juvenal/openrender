# Feature Specification: Deferred, On-Demand Evaluation of Solid (CSG) Geometry

**Feature Branch**: `021-deferred-csg-evaluation`

**Created**: 2026-09-30

**Status**: Draft

**Supersedes**: Spec 013 (`013-solid-csg-operations`) behavior of resolving a solid's composite surface once, up front, when its solid block closes (013 FR-007 and FR-022). All other spec 013 guarantees remain in force.

**Input**: User description: "Deferred, on-demand evaluation of constructive solid geometry (SolidBegin/SolidEnd solids). Solids composed with union/intersection/difference render with jagged, spiky seams, thin cracks and polygonal openings where operands meet, identically in every visibility method, because the composed shape is fixed into a fixed-precision surface when the solid block closes. A composed solid must stay its high-level definition for the whole render; its composite surface is only worked out where and when rendering needs it, at a precision fit for the view, as the last step before appearance is computed. Geometry, appearance (shading) and visibility (hiding) must stay independent concerns."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Boolean solids render with crack-free, smooth seams (Priority: P1)

A scene author carves, joins or intersects shapes with solid blocks and gets a clean result: where two shapes meet, the seam is a smooth, continuous curve with no cracks, slivers, spikes or visible polygon facets, regardless of image resolution or how closely the camera looks at it.

The reference case is the maintainer's "hero" scene: a sphere of radius 1.5 with a cylinder of radius 0.65 (spanning -2 to 2 along its axis, an open tube with no end caps that extends past the sphere on both sides) subtracted from it, viewed straight down the resulting hole at 800×500. Today, at 1600×1000, the ring where the sphere meets the hole wall shows about 16 triangular spikes, a thin lit crack arc appears inside the far opening (213 stray pixels inside the region that should be clear see-through), and both openings look polygonal. A second reference is the stock sphere-minus-cube test scene, whose cut boundaries today are coarse, irregular polylines with spurs.

**Why this priority**: This is the defect that motivated the feature. Visible seam artifacts make solid modeling unusable for any close-up or high-resolution render, and they cannot be worked around from the scene side.

**Independent Test**: Render the hero scene and the sphere-minus-cube scene at 800×500 and at 1600×1000; inspect the seams and measure stray pixels in regions that must be clear, and the deviation of each seam from the exact geometric intersection curve.

**Acceptance Scenarios**:

1. **Given** the hero scene, **When** it is rendered at 800×500 or 1600×1000, **Then** the hole's seam rings are smooth circles with no spikes, notches or facets, and the region seen through the hole contains no stray surface pixels.
2. **Given** the sphere-minus-cube scene, **When** it is rendered at 1600×1000, **Then** every cut boundary is a smooth curve following the true intersection of the sphere and the cube, with no spurs and no gaps between the sphere surface and the cut faces.
3. **Given** any solid scene rendered at a resolution and then at twice that resolution, **When** the seams are compared against the exact intersection curves, **Then** the higher-resolution image is at least as accurate (seam error measured in pixels does not grow).
4. **Given** a solid whose seams cross the boundaries between separately processed regions of the image, **When** it is rendered, **Then** the seams are continuous across those boundaries, with no breaks, offsets or duplicated edges.

---

### User Story 2 - The same solid looks the same in every visibility method (Priority: P2)

A scene author switches a solid scene between the raytrace, REYES and Z-buffer visibility methods and gets matching images, so choosing a visibility method for speed or features never changes what the solid looks like.

**Why this priority**: Cross-method consistency is an existing guarantee of spec 013 (FR-008) and of the renderer generally. It must survive the change in when and how solids are resolved.

**Independent Test**: Render the hero and sphere-minus-cube scenes with each of the three visibility methods and compare the images pairwise using the renderer's existing cross-method agreement tolerances.

**Acceptance Scenarios**:

1. **Given** a solid scene, **When** it is rendered with raytrace, REYES and Z-buffer, **Then** all three images agree within the renderer's existing cross-method tolerances and none of them shows seam defects.

---

### User Story 3 - Everything solids could already do keeps working (Priority: P2)

A scene author's existing solid scenes, including nested operations, per-operand shaders, Interior/Exterior shaders, instanced solids and malformed-block diagnostics, keep producing correct results with no changes to the scene files.

**Why this priority**: The feature changes the core of how solids are resolved. Every capability spec 013 delivered is at risk of silent regression and must be explicitly preserved.

**Independent Test**: Run the existing solid test scenes and solid-related tests; each renders with correct shape, correct shaders per face, correct Interior/Exterior behavior and the same diagnostics as before (seam quality improves).

**Acceptance Scenarios**:

1. **Given** a nested solid (for example, the union of a shape with the difference of two others, nested four levels deep), **When** rendered, **Then** the result is the correct composite shape.
2. **Given** a composite whose operands carry different surface shaders, **When** rendered, **Then** each part of the final surface shows the shader of the operand it came from, including the walls exposed by a subtraction.
3. **Given** a solid with Interior/Exterior shaders, **When** viewed from outside and from inside its volume, **Then** the exterior and interior shaders apply exactly as spec 013 requires.
4. **Given** a solid defined once as an object and instanced several times under different transforms, **When** rendered, **Then** every instance shows the correct composite shape with smooth seams.
5. **Given** malformed solid blocks (unknown operation name, unmatched or improperly nested blocks, a block nested inside a "primitive" block, a procedural inside a "primitive" block), **When** parsed, **Then** the same diagnostics are reported as before.
6. **Given** an empty solid block, or a boolean block with a single operand, **When** rendered, **Then** the empty block yields nothing and the single operand renders unchanged.

---

### User Story 4 - Solids only cost work where they are actually seen (Priority: P3)

A scene author places many solids in a large scene, most of them off-screen, hidden or small. The renderer spends refinement effort only on the solids and the parts of solids that actually contribute to the image (or to rays actually traced), and only at the precision the view needs.

**Why this priority**: This is the efficiency half of the motivation. It matters for larger scenes but does not affect the correctness of the P1 fix.

**Independent Test**: Render a scene containing a solid entirely outside the camera view and confirm, via the renderer's statistics, that no surface-refinement work was performed for it. Render the hero scene at two resolutions and confirm refinement effort scales with the view rather than being fixed up front.

**Acceptance Scenarios**:

1. **Given** a solid located entirely outside the camera view and never reached by any traced ray, **When** the frame renders, **Then** no surface-refinement work is performed for that solid.
2. **Given** a solid that is partially visible, **When** the frame renders, **Then** refinement work is concentrated on the visible parts and on the seams within them.
3. **Given** a solid block that has just been closed, **When** no rendering has yet needed it, **Then** the renderer holds only the solid's definition (its operations and original shapes), not a pre-computed detailed surface.

---

### Edge Cases

- **Open operand shapes**: an operand that is not a closed shape on its own, such as the hero scene's uncapped cylinder or a partial-sweep quadric, is treated as its implied closed solid (see Assumptions; to be confirmed in clarification).
- **Coincident faces**: operands whose faces exactly coincide (two boxes sharing a face in a union, a subtracted box flush with a face of the other) produce a clean result: no leftover internal skin, no flicker between faces, no pinholes along the shared face.
- **Containment**: one operand lying entirely inside or entirely outside another produces the correct result (for example, subtracting a shape entirely outside the first leaves the first unchanged).
- **Tangency**: operands that only touch at a point or along a curve produce no stray slivers or holes at the touching region.
- **Camera or ray origin inside a solid**: interior behavior follows spec 013 (Interior shader applies; surfaces facing the viewer from inside are shaded correctly).
- **Very small or distant solids** (smaller than a pixel) render without artifacts and without excessive work.
- **Solids partially outside the view**: the visible part renders correctly; the off-screen part costs no refinement work.
- **Translucent surfaces on operands**: surfaces seen through a translucent part of a solid (for example, the walls of a carved hole seen through a semi-transparent outer surface) appear correctly.
- **Surface displacement on an operand**: the solid's shape (which parts exist) is determined by the operands' undisplaced shapes; displacement then perturbs the surviving surface as it would on any shape.
- **Secondary rays** (shadows, reflections, refraction, occlusion) see exactly the same composite shape as camera visibility does.
- **Moving (motion-blurred) operands**: out of scope; behavior matches today's (see Assumptions).

## Requirements *(mandatory)*

### Functional Requirements

**Deferred, on-demand evaluation**

- **FR-001**: System MUST retain each solid as its high-level definition (its tree of operations over the original, unmodified operand shapes) from the close of its solid block through the end of the frame, instead of converting it into a pre-computed detailed surface when the block closes.
- **FR-002**: System MUST work out a solid's composite surface only where rendering needs it — the region of the image currently being processed, the ray currently being traced, or the whole scene when a visibility method processes the scene as a whole — and only at the moment it is needed, as the last step before that surface's appearance is computed.
- **FR-003**: System MUST choose the precision of a solid's composite surface from the current view, refining where the surface and its seams are seen in detail and not refining where they are not.
- **FR-004**: System MUST perform no surface-refinement work for a solid, or a part of a solid, that never contributes to the image and is never reached by a traced ray.
- **FR-005**: System MUST decide where a solid can be seen or hit, and where it needs refinement, using the solid's own information (its operations, its operands and their extents), without depending on how any particular visibility method works internally.

**Seam quality**

- **FR-006**: System MUST render seams between operands free of cracks, gaps and slivers at any image resolution.
- **FR-007**: System MUST render curved seams smooth to within one pixel of the exact intersection of the operand shapes at the rendered resolution, with no visible facets, spikes or spurs.
- **FR-008**: System MUST keep seams continuous across the boundaries between separately processed regions of the image.
- **FR-009**: System MUST NOT let seam accuracy degrade as image resolution increases.

**Separation of concerns**

- **FR-010**: Appearance computation MUST treat any part of a composed solid's surface exactly as it treats the operand's original shape outside any solid. A region of an operand that is not cut or joined by the boolean operation MUST look identical to the same shape rendered on its own, within existing rendering tolerances.
- **FR-011**: Surfaces exposed by an operation (for example, the inner walls of a hole carved by a subtraction) MUST face outward from the remaining material, so appearance computation shades them as ordinary outward-facing surfaces.
- **FR-012**: Visibility determination MUST contain no behavior specific to solids or boolean operations. It MUST only decide what is visible, what is occluded, and what is seen through translucent material. A visibility method that renders ordinary shapes correctly MUST render solids correctly without solid-specific additions.
- **FR-013**: Camera visibility and every kind of secondary ray (shadow, reflection, refraction, occlusion) MUST see the same composite shape.

**Preserved spec 013 behavior**

- **FR-014**: System MUST continue to support "primitive", "union", "intersection" and "difference" solid blocks with two or more operands, nested to arbitrary depth, with the same meaning as spec 013 FR-001 to FR-006.
- **FR-015**: System MUST render the same solid consistently across raytrace, REYES and Z-buffer visibility methods, within the renderer's existing cross-method agreement tolerances.
- **FR-016**: Each part of the final surface MUST keep the surface shader and the Interior/Exterior shader assignments of the operand it came from, with Interior/Exterior behavior exactly as spec 013 FR-009 to FR-012 and FR-020 define.
- **FR-017**: A solid defined inside an object definition and instanced MUST render correctly for every instance, each under its own transform.
- **FR-018**: System MUST keep every operand type spec 013 accepts as a valid operand.
- **FR-019**: System MUST report the same diagnostics as spec 013 for malformed blocks (FR-013, FR-014, FR-019, FR-021); an empty block MUST yield nothing and a single-operand boolean block MUST yield that operand unchanged (spec 013 FR-016, FR-017).
- **FR-020**: Scenes that use no solid blocks MUST render exactly as before this feature.

**Operands and settings**

- **FR-021**: An operand that is not a closed shape on its own (for example, a cylinder without end caps, or a quadric swept through less than a full turn) MUST behave as the closed solid it implies, capped across its open ends, so that scenes like the hero scene produce the intuitively expected result.
- **FR-022**: Where operand faces coincide exactly, System MUST produce a clean, deterministic result: no leftover internal skin, no flicker between the coincident faces, no pinholes along them.
- **FR-023**: The per-primitive "solid" tessellation tolerance setting introduced by spec 013 MUST remain accepted in scene files without error, but MUST no longer be required to obtain smooth seams. A one-time notice MUST tell the author that the setting no longer affects results.

### Key Entities

- **Solid definition**: a named composition of one or more operands under an operation (primitive, union, intersection, difference), possibly nested. It is retained unchanged from block close until the frame ends and is shared by every instance of it.
- **Operand**: an original shape (quadric, patch, patch mesh, NURBS, subdivision surface, polygon mesh, etc.) as the author declared it, with its own attributes and shaders. It is never replaced by an approximation of itself.
- **Composite surface region**: the part of an operand's surface that belongs to the final solid's boundary, together with the direction it faces (flipped for walls exposed by subtraction). It is determined on demand, only where and when rendering needs it.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The hero scene renders with 0 stray pixels inside the clear see-through region of the hole, at both 800×500 and 1600×1000, in all three visibility methods (baseline today: 213 at 1600×1000 raytrace, 63 REYES, 38 Z-buffer).
- **SC-002**: In the hero and sphere-minus-cube scenes, every seam lies within 1 pixel of the exact intersection curve at both 800×500 and 1600×1000, in all three visibility methods; no seam shows spikes, spurs or polygon facets under 4× zoom inspection.
- **SC-003**: The hero and sphere-minus-cube scenes show 0 see-through gaps (pixels where the background shows through a surface that should be solid) at both resolutions, in all three visibility methods.
- **SC-004**: For each solid test scene, the raytrace, REYES and Z-buffer images agree within the renderer's existing cross-method tolerances.
- **SC-005**: 100% of the existing solid test scenes and solid-related tests pass (with reference images regenerated only where seams improve), and 100% of the existing non-solid visual regression scenes are unchanged.
- **SC-006**: A solid entirely outside the view (and never reached by a traced ray) triggers 0 surface-refinement operations, as reported by the renderer's statistics.
- **SC-007**: Render time for the existing solid test scenes stays within the same order of magnitude as the current baseline: less than 10× (hero scene today: raytrace 0.31 s at 800×500 / 1.42 s at 1600×1000; REYES 0.19 s / 0.40 s; Z-buffer 0.16 s / 0.34 s). A tighter target may be set during clarification.
- **SC-008**: The region of an operand untouched by the boolean operation (for example, the far side of the hero sphere) matches a render of the same shape without any solid block, within existing rendering tolerances.
- **SC-009**: Project documentation describing solids is updated to reflect deferred evaluation, the retired role of the tessellation tolerance setting, and the treatment of open operands (constitution Principle VII).

## Assumptions

- **Open operands are implied closed solids (FR-021)**. This matches the hero scene author's intent and common CSG practice. The alternative, rejecting open operands with a diagnostic, would break the reference scene. *To be confirmed in /speckit.clarify.*
- **Tessellation tolerance setting is kept but inert (FR-023)**. Accepting it silently preserves existing scene files; a notice avoids confusion. *To be confirmed in /speckit.clarify.*
- **Procedural (deferred) primitives stay rejected as operands**, unchanged from spec 013 FR-021, even though deferred evaluation could make them possible. Lifting that restriction is a possible follow-up, not part of this feature. *To be confirmed in /speckit.clarify.*
- **Performance target is "same order of magnitude" (SC-007)**, keeping spec 013's correctness-first stance while bounding regressions. *To be confirmed or tightened in /speckit.clarify.*
- **Moving (motion-blurred) operands** keep whatever behavior they have today; motion-blurred CSG is out of scope.
- **Operand shapes, their transforms and attributes are fixed** once the solid block closes (standard RenderMan scene-description semantics).
- **Existing cross-method agreement tolerances** (the parity thresholds already used by the renderer's test suite) are the yardstick for "consistent across visibility methods"; no new tolerance is introduced.
- **Seam accuracy is measured against the exact analytic intersection** for scenes whose operands have closed-form shapes (spheres, cylinders, boxes), which covers both reference scenes.
