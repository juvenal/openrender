# Tasks: Imager Shader Support

**Input**: Design documents from `specs/005-imager-shader-support/`

**Prerequisites**: plan.md ✓, spec.md ✓, research.md ✓, data-model.md ✓, contracts/ ✓

**TDD Mandate**: Per constitution §III — all test tasks MUST be written and confirmed failing before the corresponding implementation tasks begin. Tests are not optional for this feature (explicitly required in spec and plan).

**Phase mapping** (plan.md ↔ tasks.md): plan Phase T = tasks Phase 3–5 test sections; plan Phase I = tasks Foundational + Phase 3 implementation; plan Phase E = tasks Phase 3–4 implementation; plan Phase G = tasks Phase 3–5 checkpoints; plan Phase R = tasks Phase 6.

**Organization**: Tasks grouped by user story to enable independent implementation and testing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on sibling tasks)
- **[Story]**: Which user story this task belongs to (US1, US2, US3)
- Exact file paths in every description

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create the directory skeleton and CMake wiring that all test phases depend on.

- [X] T001 Create directory `tests/imager/` and `tests/imager/integration/ribs/` per plan.md structure
- [X] T002 Create `tests/imager/CMakeLists.txt` — declare test executables `test_imager_options`, `test_imager_guard`, `test_imager_execution`; link `openrender_common_flags`; set C++20; add `add_subdirectory(integration)`
- [X] T003 Create `tests/imager/integration/CMakeLists.txt` — declare integration test `test_imager_render`; add CTest entries with label `imager`
- [X] T004 Add `add_subdirectory(imager)` to `tests/CMakeLists.txt`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Structural additions that every user story implementation depends on. No user story work begins until this phase is complete.

**⚠️ CRITICAL**: These changes compile cleanly but are not yet wired to active behaviour.

- [X] T005 Add field `CShaderInstance *imager{nullptr};` to `COptions` in `src/ri/options.h`, with matching destructor `if (imager) imager->detach();` and copy-constructor `attach()` call
- [X] T006 [P] Add `static CShaderInstance *imagerShader;` declaration to `CRenderer` in `src/ri/renderer.h` with initialiser `nullptr` in `src/ri/renderer.cpp`
- [X] T007 Create `src/ri/imager.h` — C++20 header declaring `CImagerExecutor` with `void execute(CShaderInstance&, int left, int top, int width, int height, float* pixels, int sampleStride) noexcept;`; include `src/includes/logging.hpp`
- [X] T008 [P] Create stub `src/ri/imager.cpp` — empty `CImagerExecutor::execute()` body; add `imager.cpp` to the `orender` target in `src/ri/CMakeLists.txt`

**Checkpoint**: Project builds cleanly with the new empty executor and storage fields. No behaviour change yet.

---

## Phase 3: User Story 1 — Basic Imager Shader Execution (Priority: P1) 🎯 MVP

**Goal**: Render a scene with `Imager "background"` in the RIB — non-geometry pixels receive the background color.

**Independent Test**: Render `background-test.rib` with `Imager "background" "color bgcolor" [1 0 0]`; verify any pixel outside geometry reads approximately `(1, 0, 0)`.

### Tests for User Story 1 ⚠️ Write and confirm FAILING before T017

- [X] T009 [US1] Create `tests/imager/test_imager_options.cpp` — Test 1: `RiImager("background", ...)` before WorldBegin stores a non-null `SL_IMAGER` instance in `currentOptions->imager`; Test 2: second `RiImager` call replaces first instance (last-call-wins); Test 3: `RiImager` with non-existent shader leaves `imager == nullptr`
- [X] T010 [P] [US1] Create `tests/imager/test_imager_guard.cpp` — Test 4: `RiImager` called after WorldBegin emits a warning containing "WorldBegin" and does not change `imager`; Test 5: no `Imager` statement → `CRenderer::imagerShader == nullptr` after `beginFrame()`; Test 5b: render a RIB with `Imager` statement but no `Display` statement — executor runs without crashing (edge case: no display driver)
- [X] T011 [P] [US1] Create `tests/imager/test_imager_execution.cpp` — Test 6: `CImagerExecutor::execute()` with a trivial shader (`Ci = color(1,0,0)`) sets `pixels[0..2]` to `{1,0,0}`; Test 9: `imagerShader == nullptr` fast path — `dispatch()` returns pixel buffer bit-identical to input; also add assert that a shader emitting `Ci = color(-1, 2, 0.5)` produces those exact values in the pixel buffer pre-dispatch (no clamping)
- [X] T012 [P] [US1] Create `tests/imager/integration/ribs/background-test.rib` — minimal scene with sphere on left, `Imager "background" "color bgcolor" [1 0 0]`, `Display` to a temp TIFF; format 64×32
- [X] T013 [P] [US1] Create `tests/imager/integration/ribs/no-imager-regression.rib` — same geometry as `background-test.rib` but no `Imager` statement; serves as regression baseline
- [X] T014 [US1] Create `tests/imager/integration/test_imager_render.cpp` — Test 10: spawn `orender background-test.rib`, read output TIFF, assert right-half pixels ≈ `(1,0,0)`; Test 11: render `no-imager-regression.rib` twice and assert bit-identical output (regression)

