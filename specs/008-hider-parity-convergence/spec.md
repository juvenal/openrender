# Feature Specification: Reyes/Raytrace Hider Parity Convergence

**Feature Branch**: `008-hider-parity-convergence`

**Created**: 2026-08-01

**Status**: Draft

**Input**: User description: "Implement Options A and B from the hider-parity audit (.plans/reports/hider-parity-audity-and-roadmap.md): Phase 3 refactors R1-R4 (split the hider contract, extract a shared per-sample generator, extract a shared transparency/matte compositor, extract a shared pixel-filter module) and share/integrate items S1-S5 (canonical lens/CoC model, displacement parity, depth-filter/z-visibility parity, transparent-hit AOV compositing, raytraced motion-blur verification), plus Option B's unified per-bucket sample table as a second stage. Divergence D1 (lens sampling bias) is already fixed and must be folded into, not redone by, this work."

## Clarifications

### Session 2026-08-01

- Q: Should raytrace displacement default to reyes's always-on behavior, or stay opt-in? → A: Default to reyes behavior — raytrace displaces by default like reyes; scenes with expensive displacement shaders may render slower under raytrace unless a user opts out.
- Q: Is raytraced motion blur scoped as verification-only, or as new feature work from a lower baseline? → A: Verification-only — treat existing per-hit-time interpolation as the mechanism; add cross-hider motion tests and fix any correctness bugs the tests surface, but do not design new motion-blur infrastructure.
- Q: Should the Option B unified per-bucket sample table's determinism/replay property become a documented, user-facing guarantee? → A: Internal only — sample-table sharing is purely an implementation detail that correlates noise for tighter parity-test thresholds; no new user-facing determinism contract or RIB-level API is introduced.

## User Scenarios & Testing *(mandatory)*

### User Story 1 - Cross-hider parity safety net (Priority: P1)

A renderer maintainer is about to change how either hider samples, shades, or composites a pixel. Before touching any shared code, they need a way to render the same scene with both the reyes and the raytrace hider and see, per visual effect (flat shading, depth of field, motion blur, transparency, matte, AOVs), whether the two hiders' output is within an expected, documented tolerance of each other. Today no such comparison exists — only per-hider visual-regression tests against each hider's own prior reference image.

**Why this priority**: Every other story in this spec modifies code shared by both hiders. Without a cross-hider comparison in place first, a regression in one hider caused by shared-code changes could silently pass the existing per-hider visual-regression suite (which only checks a hider against its own history, not against the other hider). This is the audit's own stated prerequisite for every subsequent change.

**Independent Test**: Render each new parity scene pair (flat-shade, DOF, motion, transparency, matte, AOVs) with both hiders and confirm the harness reports a pass/fail per scene against a documented per-effect threshold, independent of any other story in this spec being implemented.

**Acceptance Scenarios**:

1. **Given** a scene exercising a single visual effect (e.g. depth of field), **When** it is rendered with both the reyes and raytrace hiders and compared by the new harness, **Then** the harness reports a numeric divergence score and a pass/fail verdict against that effect's documented threshold.
2. **Given** the full set of new parity scene pairs, **When** run as part of the existing test invocation, **Then** results are reported per scene without requiring a maintainer to manually diff images.
3. **Given** a deliberately introduced regression in shared sampling or compositing code, **When** the parity harness runs, **Then** at least one parity scene fails, catching the regression before it reaches the existing per-hider visual-regression suite.

---

### User Story 2 - One shared per-sample generator (Priority: P2)

A renderer maintainer needs to change how pixel jitter, time-stratification, or lens/aperture sampling works (e.g. to add a new sampling pattern or fix a bias). Today this logic is written twice — once inside the reyes hider's per-sample setup and once inside the raytracer's per-sample setup — with independently hand-copied constants that have already drifted once (the pixel-jitter constant differs: `0.5001011` vs `0.5`) and were only recently reconciled for lens sampling via a shared `sampleDisk()` free function. The maintainer wants exactly one piece of code that both hiders call for "generate this pixel-sample's jitter, time, and lens point."

**Why this priority**: This closes the most mechanical, highest-value source of future drift (jitter constant, lens/CoC formulas) and gives the already-completed lens-sampling fix a permanent home instead of leaving it as two call sites into a shared free function.

