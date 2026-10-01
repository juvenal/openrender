# Implementation Plan: Deferred, On-Demand Evaluation of Solid (CSG) Geometry

**Branch**: `021-deferred-csg-evaluation` | **Date**: 2026-09-30 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/021-deferred-csg-evaluation/spec.md`

## Summary

Today every solid is baked into a camera-blind, BSP-booleaned polygon mesh at
`RiSolidEnd`. That produces spiky seams, crack arcs and polygonal openings,
identically in every hider. This feature keeps each solid as its CSG tree over
the original primitives for the whole frame, and resolves the composite surface
on demand through the generic `CObject` interface: buckets drive `dice()`, rays
drive `intersect()`.

The technique is **CSG as spatial trimming**:
- Each leaf renders as itself, at native, view-dependent precision.
- It is restricted by a geometry-owned *region predicate*: "is this point on the
  composite boundary?" The predicate is built from exact point classification of
  the other operands: analytic for quadrics, polygons and bilinear patches;
  Newton-refined winding for free-form surfaces.
- It reuses the spec 009 NURBS-trim hook shape.
- REYES terminates seam splitting by screen size and drops absent micropolygons
  through a generic coverage mask. The raytracer rejects non-boundary hits in
  place.
- Shading is untouched. The hiders gain no CSG knowledge.
- Procedural operands become legal, expanded lazily.
- Open single-quadric operands get implied caps.
- The BSP kernel is retired.

See [research.md](research.md) for decisions and rejected alternatives.

## Technical Context

**Language/Version**: C++20 (renderer), C17 where C. `-Werror` on Clang/GCC.

**Primary Dependencies**: None new. Existing: libtiff/libpng/zlib/OpenEXR (vcpkg
opt-in on macOS), LLVM JIT (unaffected), flex/bison.

**Storage**: N/A. Memory only: shared CSG definitions plus per-instance state
built on first use.

**Testing**: CTest. Unit suites in `tests/unit/csg/`; visual regression in
`tests/visual/CMakeLists.txt` (`add_visual_test`, 8×8 block-average diff,
thresholds 20–40/255); cross-hider parity (label `parity`,
`tests/visual/test_hider_parity.cpp`); a new seam-metric helper; performance
comparison via `tests/visual/test_perf_compare.cpp`.

**Target Platform**: macOS (Apple Silicon, primary dev) and Linux (Ubuntu 24.04
baseline).

**Project Type**: CLI renderer plus libraries (`libri`/`libRIBRender`).

**Performance Goals**: every existing CSG scene and the hero scene render in
under 10× their master baseline (SC-007). Hero baseline: raytrace 0.31 s / 1.42 s;
REYES 0.19 s / 0.40 s; Z-buffer 0.16 s / 0.34 s at 800×500 / 1600×1000.

**Constraints**:
- No CSG code in `src/ri/hiders/` or `src/libshader/shading/` (FR-012; grep gate
  in quickstart §7).
- Non-CSG output bit-identical (FR-020).
- Classification deterministic per point (FR-008).
- Thread-safe lazy state (`CRenderer::delayedMutex`).

**Scale/Scope**:
- About 3 new geometry source files.
- About 8 modified files across geometry, render context, the REYES grid
  plumbing and stats.
- About 3,500 lines retired: the BSP kernel, the fragment builders, possibly the
  013 adaptive-tessellation helpers.
- About 10 unit suites rewritten or added; about 6 new visual scenes.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. Clean Code | PASS | Replaces the 755-line `csgTree.cpp` plus the 697-line BSP kernel with focused units (definition, classification, region adapter, caps). Every function has a single responsibility. |
| II. Language Standards | PASS | C++20, no extensions; existing `CObject`/`CSurface` patterns. |
| III. TDD (non-negotiable) | PASS (plan) | Every phase starts with failing unit or visual tests (research Decision 13); references are regenerated only after the tests are red. |
| IV. CLI | PASS | The behavior is reached through the existing `orender` CLI and RIB. The new statistics counters appear in the existing statistics output. |
| V. Minimal Dependencies | PASS | No new dependency; the BSP kernel is removed rather than a geometry library added. |
| VI. Platform Targeting | PASS | Portable C++; no platform code. Linux validation via the existing CI and Linux builds. |
| VII. Documentation | PASS (plan) | Updates: the Hugo page `docs/site/content/manual/reference/solid-csg-operations.md`, `DEVNOTES.md`, `DEVNOTES_DETAILS/RISPEC_GAPS.md`, `CLAUDE.md` known-gotchas, and superseded notes in `specs/013-solid-csg-operations/research.md` (SC-009). |

**Post-design re-check (after Phase 1)**: still PASS.
- The only cross-domain additions are two *generic* hooks:
  - a region predicate on `CSurface`
  - a coverage mask on `CRasterGrid`
- Neither adds a dependency from hiders or shading onto CSG
  ([contracts/geometry-region-interface.md](contracts/geometry-region-interface.md)).
- No violations, so Complexity Tracking stays empty.

## Project Structure

### Documentation (this feature)

```text
specs/021-deferred-csg-evaluation/
├── plan.md              # This file
├── research.md          # Phase 0: 14 decisions + rejected alternatives
├── data-model.md        # Phase 1: entities, state transitions, rules
├── quickstart.md        # Phase 1: runnable validation checks
├── contracts/
│   ├── rib-solid-interface.md        # User-facing RIB/Ri behavior
│   └── geometry-region-interface.md  # Geometry ↔ hider ↔ shading boundary
├── checklists/requirements.md
└── tasks.md             # Phase 2 (/speckit.tasks; not created here)
```

### Source Code (repository root)

```text
src/ri/geometry/
├── csgTree.{h,cpp}          # MODIFY: capture tree + validation kept; resolveCSGTree → build CSG definition;
│                            #         procedural capture allowed; BSP/tessellation/fragment code removed
├── csgDefinition.{h,cpp}    # NEW: immutable shared definition (flattened tree, operands, flips, CSG-aware bound)
├── csgSolid.{h,cpp}         # NEW: CObject solid handle + per-instance state (replaces solidObject.{h,cpp})
├── csgClassify.{h,cpp}      # NEW: operand classification (analytic + winding), boundary rule, two-sided ON rule,
│                            #      region adapter implementing the generic predicate
├── csgCaps.{h,cpp}          # NEW: implied-closure caps for single-quadric operands (FR-021)
├── csgBoolean.{h,cpp}       # DELETE (BSP kernel)
├── solidObject.{h,cpp}      # DELETE (replaced by csgSolid)
├── object.h                 # MODIFY: CSurface generic region-predicate hook + countCrossings() virtual
├── surface.cpp              # MODIFY: dice() region stage (uniform shortcut, screen-size seam termination);
│                            #         CTesselationPatch hit acceptance consults region
├── quadrics.{h,cpp}         # MODIFY: countCrossings (analytic, all roots); region check in root loops
├── polygons.{h,cpp}         # MODIFY: countCrossings (exact); region check at hit acceptance
├── patches.{h,cpp}          # MODIFY: countCrossings (bilinear analytic; bicubic/NURBS Newton-refined)
└── subdivision*.{h,cpp}     # MODIFY (as needed): countCrossings via existing patch/polygon forms