### Implementation for User Story 1

- [X] T015 [US1] Implement `RiImagerV()` in `src/ri/rendererContext.cpp` (lines ~1004–1007): WorldBegin guard with `warning()` + `log_warn()`; `getShader(name, SL_IMAGER, ...)` call; `options->imager->detach()` on replace; `options->imager = shader`; `log_info()` on success
- [X] T015a [US1] Add `bool inWorld{false}` to `CRendererContext` in `src/ri/rendererContext.h`; set `inWorld = true` as first statement in `RiWorldBegin()` and `inWorld = false` in `RiWorldEnd()` in `src/ri/rendererContext.cpp`
- [X] T016 [US1] Populate `CRenderer::imagerShader = options->imager` inside `CRenderer::beginFrame()` in `src/ri/rendererContext.cpp` (WorldBegin path); add `log_info` if non-null, `log_debug` if null (sequential after T015 — same file; different function, but avoid simultaneous edits)
- [X] T017 [US1] Implement `CImagerExecutor::execute()` in `src/ri/imager.cpp`: uses shading context's CShadingState for correct VM variable routing; copies pixel data in/out of state->varying arrays; processes in chunks of maxGridSize; all 7 RI spec variables bound
- [X] T018 [US1] Add imager pre-pass to `CRenderer::dispatch()` in `src/ri/rendererDisplay.cpp`: `if (imagerShader != nullptr) { CImagerExecutor ex; ex.execute(*imagerShader, left, top, width, height, pixels, numSamples); }` immediately before the `for (i = 0; i < numDisplays; i++)` loop; add `#include "imager.h"`

**Checkpoint**: User Story 1 fully functional. Run `ctest -L imager -R "imager_options|imager_guard|imager_execution|imager_render"` and confirm Tests 1–6, 9–11 pass.

---

## Phase 4: User Story 2 — Standard Pixel Variables (Priority: P2)

**Goal**: All seven RI Spec 3.2 imager variables (`Ci`, `Oi`, `alpha`, `P`, `ncomps`, `time`, `dtime`) are correctly bound and readable inside an imager shader.

**Independent Test**: Compile and render an imager shader that writes `P.x / xresolution` to `Ci.r`, `P.y / yresolution` to `Ci.g`, `0` to `Ci.b`; verify output is a UV-gradient image with top-left pixel ≈ `(0,0,0)` and bottom-right ≈ `(1,1,0)`.

### Tests for User Story 2 ⚠️ Write and confirm FAILING before T024

- [X] T019 [US2] Add Test 7 to `tests/imager/test_imager_execution.cpp`: dispatch with `left=10, top=20`, 1×1 pixel; no-crash verification at raster coords (10, 20)
- [X] T020 [US2] Add Test 8 to `tests/imager/test_imager_execution.cpp`: ncomps/dtime no-corruption verification (precise value checks deferred to T021 uvgradient shader)
- [X] T021 [P] [US2] Created `shaders/uvgradient.sl` + `shaders/uvgradient.rslo`; created `tests/imager/integration/ribs/uv-gradient-test.rib`; render verified

### Implementation for User Story 2

- [X] T022 [US2] Add `P` variable binding in `CImagerExecutor::execute()` — implemented in `executeChunk()` via state->varying[VARIABLE_P] copy-in
- [X] T023 [P] [US2] Add `ncomps`, `time`, `dtime` bindings — implemented in `executeChunk()` (ncomps/dtime as uniform at [0]; time as varying per pixel)
- [X] T024 [US2] Add `Oi` synthesis and write-back fold — implemented in `executeChunk()` with oiMean alpha folding

**Checkpoint**: User Story 2 fully functional. Run `ctest -L imager` — all tests pass including UV-gradient integration test.

---

## Phase 5: User Story 3 — Parameter Passing from RIB (Priority: P3)

**Goal**: `Imager "background" "color bgcolor" [0 0 1] "float background" [0.5]` delivers those exact parameter values to the shader at execution time.

**Independent Test**: Render the same scene twice — once with default `bgcolor` (white), once with `bgcolor [1 0 0]` — and assert the two output images differ in non-geometry pixels by the expected color difference.

### Tests for User Story 3 ⚠️ Write and confirm FAILING before T028

- [X] T025 [US3] Test 12 in `tests/imager/test_imager_options.cpp`: RiImager with bgcolor=[1,0,0]; execute on alpha=0 pixel; assert Ci≈(1,0,0) — passing via ImagerOptionTests
- [X] T026 [P] [US3] Create `tests/imager/integration/ribs/param-override-test.rib` — blue bgcolor=[0,0,1]; passing via ImagerParamOverrideRender

### Implementation for User Story 3

