# Implementation Plan: NURBS Trim Curves (RiTrimCurve)

**Branch**: `009-nurbs-trim-curves` | **Date**: 2026-08-08 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/009-nurbs-trim-curves/spec.md`

**Note**: This template is filled in by the `/speckit-plan` command; its definition describes the execution workflow.

## Summary

Implement `RiTrimCurve` as `CAttributes` push/pop state (four-layer pattern, matching the existing `RI_SHADERFORMAT`
precedent) consumed once by `CNURBSPatchMesh::create()` and stored on the mesh so every per-Bezier-span child shares
one trim definition and one global knot range. Trim resolution is a single hider-agnostic Shared Trim Test: each
trim curve is flattened to a polyline once per surface at mesh-setup time (FR-018), and every tessellation path —
`CPatch::dice()` (reyes-family, z-buffer) and `CTesselationPatch` (ray-tracing) — classifies sampled `(u,v)` vertices
against that polyline with an O(edges) odd-crossing-count test, so trim behavior is identical across hiders (FR-010,
FR-012) with zero new per-hider code. Malformed loops (unclosed, `w <= 0`) are rejected individually with a
dedicated-per-definition diagnostic (FR-017/019/020), not per `ObjectInstance`. The feature is additive-only: when
no trim state is set, `NuPatch` construction and dicing take the exact pre-feature path (FR-004), proven by a new
untrimmed-`NuPatch` visual-regression baseline captured on unmodified `master` *before* any trim code lands (User
Story 4, SC-002), followed by trimmed/sense/multi-loop scenes exercising the new behavior. No new third-party
dependencies: parallel/SIMD work stays inside the C++20 standard library and this feature's own new code, and this
plan documents (without building) the extension seam a future GPU trim classifier and a future CSG boolean test
would both need — the same Shared Trim Test entry point.

## Technical Context

**Language/Version**: C++20 (project-wide; see `CMAKE_CXX_STANDARD 20` and existing `target_compile_features(... cxx_std_20)` usage in `tests/visual/CMakeLists.txt`)

**Primary Dependencies**: None new. Existing: CMake build, `Threads::Threads` (POSIX pthreads, via `osCreateThread`/`renderer.cpp`), LLVM (shading JIT, unrelated to this feature), libtiff (visual-regression comparison tool). No SIMD intrinsics library, no parallel-algorithms backend (TBB/oneDPL), no GPU compute library is introduced — see Research R1–R3 for why.

**Storage**: N/A (in-memory renderer state; trim loops live in `CAttributes` and `CNURBSPatchMesh` for the lifetime of a render, like every other geometry/attribute data in this codebase)

**Testing**: `ctest -L visual` (existing `tests/visual/CMakeLists.txt` `add_visual_test` macro — new scenes registered the same way as the ~85 existing entries); new scenes live under `examples/rib/tests/` with references under `examples/rib/tests/references/`, following the untrimmed-baseline-first ordering mandated by User Story 4/SC-002. No new ctest label or comparison tool is needed — this feature reuses `test_visual_render` and the existing 8×8 block-average diff metric (`VISUAL_THRESHOLD`).

**Target Platform**: Linux / macOS only, per Constitution Principle VI. No Windows-specific code; no platform-conditional trim logic beyond the existing `OPENRENDER_ARCH_X86_64`/`OPENRENDER_ARCH_ARM64` detection in `src/common/align.h`, used only as an optional cache-line-layout hint (Research R2), never as a functional branch.

**Project Type**: Single project — C++ renderer core (`src/ri/`), no separate frontend/backend split.

**Performance Goals**: SC-007 — no measurable regression for scenes with no trim state set (single pointer/flag check added to the existing dice/tessellate hot path). For trimmed surfaces, the per-vertex crossing test must stay O(polyline edges) per FR-018 (amortized flattening cost paid once per `NuPatch` mesh, not per vertex, not per motion sample — trim curves are static per Edge Cases).

**Constraints**: Purely additive (FR-004); identical trim result across every hider via one shared test (FR-010, FR-012); binary accept/reject only, no antialiased trim edges in v1 (FR-011); no `umin`/`umax`/`vmin`/`vmax` implementation (FR-008, explicitly deferred); no new third-party dependency (Constitution Principle V); no code path may nest new thread-level parallelism inside `CPatch::dice()` or `CTesselationPatch::tesselate()`, since both already execute inside one of the renderer's `numThreads` bucket/ray worker threads (`renderer.cpp:1180`, `renderer.cpp:1210` — `osCreateThread(rendererDispatchThread, ...)`; see Research R1 and Known Gotcha #6 in `CLAUDE.md` re: multi-threaded raster early-outs).

**Scale/Scope**: One new RI entry point (`RiTrimCurve`, currently a `CODE_INCAPABLE` stub at `rendererContext.cpp:4094-4100`), one new attribute (`"trimcurve"/"sense"`), new fields on `CAttributes` and `CNURBSPatchMesh`, a new Shared Trim Test module consulted from two existing call sites (`CPatch::dice()` in `surface.cpp`, `CTesselationPatch` in `surface.cpp`/`surface.h`), plus 4+ new visual-regression scenes (untrimmed baseline, single-loop trim, sense inversion, multi-loop). No change to the RIB grammar, Python/Lua bindings, or `CRibOut::RiTrimCurve` (FR-014, FR-015) — trim data storage stays compatible with what `ribOut.cpp:1115-1154` already reads.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Check | Status |
|---|---|---|
| I. Clean Code Standards | Shared Trim Test is one function/module consulted identically by both tessellation paths — no duplicated trim logic per hider. New `CAttributes`/`CNURBSPatchMesh` fields follow existing naming/ownership conventions. | PASS |
| II. Language Standards (C++20/C17) | All new code is C++20; no new language standard requirement. Standard-library-only concurrency (`<execution>`, `std::jthread`) considered in Research R1, adopted only where the toolchain actually supports it. | PASS |
| III. TDD (NON-NEGOTIABLE) | User Story 4 mandates capturing the untrimmed-`NuPatch` regression baseline on unmodified `master` **before** any trim implementation code is written — the baseline scene/reference image is the first task, functioning as the regression test that must stay green throughout implementation. Trimmed/sense/multi-loop scenes are added as each corresponding story's acceptance test. | PASS |
| IV. Command Line Interface | No new CLI surface. `RiTrimCurve` is a RIB/RI-API call, not a new tool; `-writerib` round-trip (`ribOut.cpp`) already handles it. | PASS |
| V. Minimal Dependencies | No new third-party dependency. Research R1 confirms the toolchain-available concurrency facilities (or lack thereof) before committing to any parallel construct, per "graceful degradation for optional deps." | PASS |
| VI. Platform Targeting | Linux/macOS only; no Windows-specific code. Architecture-detection macros already exist (`align.h`) and are reused only as an optional layout hint. | PASS |
| VII. Documentation and Site Management | `DEVNOTES_DETAILS/RISPEC_GAPS.md:9`'s stale `rendererContext.cpp:3527` reference is corrected to the actual stub location and checked off (FR-016); `DEVNOTES.md` status table updated. No `site/` Hugo content is affected by this renderer-internals feature beyond that. | PASS |

No violations requiring justification — Complexity Tracking is empty.

**Post-Design Re-check** (after Phase 1): The Shared Trim Test module and the `CAttributes`/`CNURBSPatchMesh` field additions in `data-model.md` stay within the existing class ownership/deep-copy conventions (Principle I) and introduce no new dependency (Principle V) — confirmed against `contracts/attribute-contract.md` and `contracts/shared-trim-test-contract.md`. The GPU and CSG forward-looking notes are documentation only (no interface committed), so they add no complexity for Principle I to flag. Gate re-confirmed: PASS, no changes to the table above.

## Project Structure

### Documentation (this feature)

```text
specs/009-nurbs-trim-curves/
├── plan.md              # This file (/speckit-plan command output)
├── research.md          # Phase 0 output (/speckit-plan command)
├── data-model.md        # Phase 1 output (/speckit-plan command)
├── quickstart.md        # Phase 1 output (/speckit-plan command)
├── contracts/           # Phase 1 output (/speckit-plan command)
│   ├── attribute-contract.md
│   └── shared-trim-test-contract.md
└── tasks.md              # Phase 2 output (/speckit-tasks command - NOT created by /speckit-plan)
```

### Source Code (repository root)

```text
src/ri/
├── ri.h                        # RI_TRIMCURVE / "trimcurve","sense" token constants (new, alongside existing RI_SHADERFORMAT-style tokens)
├── ri.cpp                      # Token registration; VALID_ATTRIBUTE_BLOCKS dispatch for RiTrimCurve already exists at ri.cpp:1629-1636 (currently routes to the stub)
├── rendererContext.cpp         # RiTrimCurve(...) body replacing the CODE_INCAPABLE stub (currently rendererContext.cpp:4094-4100); RiAttributeV() parsing for "trimcurve"/"sense" (new block, modeled on the RI_SHADERFORMAT block at rendererContext.cpp:3336-3338, NOT the unrelated RiOption RI_SHADERFORMAT handling at rendererContext.cpp:1732-1747)
├── rendererDeclarations.cpp    # initDeclarations() pre-declaration for "trimcurve"/"sense" (new entry, modeled on rendererDeclarations.cpp:179)
├── attributes.h                # CAttributes: new pending-trim-loop fields + trim sense field (near existing heap-owned attribute fields, class body starts attributes.h:92)
├── attributes.cpp               # CAttributes::CAttributes(const CAttributes*) deep-copy (attributes.cpp:156-211) and ~CAttributes() free (attributes.cpp:219-264): both MUST gain the new fields (FR-013); CAttributes::find() query support for "trimcurve"/"sense" (modeled on attributes.cpp:651-655)
├── patches.h                    # CNURBSPatchMesh (patches.h:167-186): new owned Shared Trim Test / flattened-loop-polyline field(s), global knot range already implicit via existing mesh fields
├── patches.cpp                   # CNURBSPatchMesh::create() (patches.cpp:1823-1891): consumes pending CAttributes trim state once per mesh, builds the Shared Trim Test (flattens loops, validates weights/closure per FR-017/019), stores it on the mesh; per-span children (constructed at patches.cpp:1874) reference the mesh's test, not a copy
├── surface.cpp                   # CPatch::dice() (surface.cpp:141+): new trim-classification call in the per-vertex probe/grid-fill stage, gated behind a single "trim state present" check (FR-004); CTesselationPatch::tesselate()/splitToChildren() (surface.cpp:1450,1914): same Shared Trim Test call for the ray-tracing on-demand path
├── surface.h                     # CTesselationPatch declaration (surface.h:61): no new public surface, consumes the mesh-owned Shared Trim Test
└── ribOut.cpp                    # CRibOut::RiTrimCurve (ribOut.cpp:1115-1154): verified compatible with the new storage layout, or updated to match (FR-014)

