# Implementation Plan: Runtime Bake-on-Load for Non-TIFF Texture Sources

**Branch**: `020-runtime-tile-paging` | **Date**: 2026-09-26 | **Spec**: [spec.md](./spec.md)

**Input**: Feature specification from `/specs/020-runtime-tile-paging/spec.md`

## Summary

`CRenderer::textureLoad()` (`src/ri/texture/texture.cpp`) only understands
baked TIFF textures today: it calls `TIFFOpen()` on the resolved path, and
on failure returns NULL, causing its caller to report the texture as not
found. This plan adds a fallback, reached only when `TIFFOpen()` already
fails: decode the file directly via spec 018's `CImageInput` and serve it
through a new `CSynthesizedTileSource` — a `CTileSource` backend (spec
019's interface) that decodes the source once, builds an in-memory mip
pyramid using the same box-filter reduction and non-power-of-two resize
logic `otexmake`'s bake pipeline already uses, and serves tiles from that
immutable structure. An opt-in, scene-level `Option` persists the prepared
representation to disk (as an ordinary baked TIFF, written via the
existing `makeTexture()` function and read back through the existing,
unmodified `CTiffTileSource` fast path) so repeated renders referencing an
unchanged source skip the decode step entirely. The existing baked-TIFF
path is untouched and always tried first — this is a purely additive
fallback.

## Technical Context

**Language/Version**: C++20 (project standard, constitution II), CMake ≥3.19

**Primary Dependencies**: `libtiff` (existing, mandatory, unchanged);
spec 018's `CImageInput`/`createImageInput()` (PNG mandatory, OpenEXR when
`HAVE_OPENEXR`, RGBE in-tree) — **no new dependency is introduced.**

**Storage**: The opt-in disk cache persists prepared textures as ordinary
baked TIFF files (the existing container format, unchanged) at a
configurable cache root, defaulting to a system temp/cache directory —
not a new file format.

**Testing**: `ctest --test-dir build -L visual --output-on-failure` (must
stay 100% passing — SC-002); `-L texture_tile` (spec 019's existing
byte-identical harness, reused as the baseline-regression check for this
spec too); new visual-regression parity scenes for PNG/EXR/RGBE sources
referenced with no bake step (SC-001); a new byte-identical (`-t:1`)
disk-cache-vs-live-synthesis comparison test (SC-003); a new stale-cache
detection test (SC-004); a new multi-threaded concurrency test for the
synthesized backend, following spec 019's real-render pattern since
`CShadingContext` cannot be hand-constructed (SC-005); a new
multi-**process** concurrent-cache-write test — genuinely new test
infrastructure this project hasn't needed before, since spec 019's
concurrency test used threads within one process, not multiple OS
processes (SC-006/FR-016).

**Target Platform**: Linux (Ubuntu 24.04 baseline) and macOS only
(constitution VI) — no Windows-specific code paths. The atomic
write-then-rename cache-write guarantee relies on POSIX `rename()`'s
same-filesystem atomicity, available on both target platforms.

**Project Type**: Single native C++ monorepo; confined to the runtime
texture-read component (`src/ri/texture/texture.cpp`) plus one new RIB
`Option` class in `src/ri/render/rendererContext.cpp`/`src/ri/parse/ri.cpp`.

**Performance Goals**: None formally set for the in-memory-only path
(spec.md's Assumptions) — the disk cache exists specifically to amortize
decode+synthesis cost across repeated renders (SC-003 proves parity, not
a specific speedup number).

**Constraints**: The existing baked-TIFF fast path (spec 019's
`CTiffTileSource`/`textureLoadBlock()`) MUST NOT change in any way — this
feature's new path is reached only on `TIFFOpen()` failure (FR-002).
Must not modify `CRenderer::environmentLoad()`/any `CEnvironment`
subclass, `CDeepShadow`, `otexmake`'s CLI/container format, `RiMakeTextureV`'s
existing call sites, `CTextureBlock`/`textureMemFlush`/`textureAllocateBlock`/
the per-block mutex, or GitHub issue #19's unrelated open question (same
boundaries spec 019 held). A disk-cache write failure must degrade to
in-memory-only, never fail or alter the render (FR-007). A cache entry
must only ever become visible to a reader once fully written — no reader
may ever observe a partial file (FR-016), achieved via atomic
write-then-rename, not locking.

