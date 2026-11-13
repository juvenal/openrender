# Proposed CLAUDE.md changes for spec 031 (draft, 2026-11-12; nothing applied)

`CLAUDE.md` is changed only by the maintainer (spec FR-018 and the project rules), so this file holds the proposed text and **no task edits `CLAUDE.md`**.
It is written against this worktree's `CLAUDE.md` (the Phase 8 base). `master` has moved since: it holds gotcha 36 (spec 056) and gotcha 37 (spec 032), and spec 030 takes 38 at
its merge. **Number 031's new gotcha "next free at merge preparation", not 36, 37 or 38**, and re-read gotchas 13, 14, 17, 19, 26, 28 and 35 on the merged `CLAUDE.md` first (spec 032 or 030
may have edited them; the replacements below are anchored on the text of this worktree). Likewise 031's `kRendererServicesVersion` step is **2.3.0 at its merge** (spec 032 stepped it to
2.2.0 first), so no sentence below fixes the number: it says "the next MINOR after spec 032's 2.2.0". The "Spec-kit agent rules" section is not touched. The numbers are the measured end state.

## 1. Gotcha 13 (CSG region hook): one added sentence

Append: "Geometry's interface headers (`rayContext.h`, `dicer.h`, `objectSink.h`, `geometryFrame.h`) mention neither CSG nor Region (FR-019, grep-checked): the hook's generic names stay where
`reyes.cpp` reads them, and `CRegionPredicate`, `regionCoverage` and `CCSGSolid`'s entry points keep their names."

## 2. Gotcha 14 (CSG classification): the last sentence

Replace "Free-form (NURBS/subdivision/bicubic) crossings live in the renderer-only `geometry/surfaceCrossingDispatch.cpp`, with `vector/surfaceCrossingDispatchStub.cpp` for ribVector."
with "Free-form (NURBS/subdivision/bicubic) crossings live in `geometry/surfaceCrossings.cpp`, compiled into both `libRIBRender` and `libRIBVector`: the dispatch and stub pair is gone (spec 031)."

## 3. Gotcha 17 (Geometry never calls the renderer core): the second half

Replace from "so a solid captures its per-instance leaves ..." to the end with:

> so a solid captures its per-instance leaves and procedural expansions through the Geometry-owned `CGeometryHost` (`geometry/geometryHost.h`: `capture()`, `expand`
> for a delayed object or instance, `findCoordinateSystem`, `releaseAtFrameEnd()`/`frameEnded()`), implemented and registered by `CRendererContext`, and reports the objects it creates
> to an `IObjectSink` (`geometry/objectSink.h`, the core implements it). Never reach `CRenderer`/`CRendererContext` from Geometry (Principle VIII; the registered violations live in
> `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`, DB-004: three lines, all in `blobbyRepeller.cpp`). The instance state holds a reference to every captured object until
> `endFrameResources()` runs in `CRenderer::endFrame()` before `root->destroy()`: REYES consumes objects by refcount, while raytracing frees the hierarchy regardless of it.

## 4. Gotcha 19 (render-only `CSurface` overrides): replace the whole gotcha

> 19. **Geometry is one library for the renderer and for `libRIBVector` (spec 031).** The Geometry bodies that used to live in `render/geometryDispatch.cpp` (`CSurface::regionCoverage`,
>    `CCSGSolid::dice`, the `CSurface`/`CObject` virtuals) are in `src/ri/geometry` and compiled by both libraries; `geometryDispatch.cpp` and `vector/geometryDispatchStub.cpp` no longer exist
>    (the only `vector/` file left is `irradianceDispatchStub.cpp`, spec 032's). A Geometry file may therefore include no Hiders or Shading header and name no `CReyes`/`CShadingContext`:
>    `Geometry_NoHiderShadingNames` scans the directory comments included, and `Geometry_RibVectorNoStubs` checks `libRIBVector`'s undefined symbols.

## 5. Gotcha 26 (`FrameParams` and the motion sites): three edits

- In "A field is a copy of the core static of the same meaning and is never recomputed", add "(spec 031 added `dSampledx` and `dSampledy`, the samples per screen unit the lens scatter and its bounds use,
  and removed the `cocFactor*` copies: the lens values are the `aperture`, `focaldistance`, `invFocaldistance`, `focallength` and `fstop` fields, read through `openrender::LensModel::fromFrame`)".
