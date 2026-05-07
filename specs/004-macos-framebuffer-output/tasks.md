# Tasks: Unified Framebuffer Display Architecture (macOS + Linux)

**Input**: Design documents from `specs/004-macos-framebuffer-output/`  
**Prerequisites**: plan.md ✅, spec.md ✅, research.md ✅, data-model.md ✅, contracts/ ✅

**TDD**: Tests MUST be written and confirmed failing before each implementation task they cover (Constitution III — non-negotiable).

**Organization**: Tasks are grouped by user story to enable independent implementation and testing.

## Format: `[ID] [P?] [Story] Description`

- **[P]**: Can run in parallel (different files, no dependencies on incomplete tasks)
- **[Story]**: Which user story this task belongs to (US1, US2, US3)
- Exact file paths included in every description

---

## Phase 1: Setup (Shared Infrastructure)

**Purpose**: Create directory scaffolding and CMake stubs so all parallel work in later phases has landing targets.

- [x] T001 Create `tests/framebuffer/` directory and empty `tests/framebuffer/CMakeLists.txt` stub
- [x] T002 Create `src/framebuffer/orender-fb-macos/Sources/` directory hierarchy and empty `src/framebuffer/orender-fb-macos/CMakeLists.txt` stub
- [x] T003 [P] Create `src/framebuffer/orender-fb-linux/` directory and empty `src/framebuffer/orender-fb-linux/CMakeLists.txt` stub
- [x] T004 [P] Add `add_subdirectory(tests/framebuffer)` to root `CMakeLists.txt` (guard: `if(BUILD_TESTING)`)

**Checkpoint**: All directories exist; CMake configures without error.

---

## Phase 2: Foundational (Shared Protocol — Blocks All User Stories)

**Purpose**: Implement `fbipc.h` — the shared TLV protocol constants and packet structs. Every driver and both helpers depend on this. No user story work begins until this phase is complete.

**⚠️ CRITICAL**: TDD required — protocol tests written and failing BEFORE implementation.

- [x] T005 Write TLV encode/decode unit tests in `tests/framebuffer/test_ipc_protocol.cpp` — cover: makeSocketPath format, all four opcodes (START/DATA/DONE/QUIT), payload round-trip, zero-length payloads, boundary conditions (max title length, max tile size)
- [x] T006 Wire `tests/framebuffer/test_ipc_protocol.cpp` into `tests/framebuffer/CMakeLists.txt` as a CTest target named `test_ipc_protocol`
- [x] T007 Implement `src/framebuffer/fbipc.h` — `enum class FBOpcode : uint8_t {START=0x01,DATA=0x02,DONE=0x03,QUIT=0x04}`, `#pragma pack(1)` structs for `FBHeader`, `FBStartPayload`, `FBDataPayload`, `makeSocketPath(pid_t)` returning `/tmp/orender-fb-<pid>.sock`, inline `sendPacket(int fd, ...)` helpers — all T005 tests pass
- [x] T007a Write unit tests for `makeHelperPath(const char* argv0, const char* helperName)` in `tests/framebuffer/test_ipc_protocol.cpp` — test: when `ORENDERHOME` env var set, returns `$ORENDERHOME/bin/<helperName>`; when `ORENDERHOME` unset, returns path relative to `argv0`'s directory; fails gracefully when neither resolves — confirm tests FAIL before T007b implementation
- [x] T007b Implement `makeHelperPath(const char* argv0, const char* helperName)` in `src/framebuffer/fbipc.h` — checks `$ORENDERHOME/bin/<helperName>` first, then `dirname(argv0)/<helperName>`, then falls back to `<helperName>` (PATH lookup via posix_spawnp); T007a tests pass (depends on T007)
- [x] T008 [P] Add `fbipc.h` include guard validation test to `tests/framebuffer/test_ipc_protocol.cpp` — confirm struct sizes match protocol spec (e.g., `sizeof(FBHeader) == 5`)

**Checkpoint**: `ctest -R test_ipc_protocol` passes. Protocol structs confirmed wire-compatible. `makeHelperPath()` resolves helper binary path correctly on all platforms.

---

## Phase 3: User Story 1 — macOS Framebuffer Window (Priority: P1) 🎯 MVP

**Goal**: Deliver a fully working macOS framebuffer display window. A user on macOS running orender with a framebuffer `Display` statement sees a HUD-style window appear, update progressively, retitle on render completion, and stay open until they close it. The terminal returns immediately.

