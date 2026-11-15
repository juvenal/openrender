# Specification Quality Checklist: Area Light Geometry

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-11-13
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs) -- this is an engine-internal spec: it names renderer concepts (attribute block, sampler sub-streams, hiders) because the project's specs do and the maintainer's reviewers need them; it prescribes no class layout or algorithm beyond the issue's stated estimator
- [x] Focused on user value and business needs (scene authors get soft shadows from extended sources; 041 gets a sampler to wrap)
- [x] Written for non-technical stakeholders -- N/A for this project: the readers are the maintainer and spec authors; user stories are in scene-author terms
- [x] All mandatory sections completed

## Requirement Completeness

- [x] No [NEEDS CLARIFICATION] markers remain -- FR-018 and FR-025 answered at the 2026-11-13 clarify session; FR-023 decided as option B the same day
- [x] Requirements are testable and unambiguous (FR-012's factor ownership was settled as Decision A on 2026-11-13: the renderer applies only 1/p_A)
- [x] Success criteria are measurable
- [x] Success criteria are technology-agnostic -- as far as this project's specs allow (closed forms, thread-count identity, parity within measured noise)
- [x] All acceptance scenarios are defined
- [x] Edge cases are identified
- [x] Scope is clearly bounded (committed primitive kinds, Out of scope list)
- [x] Dependencies and assumptions identified (A1-A12, overlaps with 033, 040, 048, 041, open issues)

## Feature Readiness

- [x] All functional requirements have clear acceptance criteria
- [x] User scenarios cover primary flows
- [x] Feature meets measurable outcomes defined in Success Criteria
- [x] No implementation details leak into specification beyond the project's convention

## Notes

- All items pass. Clarify session 2026-11-13 resolved FR-018, FR-025, the primitive priorities and Decisions A and B.
- Facts in "Measured starting point" come from reading the source on master `278b0394`. The 2026-11-13 probe used the only Release build on the machine (2026-11-01, not current master) and could not discriminate FR-023's options: no shadow appeared with or without the transmission flag. The plan must re-probe on a fresh master build.
