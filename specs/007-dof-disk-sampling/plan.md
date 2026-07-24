# Implementation Plan: Correct and Unify Depth-of-Field Lens Sampling Across Hiders

**Branch**: `007-dof-disk-sampling` | **Date**: 2026-07-24 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/007-dof-disk-sampling/spec.md`

## Summary

The raytrace hider samples its lens/aperture disk with a linear radius
(`r = urand() * aperture`), which is not area-uniform and produces a center-biased circle of
confusion. The REYES hider already samples the disk correctly, via square-to-disk rejection
sampling on a `CSobol<2>` sequence. The fix extracts REYES's existing (already-correct)
rejection-sampling algorithm into one shared, RNG-source-agnostic template function,
`sampleDisk()`, added to `src/ri/random.h` (already included by both hiders — zero new
files). REYES is repointed at it with its unchanged `CSobol<2>` sequence (so its rendered
output, and its visual-regression references, do not change); the raytracer is repointed at
it sourced from its own `urand()`, replacing the buggy polar-mapping block and fixing the
center-bias. A new radial-energy-histogram CLI tool (FR-009) validates the corrected
distribution and cross-checks regenerated raytrace reference images against REYES's
converged output. See [research.md](./research.md) for the full decision record, including
why a broader `stochastic.{h,cpp}` → `reyes` file reorganization requested alongside this fix
is deliberately out of scope (§7).

## Technical Context

**Language/Version**: C++20 (project-wide standard; `CMakeLists.txt` enforces via
`openrender_common_flags`)

**Primary Dependencies**: None new. Reuses `libtiff` (already required by `tests/visual`) and
the project's existing internal headers (`src/ri/random.h`, `src/libshader/shading/shading.h`).

**Storage**: N/A — renderer reads RIB scene files and writes TIFF images; no persistent state.

**Testing**: CTest (project-standard). One new standalone unit-test binary
(`tests/test_disk_sampling.cpp`, registered in `tests/CMakeLists.txt`) exercising
`sampleDisk()` in isolation; the existing visual-regression suite
(`ctest --test-dir build -L visual`) as the integration-level gate for both hiders; two
existing reference images regenerated (`camera-dof-raytrace.tif`,
`camera-motion-small+dof-raytrace.tif`).

**Target Platform**: Unix-like (Linux, macOS) — no platform-specific code introduced.

**Project Type**: Existing single C++ project (renderer core + CLI tools); no new
subprojects. This feature touches `src/ri/` (renderer core) and `tests/` (test suite) only.

**Performance Goals**: DOF rendering time MUST NOT regress by more than 1% vs. the pre-fix
baseline, measured on the project's existing example DOF scenes (SC-005). `sampleDisk()`'s
rejection loop has the same expected iteration count as REYES's current loop (~4/π ≈ 1.27
tries), so no algorithmic slowdown is expected; the raytracer trades one `theta`/`r`/`cosf`/`sinf`
computation for one rejection-loop iteration of comparable cost.

**Constraints**: No new RIB tokens, options, or attributes (FR-004). No change to rendered
output for non-DOF (pinhole) scenes (FR-005). REYES's own DOF visual-regression references
(`camera-dof-reyes`, `camera-motion-small-dof-reyes`) MUST remain zero-diff/unregenerated —
this is both a correctness constraint and a build-time proof that REYES's algorithm was
extracted, not altered.

**Scale/Scope**: One new shared function (`random.h`); two call sites migrated
(`stochastic.cpp`, `raytracer.cpp`); one new unit test; one new validation CLI tool
(`tests/visual/test_radial_histogram.cpp`); two reference images regenerated; one doc update
(`DEVNOTES_DETAILS/HIDER_PARITY.md`). No new hider files; no `stochastic.{h,cpp}` reorg (see
research.md §7).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. Clean Code Standards | PASS | `sampleDisk()` is small, single-purpose, replaces two divergent implementations with one; no magic numbers (disk-normalization constants match existing sibling functions' style). |
| II. Language Standards | PASS | C++20 template function in an existing header; no platform-specific APIs; matches existing `sampleHemisphere`/`sampleSphere` conventions in the same file. |
| III. TDD (NON-NEGOTIABLE) | PASS | `tests/test_disk_sampling.cpp` is written first and fails (no `sampleDisk()` yet) before implementation (research.md §6). Visual-regression suite is the integration-level red/green gate: REYES tests must stay green unmodified; raytrace tests are expected to go red against old references, then get new references validated via the new tool before being committed. |
| IV. Command Line Interface | PASS | New `test_radial_histogram` tool is a standalone CLI (argv in, stdout/exit-code out), matching `test_visual_render.cpp`'s existing convention. |
| V. Minimal Dependencies | PASS | Zero new external dependencies; reuses `libtiff`, already a `tests/visual` requirement. |
| VI. Platform Targeting | PASS | No platform-specific code. |
| VII. Documentation and Site Management | PASS | `DEVNOTES_DETAILS/HIDER_PARITY.md` updated per FR-008. No Hugo `site` docs needed — this is an internal dev-test tool and bug fix, not a new user-facing feature/CLI. |

No violations. Complexity Tracking table not needed.

**Post-design re-check** (after Phase 1 `data-model.md`/`quickstart.md`): unchanged — no new
files, dependencies, or platform-specific code were introduced beyond what's listed in
Project Structure above. Gates still PASS.

## Project Structure

### Documentation (this feature)

```text
specs/007-dof-disk-sampling/
├── plan.md              # This file (/speckit-plan command output)
├── research.md          # Phase 0 output (/speckit-plan command)
├── data-model.md        # Phase 1 output (/speckit-plan command)
├── quickstart.md        # Phase 1 output (/speckit-plan command)
└── tasks.md             # Phase 2 output (/speckit-tasks command - NOT created by /speckit-plan)
```

No `contracts/` directory — openRender is an internal renderer with no external API surface
for this change (no new RIB tokens, no new CLI, no network/service interface).

### Source Code (repository root)

Existing single-project C++ layout (`src/`, `tests/`) — no new subprojects. Only the files
below are touched; everything else in `src/ri/` is unaffected.

```text
src/ri/
├── random.h                 # + new template function sampleDisk() (shared disk sampler)
├── random.cpp                 (unaffected — sampleDisk() is header-only/inline)
├── stochastic.cpp            # rejection-sampling loop (~line 160-188) now calls sampleDisk()
├── raytracer.cpp              # buggy polar mapping in computeSamples() (~line 519-526)
│                               #   replaced with a sampleDisk() call
└── stochastic.h                (unaffected — apertureGenerator member stays as-is)

