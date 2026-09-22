# Implementation Plan: Multi-Format Texture Source Decoding for otexmake

**Branch**: `018-multi-format-texture-decode` | **Date**: 2026-09-22 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/018-multi-format-texture-decode/spec.md`

## Summary

`otexmake`'s bake pipeline (`src/ri/texture/texmake.cpp`) can currently only
read TIFF source images — every bake path (plain texture, side/cubic/
spherical/cylindrical environment, shadow) calls a shared `readLayer()`
helper that hits `libtiff` directly, with no abstraction. This plan
introduces a small, compiled-in C++ image-decode abstraction
(`CImageInput` + one concrete subclass per format) and migrates all five
`readLayer()` call sites onto it, so `otexmake` can bake from PNG, OpenEXR,
or RGBE (Radiance HDR) sources in addition to TIFF — auto-detected by
extension, with native sample precision preserved, zero colorspace
conversion, and the baked-texture *output* format completely unchanged.
TIFF-source bakes must remain byte-for-byte identical after the refactor.

## Technical Context

**Language/Version**: C++20 (project standard, constitution II), CMake ≥3.19

**Primary Dependencies**: `libtiff` (existing, mandatory — already
`find_package(TIFF REQUIRED)`), `libpng` (existing, mandatory — already
`find_package(PNG REQUIRED)`), OpenEXR/Imf (existing, optional —
`find_package(OpenEXR QUIET CONFIG)` gated by `HAVE_OPENEXR`, root
`CMakeLists.txt:383-412`), in-tree RGBE codec (`src/display/rgbe/rgbe.cpp`/
`rgbe.h` — no external library). **No new dependency is introduced.**

**Storage**: N/A — reads local image files, writes the existing baked-TIFF
container; no database or persistent service state.

**Testing**: `ctest --test-dir build -L visual --output-on-failure`
(existing visual-regression harness); new `cmp`-based byte-identical
regression test for TIFF-source bakes; new visual-regression scenes for
PNG/EXR/RGBE-sourced textures compared as reyes/raytrace parity pairs.

**Target Platform**: Linux (Ubuntu 24.04 baseline) and macOS only
(constitution VI) — no Windows-specific code paths.

**Project Type**: Single native C++ monorepo; this feature is confined to
the texture-baking component (`src/ri/texture/`) used by the `otexmake` CLI
tool and the `orender` binary's build.

**Performance Goals**: None specified beyond correctness — `otexmake` is an
offline, artist-invoked batch tool with no existing latency/throughput
target in this codebase; no regression in TIFF-source bake time is
expected since that code path is untouched in substance, only moved behind
a virtual call.

**Constraints**: No new `otexmake` CLI flags; byte-for-byte identical
output for existing TIFF-source bakes; no OpenImageIO dependency; no
colorspace/gamma conversion; must not modify the runtime
`texture()`/`environment()` read path (`src/ri/texture/texture.cpp`) or the
baked-texture container format.

**Scale/Scope**: 5 `readLayer()` call sites in one file
(`src/ri/texture/texmake.cpp`, lines 776/853/941/996/1054, each preceded by
a raw `TIFFOpen()` at 758/817/925/977/1036) migrated onto the new
abstraction; 4 concrete decoders (TIFF, PNG, OpenEXR, RGBE) plus one
abstract base and one small dispatch factory.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Assessment |
|---|---|
| I. Clean Code Standards | PASS — new abstraction is a small set of focused, single-responsibility classes (one abstract base, one factory, one concrete decoder per format); no change forces complexity into `appendLayer`/`appendPyramid` (the unaffected output side). |
| II. Language Standards | PASS — C++20, standard library only; no platform-specific APIs planned. |
| III. TDD (NON-NEGOTIABLE) | PASS, with a process requirement carried into `tasks.md`: decoder unit tests and the byte-identical TIFF regression test MUST be written and failing before any concrete `CImageInput` subclass or call-site migration is implemented. |
| IV. Command Line Interface | PASS — no interface change; format selection is fully auto-detected, `otexmake`'s existing CLI surface is untouched. |
| V. Minimal Dependencies | PASS — this feature adds **zero** new dependencies. TIFF/PNG are already mandatory; OpenEXR is already optional/`HAVE_OPENEXR`-gated; RGBE is already in-tree. `COpenExrImageInput` must degrade gracefully (compile out, clear runtime error if a `.exr` source is given) when `HAVE_OPENEXR` is off, matching existing project convention. |
| VI. Platform Targeting | PASS — no OS-specific code introduced; existing Linux/macOS-only dependencies unaffected. |
| VII. Documentation and Site Management | N/A for this branch — no `site/` directory exists at `master`@733bbfb (verified: `ls site` → not found). This is a pre-existing condition on this branch, not a gap introduced by this feature; no site documentation action is taken here. |

No violations requiring Complexity Tracking justification.

## Project Structure

### Documentation (this feature)

```text
specs/018-multi-format-texture-decode/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md         # Phase 1 output
├── quickstart.md         # Phase 1 output
├── contracts/            # Phase 1 output
└── tasks.md              # Phase 2 output (/speckit.tasks — not created here)
```

### Source Code (repository root)

Real paths on this branch (`master`@`733bbfb`), verified directly rather
than assumed from prior exploration on a different branch:

```text
src/ri/texture/
├── texmake.cpp          # MODIFIED — 5 readLayer() call sites migrated to createImageInput()
├── texmake.h
├── imageInput.h          # NEW — CImageInfo, abstract CImageInput base, createImageInput() factory
├── imageInput.cpp        # NEW — factory dispatch (by file extension)
├── imageInputTiff.h/.cpp # NEW — CTiffImageInput (wraps current readLayer() logic verbatim)
├── imageInputPng.h/.cpp  # NEW — CPngImageInput (RGB/RGBA/Grayscale/Grayscale+Alpha only)
├── imageInputExr.h/.cpp  # NEW — COpenExrImageInput, HAVE_OPENEXR-gated (single-part RGB/RGBA/luminance only)
├── imageInputRgbe.h/.cpp # NEW — CRgbeImageInput (wires up existing src/display/rgbe/rgbe.h decode calls)
├── texture.cpp/.h        # UNCHANGED — runtime read path, explicitly out of scope for this spec
└── (brickmap/pointCloud/pointHierarchy/texture3d — UNCHANGED, unrelated point-cloud pipeline)

