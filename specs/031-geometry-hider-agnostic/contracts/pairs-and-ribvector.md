# Contract: the three Dispatch/Stub pairs and `libRIBVector`

**Status**: approved 2026-11-07 (P-Q5: the CSG units, `dlobject.cpp`, `implicitSurface.cpp` stay out of
`libRIBVector`). **Requirements**: FR-008, FR-009, FR-010, FR-011; US3; SC-004. **Files**: deleted
`src/ri/render/geometryDispatch.cpp`, `src/ri/vector/geometryDispatchStub.cpp`,
`src/ri/vector/surfaceCrossingDispatchStub.cpp`, `src/ri/render/rendererDeclarationsDispatch.cpp`,
`src/ri/vector/rendererDeclarationsDispatchStub.cpp`; renamed `geometry/surfaceCrossingDispatch.{h,cpp}` ->
`geometry/surfaceCrossings.{h,cpp}`; edited `src/ri/CMakeLists.txt`, `tests/architecture/ribvector-core-residual.txt`,
`render/rendererDeclarations.cpp`, the Geometry files receiving bodies.

## When

Phases 4 and 5 change the signatures **in place**: the bodies stay in `geometryDispatch.cpp` and the three stubs
track each new signature, so `libRIBVector` links at every checkpoint. Phase 6 moves every body, switches the
`libRIBVector` sources and deletes the pair files in one step (moving `CSurface::intersect` moves `CSurface`'s
vtable into `surface.cpp`, which can join `libRIBVector` only once it names no hider).

## Where each body goes (verbatim bodies; only the receivers change)

| Body (`geometryDispatch.cpp` line) | New file | Notes |
|---|---|---|
| `CObject::dice` (`:80`) | `object.cpp` | `dicer.drawObject(cObject)` |
| `CObject::cluster` (`:113`) | `object.cpp` | `rc.scratch()` for `threadMemory` |
| `CCSGSolid::dice` (`:100`) | `csgSolid.cpp` | `resolve(dicer.rayContext()); CObject::dice(dicer);` |
| `CSurface::checkRayGuard` (`:265`), `intersect` (`:308`), `dice` (`:358`), `shade` (`:385`), `regionCoverage` (`:794`) | `surface.cpp` | `surface.cpp` now defines `CSurface`'s key function, so its vtable and typeinfo |
| `CCurve::dice` (`:414`), `CCubicCurve::splitToChildren` (`:540`), `CLinearCurve::splitToChildren` (`:558`) | `curves.cpp` | `f.clipMin`, `dicer.inFrustum`, `f.maxGridSize`, `dicer.probe` |
| `CPoints::dice` (`:576`) | `points.cpp` | |
| `CDelayedObject::intersect`/`dice`, `CDelayedInstance::intersect`/`dice` (`:708-782`) | `delayed.cpp` | through `CGeometryHost` ([host-and-sink.md](host-and-sink.md)) |
| `CRenderer::notifyGlobalVariable` (`rendererDeclarationsDispatch.cpp`) | `render/rendererDeclarations.cpp` | core; the file leaves `libRIBVector` |
| `CRenderer::findCoordinateSystem` (same) | `CRendererContext::findCoordinateSystem` (host) | core |
| `countFreeFormCrossings` | `geometry/surfaceCrossings.cpp` | `IRayContext *` parameter; the stub's "reports 0" behaviour is gone because nothing in `libRIBVector` classifies |

## `src/ri/CMakeLists.txt` (`ribVector_sources`, today `:386-432`)

- `ri_sources`: remove `render/geometryDispatch.cpp`, `render/rendererDeclarationsDispatch.cpp`; rename
  `geometry/surfaceCrossingDispatch.cpp` to `geometry/surfaceCrossings.cpp`; add `geometry/geometryFrame.cpp`.
- `REMOVE_ITEM` list: drop `geometry/surface.cpp`, `geometry/surfaceCrossingDispatch.cpp`,
  `render/geometryDispatch.cpp`, `render/rendererDeclarationsDispatch.cpp` (the latter two no longer exist); add
  `render/rendererDeclarations.cpp`.
- `APPEND` list: only `vector/irradianceDispatchStub.cpp` remains (spec 032's).
- The long comment above the list (`:297-385`) is rewritten to the new state: no Geometry stub; why the CSG units,
  `dlobject.cpp` and `implicitSurface.cpp` stay out (whole classes the preview never constructs, not stand-ins).

## `tests/architecture/ribvector-core-residual.txt` (9 -> 5 lines)

```text
src/ri/render/rendererFileSearch.cpp       src/ri/geometry/blobbyRepeller.cpp (CRenderer::locateFile)
src/ri/render/rendererNetwork.cpp          src/ri/render/rendererFileSearch.cpp
src/ri/render/rendererMutexes.cpp          src/ri/render/rendererFileSearch.cpp
src/ri/render/rendererStatics.cpp          Geometry (blobbyRepeller.cpp), Hiders (compositor.cpp, irradiance.cpp, photonMap.cpp)
src/ri/vector/irradianceDispatchStub.cpp   Hiders (irradiance.cpp)   # spec 032 deletes it
```

Deleted: `rendererDeclarations.cpp`, `geometryDispatchStub.cpp`, `rendererDeclarationsDispatchStub.cpp`,
`surfaceCrossingDispatchStub.cpp`. Spec 032 edits the `rendererStatics.cpp` and irradiance lines too; the second
merger resolves the shared line (R14). With P-Q3 (a), `photonMap.cpp` no longer reads the statics either, so the
Hiders part of that cell loses it (whichever spec measures it).

## Checks

- `SceneInterface_RibVectorResidual` (existing) passes with the shorter list.
- `Geometry_RibVectorNoStubs` (new, [tests.md](tests.md)): `ribVector_obj`'s sources name no `vector/*Stub.cpp`
  other than `irradianceDispatchStub.cpp`, and `nm -u` over `libRIBVector` names no `CShadingContext`, `CReyes`,
  `CHider` or `IShadingProducer` symbol.
- `Preview_*`, `SceneInterface_*` and the orender-wire tests pass with unchanged output.
