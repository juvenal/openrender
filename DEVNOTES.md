# Developer Notes

## Open Issues

- [ ] Purging tessellations for raytracing (Incomplete: no cache eviction mechanism found)
- [ ] Moving raytraced surface (Incomplete: CRaytracer lacks native motion blur support)
- [ ] Efficient subdivision surface creases
- [ ] Subdivision highly creased surface issues
- [x] Bug: orender hangs after render completes when framebuffer output is active (FIXED — `posix_spawn_file_actions` redirects child stdio to `/dev/null`; see research.md D-11)
- [x] Bug: Successive orender runs accumulate multiple framebuffer windows (FIXED — helper persists with outer `accept()` loop; driver tries `tryConnectExisting()` before spawning)
- [ ] Framebuffer Linux migration — `orender-fb-linux` helper + refactor `fbx.cpp` / `fbwl.cpp` to IPC clients (**Broken** — binary builds but helper has critical role inversion bug; driver uses PID-based socket path; neither side works. See Framebuffer section below for full defect list.)
- [ ] Irradiance accuracy issues

## Development Notes

### oshader Shading Language Compiler

- [x] **IR-based Backend:** Transitioned to an Intermediate Representation (IR) module structure.
- [x] **Optimization Passes:** Integrated Constant Folding, Common Subexpression Elimination (CSE), Dead Code Elimination (DCE), and Uniform Lifting.
- [x] **New Extension (.rslo):** Updated compiled shader extension to `.rslo` for RenderMan Shading Language Object compatibility.
- [x] **64-bit Compatibility:** Shader VM now uses IR with separated opcodes; alignment headers present.
- [ ] **Roadmap:** Detailed plans for LLVM integration, binary shader compilation, and imager shader support are documented in [OSHADER_UPDATES.md](OSHADER_UPDATES.md).

### Framebuffer Display Architecture (specs/004-macos-framebuffer-output)

Unified IPC-based framebuffer display model: the renderer spawns a standalone helper executable, connects via a Unix domain socket (fixed per-user path `/tmp/orender-fb-<uid>.sock`), and streams TLV-encoded packets (START / DATA / DONE / QUIT). The helper owns the window lifecycle independently of the renderer.

- [x] **TLV IPC protocol** (`src/framebuffer/fbipc.h`): shared C++ header — opcodes, packed packet structs, socket path utilities. Full spec in `specs/004-macos-framebuffer-output/contracts/ipc-protocol.md`.
- [x] **macOS driver** (`src/framebuffer/fbq.h` / `fbq.cpp`): `CQDisplay` — `posix_spawn` helper, connect, send START/DATA/DONE. Tries to reuse existing helper before spawning.
- [x] **macOS helper** (`src/framebuffer/orender-fb-macos/`): Swift 6.3 / AppKit / SwiftUI. `NSPanel` (HUD style). Persistent outer `accept()` loop — one new window per render; exits when user closes all windows.
- [x] **Helper persistence**: After DONE, helper loops back to `accept()`. Successive renders reuse the same process. `tryConnectExisting()` in driver avoids redundant spawns.
- [x] **Multiple windows**: Each render opens its own `NSPanel`. All remain visible until individually closed. `AppDelegate` tracks a `sessions: [Session]` array; `NSApp.terminate()` fires only when the array empties.
- [x] **TTY hang fix**: `posix_spawn_file_actions_adddup2` redirects child stdin/stdout/stderr to `/dev/null`. Without this, AppKit modifies the inherited controlling TTY and orender's C-runtime stdio flush blocks after `main()` returns. `POSIX_SPAWN_SETSID` was tried and rejected (breaks Mach bootstrap port / WindowServer connection).
- [x] **CoreServices elimination**: `proc_pidpath()` (`<libproc.h>`) replaces `_NSGetExecutablePath()`. CoreServices initializes background threads that prevent clean process exit. Removed `-framework CoreServices` from `src/common/CMakeLists.txt`.
- [ ] **Linux X11 migration** (`src/framebuffer/fbx.cpp` + `orender-fb-linux/main.cpp`): **Builds but is architecturally broken and untested.** Specific defects:
  - **CRITICAL — role inversion** (`main.cpp:717-724`): helper calls `connect()` (client role) instead of `bind()+listen()+accept()` (server role). Both driver and helper call `connect()`; nobody binds. Socket never connects.
  - **CRITICAL — PID-based socket path** (`fbx.cpp:79`): uses `makeSocketPath(getpid())` — path changes every run; helper reuse is impossible. Must use `makeFixedSocketPath()` (UID-based).
  - No `tryConnectExisting()` — driver always spawns a fresh helper even when one is already running.
  - No `/dev/null` stdio redirect — helper inherits orender's controlling TTY (same TTY hang risk as the macOS bug fixed in D-11).
  - Wrong failure flag on spawn error: sets `disconnected = true` instead of `failure = TRUE`.
  - No `signal(SIGCHLD, SIG_IGN)` — exited helpers may become zombies.
  - No `signal(SIGPIPE, SIG_IGN)` before `sendStart()` — write to disconnected socket crashes driver.
  - No outer accept loop in helper — single session only; process exits after one render.
  - No multiple-window support — no equivalent of `AppDelegate.sessions` array.
