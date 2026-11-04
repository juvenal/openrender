# Specification Quality Checklist: Scene Interface Untangle

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-11-01
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs) beyond the interfaces and files that are the subject of the feature
- [x] Focused on user value (maintainers, back-end authors, shader and tool authors)
- [x] Written for the project's stakeholders (the "users" are developers; stated in the spec)
- [x] All mandatory sections completed

## Requirement Completeness

- [x] No [NEEDS CLARIFICATION] markers remain (Q1 to Q3 answered by the maintainer on 2026-11-01 and recorded under Clarifications)
- [x] Requirements are testable and unambiguous
- [x] Success criteria are measurable
- [x] Success criteria are technology-agnostic (they name interfaces, files and tests, not libraries)
- [x] All acceptance scenarios are defined
- [x] Edge cases are identified
- [x] Scope is clearly bounded (DB-004/DB-011, DB-025, specs 033 and 043 are named out of scope; the compiler split with 043 is fixed by Q1)
- [x] Dependencies and assumptions identified (spec 026 recorded as merge-ordering coordination only)

## Feature Readiness

- [x] All functional requirements have clear acceptance criteria
- [x] User scenarios cover primary flows
- [x] Feature meets measurable outcomes defined in Success Criteria
- [x] No implementation details leak into specification (file names appear in the measured starting point and where a file move is the deliverable)
- [x] No requirement or task depends on the merge (FR-030)

## Notes

- Resolved 2026-11-01 (see spec Clarifications): Q1 A (038 owns the standalone compiler's tables, include path and drift test; DB-019 to Resolved; 043 keeps the public library), Q2 A (`ribOut` zero Core files; `ribVector` drops those the Scene Interface or preview need; residuals pinned shrink-only against DB-004/DB-011), Q3 A (DB-024 narrowed and Open for sloinfo/oshader and the data viewer's Hiders/Texturing reach).
- Every file:line in the measured starting point was read at master `628d6747`. Discrepancies found against the registry: the `riInterface.cpp` line is 37, not 36; the DB-015 allowlist header says 11 for 10 lines; the registry's "`RiMake*` for precomputation" does not match `precomp.cpp`; the registry names `opcodes.h` where the shading engine's header is `scriptOpcodes.h`; the registry and spec 037 put the standalone compiler in 043, issue #43 in 038 (settled: 038). These corrections are planned registry edits (FR-028), not made at the specify stage.
