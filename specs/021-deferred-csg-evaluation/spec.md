# Feature Specification: Deferred, On-Demand Evaluation of Solid (CSG) Geometry

**Feature Branch**: `021-deferred-csg-evaluation`

**Created**: 2026-09-30

**Status**: Draft

**Supersedes**: Spec 013 (`013-solid-csg-operations`) behavior of resolving a solid's composite surface once, up front, when its solid block closes (013 FR-007 and FR-022), 013's rejection of procedural primitives as solid operands (013 FR-015's exception and FR-021), 013's dropping of empty nested blocks (replaced by FR-025a), 013's best-effort treatment of open, non-manifold operands (013 Edge Cases and Assumptions; replaced by FR-021's closedness rules), and 013 FR-008's "identically" across hiders, which FR-015 restates as agreement within the existing cross-method tolerances. All other spec 013 guarantees remain in force.

**Input**: User description: "Deferred, on-demand evaluation of constructive solid geometry (SolidBegin/SolidEnd solids). Solids composed with union/intersection/difference render with jagged, spiky seams, thin cracks and polygonal openings where operands meet, identically in every visibility method, because the composed shape is fixed into a fixed-precision surface when the solid block closes. A composed solid must stay its high-level definition for the whole render; its composite surface is only worked out where and when rendering needs it, at a precision fit for the view, as the last step before appearance is computed. Geometry, appearance (shading) and visibility (hiding) must stay independent concerns."

## Clarifications

### Session 2026-09-30

- Q: When an operand isn't a closed shape on its own (e.g. an uncapped cylinder), how should it take part in a boolean operation? → A: Treat it as the closed solid it implies — open ends and partial-sweep gaps are capped with flat faces, silently (no notice).
- Q: How much slower than today may solid scenes render? → A: Same order of magnitude — under 10× today's render time on every existing solid test scene; correctness first, optimization later.
- Q: Does the seam guarantee apply to every operand type or only analytic shapes? → A: No cracks for every operand type; within-1-pixel seam accuracy for analytic shapes (quadrics, flat polygons); free-form surfaces (patches, NURBS, subdivision) must be as accurate at the seam as they already render outside a solid.
- Q: Should procedural (deferred-expansion) primitives be allowed as solid operands now that evaluation is deferred? → A: Yes — allowed in this feature, expanded only when the solid is first needed by rendering; spec 013's rejection diagnostic (013 FR-021) is retired.
- Q: What should happen to spec 013's `Attribute "solid" "tessellationtolerance"` setting? → A: Keep accepting it without error; it has no effect; print a one-time notice per render saying so.
- Q (raised during planning): Which open operands are auto-closed? → A: Only operands consisting of exactly one quadric. Multi-surface operands are the volume their surfaces jointly enclose, closed by the author with no auto-caps, which avoids doubling author caps. A lone zero-volume surface is an empty operand. A lone open free-form surface gets a one-time "not closed" diagnostic.
- Q (raised during analysis): How are closedness, overlaps and direct leaves defined, and how are seam accuracy and work bounds measured? → A:
  - **Closedness** is a defined geometric test, and a lone non-quadric operand that fails it contributes nothing while emitting the diagnostic (FR-021).
  - **Overlapping shapes, and primitives placed directly in a boolean block**, combine by parity (Edge Cases).
  - **Free-form seam accuracy** is restated measurably: crack-free and consistent across methods everywhere, and within 1 pixel of the exact intersection where the free-form shape exactly represents an analytic one (FR-007a).
  - **Work bounds** are measured through the renderer's statistics (FR-003, US4, Edge Cases).
