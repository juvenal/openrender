# Feature Specification: Blobby Implicit Surfaces

**Feature Branch**: `015-blobby-implicit-surfaces`

**Created**: 2026-08-28

**Status**: Draft

**Input**: User description: "Implement the RiBlobby blobby implicit-surface geometric primitive as defined in RenderMan Interface Specification 3.2 chapter 5.6 ('Blobby Implicit Surfaces'), cross-referenced against Pixar's PhotoRealistic RenderMan Application Note #31 (September 1999). Blobby is a standard RISpec gprim that openRender currently declares throughout its stack but leaves entirely unimplemented. Geometry generation MUST be entirely independent of the hider subsystem, carried forward in intent from spec 013 (Solid CSG Operations). Extensions introduced in PhotoRealistic RenderMan versions after the 3.2 specification are explicitly deferred to a later refinement step."

## Clarifications

### Session 2026-08-28

- Q: How large a blobby must v1 render acceptably, and is there a speed target? → A: Correctness-first with no wall-clock target, but the published 480-segment toroidal spiral is a required visual-regression scene that must complete in the suite — which rules out an extraction approach whose cost scales with bounding-box volume rather than with the surface.
- Q: For distributed rendering, should each server re-derive the blobby surface from its description, or receive pre-derived geometry? → A: Each server re-derives from the re-emitted `Blobby` declaration. FR-022's "exactly once" is scoped per renderer process, and derivation must be deterministic so that every server produces identical geometry from identical input.
- Q: How should tests establish that a blobby renders correctly, not just repeatably? → A: Analytic ground truth first — cases whose exact surface is known in closed form are asserted against computed geometry — with the published example scenes layered on top as ordinary frozen-reference regression scenes. Reference images alone are not accepted as evidence of correctness.
- Q: How should per-blob values blend when fields are combined by operations other than addition? → A: Each combining operation blends its operands' values the same way it blends their fields — add and multiply split proportionally, maximum and minimum hand the value to the winning operand, and a negated or subtracted operand contributes no value. Value blending therefore always agrees with shape blending.
- Q: How should the surface threshold value be settled? → A: Derived by calibration against the published example scenes whose intended appearance is documented, with the derivation recorded, rather than adopting the commonly cited value on faith. It remains fixed and not author-settable.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Model an organic shape from self-blending blobs (Priority: P1)

A scene author models an amorphous, organic shape — a cluster of droplets, a molecular
structure, a tubby character limb — by placing a handful of simple ellipsoidal fields
near one another and summing them, instead of hand-modelling the fillets and round
joins where the parts meet. The renderer produces one continuous surface whose parts
flow smoothly into each other exactly where the fields reinforce one another, and stay
separate where they do not. The same scene renders to the same shape whichever hider
the author selects.

**Why this priority**: This is the entire reason the primitive exists — automatic
blending with no explicitly modelled join geometry. Without a correct level surface
computed from summed ellipsoid fields there is no blobby support at all, and every
other story builds on it. It is also the minimum viable slice: an author who has only
this can already model the classic blobby-molecule shapes.

**Independent Test**: Render a scene containing two ellipsoid fields summed together,
first placed far apart, then close enough to influence one another, then close enough
to merge into a single component. Each render can be inspected on its own against the
expected level surface — two separate rounded shapes, two shapes drawn toward one
another, and one merged shape with a smooth waist. Repeat the merged case under each
camera hider (REYES, z-buffer, ray-trace) and confirm the silhouettes agree.

**Acceptance Scenarios**:

1. **Given** a Blobby statement declaring two ellipsoid fields whose regions of
   influence do not overlap, combined with the add operation, **When** the scene is
   rendered, **Then** the image shows two separate closed rounded surfaces with no
   join between them.
2. **Given** the same two fields moved close enough that their regions of influence
   overlap substantially, **When** the scene is rendered, **Then** the image shows a
   single continuous surface with a smooth blended waist joining the two lobes, and no
   crease, seam, or visible intersection curve where they meet.
3. **Given** a Blobby whose fields are combined with the maximum operation instead of
   add, **When** the scene is rendered, **Then** the image shows the unblended union
   of the individual surfaces — the parts touch but do not swell into one another.
4. **Given** any of the above scenes, **When** it is rendered once under each camera
   hider without changing anything else in the scene, **Then** every render shows the
   same resolved shape, within the tolerance the existing visual-regression comparison
   already applies to other primitives.
5. **Given** a Blobby whose declared surface lies entirely outside the view frustum,
   **When** the scene is rendered, **Then** it is culled like any other primitive and
   contributes no visible geometry and no error.

---

### User Story 2 - Control blending precisely with the full operation set (Priority: P2)

A scene author needs finer control than "everything blends with everything". They group
their blobs — for example, summing the blobs of each finger together along with a few
adjacent palm blobs, summing all the palm blobs separately, then taking the maximum of
those five groups — so that fingers blend into the palm but webs never grow between
adjacent fingers. They also subtract a small field from a large one to dent it, and
stretch the subtracted field thin to punch a hole clean through.

