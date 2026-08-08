# Feature Specification: NURBS Trim Curves (RiTrimCurve)

**Feature Branch**: `009-nurbs-trim-curves`

**Created**: 2026-08-07

**Status**: Draft

**Input**: User description: "Add support for missing 'NURBS Trim Curves' (RiTrimCurve) to openRender, grounded in RenderMan Interface Specification 3.2.1 (docs/references/RISpec3_2.pdf), adjusted to this codebase's existing geometry/attribute implementation conventions. Must be purely additive: existing (untrimmed) NURBS geometry must render exactly as it does today."

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Cut a hole or boundary out of a NURBS surface (Priority: P1)

A scene author has modeled a NURBS surface (for example, a vase body) using `NuPatch` and wants to remove part of it — an opening at the rim, a window cut into a wall, a decorative notch — without re-authoring the underlying control-point mesh. They declare one or more trim curves in the surface's parameter space via `TrimCurve`/`RiTrimCurve` immediately before the `NuPatch` call, and expect the enclosed (or, per an explicit sense flag, excluded) region to simply not appear in the rendered image, while the rest of the surface renders exactly as if no trim curve had been declared.

**Why this priority**: This is the entire point of the feature — RenderMan Interface Specification 3.2.1 promotes trim curves from optional to a required capability specifically because they are the standard way to cut arbitrary openings and boundaries into NURBS surfaces without remodeling control points. Without this, every other story in this spec has nothing to operate on.

**Independent Test**: Render a scene containing a single `NuPatch` preceded by a `TrimCurve` loop that cuts a simple closed shape (e.g. a circular hole) out of the surface's parameter domain, using the reyes hider, and confirm the trimmed region is absent from the output while the untrimmed portion of the surface matches an equivalent scene with no trim curve applied outside the trimmed region.

**Acceptance Scenarios**:

1. **Given** a `NuPatch` surface and a preceding `TrimCurve` statement defining a single closed loop entirely within the surface's parameter domain, **When** the scene is rendered, **Then** the region enclosed by the loop is not rendered (no geometry, no shading) and the surface's silhouette shows the resulting opening.
2. **Given** the same scene, **When** compared to an equivalent scene where the same `NuPatch` is rendered with no `TrimCurve` declared, **Then** every point of the surface outside the trimmed region renders identically between the two scenes.
3. **Given** a `TrimCurve` loop whose curves are non-uniform rational (varying knot multiplicity, non-unit weights), **When** rendered, **Then** the loop's shape in parameter space is evaluated correctly rather than approximated as a uniform or polynomial curve.

---

### User Story 2 - Trim curves apply for as long as they remain the current attribute (Priority: P1)

A scene author wraps a `TrimCurve` + `NuPatch` pair inside `AttributeBegin`/`AttributeEnd` — the idiomatic way to scope a trim shape to one surface — and expects sibling `NuPatch` calls outside that block to render untrimmed. They also expect that if they instead declare a `TrimCurve` once and then issue several `NuPatch` calls in a row without an intervening `AttributeBegin`/`AttributeEnd` or a new `TrimCurve` call, all of those surfaces receive the same trim (matching how every other RenderMan attribute, such as `Color` or `Surface`, behaves), not just the first one.

**Why this priority**: `RiTrimCurve` is defined by the specification as attribute state ("part of the attribute state, and may be saved and restored using `RiAttributeBegin` and `RiAttributeEnd`"), not as a per-call argument to `NuPatch`. Getting this scoping model right is what determines whether the renderer's internal storage (`CAttributes`) and its existing push/pop semantics are the correct home for trim data — get it wrong and every other story's implementation sits on the wrong foundation.

**Independent Test**: Render a scene with `AttributeBegin` / `TrimCurve` / `NuPatch` / `AttributeEnd` followed by a second, sibling `NuPatch` with no trim curve declared, and confirm only the first surface is trimmed. Separately, render a scene with a single `TrimCurve` call followed by two consecutive `NuPatch` calls with no attribute block or second `TrimCurve` between them, and confirm both surfaces are trimmed identically.

