# Open Issues & Known Bugs

This file tracks known defects and implementation gaps in openRender. Open items are sorted by impact. Resolved items are retained for reference with their fix rationale.

## Open Issues

- [ ] Purging tessellations for raytracing (Incomplete: no cache eviction mechanism found)
- [ ] Moving raytraced surface (Incomplete: CRaytracer lacks native motion blur support)
- [ ] Efficient subdivision surface creases
- [ ] Subdivision highly creased surface issues
- [ ] Framebuffer Linux migration — `orender-fb-linux` helper + refactor `fbx.cpp` / `fbwl.cpp` to IPC clients (Architecture fixed — role inversion corrected, UID socket path, tryConnectExisting, /dev/null redirect, SIGCHLD/SIGPIPE guards, persistent outer accept loop, multi-window threads. **Untested** — requires Linux build to validate end-to-end.)
- [ ] Irradiance accuracy issues

## Resolved Bugs

- [x] Bug: orender hangs after render completes when framebuffer output is active (FIXED — `posix_spawn_file_actions` redirects child stdio to `/dev/null`; see `specs/004-macos-framebuffer-output/research.md` D-11)
- [x] Bug: Successive orender runs accumulate multiple framebuffer windows (FIXED — helper persists with outer `accept()` loop; driver tries `tryConnectExisting()` before spawning)