**Why this priority**: Selective blending is what makes the primitive usable for
anything beyond a blob of spheres; the published reference model for the primitive (the
space-alien hand) cannot be expressed without it. It depends on User Story 1's field
evaluation and level-surface extraction already being correct.

**Independent Test**: Render the published reference hand model three ways — with no
blending, with every ellipsoid summed together, and with the selective grouped-sums
combined by a maximum — and confirm the third shows fingers merging into the palm with
no webs between the fingers. Separately, render a large field with a small field
subtracted from it and confirm a dent, then elongate the subtracted field and confirm a
clean through-hole.

**Acceptance Scenarios**:

1. **Given** a Blobby code array containing several add instructions whose results are
   then combined by a maximum instruction, **When** the scene is rendered, **Then**
   blobs within the same add group blend smoothly with each other while blobs in
   different groups meet without blending.
2. **Given** a Blobby that subtracts one field from another, **When** the scene is
   rendered, **Then** the subtracted field's region of influence is carved out of the
   surface, producing a dent or, if the subtracted field spans the whole body, a
   through-hole.
3. **Given** a Blobby using the multiply, minimum, negate, or identity operations,
   **When** the scene is rendered, **Then** the resulting field is the arithmetic
   combination those operations define, and the identity operation leaves its operand's
   field unchanged.
4. **Given** a Blobby whose code array uses opcodes 4 and 5, **When** the scene is
   rendered with the default interpretation, **Then** they behave as the RISpec 3.2
   table defines them (4 = subtract, 5 = divide).
5. **Given** the same scene rendered with the compatibility interpretation selected at
   scene level, **When** the scene is rendered, **Then** opcodes 4 and 5 behave as
   Pixar's Application Note defines them (4 = divide, 5 = subtract), so RIB authored
   against PhotoRealistic RenderMan renders correctly without being edited.

---

### User Story 3 - Build tubular shapes from segment blobs (Priority: P2)

A scene author models a tube, a tentacle, a rope, or a spiral — anything whose skeleton
is a piecewise-linear path — by placing segment fields along that path and summing them,
rather than approximating the tube with a long chain of overlapping spheres. Adjacent
segments join without bulges at the joints or seams along the length.

**Why this priority**: Segment fields are the primitive's second modelling shape and the
only practical way to build tubular geometry with it; a 480-segment toroidal spiral is a
published reference use. It is independent of User Story 2 — an author can use segments
with nothing but the add operation — but it is less foundational than ellipsoids.

**Independent Test**: Render a chain of segment fields laid end to end along a curved
path and summed, and inspect the surface for constant apparent thickness along its
length, absence of bulges at the joints between consecutive segments, and rounded caps
at the two free ends.

**Acceptance Scenarios**:

1. **Given** a Blobby containing a single segment field, **When** the scene is
   rendered, **Then** the image shows a cylinder of the declared radius about the
   declared endpoints, with hemispherical rounded ends.
2. **Given** several segment fields laid end to end and summed, **When** the scene is
   rendered, **Then** consecutive segments join into one smooth tube with no bulge at
   the shared endpoints and no seam along the length.
3. **Given** a segment field whose two endpoints coincide, **When** the scene is
   rendered, **Then** the result is a sphere of the declared radius rather than a
   degenerate surface, an error, or a crash.
4. **Given** a Blobby mixing segment fields, ellipsoid fields, and a constant field in
   one code array, **When** the scene is rendered, **Then** all three field types
   contribute to the same combined level surface.

---

### User Story 4 - Give each blob its own shading values (Priority: P3)

A scene author assigns a different surface colour — or any other shading value — to each
individual blob in a cluster, and expects those values to bleed into one another exactly
where the blobs blend, and to stay distinct where the blobs do not blend. The published
reference image is six coloured spheres summed at the vertices of an octahedron, each
carrying its own colour, with the colours mixing smoothly across every blend region.

**Why this priority**: This is what makes the primitive expressive rather than merely
geometric, and it is explicitly part of the specification's parameter-passing contract.
It is only meaningful once a correct blended surface exists, so it follows User Story 1.

**Independent Test**: Render the six-blob coloured octahedron and confirm each blob
shows its declared colour at its own centre and a smooth mixture of neighbouring colours
across every blend region. Re-render the same six blobs pulled far enough apart that
they no longer blend and confirm each shows only its own colour with no bleed.

**Acceptance Scenarios**:

1. **Given** a Blobby with a value supplied once per blob for a varying or vertex
   parameter, **When** the scene is rendered, **Then** each blob's own value dominates
   near that blob's centre.
2. **Given** two blobs carrying different values that blend into one another, **When**
   the scene is rendered, **Then** the value across the blend region is a smooth
   mixture weighted by how much each blob's field contributes at that point, with no
   discontinuity anywhere on the surface.
