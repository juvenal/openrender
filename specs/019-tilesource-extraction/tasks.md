# Tasks: Runtime Texture Tile-Fetch Abstraction (CTileSource)

**Input**: Design documents from `/specs/019-tilesource-extraction/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Included and REQUIRED — constitution Principle III (TDD,
NON-NEGOTIABLE) mandates both regression bars below exist and are proven
against the pre-refactor code before any `CTileSource` implementation code
is written (plan.md's Constitution Check, Post-Design section).

**Organization**: Unlike spec 018 (three independently-parallel format
stories), spec 019's three user stories are **sequential**, not parallel:
US1 (P1) *is* the refactor itself; US2 (P2) and US3 (P3) each verify a
property of the code US1 produces, and cannot start meaningfully before
US1 lands. Foundational carries both regression bars (byte-identical
render harness, concurrency test) and their pre-refactor baselines, per
the Constitution Check's explicit TDD requirement.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: US1 (refactor + zero-behavior-change), US2 (concurrency),
  US3 (structural isolation)

## Path Conventions

Single C++ monorepo project. All paths below are real, verified paths on
this branch (`019-tilesource-extraction`, based on current `master`) — see
plan.md's Project Structure section. New fixtures/scenes this spec adds
(none existed for environment/shadow maps before this spec — confirmed by
repo-wide grep during task planning) follow spec 018's existing split:
baked `.tex`/source `.tif` fixtures go directly under
`examples/rib/tests/` (the `TEXTURES` search-path root — see T006), while
the RIB scenes that reference them go one level down, in
`examples/rib/tests/parity/`; new unit tests follow the
`tests/unit/image_input/`-style layout, under `tests/unit/texture_tile/`.

---

## Phase 1: Setup

**Purpose**: Confirm the pre-refactor baseline is clean, and create the new
module's file/test scaffolding so later phases have somewhere to add code.

- [ ] T001 Confirm a clean baseline before any spec 019 code is written:
      `cmake --build build --config Release`, then
      `ctest --test-dir build -L visual --output-on-failure` and
      `ctest --test-dir build -L image_input --output-on-failure`; both
      must be 100% green.
- [ ] T002 [P] Create `src/ri/texture/tileSource.h` as an empty file with
      the project's standard file header (see any existing file in
      `src/ri/texture/` for the license/author header format) — no content
      yet, just the file existing so T014 can populate it.
- [ ] T003 [P] Create `tests/unit/texture_tile/CMakeLists.txt`, modeled
      directly on `tests/unit/image_input/CMakeLists.txt`'s per-executable
      pattern (`add_executable` → `target_compile_features(...
      cxx_std_20)` → `target_include_directories(...)` including
      `${CMAKE_SOURCE_DIR}/src/ri/texture` and the same sibling `src/ri/*`
      directories image_input's tests already include → `add_test` →
      `set_tests_properties(... LABELS "texture_tile;unit")`), initially
      empty of test executables.
- [ ] T004 [P] Add `add_subdirectory(unit/texture_tile)` to
      `tests/CMakeLists.txt`, alongside the existing
      `add_subdirectory(unit/image_input)` line.

**Checkpoint**: Empty scaffolding compiles (`cmake --build build`); no
behavior yet.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Both regression bars this spec is graded against — the
stricter-than-usual byte-identical rendered-output check (FR-002/SC-001)
and the new multi-threaded concurrency test (FR-006/SC-002) — MUST be
authored and proven against the **pre-refactor** code before any
`CTileSource`/`CTiffTileSource` implementation exists (plan.md's
Constitution Check). This also covers a real, pre-existing gap found
during task planning: no example scene in the repo exercises an
environment map or shadow map at all (only plain baked textures do), so
this phase authors the two missing ones. **No user story phase may begin
until this phase's checkpoint passes.**

### New fixtures/scenes (environment map + shadow map — real coverage gap, not previously testable)

- [ ] T005 Generate the shadow-map source fixture: author a small RIB
      scene, `examples/rib/tests/shadow-caster-source.rib` (one simple
      primitive, e.g. a sphere, camera positioned as the casting light's
      view), rendered with **`-t:1`** (determinism — see T009) and
      `Display "shadow-caster-source.tif" "file" "z"` — the `"z"` AOV
      makes `src/display/file/file_tiff.cpp`'s `isDepth` branch write
      `TIFFTAG_PIXAR_MATRIX_WORLDTOCAMERA`/`WORLDTOSCREEN` automatically,
      which `otexmake -shadow` requires of its input (confirmed by reading
      `texmake.cpp`'s shadow-bake code path, which reads those two tags
      from the input file). Check the rendered `.tif` in as a fixture
      alongside the RIB, directly under `examples/rib/tests/` (not the
      `parity/` subdirectory — see T006's note on why).
- [ ] T006 [P] Bake the T005 depth image into a checked-in shadow-map
      texture: `otexmake -shadow shadow-caster-source.tif
      examples/rib/tests/shadow-source.tex`. **Must land directly under
      `examples/rib/tests/`, not `examples/rib/tests/parity/`**: the
      renderer resolves `"shadowname"`/`"texturename"`/`"texname"` values
      against the `TEXTURES` env var
      (`tests/visual/CMakeLists.txt`'s `VISUAL_ENV`, set to
      `${CMAKE_SOURCE_DIR}/examples/rib/tests` — non-recursive), which is
      exactly where spec 018's own baked fixtures
      (`texture-{png,exr,rgbe}-source.tex`) already live, confirmed by
      `git ls-files`, even though the RIB scenes that reference them sit
      one level down in `parity/`. Verify no libtiff warnings on bake
      (sanity check against the class of issue GitHub #18 fixed —
      single-channel output was never affected by that gap, but confirm
      directly rather than assume) (depends on T005).
- [ ] T007 [P] Bake spec 018's existing checked-in `medium_rgb.tif`
      fixture (8-bit RGB, already unaffected by issue #18 per its
      `PHOTOMETRIC_RGB` tag — confirmed via `tiffinfo`) into a checked-in
      environment map: `otexmake -envlatl
      tests/unit/image_input/fixtures/medium_rgb.tif
      examples/rib/tests/env-source.tex` (same `examples/rib/tests/`
      placement rule as T006 — not `parity/`). No new source image
      needed.
- [ ] T008 Author 4 new regression RIB scenes referencing the T006/T007
      fixtures, modeled on spec 018's
      `examples/rib/tests/parity/texture-png-{reyes,raytrace}.rib`:
      `examples/rib/tests/parity/shadow-{reyes,raytrace}.rib` (a simple lit
      primitive using the `shadowspot` shader's `"shadowname"` parameter
      set to `["shadow-source.tex"]`) and
      `examples/rib/tests/parity/env-{reyes,raytrace}.rib` (a simple
      primitive using the `mirror` shader's `"texname"` parameter set to
      `["env-source.tex"]`).
      **Smoke-check before trusting either scene as a regression bar**:
      `shadowspot.sl`'s `shadowname` defaults to `""` and `mirror.sl`'s
      `texname` defaults to the sentinel `"raytrace"` — neither shader's
      baked-map code path (`shadow()`/`environment()`) is exercised by any
      existing scene in this repo today (confirmed by repo-wide grep during
      task planning), which is exactly the shape of the silent-no-op
      gotchas already logged in this repo's `CLAUDE.md` (#10/#11/#12 —
      compiles, links, runs, silently does nothing). Render each new scene
      twice — once with the real `"shadowname"`/`"texname"` parameter, once
      with it removed (or pointed at a different, visibly-distinct baked
      map) — and confirm the rendered image actually changes. If it
      doesn't, `shadow()`/`environment()` are broken on this branch
      independently of this spec; file a GitHub issue per this project's
      established pattern and scope spec.md's acceptance scenarios 2/3
      down honestly rather than keeping a trivially-passing test. Only
      once this is confirmed live: register both pairs via
      `add_parity_test` in `tests/visual/CMakeLists.txt`, following the
      existing `texture-png`/`texture-exr` entries' pattern, and only if
      the reyes/raytrace pair actually agrees within the usual parity
      threshold when first run (don't commit a foregone-failing test) —
      this closes the pre-existing environment/shadow-map coverage gap
      permanently, not just for this spec's byte-identical check (depends
      on T006, T007).

### Byte-identical rendered-output regression bar (FR-002/SC-001)

- [ ] T009 Capture the pre-refactor reference renders for **all 10** scenes
      SC-001 covers ("100% of existing visual-regression scenes that
      reference a baked texture, environment map, or shadow map"), not a
      representative sample — confirmed via `speckit.analyze` (finding
      C1) that a 3-scene subset would leave 7 of the 10 (both hiders'
      EXR/RGBE bakes, and every `-raytrace` variant) checked only by
      T021's coarser 20/255 threshold, not the strict bar SC-001
      explicitly demands. The 10:
      `examples/rib/tests/parity/texture-{png,exr,rgbe}-{reyes,raytrace}.rib`
      (6, reused from spec 018 — a non-TIFF-sourced bake is, at render
      time, indistinguishable from a TIFF-sourced one, since `otexmake`'s
      output container is always the same tiled TIFF regardless of source
      format) and the new `shadow-{reyes,raytrace}.rib`/
      `env-{reyes,raytrace}.rib` from T008 (4). **Must render every one
      with `-t:1`** (single-threaded): empirically confirmed during task
      planning that this codebase's default multi-threaded reyes rendering
      is NOT byte-identical across repeated runs of the *same* unmodified
      binary on the *same* scene (bucket-scheduling nondeterminism —
      measured directly: two back-to-back renders of
      `texture-png-reyes.rib` differed in ~90% of bytes and even in file
      size). `-t:1` eliminates this — confirmed identical across repeated
      runs — and is already this project's own established mechanism for
      exactly this situation (`tests/visual/CMakeLists.txt`'s existing
      `"-t:1"` extra-arg usage, ~lines 881-903, for other scenes that must
      stay byte-identical). With the current (pre-`CTileSource`) `orender`
      binary, render all 10 scenes with `-t:1`; check the 10 resulting
      `.tif` outputs into `tests/unit/texture_tile/fixtures/references/`
      (mirrors spec 018's `test_tiff_bake_byte_identical.sh`
      checked-in-reference approach — a permanent CI-runnable regression
      test, not a one-time manual check). If `HAVE_OPENEXR` is off, the
      2 `texture-exr-*` scenes are skipped, matching how
      `tests/visual/CMakeLists.txt` already guards the EXR parity test
      (depends on T001, T008).
- [ ] T010 [P] Write `tests/unit/texture_tile/test_render_byte_identical.sh`:
      for each of the 10 scenes from T009, render it with
      `$<TARGET_FILE:orender> -t:1` (passed as an argument, following
      `test_tiff_bake_byte_identical.sh`'s existing pattern of taking the
      tool-under-test's path as `$1` — `-t:1` is non-negotiable here, see
      T009) and `cmp` the output byte-for-byte against the matching
      checked-in reference from T009; fail loudly (naming which scene) on
      any mismatch, and skip the 2 EXR scenes when `HAVE_OPENEXR` is off
      (mirroring T009's guard). Register via `add_test`/
      `set_tests_properties(... LABELS "texture_tile;unit")` in
      `tests/unit/texture_tile/CMakeLists.txt`. Confirm it currently
      passes for all applicable scenes (the pre-refactor binary rendering
      the same scenes it just captured references from) (depends on T003,
      T009).

### Multi-threaded concurrency test (FR-006/SC-002 — new coverage, no equivalent exists today)

- [ ] T011 [P] Generate a synthetic multi-tile, multi-mip-level texture
      fixture for the concurrency test: a 512x512 source image with a
      known, closed-form per-pixel formula (reusing spec 018's
      fixture-design approach; document the formula in a new
      `tests/unit/texture_tile/fixtures/FIXTURES.md`), baked via
      `otexmake` into a checked-in reference `.tex` — `otexmake`'s default
      32x32 tile size against 512x512 gives a 16x16-tile base level plus
      several smaller mip levels, giving the concurrency test genuinely
      distinct tiles/levels to fault in concurrently.
- [ ] T012 [P] Write the multi-threaded concurrency test,
      `tests/unit/texture_tile/test_tile_source_concurrency.cpp`:
      `RiBegin(RI_NULL)`/`RiEnd()`-bracketed (per the pattern established in
      spec 018's `test_image_input_tiff.cpp` for any standalone test
      reaching code that can call `error()`), load the T011 fixture via
      `CRenderer::textureLoad()` (`src/ri/render/renderer.h:285`, returns a
      `CTexture*`) with a minimal `TSearchpath` (`src/ri/state/options.h`)
      pointed at the fixture directory (mirrors
      `rendererFiles.cpp`'s existing `textureLoad()` call-site usage), then
      spin up several `std::thread`s repeatedly calling
      `CTexture::lookup(dest, u, v, context)` — covering both of spec.md's
      acceptance scenarios: (a) different threads requesting different,
      previously-uncached tiles concurrently, and (b) different threads
      requesting the *same* previously-uncached tile concurrently.
      Assert every fetch's returned pixel data matches the known formula
      from T011; run the whole thing in a loop of several iterations to
      surface intermittent races rather than trusting one interleaving.
      Register as ctest `TileSource_Concurrency`, label
      `texture_tile;unit`, in `tests/unit/texture_tile/CMakeLists.txt`
      (depends on T003, T011).
- [ ] T013 Run T012 against the current, pre-refactor code, 5 times back
      to back (`for i in 1 2 3 4 5; do ctest --test-dir build -R
      TileSource_Concurrency --output-on-failure || break; done`); confirm
      it passes reliably every time — this is the required pre-refactor
      baseline for FR-006/SC-002 (depends on T012).

### `CTileSource` interface (no behavior yet — just the contract)

- [ ] T014 Define `CTileLevelInfo` and the abstract `CTileSource` base
      class in `src/ri/texture/tileSource.h`, exactly per
      `contracts/tile-source-interface.md` (depends on T002).

**Checkpoint**: Both regression bars exist, are checked in, and pass
against the pre-refactor code. The new environment/shadow-map example
scenes exist and are registered as permanent parity-test coverage
regardless of this spec's own success. `CTileSource`'s abstract interface
compiles. User Story 1's actual refactor can now begin.

---

## Phase 3: User Story 1 - Existing renders are completely unaffected (Priority: P1) 🎯 MVP

**Goal**: `textureLoadBlock()`'s TIFF-specific logic moves behind
`CTiffTileSource`, reached through the new `CTileSource` interface, with
zero change to rendered output.

**Independent Test**: Render the 10 scenes from Phase 2 (plain textures
across all 3 bake source formats, environment map, shadow map, both
hiders) before and after this phase; compare output bytes directly (T010's
harness).

### Implementation for User Story 1

**Correction found during task planning — to the original T020, not to
plan.md**: plan.md's framing ("the lock stays exactly where it is, ABOVE
the new `CTileSource` seam") is accurate and is exactly what the task
sequence below preserves. What needed fixing was this file's own original
T020, which planned to delete `textureLoadBlock()` outright. Directly
reading both call sites (`CBasicTexture<T>::lookupPixel()` ~line 640,
`CTiledTexture<T>::lookupPixel()`'s tile-access macro ~line 767) shows
neither caller holds any lock itself: each does an *unlocked*, per-thread
fast-path check (`threadData[thread].data == NULL`) and, only on a miss,
calls the one shared `textureLoadBlock()` — which is where `entry->mutex`
is actually acquired/released (lines 232-236/243-248/412-416), alongside
the cache-hit early-return and `entry->data`/`refCount` bookkeeping (lines
238-249, 404-411). Both callers go through this identically; there is no
divergence to reconcile. `CTileSource::fetchTile()` therefore replaces
only the actual I/O portion (`TIFFOpen` through `TIFFClose`, ~lines
266-399); the lock/cache-check/bookkeeping wrapper stays in the same
shared function, unmodified in structure — deleting `textureLoadBlock()`
outright would have silently dropped the per-block lock and reopened
exactly the race FR-006/SC-002 exists to catch, while inlining the lock
into both callers instead (considered and explicitly declined — see
below) would duplicate it and remove each caller's current lock-free fast
path, a real behavior change outside this spec's zero-behavior-change bar.
Tasks below are ordered to reflect this: keep one shared function, don't
touch the callers' fast-path structure.

**Confirmed with the user**: keep the lock in the one shared function
(this section's design) rather than duplicating it into both callers.
Filed [GitHub #19](https://github.com/juvenal/openrender/issues/19) for a
separate, pre-existing, out-of-scope question found while confirming this
(whether `block->data`/`dataBlock.data`'s unlocked read in the `access`
macros can race `textureMemFlush()`'s locked eviction) — not something
this spec touches or fixes.

- [ ] T015 [US1] Implement `CTiffTileSource` in `src/ri/texture/texture.cpp`
      (file-local, per research.md §3 — not a new file, since nothing
      outside `texture.cpp` needs to reference it in this spec): extract
      *only* the I/O portion of `textureLoadBlock()`'s current body (the
      `TIFFOpen`/`TIFFSetDirectory`/`TIFFIsTiled`/`TIFFGetFieldDefaulted`/
      `TIFFReadTile`/`TIFFReadScanline`/`TIFFClose` sequence, ~lines
      266-399) into `CTiffTileSource::fetchTile()`; populate `info()` from
      the same `TIFFGetFieldDefaulted` geometry queries. Preserve the
      currently-dead partial-sub-region-read branch (~lines 307-358) and
      the `PLANARCONFIG_SEPARATE` branches (~lines 344-353, 374-383)
      unchanged — per FR-005 and `contracts/tile-source-interface.md` rule
      5, these are NOT reachable by any current caller but MUST NOT be
      pruned. Do NOT touch the surrounding lock/cache-check/bookkeeping
      code (lines 230-249, 404-417) in this task — that stays in
      `textureLoadBlock()` itself, addressed in T015b (depends on T014).
- [ ] T015b [US1] Generalize `textureLoadBlock()`'s own signature and body
      to delegate to a `CTileSource*` instead of doing TIFF I/O inline,
      while leaving its lock acquisition/release (lines 232-236, 243-248,
      412-416), cache-hit early return (238-249), and `entry->data`/
      `refCount` bookkeeping (404-411) exactly where they are: replace the
      `char *name, int x, int y, int w, int h, int dir` parameters with
      `CTileSource *source, int tileX, int tileY` (contract rule 2 means
      there is no sub-region case to parametrize anymore — every call is a
      full tile/whole-image fetch) and replace the removed I/O body (now
      living in `CTiffTileSource::fetchTile()`, T015) with a single call
      `source->fetchTile(tileX, tileY, data)`. Remove the now-dead
      `#ifndef TEXTURE_PERBLOCK_LOCK` global-mutex branches (already dead
      in practice — `TEXTURE_PERBLOCK_LOCK` is unconditionally defined in
      `src/ri/core/ri_config.h`) as part of this same edit, since they're
      textually interleaved with the lock calls this task is touching
      anyway — leave the live `#else`/`entry->mutex` branch as plain code
      (depends on T015).
- [ ] T016 [US1] Update `CTextureLayer` (the base class `CTiledTexture<T>`/
      `CBasicTexture<T>` derive from, ~lines 503-516) to own a
      `CTileSource*` instead of a `strdup`'d filename string + TIFF
      directory index; update its constructor/destructor accordingly —
      same one-owner, freed-in-destructor discipline it already applies to
      the filename today, just redirected to the new member (depends on
      T015b).
- [ ] T017 [US1] Update `CBasicTexture<T>::lookupPixel()` (~line 646) to
      call the now-generalized `textureLoadBlock(entry, layer->tileSource,
      0, 0, context)` (always the whole image, per contract rule 1)
      instead of passing a filename/directory (depends on T016).
- [ ] T018 [US1] Update `CTiledTexture<T>::lookupPixel()`'s tile-access
      macro (~line 773) to call the now-generalized
      `textureLoadBlock(entry, layer->tileSource, tileX, tileY, context)`
      instead of passing a filename/directory (depends on T016).
- [ ] T019 [US1] Update `readMadeTexture()` (~lines 2006-2034) and
      `readTexture()` (~line 2045) to construct one `CTiffTileSource` per
      layer (from the same filename + TIFF directory index they already
      compute) and pass it to the `CTiledTexture`/`CBasicTexture`
      constructor in place of the raw filename+directory pair. Their own
      direct TIFF metadata queries for level geometry (`fileWidth`/
      `fileHeight`/`tileWidth`/`tileHeight`/`numSamples`/`bitsPerSample`)
      are unchanged — out of scope per research.md §1/§4 (depends on T016,
      T017, T018).
- [ ] T020 [US1] Write a small direct unit test asserting
      `CTiffTileSource::info()` reports the correct `CTileLevelInfo` for a
      known fixture (width/height/tileWidth/tileHeight/numChannels/
      bitsPerSample/isFloatFormat) — `info()` has no caller anywhere in
      this spec's own code (`readMadeTexture()`/`readTexture()` keep their
      existing direct geometry queries, per research.md §1/§4/§2's
      rationale), so without a direct test it would ship as unverified,
      uncalled interface surface. Add to a new small test file in
      `tests/unit/texture_tile/` (depends on T015).
- [ ] T021 [US1] Build; run `ctest --test-dir build -L visual
      --output-on-failure` in full; confirm 100% passing — the project's
      usual (coarser) visual-diff gate, as a first-pass check before the
      stricter byte-identical check below (depends on T019, T020).
- [ ] T022 [US1] Run the byte-identical harness from T010
      (`ctest --test-dir build -R <its registered name>
      --output-on-failure`) against the now-refactored build; confirm an
      exact `cmp` match for all 10 scenes from T009 (plain textures across
      all 3 bake source formats, environment map, shadow map, both hiders)
      — the actual proof of FR-002/SC-001's full "100% of existing
      scenes" scope, not a representative subset (depends on T021).

**Checkpoint**: `CTileSource`/`CTiffTileSource` fully replace
`textureLoadBlock()`; rendered output is provably byte-identical for plain
textures, environment maps, and shadow maps. MVP delivered.

---

## Phase 4: User Story 2 - Concurrent texture access is verified safe (Priority: P2)

**Goal**: Prove the extraction in Phase 3 didn't change the existing
per-`CTextureBlock`-lock concurrency model.

**Independent Test**: Re-run T012's concurrency test against the
refactored code, repeatedly; it must pass exactly as reliably as it did
pre-refactor (T013).

- [ ] T023 [US2] Re-run the concurrency test (T012) against the
      now-refactored code, 5 times back to back, exactly as T013 did
      pre-refactor; confirm it still passes reliably with no intermittent
      failures — proves FR-006/SC-002 held through the extraction (depends
      on T022).

**Checkpoint**: Concurrent tile-fetch correctness is proven both before
and after the refactor.

---

## Phase 5: User Story 3 - The fetch mechanism is isolated behind one swappable seam (Priority: P3)

**Goal**: Confirm no TIFF-specific code remains outside `CTiffTileSource`
in the render-time lookup path, and that the dead branches FR-005 requires
were genuinely preserved, not silently dropped during extraction.

**Independent Test**: Inspect the tile-lookup code path directly (T024);
review `CTiffTileSource`'s source against the pre-refactor
`textureLoadBlock()` (T025); confirm the two-level locking/caching pattern
survived structurally (T025b); confirm the cache/eviction machinery and
bake-time write path are diff-clean (T025c).

- [ ] T024 [US3] Run quickstart.md's TIFF-reference sanity check:
      `grep -n "TIFFOpen\|TIFFReadTile\|TIFFReadScanline\|TIFFClose"
      src/ri/texture/texture.cpp`; manually confirm every remaining match
      is inside `CTiffTileSource` alone, or the untouched, separate
      `readMadeTexture()`/`environmentLoad()` metadata queries this spec
      doesn't move — none in `CTiledTexture<T>::lookupPixel()`,
      `CBasicTexture<T>::lookupPixel()`, or anywhere else in the lookup
      path (SC-003) (depends on T023).
- [ ] T025 [US3] Code-review confirmation (not automatically testable, per
      `contracts/tile-source-interface.md`'s Conformance section): diff
      `CTiffTileSource::fetchTile()` against the pre-refactor
      `textureLoadBlock()` (available via `git show` on the commit before
      T015) and confirm the partial-sub-region-read and
      `PLANARCONFIG_SEPARATE` branches carried over verbatim, not
      re-derived or simplified (FR-005) (depends on T015).
- [ ] T025b [US3] Code-review confirmation that the *two-level*
      locking/caching pattern survived structurally unchanged, naming both
      levels explicitly (coarser checks like "the lock is preserved" would
      miss a subtle inversion): (1) each caller's unlocked, per-thread fast
      path (`threadData[thread].data == NULL` check in
      `CBasicTexture<T>::lookupPixel()` and `CTiledTexture<T>::lookupPixel()`'s
      tile-access macro) is untouched — no lock added, no behavior change
      to the fast path; (2) the generalized `textureLoadBlock()` (T015b)
      still acquires `entry->mutex` before checking `entry->data`, and
      still releases it in exactly the same two places (cache-hit early
      return, end of fault-in) as before this refactor. T023's concurrency
      test exercises this at runtime; this task confirms it by reading the
      diff (depends on T015b, T023).
- [ ] T025c [US3] Confirm FR-003 and FR-007's "MUST NOT change" scope
      held, not just by omission but by explicit check (`speckit.analyze`
      finding C2 — until now these had no dedicated verification task).
      Two separate checks, since the two files involved are not equally
      affected: (1) `git diff <pre-T015 commit>..HEAD --
      src/ri/texture/texmake.cpp` is empty — the bake-time write path is
      untouched (FR-007). (2) `texture.cpp` itself is *not* expected to be
      diff-clean (that's where `textureLoadBlock()`/`CTextureLayer`/the two
      `lookupPixel()` sites/`readMadeTexture()`/`readTexture()` all
      change) — within that same file's diff, confirm the `CDeepShadow`
      class and its `ropen()`-based construction branch in
      `environmentLoad()` show zero changes (FR-007), and that
      `CTextureBlock`'s definition, `textureMemFlush()`, and
      `textureAllocateBlock()` also show zero changes (FR-003) — only
      `textureLoadBlock()`'s body (generalized per T015b) and the classes/
      functions T016-T019 name are expected to differ (depends on T019).

**Checkpoint**: All 3 user stories complete and independently verified.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Documentation and a final full-suite validation pass.

- [ ] T026 [P] Update `DEVNOTES.md` with a new status row for spec 019
      (runtime tile-fetch abstraction), following the existing convention
      (see spec 018's row, added by that spec's own T032, for style) —
      note the new environment/shadow-map parity coverage (T008) as a
      side benefit, since it closes a pre-existing gap independent of this
      spec's own success criteria.
- [ ] T027 Run the full test suite once more end-to-end:
      `ctest --test-dir build -L visual --output-on-failure`,
      `ctest --test-dir build -L texture_tile --output-on-failure`, and
      `ctest --test-dir build -L image_input --output-on-failure`
      (unaffected, but part of the standard gate); confirm all green.
- [ ] T028 Execute every step in `quickstart.md` manually and confirm
      observed behavior matches what it documents.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately.
- **Foundational (Phase 2)**: Depends on Setup. **BLOCKS all user
  stories** — both regression bars (byte-identical render, concurrency)
  and their pre-refactor baselines must exist and pass before any
  `CTileSource`/`CTiffTileSource` code is written, per constitution III.
- **User Story 1 (Phase 3)**: Depends on Foundational only. This is the
  refactor itself.
- **User Story 2 (Phase 4)**: Depends on **User Story 1 being complete**
  (T022) — it re-runs the same concurrency test against the refactored
  code, so there is nothing to re-verify until the refactor lands. Unlike
  spec 018's independently-parallel stories, US2 cannot start before US1
  finishes.
- **User Story 3 (Phase 5)**: Depends on **User Story 1 being complete**
  (T023) for the same reason — there is no "fully isolated seam" to
  inspect until the refactor lands. Independent of US2 (US2 and US3 could
  run in parallel with each other once US1 is done, but both need US1
  first).
- **Polish (Phase 6)**: Depends on all three user stories being complete.

### Within Each Phase

- Foundational's fixture/scene-authoring tasks (T005-T008) must complete
  before baseline capture (T009, T011) and test authoring (T010, T012),
  which must in turn complete and pass (T013) before T014 (the interface)
  — and all of Foundational must complete before Phase 3 begins, per
  constitution III's "tests exist and pass before implementation" rule
  applied to a pure-refactor spec (there is no red/green here since
  nothing is expected to newly start failing; both tests establish a
  baseline that must hold, unchanged, after the refactor).
- Within Phase 3: T015 (CTiffTileSource I/O extraction) before T015b
  (generalize `textureLoadBlock()` to delegate to a `CTileSource*`,
  keeping its lock/cache-check/bookkeeping in place) before T016
  (CTextureLayer ownership) before T017/T018 (call-site migration) before
  T019 (construction-site migration) — a linear chain, not parallelizable,
  since each step depends on the previous one compiling. T020 (the
  `info()` unit test) only depends on T015 and can run in parallel with
  T015b-T019.

### Parallel Opportunities

- T002, T003, T004 (Setup) can run in parallel.
- T006, T007 (Foundational: baking the two new fixtures) can run in
  parallel once T005 lands (T007 doesn't even depend on T005 — it reuses
  an existing fixture — but is grouped here for readability).
- T010 (byte-identical harness) and T011+T012 (concurrency fixture + test)
  are independent of each other — both can proceed in parallel once T003/
  T004 (Setup) and T009/T008 respectively are ready.
- T015 through T019 (Phase 3) form one linear chain — not parallelizable.
  T020 (the `info()` unit test) depends only on T015 and can run alongside
  T015b-T019.
- T024, T025, T025b, and T025c (Phase 5) touch different artifacts (a
  grep pass, a diff review of the I/O body, a diff review of the locking
  structure, a diff check of the untouched files) and can all run in
  parallel.

---

## Parallel Example: Foundational phase

```bash
# Once T005 lands, bake both new fixtures in parallel:
Task: "T006 Bake shadow-caster-source.tif into shadow-source.tex via otexmake -shadow"
Task: "T007 Bake medium_rgb.tif into env-source.tex via otexmake -envlatl"

# The two regression bars are independent of each other:
Task: "T010 Write test_render_byte_identical.sh"
Task: "T011 + T012 Generate concurrency fixture and write test_tile_source_concurrency.cpp"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational (CRITICAL — both regression bars must
   exist and pass against pre-refactor code first)
3. Complete Phase 3: User Story 1 — the refactor, proven byte-identical
4. **STOP and VALIDATE**: `ctest -L visual`, the byte-identical harness,
   plus quickstart.md's manual `cmp` recipe
5. `CTileSource` exists, `CTiffTileSource` is its only backend, and
   rendered output is unchanged — spec 020 (a separate, later effort) has
   a real seam to build on

### Incremental Delivery

1. Setup + Foundational → both regression bars proven against pre-refactor
   code (no new capability yet, but the safety net exists)
2. + User Story 1 → the refactor lands, byte-identical proven (MVP)
3. + User Story 2 → concurrency safety proven to still hold
4. + User Story 3 → structural isolation confirmed by inspection
5. + Polish → docs, full-suite validation

### Sequential-Only Note

Unlike spec 018 (three genuinely parallel format stories), this spec's
stories are **strictly sequential**: US2 and US3 both verify properties of
the code US1 produces and have no independent implementation content of
their own — there is no parallel-team strategy to offer here beyond
splitting Foundational's two independent regression-bar tracks (T009+T010
vs. T011+T012) across two people.

---

## Notes

- [P] tasks = different files, no dependencies on incomplete same-phase
  work.
- [US1]/[US2]/[US3] labels map tasks to spec.md's prioritized user
  stories for traceability.
- The environment-map and shadow-map example scenes (T005-T008) are new,
  permanent test coverage this spec adds as a side effect of needing them
  for its own regression bar — not scope creep, since spec.md's own
  acceptance scenarios 2/3 (User Story 1) explicitly require them, and
  research.md's assumption that such scenes already existed turned out to
  be false once checked directly against the repo.
- `otexmake`/the bake-time write path (`appendLayer`/`appendPyramid` in
  `texmake.cpp`) is never touched by any task in this list — only baked
  *once*, ahead of time, to produce the new fixtures.
- Commit after each task or logical group, per this project's existing
  workflow (the user reviews and commits — Claude does not commit
  autonomously, per this feature's established working agreement).