src/common/
├── algebra.h                     # NOT modified by this feature — existing vector/matrix typedefs (algebra.h:31-39) stay as-is; new trim-curve polyline buffers are new, separate, contiguous POD arrays, not a layout change to shared math types
└── align.h                       # OPENRENDER_ARCH_X86_64/ARM64/CACHE_LINE_SIZE (align.h:55-68) optionally reused for new-buffer layout hints only

DEVNOTES_DETAILS/
└── RISPEC_GAPS.md                # Line 9 stale-reference correction + resolved checkbox (FR-016)

DEVNOTES.md                       # Status table update (FR-016)

examples/rib/tests/
├── nupatch-vase-untrimmed.rib          # User Story 4 baseline (captured FIRST, before trim code lands)
├── nupatch-vase-trimmed-hole.rib       # User Story 1
├── nupatch-vase-trimmed-sense.rib      # User Story 3
├── nupatch-vase-trimmed-multiloop.rib  # User Story 5
└── references/
    └── *.tif                            # Matching reference images for each scene above

tests/visual/
└── CMakeLists.txt                # New add_visual_test(...) entries for the four scenes above, following the existing macro/registration pattern (CMakeLists.txt:86)
```

**Structure Decision**: Single-project structure (this is the existing C++ renderer core, `src/ri/`) — no new top-level
directory, module, or build target. Every touched file already exists; this feature adds fields, functions, and RIB
scenes within the established layout rather than introducing new architectural units. The only new files are
documentation artifacts under `specs/009-nurbs-trim-curves/` and new RIB test scenes + reference images under
`examples/rib/tests/`, both additive to existing directories.

## Complexity Tracking

*No Constitution Check violations — this section is intentionally empty.*
