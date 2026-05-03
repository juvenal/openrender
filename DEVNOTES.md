# Developer Notes

## Open Issues

- [ ] Purging tessellations for raytracing (Incomplete: no cache eviction mechanism found)
- [ ] Moving raytraced surface (Incomplete: CRaytracer lacks native motion blur support)
- [ ] Efficient subdivision surface creases
- [ ] Subdivision highly creased surface issues
- [x] Bug: Terminating the framebuffer during rendering (FIXED)
- [ ] Irradiance accuracy issues

## Development Notes

### oshader Shading Language Compiler

- [x] **IR-based Backend:** Transitioned to an Intermediate Representation (IR) module structure.
- [x] **Optimization Passes:** Integrated Constant Folding, Common Subexpression Elimination (CSE), Dead Code Elimination (DCE), and Uniform Lifting.
- [x] **New Extension (.rslo):** Updated compiled shader extension to `.rslo` for RenderMan Shading Language Object compatibility.
- [x] **64-bit Compatibility:** Shader VM now uses IR with separated opcodes; alignment headers present.
- [ ] **Roadmap:** Detailed plans for LLVM integration, binary shader compilation, and imager shader support are documented in [OSHADER_UPDATES.md](OSHADER_UPDATES.md).

### Geometry Statement Support

- [x] **RiGeometry Implementation:** Custom RIB-based expansion for named geometry. See [GEOMETRY_STATEMENT.md](GEOMETRY_STATEMENT.md) for full implementation details and recursion safety mechanisms.

### Hider Parity: Stochastic vs. Raytrace

To ensure 'stochastic' and 'raytrace' hiders produce virtually identical images, the following areas must be aligned:

- [x] **Unified Pixel Filtering:** Both hiders already utilize the global `CRenderer::pixelFilterKernel` precomputed in `beginFrame`, ensuring consistent anti-aliasing.
- [x] **Sampling Distribution:** Both hiders respect `Option "hider" "float jitter"` for sample positions.
- [ ] **Motion Blur Implementation:** `CRaytracer` needs to implement support for moving surfaces (interpolation of vertex positions over time) to match the stochastic hider's temporal sampling.
- [ ] **Shading Interpolation & Derivatives:** Ensure that shading derivatives (Du, Dv) and variables like `s`, `t`, `u`, `v` are computed consistently. Stochastic hider shades at micro-polygon vertices, while Raytrace shades at intersection points.
- [ ] **Displacement Parity:** Both hiders should use the same dicing/tessellation levels for displaced surfaces. Raytracer currently stubs some advanced displacement cases.
- [ ] **Transparency Handling:** Align the `opacityThreshold` and `transmission` logic in `CRaytracer` with the fragment-based blending used in `CStochastic`.

### Possible Optimization

(To be documented.)

### Todos

- [ ] OpenEXR input for textures (Output is supported, but input/texture reading is missing)
- [x] RiDisplayChannel & support (Done: Implemented in CRendererContext and CRibOut)
- [x] Additional attributes, options visible from SL (Done: attribute() and option() implemented in oshader)
- [x] bake, pointcloud and brickmap support (Done)
- [x] RiFilter support (Done: RiPixelFilter implemented with standard kernels)
- [ ] Trace subsets (Incomplete: trace() does not yet support filtering by subset)
- [ ] Patch crack stitching (Incomplete: currently handled via displacement bounds)

### Missing Specification Features (RISpec 3.2 Gaps)

- [ ] **Imager Shaders (RiImager):** Currently stubbed in `src/ri/rendererContext.cpp:993`; returns `CODE_INCAPABLE`. Plan for full support in the next major version.
- [ ] **Blobby Implicit Surfaces (RiBlobby):** Currently stubbed in `src/ri/rendererContext.cpp:4711`; returns `CODE_INCAPABLE`.
- [ ] **NURBS Trim Curves (RiTrimCurve):** Currently stubbed in `src/ri/rendererContext.cpp:3527`; returns `CODE_INCAPABLE`.
- [ ] **Solid Modeling / CSG (RiSolidBegin/End):** Constructive Solid Geometry is stubbed in `src/ri/rendererContext.cpp:4719`; returns `CODE_OPTIONAL`.
- [ ] **Raytraced Motion Blur:** Standard hider supports it, but `CRaytracer` (`src/ri/raytracer.cpp`) needs implementation for moving surfaces (noted in `src/ri/curves.cpp:364`).
- [ ] **Interior/Exterior Volume Shaders (RiInterior/RiExterior):** Logically unimplemented due to missing CSG support.
- [ ] **Trace Subsets:** `trace()` in shading language (`src/ri/trace.cpp`) and built-in functions do not yet filter by the `subset` parameter.
