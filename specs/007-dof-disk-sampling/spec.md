# Feature Specification: Correct and Unify Depth-of-Field Lens Sampling Across Hiders

**Feature Branch**: `007-dof-disk-sampling`

**Created**: 2026-07-23

**Status**: Draft

**Input**: User description: "Fix the raytrace hider's depth-of-field (DOF) center-bias defect and de-duplicate/unify lens-disk sampling between the REYES (CStochastic) and raytrace (CRaytracer) hiders, so both hiders compute depth-of-field using the same correct disk-sampling resolver and produce the same, correct DOF blur-circle (circle-of-confusion) result."

## User Scenarios & Testing *(mandatory)*

<!--
  IMPORTANT: User stories should be PRIORITIZED as user journeys ordered by importance.
  Each user story/journey must be INDEPENDENTLY TESTABLE - meaning if you implement just ONE of them,
  you should still have a viable MVP (Minimum Viable Product) that delivers value.

  Assign priorities (P1, P2, P3, etc.) to each story, where P1 is the most critical.
  Think of each story as a standalone slice of functionality that can be:
  - Developed independently
  - Tested independently
  - Deployed independently
  - Demonstrated to users independently
-->

### User Story 1 - Correct depth-of-field blur when raytracing (Priority: P1)

A scene author renders a scene with a shallow depth of field (a wide aperture / low FStop) using the ray-tracing hider. They expect out-of-focus elements to blur with a smooth, evenly-distributed circle of confusion — the same physically-plausible look a real camera lens produces, and the same look the REYES hider already produces for the identical camera settings. Today, the ray-traced result instead looks like blur energy has collapsed toward the center of each blur circle, producing an artificially tight, incorrect out-of-focus look.

**Why this priority**: This is the actual visible defect reported by users of the renderer. Anyone using the ray-tracing hider with depth of field enabled is getting a wrong image today; fixing it is the core deliverable.

**Independent Test**: Render a scene with `FStop`/`FocalDistance` set to produce a strong, visible defocus blur using the ray-tracing hider, and inspect the resulting blur circles (e.g., on an out-of-focus point light or specular highlight). The blur should show uniform energy distribution across the full circle of confusion, not a bright/dense center with a faint edge.

**Acceptance Scenarios**:

1. **Given** a scene with depth of field enabled and rendered with the ray-tracing hider, **When** the image is rendered, **Then** out-of-focus highlights show a uniformly-distributed blur circle (no visible center-biased hot spot) matching the expected size for the configured aperture and focal distance.
2. **Given** the same scene rendered with the ray-tracing hider before and after the fix, **When** the two renders are compared, **Then** the after-fix render shows a measurably more even radial energy distribution within each blur circle.

---

### User Story 2 - Consistent depth-of-field results across hiders (Priority: P2)

A scene author switches a scene between the REYES hider and the ray-tracing hider (a supported, common workflow for cross-checking or choosing the appropriate hider for a shot) without changing camera, `FStop`, or `FocalDistance` settings. They expect the depth-of-field look — blur circle size and energy distribution — to be consistent between the two hiders, since both claim to implement the same camera model.

**Why this priority**: Consistency between hiders is the stated goal of this fix and prevents surprising, hider-dependent look changes for the same scene description. It is secondary to the correctness fix itself (Story 1), since fixing correctness is what makes consistency achievable.

**Independent Test**: Render the same scene with identical camera/DOF settings using both hiders and visually/statistically compare the resulting blur circles for equivalent size and distribution.

**Acceptance Scenarios**:

1. **Given** a scene with depth of field enabled, **When** it is rendered once with the REYES hider and once with the ray-tracing hider using identical `FStop`/`FocalDistance` values, **Then** the two renders produce statistically equivalent blur-circle size and energy distribution (allowing for each hider's own sampling-noise characteristics).
2. **Given** the existing depth-of-field example/test scenes for both hiders, **When** they are rendered after the fix, **Then** both hiders' outputs pass the project's visual-regression comparison against their (updated, correct) reference images.

---

### User Story 3 - Maintainable, non-duplicated lens sampling (Priority: P3)

A renderer maintainer needs to change or extend how the lens aperture is sampled in the future (e.g., to add a bladed/polygonal aperture shape for stylized bokeh). Today they would have to find and change this logic in two separate, differently-implemented places, risking the two hiders drifting apart again. They expect a single, shared piece of logic to update once.

**Why this priority**: This is a maintainability/quality improvement that reduces the risk of the original bug's root cause (divergent, duplicated logic) recurring. It follows naturally from Stories 1 and 2 but doesn't independently deliver end-user-visible value.

**Independent Test**: Inspect the renderer's lens-sampling logic and confirm there is exactly one authoritative implementation used by both hiders, rather than two separate implementations.

**Acceptance Scenarios**:

1. **Given** the renderer's source, **When** a maintainer looks for where lens/aperture sample points are generated, **Then** they find a single shared implementation referenced by both hiders rather than two separate implementations.

---

### Edge Cases

- What happens when depth of field is disabled (default aperture / pinhole camera)? The fix MUST NOT change rendered output for scenes that do not use depth of field.
- What happens at the very edge of the aperture (maximum radius) or exact center (radius zero)? The corrected sampling must remain numerically stable (no NaN/degenerate samples) at both extremes.
- What happens for scenes that combine depth of field with motion blur? Both effects must continue to combine correctly after the change, for hiders that support that combination today.
- What happens to previously-captured reference images used for automated visual comparison of ray-traced depth-of-field scenes? They were captured against the previous, incorrect behavior and must be treated as invalid baselines going forward, not as evidence of a regression.
- What happens for scenes using an extremely narrow aperture (near-pinhole, small but nonzero blur)? The corrected sampling must not introduce visible bias even when the blur circle is only a few pixels wide.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The renderer's ray-tracing hider MUST distribute lens/aperture sample points uniformly by area across the lens disk when depth of field is enabled, with no bias toward the center or edge of the disk.
- **FR-002**: The renderer's REYES hider and ray-tracing hider MUST use the same underlying lens-sampling logic, so that a single fix or future change to how the lens disk is sampled applies identically to both.
- **FR-003**: For identical camera, `FStop`, and `FocalDistance` settings, the two hiders MUST produce depth-of-field blur that is equivalent in size and radial energy distribution, within the normal sampling-noise variation already expected between the two hiders.
- **FR-004**: The fix MUST NOT change any user-facing scene-description API (no new or altered RIB tokens, options, or attributes related to depth of field, aperture, `FStop`, or `FocalDistance`).
- **FR-005**: The fix MUST NOT change rendered output for scenes that do not enable depth of field (i.e., pinhole-camera renders are unaffected).
- **FR-006**: The renderer's automated visual-regression test scenes and reference images covering ray-traced depth of field MUST be updated so they validate the corrected, unbiased behavior rather than continuing to encode the previous center-biased defect.
- **FR-007**: The corrected lens sampling MUST remain numerically stable across the full valid input range (zero radius through maximum aperture radius), producing no invalid (NaN/undefined) sample positions.
- **FR-008**: The project's renderer-parity documentation MUST be updated to reflect that depth-of-field lens sampling is no longer a known parity gap between the REYES and ray-tracing hiders.

### Key Entities

- **Lens/aperture sample**: A single 2D point on the camera's lens aperture disk, used to originate one ray (or offset one shading sample) for a depth-of-field render; characterized by its position relative to the disk center and its contribution to the overall, area-uniform distribution of samples across the disk.
- **Depth-of-field visual regression scene**: An existing example scene and its reference image, used to automatically detect unintended changes to depth-of-field rendering; affected scenes need their reference images regenerated as part of this fix since the prior references encode the defect being corrected.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Ray-traced renders with depth of field enabled show no visually detectable center-bias artifact in out-of-focus blur circles, verified by visual inspection of representative test scenes.
- **SC-002**: For the same scene and camera settings, blur-circle size and radial energy distribution produced by the ray-tracing hider and the REYES hider differ only by an amount attributable to each hider's normal sampling noise, not by a systematic distribution shape difference.
- **SC-003**: 100% of the renderer's automated visual-regression tests covering depth of field (both hiders) pass against updated reference images after the fix.
- **SC-004**: There is exactly one place in the renderer's source responsible for generating a uniform lens-disk sample point, used by both hiders that support depth of field.
- **SC-005**: Rendering time for depth-of-field scenes does not regress by more than a negligible margin (no meaningful performance cost from the fix).

## Assumptions

- "Correct" depth of field means samples are distributed uniformly by area across the lens aperture disk, which is the standard physically-based camera model used elsewhere in this renderer (and matches the existing REYES hider's already-correct behavior).
- The two hiders are not required to produce bit-identical images for the same depth-of-field scene, only statistically/distributionally equivalent blur, since they may continue to use different underlying random/sample-sequence sources.
- Existing depth-of-field example and visual-regression scenes for the ray-tracing hider are sufficient to validate this fix; no new example scenes are required, though existing reference images need regeneration.
- This work is scoped to circular (standard) lens aperture sampling only; non-circular/bladed aperture shapes (stylized bokeh) are out of scope.
- Motion blur and other camera effects that already combine with depth of field are expected to continue working after this change; no new interactions are being introduced.
