# Specification Quality Checklist: JIT/Interpreter Parity Follow-ups (post-011)

**Purpose**: Validate specification completeness and quality before proceeding to planning
**Created**: 2026-08-24
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

- Items marked incomplete require spec updates before `/speckit-clarify` or `/speckit-plan`.

### Validation record (2026-08-24)

Iteration 1 findings and the edits made in response:

1. **"No implementation details"** — first draft named source files, function
   names (`emitBin`/`emitUn`/`emitTern`, `jitIlluminanceBegin`), and the
   `usfroma` opcode mnemonic throughout the requirements. Rewritten to
   describe outcomes in domain terms ("an instruction the compiler has
   classified as uniform", "reads an element of a `uniform string` array at
   a varying index", "light iteration for the `illuminance` construct").
   `.rslo`/`.slo` and `illuminance` are retained deliberately: they are the
   project's user-facing vocabulary for the two shipping backends and for an
   RSL language construct, matching the precedent set by spec 011's spec.
   The precise code-level locations belong in `plan.md`/`research.md`, and
   the full evidentiary trail already exists in
   `specs/011-jit-opcode-parity/lessons-learned.md` Phase 10.
2. **"Success criteria are measurable"** — SC-005 originally restated spec
   011's 90% bar as a pass/fail criterion. Per the 2026-08-24 clarification
   it is now a *reported* per-scene outcome with an explicit requirement to
   identify the residual dominant cost where the bar is not met; the
   pass/fail gate moved to SC-004 (improvement on 100% of scenes, exceeding
   measured run-to-run variance).
3. **"Requirements are testable and unambiguous"** — SC-004's original
   "improves" was unfalsifiable against timing noise. Added the
   variance-exceeding qualifier, and an edge case requiring a run-to-run
   variance baseline.
4. **"Dependencies and assumptions identified"** — added an explicit
   assumption that the availability of per-instruction uniform
   classification at the JIT's dispatch-construction point is an open
   question for investigation, not an established fact. The seed description
   asserted the information "already exists in the emitter", but the
   evidence behind that statement concerns *operand* uniformity (stride
   computation), which is related to but not the same as the
   *per-instruction* classification the interpreter's short-circuit keys on.
   Treating it as unverified keeps the prerequisite visible instead of
   burying a possible scope increase.
5. **"Scope is clearly bounded"** — added an assumption that residual
   performance causes beyond the identified one are to be *characterized and
   reported*, not necessarily fixed within this feature, so the performance
   story has a definite end.
6. **"Edge cases are identified"** — added the case where the existing
   minimal reproduction shader is missing or no longer reproduces (a
   reproduction must be re-established empirically; code reading alone does
   not authorize an interpreter change), and the case where the light
   iteration convergence cannot be done without changing observable
   behavior (it then falls under the FR-011 process rather than proceeding
   as a refactor).

Iteration 2 findings and the edits made in response:

7. **"Requirements are testable and unambiguous"** — FR-008 and SC-004 had
   reinstated, as a hard gate, the very bar that finding 2 removed:
   improvement required on *every* measurement scene. By the feature's own
   stated mechanism the ratio gap tracks uniform-computation density, so a
   near-zero-uniform-density scene (already at ~1.03) has almost nothing to
   collapse and its expected delta sits inside measurement noise — the
   criterion would have failed such a scene for behaving exactly as
   predicted. Both now require variance-exceeding improvement only on scenes
   with meaningful uniform computation, no regression anywhere, and name "no
   measurable change on a near-zero-uniform-density scene" as a conforming
   outcome. SC-004 additionally requires each scene's uniform-computation
   density to be recorded alongside its ratio, so scenes cannot be
   reclassified after the fact to explain away a disappointing result.
8. **"Success criteria are measurable"** — SC-001 asserted abnormal
   termination in "100% of runs" before the fix, a determinism that has not
   been measured (the crash was observed via a single debugger session). If
   the underlying defect turns out to be intermittent, the criterion would
   have been unfalsifiable in the unhelpful direction. Now states a minimum
   run count and admits a recorded non-deterministic reproduction rate; the
   post-fix half remains 100%.

All items pass as of iteration 2. No [NEEDS CLARIFICATION] markers remain —
the four decisions that would otherwise have been markers (story priority,
performance-bar semantics, interpreter-change authorization, and feature
naming) were resolved with the maintainer before drafting and are recorded
in the spec's Clarifications section.
