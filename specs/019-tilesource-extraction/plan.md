# Implementation Plan: Runtime Texture Tile-Fetch Abstraction (CTileSource)

**Branch**: `019-tilesource-extraction` | **Date**: 2026-09-25 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/019-tilesource-extraction/spec.md`

## Summary

The renderer's runtime texture-lookup path (`texture()`/`environment()`
shading calls) reads baked textures tile-by-tile on demand via
`textureLoadBlock()` in `src/ri/texture/texture.cpp`, which calls
`TIFFOpen`/`TIFFSetDirectory`/`TIFFReadTile`/`TIFFReadScanline`/`TIFFClose`
directly inline. This plan introduces a small `CTileSource` abstraction at
that exact seam and migrates the existing TIFF-backed read path onto it —
`CTiffTileSource` is the only concrete backend, a byte-for-byte
behavior-preserving extraction of `textureLoadBlock`'s existing logic,
including its two currently-dead branches (partial-sub-region reads,
separate-planar-config layout), which are kept verbatim rather than
pruned. This is a pure refactor with zero intended behavior change — the
groundwork spec 020 (a separate, later effort) needs to let the renderer
natively read non-TIFF-sourced textures at render time without rewriting
this hot shading-time path per format. This plan also adds a genuine
multi-threaded concurrency test, closing a real, pre-existing gap (no
existing test exercises concurrent tile-fetch today).

## Technical Context

**Language/Version**: C++20 (project standard, constitution II), CMake ≥3.19

**Primary Dependencies**: `libtiff` (existing, mandatory — already
`find_package(TIFF REQUIRED)`, unchanged from spec 018). **No new
dependency is introduced.**

**Storage**: N/A — reads existing baked-texture files; no new persistent
state, no change to the baked-texture container format.

**Testing**: `ctest --test-dir build -L visual --output-on-failure`
(existing visual-regression harness, must stay 100% passing); a new
`cmp`-based byte-identical rendered-output regression test (stricter than
the visual-diff metric); a new multi-threaded concurrency stress test
(genuinely new coverage — no equivalent exists today).

**Target Platform**: Linux (Ubuntu 24.04 baseline) and macOS only
(constitution VI) — no Windows-specific code paths.

**Project Type**: Single native C++ monorepo; this feature is confined to
the runtime texture-read component (`src/ri/texture/texture.cpp`) used by
the `orender` binary's shading-time texture/environment/shadow lookups.

**Performance Goals**: None formally set — this refactor replaces direct
inline calls with one layer of virtual-call indirection; per spec.md's
Assumptions, this is expected to be negligible and is not gated by a
measured threshold, consistent with how spec 018's equivalent bake-side
refactor was treated.

**Constraints**: Zero intended behavior change (byte-identical rendered
output, see Testing above); the two currently-unreachable branches inside
`textureLoadBlock` (partial-sub-region read, `PLANARCONFIG_SEPARATE`) must
be preserved verbatim inside `CTiffTileSource`, not deleted; must not
modify `CTextureBlock`, `textureMemFlush`, `textureAllocateBlock`, or the
per-block mutex (the caching/eviction policy around the fetch); must not
modify `otexmake`/the bake-time write path or the baked-texture container
format; must not modify `CDeepShadow` or `CShadow`'s separate one-time
projection-matrix metadata read (a different mechanism entirely from the
per-tile pixel fetch this spec changes).

**Scale/Scope**: 1 function generalized (`textureLoadBlock()`, ~187
lines), 2 call sites migrated (`CBasicTexture<T>::lookupPixel()`,
`CTiledTexture<T>::lookupPixel()`'s tile-access macro), 1 concrete
`CTileSource` backend (`CTiffTileSource`), plus the construction sites in
`readMadeTexture()`/`readTexture()` (`src/ri/texture/texture.cpp`, called
from `CRenderer::textureLoad()`/`environmentLoad()`) where each
`CTiledTexture`/`CBasicTexture` layer's `(filename, directory)` pair is
replaced by a `CTileSource*` (or a shared `CTileSource*` plus a `level`
index — see `research.md`).

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Assessment |
|---|---|
| I. Clean Code Standards | PASS — one small abstract base (`CTileSource`) plus one concrete backend (`CTiffTileSource`); no new complexity added to `CTextureBlock`/eviction/locking, which stay untouched. |
| II. Language Standards | PASS — C++20, standard library only; no platform-specific APIs. |
| III. TDD (NON-NEGOTIABLE) | PASS, with a process requirement carried into `tasks.md`: the byte-identical rendered-output test and the new multi-threaded concurrency test MUST be written first — the concurrency test written and run as a *baseline* against the pre-refactor code (there is nothing to make it "fail" against, since it's new coverage, not a regression test; it must pass both before and after), the byte-identical test written and confirmed to capture real pre-refactor output before any `CTileSource` code exists. |
| IV. Command Line Interface | PASS — N/A; this is purely internal render-time code with no CLI surface of its own. |
| V. Minimal Dependencies | PASS — zero new dependencies; `libtiff` was already mandatory. |
| VI. Platform Targeting | PASS — no OS-specific code introduced. |
| VII. Documentation and Site Management | N/A for this branch — no `site/` directory exists (same finding as spec 018; unchanged since). |

No violations requiring Complexity Tracking justification.

## Project Structure

### Documentation (this feature)

```text
specs/019-tilesource-extraction/
├── plan.md              # This file
├── research.md          # Phase 0 output
├── data-model.md         # Phase 1 output
├── quickstart.md         # Phase 1 output
├── contracts/            # Phase 1 output
└── tasks.md              # Phase 2 output (/speckit.tasks — not created here)
```

### Source Code (repository root)

```text
src/ri/texture/
├── texture.cpp           # MODIFIED — textureLoadBlock() extracted into
│                          # CTiffTileSource::fetchTile(); CTiledTexture<T>/
│                          # CBasicTexture<T> hold a CTileSource* instead of
│                          # a raw filename+directory pair; readMadeTexture()/
│                          # readTexture() construct CTiffTileSource instances
├── texture.h              # UNCHANGED — public texture-system interface
│                          # (CTextureInfoBase/CTexture/CEnvironment/etc.);
│                          # CTextureBlock/CTextureLayer/CTiledTexture/
│                          # CBasicTexture/CMadeTexture remain file-local to
│                          # texture.cpp, matching current convention
├── tileSource.h           # NEW — CTileLevelInfo, abstract CTileSource base
│                          # (analogous to imageInput.h from spec 018; a
│                          # small shared header since spec 020 will add
│                          # further CTileSource backends in their own files)
└── texmake.cpp/.h         # UNCHANGED — bake-time write path, untouched