**Scale/Scope**: 1 new `CTileSource` backend (`CSynthesizedTileSource`,
file-local to `texture.cpp`, matching `CTiffTileSource`'s convention); 1
new fallback branch inside `CRenderer::textureLoad()`; 1 new RIB `Option`
class (2 tokens: enable, cache directory override); 2 existing template
functions (`adjustSize<T>`/`filterScaleImage<T>`, currently file-local to
`texmake.cpp`) relocated to a shared header so `texture.cpp` can reuse
them without re-deriving equivalent resize logic; 1 reused existing
function (`makeTexture()`) as the disk-cache writer, with no changes to
its existing call sites.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.*

| Principle | Assessment |
|---|---|
| I. Clean Code Standards | PASS — one new backend class following an established sibling's exact convention (file-local, factory-exposed); reuses existing resize/bake logic rather than duplicating it; no new complexity added to the existing caching/eviction machinery, which stays untouched. |
| II. Language Standards | PASS — C++20, standard library only (`rename()` is POSIX, already used elsewhere in this codebase's platform-targeted code); no platform-specific APIs beyond what constitution VI already scopes to Linux/macOS. |
| III. TDD (NON-NEGOTIABLE) | PASS, with a process requirement carried into `tasks.md`: every new test (byte-identical disk-cache parity, stale-cache detection, synthesized-backend concurrency, multi-process cache-write safety) MUST be written and demonstrated failing/inapplicable against pre-feature code before the corresponding implementation exists, mirroring spec 019's own TDD sequencing. |
| IV. Command Line Interface | PASS — N/A for the texture-loading code itself; the new RIB `Option` is this feature's only new user-facing surface, and it is exercised the same way every other `Option` already is (via RIB text, already CLI-driven through `orender <rib>`). |
| V. Minimal Dependencies | PASS — zero new external dependencies; only already-existing `CImageInput`/`libtiff`. |
| VI. Platform Targeting | PASS — no OS-specific code beyond the already-portable POSIX `rename()` call, available on both target platforms. |
| VII. Documentation and Site Management | N/A for this branch — no `site/` directory exists (same finding as specs 018/019, unchanged since). |

No violations requiring Complexity Tracking justification.

## Project Structure

### Documentation (this feature)

```text
specs/020-runtime-tile-paging/
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
├── texture.cpp            # MODIFIED — new CSynthesizedTileSource class
│                           # (file-local, matches CTiffTileSource's
│                           # convention); CRenderer::textureLoad() gains
│                           # a fallback branch on TIFFOpen() failure;
│                           # #include "texmake.h" added for makeTexture()
│                           # (disk-cache writer) and the relocated
│                           # adjustSize<T>/filterScaleImage<T> (below)
├── tileSource.h            # MODIFIED — new factory declaration,
│                           # createSynthesizedTileSource(...), mirroring
│                           # createTiffTileSource()'s existing pattern
├── texmake.h               # MODIFIED — adjustSize<T>/filterScaleImage<T>
│                           # relocated here (from texmake.cpp, where they
│                           # were template definitions unreachable from
│                           # any other translation unit) as reusable
│                           # template functions; texmake.cpp's own call
│                           # sites unchanged, now via the header
├── texmake.cpp             # MODIFIED — adjustSize<T>/filterScaleImage<T>
│                           # definitions moved out (see texmake.h above);
│                           # makeTexture() itself unchanged — this spec
│                           # is a new CALLER, not a modification
└── imageInput.h            # UNCHANGED — spec 018's existing interface,
                            # consumed as-is

src/ri/parse/
├── ri.h                    # MODIFIED — new RtToken constant(s) for the
│                           # texture-cache Option class + its tokens
└── ri.cpp                  # MODIFIED — new RtToken definitions (same
                            # pattern as RI_LIMITS/RI_SEARCHPATH/RI_HIDER)

src/ri/render/
├── rendererContext.cpp     # MODIFIED — CRendererContext::RiOptionV()
│                           # gains a new dispatch branch for the
│                           # texture-cache Option class, storing the
│                           # result on COptions
└── rendererFiles.cpp       # UNCHANGED — CRenderer::getTexture()'s
                            # NULL-fallback/CDummyTexture substitution
                            # stays exactly as-is; it's simply reached
                            # less often now

src/ri/state/
└── options.h               # MODIFIED — COptions gains the new
                            # texture-cache setting fields (enable flag +
                            # optional directory override), alongside
                            # existing fields like texturePath

examples/rib/tests/
└── parity/                 # NEW scenes — plain PNG/EXR/RGBE textures
                            # referenced directly (no bake step),
                            # reyes/raytrace parity pairs, modeling spec
                            # 018's own new-format visual regression
                            # precedent

tests/
└── unit/texture_tile/      # EXTENDED — spec 019's existing test
                            # directory gains: disk-cache-parity test,
                            # stale-cache-detection test, synthesized-
                            # backend concurrency test (real render,
                            # spec 019 US2 pattern), and the new
                            # multi-process cache-write-safety test
                            # (naming/exact layout TBD in tasks.md)
```

**Structure Decision**: `CSynthesizedTileSource` stays file-local to
`texture.cpp`, exposed only via a factory in `tileSource.h`
(`createSynthesizedTileSource(...)`) — the same convention spec 019
established for `CTiffTileSource`/`createTiffTileSource()`, since nothing
outside `texture.cpp` needs to reference the concrete class directly
(only the one direct unit test needs the factory, mirroring
`test_tile_source_tiff_info.cpp`'s precedent). `adjustSize<T>`/
`filterScaleImage<T>` move from `texmake.cpp` (where, as template function
*definitions* with no declaration anywhere else, they were physically
unreachable from `texture.cpp`, a different translation unit) into
`texmake.h`, since template definitions must live somewhere every
instantiating translation unit can see them — this is a pure reachability
fix, not a behavior change to either function, and `texmake.cpp`'s own
existing call sites are unaffected. The new RIB `Option` follows the
exact existing dispatch pattern already used for `"limits"`/`"searchpath"`/
`"hider"` (a `strcmp` chain inside `RiOptionV`, no separate declaration
system) rather than introducing a new mechanism.

## Complexity Tracking

*No entries — no Constitution Check violations.*

## Post-Design Constitution Check

Re-checked after Phase 1 (`data-model.md`, `contracts/`, `research.md`):
no new violations. The design stayed within the scope the initial check
assessed — one new backend class, one new fallback branch, one new
`Option` class following an existing dispatch pattern, zero new
dependencies, no CLI surface beyond the RIB `Option` itself, no `site/`
impact. The TDD process requirement (every new regression/parity/
concurrency test written and demonstrated against pre-feature code before
the corresponding implementation exists) carries forward unchanged into
`tasks.md`.