**Acceptance Scenarios**:

1. **Given** `TrimCurve` and `NuPatch` issued inside an `AttributeBegin`/`AttributeEnd` block, **When** a sibling `NuPatch` is issued after `AttributeEnd` with no new `TrimCurve` call, **Then** the sibling surface renders fully untrimmed.
2. **Given** a `TrimCurve` call followed by two `NuPatch` calls with no intervening attribute scope change, **When** rendered, **Then** both surfaces are trimmed using the same trim loops.
3. **Given** a `TrimCurve` call with an empty loop list (`ncurves` of length zero), **When** issued after a prior `TrimCurve` call in the same attribute scope, **Then** subsequent `NuPatch` calls render untrimmed, giving scene authors an explicit way to stop trimming without opening a new attribute block.

---

### User Story 3 - Invert which side of a trim loop is kept (Priority: P2)

A scene author has already authored trim loops that describe a shape's boundary and wants to render only the material *inside* that boundary (for example, cutting a decal-shaped patch out of a larger sheet) rather than the default behavior of cutting the interior *away*. They set `Attribute "trimcurve" "sense" ["outside"]` without changing the trim curve geometry itself, and the kept/discarded regions invert.

**Why this priority**: This is a documented, spec-defined attribute (`"trimcurve"`/`"sense"`, default `"inside"`) that directly controls the visible result of every trimmed surface; without it, half of the specification's documented trimming behavior is unimplemented and scene authors have no way to keep the enclosed region instead of the surrounding one.

**Independent Test**: Render the same `TrimCurve` + `NuPatch` scene twice, once with the default sense and once with `Attribute "trimcurve" "sense" ["outside"]` set beforehand, and confirm the kept and discarded regions are exact complements of each other.

**Acceptance Scenarios**:

1. **Given** a trim loop and no explicit `"trimcurve"`/`"sense"` attribute set, **When** rendered, **Then** the region enclosed by the loop is discarded (default `"inside"` behavior).
2. **Given** the same trim loop with `Attribute "trimcurve" "sense" ["outside"]` set beforehand, **When** rendered, **Then** the region enclosed by the loop is the only part kept, and everything outside it is discarded.
3. **Given** the `"trimcurve"`/`"sense"` attribute is set and then a new `AttributeBegin`/`AttributeEnd` scope is opened and closed without re-setting it, **When** a `NuPatch` is rendered outside that nested scope, **Then** the previously-set sense value is still in effect (ordinary attribute push/pop behavior).

---

### User Story 4 - Untrimmed NURBS rendering is completely unaffected (Priority: P1)

A renderer maintainer or pipeline that already relies on `NuPatch` (for example, the existing `geometry/vase.rib` scene, or any third-party RIB) renders their untrimmed scenes after this feature lands and expects pixel-identical output to before the feature existed, with no performance regression, since no trim curve was ever declared for those surfaces.

**Why this priority**: This is the explicit, non-negotiable constraint of the feature request — trim curve support must be purely additive. There is currently no automated proof that untrimmed `NuPatch` rendering is unaffected by any change in this area (no visual-regression coverage of `NuPatch` exists today), so this story is both a requirement and the mechanism for proving every other story's implementation didn't disturb existing behavior.

**Independent Test**: Capture a visual-regression reference image for an existing untrimmed `NuPatch` scene (e.g. a promoted version of `geometry/vase.rib`) on unmodified `master`, before any trim curve code is written. After the feature lands, re-render the same scene and confirm the output matches the pre-feature reference image within the existing visual-regression tolerance.

**Acceptance Scenarios**:

1. **Given** an untrimmed-`NuPatch` reference image captured before this feature's implementation began, **When** the same scene is re-rendered after the feature is complete, **Then** the output matches within the project's existing visual-regression thresholds.
2. **Given** the existing 33+-scene visual-regression suite, **When** run after this feature lands, **Then** every previously-passing scene continues to pass unchanged.
3. **Given** a scene that never issues `TrimCurve`, **When** rendered, **Then** no trim-related evaluation work is performed beyond a cheap "no trim state set" check.

---

### User Story 5 - Multiple loops describe islands and holes correctly (Priority: P3)

A scene author defines more than one trim loop for a single surface — for example, an outer boundary loop plus a separate inner "island" loop representing a hole within the kept region, or several disjoint holes scattered across the surface — and expects the combination to resolve correctly (holes are cut, islands within holes are restored, and so on) the way overlapping shapes resolve in standard 2D boundary-fill rules.

**Why this priority**: Real trimmed models (mechanical parts, architectural cutouts, vase rims with multiple perforations) routinely use more than one loop per surface; a design that only handles a single loop is not usable for realistic content, but it is lower priority than getting the single-loop case and the attribute-scoping model correct first.

**Independent Test**: Render a `NuPatch` with three trim loops — one outer hole and two additional disjoint holes elsewhere on the surface — and confirm all three holes appear independently in the output with the rest of the surface intact.

**Acceptance Scenarios**:

1. **Given** two disjoint trim loops on the same surface, **When** rendered, **Then** both enclosed regions are discarded independently and the rest of the surface is unaffected by either loop individually.
2. **Given** one trim loop nested entirely inside another (an island within a hole), **When** rendered, **Then** the region between the two loops is discarded and the innermost region is kept, consistent with an odd-crossing-count classification rule.

---

### Edge Cases

