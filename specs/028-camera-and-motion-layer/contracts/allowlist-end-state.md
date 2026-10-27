# Contract: allowlist, metrics and registry end state

**Requirements**: FR-016, FR-019, FR-022, FR-023, SC-001, SC-002, SC-003, SC-009.
**Baseline**: macOS, `build-028` at master `4eb683b`, 2026-10-27
([../tools/archcheck-base-macos.txt](../tools/archcheck-base-macos.txt)):
`ALLOWED DB-004 x 65`, `DB-011 x 173`; `METRIC Geometry 78/19`, `Hiders 567/84`, `TOTAL 721/110`;
`RESULT PASS`.

The allowlist only shrinks: **no line is added, re-keyed or swapped**. Each phase deletes its own
lines first; `ctest -L architecture` then reports exactly those edges as `VIOLATION` until the
phase's code lands, and is green with nothing else changed at the phase's end.

## Lines deleted (20 on macOS; the same 20 on Linux, none is platform-tagged)

**Phase 4 (camera), DB-011, 7 lines** (`tests/architecture/allowlist.txt:202, 204, 212, 213, 238,
239, 240` at the baseline):

```text
DB-011  link     src/ri/hiders/raytracer.cpp  ->  _ZN9CRenderer10imagePlaneE   # CRenderer::imagePlane
DB-011  link     src/ri/hiders/raytracer.cpp  ->  _ZN9CRenderer10projectionE   # CRenderer::projection
DB-011  link     src/ri/hiders/raytracer.cpp  ->  _ZN9CRenderer13focaldistanceE   # CRenderer::focaldistance
DB-011  link     src/ri/hiders/raytracer.cpp  ->  _ZN9CRenderer13invImagePlaneE   # CRenderer::invImagePlane
DB-011  link     src/ri/hiders/raytracer.cpp  ->  _ZN9CRenderer8apertureE   # CRenderer::aperture
DB-011  link     src/ri/hiders/raytracer.cpp  ->  _ZN9CRenderer8dxdPixelE   # CRenderer::dxdPixel
DB-011  link     src/ri/hiders/raytracer.cpp  ->  _ZN9CRenderer8dydPixelE   # CRenderer::dydPixel
```

`invImagePlane` and `dydPixel` are referenced only through the inline `fullImagePixels2camera()`
(`renderer.h:618`), so their edges go when `computeSamples` stops calling it. `raytracer.cpp ->
renderer.h` (include) and its other 36 link lines stay (DB-011, spec 029/030).

**Phase 5 (ray transform), DB-004, 6 lines** (`:57, 58, 59, 72, 73, 74`):

```text
DB-004  link     src/ri/geometry/dlobject.cpp  ->  _ZN9CRenderer17cameraHasRotationE   # CRenderer::cameraHasRotation
DB-004  link     src/ri/geometry/dlobject.cpp  ->  _ZN9CRenderer7relRotQE   # CRenderer::relRotQ
DB-004  link     src/ri/geometry/dlobject.cpp  ->  _ZN9CRenderer7toWorldE   # CRenderer::toWorld
DB-004  link     src/ri/geometry/quadrics.cpp  ->  _ZN9CRenderer17cameraHasRotationE   # CRenderer::cameraHasRotation
DB-004  link     src/ri/geometry/quadrics.cpp  ->  _ZN9CRenderer7relRotQE   # CRenderer::relRotQ
DB-004  link     src/ri/geometry/quadrics.cpp  ->  _ZN9CRenderer7toWorldE   # CRenderer::toWorld
```

**Phases 6, 7, 8 and 11 (`invShutterTime`), DB-004, 7 lines** (`:55, 56, 67, 68, 69, 70, 71`):

| Phase | Line |
|---|---|
| 6 | `DB-004  link     src/ri/geometry/quadrics.cpp  ->  _ZN9CRenderer14invShutterTimeE   # CRenderer::invShutterTime` |
| 7 | `DB-004  link     src/ri/geometry/patches.cpp  ->  _ZN9CRenderer14invShutterTimeE   # CRenderer::invShutterTime` |
| 8 | `DB-004  link     src/ri/geometry/polygons.cpp  ->  _ZN9CRenderer14invShutterTimeE   # CRenderer::invShutterTime` |
| 11 | `DB-004  link     src/ri/geometry/curves.cpp  ->  _ZN9CRenderer14invShutterTimeE   # CRenderer::invShutterTime` |
| 11 | `DB-004  link     src/ri/geometry/points.cpp  ->  _ZN9CRenderer14invShutterTimeE   # CRenderer::invShutterTime` |
| 11 | `DB-004  link     src/ri/geometry/patchgrid.cpp  ->  _ZN9CRenderer14invShutterTimeE   # CRenderer::invShutterTime` |
| 11 | `DB-004  link     src/ri/geometry/bsplinePatchgrid.cpp  ->  _ZN9CRenderer14invShutterTimeE   # CRenderer::invShutterTime` |

