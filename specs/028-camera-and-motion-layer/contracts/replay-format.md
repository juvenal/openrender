# Contract: the intersection and sample replay

**Tool**: `motion_replay` (`tests/unit/motion/motion_replay.cpp`), links `ribRender` and
`openrendercommon`. **Data**: `tests/unit/motion/replay/<platform>.txt`, `<platform>` is `macos` or
`linux`. **Requirements**: US2 Independent Test, FR-013, FR-014, FR-016, FR-018, FR-024; maintainer
decision 1. Research R13.

## Rules

1. **Starting-tree interfaces only.** The tool calls the Ri API, `CObject::intersect()`,
   `CObject::sample()` and `CObject::materialize()`, with a capture context modelled on
   `tests/unit/csg/csgTestUtils.h`. It names no 028 type, so it builds and records on master
   `4eb683b` unchanged. **One approved exception (maintainer, 2026-10-27, Checkpoint 1):** the tool
   reaches the two private members `CCurveMesh::create(CShadingContext *)` and `CPoints::prep()`
   through the explicit-instantiation idiom (the access check does not apply to the argument of an
   explicit instantiation; no `#define private`, no `src/` edit). Production reaches them only
   through `dice()`, which needs a Reyes bucket, and without them `curves` and `points` have no
   leaf to sample. No other private member is touched.
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
| `bicubic`, `nurbs` | `0 0.5` | static | a moving bicubic patch and a moving NURBS (tessellation path and `sample()`); family tag `tessellation` for **both** the `I` and the `S` records (the `S` records guard the `patches.cpp` sample paths of phase 7) |
| `polygons` | `0 0.5` | static | a moving triangle and a moving quad (`PointsPolygons`) |
| `implicit` | `0 1` | static | the fixture plug-in `implicit_moving_sphere` (centre moves with the plug-in's time) |
| `dlo-camera-{translate,rotate}` | `0 1` | moving | the fixture plug-in `dlo_sphere` |
| `curves`, `points`, `subdiv-grid`, `subdiv-cube` | `0 0.5` | static | moving cubic and linear curves, points, and two Catmull-Clark meshes (`sample()` only, no `I` records): an open 4 x 4 face grid with `interpolateboundary` (border faces become `CPatchGrid`, interior faces `CBicubicPatch`) and a cube with a creased edge (`CBSplinePatchGrid` strips beside `CSubdivision` corner patches). A `PatchMesh` does not make patch grids: `CPatchGrid`, `CBSplinePatchGrid` and `CSubdivision` come only from the subdivision creator, so the plan's `patchmesh` and `bsplinemesh` scenes were replaced (task T013) |
| `empty-shutter` | `0.5 0.5` | static | a moving sphere (`invShutterTime = 1`, motion sampling cleared) |

Fixture plug-ins are test-only `MODULE` libraries in `tests/unit/motion/fixtures/`, implementing the
unchanged `implicit.h` and `dlo.h` ABIs. They are the first coverage of `CImplicit` and `CDLObject`
(research R14).

## Queries

- **Intersection**: a fixed grid of 32 camera-space rays **per leaf** (origins on an 8x4 lattice in
  front of the leaf's bound, directions toward and past it, the last two lattice slots replaced by
  grazing rays), at times `-0.25, 0, 0.25, 0.5, 0.75, 1, 1.25`. The ray differential is a fixed
  orthographic footprint (`db = 0.1`): a zero footprint asks a `CSurface` for its finest
  tessellation, a tree of hundreds of thousands of nodes. The tool walks the hierarchy under the leaf
  as `CShadingContext::trace()` does (a lazily built tessellation child is visited and clustered). A
  record holds: hit flag, and for a hit `t`, `u`, `v`, `N[3]`; a miss carries no further field. The
  leaf's index is in capture order. Scenes of the `gridonly` family have no `I` records (curves,
  points and subdivision patches are not raytraced).
- **Sample**: per leaf, a 3x3 `(u, v)` grid (2x2 for the two subdivision meshes, whose leaf counts
  are large; `CPoints` samples all its points at once, 3x3 = its 9 points) at the same seven times, then with `PARAMETER_BEGIN_SAMPLE` and with
  `PARAMETER_END_SAMPLE`, requesting `P`, `Ng`, `dPdu` and `dPdv`, **and `dPdtime` only with
  `PARAMETER_BEGIN_SAMPLE`**: `CPatchGrid` and `CBSplinePatchGrid` read their end keys through a
  pointer that is already the end key set (with `PARAMETER_END_SAMPLE`) or already advanced past
  its scratch array (at an interpolated time), so `dPdtime` there reads outside the array and
  differs from run to run (an existing defect, filed as GitHub #110, not pinned and not fixed;
  FR-014).
  Varyings are set up as `surface.cpp:159-172` does, with zeroed storage in every slot; no
  primitive variables. A record holds the five vectors.

## Format

```text
# motion_replay v1 <platform> <tree>          e.g. "macos master-4eb683b"
<scene> <family> I <leaf> <ray> <time> <hit> <t> <u> <v> <Nx> <Ny> <Nz>
<scene> <family> S <leaf> <uv> <time|BEGIN|END> <Px> <Py> <Pz> <Ngx> ... <dPdtimez>
```

Records appear in the order of the tool's scene table, then kind (`I` before `S`), leaf, ray or
`(u, v)` index, and time (the seven times in ascending order, then `BEGIN`, `END`). `--check <file> [--family <f>]` prints the first
10 differing lines per family and exits 1 on any difference, any missing line or any extra line.

## ctest registrations (label `motion`)

`Motion_Replay_quadrics`, `_raytransform` (the camera and #106 scenes, the dlo scenes),
`_bilinear`, `_polygons`, `_implicit`, `_tessellation`, `_gridonly` (curves, points, patch grids,
subdivision), `_emptyshutter`. Each runs `motion_replay --check replay/<platform>.txt --family <f>`.
If the platform's file does not exist, the test fails with "no replay file for <platform>": it is
never skipped silently.

## Red evidence

Recorded on the starting tree, the replay is green there by construction (a characterization
test). Its red evidence is a mutation per family in the scratch copy `build-028-scratch` (a gitignored directory inside the worktree): swap the
operands of one `interpolatev` (patches, polygons, tessellation), drop the `(double)` of one quadric
parameter, replace one `CRenderer::invShutterTime` with `1.0f` (sample paths), drop the conjugate in
the ray SLERP. Each mutation must turn exactly its family's test red (plan, open question Q3). A mutation that is bitwise-equivalent for every input gets a named substitute, used only with the maintainer's approval at that time.