3. **Given** a Blobby with a parameter of storage class constant or uniform, **When**
   the scene is rendered, **Then** its single value applies uniformly to the whole
   primitive with no per-blob variation.
4. **Given** two groups of coloured blobs that blend within each group but are combined
   with one another by the maximum operation, so the groups meet without merging,
   **When** the scene is rendered, **Then** colours mix freely inside each group and do
   not bleed across the boundary between the groups, matching what the shapes do.
5. **Given** a blob whose field is subtracted from another to carve a dent, **When** the
   scene is rendered, **Then** the subtracted blob's own colour does not tint the carved
   surface; the surface keeps the colour of the blobs that remain.
6. **Given** a Blobby whose code array contains combining instructions, **When** per-blob
   parameter values are supplied, **Then** the author supplies values only for the
   primitive fields and never for the combining instructions, and the renderer derives
   the combining instructions' values rather than requiring or accepting them.

---

### User Story 5 - Keep a solid texture attached to a bending blob chain (Priority: P3)

A scene author applies a solid texture — wood, marble, stripes, a 3D checker — to a
chain of blobs, then bends or coils the chain. The texture stays anchored to the
surface and travels with it instead of the surface sliding through a fixed world-space
texture, because each blob carries its own mapping back into a shared reference space
and those mappings blend across the joins just as the geometry does.

**Why this priority**: Solid-texture adhesion on a deforming surface is the standard
production requirement for this primitive, but it presupposes both a correct blended
surface (User Story 1) and working per-blob parameter blending (User Story 4).

**Independent Test**: Render a straight chain of three blobs carrying reference-space
mappings equal to their own placements, confirm the solid checker mapped onto it is
undistorted, then translate the middle blob and confirm the checker on the middle blob
moves with it while the checks on the two outer blobs stay put, with the distortion
absorbed by the blend regions between them.

**Acceptance Scenarios**:

1. **Given** a Blobby whose blobs each carry a per-blob mapping into a reference
   coordinate system, **When** a shader reads that parameter, **Then** it receives a
   position value, not a matrix, computed by carrying the surface point back into that
   blob's own coordinate system and forward into the reference space.
2. **Given** a chain of blobs whose reference mappings match their placements exactly,
   **When** a solid texture is applied, **Then** the texture appears undistorted across
   the whole chain.
3. **Given** one blob in that chain displaced from its reference position, **When** the
   scene is rendered, **Then** the texture on that blob moves with it and the
   neighbouring blobs' texture stays anchored, with the mapping stretching only across
   the blend regions in between.

---

### User Story 6 - Trade surface fidelity against render cost (Priority: P3)

A scene author who sees faceting on a blobby filling the frame in a close-up shot tightens
a tolerance setting on that primitive and re-renders to get a smoother surface, and
loosens it on distant background blobbies to save time and memory. Authors who never
touch the setting still get a surface that looks correct.

**Why this priority**: Without an author-facing control, a scene that looks wrong in
close-up has no remedy at all. It is P3 rather than higher because the default must
already be good enough for the great majority of scenes; this is the escape hatch, not
the primary path.

**Independent Test**: Render one blobby filling the frame at the default tolerance, then
at a tighter setting, and confirm the tighter render shows a visibly smoother silhouette.
Render an existing scene with the setting absent and confirm it is unchanged from before
the setting existed.

**Acceptance Scenarios**:

1. **Given** a scene that never mentions the tolerance setting, **When** it is rendered,
   **Then** the blobby's surface appears smooth at typical framing without the author
   having configured anything.
2. **Given** the tolerance setting tightened on one primitive, **When** the scene is
   rendered, **Then** that primitive's silhouette is measurably smoother and no other
   primitive in the scene changes.
3. **Given** the setting declared in an enclosing attribute scope, **When** a nested
   primitive does not override it, **Then** the nested primitive inherits the enclosing
   value, consistent with how every other attribute is inherited.
4. **Given** a nonsensical value for the setting — zero, negative, or absurdly large —
   **When** the scene is rendered, **Then** the renderer reports a clear diagnostic and
   falls back to a usable value rather than hanging, exhausting memory, or crashing.

---

### User Story 7 - Repel a blobby off an irregular ground surface (Priority: P4)

A scene author animating liquid droplets wants them to interact with an irregular ground
surface — flattening and bulging as they approach it, hovering just above it, never
interpenetrating it — without modelling that interaction by hand. They add a repelling
ground plane derived from a previously rendered depth file and shape the repulsion with
four parameters controlling its height, sharpness, bulge position, and bulge strength.

**Why this priority**: This is the specification's most specialised field type, valuable
for one production scenario, and the only one that depends on reading an external file.
It is genuinely independent of every other story and can be delivered last without
holding anything back.

