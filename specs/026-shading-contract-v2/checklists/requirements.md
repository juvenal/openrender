# Specification Quality Checklist: Shading Contract v2

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-10-30
**Feature**: [spec.md](../spec.md)

## Content Quality

- [x] No implementation details (languages, frameworks, APIs) beyond the domain vocabulary this project's remediation specs use (class, header and symbol names identify the registry's evidence; where the interfaces live, their C++ spellings and the batch layout are left to the plan)
- [x] Focused on user value and business needs (maintainers, spec authors of 029/033/034/049/052, people rendering scenes that use message passing and traced lookups)
- [x] Written for non-technical stakeholders (as far as an architecture remediation allows; same register as specs 024, 025, 027, 028)
- [x] All mandatory sections completed

## Requirement Completeness

- [ ] No [NEEDS CLARIFICATION] markers remain (3 open, FR-006, FR-010 twice; each carries a working answer and is listed under Open Questions)
- [x] Requirements are testable and unambiguous
- [x] Success criteria are measurable
- [x] Success criteria are technology-agnostic (stated as link-line contents, counts, request equality, byte comparisons and suite results, not mechanisms)
- [x] All acceptance scenarios are defined
- [x] Edge cases are identified
- [x] Scope is clearly bounded (Scope Boundaries names what 029, 031, 032, 033, 034, 049/052 and 054 own)
- [x] Dependencies and assumptions identified (024, 025, 027, 028, 039 merged; texture part adopted from 027)

## Feature Readiness

- [x] All functional requirements have clear acceptance criteria
- [x] User scenarios cover primary flows
- [x] Feature meets measurable outcomes defined in Success Criteria
- [x] No implementation details leak into specification

## Notes

- The one unchecked item is the three markers; they are the maintainer's to answer (clarify stage). Everything else passes on the first iteration.
- Brief versus code findings recorded in the spec: (1) the interpreter and the JIT call the same `CShadingContext` members for message passing, so routing them through the services reaches both backends and the brief's "JIT side is 033" becomes the reshaping of the JIT entry points only; (2) the three existing test classes of the services forward to a real inner implementation, so a standalone double does not exist yet; (3) the bundle vtables are data imports that dyld binds eagerly, so the "without linking ribRender" acceptance cannot be met by routing alone unless the residual symbols are removed (029) or stubbed in the test (FR-024); (4) DB-018 has no allowlist line, so the allowlist acceptance is met through DB-006's Shading-origin lines (expected: the two `surface.h` includes; `object.h` stays for 029); (5) `option()` already goes through the services; (6) the ray type and hit mode are currently inferred from the shading dimension, the `camerahitmode` attribute (grids only) and the illumination hook.
- Counts are static (grep and allowlist reading, master `d01da47`); the plan re-measures with `archcheck` on a fresh build.