src/ri/CMakeLists.txt      # MODIFIED — add tileSource.h to the ri_headers
                           # glob (already covered by the existing
                           # texture/*.h glob pattern — verify, likely no
                           # change needed) and, if CTiffTileSource ends up
                           # in a separate .cpp rather than file-local in
                           # texture.cpp, add that new source to ri_sources
                           # (see research.md for the file-local vs.
                           # separate-file decision)

tests/
└── unit/texture_tile/     # NEW — byte-identical rendered-output regression
                           # test and the new multi-threaded concurrency
                           # stress test (naming/location TBD in tasks.md,
                           # following the tests/unit/image_input/ precedent
                           # from spec 018)
```

**Structure Decision**: `CTileSource`/`CTileLevelInfo` get a new, small
header (`tileSource.h`) rather than being declared file-local in
`texture.cpp` — even though this spec's only consumer of the interface is
`texture.cpp` itself, spec 020 (a separate, later effort) is already known
to need to add further `CTileSource` backends, most naturally each in
their own `.cpp` file (mirroring spec 018's `imageInputPng.cpp`/
`imageInputExr.cpp`/`imageInputRgbe.cpp` pattern) — those files will need
to include this header. `CTiffTileSource` itself, by contrast, stays as
close to `textureLoadBlock`'s current file-local convention as reasonably
possible (see `research.md` for the final call) since nothing outside
`texture.cpp` needs to reference it directly in this spec.

## Complexity Tracking

*No entries — no Constitution Check violations.*

## Post-Design Constitution Check

Re-checked after Phase 1 (`data-model.md`, `contracts/`, `research.md`):
no new violations. The design stayed within the scope the initial check
assessed — one small abstract interface, one concrete backend, zero new
dependencies, no CLI surface, no `site/` impact. The TDD process
requirement (byte-identical render test + new concurrency test, written
before the `CTileSource` implementation) carries forward unchanged into
`tasks.md`.