**Independent Test**: Build on macOS, run `examples/rib/camera-dof.rib` with a `Display "camera-dof" "framebuffer" "rgb"` statement. Verify: window appears before first tile, tiles update progressively, orender exits, window title changes to "Rendering Complete", window stays open.

### Tests for User Story 1 (TDD — write first, confirm failing)

- [x] T009 Write `tests/framebuffer/test_protocol.swift` — unit tests for `Protocol.swift`: TLV parser for each opcode, malformed packet handling, partial read simulation, title with zero length, title at max length (512 bytes), numSamples 3 and 4
- [x] T010 [P] Write `tests/framebuffer/test_imagestore.swift` — unit tests for `ImageStore.swift`: tile enqueue/dequeue order invariant (FIFO), CGContext pixel values after applying a tile, state transitions (idle→active→complete→interrupted), title update on DONE, title update on disconnect
- [x] T011 [P] Write `tests/framebuffer/test_fbq_driver.cpp` — unit tests for `CQDisplay` using a mock socket listener: constructor spawns helper (mock), START packet construction (field values match START payload spec), `data()` produces correct DATA packet, `finish()` produces DONE then closes socket, socket-closed mid-render returns TRUE from `data()` (render continues), `finish()` after socket closed is a safe no-op

### Implementation for User Story 1

- [x] T012 Implement `src/framebuffer/orender-fb-macos/Sources/Protocol.swift` — `FBOpcode` enum mirroring `fbipc.h` values, `TLVParser` class (streaming read state machine), `StartPayload`/`DataPayload` value types with `init(from data: Data) throws` — T009 tests pass
- [x] T013 [P] Implement `src/framebuffer/orender-fb-macos/Sources/ImageStore.swift` — `@MainActor final class ImageStore: ObservableObject`, `@Published var cgImage: CGImage?`, `@Published var windowTitle: String`, tile FIFO queue (`[DataPayload]`), `applyTile(_:)` updates `CGContext` backing store and triggers redraw, `markDone()` / `markInterrupted()` update `windowTitle`, drain-on-done logic — T010 tests pass
- [x] T014 Implement `src/framebuffer/orender-fb-macos/Sources/SocketServer.swift` — `class SocketServer`: binds Unix socket at `argv[1]` path, `accept()`s exactly one connection, reads TLV stream on `DispatchQueue.global()` background queue, dispatches parsed `StartPayload`/`DataPayload`/`DONE`/`QUIT`/`disconnect` events to `ImageStore` via `DispatchQueue.main.async`, handles EOF as disconnect event (depends on T012, T013)
- [x] T015 [P] Implement `src/framebuffer/orender-fb-macos/Sources/ContentView.swift` — SwiftUI `struct ContentView: View` with `@ObservedObject var store: ImageStore`; body: `if let img = store.cgImage { Image(cgImage: img).resizable().aspectRatio(contentMode: .fit) } else { Color.black }` (checkerboard background shown before first tile)
- [x] T016 Implement `src/framebuffer/orender-fb-macos/Sources/AppDelegate.swift` — `class AppDelegate: NSObject, NSApplicationDelegate, NSWindowDelegate`: `applicationDidFinishLaunching` creates `NSPanel(contentRect:styleMask:[.hudWindow,.closable,.titled] backing:.buffered defer:false)`, sets `panel.isFloatingPanel = false`, `panel.level = .normal`, `panel.contentView = NSHostingView(rootView: ContentView(store: store))`, `panel.title = "orender — Waiting"`, `panel.orderFrontRegardless()`; `windowWillClose(_:)` calls `NSApp.terminate(nil)` (QUIT-send on window close is added in T038); builds File menu with "Save Image..." (`NSSavePanel` for TIFF/PNG, `UTType.tiff`/`.png`) and "Quit orender-fb-macos" (`NSApp.terminate(nil)`); all other default menus removed (depends on T014, T015)
- [x] T017 Implement `src/framebuffer/orender-fb-macos/Sources/main.swift` — validates `CommandLine.arguments.count >= 2` (exits non-zero with stderr message if missing), sets `NSApp.setActivationPolicy(.regular)`, instantiates `ImageStore` and `AppDelegate`, sets `NSApp.delegate = delegate`, calls `NSApp.run()` (depends on T016)
- [x] T018 [P] Create `src/framebuffer/orender-fb-macos/Info.plist` — `CFBundleIdentifier = org.openrender.orender-fb-macos`, `LSUIElement = NO`, `NSPrincipalClass = NSApplication`, `CFBundleExecutable = orender-fb-macos`, `LSMinimumSystemVersion = 12.0`
- [x] T019 Create `src/framebuffer/orender-fb-macos/CMakeLists.txt` — `enable_language(Swift)`, `add_executable(orender-fb-macos MACOSX_BUNDLE Sources/main.swift Sources/AppDelegate.swift Sources/ContentView.swift Sources/ImageStore.swift Sources/SocketServer.swift Sources/Protocol.swift)`, `set_target_properties(... PROPERTIES SWIFT_VERSION "6" MACOSX_DEPLOYMENT_TARGET "12.0" MACOSX_BUNDLE_INFO_PLIST Info.plist)`, `target_link_libraries(orender-fb-macos "-framework AppKit" "-framework SwiftUI" "-framework Foundation")`, `install(TARGETS orender-fb-macos ...)` (depends on T012–T018)
- [x] T020 [P] Implement `src/framebuffer/fbq.h` — `class CQDisplay : public CDisplay`: fields `char socketPath[256]`, `int socketFd`, `pid_t helperPid`, `bool disconnected`; method declarations `CQDisplay(...)`, `~CQDisplay()`, `int data(int,int,int,int,float*)`, `void finish()`
- [x] T021 Implement `src/framebuffer/fbq.cpp` — constructor: `makeSocketPath(getpid())` → `socketPath`, use `makeHelperPath(argv0, "orender-fb-macos")` to resolve helper path, `posix_spawn` the resolved path with `socketPath` as argv[1], retry-connect loop (up to 5 s) to socket, send START packet via `sendPacket()`; `data()`: if `disconnected` return TRUE; `sendPacket(DATA)` — on EPIPE/error set `disconnected=true` and return TRUE (render continues without display); `finish()`: if not `disconnected` send DONE + close socket; emit warning to stderr if helper failed to start — T011 tests pass (depends on T007b, T020)
- [x] T022 Update `src/framebuffer/framebuffer.cpp` — add `#ifdef __APPLE__` / `#include "fbq.h"` and `cWindow = new CQDisplay(...)` branch in `displayStart()` before the `#else` Linux branch
- [x] T023 Update `src/framebuffer/CMakeLists.txt` — add `elseif(APPLE)` block: `add_library(framebuffer MODULE framebuffer.cpp fbq.cpp ...)`, `target_link_libraries(framebuffer openrendercommon)`, `add_subdirectory(orender-fb-macos)` (depends on T019, T021, T022)