src/ri/render/
├── rendererContext.{h,cpp}  # MODIFY: SolidEnd builds definition; operand-capture mode for procedural expansion;
│                            #         remove processDelayedSolid; tessellationtolerance once-per-render notice
└── geometryDispatch.cpp     # MODIFY (if needed): shared helpers next to checkRayGuard (classification rays bypass it)

src/ri/hiders/
├── reyes.h                  # MODIFY: CRasterGrid generic `coverage` mask field
└── reyes.cpp                # MODIFY: drawGrid fills the mask from the surface's region hook; per-quad bounds
                             #         computation gives absent quads an empty bound (no rasterizer loop changes)

src/ri/core/stats.{h,cpp}    # MODIFY: CSG counters (SC-006)
src/ri/state/attributes.h    # MODIFY (comment only): tessellationTolerance documented as inert

tests/unit/csg/
├── CMakeLists.txt           # MODIFY: register rewritten/new suites; drop retired ones
├── csgTestUtils.h           # MODIFY: analytic point sets replace volume-sign helpers
├── test_boolean_*.cpp, test_nested_tree.cpp, test_multi_primitive_leaf.cpp  # REWRITE as classification/boundary tests
├── test_interior_exterior_selection.cpp   # KEEP/UPDATE (provenance)
├── test_*_tesselation*.cpp, test_tesselation_flatness.cpp  # RETIRE with the helpers they cover (research D11)
├── test_classify_quadrics.cpp, test_count_crossings.cpp, test_winding_robustness.cpp,
│   test_implied_caps.cpp, test_boundary_rule.cpp, test_classify_determinism.cpp,
│   test_procedural_operand.cpp   # NEW

