# Specification Quality Checklist: Shading Services Routing

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-10-13
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs)
- [x] Focused on user value and business needs
- [x] Written for non-technical stakeholders
- [x] All mandatory sections completed

## Requirement Completeness

- [x] No [NEEDS CLARIFICATION] markers remain
- [x] Requirements are testable and unambiguous
- [x] Success criteria are measurable
- [x] Success criteria are technology-agnostic (no implementation details)
- [x] All acceptance scenarios are defined
- [x] Edge cases are identified
- [x] Scope is clearly bounded
- [x] Dependencies and assumptions identified

## Feature Readiness

- [x] All functional requirements have clear acceptance criteria
- [x] User scenarios cover primary flows
- [x] Feature meets measurable outcomes defined in Success Criteria
- [x] No implementation details leak into specification

## Notes

- **Markers resolved (clarify session 2026-10-13).** The 3 specify-stage markers and the 3 follow-up questions are answered in the spec's Clarifications section:
  - no services: one warning per kind of lookup per process, plus the "not found" result;
  - the JIT's no-argument `solar()`: in scope;
  - the photon hider's handling of that form: out of scope, #52;
  - DB-012: Resolved by 024;
  - the ray type: moves to DB-018's Direction line.

  No [NEEDS CLARIFICATION] markers remain.
- **Technical vocabulary is deliberate.** This is an internal architecture remediation, and its "stakeholders" are the maintainer and contributors. As in specs 022 and 023, requirements and success criteria name the registry entries, the symbols that must leave the library's import list, and the allowlist and metric counts, because those are the observable outcomes. They name no class design, file layout or algorithm: how the DSO handle type is relocated, where services get registered earlier, and whether the JIT reuses or deletes the existing wrappers are all left to the plan.
- **Measured, not copied.** The starting-point numbers (12 imports, 5 renderer-core symbols, 9 call sites, 18 DB-001 edges, metric 9/5) were measured on master `690dbfc` (macOS). Linux was measured at the plan stage (research R1/R3): the same 12 strong imports as macOS.
- **Contradiction fixed in validation pass 1.** FR-010, FR-018 and SC-006 originally forbade any non-photon JIT change, which would have blocked the no-argument `solar()` fix if the maintainer puts it in scope. All three now carry that exception, made unconditional once clarify put the fix in scope. FR-004 now names the three renderer-core-only declarations a grep actually found in Shading code. The "no photons today" claim is now measured (research R4: the JIT photon pass segfaults, no map written).
- **Verified at the plan stage.** The no-argument `solar()` JIT gap was confirmed from the compiler's intermediate code (research), and `Visual_solar0-slo` reproduces it (red diff 201.62).
