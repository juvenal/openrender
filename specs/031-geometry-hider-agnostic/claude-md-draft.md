# Proposed CLAUDE.md changes for spec 031 (draft; merge-prepared 2026-11-12 against master `c2e3cce3`; nothing applied)

`CLAUDE.md` is changed only by the maintainer (spec FR-018 and the project rules), so this file holds the proposed text and **no task edits `CLAUDE.md`**.
It is written against master's `CLAUDE.md` at `c2e3cce3` (specs 056, 032 and 030 merged): gotchas 36 (spec 056), 37 (spec 032) and 38 (spec 030) are taken, so **031's new gotcha is 39**
(re-check the number if another spec merges first). Every "Old" text below was compared verbatim with master's `CLAUDE.md` for this revision; gotchas 24, 25 and 28 (spec 032) and 35 and the
Hider-system paragraph (spec 030) are the ones master changed, and the replacements below are written over the master text. The "Spec-kit agent rules" section is not touched.
031's `kRendererServicesVersion` step is **2.3.0**, after spec 032's 2.2.0. The numbers are the measured end state of the merged tree (macOS and Linux, validation.md §1 "Merge preparation": allowlist 147, Hiders 489 and TOTAL 491 references).

## 1. Architecture, "Hider system" paragraph (spec 030's text kept)

In "`CHider` (`src/ri/hiders/hider.h`) is the base of every hider (spec 029). It owns its thread's `CShadingContext` (Shading, `final`, no virtuals), declares `renderingLoop()`, and is the context's tracer and
shading producer.", replace "and is the context's tracer and shading producer" with "and is the context's tracer and, for Geometry, its ray-side context (`IRayContext`, spec 031; `CReyes` also implements the
dicing interface `IDicer`)". Spec 030's registry paragraph and spec 032's last sentence stay as they are. After the "GI stores (spec 032)" and "Hider and integrator registries (spec 030)" sections add the short
section "### Geometry interfaces and the lens (spec 031)" with the first two sentences of gotcha 39.

## 2. Gotcha 13 (CSG region hook): one added sentence

Append: "Geometry's interface headers (`rayContext.h`, `dicer.h`, `objectSink.h`, `geometryFrame.h`) mention neither CSG nor Region (FR-019, grep-checked): the hook's generic names stay where
`reyes.cpp` reads them, and `CRegionPredicate`, `regionCoverage` and `CCSGSolid`'s entry points keep their names."

## 3. Gotcha 14 (CSG classification): the last sentence

Old: "Free-form (NURBS/subdivision/bicubic) crossings live in the renderer-only `geometry/surfaceCrossingDispatch.cpp`, with `vector/surfaceCrossingDispatchStub.cpp` for ribVector." (it is split over three lines
in `CLAUDE.md`: "...live in the / renderer-only `geometry/surfaceCrossingDispatch.cpp`, with / `vector/surfaceCrossingDispatchStub.cpp` for ribVector.")
New: "Free-form (NURBS/subdivision/bicubic) crossings live in `geometry/surfaceCrossings.cpp`, compiled into both `libRIBRender` and `libRIBVector`: the dispatch and stub pair is gone (spec 031)."

## 4. Gotcha 17 (Geometry never calls the renderer core): the second half

Old: from "so a solid captures its per-instance leaves and procedural expansions through the Geometry-owned `CGeometryHost` (`geometry/geometryHost.h`: `capture()`, `expand()`, `releaseAtFrameEnd()`), implemented
and registered by `CRendererContext`." to the end of the gotcha. New:

