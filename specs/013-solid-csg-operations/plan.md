# Implementation Plan: Solid CSG Operations

**Branch**: `013-solid-csg-operations` | **Date**: 2026-08-26 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/013-solid-csg-operations/spec.md`

**Note**: This template is filled in by the `/speckit-plan` command; its definition describes the execution workflow.

## Summary

Implement RenderMan Spec 3.2 §5.9 `SolidBegin`/`SolidEnd` CSG
(`"primitive"`/`"union"`/`"intersection"`/`"difference"`) plus
Interior/Exterior shader support, under a hard architectural constraint from
the plan input: CSG boundary resolution happens exactly once, purely in the
geometry domain, before any hider runs, and the resolved result must dice
correctly for REYES/z-buffer and intersect correctly for raytrace/photon/
future path-tracing through the same shared primitive interface, with zero
hider-specific CSG code.

Technical approach (fully detailed in `research.md`): intercept
`RiSolidBegin`/`RiSolidEnd` to capture each solid block's operand geometry
into a build-time CSG tree instead of dispatching it to the scene
immediately — mirroring the existing `RiObjectBegin`/`RiObjectEnd`
instancing-capture pattern. At the outermost `RiSolidEnd`, resolve the tree
with an in-tree BSP-tree boolean kernel (no new external dependency, per
Principle V) into a Resolved Solid Boundary: a container `CObject` whose
`CPolygonMesh` children each retain their *originating leaf's* `CAttributes*`
(giving per-face Interior/Exterior/shader provenance, FR-009, for free).
That resolved primitive re-enters `addObject()` exactly like any other
primitive, so it reaches `CRenderer::render()` — the single existing
chokepoint that registers a primitive with both the raytrace object tree and
the REYES rasterizer — automatically satisfying "no hider-specific CSG code"
by construction rather than by per-hider parity effort.

Each leaf operand is tessellated into a triangle mesh before BSP
classification using the same flatness/chordal-deviation adaptive criterion
`CTesselationPatch` already applies for raytrace grid-splitting, extended to
cover quadrics (never tessellated before) and gated by a scene-author
tolerance attribute (`research.md` Decision 4) — so a resolved boundary's
polygon density concentrates where curvature demands it rather than being
spread uniformly, keeping NURBS-, quadric-, and subdivision-surface-sourced
solids from visibly faceting. NURBS- and quadric-sourced fragments further
carry analytic per-vertex `"N"` shading normals so silhouettes and shading
stay smooth even at moderate polygon counts; subdivision-surface-sourced
fragments do not (`research.md` Decision 4b, an accepted v1 limitation).

## Technical Context

**Language/Version**: C++20 (repository-wide standard, Principle II)

**Primary Dependencies**: None new. BSP-tree CSG boolean kernel implemented
in-tree (`research.md` Decision 3), per Principle V (Minimal Dependencies).

**Storage**: N/A (in-process scene-description feature, no persistence)

**Testing**: ctest, extending the existing `-L libshader` unit-test pattern
for the boolean kernel (hand-computable box/sphere/coplanar-face cases,
`research.md` Decision 6) and the existing `-L visual` scene-regression
pattern for end-to-end cross-hider parity and non-regression coverage.

**Target Platform**: Linux and macOS (Principle VI; no new platform surface)

**Project Type**: Existing single-repo C++ renderer (`src/ri/` core). This
feature is additive to that module — no new top-level project.

**Performance Goals**: Correctness-first; no explicit performance target for
v1 (clarified in `/speckit-clarify` session 2026-08-26). Operand
tessellation density is driven by the same flatness/chordal-deviation
adaptive stopping criterion `CTesselationPatch` already uses for raytrace
grid-splitting (`surface.cpp:1858-1897`), reused rather than reimplemented,
with a scene-author tolerance override attribute (`research.md` Decision 4) —
curvature-adaptive, not a fixed uniform density, so NURBS/quadric/
subdivision-surface CSG operands resolve to smooth-looking boundaries rather
than visibly faceted ones. NURBS- and quadric-sourced boundary fragments
additionally carry analytic per-vertex shading normals (`research.md`
Decision 4b) so shading itself stays smooth even where polygon density is
locally coarse; subdivision-surface operands do not get this and rely on
denser tessellation alone — see the fidelity tradeoffs recorded under
Complexity Tracking.

**Constraints**: CSG boundary resolution MUST occur exactly once, in the
geometry domain, before any hider-specific code runs — no lazy per-dice or
per-ray resolution, and zero new hider-specific CSG logic in any of REYES,
raytrace, photon, or future path-tracing hiders (hard constraint from the
plan input).

**Scale/Scope**: Bounded by existing scene-complexity norms already exercised
by the visual-regression suite (87+ scenes); no new scale target introduced.
Nesting depth: arbitrary (FR-006), validated to at least 4 levels (SC-003).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. Clean Code Standards | PASS | New types (`CSolidObject`/CSG tree builder/BSP kernel) follow existing `CObject` hierarchy conventions in `src/ri/`; boolean kernel isolated as small, focused, testable functions rather than one monolithic resolver. |
| II. Language Standards | PASS | C++20, no platform-specific APIs, no new language extensions required. |
| III. Test-Driven Development (NON-NEGOTIABLE) | PASS (planned) | `research.md` Decision 6 / `quickstart.md` §1 commit to writing and approving failing unit tests for the boolean kernel *before* any `SolidBegin`/`SolidEnd` integration code, per Red-Green-Refactor. This is the constitution's highest-risk gate for this feature and is addressed explicitly, not deferred. |
| IV. Command Line Interface | PASS | No new CLI surface; RIB scenes through the existing `orender <rib>` entry point remain the interface, consistent with every other primitive type. |
| V. Minimal Dependencies | PASS | BSP-tree CSG chosen specifically to avoid a new external boolean-mesh library dependency (`research.md` Decision 3 rationale, explicitly weighed against CGAL/libigl-style alternatives). |
| VI. Platform Targeting | PASS | No new platform-specific code; runs wherever the existing renderer core does. |
| VII. Documentation and Site Management | PASS (planned) | `quickstart.md` includes a Hugo `site/` documentation task for `SolidBegin`/`SolidEnd`, `Attribute "solid"`, and Interior/Exterior usage, to be delivered alongside the feature rather than after. |

No violations requiring Complexity Tracking justification on the "process"
axis. One tradeoff is recorded below because it is a direct, unavoidable
consequence of the user's hard architectural constraint, not a corner cut —
see Complexity Tracking.

*Re-evaluated after Phase 1 design (below): unchanged — no new gate
violations introduced by `data-model.md` or `contracts/`. Re-evaluated again
after the Decision 4/4b tessellation-quality refinement (reusing
`CTesselationPatch`'s flatness criterion; analytic normals for NURBS/quadric
operands only): still unchanged — no new external dependency (Principle V:
the flatness criterion and `crossvv` normal evaluation are both already
in-tree), no new hider-specific code (the extracted criterion and normal
evaluation live in the geometry domain alongside the rest of CSG resolution,
consumed once at `RiSolidEnd`), and no new CLI/platform surface.*

## Project Structure

### Documentation (this feature)

```text
specs/013-solid-csg-operations/
├── plan.md              # This file (/speckit-plan command output)
├── research.md          # Phase 0 output (/speckit-plan command)
├── data-model.md         # Phase 1 output (/speckit-plan command)
├── quickstart.md         # Phase 1 output (/speckit-plan command)
├── contracts/
│   └── solid-rib-interface.md   # Phase 1 output (/speckit-plan command)
└── checklists/
    └── requirements.md   # Pre-existing spec quality checklist