- Q (raised during analysis, round 3): How does an empty operand combine with the others? → A: As the empty set, in its position (FR-025a). This covers an empty nested block, a procedural that expands to nothing, a lone disk, a lone open non-quadric surface, and shapes that cancel by parity. It changes nothing in a union or as a later operand of a difference, and makes an intersection, or a difference whose first operand it is, yield nothing. Bilinear patches count as analytic shapes for seam accuracy (FR-007), and "render" means one frame (Assumptions).
- Q (raised during analysis, round 4): What happens to existing scenes whose operands FR-021 now treats as open, and how is a torus cut across its tube closed? → A: FR-021 stands; spec 013's best-effort treatment of open operands is superseded, and a reference whose result FR-021 or FR-025a changes is regenerated with its reason recorded (SC-005). The existing `csg-nurbs-sphere-union-raytrace` scene, whose NURBS operand is a single open sheet, becomes the open-sheet regression case. A torus cut across its tube is closed by the cone-shaped surface joining its two cut circles.
- Q (raised during analysis, round 5): Does a solid that reduces to a single open quadric get implied caps, and what happens to implicit and DSO ("dlo") operands, whose own rendering cannot be trimmed? → A: A whole solid that collapses to one operand is a passthrough with no caps (FR-019); implied closure applies only inside a boolean. Implicit and DSO operands are not supported as solid operands: dropped with one warning per offending operand per frame (FR-018). Closedness is judged per declared primitive, not per internal piece (FR-021).
- Q (raised during analysis, round 6): Do empty operands count as "another operand" for implied closure, which other primitive types cannot take part in a solid, and may procedurals sit directly in a boolean block? → A: Empty operands that change nothing are removed first, so `difference(openCylinder, empty)` is a passthrough with no caps (FR-019, FR-021). Points, curves and object instances join implicit and DSO primitives as unsupported operands (FR-018). Procedurals placed directly in a boolean block are accepted and join its first operand (FR-024). None of an empty or open operand's surfaces ever renders (FR-025a).

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
5. **Given** malformed solid blocks (unknown operation name, unmatched or improperly nested blocks, a block nested inside a "primitive" block), **When** parsed, **Then** the same diagnostics are reported as before.
6. **Given** an empty solid block, or a boolean block with a single operand, **When** rendered, **Then** the empty block yields nothing and the single operand renders unchanged.
7. **Given** a "primitive" solid block containing a procedural primitive (for example, a delayed-read archive holding a sphere), **When** the solid is rendered, **Then** the procedural's expanded geometry takes part in the boolean operation exactly as if it had been written directly in the block, and it is expanded no earlier than the first time rendering needs that solid.

---

### User Story 4 - Solids only cost work where they are actually seen (Priority: P3)

A scene author places many solids in a large scene, most of them off-screen, hidden or small. The renderer spends refinement effort only on the solids and the parts of solids that actually contribute to the image (or to rays actually traced), and only at the precision the view needs.

**Why this priority**: This is the efficiency half of the motivation. It matters for larger scenes but does not affect the correctness of the P1 fix.

**Independent Test**: Render a scene containing a solid entirely outside the camera view and confirm, via the renderer's statistics, that no surface-refinement work was performed for it. Render the hero scene at two resolutions and confirm refinement effort scales with the view rather than being fixed up front.

**Acceptance Scenarios**:

1. **Given** a solid located entirely outside the camera view and never reached by any traced ray, **When** the frame renders, **Then** no surface-refinement work is performed for that solid.
2. **Given** a solid that is partially visible, **When** the frame renders, **Then** refinement work is concentrated on the visible parts and on the seams within them: with only half of the solid in frame, seam refinement work is at most 75% of what the fully framed solid requires, as reported by the renderer's statistics.
3. **Given** a solid block that has just been closed, **When** no rendering has yet needed it, **Then** the renderer holds only the solid's definition (its operations and original shapes), not a pre-computed detailed surface.

---

### Edge Cases

