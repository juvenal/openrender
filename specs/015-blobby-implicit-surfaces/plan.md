# Implementation Plan: Blobby Implicit Surfaces

**Branch**: `015-blobby-implicit-surfaces` | **Date**: 2026-08-29 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/015-blobby-implicit-surfaces/spec.md`

**Note**: This template is filled in by the `/speckit-plan` command; its definition describes the execution workflow.

## Summary

Implement RISpec 3.2 §5.6 `RiBlobby` — all four primitive-field opcodes
(1000 constant, 1001 ellipsoid, 1002 segment, 1003 repelling ground plane),
all eight combining opcodes (0–7), per-blob primvar blending, and the
`mpoint` reference-space type — under the same hard architectural constraint
spec 013 carried: the surface is derived **once, in the geometry domain,
before any hider runs**, with zero hider-specific blobby code.

The approach turns out to be unusually clean because the existing primitive
representation already provides most of what the constraint demands. A
blobby's field is evaluated and polygonized at `RiBlobby` time into a
`CPolygonMesh` handed to `CRendererContext::addObject()`
(`rendererContext.cpp:499`) — the single chokepoint every other primitive
already uses, which registers geometry with the raytrace object tree and the
REYES rasterizer alike. Three consequences fall out for free rather than
needing per-hider parity work:

- **Motion blur** (FR-026) is already hider-independent in this
  representation: `CPl` carries two vertex-data samples (`data0`, `data1`)
  and `CPolygonTriangle::moving()` is literally
  `mesh->pl->data1 != NULL` (`polygons.h:104`). Emitting a second sample is
  the entire integration.
- **Blended primvars** (FR-019) ride as ordinary `CONTAINER_VERTEX`
  `CPlParameter` entries, exactly as CSG's `csgBuildMeshForAttributeGroup`
  packs P and N (`csgTree.cpp:547`).
- **CSG operand use** (FR-027) needs no new code at all: `addObject()`
  already chains into `currentSolid->leafObjects` when a solid block is open
  (`rendererContext.cpp:504`), and `csgTessellateOperand` already handles a
  `CPolygonMesh` leaf (`csgTree.cpp:316`). The procedural-capture guard is
  never on this path. This becomes a *regression test*, not an
  implementation task.

The genuinely new engineering is therefore confined to three things the
repository has never had: a field evaluator over the code array, a surface
extractor, and a depth-file reader usable outside a shading context. Surface
extraction uses **seeded continuation marching tetrahedra** — flood-fill
outward from a seed cell per blob through only those cells the surface
actually crosses. This is what satisfies SC-012 (the published 480-segment
spiral must render: a large bounding box whose surface occupies a small
fraction of it, which a dense grid over the bound cannot afford), and
tetrahedra avoid marching cubes' ambiguous-face cases so the mesh is
watertight — a hard prerequisite for FR-027, since a leaky mesh silently
corrupts boolean resolution. Per-vertex normals come from the analytic field
gradient (FR-024), which offsets the higher triangle count tetrahedra imply.

## Technical Context

**Language/Version**: C++20 (repository-wide standard, Principle II)

**Primary Dependencies**: None new. The polygonizer, field evaluator, and
depth-file reader are all in-tree (Principle V). No external mesh, implicit
surface, or isosurface library is introduced — `research.md` Decision 2
weighs that explicitly.

**Storage**: N/A (in-process scene description). One new *read* path: the
repelling ground plane loads a depth file at build time (`research.md`
Decision 5).

**Testing**: ctest. Unit tests follow the `tests/unit/csg/` precedent — a new
`tests/unit/blobby/` directory with one file per concern and its own
`CMakeLists.txt`, registered from `tests/CMakeLists.txt`. Visual and
cross-hider-parity scenes register through the existing
`tests/visual/CMakeLists.txt` macros, which already emit both `Visual_` and
`Parity_` tests per scene — the latter is precisely SC-004's cross-hider
agreement check and needs no new harness.

**Target Platform**: Linux and macOS (Principle VI; no new platform surface)

**Project Type**: Existing single-repo C++ renderer. This feature is additive
to `src/ri/` — no new top-level project.

**Performance Goals**: Correctness-first, no wall-clock target (spec
Clarifications Q1). The binding constraint is structural rather than
temporal: extraction cost must track the *surface*, not the bounding-box
volume, verified by SC-012 and measured by the new surface-cell ratio
statistic (`research.md` Decision 7).

**Constraints**:
- Surface derivation happens once per renderer process, in the geometry
  domain, before any hider runs (FR-022). Zero hider-specific blobby code.
- Derivation must be **deterministic** (FR-023a): identical geometry from
  identical input on any machine, at any thread count, in any bucket order.
  This is not a nicety — each server in a distributed render derives its own
  copy from the re-emitted `Blobby` declaration, so any ordering dependence
  becomes a visible seam between servers. It constrains the polygonizer's
  visited-cell container and traversal order specifically.
- Motion blur is limited to **two time samples** by the `CPl` `data0`/`data1`
  representation. This is a property of the existing format, shared with
  every other primitive, not a blobby limitation.

**Scale/Scope**: Bounded by the existing visual-regression suite's norms,
plus one new large scene: ~500 segment fields (the published toroidal
spiral, SC-012).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. Clean Code Standards | PASS | Work splits along four natural seams — code-array validation, field evaluation, surface extraction, repeller I/O — each a separate translation unit with a narrow header, rather than one monolithic `CBlobby`. The evaluator is a pure function of (code array, point) with no renderer state, which is what makes it unit-testable at all. |
| II. Language Standards | PASS | C++20, standard library only, no platform-specific APIs. |
| III. Test-Driven Development (NON-NEGOTIABLE) | PASS (planned) | This is the feature's highest-risk gate and is addressed head-on rather than deferred. The field evaluator and code-array validator are pure functions with hand-computable expected values, so Red-Green-Refactor applies literally: `quickstart.md` §1 and `tasks.md` sequence the failing unit tests before any renderer integration. SC-003's analytic ground-truth cases (a lone ellipsoid *is* that ellipsoid; a lone segment *is* a capsule) extend TDD to the polygonizer, which would otherwise only be testable by eye. |
| IV. Command Line Interface | PASS | No new CLI surface. RIB scenes through `orender <rib>` remain the interface, as for every other primitive. New statistics print through the existing `CStats::printStats()` level-gated output. |
| V. Minimal Dependencies | PASS | No new external dependency. Marching tetrahedra, the field functions, and the depth-file read are all small, well-understood, in-tree implementations (`research.md` Decisions 2 and 5, both weighed against library alternatives). |
| VI. Platform Targeting | PASS | No platform-specific code. |
| VII. Documentation and Site Management | PASS (planned) | FR-032 requires Hugo site documentation for the primitive, the fidelity attribute, and the opcode 4/5 erratum, delivered with the feature. `quickstart.md` carries it as a first-class task, not a follow-up. |

No violations require Complexity Tracking justification. Two accepted
tradeoffs are recorded under Complexity Tracking below — both are direct
consequences of decisions already taken in the spec and its clarification
session, not corners cut here.

*Re-evaluated after Phase 1 design (`data-model.md`, `contracts/`,
`quickstart.md`): unchanged. No new external dependency, no new CLI or
platform surface, and no hider-specific code was introduced by the design —
the polygonizer and evaluator live wholly in the geometry domain and are
consumed once, at `RiBlobby` time.*

## Project Structure

### Documentation (this feature)

```text
specs/015-blobby-implicit-surfaces/
├── spec.md               # Feature specification (/speckit-specify + /speckit-clarify)
├── research-inputs.md    # Verified primary-source material (/speckit-specify)
├── plan.md               # This file (/speckit-plan)
├── research.md           # Phase 0 output (/speckit-plan)
├── data-model.md         # Phase 1 output (/speckit-plan)
├── quickstart.md         # Phase 1 output (/speckit-plan)
├── contracts/            # Phase 1 output (/speckit-plan)
│   ├── rib-binding.md         # Blobby statement, attribute, option grammar
│   └── field-semantics.md     # Opcode table, field functions, blend rules
├── checklists/
│   └── requirements.md   # Spec quality checklist
└── tasks.md              # Phase 2 output (/speckit-tasks — NOT created here)
```

### Source Code (repository root)

```text
src/ri/
├── blobby.h / blobby.cpp              # NEW: CBlobby construction; owns the
│                                      #      build-time pipeline from code
│                                      #      array to CPolygonMesh
├── blobbyField.h / blobbyField.cpp    # NEW: code-array validation, field
│                                      #      evaluation, analytic gradient,
│                                      #      per-blob value propagation.
│                                      #      Pure — no renderer state.
├── blobbyPolygonize.h / .cpp          # NEW: seeded continuation marching
│                                      #      tetrahedra; deterministic
│                                      #      traversal
├── blobbyRepeller.h / .cpp            # NEW: context-free depth-file load +
│                                      #      repulsion field evaluation
├── rendererContext.cpp                # RiBlobbyV: replace CODE_INCAPABLE
│                                      #   stub (line 5571) with construction
│                                      #   + addObject; RiAttributeV /
│                                      #   RiOptionV for the two new tokens
├── rib.y                              # Both RIB_BLOBBY productions (lines
│                                      #   2617, 2626), currently
│                                      #   "// FIXME: Not implemented"
├── ribOut.cpp                         # RiBlobbyV: replace RIE_UNIMPLEMENT
│                                      #   stub (line 1418) with real emit
├── attributes.h / attributes.cpp      # Fidelity attribute storage + find()
├── options.h / options.cpp            # Opcode-order compatibility option
├── ri.h / ri.cpp                      # Token constants
├── rendererDeclarations.cpp           # initDeclarations() for both tokens
│                                      #   and the mpoint type
├── rendererc.h                        # TYPE_MPOINT, following TYPE_QUAD
├── pl.cpp                             # TYPE_MPOINT case (line 720)
├── ribOut.cpp                         # TYPE_MPOINT cases (lines 1609, 1722)
│                                      #   — all four EVariableType switch
│                                      #   sites must gain a case together,
│                                      #   or the type is half-wired
├── stats.h / stats.cpp                # Blobby counters + surface-cell ratio
└── CMakeLists.txt                     # New translation units