```

### Source Code (repository root)

```text
src/ri/
├── ri.cpp                    # RiSolidBegin/RiSolidEnd: add the missing
│                              #   RENDERMAN_SOLID_PRIMITIVE_BLOCK push/pop
│                              #   (research.md, "RIB block-state enforcement
│                              #   gap"), mirroring RiObjectBegin's pattern.
├── rendererContext.h          # Add currentSolid / savedSolids state,
│                              #   mirroring the existing instance /
│                              #   instanceStack members (~line 225-236).
├── rendererContext.cpp        # RiSolidBegin/RiSolidEnd (currently the
│                              #   unimplemented stub at lines 5502-5513):
│                              #   implement the capture/resolve/re-enter
│                              #   flow. addObject() (line 457): add the
│                              #   third capture gate for an open solid
│                              #   block, alongside the existing instance/
│                              #   delayed gates.
├── csgTree.h / csgTree.cpp    # NEW. CSG Tree node type + validation rules
│                              #   (data-model.md).
├── csgBoolean.h / csgBoolean.cpp   # NEW. BSP-tree boolean kernel: build,
│                              #   classify/clip/merge, per-operation
│                              #   combination, difference winding-reversal
│                              #   (research.md Decision 3).
├── surface.h / surface.cpp    # Extract the flatness/chordal-deviation
│                              #   stopping criterion out of
│                              #   CTesselationPatch::tesselate (currently
│                              #   entangled with a ray-footprint criterion
│                              #   that has no meaning outside a traced ray,
│                              #   lines ~724-759/1858-1897) into a form
│                              #   csgTree.cpp can call directly to tessellate
│                              #   NURBS/quadric CSG operands (research.md
│                              #   Decision 4). Also add the analytic
│                              #   per-vertex crossvv(dPdu, dPdv) normal
│                              #   evaluation consumed by solidObject.cpp when
│                              #   building Boundary Fragments from these
│                              #   operand types (research.md Decision 4b).
├── solidObject.h / solidObject.cpp  # NEW. CSolidObject : CObject —
│                              #   Resolved Solid Boundary container
│                              #   (data-model.md), presenting CPolygonMesh
│                              #   Boundary Fragments to every hider via the
│                              #   existing generic CObject dispatch.
├── attributes.h / attributes.cpp    # Add "solid" "tessellationtolerance"
│                              #   attribute storage (existing
│                              #   Interior/Exterior fields are reused
│                              #   unchanged — no new fields needed there).
├── rendererDeclarations.cpp   # Pre-declare the new "solid" attribute
│                              #   namespace (4-layer attribute pattern,
│                              #   CLAUDE.md).
├── shading.cpp / shaderFunctions.h  # Consume attributes->interior /
│                              #   attributes->exterior when shading a
│                              #   Boundary Fragment (FR-010/011/012/020) —
│                              #   reuses existing shader-dispatch, no new
│                              #   hider-side logic.
└── ri.h                       # Any new token constants needed for the
                                #   "solid" attribute namespace.