- **Open operand shapes**: a single-quadric operand that is not closed on its own, such as the hero scene's uncapped cylinder or a partial-sweep quadric, is treated as its implied closed solid, with its open ends and sweep gaps capped by flat faces (a torus cut across its tube is closed by the cone-shaped surface joining its two cut circles); no notice is printed. A multi-surface operand is what its surfaces jointly enclose, with no auto-caps. A lone non-quadric surface that is not closed contributes nothing and emits a "not closed" diagnostic (FR-021).
- **Overlapping shapes in one "primitive" block**: where closed shapes declared in the same "primitive" block overlap, the overlap counts as outside, because it is enclosed twice. Overlapping shapes are combined with a "union" block. Spec 013 left this case undefined.
- **A solid with a single operand**: when a whole solid reduces to one operand (for example `SolidBegin "difference"` around a single open `Cylinder`), the operand renders exactly as authored, with no implied caps (FR-019).
- **Operands the solid cannot trim**: `Geometry "implicit"`, `Geometry "dlo"`, `Points`, `Curves` and `ObjectInstance` inside a solid are not supported: they are dropped with a warning and the operand they belonged to is empty if nothing else remains (FR-018, FR-025a).
- **Primitives placed directly in a boolean block**: primitives declared directly inside a "union", "intersection" or "difference" block, without a "primitive" block around them, together form one operand, positioned first (as in spec 013). They combine like a "primitive" block, so overlaps among them count as outside.
- **Empty operands**: an operand that encloses no volume (an empty nested block, a procedural that expands to nothing, a lone disk, a lone open non-quadric surface, or shapes that cancel by parity) behaves as the empty set in its position (FR-025a). Unlike spec 013, an empty nested block is not silently dropped, so subtracting from or intersecting with it yields nothing.
- **Coincident faces**: operands whose faces exactly coincide (two boxes sharing a face in a union, a subtracted box flush with a face of the other) produce a clean result: no leftover internal skin, no flicker between faces, no pinholes along the shared face.
- **Containment**: one operand lying entirely inside or entirely outside another produces the correct result (for example, subtracting a shape entirely outside the first leaves the first unchanged).
- **Tangency**: operands that only touch at a point or along a curve produce no stray slivers or holes at the touching region.
- **Camera or ray origin inside a solid**: interior behavior follows spec 013 (Interior shader applies; surfaces facing the viewer from inside are shaded correctly).
- **Very small or distant solids** render without artifacts. Seam refinement stops at pixel precision, so a solid covering under 4×4 pixels performs at most 16 seam-refinement steps, as reported by the renderer's statistics.
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
- **FR-003**: System MUST choose the precision of a solid's composite surface from the current view, refining where the surface and its seams are seen in detail and not refining where they are not. Measurably: the same solid rendered at a higher resolution performs more seam-refinement work than at a lower one, as reported by the renderer's statistics.
- **FR-004**: System MUST perform no surface-refinement work for a solid, or a part of a solid, that never contributes to the image and is never reached by a traced ray.
- **FR-005**: System MUST decide where a solid can be seen or hit, and where it needs refinement, using the solid's own information (its operations, its operands and their extents), without depending on how any particular visibility method works internally.

**Seam quality**

- **FR-006**: System MUST render seams between operands free of cracks, gaps and slivers at any image resolution, for every accepted operand type.
- **FR-007**: Where every operand meeting at a seam is an analytic shape (quadrics, flat polygons and bilinear patches), System MUST render the seam within one pixel of the exact intersection of the operand shapes at the rendered resolution, with no visible facets, spikes or spurs.
- **FR-007a**: Where a free-form operand (bicubic patch, patch mesh, NURBS, subdivision surface) meets another operand at a seam:
  - the seam MUST be crack-free, and consistent across visibility methods to within 1 pixel;
  - where the free-form operand exactly represents an analytic shape (for example, a rational NURBS sphere or a planar bicubic patch), the seam MUST lie within 1 pixel of the exact intersection;
  - elsewhere, the free-form operand MUST render as it does outside any solid.

  This keeps the clarified "as accurate as it renders on its own" intent, stated so it can be measured.