**Independent Test**: Render a single blob descending toward a repelling ground plane
built from a depth file of an irregular surface, at several heights, and confirm the blob
flattens and bulges as it nears the ground, hovers without interpenetrating, and is
unaffected once it is above the declared cut-off height. Vary each of the four shaping
parameters in isolation and confirm each changes the profile in the documented direction.

**Acceptance Scenarios**:

1. **Given** a Blobby containing a repelling ground plane whose depth file exists,
   **When** the scene is rendered, **Then** the blobby's surface is deflected away from
   the ground surface recorded in that file.
2. **Given** the blob positioned entirely higher above the ground than the declared
   cut-off height, **When** the scene is rendered, **Then** the repeller contributes
   nothing and the surface is identical to the same scene without it.
3. **Given** each of the four shaping parameters varied one at a time, **When** the
   scene is rendered, **Then** the overall height of influence, the sharpness of the
   barrier, the position of the bulge, and the height of the bulge each change
   independently and in the direction the specification describes.
4. **Given** a repelling ground plane naming a depth file that is missing, unreadable,
   or not a depth file, **When** the scene is rendered, **Then** the renderer reports a
   clear, actionable diagnostic naming the file and continues rendering the rest of the
   scene rather than aborting or crashing.

---

### User Story 8 - Blur a moving blobby (Priority: P4)

A scene author animates a blobby across a frame — a splashing droplet, a flung blob of
goo — inside a motion block, and the rendered image shows it smeared along its path
exactly as any other moving primitive in the same scene would be, using the same shutter
settings and the same quality controls.

**Why this priority**: Motion blur is a general renderer capability, not a blobby
feature, and this story exists to guarantee the new primitive participates in it rather
than silently ignoring motion blocks. It is P4 because a still blobby is already useful,
but it must not be dropped: shipping a primitive that silently ignores motion blocks
would be a correctness defect, not a missing nicety.

**Independent Test**: Render a scene containing a blobby and an ordinary primitive both
moving along the same path inside the same motion block, and confirm both are smeared
over the same extent with comparable quality. Render the same scene with the motion block
removed and confirm the blobby is sharp.

**Acceptance Scenarios**:

1. **Given** a blobby declared inside a motion block with two or more time samples,
   **When** the scene is rendered with a non-zero shutter, **Then** the blobby appears
   blurred along its motion path.
2. **Given** a blobby and an ordinary primitive undergoing identical motion in the same
   scene, **When** the scene is rendered, **Then** the two are blurred over the same
   extent with the same shutter response, with no blobby-specific difference.
3. **Given** a blobby whose motion samples change its field parameters without changing
   the surface's topology — a blob growing, shrinking, or moving while the surface keeps
   the same number of separate pieces and holes — **When** the scene is rendered,
   **Then** the blurred result reflects that changing shape rather than only a rigid
   displacement.
4. **Given** a blobby whose motion samples change the surface's topology over the
   shutter interval — two lobes merging into one, a piece splitting off, a hole opening
   or closing, or a blob vanishing entirely — **When** the scene is rendered, **Then**
   the renderer produces a bounded, non-crashing result and the limitation is documented,
   rather than producing torn geometry, invalid values, or a crash.
5. **Given** a blobby outside any motion block in a scene with a non-zero shutter,
   **When** the scene is rendered, **Then** it appears sharp with no spurious blur.

---

### Edge Cases

- A `Blobby` statement written in the three-array RIB form, with the strings array
  omitted entirely: accepted and rendered identically to the four-array form with an
  empty strings array, since both forms are valid RIB.
- A declared blob count that disagrees with the number of primitive fields actually
  present in the code array: reported as a clear diagnostic and handled within bounds.
  Pixar's own published reference model declares 21 blobs while emitting 22 ellipsoid
  instructions, so real-world RIB does contain this inconsistency and the renderer must
  survive it rather than reading past the end of a parameter array.
- An instruction whose operand refers to its own result or to the result of a later
  instruction: rejected with a clear diagnostic, since results may only be referenced
  after they are computed.
- An operand that indexes past the end of the floats array, the strings array, or the
  list of instruction results: rejected with a clear diagnostic rather than read.
- A code array that ends part-way through an instruction, or a variable-arity operation
  declaring more operands than the array can hold: rejected with a clear diagnostic.
- An unrecognised opcode, including anything in the range the specification reserves for
  future use: rejected with a diagnostic that names the opcode, rather than silently
  ignored or misinterpreted as a neighbouring opcode.
- A variable-arity operation declaring zero operands, or a negative operand count:
  rejected with a clear diagnostic.
- An empty code array, or a code array containing no primitive fields at all: yields no
  geometry, with no error and no crash.
- A code array whose combined field never reaches the surface threshold anywhere — every
  blob too small, or every field cancelled out: yields no geometry, with no error and no
  crash.