Phases 9 (implicit) and 10 (tessellation) delete no line.

## Lines kept, and why

- **The DB-004 include lines** of the touched files (`objectMisc.h`, `dlobject.cpp`,
  `implicitSurface.cpp`, `patchgrid.cpp`, `points.cpp`, `curves.cpp`, `quadrics.cpp`,
  `patches.cpp`, `polygons.cpp -> renderer.h`/`rendererContext.h`): the `#include` lines remain,
  so the edges remain. After phase 11 several of these files name no `CRenderer` member. Removing
  those includes is optional pruning (plan, open question Q4); by default 028 does not remove them,
  and they stay valid lines (not `STALE`).
- **`surface.cpp` and `object.cpp`'s 16 DB-004 dicing lines** (`imagePlane`, `projection`,
  `dPixeldx/dy`, `pixelLeft/Top`, `cocFactorPixels`, `invFocaldistance`, `flags`,
  `raytracingFlags`, ...): spec 031's (FR-018). Phase 10 edits only `CTesselationPatch::intersect`'s
  moving branch, which reads none of them.
- **Every Reyes and zbuffer DB-011 line**: spec 031.

## End state

| Measure | Baseline (macOS) | End (macOS) | Source |
|---|---|---|---|
| `ALLOWED DB-004` | 65 | **52** (65 - 6 - 7) | SC-003 "at most 52" |
| `ALLOWED DB-011` | 173 | **166** (173 - 7) | SC-003 "at most 166" |
| allowlist header `DB-004 (65)` | 65 | 52 | SC-009 |
| allowlist header `DB-011 (182)` (both platforms' lines) | 182 | 175 | SC-009 |
| `METRIC Geometry` | 78 / 19 | predicted **48 / 15** (-24 `invShutterTime`, -4 `relRotQ`, -1 `toWorld`, -1 `cameraHasRotation`) | lowered to the check's report |
| `METRIC Hiders` | 567 / 84 | predicted **558 / 82** (-9 references in `computeSamples`; `focaldistance` and `dxdPixel` leave the class) | lowered to the check's report |
| `METRIC TOTAL` | 721 / 110 | predicted **682 / 107** (`invShutterTime`, `focaldistance`, `dxdPixel` leave every counted class) | lowered to the check's report |
| Linux | measured in the Linux baseline run | the same 20 lines deleted; counts per the Linux report | quickstart §4 |

The metric predictions come from a `grep` of code (not comment) references; the check's own report
is authoritative, and `metrics-baseline.txt` is lowered only to values it prints (`NOTE metric ... <
baseline`), never raised for any class.

## Registry text (`DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`, phase 12)

- **DB-009** stays **Open**. Evidence narrowed to what remains: Reyes's circle-of-confusion model
  (`CSampler::circleOfConfusion`, `sampler.cpp:23`, 7 call sites in `reyes.cpp`) against the
  camera layer's thin lens, and Reyes's own motion and camera-rotation code (`reyes.cpp`,
  `stochastic.cpp`, `stochasticQuad.h`), with the line references refreshed. The `invShutterTime`
  bullet is removed (the reads are gone). Tracking stays on spec 031 (#37). History: "spec 028,
  2026-MM-DD: the raytrace side moved onto `ICamera`, `FrameParams` and `motion.h`; Reyes's side is
  spec 031". Never "Resolved".
- **DB-004**: evidence drops `invShutterTime` (24), `relRotQ` (4), `toWorld`, `cameraHasRotation`;
  "Measured by the architecture check" becomes 52 edges (28 includes, 24 links); History records
  spec 028, the date, the 13 deleted lines.
- **DB-011**: Size corrected from the stale "180 edges per platform" to the check's counts (173
  before, 166 after on macOS; Linux per its report); History records spec 028, the date and the 7
  deleted lines.
- Nothing refers to a merge commit or to anything that exists only after the merge.
