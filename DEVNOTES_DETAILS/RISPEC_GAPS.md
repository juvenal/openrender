# RenderMan Spec 3.2 — Implementation Gaps

This file tracks the delta between RI Spec 3.2 and the current openRender implementation. Items marked `[x]` are fully implemented. Items marked `[ ]` are stubbed or unimplemented.

## Feature Coverage

- [x] **Imager Shaders (RiImager):** Fully implemented per RI Spec 3.2. See [OSHADER_UPDATES.md](OSHADER_UPDATES.md) and `specs/005-imager-shader-support/`.
- [ ] **Blobby Implicit Surfaces (RiBlobby):** Currently stubbed in `src/ri/rendererContext.cpp:4711`; returns `CODE_INCAPABLE`.
- [x] **NURBS Trim Curves (RiTrimCurve):** Fully implemented per RI Spec 3.2.1 (`CRendererContext::RiTrimCurve`, `src/ri/rendererContext.cpp:4119`). Attribute-scoped trim-loop state consumed by `CNURBSPatchMesh::create()`; shared odd-winding classification test applied at both the REYES (`CPatch::dice()`) and raytrace (`CTesselationPatch`) tessellation paths, plus the `"trimcurve"/"sense"` attribute for inside/outside inversion. `RiTrimCurve`'s existing `CRibOut` serialization (`src/ri/ribOut.cpp`) round-trips correctly with the new storage layout. See `specs/009-nurbs-trim-curves/`.
- [ ] **Solid Modeling / CSG (RiSolidBegin/End):** Constructive Solid Geometry is stubbed in `src/ri/rendererContext.cpp:4719`; returns `CODE_OPTIONAL`.
- [x] **Raytraced Motion Blur:** Verified 2026-08 (spec 008 Phase 8/US6) — `CRaytracer`'s tessellation-path intersection kernels already interpolate geometry on the ray's shutter time (`patches.cpp`/`polygons.cpp`/`quadrics.cpp`); 7 new cross-hider parity scenes confirm convergence with the standard hider. See [HIDER_PARITY.md](HIDER_PARITY.md).
- [ ] **Interior/Exterior Volume Shaders (RiInterior/RiExterior):** Logically unimplemented due to missing CSG support.
- [ ] **Trace Subsets:** `trace()` in shading language (`src/ri/trace.cpp`) and built-in functions do not yet filter by the `subset` parameter.
