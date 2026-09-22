# Tasks: Multi-Format Texture Source Decoding for otexmake

**Input**: Design documents from `/specs/018-multi-format-texture-decode/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/, quickstart.md

**Tests**: Included and REQUIRED — constitution Principle III (TDD, NON-NEGOTIABLE)
mandates tests written and failing before implementation for every task
below; this is not the generic "tests optional" default.

**Organization**: Tasks are grouped by user story (P1 PNG, P2 OpenEXR,
P3 RGBE) per spec.md, after a Foundational phase that builds the shared
`CImageInput` abstraction and migrates the existing TIFF path onto it
(required before any new format can be added, and carries the
byte-identical regression gate every later phase depends on).

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: US1 (PNG), US2 (OpenEXR), US3 (RGBE)

## Path Conventions

Single C++ monorepo project. All paths below are real, verified paths on
this branch (`master`@`733bbfb`) — see plan.md's Project Structure section.

---

## Phase 1: Setup

**Purpose**: Create the new module's file/test scaffolding so later phases
have somewhere to add code.

- [ ] T001 Create `src/ri/texture/imageInput.h` and
      `src/ri/texture/imageInput.cpp` as empty files with the project's
      standard file header (see any existing file in `src/ri/texture/` for
      the license/author header format) — no content yet, just the files
      existing so T006 can populate them.
- [ ] T002 [P] Create `tests/unit/image_input/CMakeLists.txt`, modeled
      directly on `tests/unit/csg/CMakeLists.txt`'s per-executable pattern
      (`add_executable` → `target_compile_features(... cxx_std_20)` →
      `target_include_directories(...)` including
      `${CMAKE_SOURCE_DIR}/src/ri/texture` and the same sibling
      `src/ri/*` directories csg's tests already include → `add_test` →
      `set_tests_properties(... LABELS "image_input;unit")`), initially
      empty of test executables (later tasks add them).
- [ ] T003 [P] Add `add_subdirectory(unit/image_input)` to
      `tests/CMakeLists.txt`, alongside the existing
      `add_subdirectory(unit/csg)` / `add_subdirectory(unit/blobby)` lines.
- [ ] T004 [P] Create `tests/unit/image_input/fixtures/` and generate tiny
      (e.g. 4×4 or 8×8 pixel) synthetic test source files checked into it:
      a small TIFF, an 8-bit RGB PNG, an 8-bit RGBA PNG, a 16-bit PNG, a
      grayscale PNG, a grayscale+alpha PNG, an indexed/palette PNG (for the
      negative test), a single-part RGB OpenEXR, a single-part RGBA
      OpenEXR, a single-part luminance OpenEXR, a multi-part OpenEXR (for
      the negative test), an OpenEXR with an unsupported channel layout
      (for the negative test), and a small RGBE `.hdr` file. Each fixture's
      exact pixel values must be known/recorded so decoder tests can assert
      on them, not just "decoded without crashing."

**Checkpoint**: Empty scaffolding compiles (`cmake --build build`); no
behavior yet.

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: The shared `CImageInput` abstraction, the TIFF decoder that
retires the current `readLayer()` special case, and the byte-identical
regression gate every later phase depends on. **No user story phase may
begin until this phase's checkpoint passes.**

### Tests for Foundational phase (write first, confirm failing)

- [ ] T005 [P] Write a failing unit test asserting the `CImageInput`/
      `CImageInfo` contract shape compiles and a `CTiffImageInput` decodes
      the TIFF fixture from T004 to the exact expected pixel values, in
      `tests/unit/image_input/test_image_input_tiff.cpp` (register in
      `tests/unit/image_input/CMakeLists.txt` per T002's pattern).
- [ ] T006 [P] Write a failing regression script/test that bakes an
      existing TIFF-sourced test texture with `otexmake` before and after
      this feature and `cmp`s the two outputs, in
      `tests/unit/image_input/test_tiff_bake_byte_identical.cpp` (or a
      shell-driven `add_test` invoking `otexmake` twice and running `cmp`
      if that is simpler than a C++ harness — either is acceptable, but it
      MUST fail today since `CTiffImageInput` does not exist yet to
      produce a "before" run through the new path). Register with label
      `"image_input;unit"`.

### Implementation for Foundational phase

- [ ] T007 Define `CImageInfo` and the abstract `CImageInput` base class in
      `src/ri/texture/imageInput.h`, exactly per
      `contracts/image-input-interface.md` (depends on T001).
- [ ] T008 [P] Implement `CTiffImageInput` in
      `src/ri/texture/imageInputTiff.h` and `.cpp`, wrapping the existing
      `readLayer()` logic (`src/ri/texture/texmake.cpp:214-247`) behind the
      `open()`/`readImage()`/`close()` contract, with no behavior change
      (depends on T007).
- [ ] T009 Implement `createImageInput(filename)` in
      `src/ri/texture/imageInput.cpp` with the suffix-dispatch table from
      `data-model.md`'s Format Registration Table — registering `.tif`/
      `.tiff` and the no-match fallback to `CTiffImageInput` only for now
      (later phases add their own suffix registrations) (depends on T007,
      T008).
- [ ] T010 Migrate all 5 `readLayer()` call sites in
      `src/ri/texture/texmake.cpp` (currently at lines 776, 853, 941, 996,
      1054, each preceded by a direct `TIFFOpen()` at 758, 817, 925, 977,
      1036) to use `createImageInput()`/`CImageInput` instead of calling
      `TIFFOpen`/`readLayer()` directly. `appendLayer`/`appendPyramid`
      (lines 68-205) are NOT touched — only how source pixels are obtained
      changes (depends on T009).
- [ ] T011 Add `imageInput.cpp`, `imageInputTiff.cpp` to the `src/ri`
      CMake target source lists in `src/ri/CMakeLists.txt` (the same lists
      that already include `texture/texmake.cpp`, at the locations noted
      in plan.md's Project Structure section) (depends on T007, T008).
- [ ] T012 Run T005 and T006; confirm both now pass. Run
      `ctest --test-dir build -L visual --output-on-failure` in full;
      confirm zero regressions against the pre-existing suite.

**Checkpoint**: TIFF sources bake byte-identically through the new
abstraction. `CImageInput` is proven end-to-end for one format. User
stories US1/US2/US3 can now proceed, each independently.

---

## Phase 3: User Story 1 - Bake a texture from a PNG source (Priority: P1) 🎯 MVP

**Goal**: `otexmake` can bake a texture, environment map, or shadow map
from a PNG source file, preserving alpha and 16-bit precision, and
rejecting indexed/palette PNGs with a clear error.

**Independent Test**: Run `otexmake` against the PNG fixtures from T004
(RGB, RGBA, 16-bit, grayscale, grayscale+alpha, indexed) and confirm
correct output for the supported layouts and a clear rejection error for
indexed — independently of OpenEXR/RGBE work.

### Tests for User Story 1 (write first, confirm failing)

- [ ] T013 [P] [US1] Write failing unit tests for `CPngImageInput` in
      `tests/unit/image_input/test_image_input_png.cpp`: round-trip
      decode against the RGB/RGBA/16-bit/grayscale/grayscale+alpha PNG
      fixtures from T004 asserting exact pixel values and correct
      `CImageInfo` (`numChannels`, `bitsPerSample`), plus a negative test
      that the indexed PNG fixture causes `open()` to return `false` with
      an error rather than decoding. Register in
      `tests/unit/image_input/CMakeLists.txt`.
- [ ] T014 [P] [US1] Add a new visual-regression scene pair,
      `examples/rib/tests/texture-png-reyes.rib` and
      `examples/rib/tests/texture-png-raytrace.rib`, each referencing a
      texture baked from a PNG source, and register them via
      `add_parity_test(texture-png examples/rib/tests/texture-png-reyes.rib
      texture-png-reyes.tif
      examples/rib/tests/texture-png-raytrace.rib
      texture-png-raytrace.tif)` in `tests/visual/CMakeLists.txt`,
      following the existing `add_parity_test` call pattern already used
      there for other scenes (e.g. the `flatshade`/`dof` entries).

### Implementation for User Story 1

- [ ] T015 [US1] Implement `CPngImageInput` in
      `src/ri/texture/imageInputPng.h` and `.cpp` per `research.md` §4:
      row-based `libpng` API matching `src/display/file/file_png.cpp`'s
      existing usage style, `PNG_COLOR_TYPE_RGB`/`RGB_ALPHA`/`GRAY`/
      `GRAY_ALPHA` accepted, `PNG_COLOR_TYPE_PALETTE` rejected before any
      pixel read, 16-bit samples byte-swapped to native order via
      `png_set_swap()` (depends on T007).
- [ ] T016 [US1] Register `.png` → `CPngImageInput` in
      `createImageInput()` in `src/ri/texture/imageInput.cpp` (depends on
      T009, T015).
- [ ] T017 [US1] Add `imageInputPng.cpp` to the `src/ri` CMake target
      source lists in `src/ri/CMakeLists.txt` (same locations as T011)
      (depends on T015).
- [ ] T018 [US1] Run T013 and T014; confirm both pass. Run
      `ctest --test-dir build -L visual --output-on-failure`; confirm zero
      regressions.

**Checkpoint**: PNG sources bake and render correctly, independently
testable/deployable as the MVP slice.

---

## Phase 4: User Story 2 - Bake a texture from an OpenEXR source (Priority: P2)

**Goal**: `otexmake` can bake from single-part OpenEXR sources with
RGB/RGBA/luminance channels at full float precision, unconditionally
rejecting any multi-part file and any unsupported channel layout with a
clear error.

**Independent Test**: Run `otexmake` against the OpenEXR fixtures from
T004 (RGB, RGBA, luminance, multi-part, unsupported-channel-set) on a
`HAVE_OPENEXR` build and confirm correct output/rejection for each,
independently of PNG/RGBE work.

### Tests for User Story 2 (write first, confirm failing)

- [ ] T019 [P] [US2] Write failing unit tests for `COpenExrImageInput` in
      `tests/unit/image_input/test_image_input_exr.cpp`, guarded by
      `#ifdef HAVE_OPENEXR`: round-trip decode against the RGB/RGBA/
      luminance OpenEXR fixtures from T004 asserting exact float pixel
      values (including a value outside 0-1 to prove no clamping) and a
      `HALF`-channel fixture asserting promotion to `bitsPerSample == 32`,
      plus negative tests that the multi-part fixture and the
      unsupported-channel-set fixture both cause `open()` to return
      `false` with a clear error. Register in
      `tests/unit/image_input/CMakeLists.txt`, conditionally compiled only
      when `HAVE_OPENEXR` (mirroring how `src/display/openexr/` is
      conditionally built today).
- [ ] T020 [P] [US2] Add a new visual-regression scene pair,
      `examples/rib/tests/texture-exr-reyes.rib` and
      `examples/rib/tests/texture-exr-raytrace.rib`, referencing a texture
      baked from an OpenEXR source, registered via `add_parity_test` in
      `tests/visual/CMakeLists.txt` following T014's pattern, guarded so it
      is only added when the build has `HAVE_OPENEXR`.

### Implementation for User Story 2

- [ ] T021 [US2] Implement `COpenExrImageInput` in
      `src/ri/texture/imageInputExr.h` and `.cpp`, guarded by
      `#ifdef HAVE_OPENEXR`, per `research.md` §3: open via
      `Imf::MultiPartInputFile` first and reject unconditionally if
      `parts() > 1`; otherwise validate the single part's `ChannelList` is
      exactly `{R,G,B}`, `{R,G,B,A}`, or one channel (rejecting anything
      else with a clear error); read pixel data through the classic `Imf`
      API; promote any `HALF`-typed channel to 32-bit float on decode
      (depends on T007).
- [ ] T022 [US2] Register `.exr` → `COpenExrImageInput` in
      `createImageInput()` in `src/ri/texture/imageInput.cpp`, guarded by
      `#ifdef HAVE_OPENEXR`; when not defined, a `.exr` source file must
      cause a clear "OpenEXR support not built into this binary" error
      rather than falling through to the TIFF decoder (depends on T009,
      T021).
- [ ] T023 [US2] Wire `HAVE_OPENEXR` and the existing
      `OPENRENDER_OPENEXR_LIBS` variable (root `CMakeLists.txt:383-412`)
      into `src/ri/CMakeLists.txt`: add `imageInputExr.cpp` to the
      relevant target source lists only when `HAVE_OPENEXR` is `ON`, and
      link `${OPENRENDER_OPENEXR_LIBS}` into those same targets, per
      `research.md` §2 (depends on T021).
- [ ] T024 [US2] Run T019 and T020 on a `HAVE_OPENEXR` build; confirm both
      pass. Confirm a build with `HAVE_OPENEXR` off still compiles cleanly
      and produces the expected graceful-degradation error for a `.exr`
      source. Run `ctest --test-dir build -L visual --output-on-failure`;
      confirm zero regressions.

**Checkpoint**: OpenEXR sources bake and render correctly at full float
precision; unsupported layouts are cleanly rejected; US1 and US2 both work
independently.

---

## Phase 5: User Story 3 - Bake a texture from an RGBE / Radiance HDR source (Priority: P3)

**Goal**: `otexmake` can bake from RGBE (`.hdr`/`.pic`) sources, reusing the
existing (currently dead) `RGBE_ReadHeader`/`RGBE_ReadPixels` codec.

**Independent Test**: Run `otexmake` against the RGBE fixture from T004 and
confirm the baked texture's decoded values match the source, independently
of PNG/OpenEXR work.

### Tests for User Story 3 (write first, confirm failing)

- [ ] T025 [P] [US3] Write a failing unit test for `CRgbeImageInput` in
      `tests/unit/image_input/test_image_input_rgbe.cpp`: round-trip
      decode against the RGBE fixture from T004 asserting exact expected
      float RGB values (proving `RGBE_ReadHeader`/`RGBE_ReadPixels` are
      actually invoked, not stubbed). Register in
      `tests/unit/image_input/CMakeLists.txt`.
- [ ] T026 [P] [US3] Add a new visual-regression scene pair,
      `examples/rib/tests/texture-rgbe-reyes.rib` and
      `examples/rib/tests/texture-rgbe-raytrace.rib`, referencing a texture
      baked from an RGBE source, registered via `add_parity_test` in
      `tests/visual/CMakeLists.txt` following T014's pattern.

### Implementation for User Story 3

- [ ] T027 [US3] Implement `CRgbeImageInput` in
      `src/ri/texture/imageInputRgbe.h` and `.cpp`, calling the existing
      `RGBE_ReadHeader()`/`RGBE_ReadPixels()` (`display/rgbe/rgbe.h`,
      already includable per `src/ri/CMakeLists.txt:43`) via a standard
      `fopen()`-obtained `FILE*`; always reports `numChannels = 3`,
      `bitsPerSample = 32`, `isFloatFormat = true` (depends on T007).
- [ ] T028 [US3] Register `.hdr`/`.pic` → `CRgbeImageInput` in
      `createImageInput()` in `src/ri/texture/imageInput.cpp` (depends on
      T009, T027).
- [ ] T029 [US3] Add `imageInputRgbe.cpp` **and** `display/rgbe/rgbe.cpp`
      to the `src/ri` CMake target source lists in `src/ri/CMakeLists.txt`
      (same locations as T011/T017) — `rgbe.cpp` is currently compiled
      only into the `rgbe.dsply` `MODULE`, which these targets do not link
      against, so it must be added directly per `research.md` §5's
      addendum (depends on T027).
- [ ] T030 [US3] Run T025 and T026; confirm both pass. Run
      `ctest --test-dir build -L visual --output-on-failure`; confirm zero
      regressions.

**Checkpoint**: All three new source formats (PNG, OpenEXR, RGBE) bake and
render correctly, each independently proven; TIFF remains byte-identical.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Coverage and validation that spans all formats, plus closing
out FR-008/SC-004 (clear errors for unsupported/corrupted input) with
explicit negative-case tests not already covered per-format above.

- [ ] T031 [P] Write unit tests for cross-format negative cases in
      `tests/unit/image_input/test_image_input_errors.cpp`:
      `createImageInput()` returns `nullptr` for an unsupported extension
      (e.g. `.bmp`), and each decoder's `open()` returns `false` (not a
      crash) for a file with a supported extension but corrupted/invalid
      content for that format (e.g. a truncated PNG, a `.exr`-named file
      that isn't a valid EXR). Register in
      `tests/unit/image_input/CMakeLists.txt`.
- [ ] T032 [P] Update `DEVNOTES.md` to note `otexmake` now accepts PNG/
      OpenEXR/RGBE bake sources in addition to TIFF, per this repo's
      convention of tracking feature status there (see CLAUDE.md's Dev
      workflow section).
- [ ] T033 Run the full test suite once more end-to-end:
      `ctest --test-dir build -L visual --output-on-failure`,
      `ctest --test-dir build -L image_input --output-on-failure`, and
      `ctest --test-dir build -L libshader --output-on-failure`; confirm
      all green.
- [ ] T034 Execute every step in `quickstart.md` manually (build,
      byte-identical `cmp` check, bake+render each new format, all five
      negative-case sanity checks) and confirm the observed behavior
      matches what quickstart.md documents.

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately.
- **Foundational (Phase 2)**: Depends on Setup. **BLOCKS all user
  stories** — the `CImageInput` contract, `CTiffImageInput`, and the
  byte-identical regression gate must exist and pass before any new
  decoder is built against the same interface.
- **User Stories (Phases 3-5)**: All depend on Foundational completion
  only. US1/US2/US3 do not depend on each other — each adds one new
  concrete decoder plus one new `createImageInput()` registration line,
  touching disjoint new files (only `imageInput.cpp`'s registration
  function and `src/ri/CMakeLists.txt`'s source lists are shared touch
  points across stories — see Parallel Opportunities below).
- **Polish (Phase 6)**: Depends on whichever of US1/US2/US3 are complete
  (T031's cross-format error tests need at least one non-TIFF decoder to
  exist to be meaningful; full coverage needs all three).

### Within Each Phase

- Tests MUST be written and failing before implementation tasks in the
  same phase (constitution III, NON-NEGOTIABLE).
- Interface (T007) before any concrete decoder.
- Concrete decoder implementation before its `createImageInput()`
  registration before its CMake wiring before its checkpoint test run.

### Parallel Opportunities

- T002, T003, T004 (Setup) can run in parallel.
- T005, T006 (Foundational tests) can run in parallel with each other, but
  both must land before T007-T010.
- T008 depends on T007 but is otherwise independent of T009/T010 until
  they need it.
- Once Phase 2's checkpoint passes, **US1, US2, and US3 can be implemented
  in parallel** (e.g. by different developers) — each story's test tasks
  (T013/T014, T019/T020, T025/T026) and implementation tasks touch
  entirely separate new files. The only shared files are
  `src/ri/texture/imageInput.cpp` (each story adds one registration line —
  small, easily sequenced or merged) and `src/ri/CMakeLists.txt` (each
  story adds its own source-list entries — same caveat).
- Within each story, the two test tasks are marked [P] (different files);
  implementation tasks are sequential within that story due to the
  interface→registration→CMake-wiring→verification chain.

---

## Parallel Example: User Story 1

```bash
# Launch both User Story 1 test tasks together:
Task: "Write failing unit tests for CPngImageInput in tests/unit/image_input/test_image_input_png.cpp"
Task: "Add texture-png-reyes.rib / texture-png-raytrace.rib parity scene pair"
```

## Parallel Example: Across User Stories (post-Foundational)

```bash
# Once Phase 2's checkpoint passes, all three stories' first test tasks
# can be launched together, since they touch disjoint files:
Task: "T013 [US1] Write failing unit tests for CPngImageInput"
Task: "T019 [US2] Write failing unit tests for COpenExrImageInput"
Task: "T025 [US3] Write failing unit test for CRgbeImageInput"
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational (CRITICAL — blocks everything; this is
   also where the hardest regression bar, byte-identical TIFF output, is
   proven)
3. Complete Phase 3: User Story 1 (PNG)
4. **STOP and VALIDATE**: `ctest -L visual`, plus quickstart.md's PNG and
   TIFF-regression steps
5. PNG-sourced texture baking is now genuinely usable end-to-end

### Incremental Delivery

1. Setup + Foundational → TIFF path proven byte-identical through the new
   abstraction (no new capability yet, but zero risk introduced)
2. + User Story 1 (PNG) → MVP: artists can bake from PNG sources
3. + User Story 2 (OpenEXR) → HDR/float sources supported
4. + User Story 3 (RGBE) → all three formats from the spec supported
5. + Polish → cross-format error coverage, docs, full-suite validation

### Parallel Team Strategy

After Phase 2's checkpoint, up to three people can each own one of
US1/US2/US3 concurrently — they touch disjoint new files, with only two
small shared-file touch points (`imageInput.cpp`'s registration function,
`src/ri/CMakeLists.txt`'s source lists) to coordinate on merge.

---

## Notes

- [P] tasks = different files, no dependencies on incomplete same-phase
  work.
- [US1]/[US2]/[US3] labels map tasks to spec.md's prioritized user
  stories for traceability.
- Every test task must be run and confirmed **failing** before its
  paired implementation task begins (constitution III).
- `appendLayer`/`appendPyramid` (`texmake.cpp:68-205`) — the pyramid-
  writing/output side — are never touched by any task in this list; only
  how source pixels are *obtained* changes.
- Commit after each task or logical group, per this project's existing
  workflow (the user reviews and commits — Claude does not commit
  autonomously, per this feature's established working agreement).