**Checkpoint**: macOS build succeeds. `orender camera-dof.rib` shows HUD window, updates progressively, terminal returns on completion, window stays open. T011 tests pass.

---

## Phase 4: User Story 2 — Non-Blocking Linux Framebuffer (Priority: P2)

**Goal**: Migrate Linux X11 and Wayland framebuffer backends to the helper-exe IPC model. All observable behavior is preserved — progressive window updates, window persists after render, terminal returns immediately. Internally the in-process pthread mechanism is replaced by the same TLV socket protocol.

**Independent Test**: On Linux (Wayland and X11), run an existing framebuffer regression RIB. Confirm behavior identical to pre-migration: window appears, tiles update, orender exits, window stays open.

### Tests for User Story 2 (TDD — write first, confirm failing)

- [x] T024 Write `tests/framebuffer/test_fbx_driver.cpp` — unit tests for refactored `CXDisplay` as IPC client: mirrors T011 pattern (mock socket, START payload, DATA forwarding, DONE/close, socket-closed mid-render returns TRUE)
- [x] T025 [P] Write `tests/framebuffer/test_fbwl_driver.cpp` — unit tests for refactored `CWDisplay` as IPC client: same structure as T024
- [x] T025b Write unit tests for `orender-fb-linux` TLV server in `tests/framebuffer/test_linux_helper_server.cpp` — test: START packet → creates display context with correct width/height/numSamples/title; DATA packet → updates pixel buffer at correct coordinates; DONE packet → sets complete state + retitles; QUIT packet → exits cleanly; EOF without QUIT → interrupted state; malformed packet → safe close — confirm tests FAIL before T026 implementation

### Implementation for User Story 2

