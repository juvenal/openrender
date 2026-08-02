# Implementation Plan: Reyes/Raytrace Hider Parity Convergence

**Branch**: `008-hider-parity-convergence` | **Date**: 2026-08-01 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `/specs/008-hider-parity-convergence/spec.md`

**Note**: This template is filled in by the `/speckit-plan` command; its definition describes the execution workflow.

## Summary

Converge the `reyes` (`CStochastic`) and `raytrace` (`CRaytracer`) hiders onto
shared sampling, compositing, and filtering code so the same scene produces
comparable output regardless of hider choice, gated by a new cross-hider
parity test harness that must land first. The already-fixed `sampleDisk()`
lens-sampling logic (spec 007) is folded into a new shared `CSampler` (R2)
rather than redone. Four refactors (R1 hider-contract split, R2 shared
sampler, R3 shared transparency/matte compositor, R4 shared pixel-filter
module) and five parity fixes (S1 canonical lens/CoC, S2 displacement
default, S3 depth-filter/z-visibility parity, S4 transparent-hit AOV
compositing, S5 raytraced motion-blur verification) converge the two hiders
without altering the `CFragment`/`CPixel` fragment-list structure that
deep-shadow reads directly. Option B (a shared per-bucket sample table) is a
second stage, gated on R2, that correlates noise between hiders so parity
thresholds can be tightened. New tests are built by extending the existing
`test_visual_render.cpp` / `test_radial_histogram.cpp` TIFF-diff pattern
(same block-average metric, same `add_visual_test`-style CMake macro) rather
than inventing new test infrastructure.

## Technical Context

**Language/Version**: C++20 (project standard; `CMAKE_CXX_STANDARD 20`), C17 for any C-linkage shims.

**Primary Dependencies**: None new. Existing: CMake build, libtiff (`TIFF::TIFF`, used by
`test_visual_render`/`test_radial_histogram`), LLVM (JIT shader backend, untouched by this
feature), the renderer's own `src/common/mathSpec.h` vector/matrix/quaternion primitives.

**Storage**: N/A (in-memory rendering state; output is TIFF/PNG/EXR/RGBE files via existing display drivers).

**Testing**: `ctest` — existing `-L visual` (33+ scene visual-regression suite) and
`-L libshader` (compiler unit tests) labels, plus:
- A new `-L parity` ctest label for the Story 1 cross-hider parity harness, implemented as a
  new small CLI driver (`tests/visual/test_hider_parity.cpp`) that duplicates
  `test_visual_render.cpp`'s `TiffImage`/`readTiff`/`compareTiffs` block-average diff code
  (the same reuse-by-duplication pattern `test_radial_histogram.cpp` already used, per its own
  header comment "same approach as test_visual_render.cpp — no new dependency") but renders
  **two** RIB variants of one scene (one per hider) and diffs the two fresh outputs against
  each other instead of against a static reference image.
- A new `add_parity_test(...)` CMake macro in `tests/visual/CMakeLists.txt`, modeled directly
  on the existing `add_visual_test(...)` macro (same `WORKING_DIRECTORY`/scratch-dir pattern,
  same `ENVIRONMENT "${VISUAL_ENV}"`, same `"visual;regression"`-style labels plus `;parity`).
- Existing `tests/test_disk_sampling.cpp` (chi-square disk-uniformity unit test) and
  `tests/visual/test_radial_histogram.cpp` (radial-energy two-file comparison, already
  supports "candidate vs. ground-truth" mode) are reused unmodified as the acceptance tests
  for R2/S1's lens-sampling behavior — FR-008 requires they keep passing, not be replaced.

**Target Platform**: Linux and macOS (Unix-only per constitution Principle VI); no Windows support.

**Project Type**: Single C++ renderer project (native CLI + libraries), not a web/mobile split.

**Performance Goals**: No hot-loop regression >2-3% (FR-030/SC-007) on
`examples/rib/camera-dof.rib` and a motion scene (`examples/rib/tests/motion-1-reyes.rib` /
`motion-1-raytrace.rib`), measured before/after each refactor (R1-R4), for **both** hiders.
Narrowing the pre-existing raytrace-vs-reyes speed gap is explicitly out of scope.