- What happens when a `TrimCurve`'s control curves reference `(u,v)` coordinates outside the surface's actual parameter range (the full knot-vector span, since `umin`/`umax`/`vmin`/`vmax` clamping remains unimplemented independent of this feature)? Coordinates outside the domain MUST NOT crash the renderer; the out-of-range portion of the curve is treated as outside the surface and has no effect beyond the domain boundary.
- What happens when a trim loop is self-intersecting or its authored curve direction disagrees with the odd-crossing-count classification (a malformed scene per the specification's own "trim curves are approximations" caveat)? The renderer's classification result (see FR-005) is authoritative; no error is raised, consistent with the specification's own warning that trimming is approximate.
- What happens when `TrimCurve` is declared but the following primitive in scope is not a `NuPatch` (e.g. a `Sphere` or `Patch`)? The pending trim attribute state is simply not consulted by any primitive type other than `NuPatch`; it is not an error and does not affect the other primitive.
- What happens when a trim loop's curves are not closed (head-to-tail continuity is broken)? This is a malformed scene per the specification; the renderer's per-loop closure MUST NOT crash — an incomplete loop is closed implicitly by connecting its last point back to its first, matching the specification's requirement that loops be explicitly closed, and any resulting visual artifact is the scene author's responsibility.
- What happens to a trim loop when the surface it targets is itself part of motion blur (moving geometry)? Trim curves are static, parameter-space data; they apply identically to every motion sample of the same `NuPatch`, since the trim shape is defined in `(u,v)` space, not world space.
- What happens for a scene that already uses `-writerib`/`ribOut` to round-trip a RIB file containing `TrimCurve` statements? The output RIB MUST continue to contain an equivalent `TrimCurve` statement, since round-trip serialization already works today and must not regress.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The renderer MUST accept `TrimCurve`/`RiTrimCurve` statements and store the described trim loops (per-curve order, knot vector, parameter range, homogeneous control points) as renderer state, rather than reporting `CODE_INCAPABLE` as it does today.
- **FR-002**: Trim curve state MUST be stored as part of the renderer's graphics-state attribute stack, following the same save/restore semantics as every other attribute: pushed and deep-copied on `AttributeBegin`, restored on `AttributeEnd`.
- **FR-003**: A `TrimCurve` call MUST replace any previously-set trim curve state in the current attribute scope; a `TrimCurve` call with zero loops MUST clear trim curve state in the current attribute scope, causing subsequent `NuPatch` calls to render untrimmed until a new `TrimCurve` is declared or a new attribute scope is entered.
- **FR-004**: When a `NuPatch` is issued while no trim curve state is set in the current attribute scope, the renderer MUST produce identical tessellation, shading, and output to its pre-feature behavior — no new code path may be exercised for untrimmed surfaces beyond a single cheap check that no trim state is present.
- **FR-005**: When a `NuPatch` is issued while trim curve state IS set, the renderer MUST classify each candidate point of the surface's `(u,v)` domain as inside or outside the union of trim loops using an odd-crossing-count (ray-casting) rule evaluated against the loops' curves in parameter space; this rule is authoritative for rendering purposes even where the specification's separate curve-orientation ("left of the curve") convention would disagree for a malformed loop.
- **FR-006**: The renderer MUST support an implementation-specific attribute, `Attribute "trimcurve" "sense" [<string>]`, accepting `"inside"` (default) or `"outside"`, controlling whether the region enclosed by trim loops is discarded (`"inside"`) or kept (`"outside"`) for the current attribute scope.
- **FR-007**: The `"trimcurve"`/`"sense"` attribute MUST be implemented via the renderer's existing four-layer attribute pattern: a declared token, `RiAttributeV` parsing, `CAttributes` storage, and mandatory pre-declaration in `initDeclarations()`.
- **FR-008**: Trim curve `(u,v)` coordinates MUST be interpreted relative to the full parameter domain of the `NuPatch`'s knot vectors (the existing per-Bezier-span `uOrg`/`uMult` convention's combined mesh-wide range), since the `umin`/`umax`/`vmin`/`vmax` `NuPatch` arguments remain unimplemented independent of this feature; this feature MUST NOT implement `umin`/`umax`/`vmin`/`vmax` clamping as part of its scope.
- **FR-009**: Because a `NuPatch` mesh is internally split into one child surface per non-degenerate Bezier span, trim classification MUST use the mesh's global knot range rather than any individual span's local parameter range, so that trimming is correct for spans away from the mesh's origin span.
- **FR-010**: Trim rejection MUST integrate into the renderer's existing shared tessellation/dicing path used by all patch-type surfaces (bilinear, bicubic, NURBS): micropolygon grids entirely outside all retained trim regions MUST be culled before dicing continues, and individual micropolygon vertices outside the retained region MUST be excluded from the diced grid's output.
- **FR-011**: Trim boundary classification in v1 MUST be a binary accept/reject test per micropolygon vertex; antialiased or partial-coverage treatment of trim edges is out of scope for this feature.
- **FR-012**: Trim curve support in v1 applies to the shared bucket-rasterization (reyes-family) tessellation path only. When the ray-tracing hider encounters a `NuPatch` with active trim curve state, it MUST render the surface untrimmed and MUST emit a diagnostic warning identifying trim curves as unsupported for that hider, rather than silently ignoring the trim state without any indication.
- **FR-013**: Any new heap-owned fields added to `CAttributes` to hold trim loop data MUST be deep-copied in `CAttributes`'s copy constructor and released in its destructor, matching the existing pattern for every other heap-owned attribute field, to avoid a use-after-free on `AttributeEnd`.
- **FR-014**: The existing RIB-output round-trip path (`CRibOut::RiTrimCurve`), which already correctly serializes `TrimCurve` data, MUST continue to produce correct RIB output for any internal storage representation this feature introduces.
- **FR-015**: Existing language bindings that already expose `TrimCurve` for RIB serialization (Python, Lua) MUST continue to function without signature changes.
- **FR-016**: The tracked gap entry for NURBS Trim Curves in the project's implementation-gaps documentation MUST be updated to reference the correct implementation location and marked resolved once this feature lands; the project's living status documentation MUST be updated accordingly.

