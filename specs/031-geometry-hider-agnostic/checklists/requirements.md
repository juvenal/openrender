# Specification Quality Checklist: Geometry, Hider-Agnostic

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-11-06
**Feature**: [spec.md](../spec.md)

## Content Quality

- [ ] No implementation details (languages, frameworks, APIs) - names classes, files and registry ids by design: this is an architecture-remediation spec whose "users" are maintainers and whose acceptance is stated in those terms (as specs 028 and 029 do). Interface member lists are left to the plan.
- [x] Focused on user value and business needs (maintainer value: unchanged renders, guard shrinks, preview builds honestly)
- [x] Written for non-technical stakeholders - to the extent a Principle VIII spec can be
- [x] All mandatory sections completed

## Requirement Completeness

- [x] No [NEEDS CLARIFICATION] markers remain - the three were answered by the maintainer on 2026-11-07 (spec.md, Session 2026-11-07), plus a fourth question on the DB-025 line
- [x] Requirements are testable and unambiguous
- [x] Success criteria are measurable
- [x] Success criteria are technology-agnostic - measured in allowlist lines, byte-identical images and named tests, the project's own acceptance vocabulary
- [x] All acceptance scenarios are defined
- [x] Edge cases are identified
- [x] Scope is clearly bounded (concurrent specs 030/032/056/057 and overlapping Bug Tracker issues listed)
- [x] Dependencies and assumptions identified

## Feature Readiness

- [x] All functional requirements have clear acceptance criteria
- [x] User scenarios cover primary flows
- [x] Feature meets measurable outcomes defined in Success Criteria
- [ ] No implementation details leak into specification - see first item

## Notes

- The two unchecked "implementation details" items are accepted for this class of spec; the clarification session of 2026-11-07 is recorded. Its largest effect is the lens phase (US7, FR-022 to FR-027, FR-029, FR-030, SC-009, SC-010): the maintainer chose to unify the Reyes and raytracer lens models, reversing the clarify stage's recommendation, so depth-of-field images may change in the last phase only.
- Counts after the session: 30 functional requirements (FR-001 to FR-030), 11 success criteria (SC-001 to SC-011), 7 user stories. Allowlist target 175 lines (deletions 169: 30 + 78 + 49 + 11 + 1).
- Numbers in the spec were measured on master e83d005d, not copied from the plan; discrepancies are listed in the stage report.
