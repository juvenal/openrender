# Specification Quality Checklist: GI Determinism

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-11-13
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs) beyond the measured starting point (see Notes)
- [x] Focused on user value and business needs (reproducible GI output for render users, test authors and the maintainer)
- [x] Written for the project's stakeholders (maintainer, contributors, shader authors, render users)
- [x] All mandatory sections completed

## Requirement Completeness

- [x] No [NEEDS CLARIFICATION] markers remain (all 3 answered 2026-11-13: see Notes)
- [x] Requirements are testable and unambiguous
- [x] Success criteria are measurable
- [x] Success criteria are technology-agnostic where the subject allows (bit-identical output, file bytes, run counts)
- [x] All acceptance scenarios are defined
- [x] Edge cases are identified
- [x] Scope is clearly bounded (Out of Scope, Overlap table)
- [x] Dependencies and assumptions identified (032 and 039 confirmed merged with git; #48 and #55 closed)

## Feature Readiness

- [x] All functional requirements have clear acceptance criteria
- [x] User scenarios cover primary flows (photon map, cached GI image, files, cache miss, cost and record)
- [x] Feature meets measurable outcomes defined in Success Criteria
- [x] No implementation details leak into the requirements (the mechanism is left to plan: FR-008 states properties, not methods)

## Notes

- **Project convention.** This project's specs carry file and line references and symbol names in the Context ("measured starting point") because the spec is the record the plan re-measures against (see specs 032 and 039). They are confined to Context and the Overlap table; the Requirements and Success Criteria state properties.
- **Clarified 2026-11-13.** (1) Cached GI images at `-t:1` may change once (FR-018, references re-rendered once, no threshold widened); (2) `-P:N` for cached irradiance scenes is in scope (FR-015, SC-001); (3) #102's `.ptc` order is out of scope. See the Clarifications section.
- **Not decided in the spec.** The mechanism for an order-independent irradiance cache (issue #79: "chosen in plan"), the photon ordering rule, the lookup-memo decision, and any format change are plan decisions listed under "Decisions the plan must present for approval".
- **Bug states corrected.** #158, #159, #160, #161, #164, #165 and #166 were closed by a closing keyword in the 032 merge message ("not fixed: #N" read as "fixed: #N") and reopened on 2026-11-13; they are open defects, and #160, #161, #164 and #165 are still in the code at master `278b0394`.