> so a solid captures its per-instance leaves and procedural expansions through the Geometry-owned `CGeometryHost` (`geometry/geometryHost.h`: `capture()`, `expand` for a delayed object or instance,
> `findCoordinateSystem`, `releaseAtFrameEnd()`/`frameEnded()`), implemented and registered by `CRendererContext`, and reports the objects it creates to an `IObjectSink` (`geometry/objectSink.h`, which the core
> implements). Never reach `CRenderer`/`CRendererContext` from Geometry (Principle VIII; the registered violations live in `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`, DB-004: three lines, all in
> `blobbyRepeller.cpp`). The instance state holds a reference to every captured object until `endFrameResources()` runs in `CRenderer::endFrame()` before `root->destroy()`: REYES consumes objects by refcount,
> while raytracing frees the hierarchy regardless of it.

## 5. Gotcha 19 (render-only `CSurface` overrides): replace the whole gotcha

> 19. **Geometry is one library for the renderer and for `libRIBVector` (spec 031).** The Geometry bodies that used to live in `render/geometryDispatch.cpp` (`CSurface::regionCoverage`,
>    `CCSGSolid::dice`, the `CSurface`/`CObject` virtuals) are in `src/ri/geometry` and compiled by both libraries; `geometryDispatch.cpp` and its stub no longer exist, and neither does any other
>    `vector/*Stub.cpp` (spec 032 deleted the irradiance one). A Geometry file includes no Hiders or Shading header and names no `CReyes`/`CShadingContext`: `Geometry_NoHiderShadingNames` scans the directory,
>    comments included, and `Geometry_RibVectorNoStubs` checks `libRIBVector`'s undefined symbols (no exemption).

## 6. Gotcha 26 (`FrameParams` and the motion sites): three edits

- After "A field is a copy of the core static of the same meaning and is never recomputed:" add (inside the sentence, before "the shutter scale") "(spec 031 added `dSampledx` and `dSampledy`, the samples per screen unit that
  the lens scatter and its bounds use, and removed the `cocFactor*` copies: the lens values are the `aperture`, `focaldistance`, `invFocaldistance`, `focallength` and `fstop` fields, read through `openrender::LensModel::fromFrame`)".
- Old: "Reyes's own camera-rotation SLERP and motion bounds are untouched (DB-009, spec 031)." New: "Reyes rotates the camera through `motion::cameraRotationAt` since spec 031 (the one function the raytrace side
  uses; `Motion_CameraSlerpEquivalence` pins it); Reyes's motion bounds are untouched, and DB-009 is Resolved."
- Add: "Reyes and the stochastic rasterizer read the frame (`frameParams()`, once per function or grid, never per sample) instead of `CRenderer::aperture`, `cameraHasRotation`, `relRotQ`, `relTrans`, `dSampledx/y` and the lens statics."

## 7. Gotcha 28 (Shading's boundary): the version sentence and the entries

- Old (master): "`kRendererServicesVersion` stayed 2.0.0 at spec 038, is 2.1.0 since spec 029 and 2.2.0 since spec 032 (a MINOR step that is breaking within 2.x: `getCache` answers `nullptr` on a kind mismatch;
  otherwise additions: `photonMapAccess.h`, `irradianceSampler.h`, `irradianceCalls.h`, `boundDraws()`)." Append: "It is 2.3.0 since spec 031 (a MINOR step that is breaking within 2.x: `CShadingContext`'s Geometry-facing
  `shade(CSurface *, ...)`, `displace()`, `displaceEstimate()` and `setProducer()` and `IShadingProducer` are removed, and `EShadingDim` moved to `common/shadingDim.h`)."
- The first sentence of the gotcha ("`CRendererServices` is version 2.2.0 (...)") becomes "version 2.3.0".

## 8. Gotcha 35 (hiders own a shading context)