- The dual case: a code array whose combined field meets or exceeds the surface threshold
  *everywhere*, such as a lone constant field at or above the threshold, or a large
  constant added to every other field. There is no boundary to find, so the renderer must
  terminate promptly with no geometry and a clear diagnostic, rather than searching an
  unbounded region for a surface that does not exist.
- An ellipsoid whose transformation matrix is singular or degenerate, collapsing the unit
  sphere to a plane, a line, or a point: handled without producing infinities, invalid
  numbers, or a crash.
- A blobby placed inside a solid block as an operand of a boolean set operation: accepted
  and combined like any other closed primitive, since its surface is fully determined
  before the boolean operation is resolved.
- A blobby placed inside an object definition used for instancing: each instance is
  resolved like any other instanced geometry, with no special-casing.
- A scene rendered across a network of render servers, or written back out as RIB and
  re-rendered: the blobby survives the round trip and renders identically, rather than
  being dropped or reported as unimplemented.
- A blobby with several hundred blobs, such as the published 480-segment toroidal spiral:
  renders to completion as part of the regression suite. Because such a blobby occupies a
  large bounding box while its surface fills only a small fraction of it, the cost of
  finding the surface must be governed by the surface itself rather than by the volume of
  the bounding box.

## Requirements *(mandatory)*

### Functional Requirements

#### Accepting the primitive

- **FR-001**: The RIB parser MUST accept the `Blobby` statement in both of its valid
  forms — with an explicit strings array, and with the strings array omitted — and pass
  it through to the renderer. Today both forms are silently discarded by the parser
  before the renderer ever sees them.
- **FR-002**: The programmatic RenderMan interface entry points for the primitive MUST
  construct real geometry rather than reporting the primitive as unsupported.
- **FR-003**: Declaring a blobby MUST NOT emit any "not supported" or "not implemented"
  diagnostic under any supported rendering mode.
- **FR-004**: RIB output MUST emit a `Blobby` statement that reproduces the original
  declaration faithfully enough to render identically when read back, so that blobbies
  survive RIB round-tripping and distributed rendering across render servers. This is a
  correctness requirement, not a convenience: today the combination of an unimplemented
  RIB writer and an early return in the distributed path means a blobby is silently lost
  when a scene is rendered across servers.
- **FR-005**: The existing Python and Lua scene-description bindings MUST emit blobby
  declarations that the renderer accepts and renders, verified end to end.

#### Field description

- **FR-006**: Instructions in the code array MUST be numbered from zero in declaration
  order, and an instruction MUST be able to reference the result of any earlier
  instruction by that number. Self-references and forward references MUST be rejected
  with a clear diagnostic.
- **FR-007**: The renderer MUST support the constant primitive field, whose single
  operand names one value in the floats array.
- **FR-008**: The renderer MUST support the ellipsoid primitive field, whose operand
  names the first of sixteen values in the floats array forming the transformation that
  carries the unit sphere onto the ellipsoidal bump in the primitive's own coordinate
  system.
- **FR-009**: The renderer MUST support the segment primitive field, whose operand names
  the first of twenty-three values in the floats array giving the segment's two
  endpoints, its radius, and the transformation carrying it into the primitive's own
  coordinate system. The segment's field MUST be the convolution of a segment impulse
  with the same spherical bump the ellipsoid field uses, so that segments laid end to end
  join without bulges or seams.
- **FR-010**: The renderer MUST support the repelling ground plane primitive field, whose
  two operands name a depth file in the strings array and the first of four shaping
  values in the floats array. Its contribution MUST follow the reference formulation
  published with the specification: zero at or above the declared cut-off height, and
  otherwise a barrier term that falls off with the sharpness value combined with a bump
  term peaking at the declared bulge position with the declared bulge height, the whole
  eased smoothly to zero at the cut-off height, with the distance clamped away from zero
  so the barrier term stays finite.
- **FR-011**: The ellipsoid and segment fields MUST use the specification's spherical
  bump function, which is one at the centre, falls smoothly to zero at unit distance, and
  is exactly zero beyond unit distance, so that every primitive field has strictly
  bounded influence.
- **FR-012**: The renderer MUST support all eight combining operations the specification
  defines: add, multiply, maximum, and minimum, each taking a leading operand count
  followed by that many result references; subtract and divide, each taking exactly two
  result references; and negate and identity, each taking exactly one.
- **FR-013**: RISpec 3.2's and Pixar's Application Note's tables assign opcodes 4 and 5
  in opposite orders. The renderer MUST default to the RISpec 3.2 assignment (4 =
  subtract, 5 = divide) and MUST provide a scene-level option selecting the Application
  Note assignment (4 = divide, 5 = subtract) so that scene descriptions authored against
  PhotoRealistic RenderMan render correctly without being edited. Both assignments MUST
  be independently verifiable, and the conflict MUST be recorded as a documented erratum
  with citations to both sources rather than silently resolved one way. The option MUST
  be usable from a scene description without the author declaring it first.