### Key Entities

- **Trim Loop**: One or more homogeneous rational B-spline curves in `(u,v,w)` parameter space, connected head-to-tail into a single closed boundary within a `NuPatch` surface's parameter domain.
- **Trim Curve Attribute State**: The renderer's current, possibly-empty set of trim loops for the active attribute scope; ordinary graphics-state attribute data, saved and restored by `AttributeBegin`/`AttributeEnd` like color, shaders, or other surface attributes.
- **Trim Sense**: The `"trimcurve"`/`"sense"` attribute value (`"inside"` or `"outside"`) determining whether the region enclosed by trim loops is discarded or kept.
- **Retained Region**: The portion of a `NuPatch`'s parameter domain that survives trim classification and is diced, shaded, and rendered normally.
- **Untrimmed-NuPatch Regression Baseline**: A visual-regression scene and reference image, captured before this feature's implementation begins, whose continued pass/fail status is the primary evidence that existing NURBS rendering is unaffected.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A scene author can remove an arbitrarily-shaped region from a `NuPatch` surface using `TrimCurve` and see that region absent from the rendered image, demonstrated by at least one new visual-regression scene with a captured reference image.
- **SC-002**: 100% of the existing 33+-scene visual-regression suite, plus a newly added untrimmed-`NuPatch` baseline scene captured before implementation begins, continue to pass within existing thresholds after this feature lands.
- **SC-003**: A scene author can invert which side of a trim loop is kept using only the `"trimcurve"`/`"sense"` attribute, without modifying the trim curve's control data, verified by a scene rendered both ways.
- **SC-004**: Trim curve state scoped inside an `AttributeBegin`/`AttributeEnd` block does not affect sibling `NuPatch` calls outside that block, verified by a scene containing both a trimmed and an untrimmed surface.
- **SC-005**: A surface with multiple trim loops (disjoint holes, or a nested island-within-a-hole) resolves correctly in a single render, verified by a dedicated multi-loop test scene.
- **SC-006**: RIB files containing `TrimCurve` statements continue to round-trip correctly through the renderer's existing RIB-output path after this feature lands.
- **SC-007**: Rendering time for scenes with no trim curve state set does not measurably regress relative to pre-feature baseline, since untrimmed rendering follows the same code path as before.

## Assumptions

- Trim coordinates are interpreted against the full mesh-wide knot-vector parameter range. Implementing the currently-unimplemented `umin`/`umax`/`vmin`/`vmax` `NuPatch` clamping parameters is explicitly out of scope for this feature and is tracked as a separate, pre-existing limitation; doing so here would risk changing rendering of existing scenes that already pass non-full knot ranges, violating the additive-only constraint.
- Trim curve support is scoped to the shared reyes-family bucket-rasterization tessellation path (used by the stochastic/REYES and z-buffer hiders) in v1. The ray-tracing hider renders trimmed `NuPatch` surfaces untrimmed, with a diagnostic warning, and gaining trim support there is left to a future spec.
- Trim boundary evaluation is binary (per-micropolygon-vertex accept/reject) in v1; antialiased or partial-coverage trim edges are deferred to a future iteration, consistent with the specification's own note that trim curves are inherently approximate.
- Inside/outside classification uses the odd-crossing-count (ray-casting) rule as the sole runtime algorithm; the specification's curve-orientation convention is treated as authoring guidance, not as a second rule enforced or cross-checked at render time.
- There is currently no visual-regression coverage of `NuPatch` at all. This feature's work explicitly includes capturing an untrimmed baseline (likely derived from the existing `geometry/vase.rib` scene) on unmodified code before any trim implementation lands, as the mechanism for proving non-interference.
- Existing Python and Lua bindings that already serialize `TrimCurve` calls to RIB require no changes, since this feature only changes what the renderer does with trim data after parsing, not the RIB grammar or call signatures themselves.