**Constraints**: Must not alter the `CFragment`/`CPixel` fragment-list data structure in
`stochastic.h` (deep-shadow reads it directly, `stochastic.cpp:1302-1415`). Must not change any
user-facing RIB token/attribute/option except Story 5's documented displacement-default flip
(FR-029). Must not regress the 33+-scene visual-regression suite except where a story
documents an intentional reference-image regeneration (FR-024).

**Scale/Scope**: Touches `src/ri/raytracer.{h,cpp}`, `src/ri/stochastic.{h,cpp}`,
`src/ri/stochasticQuad.h`, `src/ri/reyes.{h,cpp}`, `src/ri/zbuffer.{h,cpp}` (inherits the R1
contract automatically), `src/ri/photon.h` (also sheds stub overrides under R1 — it inherits
`CShadingContext` directly like `CRaytracer`, confirmed via source inspection), `src/ri/object.{h,cpp}`
and every `CSurface` subclass overriding `dice()` (`~27` types: patches, polygons, quadrics,
points, NURBS, implicit surfaces, etc. — see Research §R1), `src/libshader/shading/shading.{h,cpp}`,
`src/ri/renderer.cpp`, `src/ri/random.h` (existing `sampleDisk()` folded into `CSampler`, not
replaced), new `src/ri/sampler.{h,cpp}` and `src/ri/compositor.{h,cpp}` (R2/R3), a new shared
pixel-filter module (R4, exact file TBD in research), new RIB scene pairs under
`examples/rib/tests/parity/`, a new `tests/visual/test_hider_parity.cpp` driver, and
`DEVNOTES_DETAILS/HIDER_PARITY.md` (checkbox updates as each item lands).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Check | Status |
|---|---|---|
| I. Clean Code Standards | R1-R4 each *reduce* duplication (one sampler, one compositor, one filter, one hider contract) rather than add it. No new abstraction beyond what the audit's four refactor items require. | PASS |
| II. Language Standards | New code (`CSampler`, compositor, filter module, parity test driver) is C++20, matching the existing codebase; no new language/toolchain. | PASS |
| III. Test-Driven Development (NON-NEGOTIABLE) | Story 1 (parity harness) is explicitly required to land **before** R1-R4/S1-S5 so every subsequent refactor has a failing/passing gate to develop against (Red-Green-Refactor at the story level). `computeSamples` (raytracer.cpp), `CReyes`/`CZbuffer` construction, and `CRaytracer`'s constructor currently have **zero** covering tests (confirmed via blast-radius analysis) — the new parity scenes plus reused `test_disk_sampling.cpp`/`test_radial_histogram.cpp` close that gap before R2 touches sampling. Per-story acceptance scenarios in spec.md double as the test plan. | PASS |
| IV. Command Line Interface | No new CLI surface beyond the existing `orender`/`ctest` invocation; the new parity driver follows `test_visual_render`'s existing stdin/args→stdout, non-zero-exit-on-failure convention. | PASS |
| V. Minimal Dependencies | Zero new external dependencies — reuses libtiff (already linked), reuses `sampleDisk()`, reuses `CRenderer::pixelFilterKernel`. | PASS |
| VI. Platform Targeting | No platform-specific code introduced; parity driver builds the same way as `test_visual_render`/`test_radial_histogram` on Linux/macOS. | PASS |
| VII. Documentation and Site Management | `DEVNOTES_DETAILS/HIDER_PARITY.md` checkboxes (motion blur, shading interpolation, displacement parity, transparency handling) are updated as each story lands (FR-019, FR-017); no Hugo `site/` content is affected by this renderer-internals feature. | PASS |

No violations requiring justification — Complexity Tracking is empty.

**Post-Design Re-check** (after Phase 1 `data-model.md`/`contracts/`/`quickstart.md`):
All four contracts (`sampler-contract.md`, `compositor-contract.md`,
`filter-module-contract.md`, `hider-contract.md`) keep every new type
(`CSampler`, `CompositeSample`, `CCompositor`, `CPixelFilterAccumulator`)
internal to `src/ri/`, none introduce a new external dependency or RIB
token, and the `hider-contract.md` design generalizes to `zbuffer` and the
backlogged `abuffer` hider via inheritance alone (no new dispatch
mechanism) — reconfirms Principles I, IV, V, and the FR-029 no-new-API
constraint hold after design, not just at the outline stage. No new
violations found; Complexity Tracking remains empty.

## Project Structure

### Documentation (this feature)