- **FR-014**: Any opcode the specification does not define — including every value in the
  range it reserves for future use — MUST be rejected with a diagnostic naming the
  offending opcode and its position in the code array.
- **FR-015**: The rendered surface MUST be the level set of the combined field at a fixed
  threshold, matching the threshold PhotoRealistic RenderMan uses, so that scene
  descriptions written for that renderer produce the same shape here. Because neither
  primary source states that value, it MUST be established by calibrating against the
  published example scenes whose intended appearance is documented — the six-blob
  octahedron must resolve to one connected surface, and a pair of blobs described as
  unblended must resolve to two separate surfaces — and the derivation MUST be recorded
  alongside the value rather than the value being adopted on faith. The threshold is not
  author-configurable.

#### Per-blob values

- **FR-016**: A blob's index MUST be its ordinal position among the instructions that
  declare primitive fields, counting all four primitive field types including constant
  and repelling ground plane, because the specification defines a per-blob parameter as
  supplying one value for each such instruction.
- **FR-017**: A declared blob count that disagrees with the number of primitive fields
  actually present MUST produce a clear, actionable diagnostic and MUST NOT cause a read
  beyond the end of any supplied parameter array, an invalid value, or a crash.
- **FR-018**: Parameters of storage class constant or uniform MUST supply exactly one
  value for the whole primitive. Parameters of storage class varying or vertex MUST
  supply exactly one value per blob.
- **FR-019**: Per-blob parameter values MUST be blended at every point on the surface by
  carrying values up the code array alongside the fields, with each combining operation
  blending its operands' values the same way it blends their fields: add and multiply
  apportion the result among their operands in proportion to each operand's contribution;
  maximum and minimum pass through the value of whichever operand won at that point; and
  an operand that is negated, or that is the subtracted side of a subtraction, contributes
  no value. Value blending therefore agrees with shape blending everywhere — blobs kept
  from merging geometrically never bleed values into one another. Values for combining
  instructions MUST be derived by the renderer and MUST NOT be required from the author.
- **FR-019a**: Where an apportionment would divide by zero — every contributing operand
  evaluating to zero at a point — the renderer MUST fall back to a defined, continuous
  result rather than producing an invalid value or a visible discontinuity.
- **FR-020**: The renderer MUST support a per-blob parameter type whose declared value is
  a transformation carrying points from that blob's own coordinate system into a shared
  reference coordinate system, and which a shader reads as a position. The value at a
  surface point MUST be computed by carrying that point back into the blob's coordinate
  system and then forward into the reference space, so that a solid texture stays
  attached to the surface as the blobs move relative to one another.
- **FR-021**: Blobbies have no global surface parameterisation, as the specification
  states explicitly. The renderer MUST define what the standard surface parameter and
  texture coordinate variables evaluate to on a blobby, consistently with how
  subdivision surfaces — which have the same limitation — already behave, so that shaders
  bound to a blobby read defined values rather than uninitialised ones.

#### Geometry generation independent of the hider

- **FR-022**: Within any one renderer process, the surface MUST be derived from the field
  exactly once, in the geometry domain, before any hider begins work, and the resulting
  geometry MUST be the single representation every hider consumes. It MUST NOT be
  re-derived per hider, per bucket, per ray, or lazily during rendering. A distributed
  render is the one place the surface is derived more than once — once per participating
  server, each from the same re-emitted declaration — which FR-023a constrains.
- **FR-023**: A blobby MUST render to the same resolved shape under every camera hider the
  renderer offers, within the same comparison tolerance the visual regression suite
  already applies to other primitives. Adding a future hider of **any** kind — camera or
  otherwise — MUST require no blobby-specific work whatsoever; the agreement assertion is
  scoped to camera hiders only because non-camera hiders produce no comparable image, not
  because they are exempt from consuming the same geometry.
- **FR-023a**: Deriving the surface MUST be deterministic: the same declaration rendered
  with the same tolerance setting MUST produce identical geometry every time, on every
  machine, regardless of thread count or bucket order. Without this, a distributed render
  in which each server derives its own copy would show seams where geometry from
  different servers meets.
- **FR-024**: Shading normals on the resolved surface MUST be derived from the field's
  own gradient rather than from the faceting of the generated geometry, so that shading
  and silhouettes stay smooth at moderate tolerance settings instead of revealing the
  generated facet structure.
- **FR-025**: The renderer MUST provide an author-facing attribute controlling the
  tolerance of the generated surface, inheritable through attribute scope like every other
  attribute, with a default derived from the primitive's own extent so that scenes which
  never set it still render a smooth-looking surface. The attribute MUST be usable from
  RIB without the author declaring it first.