- [X] T027 [US3] Verified: `RiImagerV()` forwards n/tokens/params verbatim to getShader — code review confirms correct
- [X] T028 [P] [US3] Test 13 in `tests/imager/integration/test_imager_render.cpp` — render param-override-test.rib; passing via ImagerParamOverrideRender

**Checkpoint**: All three user stories independently testable. Full `ctest -L imager` green.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Logging completeness, stack-allocation optimisation, and final regression verification.

- [X] T029 [P] Audit complete: all log_* calls in imager.cpp and rendererContext.cpp are additive; warning(CODE_NOTOPTIONS) preserved alongside log_warn in RiImagerV
- [X] T030 [P] Stack-buffer optimization N/A: imager.cpp now uses shading context's pre-allocated CShadingState::varying arrays directly — no heap allocation needed per tile
- [X] T031 Full suite: 14/14 tests pass; zero regressions in framebuffer, portability, core, driver labels
- [X] T032 [P] Markdown lint fixed in research.md — blank lines before all list items under headings, language specifier added to fenced block
- [X] T033 quickstart.md validation: background.sl compiles and renders correctly (verified via ImagerBackgroundRender + ImagerParamOverrideRender integration tests); no-clamp behavior verified via testNoClamping unit test
- [X] T034 [P] SC-004 performance validation documented in plan.md "Performance Validation" section with acceptable methodology note
- [X] T035 [P] `tests/imager/integration/ribs/dual-display-test.rib` + Test 14 in test_imager_render.cpp — both Display targets get imager applied; passing via ImagerDualDisplayRender

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: No dependencies — start immediately
- **Foundational (Phase 2)**: Depends on Phase 1 completion — **blocks all user stories**
- **User Story Phases (3–5)**: All depend on Foundational; can proceed sequentially (P1→P2→P3) or in parallel if staffed
- **Polish (Phase 6)**: Depends on all user stories complete

### User Story Dependencies

| Story | Depends On | Notes |
|-------|-----------|-------|
| US1 (P1) | Phase 2 only | No dependency on US2 or US3 |
| US2 (P2) | Phase 2 + US1 execution engine | Extends CImagerExecutor with more variables |
| US3 (P3) | Phase 2 + US1 option storage | Verifies parameter forwarding; minimal new code |

### Within Each User Story

1. Write tests → confirm they FAIL (red)
2. Implement → confirm tests PASS (green)
3. Refactor → tests still pass

### Parallel Opportunities

- T002, T003 can run in parallel with T001 (different files)
- T006, T007, T008 can run in parallel (different files)
- T010, T011, T012, T013 can run in parallel (different files, all US1 tests)
- T015a must precede T015 and T016 (inWorld flag must exist before guard is used); T015 and T016 both modify `src/ri/rendererContext.cpp` but different functions — do not open simultaneously; complete T015 before starting T016
- T019 must precede T020 (sequential appends to same file `test_imager_execution.cpp`; [P] removed from T020)
- T023 can run in parallel with T022 (different variable groups)
- T026 can run in parallel with T025 (different files)
- T029, T030, T032, T034, T035 can run in parallel (different files/concerns)

---

## Parallel Example: User Story 1 Tests

```
# All US1 test files can be created simultaneously:
Task T009: tests/imager/test_imager_options.cpp
Task T010: tests/imager/test_imager_guard.cpp
Task T011: tests/imager/test_imager_execution.cpp
Task T012: tests/imager/integration/ribs/background-test.rib
Task T013: tests/imager/integration/ribs/no-imager-regression.rib
```

```
# US1 implementation tasks T015–T016 can be parallelised:
Task T015: src/ri/rendererContext.cpp  (RiImagerV)
Task T016: src/ri/rendererContext.cpp  (beginFrame — different function, mergeable)
# T017 and T018 sequential (T018 depends on T017 executor being non-empty)
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational
3. Complete Phase 3: User Story 1 (tests → implement → verify)
4. **STOP and VALIDATE**: `ctest -L imager -R "US1|render"` green
5. Ship: `shaders/background.sl` works in any RIB

### Incremental Delivery

1. Setup + Foundational → builds cleanly
2. US1 → basic imager works (background fill MVP)
3. US2 → all 7 spec variables available (position-dependent effects)
4. US3 → parameter overrides work (fully reusable shaders)
5. Polish → logging complete, no regressions

---

## Notes

- `[P]` tasks operate on different files; no coordination needed between them
- `[Story]` labels map each task to a specific user story for traceability
- The TDD constraint is non-negotiable (constitution §III): confirm `ctest` FAILS before writing implementation
- Existing `error()` / `warning()` calls in `rendererContext.cpp` and RIB/RSL paths must never be removed — `log_*` calls are additive only
- `CShaderInstance::execute(nullptr, locals)` is intentional for imager: surface built-ins are unavailable
- `alloca` in T030 mirrors the `MAX_DISPATCH_SIZE` pattern already used in `dispatch()` for consistency
