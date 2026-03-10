# Tasks: Shader Extension Update (.sdr to .rslo)

**Input**: Design documents from `/specs/003-update-shader-extension/`
**Prerequisites**: plan.md, spec.md, research.md, data-model.md, contracts/

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies)
- **[Story]**: Which user story this task belongs to (e.g., US1, US2, US3)
- Include exact file paths in descriptions

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Project initialization and basic structure

- [x] T001 [P] Create test directory for new shader assets in `tests/shaders/`
- [x] T002 [P] Create a set of sample `.sl` shaders for compilation testing in `tests/shaders/test_materials.sl`

---

## Phase 2: Foundational (Blocking Prerequisites)

**Purpose**: Core infrastructure updates required for all stories

- [x] T003 [P] Update renderer networking to recognize `.rslo` in `src/ri/rendererJobs.cpp`
- [x] T004 [P] Update build system to include `.rslo` files in `CMakeLists.txt`
- [x] T005 [P] Update installation manifest in `INSTALL_ARTIFACTS.md` to include `.rslo`

---

## Phase 3: User Story 1 - Shader Compilation (Priority: P1) 🎯 MVP

**Goal**: Update compiler to produce `.rslo` by default and support legacy override.

**Independent Test**: Run `oshader tests/shaders/test_materials.sl` and verify `test_materials.rslo` exists.

### Tests for User Story 1

- [x] T006 [P] [US1] Create compilation verification script in `tests/test_compiler_extension.sh`

### Implementation for User Story 1

- [x] T007 [US1] Add `--legacy-sdr` flag parsing to `src/oshader/oshader.cpp`
- [x] T008 [US1] Update default output filename generation to use `.rslo` in `src/oshader/oshader.cpp`
- [x] T009 [US1] Update default output filename generation in Bison parser `src/oshader/sl.y`
- [x] T010 [US1] Implement flag propagation from `oshader.cpp` to `CScriptContext` for legacy support in `src/oshader/oshader.cpp`

---

## Phase 4: User Story 2 - Renderer Shader Loading (Priority: P1)

**Goal**: Update renderer to prioritize `.rslo` files and handle explicit extensions.

**Independent Test**: Render `tests/RIB/test_rslo.rib` referencing a `.rslo` shader.

### Tests for User Story 2

- [x] T011 [P] [US2] Create integration test RIB file in `tests/RIB/test_rslo.rib`

### Implementation for User Story 2

- [x] T012 [US2] Update `sdrGet` to strip explicit `.sdr` extensions from input in `src/sdr/sdr.y`
- [x] T013 [US2] Update `sdrGet` to attempt loading `.rslo` first in `src/sdr/sdr.y`

---

## Phase 5: User Story 3 - Backward Compatibility (Priority: P2)

**Goal**: Fallback to `.sdr` shaders when `.rslo` is missing and log informational message.

**Independent Test**: Render a scene with only `.sdr` available and verify "Falling back to .sdr" log in stdout.

### Tests for User Story 3

- [x] T014 [P] [US3] Create integration test RIB file for legacy fallback in `tests/RIB/test_legacy_fallback.rib`

### Implementation for User Story 3

- [x] T015 [US3] Implement search loop in `sdrGet` to try `.sdr` if `.rslo` fails in `src/sdr/sdr.y`
- [x] T016 [US3] Add informational logging for fallback event using "[INFO] sdr: Falling back to .sdr shader..." prefix in `src/sdr/sdr.y`

---

## Phase 6: User Story 4 - Consistent Error Handling (Priority: P3)

**Goal**: Ensure standard error reporting when neither version is found.

**Independent Test**: Attempt to render a non-existent shader and verify standard error message.

### Tests for User Story 4

- [x] T017 [P] [US4] Create integration test RIB file for missing shader in `tests/RIB/test_missing_shader.rib`

### Implementation for User Story 4

- [x] T018 [US4] Validate error state and cleanup in `sdrGet` after exhaustive search in `src/sdr/sdr.y`

---

## Phase 7: Polish & Cross-Cutting Concerns

**Purpose**: Documentation and utility tool updates.

- [x] T019 [P] Update `sdrinfo` to use dual-search protocol for extensionless names in `src/sdrinfo/sdrinfo.cpp`
- [x] T020 [P] Update user documentation in `docs/site/content/manual/reference/installing-and-running.md`
- [x] T021 [P] Update News and ChangeLog with extension change in `NEWS.md` and `ChangeLog.md`
- [x] T022 Run full integration suite using `quickstart.md` scenarios
- [x] T023 [P] Perform performance benchmark comparison between .rslo and legacy .sdr loading to verify <5% regression (SC-003)

---

## Dependencies & Execution Order

### Phase Dependencies

- **Setup (Phase 1)**: Can start immediately.
- **Foundational (Phase 2)**: Depends on T001, T002.
- **User Story 1 (Phase 3)**: Depends on Phase 2.
- **User Story 2 (Phase 4)**: Depends on Phase 2.
- **User Story 3 (Phase 5)**: Depends on US2 (T012, T013).
- **User Story 4 (Phase 6)**: Depends on US3 (T015).
- **Polish (Phase 7)**: Depends on all user stories.

### Parallel Opportunities

- Setup tasks (T001, T002)
- Foundational tasks (T003, T004, T005)
- All test tasks (T006, T011, T014, T017) can be prepared in parallel with implementation
- Documentation and site updates (T020, T021)

---

## Parallel Example: User Story 1

```bash
# Prepare tests and infrastructure updates
Task: "Create compilation verification script in tests/test_compiler_extension.sh"
Task: "Update build system to include .rslo files in CMakeLists.txt"
```

---

## Implementation Strategy

### MVP First (User Story 1 & 2)

1. Complete Setup and Foundational.
2. Implement US1: Compiler generates `.rslo`.
3. Implement US2: Renderer loads `.rslo`.
4. **Validate**: New shaders compile and render correctly.

### Incremental Delivery

1. Add US3: Enable legacy `.sdr` support via fallback.
2. Add US4: Refine error handling.
3. Polish: Update `sdrinfo` and documentation.