- Replace "Reyes's own camera-rotation SLERP and motion bounds are untouched (DB-009, spec 031)." with "Reyes rotates the camera through `motion::cameraRotationAt` since spec 031 (the one function the
  raytrace side uses; `Motion_CameraSlerpEquivalence` pins it); Reyes's motion bounds are untouched, and DB-009 is Resolved."
- Add: "Reyes and the stochastic rasterizer read the frame (`frameParams()`, once per function or grid, never per sample) instead of `CRenderer::aperture`, `cameraHasRotation`, `relRotQ`, `relTrans`, `dSampledx/y`
  and the lens statics (the Hiders `CRenderer::` metric is 495 references)."

## 6. Gotcha 28 (Shading's boundary): the version sentence

The sentence "`kRendererServicesVersion` is 2.1.0 since spec 029" becomes "... is the version in `rendererServices.h` (2.1.0 since spec 029; spec 032 stepped it to 2.2.0 and spec 031 to the next MINOR, 2.3.0,
with its history entry)". The Geometry-facing entries `CShadingContext::shade(CSurface *, ...)`, `displace()` and `displaceEstimate()` that this gotcha's neighbours mention are retired by spec 031.

## 7. Gotcha 35 (hiders own a shading context): the Geometry sentence

Replace "Geometry keeps calling `CShadingContext::shade(CSurface *, ...)`, `displace()` and `displaceEstimate()`: an opaque forwarding entry that calls the owning hider's `IShadingProducer`, which builds the point
source (`CSurfacePointSource`) and the `ShaderGlobals` batch and calls `shade(const ShaderGlobals &, IPointSource &)`; spec 031 retires the forwarding entry."
with "Geometry reaches shading and displacement through its own `IRayContext` (`shade`, `displace`, shading-state save/restore) that the hider implements over the context it owns; the forwarding entries
and `IShadingProducer` are gone since spec 031, and `CHider` builds the point source (`CSurfacePointSource`) and the `ShaderGlobals` batch itself and calls `shade(const ShaderGlobals &, IPointSource &)`."

## 8. New gotcha (number: next free at merge preparation)