- [x] T026 Implement `src/framebuffer/orender-fb-linux/main.cpp` skeleton — `int main(int argc, char** argv)`: validates socket path arg, creates Unix socket server (bind/listen/accept), reads TLV packets in loop, dispatches START/DATA/DONE/QUIT/EOF; START → `isWaylandAvailable() ? runWaylandWindow() : runX11Window()`; stub `runX11Window()` and `runWaylandWindow()` that log and return (non-crashing stubs allow driver tests T024/T025 to run against a real helper)
- [x] T027 Port X11 display code into `src/framebuffer/orender-fb-linux/main.cpp` — extract `CXDisplay::main()`, constructor pixel-format setup (15/16/32 bpp detection, checkerboard init), `handleData_*` pixel conversion methods, thread loop, `WM_DELETE_WINDOW` handling, retitle-on-DONE, retitle-on-disconnect as `runX11Window(struct SessionCtx*)` free function; receives tile data via `SessionCtx` populated by TLV reader (depends on T026)
- [x] T028 [P] Port Wayland display code into `src/framebuffer/orender-fb-linux/main.cpp` — extract `CWDisplay::main()`, all wl_* callbacks, libdecor conditional, shm buffer management, keyboard/pointer listeners as `runWaylandWindow(struct SessionCtx*)` free function (depends on T026)
- [x] T029 Refactor `src/framebuffer/fbx.h` — remove `pthread_t thread`, `wakeup_pipe[2]`, all X11 display fields (`Display*`, `GC`, `XImage*`, atoms, `dataHandlerFn`, `imageData`, etc.); add `char socketPath[256]`, `int socketFd`, `pid_t helperPid`, `bool disconnected`; keep `CXDisplay : public CDisplay` inheritance
- [x] T030 Refactor `src/framebuffer/fbx.cpp` — constructor: `makeSocketPath(getpid())`, use `makeHelperPath(argv0, "orender-fb-linux")` to resolve helper path, `posix_spawn` the resolved path with socket path arg, retry-connect, send START; `data()`: if disconnected return TRUE; send DATA — on error set disconnected=true return TRUE; `finish()`: if not disconnected send DONE + close; destructor: clean up socket fd — T024 tests pass (depends on T007b, T026–T029)
- [x] T031 [P] Refactor `src/framebuffer/fbwl.h` — same transformation as T029 for Wayland; remove all `wl_*`, libdecor, pthread fields; add IPC client fields
- [x] T032 [P] Refactor `src/framebuffer/fbwl.cpp` — same transformation as T030 for Wayland — T025 tests pass (depends on T026, T031)
- [x] T033 Create `src/framebuffer/orender-fb-linux/CMakeLists.txt` — `add_executable(orender-fb-linux main.cpp)`, `target_compile_features(... cxx_std_20)`, `target_include_directories(... ${X11_INCLUDE_DIR} ${WAYLAND_INCLUDE_DIRS} ${LIBDECOR_INCLUDE_DIRS})`, `target_link_libraries(... ${X11_LIBRARIES} ${WAYLAND_LIBRARIES} ${LIBDECOR_LIBRARIES} pthread)`; `install(TARGETS orender-fb-linux ...)`
- [x] T034 Update `src/framebuffer/CMakeLists.txt` — in Linux (`UNIX AND NOT APPLE`) block: ensure `fbq.cpp` is excluded from Linux sources (it is Apple-only), `add_subdirectory(orender-fb-linux)`, install helper alongside `framebuffer.so`; remove standalone `fbwl` module target if migration makes it redundant (depends on T030, T032, T033)
- [x] T035 Linux regression validation — run `examples/rib/camera-dof.rib` with framebuffer display on Wayland and X11; confirm: window appears, tiles update in order, orender exits within 1 second of completion, window stays open with correct final title; document results in `tests/framebuffer/test_linux_regression.sh`

**Checkpoint**: Linux regression passes on both Wayland and X11. T024/T025 tests pass. Behavior identical to pre-migration.

---

## Phase 5: User Story 3 — Graceful Handling of Interrupted Renders (Priority: P3)

**Goal**: When the renderer is killed mid-render, the display window (on all platforms) detects the disconnection, retitles to "Interrupted", shows the last rendered state, and stays open until the user closes it. No orphaned processes remain.

**Independent Test**: Start a long render on both macOS and Linux, send `SIGTERM` to the orender process mid-render. Verify: window retitles to "Interrupted", last partial image is visible, window remains interactive, no zombie processes exist after window is closed.

### Tests for User Story 3 (TDD — write first, confirm failing)

