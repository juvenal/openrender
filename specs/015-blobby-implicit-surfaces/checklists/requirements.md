# Specification Quality Checklist: Blobby Implicit Surfaces

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-08-28
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

- Items marked incomplete require spec updates before `/speckit-clarify` or `/speckit-plan`

### Validation record

Validated in a single pass; no item failed, so no revision iteration was needed.

Two things were watched for specifically while drafting, because the feature description
supplied by the user is unusually implementation-specific:

1. **Implementation detail leak** — the description names concrete source files, C++
   classes, and grammar productions (`src/ri/rib.y`, `CRibOut`,
   `CRendererContext::RiBlobbyV`, `CImplicit`, `CMakeLists.txt`). None of these were
   carried into the spec. FR-001 through FR-005 state the observable behaviour instead
   ("the RIB parser MUST accept", "RIB output MUST emit"). The specific call sites are
   preserved in the Input field and belong in `plan.md`/`tasks.md`. The one place the
   spec comes close is the Out of Scope entry excluding the existing plug-in
   implicit-surface path, which is a genuine scope boundary and is described by what it
   does rather than by its class name.
2. **Measurability of the fidelity criterion** — "looks smooth" is not verifiable, so
   SC-006 is stated as two checkable conditions: no facet edges visible on the
   silhouette at the default setting, and measurably reduced silhouette deviation from
   the true level set when the setting is tightened.

### Post-draft consistency review

A second read for internal contradictions caught three, all corrected in the spec:

1. **US8 AC-3 vs. the motion-blur assumption** — an acceptance scenario required blurring
   a blob "being subtracted over the shutter interval," which is exactly the
   topology-changing case that per-vertex motion samples on a once-generated surface
   cannot represent, while the Assumptions section flagged that same problem as
   unsolved. AC-3 now covers only topology-preserving change, a new AC-4 requires a
   bounded, documented result for topology change, and FR-026 carries the same qualifier.
2. **SC-006 vs. the fidelity-default assumption** — the criterion demanded no visible
   faceting on a blobby *filling the frame* at the *default* setting, while the
   assumption said the default targets typical framing and the attribute is the
   close-up escape hatch. SC-006 now states the default standard at typical framing and
   the tightened-setting standard for a full-frame close-up.
3. **FR-013 missing a pre-declaration clause** — FR-025 requires its attribute be usable
   from RIB without prior declaration; the scene-level option in FR-013 has the same
   dependency and omitted it. Added.

One coverage gap was also closed: the Edge Cases section covered a field that never
reaches the threshold but not its dual, a field at or above the threshold everywhere,
which has no boundary to find and is a classic non-termination case for surface
extraction.

### Deliberate judgement calls (no clarification markers raised)

Three details were unspecified by both primary sources. Reasonable defaults were chosen
and recorded in the spec's Assumptions section rather than raised as blocking questions:

- **Surface threshold value** — neither RISpec 3.2 §5.6 nor Application Note #31 states
  the level at which the combined field defines the surface. The spec fixes it to
  PhotoRealistic RenderMan's value, to be confirmed during design by reproducing the
  published reference images. This is a design-phase research item, not a scope question.
- **Surface parameter and texture coordinate variables** — the specification says only
  that blobbies have no global parameterisation. FR-021 defers to whatever subdivision
  surfaces (which share the limitation) already do, which is both a reasonable default
  and directly testable.
- **How the compatibility option for opcodes 4/5 is named and spelled** — a naming
  decision with no scope impact, left to `plan.md`.

### Known cross-source conflict (recorded, not deferred)

RISpec 3.2 Table 5.3 and Application Note #31 assign opcodes 4 and 5 in opposite orders.
Both sources were read verbatim; this is a genuine contradiction, not a transcription
error. FR-013 resolves it by supporting both readings with the RISpec order as the
default, and requires the erratum be documented with citations.