tests/
├── unit/csg/                  # NEW. Boolean-kernel unit tests
│                              #   (quickstart.md §1): box/box,
│                              #   sphere/box, coplanar-face cases.
└── (existing visual test dirs)/  # NEW scenes for cross-hider parity
                                #   (quickstart.md §3), Interior/Exterior
                                #   (§4), nested composite (§5), and error
                                #   diagnostics (§7), added alongside
                                #   existing examples/rib/ scene patterns.

site/
└── (existing content structure)/  # NEW page documenting SolidBegin/
                                #   SolidEnd, Attribute "solid", and
                                #   Interior/Exterior (Principle VII).
```

**Structure Decision**: This is an addition to the existing single-project
C++ renderer core (`src/ri/`) — no new top-level project or service
boundary. New CSG-specific logic (tree building, BSP boolean kernel,
resolved-boundary container type) is factored into new, focused files
(`csgTree.*`, `csgBoolean.*`, `solidObject.*`) rather than folded into
`rendererContext.cpp`, keeping the boolean kernel independently unit-testable
(Principle III) and physically separate from RIB-parsing/state-machine
concerns, consistent with the file-per-concern granularity already used
elsewhere in `src/ri/` (e.g. `patches.cpp` vs. `patchUtils.h` vs.
`zbufferQuad.h`). Every new type joins the existing `CObject` hierarchy
(`object.h`) rather than introducing a parallel geometry representation, so
it reaches every hider through the dispatch mechanisms that already exist —
this is what makes "zero hider-specific CSG code" achievable at all.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

No constitution gate is violated. One design tradeoff is recorded here
because it is a direct, load-bearing consequence of the plan input's hard
constraint, not a corner cut, and future readers of this plan should not
mistake it for an oversight:

| Tradeoff | Why Needed | Simpler Alternative Rejected Because |
|---|---|---|
| Operand boundary tessellation density is fixed at `RiSolidEnd` (geometry-build) time, from a curvature-adaptive flatness/chordal-deviation tolerance (reusing `CTesselationPatch`'s existing raytrace grid-splitting criterion) — not derived from `ShadingRate` or camera distance, which are unknown at that point. A curved primitive's CSG silhouette is a polygonal approximation regardless of final render distance, though now one that concentrates density where curvature is highest rather than spreading it uniformly (`research.md` Decision 4). | The plan input requires CSG resolution to happen exactly once, in the geometry domain, before any hider exists — so no hider-specific `ShadingRate`/camera information is available to drive tessellation density at resolution time. | Deferring tessellation density to each hider's own dice/intersect call would recover per-hider adaptive fidelity, but reintroduces the lazy, hider-coupled resolution the plan input explicitly forbids ("no hider dependency... independent of hider or shading backend"). A fixed *uniform* density (independent of curvature) was also rejected: it would either overspend triangles on flat regions or underspend on tight curves to hit the same budget, and reusing the already-proven flatness criterion costs no new algorithm. The `Attribute "solid" "float tessellationtolerance"` escape valve (`contracts/solid-rib-interface.md`) remains the mitigation for authors needing tighter fidelity on a specific solid than the default. |
| Analytic per-vertex `"N"` shading normals are populated for NURBS- and quadric-sourced Boundary Fragments only; subdivision-surface-sourced fragments always fall back to flat, facet-derived `VARIABLE_NG` regardless of subdivision level (`research.md` Decision 4b). | `CLoopSubdivMesh` treats `"N"` as an ordinary vertex primvar under plain linear-averaging refinement (`subdivisionLoop.cpp:22-27,44-48`) — it has no eigenbasis/extraordinary-vertex analytic limit-normal evaluation today, for CSG or otherwise. Adding one is a substantial, general-purpose subdivision-surface feature in its own right, not a CSG-scoped change. | Implementing analytic subdivision limit normals as part of this feature would silently expand its scope well beyond CSG (it would also change non-CSG subdivision-surface rendering) and add real risk to a feature whose hard constraint is already the highest-risk item on this plan (per-hider parity, Constitution Principle III). Denser subdivision (already planned, `research.md` Decision 4) mitigates visible faceting for v1; closing this gap properly is deferred to a future, subdivision-surface-scoped spec. |