Old: "Geometry keeps calling `CShadingContext::shade(CSurface *, ...)`, `displace()` and `displaceEstimate()`: an opaque forwarding entry that calls the owning hider's `IShadingProducer`, which builds the point source
(`CSurfacePointSource`) and the `ShaderGlobals` batch and calls `shade(const ShaderGlobals &, IPointSource &)`; spec 031 retires the forwarding entry." New: "Geometry reaches shading and displacement through its own
`IRayContext` (`shade`, `displace`, shading-state save/restore) that `CHider` implements over the context it owns; the forwarding entries and `IShadingProducer` are gone since spec 031, and the hider (`shadingProducer.cpp`,
`shadeSurface()`) builds the point source (`CSurfacePointSource`) and the `ShaderGlobals` batch itself and calls `shade(const ShaderGlobals &, IPointSource &)`." Also in the same gotcha, "the producer takes from
`classifyShading()`" becomes "the hider takes from `classifyShading()`". Spec 030's sentences on `contexts` stay.

## 9. Gotcha 38 (hiders and integrators, spec 030): one sentence

Append: "`CReyes::drawObject` is one declaration that overrides both `CHider::drawObject` (spec 030: the core submits objects through it) and `IDicer::drawObject` (spec 031: what Geometry's `dice()` calls); do not
declare a second one."

## 10. New gotcha (39)

> 39. **Geometry's interfaces, the frame value and the one lens (spec 031).** Geometry's dice and intersect take Geometry-owned interfaces that the hiders implement: `IDicer` (`geometry/dicer.h`:
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

## 11. Testing, Active Technologies, Recent Changes

- "Testing": add (the list already holds `-L gi_split`, `-L registry` and `-L volume` from the other specs)

```bash
ctest --test-dir build -L geometry_contract             # spec 031: Geometry's five contract programs (a dicer, a ray context, a host and a sink double; no hider, no Shading)
ctest --test-dir build -L camera                        # spec 028 and 031: the camera layer, the lens model, the signed circle of confusion, the lens ray
ctest --test-dir build -L hiders                        # spec 031: Hiders_ReyesBucketBoundCoversCoC
```

- The architecture-check paragraph, if it quotes numbers: the allowlist is 147 entries on the merged tree (344 on the common base; DB-004 x 3, DB-011 x 144); `ribvector-core-residual.txt` lists `rendererStatics.cpp` and the file-search trio only.
- "Active Technologies", add: "- C++20, CMake >=3.19, `-Werror`; no new dependencies: Geometry-owned `IDicer`, `IRayContext` and `IObjectSink`, a per-frame `CGeometryFrame`, Reyes on `FrameParams`, one
  `LensModel::circleOfConfusion` (031-geometry-hider-agnostic)" and "- Storage: N/A; memory only (031-geometry-hider-agnostic)".
- "Recent Changes", first entry:

> - 031-geometry-hider-agnostic: Geometry names neither the hiders nor Shading: `IRayContext`, `IDicer` and `IObjectSink` (implemented by the hiders and the core) replace the `CReyes *`/`CShadingContext *`
>   parameters of every Geometry virtual, the forwarding entries of `CShadingContext` and `IShadingProducer` are retired, a per-frame `CGeometryFrame` replaces Geometry's `CRenderer::` reads, and `libRIBVector` compiles the real
>   Geometry (`geometryDispatch.cpp` and every `vector/*Stub.cpp` are gone). Reyes reads `FrameParams` and rotates the camera through `motion::cameraRotationAt` (DB-009's rotation half). One thin lens, `LensModel::circleOfConfusion`, serves
>   Reyes and the raytracer (the bounds in `reyesLensBounds.h`; orthographic depth of field stays off): the only images that changed are the five Reyes depth-of-field renders, approved one by one (`Parity_dof` 40.45 to 9.92).
>   DB-006, DB-009, DB-017 and DB-025 Resolved (the last two with spec 032); the allowlist 344 to 147 on the merged tree; metrics Geometry 48 to 2 references, Hiders and TOTAL as archcheck reports on the merged tree
>   (489 and 491); `kRendererServicesVersion` 2.3.0 (after spec 032's 2.2.0). No tolerance widened, no test disabled. The default Catmull-Rom pixel filter's effect on depth-of-field noise is filed as #166.
