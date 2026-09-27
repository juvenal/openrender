# Tasks: Runtime Bake-on-Load for Non-TIFF Texture Sources

**Input**: Design documents from `/specs/020-runtime-tile-paging/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Included and REQUIRED — constitution Principle III (TDD,
NON-NEGOTIABLE) mandates the Foundational phase's new example scenes
exist and are demonstrated to genuinely fail (Red) before any
`CSynthesizedTileSource`/fallback implementation code is written, and
every User Story's own regression/parity/concurrency test is written
before (or alongside, where a test IS the deliverable, per plan.md's
Constitution Check) the code it verifies.

**Organization**: Sequential, not parallel, same as spec 019: US1 (P1) is
the core capability every later story depends on existing first; US2
(P2) and US3 (P3) each add a property of what US1 produces (disk-cache
reuse, concurrency safety) and cannot start meaningfully before US1
lands.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: US1 (reference an unbaked source directly), US2 (opt-in
  disk cache), US3 (synthesized-backend concurrency safety)

## Path Conventions

Single C++ monorepo project. New example scenes referencing unbaked
sources follow spec 018/019's existing `examples/rib/tests/parity/`
convention; new unit/regression tests extend spec 019's existing
`tests/unit/texture_tile/` directory rather than creating a new one.
Reused fixtures (spec 018's `tests/unit/image_input/fixtures/`) are
referenced directly via an extended `TEXTURES` search path, not copied —
matching spec 019's own established "reuse, don't duplicate" practice.

---

## Phase 1: Setup

**Purpose**: Confirm the pre-feature baseline is clean before any spec
020 code is written.

- [X] T001 Confirm a clean baseline: `cmake --build build --config
      Release`, then `ctest --test-dir build -L visual
      --output-on-failure`, `ctest --test-dir build -L texture_tile
      --output-on-failure`, and `ctest --test-dir build -L image_input
      --output-on-failure`; all three must be 100% green. Confirmed:
      207/207 visual, 3/3 texture_tile, 6/6 image_input.

**Checkpoint**: Baseline confirmed clean; safe to begin.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Author the new example scenes this spec's core capability
(User Story 1) must make pass, and confirm they genuinely fail today
(Red — the feature doesn't exist yet), before any implementation code
exists. Also reconfirm spec 019's existing byte-identical regression bar
as the SC-002 starting point this spec must not break. **No user story
phase may begin until this phase's checkpoint passes.**

- [X] T002 [P] Confirm reusable unbaked-source fixtures: spec 018's
      `tests/unit/image_input/fixtures/large_rgb8.png` (512x512, 8-bit
      RGB), `large_rgb.exr` (512x512, RGB float, `HAVE_OPENEXR`-gated),
      and `large.hdr` (512x512, Radiance HDR/RGBE) — all with known
      closed-form per-pixel formulas already documented in that
      directory's own `FIXTURES.md`, all already sized for real
      tile/mip-pyramid coverage (16x16 tiles per level, full 10-level
      pyramid, per that file's own note). No new fixtures generated —
      reuse only. Confirmed: all 3 files exist; this build has
      `HAVE_OPENEXR` enabled (openexr.dsply built, `Parity_texture-exr`/
      `ImageInput_Exr` registered), so the EXR scene pair will be
      exercised, not gated off.
- [X] T002b [P] Generate one new, checked-in **non-power-of-two** PNG
      fixture, `tests/unit/texture_tile/fixtures/nonpot_rgb.png` (e.g.
      500x300 — deliberately not a power of two and not a multiple of
      `DEFAULT_TILE_SIZE`), reusing the same closed-form RGB formula
      style as `tests/unit/image_input/fixtures/FIXTURES.md`
      (`R(x,y)=(3x+10) mod 256` etc.) so exact values stay assertable;
      document its provenance/formula in a new
      `tests/unit/texture_tile/fixtures/FIXTURES.md`. No suitable
      existing fixture is non-power-of-two (every `tiny_*`/`large_*`
      fixture from spec 018 is 4x4 or 512x512) — this is new test data,
      not a duplication of existing coverage (analysis finding C1;
      FR-012, spec.md's own non-power-of-two Edge Case). Generated
      (Pillow + numpy), verified pixel (10,20)=(40,120,80) matches the
      formula exactly. Appended its provenance section to the *existing*
      `FIXTURES.md` (spec 019's `concurrency_rgb.tex` entry, preserved —
      caught and fixed an accidental overwrite before it was committed).
- [X] T002c [P] Wire up the `TEXTURES` search path so the fixtures T002/
      T002b reuse are actually resolvable once T003's scenes are
      registered (analysis findings F1/F2 — a fresh `/speckit.analyze`
      pass, checking the actual current CMake files rather than assuming,
      found neither existing suite searches
      `tests/unit/image_input/fixtures/` at all): extend `VISUAL_ENV`'s
      `TEXTURES` in `tests/visual/CMakeLists.txt` (currently only
      `${CMAKE_SOURCE_DIR}/examples/rib/tests`) to also include
      `${CMAKE_SOURCE_DIR}/tests/unit/image_input/fixtures`; extend
      `TEXTURE_TILE_ENV`'s `TEXTURES` in
      `tests/unit/texture_tile/CMakeLists.txt` (currently
      `examples/rib/tests:${TEXTURE_TILE_FIXTURES_DIR}`) the same way.
      Without this, every scene T003/T019-T027 author would fail to
      resolve `large_rgb8.png`/`large_rgb.exr`/`large.hdr` at ctest time
      — a search-path miss, not a real feature regression — since T011
      registers T003's scenes into the `VISUAL_ENV`-driven suite and
      T022/T027 register T019-T021/T024-T027 into the
      `TEXTURE_TILE_ENV`-driven one, and neither currently searches
      where spec 018 actually left these fixtures.
- [X] T003 Author 4 new example RIB scene pairs (reyes+raytrace) in
      `examples/rib/tests/parity/`, modeled directly on spec 018's
      `texture-{png,exr,rgbe}-{reyes,raytrace}.rib` but referencing the
      T002/T002b fixtures **directly** as `Surface "paintedplastic"
      "texturename" [...]` — no `otexmake` bake step, no `.tex`
      intermediate at all:
      `unbaked-png-{reyes,raytrace}.rib` (`large_rgb8.png`),
      `unbaked-exr-{reyes,raytrace}.rib` (`large_rgb.exr`,
      `HAVE_OPENEXR`-gated), `unbaked-rgbe-{reyes,raytrace}.rib`
      (`large.hdr`), `unbaked-nonpot-{reyes,raytrace}.rib`
      (`nonpot_rgb.png`, T002b — exercises FR-012's non-power-of-two
      resize path end to end, closing analysis finding C1). Unlike spec
      018's own PNG scene (which deliberately used a grayscale source to
      sidestep a since-fixed, bake-time-only `PHOTOMETRIC` gap — commit
      `4a6d6f6`, already on `master`), these scenes use full RGB fixtures
      throughout: the gap was specific to `otexmake`'s bake-time
      `appendLayer()` path, which this spec's unbaked-source path never
      goes through at all (depends on T002, T002b).
- [X] T004 Smoke-check T003's 4 scene pairs against the current
      (pre-spec-020) `orender` binary: confirm each fails to render the
      texture correctly today — reported as `error(CODE_NOFILE, "Failed
      open texture ...")` and substituted with `CDummyTexture`
      (untextured/inert output), the existing, correct behavior for a
      file `TIFFOpen()` can't read — establishing genuine Red state
      before any implementation exists, per constitution III. Confirmed
      all 4: each prints `Failed open texture "..."` and still renders
      (untextured) (depends on T003).
- [X] T004b [P] Confirm suitable negative-test fixtures already exist for
      User Story 1 Acceptance Scenario 3 (a file `CImageInput` itself
      correctly refuses to decode): spec 018's
      `tests/unit/image_input/fixtures/tiny_indexed.png` (palette-
      indexed, must be rejected) and `large_unsupported_channels.exr`
      (unsupported channel layout, `HAVE_OPENEXR`-gated) are already
      established negative-test fixtures — no new fixture needed.
      Confirmed both exist. Actually *rendering* a scene against one of
      these to prove graceful degradation is T010c, once the fallback
      exists (analysis finding C4).
- [X] T005 Confirm spec 019's existing byte-identical regression harness
      (`ctest --test-dir build -R TextureTile_RenderByteIdentical
      --output-on-failure`) still passes against the current,
      pre-spec-020 build — the SC-002 starting point ("100% of existing
      scenes referencing an already-baked texture... continue to produce
      byte-for-byte identical output") this spec must not regress.
      Confirmed passing (depends on T001).

**Checkpoint**: 4 new unbaked-source scene pairs (including the
non-power-of-two one, T002b/T003) exist, are checked in, and are
confirmed to genuinely fail today. The `TEXTURES` search path actually
resolves the fixtures they reference (T002c). Suitable negative-test
fixtures for graceful-degradation coverage are confirmed to already
exist. The existing byte-identical regression bar is reconfirmed green.
User Story 1's actual implementation can now begin.

---

## Phase 3: User Story 1 - Reference an unbaked image directly as a texture (Priority: P1) 🎯 MVP

**Goal**: `CRenderer::textureLoad()` gains a fallback, reached only when
`TIFFOpen()` fails, that decodes a supported plain image via
`CImageInput` and serves it through a new `CSynthesizedTileSource`
backend — making Foundational's new scenes render correctly, with zero
change to the existing baked-TIFF fast path.

**Independent Test**: Render the 4 new scene pairs from Phase 2; confirm
each now renders the texture correctly, including the non-power-of-two
one. Bake one fixture and confirm the baked vs. unbaked renders are
visually equivalent (Acceptance Scenario 2). Confirm a genuinely
undecodable file still degrades gracefully (Acceptance Scenario 3).
Separately, re-run spec 019's byte-identical harness; confirm it still
passes unchanged.

### Implementation for User Story 1

- [X] T006 [US1] Relocate `adjustSize<T>`/`filterScaleImage<T>` template
      *definitions* from `src/ri/texture/texmake.cpp` into
      `src/ri/texture/texmake.h` (research.md §4) — a pure reachability
      fix (both were physically unreachable from any other translation
      unit as template definitions living only in a `.cpp` file), zero
      behavior change to either function or to `texmake.cpp`'s own
      existing call sites (which gain a `#include "texmake.h"` if not
      already present). Build; confirm `ctest -L visual` and `-L
      image_input` are still 100% green after this one, isolated,
      mechanical change before touching anything else.
      **Scope correction found during implementation**: `adjustSize<T>`'s
      own body also calls `copyData<T>`/`initData<T>`/`initDataValues<T>`/
      `filterImage<T>` internally, and references the file-scope
      `resizeDownMode`/`resizeRoundMode`/`resizeNoneMode` string
      constants (`resizeUpMode` stays in `texmake.cpp` — used only by an
      unrelated macro, not by anything relocated) — all of these needed
      to move together for `adjustSize<T>` to actually be instantiable
      from `texture.cpp`, since a template must be fully visible at
      every point it's instantiated. Relocated the full transitive
      closure (6 template functions + 3 constants); added `#include
      "memory.h"`/`"renderer.h"`/`<math.h>`/`<string.h>` to `texmake.h`
      for their own dependencies. `texmake.cpp`'s own `appendTexture()`
      caller is unaffected — same zero-behavior-change guarantee, just a
      larger mechanical move than originally scoped. Verified: builds
      clean, 207/207 visual, 6/6 image_input (depends on T005).
- [X] T007 [US1] Define `CSynthesizedPyramid` (data-model.md) and
      `CSynthesizedTileSource` (file-local to `texture.cpp`, matching
      `CTiffTileSource`'s existing convention — per
      `contracts/synthesized-tile-source.md`) in
      `src/ri/texture/texture.cpp`: `CSynthesizedPyramid` holds one
      buffer per mip level; construction resizes a decoded, non-power-
      of-two source up to the nearest power of two first (via T006's
      relocated `adjustSize<T>`/`filterScaleImage<T>`, `RiCatmullRomFilter`
      default, matching `otexmake`'s own CLI default — research.md §3),
      then reduces level-by-level using the identical 2x2-block-average
      math `appendPyramid<T>()` already performs (`texmake.cpp:200-273`),
      producing `tiffNumLevels(width, height)` levels total.
      `CSynthesizedTileSource::info()`/`fetchTile()` implement the
      `CTileSource` contract per `contracts/synthesized-tile-source.md`
      (`DEFAULT_TILE_SIZE`-square tiles, partial trailing tiles handled
      the same way `libtiff`'s own tiled-image API already does — no new
      zero-fill/padding logic) (depends on T006).
      **Thread-safety requirement found during implementation** (research.md
      §4a, plan.md Constraints): `adjustSize<T>`/`filterScaleImage<T>`/
      `filterImage<T>` allocate from `CRenderer::globalMemory`, an
      unsynchronized stack-based bump allocator (confirmed via full read of
      `src/ri/core/memory.h` — no lock anywhere in `ralloc()`/`memBegin`/
      `memEnd`); `textureLoad()` (this task's call site) is reachable from
      multiple shading threads concurrently (`getTexture()` is called from
      the `texture()`/`environment()` RSL builtin,
      `src/libshader/shading/rslBuiltins.cpp:181`). This task MUST wrap its
      entire decode+resize+pyramid-reduction body in a new mutex, following
      this project's own existing convention rather than `std::mutex`: add
      `static TMutex synthesizeMutex;` to `CRenderer` in
      `src/ri/render/renderer.h` (alongside the 11 existing project-wide
      mutexes, e.g. `textureMutex`) and `osCreateMutex`/`osDeleteMutex` it
      in `CRenderer::initMutexes()`/`shutdownMutexes()`
      (`src/ri/render/rendererMutexes.cpp`), then `osLock(CRenderer::
      synthesizeMutex)`/`osUnlock(...)` around the synthesis body in
      `texture.cpp` (same `osLock`/`osUnlock` idiom already used elsewhere
      in this file, e.g. `textureMemFlush()`). Do NOT reuse the existing
      `CRenderer::textureMutex` for this — under this project's build
      (`TEXTURE_PERBLOCK_LOCK` always defined), it currently serializes
      only `textureMemFlush()`'s eviction scan, and holding it for this
      task's slower decode/resize work would give it a second, unrelated
      meaning and could block unrelated texture-memory eviction elsewhere
      (research.md §4a). Bracket the arena use with `memBegin(CRenderer::
      globalMemory)`/`memEnd(CRenderer::globalMemory)` held for the same
      duration as the mutex, and copy resized/reduced results out into
      `CSynthesizedPyramid`'s own heap (`std::vector`) buffers *before* the
      matching `memEnd()` (no early return between begin/end, per
      `memory.h`'s own comment). `fetchTile()` itself stays lock-free,
      reading only the finished, immutable pyramid. This is a new hazard
      introduced by this feature's own code, distinct from the
      pre-existing, separately-filed `frameFiles`/`CTrie` concurrent-first-
      load race (GitHub #20, out of scope for this spec, same boundary as
      GitHub #19).
- [X] T008 [US1] Add the factory `createSynthesizedTileSource(filename,
      level)` to `src/ri/texture/tileSource.h` (declaration) and
      `texture.cpp` (definition, per `contracts/synthesized-tile-source.md`),
      plus the internal helper that decodes a source exactly once via
      `createImageInput()`/`CImageInput`, builds the full
      `CSynthesizedPyramid` (T007), and constructs one
      `CSynthesizedTileSource` per level, each holding a
      `std::shared_ptr<CSynthesizedPyramid>` into the same shared
      structure (research.md §1) — no per-level re-decoding (depends on
      T007).
- [X] T009 [US1] Add the new fallback branch inside
      `CRenderer::textureLoad()` (`texture.cpp:2281-2316`): on
      `TIFFOpen()` failure, attempt `createImageInput(fn)`; on success,
      build one `CTiledTexture<T>` layer per level from T008's factory
      (mirroring `readMadeTexture()`'s existing per-level layer-array
      construction shape, `CMadeTexture`/`numLayers`/`layers[]`
      unchanged) with periodic wrap mode both axes (FR-013's resolved
      default); on `CImageInput` failure too (unrecognized/unsupported
      format, or decode failure), fall through to the existing,
      unmodified NULL-return path (`CRenderer::getTexture()`'s
      `CDummyTexture` substitution, `rendererFiles.cpp:373`, untouched)
      (depends on T008).
      **Scope correction found during implementation** (research.md §4b):
      a manual smoke render surfaced that `TIFFOpen(fn, "r")` on a
      genuine PNG/EXR/RGBE source (the exact case this task's own
      fallback exists to serve) makes libtiff invoke the already-
      registered `tiffErrorHandler()` -> `error(CODE_SYSTEM, "Not a
      TIFF...")`, which sets the global `RiLastError` and makes
      `orender`'s own exit code nonzero (`orender.cpp:919`) even though
      the fallback then succeeds and the render is pixel-correct.
      `test_hider_parity.cpp:339-343` (the harness T011's own parity
      tests run under) treats any nonzero `orender` exit code as an
      outright failure before ever comparing pixels — meaning every one
      of T011's 4 new scene pairs would fail permanently, regardless of
      correctness, without a fix. Two fixes were considered and rejected
      (both would touch shared, unsynchronized global state --
      `TIFFSetErrorHandler`'s process-global handler pointer, or the
      global `RiLastError` -- from a call path reachable by multiple
      concurrent shading threads, research.md §4a's same class of
      hazard). The fix actually applied: a new file-local
      `looksLikeTiff(fn)` helper checks fn's first 4 bytes against
      TIFF's own magic number (both byte orders) *before* ever calling
      `TIFFOpen()`; `TIFFOpen()` is only attempted when the magic
      matches (or the check itself couldn't be performed, e.g. an
      unreadable file — defaults to attempting `TIFFOpen()` regardless,
      never silently diverting a real TIFF). A valid-magic-but-corrupt
      TIFF still reaches `TIFFOpen()` and still reports a real error, so
      no existing correct-error-reporting behavior is lost — only the
      spurious "not a TIFF at all" case (now this spec's own normal,
      successful path) is silenced. GitHub #21 filed for the underlying,
      broader architectural facts (process-global libtiff handler,
      unsynchronized `RiLastError`) this correction's rejected
      alternatives ran into, out of scope for this spec to fix generally.
      Verified: `Not a TIFF` message and nonzero exit code both gone for
      the 4 new unbaked scenes; `orender`'s exit code is 0 for a
      successful synthesized-fallback render.
- [X] T010 [US1] Build; render Phase 2's 4 new scene pairs (PNG, EXR,
      RGBE, and the non-power-of-two PNG); confirm each now renders the
      texture correctly (Green) — visual smoke-check before trusting
      them as permanent regression coverage (depends on T009, T002c).
      Verified via manual render (not just "a TIF was produced"): all 4
      unbaked-source scenes (raytrace hider; PNG also spot-checked under
      reyes) render with `orender` exiting 0 and no error output, after
      T009's own `looksLikeTiff()` scope correction.
- [X] T010b [US1] Prove User Story 1 Acceptance Scenario 2 (baked vs.
      unbaked visual equivalence, analysis finding C2): bake
      `large_rgb8.png` via `otexmake` into a `.tex`; render two otherwise
      identical scenes — one referencing the baked `.tex`, one
      referencing `large_rgb8.png` directly (T003's own
      `unbaked-png-reyes.rib`) — compare via the existing block-average
      parity mechanism (`test_hider_parity`/`add_parity_test`'s own
      comparison tool), confirming the two are visually equivalent, not
      merely that the unbaked one renders *something* plausible (depends
      on T010).
      Verified manually (ahead of T011's permanent registration) for all
      4 formats, not just PNG: baked-vs-unbaked `test_hider_parity`
      comparisons (raytrace hider, `-t:1`) for PNG, the non-power-of-two
      PNG (`nonpot_rgb.png`, exercising `adjustSize<T>`/
      `filterScaleImage<T>` end to end), EXR, and RGBE all report
      **MaxBlockAvgDiff: 0.00** against a threshold of 20 — pixel-
      identical, not merely within tolerance. This single check exercises
      `CSynthesizedPyramid`'s `validWidth`/`validHeight` propagation, the
      per-bit-depth `M` normalization in `readSynthesizedTexture<T>()`,
      the fixed-`DEFAULT_TILE_SIZE` tiling decision, and the box-filter
      pyramid reduction all at once.
- [X] T010c [US1] Prove User Story 1 Acceptance Scenario 3 holds
      post-implementation (analysis finding C4): render a scene
      referencing `tiny_indexed.png` (or, when `HAVE_OPENEXR`,
      `large_unsupported_channels.exr` — both already-established
      negative-test fixtures per T004b) as `texturename` — confirm the
      render still succeeds, with `CDummyTexture` substitution and the
      expected `error(CODE_NOFILE, "Failed open texture ...")` message,
      and no crash, proving `CImageInput`'s own decode-failure reporting
      falls through T009's fallback cleanly (depends on T009, T004b,
      T002c).
      Verified via manual render against `tiny_indexed.png`: renders to
      completion (no crash), prints exactly `Failed open texture
      "tiny_indexed.png"`, substitutes `CDummyTexture` — confirming
      `CImageInput`'s own decode failure (indexed PNG, an unsupported
      layout) falls through T009's fallback to the existing, unmodified
      NULL-return path cleanly. Exit code 255 here is the *correct*,
      intentional signal for a genuine failure — unlike T009's
      `looksLikeTiff()` correction, which addresses only the spurious
      case where the overall load actually succeeds.
- [X] T011 [US1] Register the 4 new scene pairs as permanent
      `add_parity_test` entries in `tests/visual/CMakeLists.txt`,
      promoting Phase 2's authored-but-unregistered scenes into the
      standing visual-regression suite, matching spec 018/019's own
      registration convention (depends on T010, T010b, T010c).
      **Scope correction found during implementation**: `unbaked-nonpot`
      references `nonpot_rgb.png` (T002b), which lives under
      `tests/unit/texture_tile/fixtures/`, not
      `tests/unit/image_input/fixtures/` — `VISUAL_ENV`'s `TEXTURES`
      (extended for F1/F2) didn't search that directory either. Extended
      it a second time to include `tests/unit/texture_tile/fixtures`,
      catching what would otherwise have been the same class of
      search-path-miss bug F1/F2 already found once. Verified: all 4
      new tests (`Parity_unbaked-png`, `-nonpot`, `-exr`, `-rgbe`) pass
      via `ctest -R Parity_unbaked` (4/4, ~0.3s each — cheap, since a
      parity test's own comparison threshold of 20 is generous relative
      to these pixel-identical renders).
- [X] T012 [US1] Write a direct unit test for
      `CSynthesizedTileSource::info()`/`fetchTile()` against
      `large_rgb8.png` (mirroring `test_tile_source_tiff_info.cpp`'s
      existing pattern): assert level-0 pixel values match the
      closed-form formula already documented in
      `tests/unit/image_input/fixtures/FIXTURES.md` exactly (level 0 is
      a direct, power-of-two-already resample with no reduction to
      account for — `large_rgb8.png` is already 512x512) — proving the
      decode+pyramid-construction pipeline is correct at the source,
      independent of any full render. This is a standalone C++ binary,
      not a RIB render — it never goes through the renderer's `TEXTURES`
      search path at all, so T002c's fix doesn't reach it (analysis
      finding F3): read the fixture path from its own
      `IMAGE_INPUT_FIXTURES_DIR` environment variable instead, matching
      spec 018's own established convention
      (`tests/unit/image_input/test_image_input_tiff.cpp`'s
      `getenv("IMAGE_INPUT_FIXTURES_DIR")`) rather than
      `TEXTURE_TILE_FIXTURES_DIR` (which points at this spec's own,
      different, fixtures directory). Register as ctest
      **`TileSource_SynthesizedInfo`** (`texture_tile` label, analysis
      finding G2 — every new test this spec adds gets a concrete,
      fixed name at task-authoring time, matching spec 019's own
      established practice, rather than being left for the implementer
      to invent) in `tests/unit/texture_tile/CMakeLists.txt` with
      `ENVIRONMENT
      "IMAGE_INPUT_FIXTURES_DIR=${CMAKE_SOURCE_DIR}/tests/unit/image_input/fixtures"`
      added alongside its existing `TEXTURE_TILE_FIXTURES_DIR` entry
      (depends on T008). Needs `PNG::PNG` linked (not just `TIFF::TIFF`,
      unlike `test_tile_source_tiff_info`), since
      `createSynthesizedTileSource()` decodes via
      `CImageInput`/`createImageInput()`.
      **Scope correction found during implementation**: a first version
      of this test (calling `createSynthesizedTileSource()` with no
      further setup, mirroring `test_tile_source_tiff_info.cpp` exactly)
      segfaulted twice, for two distinct reasons neither `CTiffTileSource`
      nor its own test ever had to contend with, since that backend
      touches neither: (1) `adjustSize<T>`/`filterScaleImage<T>`/
      `filterImage<T>` (T006/T007) allocate from `CRenderer::globalMemory`,
      which is a bare global initialized to `NULL`
      (`rendererStatics.cpp:106`) and only ever set up inside
      `CRenderer::beginRenderer()`; (2) `CImageInput::open()`'s own
      failure-reporting paths call `error()`, which depends on the global
      `renderMan` singleton, likewise only initialized as part of the same
      renderer lifecycle. Both are unavailable in a bare standalone
      binary. Fixed by calling `RiBegin(RI_NULL)`/`RiEnd()` around the
      test body — the exact same minimal-context initialization
      `test_image_input_png.cpp`/`test_image_input_tiff.cpp` already use
      for the identical reason (confirmed via their own header comments),
      not a new pattern invented for this spec. Verified via `lldb`
      backtraces for both crashes before applying the fix, then a clean
      pass after. Registered `ctest` result: `TileSource_SynthesizedInfo`
      passes (all level-0/level-1 geometry checks, two pixel-exact tile
      spot-checks including one away from the origin, and both
      out-of-range/unrecognized-source rejection checks).
- [X] T013 [US1] Run `ctest --test-dir build -L visual
      --output-on-failure` in full; confirm 100% passing (existing
      scenes plus the 4 newly-registered ones) (depends on T011, T012).
      Confirmed: full suite green, no failures (two earlier apparent
      timeouts on `teapot-wood-raytrace`/`teapot-motion-raytrace` during
      an earlier, resource-contended run were confirmed as CPU
      contention from concurrent background builds, not real failures —
      both pass in isolation in 1s/47s well under their timeout; a
      subsequent clean run confirmed 100% passing with no contention).
- [X] T014 [US1] Run `ctest --test-dir build -R
      TextureTile_RenderByteIdentical --output-on-failure`; confirm it
      still passes unchanged — proves SC-002 (the existing baked-TIFF
      fast path is completely unaffected) holds after this story's
      changes (depends on T013). Confirmed passing (6.98s) as part of
      the full `texture_tile` suite run (4/4 passing:
      `TextureTile_RenderByteIdentical`, `TileSource_Concurrency`,
      `TileSource_TiffInfo`, `TileSource_SynthesizedInfo`).

**Checkpoint**: An artist can reference a plain PNG/EXR/RGBE image
directly as a texture, with no bake step, and it renders correctly.
Existing baked-TIFF behavior is provably unaffected. MVP delivered.

---

## Phase 4: User Story 2 - Repeated renders skip re-decoding the same unbaked source (Priority: P2)

**Goal**: An opt-in, scene-level `Option "texturecache"` persists the
prepared (synthesized) representation of an unbaked source to disk as an
ordinary baked TIFF (via the existing `makeTexture()`), read back through
the completely unmodified `CTiffTileSource` fast path on a cache hit —
with atomic write-then-rename guaranteeing a reader never observes a
corrupted or partial entry, even when multiple render processes race to
write the same one.

**Independent Test**: Render the same unbaked-source scene twice with the
cache enabled; confirm the second render reuses the cached entry and
produces byte-identical (`-t:1`) output to the first. Modify the source
and render again; confirm the stale cache is detected and rebuilt.
Launch several concurrent render processes against the same missing
cache entry; confirm no reader ever observes a corrupted file.

### Implementation for User Story 2

- [ ] T015 [US2] Add `textureCacheEnabled` (bool, default `false`) and
      `textureCacheDirectory` (nullable owned string, default unset) to
      `COptions` (`src/ri/state/options.h`), per `data-model.md`'s
      `COptions` additions table (depends on T014).
- [ ] T016 [US2] Add a new `RtToken` constant (`src/ri/parse/ri.h`/
      `ri.cpp`, matching `RI_LIMITS`/`RI_SEARCHPATH`/`RI_HIDER`'s
      existing pattern) for the `"texturecache"` `Option` class, and a
      new dispatch branch inside `CRendererContext::RiOptionV()`
      (`rendererContext.cpp:1455`) recognizing its `"enable"`/
      `"directory"` tokens (per
      `contracts/texturecache-option.md`), storing results on the T015
      `COptions` fields. An unrecognized token under this class reports
      via the same existing `error(CODE_BADTOKEN, ...)` path every other
      `Option` class already uses (depends on T015).
- [ ] T017 [US2] Implement the disk-cache key/lookup: a small helper
      computing `<hash-of-absolute-source-path>-<source-mtime-epoch-
      seconds>.tex` (`std::hash<std::string>`, research.md §5) under the
      configured (or default system temp/cache) directory. Inside
      `textureLoad()`'s fallback branch (T009), when the cache is
      enabled, attempt `TIFFOpen()` on this exact resolved path *before*
      falling to `CSynthesizedTileSource` — a hit reads back through the
      completely ordinary, unmodified `CTiffTileSource` path (T009's own
      `TIFFOpen`-succeeds branch, no new read-side code) (depends on
      T016).
- [ ] T018 [US2] Implement the cache-write path on a miss (cache
      enabled, no entry found at the T017 key): call `makeTexture()`
      (`texmake.h`) with `sourcePath`, a temporary/uniquely-named path in
      the *same directory* as the final cache path (guaranteeing a
      same-filesystem `rename()`), the existing `TSearchpath*`,
      `"periodic"`/`"periodic"` wrap modes, `RiCatmullRomFilter` at
      width/height 3.0 (matching `otexmake`'s own actual CLI default
      filter size, `otexmake.cpp:74-75` — **corrected during T007**, an
      earlier version of this task said 1.0, which would be a materially
      narrower filter and not match a real `otexmake` bake of the same
      source), and no extra params so `makeTexture()`'s own
      `getResizeMode()` defaults the resize mode to `"up"` (research.md
      §8; §3's correction re-verified this default directly against
      `otexmake.cpp`/`texmake.cpp` rather than trusting an earlier
      grounding pass, which had wrongly said "round"); on success,
      `rename()` the temporary file into the final path (research.md §6)
      — a reader only ever attempts the final path, never the temporary
      one. A write failure (unwritable location) is caught and degrades
      to T009's in-memory-only `CSynthesizedTileSource` path for that
      render, never failing or altering it (FR-007) (depends on T017).
- [ ] T018b [US2] Verify the *default* cache-directory resolution
      (analysis finding U1): render a scene with `Option "texturecache"
      "enable" [1]` alone — no `"directory"` token at all, the most
      common configuration a user would actually try first — confirm a
      cache entry is successfully written to and read back from whatever
      the resolved default system temp/cache location actually is on
      this platform, not just the explicit-override path T017/T018 were
      otherwise exercised against. Ctest name: **`TextureTile_CacheDefaultDirectory`**
      (analysis finding G2) (depends on T018).
- [ ] T019 [US2] Author a new scene + script proving SC-003: render the
      same unbaked-source scene (`-t:1`) twice — once with the disk
      cache freshly populated (reads back via `CTiffTileSource`), once
      with the cache disabled (reads via `CSynthesizedTileSource`
      directly) — `cmp` the two outputs byte-for-byte (research.md §9).
      Ctest name: **`TextureTile_CacheByteIdentical`** (analysis finding
      G2) (depends on T018).
- [ ] T020 [US2] Author a new test proving SC-004 (stale-cache
      detection): populate the cache for a fixture source, modify
      (touch) the source file's mtime, re-render, and confirm (a) the
      render output reflects the *modified* source, not stale data, and
      (b) a *new* cache filename now exists (the T017 key naturally
      changed) rather than the old entry being reused. Ctest name:
      **`TextureTile_CacheStaleDetection`** (analysis finding G2) (depends
      on T018).
- [ ] T021 [US2] Author the new multi-**process** concurrent
      cache-write test (research.md §10 — genuinely new test
      infrastructure, since spec 019's own concurrency test used threads
      within one process, not multiple OS processes): a shell script
      that deletes any existing cache entry for a fixture source, then
      launches N (e.g. 8) `orender` processes concurrently (each pinned
      `-t:1` to isolate this test to cache-write concurrency, not
      intra-process shading-thread concurrency), all rendering the same
      scene with the cache enabled; after all complete, assert (a)
      every process's own rendered output is correct, and (b) the single
      resulting cache file opens successfully as a valid baked texture
      (`tiffinfo`/`TIFFOpen`-based check) — a corrupt/partial write would
      fail this directly. Ctest name: **`TextureTile_CacheConcurrentWrite`**
      (analysis finding G2) (depends on T018).
- [ ] T022 [US2] Register `TextureTile_CacheDefaultDirectory`
      (T018b)/`TextureTile_CacheByteIdentical`
      (T019)/`TextureTile_CacheStaleDetection`
      (T020)/`TextureTile_CacheConcurrentWrite` (T021) as ctest entries
      (`texture_tile` label) in `tests/unit/texture_tile/CMakeLists.txt`
      (depends on T018b, T019, T020, T021).
- [ ] T023 [US2] Run `ctest --test-dir build -L texture_tile
      --output-on-failure` in full; confirm 100% passing (depends on
      T022).

**Checkpoint**: The opt-in disk cache works, is proven byte-identical to
live synthesis, correctly detects and rebuilds a stale entry, and is
proven safe against concurrent multi-process writers.

---

## Phase 5: User Story 3 - Concurrent renders don't corrupt or crash on the new path (Priority: P3)

**Goal**: Prove `CSynthesizedTileSource`'s own concurrent `fetchTile()`
handling (new code this spec introduces, distinct from the existing,
unchanged per-`CTextureBlock` lock model) is safe under this renderer's
normal multi-threaded operation.

**Independent Test**: Render a scene heavily reusing a single unbaked
texture reference under the renderer's normal (multi-threaded) operation,
repeated several times; confirm no crash, no data corruption, and
consistent rendered output across repeated runs.

### Implementation for User Story 3

- [ ] T024 [US3] Author a new concurrency scene + script targeting
      `CSynthesizedTileSource` specifically (research.md §11, mirroring
      spec 019's `concurrency-scene.rib`/`test_tile_source_concurrency.sh`
      pattern exactly, since `CShadingContext` cannot be hand-constructed
      in a standalone unit test — confirmed during spec 019's own
      planning): a large, finely-diced, periodically-wrapped polygon
      referencing an unbaked source **directly** (disk cache disabled,
      so `CSynthesizedTileSource`'s own `fetchTile()` is what's under
      test, not `CTiffTileSource`'s) (depends on T014).
- [ ] T025 [US3] Render T024's scene once single-threaded (`-t:1`);
      check in the result as the reference (depends on T024).
- [ ] T026 [US3] Render T024's scene several times with orender's
      default (multi-threaded) thread count; measure the actual
      block-average diff against the T025 reference empirically (not
      guessed, per spec 019's own established methodology) and fix the
      test's threshold accordingly (depends on T025).
- [ ] T027 [US3] Register as ctest **`TileSource_SynthesizedConcurrency`**
      (analysis finding G2) (`texture_tile` label) in
      `tests/unit/texture_tile/CMakeLists.txt`; run 5 times back to
      back; confirm reliable passing with no intermittent failures —
      proves SC-005 (depends on T026).

**Checkpoint**: All 3 user stories complete and independently verified.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Documentation and a final full-suite validation pass.

- [ ] T028 [P] Update `DEVNOTES.md` with a new status row for spec 020,
      following the existing convention (see specs 018/019's own rows
      for style).
- [ ] T029 Diff review (`git diff <pre-spec-020 commit>..HEAD --
      src/ri/texture/texture.cpp`) confirming spec 019's existing
      functions/classes (`textureLoadBlock()`, `CTiffTileSource`,
      `CTiledTexture<T>::lookupPixel()`, `CBasicTexture<T>::lookupPixel()`)
      show zero behavioral changes — this spec only *adds*
      (`CSynthesizedTileSource`, the new fallback branch), never
      modifies, any of spec 019's existing code bodies within this one
      file (FR-002's "MUST NOT alter" boundary, verified by diff, not
      just asserted) (depends on T027).
- [ ] T029b Diff review confirming FR-008's boundary held, not just by
      omission but by explicit check (analysis finding C3 — this spec's
      own T006/T015/T016 touch files beyond `texture.cpp`, which T029
      alone never checked): `git diff <pre-spec-020 commit>..HEAD --
      src/ri/texture/texture.cpp src/ri/render/rendererContext.cpp`,
      confirming `CRenderer::environmentLoad()` and every `CEnvironment`
      subclass (`CCubicEnvironment`/`CSphericalEnvironment`/
      `CCylindericalEnvironment`/`CShadow`) show zero changes, and that
      `RiMakeTextureV`'s existing call sites into `makeTexture()`
      (`rendererContext.cpp` ~lines 5876, 5886) show zero changes either
      — only the new `Option "texturecache"` dispatch branch (T016) is
      expected to differ in `rendererContext.cpp` (depends on T029).
- [ ] T029c Diff review confirming the rest of FR-010's boundary held
      (analysis finding C3): `git diff <pre-spec-020 commit>..HEAD --
      src/ri/texture/texmake.cpp src/ri/texture/texmake.h`, confirming
      `appendLayer()`/`appendPyramid()`/`makeTexture()`'s own bodies show
      zero logic changes — the only expected diff is T006's mechanical
      relocation of `adjustSize<T>`/`filterScaleImage<T>` (definitions
      moved from `.cpp` to `.h`, `#include` adjustments), not a change to
      either function's behavior or to `otexmake`'s CLI (depends on
      T029b).
- [ ] T030 Run the full test suite once more end-to-end: `ctest
      --test-dir build -L visual --output-on-failure`, `-L texture_tile`,
      and `-L image_input` (unaffected, but part of the standard gate);
      confirm all green (depends on T029c).
- [ ] T031 Execute every step in `quickstart.md` manually and confirm
      observed behavior matches what it documents (depends on T030).

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — can start immediately.
- **Foundational (Phase 2)**: Depends on Setup — BLOCKS all user
  stories.
- **User Story 1 (Phase 3)**: Depends on Foundational. Nothing else
  depends on it structurally, but it is the MVP and the only story that
  can start immediately after Foundational.
- **User Story 2 (Phase 4)**: Depends on User Story 1 (needs
  `CSynthesizedTileSource`/the fallback branch to exist — its own tests
  compare against live synthesis, and its cache-miss path falls through
  to it).
- **User Story 3 (Phase 5)**: Depends on User Story 1 (needs
  `CSynthesizedTileSource` to exist to test its concurrency) — does not
  depend on User Story 2, and could run before or in parallel with it if
  staffed separately.
- **Polish (Phase 6)**: Depends on all 3 user stories being complete.

### Within Each Phase

- Tasks without `[P]` and listed sequentially have a real dependency on
  the immediately preceding task (stated explicitly in each task's own
  `(depends on ...)` note) — respect that order.
- `[P]`-marked tasks touch different files with no dependency on each
  other and may run in parallel.

### Parallel Opportunities

- T002, T002b, and T002c can all run in parallel with each other —
  different files (two fixture directories, two CMakeLists.txt), no
  dependency between them; T002/T002b are prerequisites for T003, and
  T002c is a prerequisite for anything that actually *renders* against
  them (T010 onward).
- T004b (confirming existing negative-test fixtures suffice) can run in
  parallel with T004/T005 — different files, no shared dependency.
- T028 (DEVNOTES.md) can run in parallel with T029-T031 (different
  file).
- User Story 3 (Phase 5) may be worked in parallel with User Story 2
  (Phase 4) once User Story 1 (Phase 3) is complete, if staffed
  separately — both depend only on US1, not on each other.

---

## Parallel Example: Once User Story 1 is complete

```bash
# User Story 2 and User Story 3 both only depend on US1 -- if staffed
# separately, both can start immediately:
Task: "US2: Add COptions texture-cache fields + Option \"texturecache\" dispatch"
Task: "US3: Author the CSynthesizedTileSource concurrency scene + script"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup.
2. Complete Phase 2: Foundational (new scenes exist, confirmed Red).
3. Complete Phase 3: User Story 1.
4. **STOP and VALIDATE**: the 4 new scene pairs render correctly
   (including the non-power-of-two one); baked-vs-unbaked visual
   equivalence holds; a genuinely undecodable file still degrades
   gracefully; spec 019's byte-identical harness still passes unchanged.
5. This alone is a complete, shippable capability — artists can
   reference PNG/EXR/RGBE textures directly with no bake step.

### Incremental Delivery

1. Setup + Foundational → new coverage exists, confirmed failing.
2. User Story 1 → MVP: unbaked-source textures work.
3. User Story 2 → disk-cache reuse, proven byte-identical and
   crash-safe under concurrent writers.
4. User Story 3 → in-process multi-threaded safety proven for the new
   backend specifically.
5. Each story adds value without touching what the previous one already
   proved.

---

## Notes

- `[P]` tasks = different files, no dependencies.
- `[Story]` label maps task to specific user story for traceability.
- Commit after each task or logical group — the user's own action, never
  mine.
- Stop at any checkpoint to validate a story independently before moving
  on.