- **FR-026**: A blobby MUST participate in motion blur through the same mechanism every
  other primitive uses, with no blobby-specific handling in any hider, and MUST correctly
  blur topology-preserving shape changes over the shutter interval and not merely rigid
  displacement. Motion that changes the surface's topology within the shutter interval —
  pieces merging, splitting, or vanishing — MUST produce a bounded, non-crashing result
  and MUST be documented as a known limitation; blurring it faithfully is not required.
- **FR-027**: A blobby MUST be usable as an operand of a boolean solid set operation and
  MUST NOT be rejected by the guard that excludes primitives whose geometry does not
  exist until render time, since a blobby's surface is fully determined before any
  boolean operation is resolved.
- **FR-028**: A blobby MUST report an extent that fully contains its surface, so that it
  is culled, bucketed, and accelerated exactly like every other primitive and never
  clipped away while still visible.

#### Robustness and diagnostics

- **FR-029**: Every malformed code array the Edge Cases section identifies MUST produce a
  clear, actionable diagnostic naming the problem and its position, and MUST NOT cause an
  out-of-bounds read, an invalid numeric value, an unbounded loop, or a crash.
- **FR-030**: A blobby that yields no surface — no primitive fields, or a combined field
  that never reaches the threshold — MUST contribute no geometry, with no error and no
  crash.
- **FR-031**: Diagnostics MUST follow the renderer's existing conventions for reporting
  scene-description problems, so that a blobby error is no harder to locate in a large
  scene than any other primitive's error.

#### Documentation

- **FR-032**: The project documentation site MUST document the primitive and the new
  tolerance attribute alongside the feature — the code array format, every opcode and its
  operands, per-blob parameters, the opcode 4/5 erratum and the option that selects
  between the two readings, and worked examples — delivered with the feature rather than
  afterwards.

### Key Entities

- **Blobby primitive**: One geometric primitive in the scene, described by a declared
  blob count, a code array of instructions, a floats array, a strings array, and a
  parameter list. Placed and oriented by the current transformation like any other
  primitive.
- **Instruction**: One entry in the code array — an opcode plus its operands — numbered
  by its position, whose computed result later instructions may reference.
- **Primitive field (blob)**: An instruction that contributes a field directly: a
  constant, an ellipsoid, a segment, or a repelling ground plane. Its ordinal position
  among primitive fields is its blob index, which is how per-blob parameter values are
  matched to it.
- **Combining operation**: An instruction that computes a new field from the results of
  earlier instructions, and which correspondingly blends those instructions' per-blob
  parameter values.
- **Repeller**: A primitive field derived from an external depth file plus four shaping
  values, contributing a negative-going barrier with a bulge, used to push a surface away
  from irregular ground.
- **Per-blob parameter set**: The values supplied once per blob for a varying or vertex
  parameter, including the reference-space mapping type, blended across the surface in
  proportion to field contribution.
- **Resolved blobby geometry**: The concrete surface derived once from the field before
  rendering begins, carrying its own gradient-derived shading normals, its blended
  per-blob values, and its motion samples — the single representation every hider
  consumes.
- **Tolerance attribute**: The inheritable author-facing control over how finely the
  resolved surface approximates the true level set.
- **Camera hider**: A hider that resolves visibility for camera rays and produces the
  rendered image — currently the REYES, z-buffer, and ray-trace hiders. Cross-hider
  agreement is asserted over camera hiders **only**. The photon-map pass and the
  debug visualiser are **not** camera hiders: they do not produce a comparable
  rendered image, so comparing their output against a camera hider's is meaningless
  and MUST NOT be required. This distinction bounds every "under every hider" claim
  about *verification* below; it does not narrow the architectural requirement in
  FR-022, where the resolved geometry really is consumed by every hider without
  exception, camera or not.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: All four primitive field types and all eight combining operations are
  exercised by tests with independently hand-computed expected field values — 100% of the
  twelve opcodes RISpec 3.2 defines, with no opcode covered only indirectly.
- **SC-002**: Both readings of opcodes 4 and 5 are covered by separate tests that fail if
  either reading regresses, and the default reading is confirmed to be the RISpec 3.2 one.
- **SC-003**: Correctness is established analytically before any reference image is
  frozen. Every case whose exact surface is known in closed form is asserted against the
  generated geometry within a stated tolerance — a single ellipsoid produces that
  ellipsoid, a single segment produces a capsule of the declared radius about the
  declared endpoints, two coincident identical blobs produce a sphere of the analytically
  predicted larger radius, and gradient-derived normals agree with the analytic surface
  normal. A frozen reference image is never accepted as the sole evidence that a surface
  is correct.
- **SC-003a**: With that analytic base in place, every published reference model from the
  specification and the application note renders without error under each camera hider and is
  committed as a frozen regression reference: the six-blob coloured octahedron with
  colours blending, the selectively blended hand with fingers joining the palm and no
  webs between fingers, a multi-segment tube, a blob dented and then pierced by a
  subtracted blob, and a blob deflected by a repelling ground plane.
