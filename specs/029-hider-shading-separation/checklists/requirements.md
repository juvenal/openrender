# Specification Quality Checklist: Hider / Shading Separation

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-11-01
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs) beyond the domain vocabulary this project's remediation specs use (class, header, symbol and file:line names identify the registry's evidence; where the new interfaces live, their C++ spellings and the mechanism that moves the bundles are left to the plan)
- [x] Focused on user value and business needs (maintainers, spec authors of 030/031/032/033/049/052, people who link the renderer statically)
- [x] Written for non-technical stakeholders (as far as an architecture remediation allows; same register as specs 024, 025, 026, 027, 028)
- [x] All mandatory sections completed

## Requirement Completeness

- [x] No [NEEDS CLARIFICATION] markers remain (the 3 that were open, Q1 the input of `shade()`, Q2 the photon-map lookup route and Q3 the arena/stats injection, were answered on 2026-11-01 and encoded in Clarifications, Session 2026-11-01; spec.md has none)
- [x] Requirements are testable and unambiguous
- [x] Success criteria are measurable
- [x] Success criteria are technology-agnostic (stated as link-line contents, counts, request equality, byte comparisons and suite results, not mechanisms)
- [x] All acceptance scenarios are defined
- [x] Edge cases are identified
- [x] Scope is clearly bounded (Scope Boundaries places #117, #118, #119, #120, #123, #134, #44 and specs 030, 031, 032, 033, 034, 049, 052, 054 explicitly)
- [x] Dependencies and assumptions identified (specs 023-028, 037, 039 merged; spec 026's contract evolved under the version rule)

## Feature Readiness

- [x] All functional requirements have clear acceptance criteria
- [x] User scenarios cover primary flows
- [x] Feature meets measurable outcomes defined in Success Criteria
- [x] No implementation details leak into specification (same carve-out as the first item)

## Notes

- All items pass. The three markers were answered by the maintainer on 2026-11-01: Q1 option B (point-source interface, Hiders-side adapter), Q2 option A (Shading-owned photon lookup interface, MAJOR bump), Q3 option A (arena/stats injection out of 029).
- Brief versus code findings recorded in the spec: (1) `CShadingContext` has six virtuals (destructor, `renderingLoop`, four hooks) and five test classes subclass it only for an empty loop; (2) `traceEx` reads private members of the Shading class, so moving it needs a public ray-level interface (FR-017); (3) `shade()` calls the primitive back twice on the ray-hit derivative path and `interpolate()` writes into locals that exist only after the shaders are prepared, so a passive `ShaderGlobals` is not enough for byte-identical renders (Q1); (4) the bundle classes contain Shading logic only and are used by Hiders and the core, so they can move to Shading without a new edge; (5) `irradiance.h` looks unused in the two Shading files that include it; (6) DB-006's link lines name `shade`/`saveState`/`restoreState`, so a signature change would re-key allowlist lines (FR-028); (7) the registry's line references for DB-005, DB-013 and DB-020 are stale (FR-031); (8) `LibShader_McompConsistency` (#44) is expected to start passing on Linux, which the pass/fail criterion allows (SC-009); (9) the arena/stats injection that DB-016 leaves "with spec 029" is not in #35's scope (Q3).
- Counts are static (grep, allowlist and the spec 026 records, master `d6910bd0`); the plan re-measures with `archcheck`, `dyld_info` and `nm` on a fresh build.
- Items marked incomplete require spec updates before `/speckit-plan`.