- [ ] **Linux Wayland migration** (`src/framebuffer/fbwl.cpp`): Same defects as X11 above (same driver pattern, same helper binary). Untested.
- [ ] **Documentation**: Hugo site page for new framebuffer IPC architecture and macOS build notes.

**Key files**: `src/framebuffer/fbipc.h`, `fbq.cpp`, `orender-fb-macos/Sources/SocketServer.swift`, `AppDelegate.swift`  
**Full spec**: `specs/004-macos-framebuffer-output/` (plan.md, research.md, contracts/ipc-protocol.md)

---

### Geometry Statement Support

- [x] **RiGeometry Implementation:** Custom RIB-based expansion for named geometry. See [GEOMETRY_STATEMENT.md](GEOMETRY_STATEMENT.md) for full implementation details and recursion safety mechanisms.

### Hider Parity: Stochastic vs. Raytrace

To ensure 'stochastic' and 'raytrace' hiders produce virtually identical images, the following areas must be aligned:

- [x] **Unified Pixel Filtering:** Both hiders already utilize the global `CRenderer::pixelFilterKernel` precomputed in `beginFrame`, ensuring consistent anti-aliasing.
- [x] **Sampling Distribution:** Both hiders respect `Option "hider" "float jitter"` for sample positions.
- [ ] **Motion Blur Implementation:** `CRaytracer` needs to implement support for moving surfaces (interpolation of vertex positions over time) to match the stochastic hider's temporal sampling.
- [ ] **Shading Interpolation & Derivatives:** Ensure that shading derivatives (Du, Dv) and variables like `s`, `t`, `u`, `v` are computed consistently. Stochastic hider shades at micro-polygon vertices, while Raytrace shades at intersection points.
- [ ] **Displacement Parity:** Both hiders should use the same dicing/tessellation levels for displaced surfaces. Raytracer currently stubs some advanced displacement cases.
- [ ] **Transparency Handling:** Align the `opacityThreshold` and `transmission` logic in `CRaytracer` with the fragment-based blending used in `CStochastic`.

### Possible Optimization

(To be documented.)

### Todos

- [ ] OpenEXR input for textures (Output is supported, but input/texture reading is missing)
- [x] RiDisplayChannel & support (Done: Implemented in CRendererContext and CRibOut)
- [x] Additional attributes, options visible from SL (Done: attribute() and option() implemented in oshader)
- [x] bake, pointcloud and brickmap support (Done)
- [x] RiFilter support (Done: RiPixelFilter implemented with standard kernels)
- [ ] Trace subsets (Incomplete: trace() does not yet support filtering by subset)
- [ ] Patch crack stitching (Incomplete: currently handled via displacement bounds)

### Missing Specification Features (RISpec 3.2 Gaps)

- [ ] **Imager Shaders (RiImager):** Currently stubbed in `src/ri/rendererContext.cpp:993`; returns `CODE_INCAPABLE`. Plan for full support in the next major version.
- [ ] **Blobby Implicit Surfaces (RiBlobby):** Currently stubbed in `src/ri/rendererContext.cpp:4711`; returns `CODE_INCAPABLE`.
- [ ] **NURBS Trim Curves (RiTrimCurve):** Currently stubbed in `src/ri/rendererContext.cpp:3527`; returns `CODE_INCAPABLE`.
- [ ] **Solid Modeling / CSG (RiSolidBegin/End):** Constructive Solid Geometry is stubbed in `src/ri/rendererContext.cpp:4719`; returns `CODE_OPTIONAL`.
- [ ] **Raytraced Motion Blur:** Standard hider supports it, but `CRaytracer` (`src/ri/raytracer.cpp`) needs implementation for moving surfaces (noted in `src/ri/curves.cpp:364`).
- [ ] **Interior/Exterior Volume Shaders (RiInterior/RiExterior):** Logically unimplemented due to missing CSG support.
- [ ] **Trace Subsets:** `trace()` in shading language (`src/ri/trace.cpp`) and built-in functions do not yet filter by the `subset` parameter.