- **SC-004**: A blobby scene rendered under every camera hider produces images that
  agree within the same difference threshold the existing visual regression suite already
  applies, with no hider requiring a scene change to render the primitive.
- **SC-005**: A corpus of at least fifteen deliberately malformed blobby declarations —
  covering every malformed case in the Edge Cases section — produces a clear diagnostic in
  every case and zero crashes, hangs, or invalid numeric outputs.
- **SC-006**: A blobby at typical framing renders with no facet edges visible on its
  silhouette or in its shading at the default tolerance setting; a blobby filling the
  frame reaches the same standard once the setting is tightened, and tightening the
  setting measurably reduces silhouette deviation from the true level set.
- **SC-007**: The full existing visual regression suite passes unchanged, confirming no
  scene without a blobby is affected.
- **SC-008**: A blobby used as an operand inside a boolean solid block resolves into the
  composite shape correctly, verified against the same expectations the existing solid
  operation tests apply to other primitives.
- **SC-009**: A scene containing a blobby survives a RIB write-and-reread round trip and a
  distributed render across render servers, producing the same image in all three paths,
  with no seam visible where geometry derived by different servers meets. Deriving the
  same declaration twice — in separate processes, and at differing thread counts —
  produces identical geometry both times.
- **SC-010**: A moving blobby and an ordinary primitive undergoing identical motion in the
  same scene blur over the same extent, and the same blobby outside a motion block renders
  sharp.
- **SC-011**: Documentation for the primitive and the tolerance attribute is published on
  the project site, covering every opcode, per-blob parameters, and the opcode 4/5
  erratum, with worked examples an author can copy and render.
- **SC-012**: A blobby of approximately 500 segment fields — the published toroidal
  spiral — renders to completion as a member of the regression suite. No wall-clock
  target is set, but the run must demonstrate that surface-finding cost tracks the
  surface rather than the bounding-box volume: a sparse blobby of the same overall extent
  but far fewer blobs must not cost disproportionately more than its surface area
  warrants.

## Assumptions

- **Surface threshold**: Neither primary source states the numeric level at which the
  field defines the surface. Rather than adopting the commonly cited value on faith, it is
  derived during design by calibration against the published scenes whose intended
  appearance is documented, and the derivation is recorded with the value (FR-015). It is
  fixed rather than author-configurable, matching PhotoRealistic RenderMan's behaviour.
- **Opcode 4/5 default**: RISpec 3.2 is the normative source the feature targets, so its
  assignment is the default and the application note's is the opt-in compatibility mode.
  The application note is nonetheless treated as authoritative evidence of what
  PhotoRealistic RenderMan actually shipped, which is why the compatibility mode exists
  at all.
- **Blob count leniency**: Because Pixar's own published example declares a blob count
  that disagrees with its code array, a mismatch is treated as a recoverable diagnostic
  rather than a fatal scene error.
- **Depth files**: Repelling ground planes read the same depth file format the renderer
  already reads for shadow mapping; no new file format is introduced.
- **Coordinate systems**: The per-blob transformation matrices carry their shapes into the
  primitive's own coordinate system, and the primitive as a whole is then placed by the
  current transformation exactly like any other primitive.
- **Tolerance default**: The default tolerance is derived from each primitive's own extent
  so that a scene which never mentions the attribute still renders a smooth surface at
  typical framing; the attribute is the escape hatch for close-ups and distant background
  geometry, not the normal path.
- **Motion samples**: Motion blur is delivered by carrying motion samples on the resolved
  geometry so the renderer's existing, hider-independent motion machinery applies
  unchanged. That approach can represent a surface that changes shape over the shutter
  interval but not one that changes topology — pieces merging, splitting, or vanishing —
  which is why FR-026 and User Story 8 require only a bounded, documented result in the
  topology-changing case. Keeping the generated surface consistent across
  topology-preserving motion samples remains a design problem the planning phase must
  address explicitly.
- **Shading caveats**: The application note's warnings about this primitive stressing
  PhotoRealistic RenderMan — gritty shading under flat interpolation, poor results from
  normal-computation shading functions, dicing-rate concerns under orthographic
  projection — are recorded as known hazards to verify against, not as requirements to
  reproduce.

## Out of Scope

The following are deliberately excluded from this feature and deferred to a later
refinement specification:

- Every blobby extension introduced in PhotoRealistic RenderMan releases after the 3.2
  specification, including any opcode in the range the specification reserves for future
  use, plug-in supplied field types, and any combining operation beyond the eight defined
  here.
- Any change to the existing plug-in implicit-surface path, which remains exactly as it
  is. That path derives its surface inside the ray-tracing hider and has no equivalent for
  the scanline hiders, which is precisely the hider-coupled shape this feature's
  architectural constraint forbids; it is neither extended nor reused here.
- Author control over the surface threshold.
- Level-of-detail or view-dependent tolerance selection beyond the single inheritable
  attribute described above.