> N. **Geometry's interfaces, the frame value and the one lens (spec 031).** Geometry's dice and intersect take Geometry-owned interfaces that the hiders implement: `IDicer` (`geometry/dicer.h`:
>    `drawObject`, `drawGrid`, `drawPoints`, `probe`, `inFrustum`, `rayContext()`) for dicing, `IRayContext` (`geometry/rayContext.h`: `threadIndex()`, the scratch arena, the varyings,
>    `shade`/`displace`, `saveShadingState`/`restoreShadingState`) for intersection and tessellation, and `IObjectSink` (`geometry/objectSink.h`) for the objects `instantiate()` creates; a new Geometry file includes
>    nothing from Hiders or Shading. The frame limits Geometry reads are a `CGeometryFrame` (`geometry/geometryFrame.h`: thread count, `maxGridSize`, `maxEyeSplits`, `flags`, `raytracingFlags`, projection, image plane,
>    pixel size and origin, `clipMin`, and `lens`) that the core builds in `beginFrame()` **after the hiders' constructors have run** (its `raytracingFlags` copies a static those constructors set) and
>    installs until `endFrame()`; with nothing installed it is all zero and `geometryFrame()` never returns null. The tessellation lock is Geometry's (`surface.cpp`, translation-unit local); delayed objects and
>    instances expand once through `CGeometryHost`. **One lens:** `openrender::LensModel::circleOfConfusion(z)` (`state/camera.h`) is the signed screen-space circle of confusion
>    `imagePlane * aperture * (1/f - 1/z)` (0 for a pinhole and for orthographic, where depth of field is off: the core sets `aperture` to 0). Reyes writes it into `vertices[9]` and scatters a vertex per axis by
>    `lens * dSampled * c` (`pixel->jdx = lens * dSampledx`); **only the scatter reads `vertices[9]` signed, every bound, comparison and maximum takes its magnitude**, and the bounds come from
>    `hiders/reyesLensBounds.h` (`Hiders_ReyesBucketBoundCoversCoC`; a `zMin` at or behind the eye plane is clamped to `kMinDepth`). The raytracer's `CFrameCamera::generateRay` keeps its own expressions
>    and `Camera_LensRayMatchesCoC` pins them to the function. Geometry's dicing heuristic `minCocPixels` reads `|c|` times the pixel size. Before spec 031 Reyes blurred `sqrt(2) f/(f + A)` times the raytracer's radius
>    (1.35 times for `DepthOfField 2 1 5`); only the five Reyes depth-of-field images changed (`Parity_dof` 40.45 to 9.92). The raytracer's remaining depth-of-field noise is the default Catmull-Rom pixel filter's
>    (`specs/031-geometry-hider-agnostic/lens-investigation.md`, GitHub #166), not the lens sampler.

## 9. Architecture section and Testing

- Under "Architecture", add after "Scene Interface services and the active context (spec 038)" a short section "### Geometry interfaces and the lens (spec 031)" repeating the first two sentences of the new gotcha.
- "Testing": add

```bash
ctest --test-dir build -L geometry_contract             # spec 031: Geometry's five contract programs (a dicer, a ray context, a host and a sink double; no hider, no Shading)
ctest --test-dir build -L camera                        # spec 028 and 031: the camera layer, the lens model, the signed circle of confusion, the lens ray
ctest --test-dir build -L hiders                        # spec 031: Hiders_ReyesBucketBoundCoversCoC
```

- The architecture-check paragraph, if it quotes numbers: allowlist 344 to 173 (DB-004 x 3, DB-010 x 8, DB-011 x 151, DB-025 x 11); `libRIBVector` residual list unchanged (`ribvector-core-residual.txt`).

## 10. "Active Technologies" and "Recent Changes"

Add to "Active Technologies":

> - C++20, CMake >=3.19, `-Werror`; no new dependencies: Geometry-owned `IDicer`, `IRayContext` and `IObjectSink`, a per-frame `CGeometryFrame`, Reyes on `FrameParams`, one `LensModel::circleOfConfusion`
>   (031-geometry-hider-agnostic)
> - Storage: N/A; memory only (031-geometry-hider-agnostic)

Add to "Recent Changes" (first entry):

> - 031-geometry-hider-agnostic: Geometry names neither the hiders nor Shading: `IRayContext`, `IDicer` and `IObjectSink` (implemented by the hiders and the core) replace the `CReyes *`/`CShadingContext *`
>   parameters of every Geometry virtual, the forwarding entries of `CShadingContext` and `IShadingProducer` are retired, a per-frame `CGeometryFrame` replaces Geometry's `CRenderer::` reads, and `libRIBVector` compiles the real
>   Geometry (`geometryDispatch.cpp` and its stubs are gone). Reyes reads `FrameParams` and rotates the camera through `motion::cameraRotationAt` (DB-009's rotation half). One thin lens, `LensModel::circleOfConfusion`, serves
>   Reyes and the raytracer (the bounds in `reyesLensBounds.h`; orthographic depth of field stays off): the only images that changed are the five Reyes depth-of-field renders, approved one by one (`Parity_dof` 40.45 to 9.92).
>   DB-006 and DB-009 Resolved; allowlist 344 to 173 (deletions only); metrics Geometry 48 to 2 references, Hiders 556 to 495, TOTAL 604 to 497; `kRendererServicesVersion` steps one MINOR at merge (2.3.0 after spec 032's 2.2.0). No tolerance
>   widened, no test disabled. The default Catmull-Rom pixel filter's effect on depth-of-field noise is filed as #166 (see spec 031).