- **FR-008**: System MUST keep seams continuous across the boundaries between separately processed regions of the image.
- **FR-009**: System MUST NOT let seam accuracy degrade as image resolution increases: the seam stays within the same pixel bound (FR-007, FR-007a) at every resolution, so its error measured in scene units shrinks as resolution grows.

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
- **FR-018**: System MUST keep every operand type spec 013 accepts as a valid operand, except primitives whose own rendering cannot be trimmed by the solid, or that enclose no volume: `Geometry "implicit"`, `Geometry "dlo"`, `Points`, `Curves`, and `ObjectInstance` (which spec 013 also rejected). Inside a solid these are not supported: they contribute nothing, are not rendered, and produce one warning per offending operand per frame.
- **FR-019**: System MUST report the same diagnostics as spec 013 for malformed blocks (013 FR-013, FR-014, FR-019); an empty block MUST yield nothing and a single-operand boolean block MUST yield that operand unchanged (spec 013 FR-016, FR-017). When a whole solid collapses to a single operand, after removing empty operands that change nothing (FR-025a), that operand is a passthrough: it renders as authored, with no implied closure or caps (FR-021 applies only to operands that take part in a boolean with another non-empty operand). A single-operand block nested inside a boolean still takes part in that boolean. Spec 013's rejection of procedural operands (013 FR-021) is retired by FR-024.
- **FR-020**: Scenes that use no solid blocks MUST render exactly as before this feature.

**Operands and settings**

- **FR-021**: Within a boolean with at least one other non-empty operand, an operand consisting of exactly one quadric that is not closed on its own (for example, a cylinder without end caps, a z-cut sphere, or a quadric swept through less than a full turn) MUST behave as the closed solid it implies: its open ends and sweep gaps are treated as capped by flat faces (a torus cut across its tube, i.e. with a partial cross-section angle, is instead closed by the cone-shaped surface joining its two cut circles), so that scenes like the hero scene produce the intuitively expected result. Where a cap lies on the final composite boundary, it MUST appear as a real surface with the operand's shader. No notice or warning is emitted for auto-closed operands. An operand made of several surfaces is the volume those surfaces jointly enclose, and is not auto-capped: the author closes it, as in spec 013, so author-provided caps are never doubled. Closedness rules:
  - A quadric is closed only when it is a full sphere or a full torus; every other single quadric is auto-closed as above, except a disk, which encloses no volume and is silently empty.
  - A single non-quadric surface or mesh (one declared primitive, however many pieces it is internally made of) is closed when every one of its boundary edges either collapses to a point or coincides with another of its boundary edges, or, for polygon and subdivision meshes, when every edge is shared by exactly two faces.
  - A single non-quadric surface or mesh that is not closed (for example, one open NURBS sheet or one bilinear patch) contributes nothing (an empty operand) and MUST produce one diagnostic per offending operand per frame stating that the operand is not closed; further instances of the same solid do not repeat it.
  - A closed single surface, such as a single-patch NURBS sphere, produces no diagnostic.
- **FR-022**: Where operand faces coincide exactly, System MUST produce a clean, deterministic result: no leftover internal skin, no flicker between the coincident faces, no pinholes along them.
- **FR-023**: The per-primitive "solid" tessellation tolerance setting introduced by spec 013 MUST remain accepted in scene files without error, but MUST have no effect on the rendered result; seam precision is governed solely by the view (FR-003, FR-007, FR-007a). The first time the setting is encountered in a render, a single notice MUST tell the author that it no longer affects results; later occurrences in the same render MUST NOT repeat the notice.
- **FR-024**: System MUST accept procedural (deferred-expansion) primitives inside solid blocks: in a "primitive" block, or placed directly in a boolean block, where they join the first operand like any other direct leaf. Everything a procedural expands to belongs to the operand of the block it was declared in, exactly as if written there directly. A procedural operand MUST NOT be expanded before rendering first needs its solid, and MUST NOT be expanded at all if its solid never contributes to the image or to any traced ray. Each procedural operand is expanded at most once per solid instance.
- **FR-025**: If a procedural operand's expansion itself opens a solid block, System MUST report the same diagnostic as a solid block nested inside a "primitive" block (spec 013 FR-019), and ignore that nested block. A procedural operand that expands to nothing contributes an empty operand (FR-025a).
- **FR-025a**: An empty operand (an empty nested block, a procedural that expands to nothing, a lone disk, a lone open non-quadric surface, or shapes that cancel by parity) MUST behave as the empty set in its position: it changes nothing in a union or as a later operand of a difference, and makes an intersection, or a difference whose first operand it is, yield nothing. None of an empty operand's own surfaces is ever rendered. A solid block that is empty as a whole still yields nothing (FR-019).

### Key Entities