- [ ] T035a [US3] Write unit tests for macOS disconnect detection in `tests/framebuffer/test_socketserver_disconnect.swift` — test: `SocketServer` calls `store.markInterrupted()` on EOF without prior QUIT, calls `store.markDone()` on DONE, title set to "Interrupted — <original>" after markInterrupted, `ImageStore` stops accepting tiles after markInterrupted — confirm tests FAIL before T036 implementation
- [ ] T035b [P] [US3] Write unit tests for Linux helper disconnect detection in `tests/framebuffer/test_linux_helper_disconnect.cpp` — test: TLV read loop invokes interrupted-retitle path on EOF without prior QUIT, invokes done-retitle path on DONE packet — confirm tests FAIL before T037 implementation
- [ ] T035c [US3] Write unit tests for macOS window-close QUIT behavior in `tests/framebuffer/test_appdelegate_quit.swift` — test: `AppDelegate.windowWillClose(_:)` triggers QUIT send via `SocketServer`; `CQDisplay::data()` returns TRUE after EPIPE (disconnected flag set) — confirm tests FAIL before T038 implementation
- [ ] T035d [P] [US3] Write unit tests for Linux helper window-close QUIT behavior in `tests/framebuffer/test_linux_helper_quit.cpp` — test: X11 `WM_DELETE_WINDOW` event triggers QUIT packet send + exit 0; Wayland `xdg_toplevel_close` triggers same — confirm tests FAIL before T039 implementation

### Implementation for User Story 3

- [ ] T036 [US3] Implement disconnect detection in `src/framebuffer/orender-fb-macos/Sources/SocketServer.swift` — when `read()` returns 0 or error (and QUIT was not received), call `store.markInterrupted()` on main queue; add `markInterrupted()` to `ImageStore` that drains any pending tile queue then sets `windowTitle = "Interrupted — <original title>"` and stops accepting new tiles
- [ ] T037 [P] [US3] Implement disconnect detection in `src/framebuffer/orender-fb-linux/main.cpp` — when TLV `read()` returns ≤ 0 without a prior QUIT packet, drain pending tile queue then retitle window to "Interrupted — <original title>" and keep event loop running until user closes window (X11: `XNextEvent`; Wayland: `wl_display_dispatch`); on window close: exit 0
- [ ] T038 [US3] Implement window-close behavior in `src/framebuffer/orender-fb-macos/Sources/AppDelegate.swift` — `windowWillClose(_:)`: send QUIT packet to socket server before closing so driver handles it gracefully, then call `NSApp.terminate(nil)`; ensure `CQDisplay::data()` handles QUIT response (driver side: set `disconnected=true`, continue returning TRUE from `data()`)
- [ ] T039 [P] [US3] Implement window-close behavior in `src/framebuffer/orender-fb-linux/main.cpp` — when window is closed by user (X11: `WM_DELETE_WINDOW`; Wayland: `xdg_toplevel_close`): send QUIT TLV packet on socket then exit 0; driver receives QUIT (or EPIPE on next write) and sets `disconnected=true`, continues rendering
- [ ] T040 [US3] Write integration test script `tests/framebuffer/test_interrupt_macos.sh` — starts render of a slow RIB, sends SIGTERM to orender after first tile, waits 2 s, uses `pgrep orender-fb-macos` to confirm no orphan, uses `osascript` to confirm window title contains "Interrupted"
- [ ] T041 [P] [US3] Write integration test script `tests/framebuffer/test_interrupt_linux.sh` — mirrors T040 for Linux using `wmctrl -l` or `xdotool` to check window title

**Checkpoint**: All three platforms handle mid-render interruption cleanly. No zombie processes. Window retitles and stays open.

---

## Phase 6: Polish & Cross-Cutting Concerns

**Purpose**: Debug tooling, warning messages, site documentation, and end-to-end validation.

- [ ] T042 [P] Add `OPENRENDER_FB_DEBUG=1` stderr logging to `src/framebuffer/fbq.cpp`, `src/framebuffer/fbx.cpp`, `src/framebuffer/fbwl.cpp` — log each packet type (opcode name, length, tile coords for DATA) when env var is set; document in `specs/004-macos-framebuffer-output/quickstart.md` (already referenced)
- [ ] T043 [P] Add FR-013 warning message path to `src/framebuffer/fbq.cpp`, `src/framebuffer/fbx.cpp`, `src/framebuffer/fbwl.cpp` — when `posix_spawn` fails or socket connect times out (> 5 s): `fprintf(stderr, "openRender: framebuffer display unavailable — %s\n", reason)` then set `failure = FALSE` so render continues (not `TRUE`, since failure=TRUE aborts the display but we want to continue)
- [ ] T044 [P] Update Hugo site — add `site/content/docs/framebuffer/architecture.md` covering: IPC model overview, protocol summary table, platform matrix, helper binary co-installation note; reference `contracts/ipc-protocol.md`
- [ ] T045 Run `quickstart.md` end-to-end validation — execute all steps in `specs/004-macos-framebuffer-output/quickstart.md` on macOS (camera-dof.rib with framebuffer display); confirm build, render, window, save image (TIFF + PNG), quit; record any deviations and fix