src/display/rgbe/
└── rgbe.h/.cpp           # Content UNCHANGED — RGBE_ReadHeader/RGBE_ReadPixels already
                           # implemented here; imageInputRgbe.cpp calls these, does not
                           # reimplement or move them. Build wiring DOES change: rgbe.cpp
                           # must additionally be compiled into the src/ri targets (today
                           # it's only compiled into the rgbe.dsply MODULE, which orender/
                           # otexmake never link against) — see research.md §5 addendum.

src/ri/CMakeLists.txt      # MODIFIED — add new imageInput*.cpp sources to the existing
                           # texture/*.cpp source lists (same targets that already build
                           # texmake.cpp); add conditional HAVE_OPENEXR compile define +
                           # OPENRENDER_OPENEXR_LIBS link for imageInputExr.cpp only

tests/
├── unit/                  # NEW — per-decoder round-trip unit tests (tiny synthetic fixtures)
│   └── image_input/
└── visual/                # NEW scenes — PNG/EXR/RGBE-sourced texture bakes, reyes+raytrace parity
```

**Structure Decision**: New files live alongside `texmake.cpp` inside the
existing `src/ri/texture/` directory (not a new top-level module) because
that directory is already the texture-baking component's home, is already
included via `include_directories(.../texture)` in `src/ri/CMakeLists.txt`,
and its `.cpp` files are already added directly to the same target source
lists `texmake.cpp` belongs to (confirmed at `src/ri/CMakeLists.txt`
lines ~127-129, ~293, ~387) — new decoder files should be added to those
same source lists rather than introducing a new library target, which
would be unjustified complexity for four small classes.

## Complexity Tracking

*No entries — no Constitution Check violations.*

## Post-Design Constitution Check

Re-checked after Phase 1 (`data-model.md`, `contracts/`, `research.md`):
no new violations. The design stayed within the scope the initial check
assessed — one abstract interface, four small concrete decoders, zero new
dependencies, no CLI surface change, no `site/` impact. The TDD process
requirement (decoder unit tests + the byte-identical TIFF regression test
written and failing before implementation) carries forward unchanged into
`tasks.md`.
