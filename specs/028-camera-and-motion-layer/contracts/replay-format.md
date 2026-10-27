# Contract: the intersection and sample replay

**Tool**: `motion_replay` (`tests/unit/motion/motion_replay.cpp`), links `ribRender` and
`openrendercommon`. **Data**: `tests/unit/motion/replay/<platform>.txt`, `<platform>` is `macos` or
`linux`. **Requirements**: US2 Independent Test, FR-013, FR-014, FR-016, FR-018, FR-024; maintainer
decision 1. Research R13.

## Rules

1. **Starting-tree interfaces only.** The tool calls the Ri API, `CObject::intersect()`,
   `CObject::sample()` and `CObject::materialize()`, with a capture context modelled on
   `tests/unit/csg/csgTestUtils.h`. It names no 028 type, so it builds and records on master
   `4eb683b` unchanged.
2. **Recorded once per platform, on the starting tree** (decision 1), before any `src/` change:
   macOS in phase 1, Linux in the maintainer's Linux baseline run at Checkpoint 2 (quickstart §4a).
   A replay file is never re-recorded from a changed tree. There is no cross-platform comparison.
   Recording goes through ctest, never by running the binary directly: `Motion_ReplayRecord`
   (label `motion-record`, `TIMEOUT` set) runs `motion_replay --record` into the **build tree**
   (`<build>/tests/unit/motion/replay-recorded-<platform>.txt`); copying that file to
   `tests/unit/motion/replay/<platform>.txt` is a deliberate, starting-tree-only step. Running the
   record test on a later tree therefore cannot overwrite the tracked replay.
3. **Bitwise.** Every float is printed as a C99 hex float (`%a`), and every check compares text.
4. **Production paths.** The capture context's `addObject` calls `CRendererContext::addObject`
   first, so camera-motion synthesis (`rendererContext.cpp:686-760`) runs as in production, then
   attaches and records the object. Rays are fired at each captured object's leaves after
   `materialize()`, and at the object itself for `CSurface` (the tessellation path,
   `geometryDispatch.cpp`).

## Scenes (each in its own in-process session)

| Scene id | Shutter | Camera | Content |
|---|---|---|---|
| `quadrics-deform` | `0 0.5` | static | the 7 quadrics, each `MotionBegin` around two parameter sets |
| `quadrics-xform` | `0.25 0.75` | static | the 7 quadrics under `MotionBegin` `Translate` + `Rotate` |
| `quadrics-camera-{translate,rotate,rotonly}` | `0 1` | `MotionBegin` before `WorldBegin` | static quadrics (camera motion synthesized) |
| `quadrics-own-motion-camera` | `0 1` | moving | a quadric with its own `MotionBegin` `Translate` (#106: no camera motion) |
| `bilinear` | `0 0.5` | static | a moving `Patch "bilinear"` |
| `bicubic`, `nurbs` | `0 0.5` | static | a moving bicubic patch and a moving NURBS (tessellation path and `sample()`) |
| `polygons` | `0 0.5` | static | a moving triangle and a moving quad (`PointsPolygons`) |
| `implicit` | `0 1` | static | the fixture plug-in `implicit_moving_sphere` (centre moves with the plug-in's time) |
| `dlo-camera-{translate,rotate}` | `0 1` | moving | the fixture plug-in `dlo_sphere` |
| `curves`, `points`, `patchmesh`, `bsplinemesh`, `subdiv` | `0 0.5` | static | moving linear and cubic curves, points, a bicubic `PatchMesh` (patch grid), a B-spline `PatchMesh`, a Catmull-Clark mesh (`sample()` only) |
| `empty-shutter` | `0.5 0.5` | static | a moving sphere (`invShutterTime = 1`, motion sampling cleared) |

Fixture plug-ins are test-only `MODULE` libraries in `tests/unit/motion/fixtures/`, implementing the
unchanged `implicit.h` and `dlo.h` ABIs. They are the first coverage of `CImplicit` and `CDLObject`
(research R14).

## Queries

- **Intersection**: a fixed grid of 64 camera-space rays per scene (origins on a 8x8 lattice in front
  of the object, directions toward and past its bound, plus 4 grazing rays), at times `-0.25, 0,
  0.25, 0.5, 0.75, 1, 1.25`. A record holds: hit flag, `t`, `u`, `v`, `N[3]`, and the leaf's index
  in capture order.
- **Sample**: per leaf, a 5x5 `(u, v)` grid at the same seven times, then with
  `PARAMETER_BEGIN_SAMPLE` and with `PARAMETER_END_SAMPLE`, requesting `P`, `Ng`, `dPdu`, `dPdv`,
  `dPdtime`; varyings set up as `surface.cpp:159-172` does; no primitive variables. A record holds
  the five vectors.

## Format

```text
# motion_replay v1 <platform> <tree>          e.g. "macos master-4eb683b"
<scene> <family> I <leaf> <ray> <time> <hit> <t> <u> <v> <Nx> <Ny> <Nz>
<scene> <family> S <leaf> <uv> <time|BEGIN|END> <Px> <Py> <Pz> <Ngx> ... <dPdtimez>
```

Sorted by scene, family, kind, leaf, ray/uv, time. `--check <file> [--family <f>]` prints the first
10 differing lines per family and exits 1 on any difference, any missing line or any extra line.

## ctest registrations (label `motion`)

`Motion_Replay_quadrics`, `_raytransform` (the camera and #106 scenes, the dlo scenes),
`_bilinear`, `_polygons`, `_implicit`, `_tessellation`, `_gridonly` (curves, points, patch grids,
subdivision), `_emptyshutter`. Each runs `motion_replay --check replay/<platform>.txt --family <f>`.
If the platform's file does not exist, the test fails with "no replay file for <platform>": it is
never skipped silently.

## Red evidence

Recorded on the starting tree, the replay is green there by construction (a characterization
test). Its red evidence is a mutation per family in a scratch copy, outside the worktree: swap the
operands of one `interpolatev` (patches, polygons, tessellation), drop the `(double)` of one quadric
parameter, replace one `CRenderer::invShutterTime` with `1.0f` (sample paths), drop the conjugate in
the ray SLERP. Each mutation must turn exactly its family's test red (plan, open question Q3).