tests/
├── unit/blobby/                       # NEW, mirroring tests/unit/csg/
│   ├── CMakeLists.txt
│   ├── blobbyTestUtils.h
│   ├── test_field_primitives.cpp      # opcodes 1000-1003
│   ├── test_field_combining.cpp       # opcodes 0-7, both 4/5 orders
│   ├── test_code_validation.cpp       # malformed arrays (SC-005)
│   ├── test_value_blending.cpp        # FR-019 propagation
│   ├── test_polygonize_analytic.cpp   # SC-003 closed-form ground truth
│   └── test_determinism.cpp           # FR-023a
├── visual/CMakeLists.txt              # Register new scenes (Visual_ + Parity_)
└── RIB/ or examples/rib/              # New blobby scenes

site/                                  # Hugo documentation (FR-032)
```

**Structure Decision**: The feature is additive to the existing `src/ri/`
renderer core — the same module every other geometric primitive lives in.
Four new translation-unit pairs keep the pure, testable logic (field
evaluation, extraction) separate from renderer integration (`blobby.cpp`),
which is what lets the constitution's TDD gate be satisfied honestly rather
than nominally. Unit tests mirror the `tests/unit/csg/` layout established by
spec 013 rather than inventing a new one.

## Complexity Tracking

> Filled because two tradeoffs are being accepted knowingly, not because the
> Constitution Check found violations.

| Tradeoff | Why Needed | Simpler Alternative Rejected Because |
|---|---|---|
| Marching **tetrahedra** rather than marching cubes, accepting roughly 2× the triangle count for equal fidelity | FR-027 requires a blobby to work as a CSG operand, and boolean resolution over a non-watertight mesh fails silently rather than loudly. Marching cubes' ambiguous-face configurations can open holes unless a full disambiguation table is added and tested. | Marching cubes with a disambiguation table was considered. It trades a well-understood correctness property for a table whose incorrect entries would surface only as rare, hard-to-attribute holes. The triangle-count cost is largely offset by analytic gradient normals (FR-024), which keep shading smooth at lower densities. |
| Motion blur limited to **two time samples** | `CPl` stores exactly `data0` and `data1` (`pl.h:125`). This is the representation every primitive in the renderer shares. | Extending `CPl` to N samples would touch every primitive, every hider's motion path, and the network serialization — vastly beyond this feature's scope, and it would violate the "implement effects once, generically" rule by making blobby the reason for a core format change. Recorded as a renderer-wide limitation, not a blobby one. |