- **Solid definition**: a named composition of one or more operands under an operation (primitive, union, intersection, difference), possibly nested. It is retained unchanged from block close until the frame ends and is shared by every instance of it.
- **Operand**: an original shape (quadric, patch, patch mesh, NURBS, subdivision surface, polygon mesh, etc.), or a procedural that expands into such shapes on demand, as the author declared it, with its own attributes and shaders. It is never replaced by an approximation of itself.
- **Composite surface region**: the part of an operand's surface that belongs to the final solid's boundary, together with the direction it faces (flipped for walls exposed by subtraction). It is determined on demand, only where and when rendering needs it.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The hero scene renders with 0 stray pixels inside the clear see-through region of the hole, at both 800×500 and 1600×1000, in all three visibility methods (baseline today: 213 at 1600×1000 raytrace, 63 REYES, 38 Z-buffer).
- **SC-002**: In the hero and sphere-minus-cube scenes, every seam lies within 1 pixel of the exact intersection curve at both 800×500 and 1600×1000, in all three visibility methods. This is measured by projecting the exact intersection curves through the scene camera and checking the rendered seam edge against them; spikes, spurs and polygon facets show up as deviations. 4× zoom crops are kept as supporting evidence.
- **SC-003**: The hero and sphere-minus-cube scenes show 0 see-through gaps (pixels where the background shows through a surface that should be solid) at both resolutions, in all three visibility methods.
- **SC-004**: For each solid test scene that exists in more than one visibility method, the images of those methods agree within the renderer's existing cross-method tolerances.
- **SC-005**: 100% of the existing solid test scenes and solid-related tests pass (with reference images regenerated only where seams improve, or where FR-021 or FR-025a requires a different result, each recorded with its reason), and 100% of the existing non-solid visual regression scenes are unchanged.
- **SC-006**: A solid entirely outside the view (and never reached by a traced ray) triggers 0 surface-refinement operations and 0 procedural expansions, as reported by the renderer's statistics.
- **SC-007**: Render time for every existing solid test scene (and the hero scene) stays under 10× its current baseline, measured on the same machine and build configuration (hero scene today: raytrace 0.31 s at 800×500 / 1.42 s at 1600×1000; REYES 0.19 s / 0.40 s; Z-buffer 0.16 s / 0.34 s). Correctness takes priority; performance optimization beyond this bound is a follow-up.
- **SC-008**: The region of an operand untouched by the boolean operation (for example, the far side of the hero sphere) matches a render of the same shape without any solid block, within existing rendering tolerances.
- **SC-009**: Project documentation describing solids is updated to reflect deferred evaluation, the retired role of the tessellation tolerance setting, the treatment of open operands, and the acceptance of procedural operands (constitution Principle VII).
- **SC-010**: A solid built from a procedural operand (for example, a delayed-read archive containing a sphere, subtracted by a cylinder) renders identically, within existing rendering tolerances, to the same solid with the sphere written directly in the scene.

## Assumptions

- **A procedural operand's declared bound encloses everything it expands to**, as the RenderMan standard requires of every procedural; geometry outside the declared bound may be missed, exactly as for procedurals outside solids.
- **Moving (motion-blurred) operands** keep whatever behavior they have today; motion-blurred CSG is out of scope.
- **Operand shapes, their transforms and attributes are fixed** once the solid block closes (standard RenderMan scene-description semantics).
- **Existing cross-method agreement tolerances** (the parity thresholds already used by the renderer's test suite) are the yardstick for "consistent across visibility methods"; no new tolerance is introduced.
- **Seam accuracy is measured against the exact analytic intersection** for scenes whose operands are analytic shapes (spheres, cylinders, boxes made of flat or bilinear faces), which covers both reference scenes. For free-form operands, accuracy follows FR-007a: within 1 pixel of the exact intersection where the surface exactly represents an analytic shape; elsewhere crack-free, consistent across visibility methods within 1 pixel, and rendering as it does outside any solid.
- **A "render" is one frame** (one `WorldBegin`/`WorldEnd`). "Once per render" notices and diagnostics (FR-021, FR-023) are therefore once per frame.
