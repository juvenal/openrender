# RenderMan Spec 3.2 — Implementation Gaps

This file tracks the delta between RI Spec 3.2 and the current openRender implementation. Items marked `[x]` are fully implemented. Items marked `[ ]` are stubbed or unimplemented.

## Feature Coverage

- [x] **Imager Shaders (RiImager):** Fully implemented per RI Spec 3.2. See [OSHADER_UPDATES.md](OSHADER_UPDATES.md) and `specs/005-imager-shader-support/`.
- [ ] **Blobby Implicit Surfaces (RiBlobby):** Currently stubbed in `src/ri/rendererContext.cpp:4711`; returns `CODE_INCAPABLE`.
- [ ] **NURBS Trim Curves (RiTrimCurve):** Currently stubbed in `src/ri/rendererContext.cpp:3527`; returns `CODE_INCAPABLE`.
- [ ] **Solid Modeling / CSG (RiSolidBegin/End):** Constructive Solid Geometry is stubbed in `src/ri/rendererContext.cpp:4719`; returns `CODE_OPTIONAL`.
- [ ] **Raytraced Motion Blur:** Standard hider supports it, but `CRaytracer` (`src/ri/raytracer.cpp`) needs implementation for moving surfaces (noted in `src/ri/curves.cpp:364`).
- [ ] **Interior/Exterior Volume Shaders (RiInterior/RiExterior):** Logically unimplemented due to missing CSG support.
- [ ] **Trace Subsets:** `trace()` in shading language (`src/ri/trace.cpp`) and built-in functions do not yet filter by the `subset` parameter.