---

## Dependencies & Execution Order

### Phase Dependencies

- **Phase 1 (Setup)**: No dependencies — start immediately
- **Phase 2 (Foundational)**: Depends on Phase 1 — **BLOCKS all user stories**
- **Phase 3 (US1 — macOS)**: Depends on Phase 2 completion
- **Phase 4 (US2 — Linux)**: Depends on Phase 2 completion — can run in parallel with Phase 3
- **Phase 5 (US3 — Interruption)**: Depends on Phase 3 AND Phase 4 (both helpers must exist)
- **Phase 6 (Polish)**: Depends on Phase 3 + Phase 4; Phase 5 can complete in parallel

### User Story Dependencies

- **US1 (P1)**: Starts after Foundational (Phase 2) — no dependency on US2/US3
- **US2 (P2)**: Starts after Foundational (Phase 2) — no dependency on US1/US3
- **US3 (P3)**: Starts after US1 AND US2 (adds behavior to both helpers)

### Within Each User Story

1. Tests written and confirmed FAILING
2. Protocol/data layer (fbipc.h, Protocol.swift, packet structs)
3. Storage layer (ImageStore, pixel buffers)
4. I/O layer (SocketServer, TLV read loop)
5. UI layer (ContentView, window setup)
6. Driver layer (CQDisplay, CXDisplay, CWDisplay IPC client)
7. Build system integration
8. End-to-end validation

### Parallel Opportunities

Within Phase 3 (US1):
```
T009 (Protocol tests) ─┬─ T012 (Protocol.swift impl)
T010 (ImageStore tests) ┼─ T013 (ImageStore.swift impl)
T011 (Driver tests)    ─┘

T015 (ContentView) ──────────────────────────────────┐
T018 (Info.plist)  ──────────────────────────────────┤
T020 (fbq.h)       ──────────────────────────────────┘ → T019, T021, T022, T023
```

Within Phase 4 (US2):
```
T024 (CXDisplay tests) ─┬─ T026 (helper skeleton)
T025 (CWDisplay tests) ─┘        ├─ T027 (X11 window port)
                                  └─ T028 (Wayland port) [P with T027]

T029 (fbx.h refactor) ─┬─ T030 (fbx.cpp refactor)
T031 (fbwl.h refactor) ┘  T032 (fbwl.cpp refactor) [P with T030]
```

---

## Implementation Strategy

### MVP First (User Story 1 Only)

1. Complete Phase 1: Setup
2. Complete Phase 2: Foundational — protocol header + tests
3. Complete Phase 3: macOS driver (`fbq.cpp`) + Swift helper (`orender-fb-macos`)
4. **STOP and VALIDATE**: macOS framebuffer renders correctly end-to-end
5. Merge / demo P1 delivery

### Incremental Delivery

1. **Foundation** (Phase 1+2): Protocol defined → shared by all platforms
2. **macOS** (Phase 3): First working platform → MVP!
3. **Linux** (Phase 4): Linux migration → all platforms consistent
4. **Resilience** (Phase 5): Interruption handling → production-quality
5. **Polish** (Phase 6): Docs + debug tooling → developer-ready

### Parallel Team Strategy

- Developer A: Phase 3 (US1 — macOS helper Swift + CQDisplay)
- Developer B: Phase 4 (US2 — Linux helper C++ + fbx/fbwl refactor)
- Both teams unblock simultaneously after Phase 2 completes.

---

## Notes

- All tasks follow TDD — test ID precedes its implementation ID in execution order
- `[P]` tasks write to different files and have no incomplete-task dependencies
- `[Story]` label is omitted in Phase 1, 2, and 6 (infrastructure/polish)
- Each user story phase is independently validatable without the others
- Commit after each completed task or logical group
- `CQDisplay::data()` and `CXDisplay::data()` / `CWDisplay::data()` MUST return TRUE even after socket close — rendering must never abort due to a display failure
- Windows (`fbw.cpp`) is untouched throughout
