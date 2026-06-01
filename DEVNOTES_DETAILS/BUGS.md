# Open Issues & Known Bugs

This file tracks known defects and implementation gaps in openRender. Open items are sorted by impact. Resolved items are retained for reference with their fix rationale.

## Open Issues

- [ ] Purging tessellations for raytracing (Incomplete: no cache eviction mechanism found)
- [ ] Moving raytraced surface (Incomplete: CRaytracer lacks native motion blur support)
- [ ] Efficient subdivision surface creases
- [ ] Subdivision highly creased surface issues
- [ ] Irradiance accuracy issues

## Resolved Bugs

- [x] Bug: CSE optimizer cross-block corruption — `windowhighlight.sl` sphere highlight regression vs. PRMan 3.9 (FIXED — `CCSEPass::cseFn()` now clears `exprMap` at the start of each IR basic block. The shared map caused `"vufloat||0" → yfract` cached in the y-range ELSE block to replace `vufloat tmp 0` in the x-range block, corrupting the boundary test. Fix: intra-block-only CSE in `src/oshader/passes/passCSE.cpp`).
- [x] Bug: `oshader` accepted `Ps` (surface point in light shaders) inside surface shaders — violates RI Spec (FIXED — Added `globalVarScope` map to `CScriptContext`; `getVariable()` restructured so scope check applies to all lookup paths including the `rootFunction` fallback. `Ps` scope changed to `SLC_LIGHT` only. Imager output variables `Ci`/`Oi`/`alpha`/`ncomps`/`time`/`dtime` scope masks corrected to include `SLC_IMAGER`; `src/oshader/rslo.cpp`, `src/oshader/rslo.h`).

- [x] Bug: Framebuffer Linux migration — `orender-fb-linux` helper + refactor `fbx.cpp` / `fbwl.cpp` to IPC clients (FIXED — Unified platform-neutral `fbipc_display.cpp` consolidated macOS/Linux drivers. Linux helper architecture fixed: role inversion corrected, UID socket path, tryConnectExisting, /dev/null redirect, SIGCHLD/SIGPIPE guards, persistent outer accept loop, multi-window threads.)
- [x] Bug: Renderer crashes when converting non-finite float values to bytes (FIXED — `floatToByte` in `align.h` now guards against `NaN` and infinity before clamped conversion).
- [x] Bug: macOS build fails due to outdated system Bison/Flex (FIXED — CMake updated to prioritize Homebrew keg-only paths for modern Bison/Flex versions).
- [x] Bug: orender hangs after render completes when framebuffer output is active (FIXED — `posix_spawn_file_actions` redirects child stdio to `/dev/null`; see `specs/004-macos-framebuffer-output/research.md` D-11)
- [x] Bug: Successive orender runs accumulate multiple framebuffer windows (FIXED — helper persists with outer `accept()` loop; driver tries `tryConnectExisting()` before spawning)