tests/
├── CMakeLists.txt            # + register test_disk_sampling
├── test_disk_sampling.cpp    # NEW — unit test for sampleDisk() (written first, per TDD)
└── visual/
    ├── CMakeLists.txt        # + register test_radial_histogram, regenerate 2 references
    └── test_radial_histogram.cpp   # NEW — FR-009 radial-energy-histogram CLI tool

examples/rib/tests/references/
├── camera-dof-raytrace.tif                 # regenerated (buggy baseline replaced)
└── camera-motion-small+dof-raytrace.tif    # regenerated (buggy baseline replaced)

DEVNOTES_DETAILS/HIDER_PARITY.md            # + DOF lens-sampling parity entry (FR-008)
```

**Structure Decision**: No new directories or subprojects. The shared sampler lives in the
existing `src/ri/random.h` (already included by both hiders' translation units — see
research.md §3), avoiding any new "noisy" file for the fix itself. The two additions that
*are* new files — `tests/test_disk_sampling.cpp` and `tests/visual/test_radial_histogram.cpp`
— are each independently required: the former by the constitution's non-negotiable TDD gate
(there is no existing test to extend), the latter by spec requirement FR-009 (a net-new
deliverable, not a refactor). Both follow existing conventions exactly (flat-file CTest binary
in `tests/`, libtiff-based CLI tool in `tests/visual/`) rather than introducing a new pattern.

## Complexity Tracking

> **Fill ONLY if Constitution Check has violations that must be justified**

| Violation | Why Needed | Simpler Alternative Rejected Because |
|-----------|------------|-------------------------------------|
| [e.g., 4th project] | [current need] | [why 3 projects insufficient] |
| [e.g., Repository pattern] | [specific problem] | [why direct DB access insufficient] |
