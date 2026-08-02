# T001 Baseline (pre-change, master @ 6960b86)

Captured 2026-08-01. This is the FR-030/SC-007 performance-regression and
FR-024 correctness-regression reference point every subsequent phase's
checkpoint compares against.

## Build

- `cmake --build build --config Release` (full `all` target) **fails** on a
  pre-existing, unrelated issue: the `orender-fb-macos` Swift framebuffer
  target hits `error: module '_DarwinFoundation1' is defined in both ...` —
  a stale ModuleCache collision between two absolute paths for the same repo
  (`/Users/juvenal/Projects/...` vs `/Volumes/Projects/...`, apparently a
  symlink/mount alias). Not caused by this feature; not touched by this
  feature's scope (`src/ri/`, `src/libshader/`). Confirmed the actual targets
  this feature depends on build cleanly in isolation:
  `cmake --build build --target orender test_visual_render test_radial_histogram test_disk_sampling`
  — all succeed. Use targeted `--target` builds for the remainder of this
  feature to avoid tripping over the unrelated Swift target.

## `ctest -L visual` (44 tests)

100% pass (44/44), 106.19 sec*proc total. **No pre-existing red state** —
contrary to CLAUDE.md's general warning about stale `.slo` deploy-tree
shaders (wood/blue_marble/brushedmetal), all `-slo` variants currently pass
on this checkout. No regression accounting needed; any later red here is a
genuine regression.

## `ctest -R "disk_sampling|radial_histogram"`

- `DiskSampling` (ctest test #2, labels `core;sampling`): **Passed** (0.49s).
- `radial_histogram`: **not registered as a ctest test.** The
  `test_radial_histogram` executable is built (`tests/visual/CMakeLists.txt`)
  but never wired via `add_test`/`add_visual_test` — it exists as a
  standalone manual-verification binary from spec 007, not a gated ctest
  target. quickstart.md's `-R radial_histogram` command currently matches
  zero tests ("No tests were found!!!"). This is a pre-existing gap, not
  something this feature broke — noted here so later phases don't mistake
  "no tests found" for a build regression.

## Perf timing (single run, wall time via `time`)

| Scene | user | system | wall (real) |
|---|---|---|---|
| `camera-dof.rib` | 0.09s | 0.03s | 0.167s |
| `motion-1-reyes.rib` | 2.35s | 0.04s | 1.277s |
| `motion-1-raytrace.rib` | 2.57s | 0.03s | 1.333s |

All three renders completed and produced valid TIFF output
(`camera-dof.tif` 21635 bytes, `motion-1-reyes.tif` 137367 bytes,
`motion-1-raytrace.tif` 138697 bytes). `camera-dof.rib`'s wall time is short
enough that single-run noise may dominate the ±2-3% FR-030 budget — later
phases should average a few runs on this scene specifically before flagging
a regression.

## Environment used

```
SHADERS=<repo>/openrender/shaders
ORENDERHOME=<repo>/openrender
DISPLAYS=<repo>/openrender/displays
GEOMETRIES=<repo>/openrender/geometry
```
