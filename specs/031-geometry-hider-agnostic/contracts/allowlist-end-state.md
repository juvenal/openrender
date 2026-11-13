# Contract: allowlist, registry and metric end state

**Requirements**: FR-016, FR-017, FR-018; US6; SC-003 to SC-007. Every line below is **deleted**; none is added.
The one other edit is the DB-017 re-key below (approved 2026-11-09). Counts are from `e83d005d` (344 entries) and are re-measured at each phase; the second spec of this round
to merge re-measures the shared figures (research R14).

## Deleted lines, by entry and phase

| Entry | Lines | Phase | Which |
|---|---|---|---|
| DB-025 | 1 | 4 | `geometry/surface.cpp -> dataviewer/debug.h` (`:33`), and the two dead blocks that use `CDebugView`: `DEBUG_HITS` (`:856`, `:1085-1094` and the `debugHit()` calls at `:1141`, `:1150`) and `DEBUG_TESSELATIONS` (`:857`, `:1833-1857`). The `DEBUG_STATS` blocks do not use the viewer and stay |
| DB-017 | 78 | 3 (1 line), 6 (77) | `object.cpp -> CRenderer::findCoordinateSystem` goes in Phase 3 (the call moves to the host); the other 77 go in Phase 6. all: the 70 Geometry lines bound to `geometryDispatch.cpp` symbols (`CSurface` vtable/typeinfo/`dice`/`intersect`/`shade`/`regionCoverage`/`checkRayGuard`, `CObject::dice`, curve, point, delayed and CSG bodies), the 7 Hiders lines (`photon.cpp` x6, `trace.cpp -> CObject::cluster`), and `object.cpp -> CRenderer::findCoordinateSystem` |
| DB-006 | 30 | 4-6 | all: 23 includes (`reyes.h` x9, `core/shading.h` x13, `core/shader.h` x1) and 7 links (`CShadingContext::shade` x3, `saveState`, `restoreState`, `CReyes::drawObject`, `drawGrid`) |
| DB-004 | 49 | 3-6 | every line except `blobbyRepeller.cpp`'s three (`-> renderer.h`, `CRenderer::locateFile`, `CRenderer::texturePath`) |
| DB-011 | 13 | 7 (and 6) | `reyes.cpp -> CRenderer::aperture, cameraHasRotation, cocFactorSamples, cocFactorScreen, invFocaldistance, relRotQ, relTrans`; `stochastic.cpp -> CRenderer::cameraHasRotation, cameraRotationOnly, relRotQ, relTrans`; `photonMap.cpp -> CRenderer::toWorld, fromWorld` (allowlist lines 137-138, P-Q3 (a); spec 032 deletes the same two, and whichever merges second drops its duplicate) |
| **Total** | **171** | | **344 -> 173** |

The phases' order lets the allowlist shrink at each checkpoint: a line is deleted in the phase that removes its edge,
never earlier (a stale line fails the check) and never later.

## DB-017 re-key (approved amendment, 2026-11-09, tasks-stage Q1)

DB-017 link lines are keyed on mangled symbols (`allowlist.txt:276-279`), so the in-place signature changes of
Phases 4 and 5 (bodies still in `geometryDispatch.cpp`, stubs tracking) change the key of the line. Each affected line
is edited **in place** to the new mangled name and demangled comment: same `DB-017 link <file> ->` prefix, same
function, count unchanged, no line added. Phase 4: 32 lines (`CSurface::regionCoverage` 11, `shade` 9, `intersect` 6,
`checkRayGuard` 3, `CObject::cluster` 1, `CDelayedObject::intersect` 1, `CDelayedInstance::intersect` 1). Phase 5: 20
lines (`CSurface::dice` 7, `CObject::dice` 6, and `CCSGSolid`, `CPoints`, `CCurve`, `CDelayedObject`,
`CDelayedInstance` `dice`, `CLinearCurve`/`CCubicCurve::splitToChildren` 1 each). The 25 `_ZTV8CSurface`/`_ZTI8CSurface`
lines do not change. The diff is shown at the pause of each phase; all 77 remaining lines are deleted in Phase 6.

## Registry (`DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`)

- **DB-006** -> Resolved, "spec 031, <date>": Geometry includes no Hiders or Shading header and calls neither;
  `EShadingDim` is Infra; `CShadingContext`'s Geometry-facing entries and `IShadingProducer` are gone.
- **DB-009** -> Resolved with the lens phase's date: one camera-rotation evaluator (`motion::cameraRotationAt`) for
  both hiders (Phase 7) and one lens (`LensModel::circleOfConfusion`) for both (lens phase). If the lens phase is
  stopped by the maintainer, DB-009 stays Open with a history entry for the rotation half.
- **DB-017** stays Open, narrowed: no allowlist line; the irradiance pair (spec 032) is its only content.
- **DB-004** history: measured counts; what remains (`blobbyRepeller.cpp`, 3 lines), the coordinate-system lookup and the
  expansion now through `CGeometryHost`, the variable lookup through `CDeclarations`, the object sink.
  Its stale evidence (`processDelayedSolid`, `environmentLoad`, "19 files", "27 files reference it") is corrected.
- **DB-011** history: the lines removed and the metric.
- **DB-025** history: the one line; moves to Resolved with whichever of 031 and 032 merges second.
- **DB-023** history: none required (no include path changes).

## Metric (`tests/architecture/metrics-baseline.txt`)

Geometry 48/15 -> 2/2 (`CRenderer::locateFile`, `texturePath`), with the reason. Hiders 556/81 -> the measured value
(about 499). No class rises.