```text
specs/008-hider-parity-convergence/
├── plan.md              # This file (/speckit-plan command output)
├── research.md          # Phase 0 output (/speckit-plan command)
├── data-model.md        # Phase 1 output (/speckit-plan command)
├── quickstart.md        # Phase 1 output (/speckit-plan command)
├── contracts/           # Phase 1 output (/speckit-plan command)
└── tasks.md             # Phase 2 output (/speckit-tasks command - NOT created by /speckit-plan)
```

### Source Code (repository root)

```text
src/
├── ri/
│   ├── reyes.h, reyes.cpp              # R1: gains drawObject/drawGrid/drawPoints ownership
│   ├── stochastic.h, stochastic.cpp    # R2 sampler + R3 compositor consumer (rasterBegin/rasterEnd)
│   ├── stochasticQuad.h                # per-quad rasterization touched by R1/R3 fast paths
│   ├── zbuffer.h, zbuffer.cpp          # inherits CReyes already — R1 contract, no direct edits needed
│   ├── raytracer.h, raytracer.cpp      # R1: sheds drawObject/drawGrid/drawPoints stubs;
│   │                                   #   R2/R3/S1-S5 consumer (computeSamples, CPrimaryBundle)
│   ├── photon.h                        # R1: also sheds stub overrides (inherits CShadingContext directly)
│   ├── object.h, object.cpp            # R1: CObject::dice() signature change (CShadingContext* → CReyes*)
│   ├── patches.cpp, polygons.cpp,      # R1 ripple: every CSurface::dice() override updates its
│   │   quadrics.cpp, points.cpp,       #   parameter type to match; S5 motion-blur fixes land here
│   │   implicitSurface.cpp, dlobject.cpp, nurbs*.cpp  # if a per-type bug surfaces
│   ├── sampler.h, sampler.cpp          # NEW — R2 shared CSampler (absorbs sampleDisk(), jitter/time/lens)
│   ├── compositor.h, compositor.cpp    # NEW — R3 shared transparency/matte compositor
│   ├── random.h                        # sampleDisk() stays here; CSampler calls it, doesn't replace it
│   └── renderer.cpp                    # hider-selection strcmp chain; R4 filter module wiring
├── libshader/shading/
│   └── shading.h, shading.cpp          # R1: CShadingContext loses drawObject stub (line 568);
│                                       #   S2 displacement-gating condition (~line 677)
tests/
├── test_disk_sampling.cpp              # REUSED unmodified — R2/S1 regression gate (FR-008)
└── visual/
    ├── CMakeLists.txt                  # add_parity_test(...) macro added alongside add_visual_test(...)
    ├── test_visual_render.cpp          # REUSED unmodified — existing per-hider regression gate
    ├── test_radial_histogram.cpp       # REUSED unmodified — R2/S1 regression gate (FR-008)
    └── test_hider_parity.cpp           # NEW — Story 1 cross-hider parity driver (duplicates
                                        #   test_visual_render.cpp's TiffImage/readTiff/compareTiffs,
                                        #   runs orender twice, diffs the two fresh outputs)
examples/rib/tests/
├── references/                         # existing per-hider reference TIFs (unchanged, except S2's
│                                       #   documented displacement-default reference regeneration)
└── parity/                             # NEW — scene pairs land incrementally as each story's
                                        #   phase completes: flat-shade/DOF/AOV/depth-default
                                        #   (Story 1), transparency/matte/combined-effect (Story 3),
                                        #   depth-filter modes (Story 4), displacement (Story 5),
                                        #   motion ×3 primitive types ×2 translate/deform (Story 6)
DEVNOTES_DETAILS/HIDER_PARITY.md         # checkbox updates as each story lands (FR-017/FR-019)
```

**Structure Decision**: Single-project C++ layout (existing `src/ri/` + `src/libshader/` +
`tests/` + `tests/visual/` + `examples/rib/tests/` — no new top-level directories). All new
production code (`sampler.{h,cpp}`, `compositor.{h,cpp}`) lives beside the hiders it serves in
`src/ri/`, matching how `random.h`'s `sampleDisk()` already lives there. All new test code
extends `tests/visual/` alongside its two existing precedent drivers rather than introducing a
separate test framework or directory convention.

## Complexity Tracking

*No Constitution Check violations — this section is intentionally empty.*
