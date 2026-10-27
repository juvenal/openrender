# Specification Quality Checklist: Camera and Motion Layer

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-10-27
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs) beyond the domain vocabulary this project's remediation specs use (class, header and symbol names identify the registry's evidence; where the interfaces live and how the evaluator is shaped are left to the plan)
- [x] Focused on user value and business needs (maintainers, spec authors, people rendering motion-blur and depth-of-field scenes)
- [x] Written for non-technical stakeholders (as far as an architecture remediation allows; see specs 023, 024 and 025 for the same register)
- [x] All mandatory sections completed

## Requirement Completeness

- [x] No [NEEDS CLARIFICATION] markers remain (the three were answered 2026-10-27 and encoded: FR-014, FR-018, FR-021; the CSG assumption is confirmed)
- [x] Requirements are testable and unambiguous
- [x] Success criteria are measurable
- [x] Success criteria are technology-agnostic (they are stated as architecture-check counts, bit-comparisons of renders and suite results, not mechanisms)
- [x] All acceptance scenarios are defined
- [x] Edge cases are identified
- [x] Scope is clearly bounded (no Reyes or zbuffer hider change, no multi-key motion, no DOF-model unification, no BDPT behaviour, no Shading-side edit)
- [x] Dependencies and assumptions identified (spec 023, 039, 024/025 merged; 027 in progress; 031 boundary)

## Feature Readiness

- [x] All functional requirements have clear acceptance criteria
- [x] User scenarios cover primary flows
- [x] Feature meets measurable outcomes defined in Success Criteria
- [x] No implementation details leak into specification (FR-002, FR-003, FR-020 and the Assumptions leave the structure, location and addObject placement to the plan)

## Notes

- Clarify stage (2026-10-27): the three markers and the provisional CSG item are answered and encoded (Clarifications section). Further questions were returned to the maintainer and are not encoded.
- Brief versus code findings recorded in the spec: (1) none of the 24 `invShutterTime` reads in Geometry is in an `intersect()`; they fill `dPdtime` in each family's `sample()`, so removing them touches the grid-sampling path that both hiders use (FR-016, FR-018); (2) `invShutterTime` appears in 13 files, but only 7 are Geometry and 2 are Shading reads through the services interface (FR-017); (3) there are 12 intersection entry points plus `CDLObject`, not five families' worth of "intersect" code, and each family has a second copy of the same interpolation in `sample()`; (4) DB-009 has no allowlist line (it is not an edge), so "the allowlist lines of every resolved entry deleted" is met through DB-004 and DB-011 lines, and DB-009 cannot move to Resolved while Reyes (spec 031) is unfinished (FR-023); (5) DB-009 and DB-011 registry counts and line references are stale; (6) the reference doc's `ICameraService::sampleRay(CameraSample)` and the brief's `ICamera::generateRay(x, y, t, lensUV)` differ in name and shape.
- Tension recorded in FR-014: the copies of the motion interpolation use different arithmetic (double in the quadrics, `interpolatev` and the `sample()` paths; float in `interpolate()`/`interpolate3()`), so one evaluator is bit-identical to master only if it keeps per-site precision; unifying it is a later, approved change. The determinism sweep cannot detect a change relative to master, so byte-identity needs a before/after comparison against the starting tree.
- Found, not introduced (Assumptions): an object with its own transform motion receives no camera motion blur in either hider (probed on this tree); `shading.cpp` range-corrects the `time` shading variable with the reciprocal of the shutter length (unverified, invisible while every shutter is `0 1`).