**Independent Test**: Inspect the renderer's source and confirm a single sampler component supplies jitter offset, time stratum, and lens point for a pixel sample, consumed identically by both hiders; verify via the existing disk-sampling and radial-histogram tests that lens-sample quality is unchanged.

**Acceptance Scenarios**:

1. **Given** the renderer's source, **When** a maintainer looks for where a pixel sample's jitter/time/lens values are generated, **Then** they find one shared component used by both hiders, not two independent implementations.
2. **Given** the existing depth-of-field disk-sampling and radial-histogram validation tests, **When** they are run after this change, **Then** they continue to pass with no change to the measured area-uniform distribution.
3. **Given** a scene rendered with both hiders before and after this change, **When** compared via the User Story 1 harness, **Then** the previously-known pixel-jitter-constant divergence no longer contributes to any measured difference.
4. **Given** reyes's per-vertex circle-of-confusion computation and raytrace's per-ray lens-point computation, **When** both are traced back to their governing formulas, **Then** both derive from one canonical lens/CoC formula set rather than two independently maintained ones.

---

### User Story 3 - One shared transparency and matte compositor (Priority: P2)

A scene author renders a scene containing semi-transparent surfaces, matte objects, and extra output channels (AOVs), and expects the same opacity, matte, and channel-compositing behavior regardless of which hider they choose. Today the two hiders implement transparency and matte compositing independently: reyes composites a per-sample depth-sorted fragment list front-to-back with matte encoded as negative opacity, while raytrace composites via iterative continuation rays with a residual-opacity value and captures extra AOVs from the first hit only. The renderer maintainer wants one shared "combine a transparent/matte hit into the running result" component used by both.

**Why this priority**: Transparency and matte handling are common, visible scene features (not edge cases), and their current divergence produces different-looking results for the same scene depending on hider choice — a correctness gap for any pipeline that cross-checks or switches hiders.

**Independent Test**: Render a scene with layered semi-transparent surfaces, a matte object, and multiple AOVs using both hiders and confirm final composited color, opacity, and every AOV channel agree within the User Story 1 harness's transparency/matte/AOV thresholds.

**Acceptance Scenarios**:

1. **Given** a scene with several stacked semi-transparent surfaces, **When** rendered with either hider, **Then** the front-to-back composited color and opacity match within the harness's transparency threshold.
2. **Given** a scene containing a matte object partially covering non-matte geometry, **When** rendered with either hider, **Then** matte carve-out behavior (what is/isn't visible or composited) is equivalent between hiders.
3. **Given** a scene with extra AOV channels behind a semi-transparent surface, **When** rendered with the raytrace hider, **Then** those AOVs reflect properly composited (not first-hit-only) values, matching reyes's compositing/non-compositing channel rules.
4. **Given** the existing deep-shadow output (which reads reyes's per-sample fragment list directly), **When** this change lands, **Then** deep-shadow output is unchanged, since the fragment-list data structure itself is not altered.

---

### User Story 4 - Consistent depth compositing (depth filters + z-visibility) (Priority: P3)

A scene author relies on depth-filter modes (minimum, maximum, average, mid) and a z-visibility threshold to control how the `z` (depth) output channel is derived from overlapping samples. Today reyes implements all of this; raytrace implements none of it (its `z` is always the first-hit distance). The scene author wants the same depth-filter and z-visibility-threshold options to behave the same way regardless of hider.

**Why this priority**: Depth output is a commonly-consumed AOV for compositing pipelines; a hider that silently ignores a scene's depth-filter/z-visibility settings produces a wrong `z` channel without any error, which is a correctness gap a maintainer or compositor artist may not discover for a long time.

**Independent Test**: Render a scene with overlapping geometry at different depths using each depth-filter mode and a configured z-visibility threshold with the raytrace hider, and confirm the resulting `z` channel matches reyes's behavior for the same settings.

**Acceptance Scenarios**:

1. **Given** a scene with overlapping surfaces at different depths and a depth-filter mode set to minimum, maximum, average, or mid, **When** rendered with the raytrace hider, **Then** the resulting `z` channel matches the value reyes produces for the same mode and scene.
2. **Given** a scene with a configured z-visibility threshold, **When** rendered with the raytrace hider, **Then** samples beyond the threshold are excluded from the depth computation the same way reyes excludes them.
3. **Given** a scene rendered with the raytrace hider using the default depth-filter mode, **When** compared to pre-change raytrace output for the same scene, **Then** default-mode output is unchanged (no behavior change for scenes not explicitly using an alternate depth-filter mode).

---

### User Story 5 - Displacement parity by default (Priority: P3)

A scene author applies a displacement shader to geometry and expects both hiders to displace it the same way without needing hider-specific scene attributes. Today reyes always displaces; raytrace only displaces a ray-hit surface if the scene explicitly sets `Attribute "trace" "displacements"`. The scene author wants raytrace to displace by default, matching reyes, since displacement is a visible geometric feature that currently silently differs by hider.

**Why this priority**: A silent, attribute-gated geometric difference between hiders is a correctness surprise for any scene authored without raytrace-specific tuning in mind; defaulting to parity removes that trap, at the cost of raytrace needing to displace more often (a performance tradeoff the scene author can still opt out of).

**Independent Test**: Render a scene with a displacement shader applied to geometry, using the raytrace hider with no special attributes set, and confirm the surface is displaced consistent with reyes's output for the same scene.

**Acceptance Scenarios**:

1. **Given** a scene with a displacement shader and no `Attribute "trace" "displacements"` set, **When** rendered with the raytrace hider, **Then** the geometry is displaced, matching reyes's silhouette and shading for the same scene within the harness's threshold.
2. **Given** the same scene, **When** a scene author explicitly disables ray-traced displacement via the existing attribute mechanism, **Then** raytrace honors that opt-out and renders the undisplaced surface, preserving an escape hatch for performance-sensitive scenes.
3. **Given** existing scenes that rely on today's opt-in-only raytrace displacement behavior, **When** this change lands, **Then** the change is documented as a default-behavior change (not silently absorbed) so pipelines relying on the old default can adjust.

---

### User Story 6 - Verified raytraced motion blur (Priority: P3)

A scene author renders a scene with moving geometry (not just a moving camera) using the raytrace hider and expects motion blur to render correctly, matching reyes's motion-blurred result for the same scene and shutter settings. The renderer's own status documentation currently flags raytraced object motion blur as unverified/incomplete, even though the underlying ray-surface intersection code already interpolates vertex positions by the ray's stratified time.

**Why this priority**: Motion blur is a widely-used effect; an "unverified" status on a code path that's already exercised by the raytracer means either a hidden bug or an outdated warning — both need resolving, but this depends on the parity harness (Story 1) to distinguish the two.

**Independent Test**: Render a scene with moving (deforming or translating) geometry using both hiders with identical shutter/motion settings and compare motion-blurred results via the Story 1 harness.

**Acceptance Scenarios**:

1. **Given** a scene with translating geometry over the shutter interval, **When** rendered with the raytrace hider, **Then** the motion-blurred result matches reyes's motion-blurred result for the same scene within the harness's motion threshold.
2. **Given** a scene with deforming (per-vertex time-varying) geometry over the shutter interval, **When** rendered with the raytrace hider, **Then** the result shows correct motion blur, and any correctness bug the harness surfaces is fixed as part of this story.
3. **Given** raytraced object motion blur passes the new cross-hider tests, **When** this story is complete, **Then** the renderer's status documentation is updated to remove the "incomplete/unverified" flag for this path.

---

### User Story 7 - One shared pixel-filter module (Priority: P4)

A renderer maintainer needs to change how sub-pixel samples are combined into a final pixel value (the reconstruction filter kernel and its normalization). Today this combining logic is implemented independently for reyes, for raytrace, and for the z-buffer hider, even though all three already share the same precomputed filter kernel. The maintainer wants one shared "combine these weighted samples into a pixel" component used by all three.

**Why this priority**: Lower risk/urgency than the sampling and compositing stories (the kernel itself is already shared; only the splat/gather and normalization code around it is duplicated), but it removes the last significant place a future filtering change would need to be applied three times.

**Independent Test**: Inspect the renderer's source and confirm one shared component performs sample-to-pixel combination and weight normalization for all three hiders; render a scene with each hider before and after and confirm pixel output is unchanged.

**Acceptance Scenarios**:

1. **Given** the renderer's source, **When** a maintainer looks for where sub-pixel samples are combined into a final pixel color, **Then** they find one shared component referenced by all three hiders.
2. **Given** a scene rendered by each of the three hiders before and after this change, **When** compared, **Then** each hider's own output is unchanged (this story changes structure, not results).

---

### User Story 8 - Hider contract matches what hiders actually do (Priority: P4)

A renderer maintainer implementing or reviewing a new hider (or maintaining the raytrace hider) currently has to understand and stub out `drawObject`/`drawGrid`/`drawPoints` — bucket-rasterization operations that only the reyes-family hiders ever use — because these are declared on the shared shading engine that every hider inherits from. The maintainer wants the shared engine to expose only shading and ray-tracing, with rasterization operations living only on the hiders that perform rasterization.

**Why this priority**: Purely structural/mechanical — it has no rendered-output effect — so it carries the least urgency, but it removes a standing source of confusion (and unused stub methods) for anyone extending the hider set, including the planned future path-tracing hider.

**Independent Test**: Inspect the renderer's source and confirm the shared shading/tracing engine no longer declares `drawObject`/`drawGrid`/`drawPoints`, and that the raytrace hider no longer contains stub overrides of them; confirm the full visual-regression suite still passes.

**Acceptance Scenarios**:

1. **Given** the shared shading engine's interface, **When** inspected, **Then** it exposes shading and ray-tracing operations only, with no rasterization-specific methods.
2. **Given** the raytrace hider's source, **When** inspected, **Then** it contains no stub/no-op overrides of rasterization methods it never uses.
3. **Given** the existing visual-regression suite (33+ scenes across all hiders), **When** run after this change, **Then** all previously-passing scenes continue to pass, since this story is a structural move with no intended behavior change.

---

### User Story 9 - Correlated sampling for tighter parity checks (Priority: P5)

A renderer maintainer wants to tighten the pass/fail thresholds on the Story 1 parity harness so it can catch smaller regressions. Today, even after Stories 2-8 land, each hider still independently draws its own per-sample jitter/time/lens values from the shared generator, so the two hiders' noise patterns for the same scene are uncorrelated — real per-pixel differences are indistinguishable from independent-noise differences unless thresholds stay loose. The maintainer wants both hiders, for a given bucket, to consume the exact same table of per-sample position/time/lens values, so that any remaining difference in a comparison is attributable to the hiders' own algorithms rather than to independent random noise.

**Why this priority**: This is explicitly the second stage in the audit's plan, valuable only once Stories 2 (shared sampler) and 1 (harness) exist to make use of it; it is a precision improvement on top of an already-working parity story, not a new capability on its own.

**Independent Test**: Render the same bucket of a scene with both hiders using the shared per-bucket sample table and confirm both hiders consumed identical per-sample position/time/lens values (not just statistically similar ones); confirm the Story 1 harness's thresholds for at least one previously-loose effect (e.g. transparency or motion) can be measurably tightened without introducing false failures on unmodified code.

**Acceptance Scenarios**:

1. **Given** a bucket of pixel samples generated once by the shared sampler for a given scene render, **When** both hiders render that bucket, **Then** both consumed the same underlying position/time/lens values for corresponding samples.
2. **Given** the tightened parity thresholds enabled by correlated sampling, **When** the full parity suite from Story 1 is run against unmodified, already-passing code, **Then** it continues to pass (no new false failures introduced by tightening).
3. **Given** this story is not yet implemented, **When** Stories 1-8 are complete, **Then** the renderer is already in a fully working, shipped state — Story 9 strictly improves test precision and does not gate any other story's value.

---

### Edge Cases

- What happens to bit-for-bit residual differences that this spec explicitly does not aim to close (shading-density/interpolation differences between grid-vertex shading and per-hit shading; the physical difference between screen-space depth-of-field scatter and true lens rays)? These MUST remain as documented, bounded residuals with their own (looser) parity thresholds in the Story 1 harness, not as pass/fail failures blocking this spec.
- What happens when a parity scene is rendered twice in a row with the same hider (not across hiders)? Same-hider run-to-run noise variance (already documented as non-deterministic due to bucket→thread jitter-stream assignment) must not be mistaken for a cross-hider divergence; per-effect thresholds must be set above known same-hider run-to-run variance.
- What happens to deep-shadow map output, which reads reyes's per-sample fragment list directly, once the shared transparency/matte compositor (Story 3) lands? It must continue to work unchanged, since the fragment-list data structure itself is out of scope for modification.
- What happens for scenes that use depth of field and motion blur together, or transparency and matte together, once the relevant shared components land? Combined-effect scenes must continue to combine correctly — the parity harness (Story 1) must include at least one combined-effect scene pair per interacting pair of stories.
- What happens for existing example/test scenes and reference images (33+ visual-regression scenes, including the already-known-stale `.slo`-dependent ones) once shared components land? They must continue to pass except where a story explicitly documents an intentional default-behavior change (e.g. Story 5's displacement default), in which case affected reference images are regenerated and the change is documented, not silently absorbed.
- What happens if a parity scene reveals that an "algorithmic residual" (D3/D4/D9-class difference) is larger than expected, suggesting an actual bug rather than an inherent algorithmic limit? The harness must make this distinguishable (e.g. a residual exceeding its own documented bound is still a reportable failure) rather than permanently suppressing all differences for that effect.

## Requirements *(mandatory)*

### Functional Requirements

- **FR-001**: The renderer MUST provide a cross-hider parity test harness that renders the same scene with both the reyes and raytrace hiders and reports a per-scene divergence measurement against a documented, per-effect threshold.
- **FR-002**: The parity harness MUST include at least one scene pair for each of: flat shading, depth of field, motion blur, transparency, matte objects, and extra AOV channels, plus at least one combined-effect scene (e.g. depth of field + motion blur, transparency + matte).
- **FR-003**: The parity harness MUST run as part of the renderer's existing automated test invocation and MUST NOT require a maintainer to manually diff images to get a pass/fail result.
- **FR-004**: The renderer MUST expose exactly one shared component that generates a pixel sample's jitter offset, time stratum, and lens/aperture point, consumed identically by both the reyes and raytrace hiders.
- **FR-005**: The shared per-sample generator MUST incorporate the existing, already-fixed area-uniform lens-disk sampling logic as its lens/aperture point source, rather than duplicating or replacing it.
- **FR-006**: The renderer MUST derive reyes's per-vertex circle-of-confusion and raytrace's per-ray lens point from one canonical set of lens/CoC formulas.
- **FR-007**: After the shared per-sample generator lands, the previously-known pixel-jitter-constant divergence between the two hiders MUST no longer measurably contribute to cross-hider differences reported by the parity harness.
- **FR-008**: The existing depth-of-field disk-sampling and radial-histogram validation tests MUST continue to pass, unmodified in their pass/fail intent, after the shared per-sample generator lands.
- **FR-009**: The renderer MUST expose exactly one shared component responsible for compositing transparent and matte samples front-to-back (opacity accumulation, matte carve-out, and compositing/non-compositing AOV channel rules), consumed by both reyes's per-sample compositing and raytrace's continuation-ray compositing.
- **FR-010**: The shared transparency/matte compositor MUST NOT alter reyes's underlying per-sample fragment-list data structure, since deep-shadow map output reads it directly.
- **FR-011**: After the shared compositor lands, the raytrace hider MUST composite extra AOV channels through transparent/matte hits using the same compositing/non-compositing channel rules reyes uses, rather than capturing those channels from the first hit only.
- **FR-012**: The raytrace hider MUST support the same depth-filter modes (minimum, maximum, average, mid) as reyes, producing equivalent `z`-channel output for the same scene and mode.
- **FR-013**: The raytrace hider MUST respect a configured z-visibility threshold the same way reyes does, excluding samples beyond the threshold from depth computation.
- **FR-014**: The raytrace hider's default depth-filter behavior MUST remain unchanged for scenes that do not explicitly configure a non-default depth-filter mode.
- **FR-015**: The raytrace hider MUST displace geometry with a displacement shader applied by default (no scene attribute required), matching reyes's always-displace behavior.
- **FR-016**: Scene authors MUST retain a way to explicitly opt a surface out of ray-traced displacement for performance reasons, via the existing attribute mechanism.
- **FR-017**: This default-behavior change to raytrace displacement MUST be documented as a behavior change (not a silent default shift) in the renderer's parity/status documentation.
- **FR-018**: The renderer MUST provide cross-hider parity test coverage for both translating and deforming (time-varying-vertex) moving geometry rendered with the raytrace hider, verified against reyes's motion-blurred output for the same scene.
- **FR-019**: Any correctness bug in raytraced object motion blur that the new motion-blur parity tests surface MUST be fixed as part of this work; if no bug is found, the existing per-hit-time interpolation mechanism MUST be confirmed sufficient and the renderer's status documentation updated to remove the "incomplete/unverified" flag.
- **FR-020**: The renderer MUST expose exactly one shared component that combines weighted sub-pixel samples into a final pixel value (filter kernel evaluation and weight normalization), consumed by the reyes, raytrace, and z-buffer hiders.
- **FR-021**: Introducing the shared pixel-filter module MUST NOT change any hider's rendered output for scenes that already pass the existing visual-regression suite.
- **FR-022**: The shared shading/tracing engine that all hiders inherit from MUST expose shading and ray-tracing operations only; bucket-rasterization operations (`drawObject`, `drawGrid`, `drawPoints`) MUST be owned only by the reyes-family hiders that perform rasterization.
- **FR-023**: The raytrace hider MUST NOT contain stub or no-op overrides of rasterization-only operations it does not use.
- **FR-024**: The existing 33+-scene visual-regression suite MUST continue to pass after every structural change in this spec (Stories 2, 3, 7, 8), except where a story explicitly documents an intended default-behavior change, in which case the affected reference images are regenerated and the change documented.
- **FR-025**: The renderer MUST provide a shared per-bucket sample table (position/time/lens values per pixel sample) generated once by the shared per-sample generator and consumed identically by both hiders for the same bucket, so that corresponding samples in both hiders' renders originate from the same underlying values.
- **FR-026**: Once the shared per-bucket sample table is in place, at least one previously-loose parity threshold in the Story 1 harness (e.g. transparency or motion) MUST be measurably tightened without introducing false failures against unmodified, already-passing code.
- **FR-027**: The shared per-bucket sample table MUST remain an internal implementation detail: it MUST NOT introduce a new user-facing determinism/replay guarantee, RIB token, or option.
- **FR-028**: The Story 1 parity harness MUST document, for each effect where this spec does not aim for full convergence (shading-density/interpolation differences; depth-of-field occlusion-model differences), the residual difference as an explicit, bounded, documented threshold rather than a silently-loosened or suppressed check.
- **FR-029**: None of the refactors in this spec (Stories 2, 3, 7, 8) MUST change any user-facing scene-description API — no new or altered RIB tokens, options, or attributes are introduced except where a story explicitly says otherwise (Story 5's existing displacement opt-out attribute continues to exist unchanged).
- **FR-030**: Raster hot-loop performance for the reyes hider MUST NOT regress by more than 2-3% on the renderer's existing depth-of-field and motion example scenes as a result of any refactor in this spec, measured before and after each change.

### Key Entities

- **Parity scene pair**: A single RIB scene rendered once with each hider under identical settings, tagged with the single visual effect (or effect combination) it exercises, used by the Story 1 harness to detect cross-hider divergence.
- **Per-effect parity threshold**: A documented, numeric tolerance for how much a given effect's cross-hider divergence measurement may be before the parity harness reports a failure; distinct thresholds exist per effect to account for effects with inherent, documented residual differences (shading interpolation, DOF occlusion model) versus effects expected to converge tightly.
- **Shared per-sample generator**: The single component responsible for producing a pixel sample's jitter offset, time stratum, and lens/aperture point; the sole home for the previously-duplicated jitter/time/lens constants and formulas, including the already-fixed area-uniform lens-disk sampling logic.
- **Shared transparency/matte compositor**: The single component responsible for combining a transparent or matte sample into a running front-to-back composited result, including opacity accumulation, matte carve-out, and compositing/non-compositing AOV channel rules.
- **Shared pixel-filter module**: The single component responsible for evaluating the reconstruction filter kernel and normalizing sub-pixel sample weights into a final pixel value, used by all three hiders.
- **Depth-filter mode**: One of minimum, maximum, average, or mid — a configured policy for how the `z` output channel is derived when multiple samples at different depths contribute to a pixel.
- **Shared per-bucket sample table**: A table of per-pixel-sample position/time/lens values generated once per bucket by the shared per-sample generator and consumed identically by both hiders rendering that bucket, correlating their noise patterns for the same scene.
- **Documented residual**: A cross-hider difference this spec explicitly does not aim to close (e.g. grid-vertex-interpolated shading vs. per-hit shading; screen-space depth-of-field scatter vs. true lens rays), recorded with its own bounded parity threshold rather than treated as a defect.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: A maintainer can render any of the new parity scene pairs with both hiders and get a pass/fail parity verdict, per effect, without manually inspecting or diffing images.
- **SC-002**: For flat-shading, matte, and depth-filter/z-visibility parity scenes, cross-hider divergence is at or below each effect's documented threshold on 100% of parity scene pairs.
- **SC-003**: For depth-of-field, motion-blur, and transparency parity scenes, cross-hider divergence is at or below each effect's documented threshold, accounting for that effect's documented residual (algorithmic shading-interpolation or DOF-occlusion-model differences), on 100% of parity scene pairs.
- **SC-004**: There is exactly one shared implementation each for: per-sample jitter/time/lens generation, transparency/matte compositing, and pixel-filter combination — each independently verifiable by inspecting the renderer's source.
- **SC-005**: 100% of the existing 33+-scene visual-regression suite continues to pass after this spec's structural changes, except for scenes affected by the documented displacement-default change, whose reference images are regenerated and whose change is documented.
- **SC-006**: Raytraced motion blur for both translating and deforming geometry passes cross-hider parity comparison against reyes for 100% of the new motion parity scenes.
- **SC-007**: Reyes raster-loop rendering time for the existing depth-of-field and motion example scenes does not regress by more than 2-3% versus pre-change baseline, measured after each structural change lands.
- **SC-008**: After the shared per-bucket sample table (Option B) lands, at least one previously-loose parity threshold is tightened and the full parity suite still passes with zero false failures against unmodified code.

## Assumptions

- The already-completed lens-sampling fix (shared `sampleDisk()` in `src/ri/random.h`, consumed by both hiders' own random-number sources) is treated as a correct, working starting asset. This spec folds its logic into the new shared per-sample generator rather than re-deriving or re-validating disk-sampling correctness from scratch; the existing disk-sampling and radial-histogram tests remain the source of truth for that correctness.
- "Parity" in this spec means cross-hider divergence within a documented, per-effect threshold — not bit-exact identical images. Effects with inherent algorithmic or physical differences (shading-density/interpolation between grid-vertex shading and per-hit shading; depth-of-field's screen-space scatter vs. true lens-ray occlusion) are explicitly out of scope for full convergence and are instead recorded as bounded, documented residuals with their own thresholds.
- The full hybrid/unified hider architecture described as "Option C" in the source audit (one pipeline with pluggable rasterization/ray-tracing visibility) is out of scope for this spec; it belongs to the separate, not-yet-started path-tracing hider effort.
- Raytrace displacement defaults to matching reyes's always-on behavior (per the resolved clarification); this is a deliberate, documented default-behavior change, not a bug fix disguised as a non-change, and scene authors retain an explicit opt-out for performance.
- Raytraced motion blur is scoped as verification plus bug-fixing against the existing per-hit-time interpolation mechanism, not as new ground-up feature design; if the new parity tests find no bug, the outcome is a documentation update (removing the "incomplete/unverified" flag), not new code.
- The Option B shared per-bucket sample table's determinism/replay property is an internal testing/precision improvement only; it does not add a new user-facing feature, RIB token, or documented guarantee about reproducibility.
- Existing per-hider visual-regression tests and their known caveats (run-to-run noise variance on certain scenes; stale `.slo`-dependent shader tests unrelated to this work) remain in place unchanged except where a story explicitly requires reference-image regeneration.
- The audit's recommended execution order (parity harness first, then cheap/mechanical fixes, then the shared sampler, then the shared compositor and filter module, then motion-blur verification, then the Option B sample table) is assumed to be a reasonable build order for a subsequent implementation plan, though this spec does not itself mandate a strict sequencing beyond "the harness exists before any other story's changes are validated."