tests/visual/CMakeLists.txt  # MODIFY: register hero ×3 hiders ×2 resolutions, caps, coincident, procedural,
                             #         off-screen stats scenes; parity pairs; move csg-malformed-procedural to visual
tests/visual/                # NEW helper: seam metric (stray pixels in analytic regions + radial seam deviation)
examples/rib/tests/          # NEW: csg-hero-*.rib, csg-open-quadric-caps-*.rib, csg-coincident-*.rib,
                             #      csg-procedural-operand-*.rib (+ archive), csg-offscreen-stats.rib

docs/site/content/manual/reference/solid-csg-operations.md, DEVNOTES.md,
DEVNOTES_DETAILS/RISPEC_GAPS.md, CLAUDE.md, specs/013-solid-csg-operations/research.md   # MODIFY (docs)
```

**Structure Decision**: single project; work stays inside the existing domain
layout. CSG lives entirely in `src/ri/geometry/`. The render context wires
capture and expansion. The hiders receive one generic grid field. Shading is
unchanged.

## Implementation phases (input to /speckit.tasks)

Each phase is a pause-and-commit point for the maintainer (stage only).

0. **Baseline and harness.** Promote the Step 0 measurement into the seam-metric
   helper. Add the hero, caps, coincident, procedural and off-screen scenes. Record
   master timings. All new tests fail or mark expected failure against the
   current output. *(US1–US4 tests red.)*
1. **Geometry core, no rendering change.** CSG definition, CSG-aware bounds,
   `countCrossings` per primitive type, analytic implied-closed quadric
   classification, winding with robustness, boundary rule, static orientation,
   two-sided ON rule, determinism. Unit suites rewritten or added and green.
2. **CSG solid and per-instance state.** Replace `CSolidObject`; instantiate
   leaves on first use; move the `SOLID_FRAGMENT` flag and orientation onto
   cloned leaf attributes; instancing; statistics counters.
3. **Raytrace path.** Region check at every hit-acceptance point; secondary rays;
   displaced-leaf handling. **Exit criteria: raytrace only.** The hero and
   sphere-cube *raytrace* seam metrics go green (SC-001 to SC-003 for raytrace).
   REYES and Z-buffer scenes are expected to stay red until phase 4, because
   their seam accuracy depends on the screen-size split termination and
   coverage mask introduced there.
4. **REYES / Z-buffer path.** Region stage in `CSurface::dice()` (uniform
   shortcut, screen-size seam termination, keep-at-cap); generic coverage mask
   plus empty per-quad bounds, with generically named hooks (contract §0).
   **Exit criteria**: REYES and Z-buffer seam metrics go green (SC-001 to SC-003
   for all three hiders), and CSG parity pairs pass (SC-004).
5. **Implied closure caps** (FR-021) for single-quadric operands.
6. **Procedural operands** (FR-024/025): operand-capture mode, lazy recursive
   expansion, nested-`SolidBegin` diagnostic, empty-operand semantics;
   `csg-malformed-procedural-in-primitive` becomes a visual test (SC-010).
7. **Settings and retirement.** Make `tessellationtolerance` inert with a
   once-per-render notice. Delete the BSP kernel, the fragment builders,
   `CSolidObject`, `processDelayedSolid`, and the unused 013 tessellation helpers
   and their suites.
8. **Validation and docs.** Deliberately regenerate CSG references (before/after
   crops); run full `ctest -L visual` sequentially; timing vs. baseline (SC-007);
   grep gate; documentation (SC-009).

## Complexity Tracking

No constitution violations to justify.
