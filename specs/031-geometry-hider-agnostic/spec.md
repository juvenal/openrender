# Feature Specification: Geometry, Hider-Agnostic

**Feature Branch**: `031-geometry-hider-agnostic`
**Created**: 2026-11-06
**Status**: Draft
**Input**: Constitution Principle VIII remediation, layer L4, after specs 028 and 029 (both merged); GitHub #37. Geometry stops naming the hiders and the shading engine. Its dice and ray-side entry points take Geometry-owned interfaces (dicing, ray-side, procedural expansion) instead of `CReyes *` and `CShadingContext *`; Reyes reads the camera and the motion it needs from the shared camera and motion layer spec 028 built; the link-time `*Dispatch`/`*DispatchStub` substitution pairs that exist only because of the old coupling are deleted, and `ribVector` (the preview's and the data viewer's geometry library) builds without stubs. Resolves registry entry DB-006 (all of it: every edge is Geometry's) and, with the lens phase, DB-009; narrows DB-017 to the irradiance pair that spec 032 deletes; reduces DB-004 (#27) to the three `blobbyRepeller.cpp` lines and DB-011 by 11 lines. Every render is byte-identical through the interface and Reyes-rotation phases; the last phase, a physically based lens model shared by Reyes and the raytracer, may change depth-of-field images only, each approved by the maintainer.

## Clarifications

### Decisions recorded at the specify stage

- The three bodies of work in #37 are one spec, because they share one change: every geometry class's `dice`, `intersect`, `shade` and `splitToChildren` signature changes once, and the pairs can only be deleted when the last of those signatures is interface-based. (brief, plan)
- "No image changes" is a hard constraint (maintainer, 2026-11-06) for every phase except the lens phase (see the 2026-11-07 session).
- The shared work lives at the three boundaries the brief names (dicing, ray-side, procedural expansion). Their member lists are the plan stage's to design from the call-site inventory in the Measured starting point; this spec fixes what must be possible through them, not their shape.
- The part of DB-004 taken is the part dicing and the ray side need (frame and dicing parameters, locks and thread counts, and the coordinate-system and variable lookups that deleting the declarations pair forces). The rest of DB-004 stays registered and open. The share is measured below.

### Session 2026-11-07

- Q: How far does the Reyes half of DB-009 go under "no image changes"? -> A: The maintainer rejected "keep the formula" and chose to **unify the lens models**. The physically based thin-lens camera is the single optical reference for both Reyes and the raytracer, and one `LensModel` function drives both. This reverses the clarify stage's recommendation and adds scope: a last phase (the lens phase) in which depth-of-field images may change. The current raytracer's noisy depth of field is not the quality target, Reyes's smoother defocus is the quality to preserve, and the goal is photographic realism comparable to a real camera lens, not a match to either renderer's current output. The maintainer's six requirements are FR-022 to FR-027. Everything before the lens phase stays byte-identical, proven on both platforms against each platform's own baseline first, so a lens change cannot be mistaken for a refactoring error.
- Q: Who removes the fourth pair, `irradianceDispatch.cpp` and its stub? -> A: Spec 032. 031 deletes the three Geometry-side pairs and leaves the irradiance pair to 032, which the maintainer has decided will delete it (032's miss-path option A, in Shading). "`ribVector` builds without stubs" means no stubs for Geometry symbols. DB-017 stays Open in this spec, narrowed to that pair; #37's "four pairs" is met only by the two specs together. No task of 031 depends on 032's merge.
- Q: The `instantiate(..., CRiInterface *)` edge of DB-004? -> A: Replace it now. A Geometry-owned one-method `IObjectSink::addObject(CObject *)`, which `CRiInterface` derives from; callers pass `this` and do not change. DB-004 then keeps only the three `blobbyRepeller.cpp` lines. (The clarify stage found that with the edge left alone DB-004 would keep 12 lines, not 5: seven files include `rendererContext.h` only to see `CRiInterface::addObject`.)
- Q: Does 031 remove `geometry/surface.cpp -> dataviewer/debug.h` (DB-025)? -> A: Yes. 031 deletes the include and the two dead `DEBUG_HITS`/`DEBUG_TESSELATIONS` blocks and records a history entry in DB-025, but does not move DB-025 to Resolved: spec 032 deletes the other 11 lines, and whichever of the two merges second moves the entry, so no task depends on the other's merge.
- Delivered against #37: Geometry includes no Hider or Shading header (DB-006 Resolved); three of the four pairs are deleted and DB-017's 78 lines with them, the fourth (irradiance) goes with spec 032, so DB-017 is Resolved only by the two specs together; DB-009 is resolved in both halves (camera rotation, then the shared lens); DB-004 is reduced to 3 lines, not resolved; `ribVector` builds without stubs for Geometry symbols.
- Corrections from the clarify stage's verification: the `addObject` calls are 24 in 9 files, not 25 in 10 (the other hits are comments); `FrameParams` lacks the dicing constants and `cocFactorSamples`; the dicing constants belong in one shared Geometry-owned frame-limits value rather than duplicated on two interfaces (a plan-stage note); no member for spec 057 is added speculatively; the interface names `IDicer`/`IRayContext`/`IProceduralExpander` of #37 against FR-004's "extend `CGeometryHost`" are for the plan to settle.

### Plan-stage corrections (2026-11-07)

The maintainer approved the plan with every recommendation (P-Q1 to P-Q14, `plan.md` "Maintainer answers") and kept orthographic depth of field off. The spec is corrected accordingly; requirement numbers are unchanged and one requirement is appended (FR-031).

- US5 AS2, FR-010 and the Key Entities: `blobby.cpp`'s variable lookup calls Graphics State's `CDeclarations::find` directly (it is exactly what `CRenderer::retrieveVariable` returns); only the coordinate-system lookup goes through a Geometry-owned service (`CGeometryHost`). (P-Q4)
- FR-003: the tessellation lock is Geometry's own (it guards only Geometry's data); the ray-side interface does not carry it. (P-Q6)
- US7 (description, AS2): zbuffer has no depth of field and "stochastic" is the Reyes hider, so the defocus-agreement statement covers Reyes and raytrace. (P-Q9)
- FR-024 and Scope Boundaries: orthographic depth of field stays off in both hiders, as today (`renderer.cpp:447`); enabling it is out of scope, a possible follow-up. (P-Q10)
- SC-005, SC-006, SC-007: `hiders/photonMap.cpp` relies on `object.h -> renderer.h`, so it reads the camera matrices from `FrameParams` and its two DB-011 lines go too (P-Q3 (a)); spec 032 deletes the same two lines, and whichever spec merges second drops its duplicate edit. The allowlist goes from 344 to 173.
- FR-031 (new): the Reyes bucket bound (`reyes.cpp:616-636`), narrower than the scatter today, must cover the lens phase's circle of confusion, verified by a test or measurement; a possible link to #112 is noted, not claimed as a fix.
- Scope Boundaries: spec 032 does not edit `surface.cpp` (its FR-016); the earlier text said both specs touch it.

## User Scenarios & Testing *(mandatory)*

### Measured starting point (2026-11-06, master `e83d005d`)

Every number below was taken from the tree and from `tests/architecture/allowlist.txt` (344 lines), not copied from the plan.

- **DB-006, Geometry side: 30 allowlist lines, all Geometry's** (23 include, 7 link). 9 files include `hiders/reyes.h` (`curves.cpp`, `dlobject.cpp`, `implicitSurface.cpp`, `patches.cpp`, `points.cpp`, `polygons.cpp`, `subdivisionCreator.cpp`, `subdivisionLoop.cpp`, `surface.cpp`); 13 include the `core/shading.h` shim and 1 (`quadrics.h`) includes `core/shader.h`. The 7 link lines are `CShadingContext::shade` (3 files), `saveState`, `restoreState`, `CReyes::drawObject` and `CReyes::drawGrid`. (The registry text says Geometry includes Shading headers 15 times; the allowlist records 14.)
- **DB-017: 78 allowlist lines** (71 from Geometry files, 7 from `hiders/photon.cpp` x6 and `hiders/trace.cpp` x1, all bound to Geometry symbols that `render/geometryDispatch.cpp` defines). The four pairs: `geometryDispatch.cpp`/`geometryDispatchStub.cpp` (the virtual overrides `CObject::dice`, `CSurface::dice/intersect/shade/regionCoverage/checkRayGuard`, the curve, point and delayed-object bodies, `CObject::cluster`, `CCSGSolid::dice`), `surfaceCrossingDispatch.cpp`/stub (CSG crossing counts), `rendererDeclarationsDispatch.cpp`/stub (`CRenderer::findCoordinateSystem` for `object.cpp`, `CRenderer::notifyGlobalVariable`), and `hiders/irradianceDispatch.cpp`/stub (`CIrradianceCache::sample`; no allowlist line; Hiders' own and deleted by spec 032, not by this one).
- **DB-004: 52 allowlist lines** (28 include, 24 link; 48 `CRenderer::` references to 15 members in Geometry). By file: `surface.cpp` 14 link + 2 include, `object.cpp` 7 link + 2 include, `blobby.cpp` 1 link, `blobbyRepeller.cpp` 2 link; 2 `riInterface.h` includes (`csgSolid.cpp`, `subdivisionLoop.cpp`) and 9 `rendererContext.h` includes, 7 of which (`curves`, `delayed`, `patches`, `points`, `polygons`, `quadrics`, `subdivisionCreator`) exist only to see the complete `CRiInterface` type for `c->addObject(...)` (24 calls in 9 files, 14 in `quadrics.cpp`; callers: `rendererContext.cpp:488`, `:525`, `previewContext.cpp:139`, all passing `this`). The link lines in `surface.cpp` and `object.cpp` are dicing and ray-tessellation reads (`maxGridSize`, `maxEyeSplits`, `flags`, `projection`, `imagePlane`, `dPixeldx/y`, `pixelLeft/Top`, `cocFactorPixels`, `invFocaldistance`, `numThreads`, `tesselateMutex`, `inFrustrum`, `raytracingFlags`); `blobby.cpp` and `blobbyRepeller.cpp` read services (`retrieveVariable`; `locateFile`, `texturePath`).
- **Where Geometry actually uses the two context types.** Through `CShadingContext`: per-thread scratch memory (19 uses), `displace`/`displaceEstimate` and `currentShadingState->varying` (probe buffers), `shade`, `saveState`/`restoreState`, the thread index. Through `CReyes`: `drawObject`, `drawGrid`, `drawPoints`, and `shadingContext()`. Through `CRenderer::context`: the three `processDelayed*` calls, inside `geometryDispatch.cpp`. The `CRegionPredicate` hooks (`accepts`, `classifyBound`) and `countCrossings` take a `CShadingContext *` they pass along. Shading's side of the same coupling: `CShadingContext::shade/displace/displaceEstimate(CSurface *, ...)` and `IShadingProducer::shadeSurface(CSurface *, ...)` are the forwarding entry gotcha 35 says this spec retires.
- **DB-009, Reyes side.** Reyes keeps its own circle-of-confusion model (`CSampler::circleOfConfusion`: `reyes.cpp:1164`, `:1468-1469`, `:1887-1888`, reading `CRenderer::invFocaldistance`, `cocFactorSamples`) and its own camera-rotation SLERP (`reyes.cpp:1426-1432`, `:1599-1603`, `:1636-1642`; `stochastic.cpp:166-169`, `:220`; `stochasticQuad.h:497-506`, `:859-870`), reading `CRenderer::cameraHasRotation`, `relRotQ`, `relTrans`, `cameraRotationOnly`, `aperture`, `cocFactorScreen`: 11 DB-011 allowlist lines (7 in `reyes.cpp`, 4 in `stochastic.cpp`, verified). Spec 028 already left `openrender::LensModel` (with `cocFactorScreen()`) and `motion::cameraRotationAt`, pinned equal to Reyes's SLERP by `Motion_CameraSlerpEquivalence`. `FrameParams` carries `aperture`, `invFocaldistance`, `camera.relRotQ`, `relTrans`, `hasRotation`, `rotationOnly`; it lacks `cocFactorSamples` (computed at `renderer.cpp:456`, before `beginFrameParams` at `:805`) and every dicing constant (`maxGridSize`, `maxEyeSplits`, `numThreads`, `flags`, `dPixeldx/dy`, `pixelLeft/Top`, `cocFactorPixels`, the frustum test).
- **The two lens constructions (reading to verify at plan, not a settled fact).** For a perspective camera the raytracer's ray (`state/camera.cpp:94-118`) gives a screen blur of `imagePlane * aperture * |1/z - 1/f|`; Reyes uses the same expression times an extra `f / (f + aperture)` (`renderer.cpp:455`, `LensModel::cocFactorScreen`, `camera.cpp:35-39`). Spec 008 documents the difference as D9, a "non-closable residual", and `sampler.h:90-94` repeats it; both need correcting to whatever is finally true. Scenes with a lens: `examples/rib/camera-dof-{reyes,raytrace}.rib` (and `camera-dof.rib`, to check), `examples/rib/tests/camera-dof-{reyes,raytrace}.rib`, `camera-motion-small+dof-{reyes,raytrace}.rib`, `examples/rib/tests/parity/dof-{reyes,raytrace}.rib`, `dof-motion-{reyes,raytrace}.rib`, with their references in `examples/rib/tests/references/` and `tests/convergence/reference/`, and `examples/rib/tests/sampling-frames.rib` (to check). The plan finds any other scene by searching for `DepthOfField`.
- **`ribVector` today** compiles 9 Core files pinned in `tests/architecture/ribvector-core-residual.txt`, including the four stubs.

### User Story 1 - Renders do not change (Priority: P1)

A maintainer lands a refactor that rewrites the signature of nearly every geometry class and moves Reyes's camera reads, and every image the renderer writes is the same bytes as before, until the last phase, where only depth-of-field images may change.

**Why this priority**: every other story is worth nothing if a pixel moves. The refactor touches dicing decisions (grid sizes depend on float expression order), tessellation caches, region coverage and the motion expressions that spec 028 pinned.

**Independent Test**: build before and after, render the visual, parity and default determinism suites, compare bytes against each platform's own pre-change baseline; run `Motion_Replay_*`. The comparison is made at the end of the phase before the lens phase, and again after it for everything outside the depth-of-field scenes.

**Acceptance Scenarios**:

1. **Given** the full visual suite, **When** run at the end of the last phase before the lens phase, **Then** every image is byte-identical to the one the pre-change tree writes, on macOS and on Linux (GCC 13, `-Werror`), each against its own platform's baseline. After the lens phase, every image outside the depth-of-field scenes (US7) is still byte-identical.
2. **Given** the 12,943-call motion replay, **When** run, **Then** it reports no difference and no site's expression, precision or grouping changed.
3. **Given** the default determinism set and the cross-hider parity tests, **When** run, **Then** they pass unchanged, with no widened tolerance and no re-recorded reference.

---

### User Story 2 - Geometry names neither the hiders nor Shading (Priority: P1)

A contributor reads any file under `src/ri/geometry/` and finds no Hider or Shading header and no call into either; the entry points that used to take `CReyes *` and `CShadingContext *` take interfaces Geometry owns.

**Why this priority**: it is the acceptance statement of #37 and removes the last downward cycle between Geometry and the domains above it.

**Independent Test**: `ctest -L architecture` with the DB-006 lines deleted; a compile of `src/ri/geometry/` with the Hiders and Shading directories off its include path.

**Acceptance Scenarios**:

1. **Given** the architecture check, **When** the 30 DB-006 lines are deleted, **Then** `Architecture_DomainBoundaries` passes and DB-006 moves to Resolved.
2. **Given** the Reyes, raytrace, zbuffer, stochastic and photon hiders, **When** each dices or intersects geometry, **Then** it does so by implementing the Geometry-owned interfaces; no hider is named in Geometry.
3. **Given** a procedural, an instance or a delayed solid met during dicing or during ray traversal, **When** it expands, **Then** it expands through the procedural-expansion interface with the same serialization, ordering and results as today.

---

### User Story 3 - The preview library builds with no stubs (Priority: P1)

`ribVector` (orender-wire's and the data viewer's geometry library) compiles the same geometry classes the renderer does, with no stub file that fakes a body with an error, so a wrong assumption about what the preview reaches fails at compile time instead of at run time.

**Why this priority**: it is the part of V14 that makes the substitution a lie today; the stubs say "never reached" and enforce it with an assertion.

**Independent Test**: `SceneInterface_RibVectorResidual`, `Preview_*`, and the orender-wire tests; the CMake list for `ribVector_obj` names no `vector/*Stub.cpp` that stands in for a Geometry symbol (the irradiance stub is spec 032's and remains until it lands).

**Acceptance Scenarios**:

1. **Given** the CMake definition of `ribVector_obj`, **When** read, **Then** `geometryDispatchStub.cpp`, `surfaceCrossingDispatchStub.cpp` and `rendererDeclarationsDispatchStub.cpp` are gone with their real counterparts, and the geometry classes they stood for are ordinary Geometry files compiled for both libraries.
2. **Given** `ribvector-core-residual.txt`, **When** the build is checked, **Then** the lines for the three deleted stubs, and for `render/rendererDeclarations.cpp` once `blobby.cpp`'s variable lookup moves behind a Geometry-owned service, are removed and no line was added; the irradiance stub's line stays, named as spec 032's.
3. **Given** orender-wire and the data viewer, **When** their tests run, **Then** they pass with the wire output and viewer output unchanged.

---

### User Story 4 - Reyes reads its camera rotation and motion from the shared layer (Priority: P2)

Reyes takes the camera rotation and translation and the shutter from the per-frame values and the motion evaluator the raytracer already uses, not from `CRenderer` statics and its own SLERP copies.

**Why this priority**: it is the camera-rotation half of DB-009 and what DB-011 needs to shrink. It is bit-preserving and finishes before the lens phase.

**Independent Test**: `Motion_CameraSlerpEquivalence` and `Motion_Replay_*`, the motion scenes byte-compared, the DB-011 lines for the camera statics deleted.

**Acceptance Scenarios**:

1. **Given** a rotating-camera motion-blur scene, **When** rendered, **Then** the image is byte-identical and Reyes's rotation uses `motion::cameraRotationAt` over `FrameParams` (no second SLERP expression).
2. **Given** the DB-011 allowlist, **When** Reyes and the stochastic hider stop reading the camera statics, **Then** their lines are deleted (up to 11, counting those the lens phase removes).

---

### User Story 5 - Dicing and ray tessellation read parameters from the interface, not the renderer core (Priority: P2)

Geometry reads grid-size limits, split limits, the motion-blur and focal-blur flags, the projection and screen-to-pixel constants, the thread count and the tessellation lock from the Geometry-owned dicing and ray-side interfaces (or the per-frame values), never from `CRenderer`.

**Why this priority**: it is the part of DB-004 that the interface change makes nearly free and that "builds without stubs" needs; it is P2 because it follows the signature change.

**Independent Test**: the DB-004 link lines for `object.cpp` and `surface.cpp` (21) and the 9 `rendererContext.h` and 2 `riInterface.h` include lines are deleted and `Architecture_DomainBoundaries` passes; the Geometry `CRenderer::` metric in `metrics-baseline.txt` falls.

**Acceptance Scenarios**:

1. **Given** `surface.cpp` and `object.cpp`, **When** read, **Then** no `CRenderer::` member remains in either file.
2. **Given** `blobby.cpp`'s variable lookups and `object.cpp`'s maximum-displacement coordinate-system lookup, **When** read, **Then** the variable lookups call Graphics State's `CDeclarations::find` directly and the coordinate-system lookup goes through a Geometry-owned service (`CGeometryHost`), neither through `CRenderer::retrieveVariable`/`findCoordinateSystem`.
3. **Given** the registry, **When** read after the change, **Then** DB-004 lists only what remains (file and texture-path lookup in `blobbyRepeller.cpp`, 3 lines) with the measured counts, and no Geometry file includes `rendererContext.h` or `riInterface.h`: `instantiate` takes the Geometry-owned object sink.

---

### User Story 6 - The registry, the guard and the next spec reflect what moved (Priority: P3)

DB-006 is Resolved with spec 031 and its date, and DB-009 with the lens phase; DB-017 (narrowed to the irradiance pair), DB-004, DB-011 and DB-025 (one line) carry history entries with measured counts; the interfaces leave Geometry open for spec 057's area sampling without carrying any of it.

**Why this priority**: the allowlist is shrink-only and the registry is the maintainer's source of truth; 057 must find the surface-sampling entry points on the new interfaces.

**Independent Test**: `Architecture_DomainBoundaries`, the architecture metric checks, and a review of the registry diff against `allowlist.txt`.

**Acceptance Scenarios**:

1. **Given** the allowlist, **When** the change is complete, **Then** it only shrank and every deleted line belongs to DB-006, DB-017, DB-004, DB-011 or DB-025 (one line).
2. **Given** spec 057's description (#152), **When** its author looks for where area sampling attaches, **Then** the ray-side interface can carry a surface-sampling request without Geometry gaining a Hider or Shading dependency; nothing of 057 is implemented here.

---

### User Story 7 - One physically based lens for Reyes and the raytracer (Priority: P2, last phase)

A maintainer renders the same depth-of-field scene under the Reyes hider (`reyes`, also reached as `stochastic`) and raytrace and gets the same defocus geometry, because one `LensModel` function is the optical reference for both (zbuffer has no depth of field, as it has no motion blur, and stays sharp), and the result looks like a photograph taken through a real lens: Reyes's smooth defocus is kept, and the raytracer's isolated bright samples and speckled bokeh are investigated and reduced.

**Why this priority**: it is the depth-of-field half of DB-009 and the maintainer's decision of 2026-11-07. It is last, so every earlier change is proven byte-identical on both platforms against each platform's own baseline first and a lens change cannot be confused with a refactoring mistake.

**Independent Test**: the new analytic circle-of-confusion tests; the depth-of-field scenes re-rendered and inspected image by image; the byte comparison of everything else; `-L '^determinism$'` and the sweep.

**Acceptance Scenarios**:

1. **Given** apertures, focus distances and object distances across the range the scenes use, **When** the circle-of-confusion diameter is computed by the shared lens function, **Then** it equals the analytic thin-lens value for perspective and orthographic cameras within a stated float tolerance, and both Reyes and the raytracer take it from that function.
2. **Given** a depth-of-field scene, **When** rendered under Reyes and raytrace, **Then** the defocus geometry (blur diameter against depth, foreground and background) agrees across the two hiders within a measured noise bound, and the result is no noisier than today's Reyes defocus.
3. **Given** the raytracer's depth-of-field output, **When** investigated, **Then** the cause of isolated bright samples, speckling and under-reconstructed bokeh is identified and reported, and what is changed (aperture sampling, filtering) does not change the physical blur diameter.
4. **Given** a scene with a bright out-of-focus highlight or a defocused foreground object over a sharp background, **When** rendered, **Then** foreground/background occlusion and highlight behaviour stay physically meaningful.
5. **Given** the change, **When** the spec 050 path tracer is read against it, **Then** the lens function and the aperture sampling it uses are reusable by it (no hider-specific state).
6. **Given** every scene outside the depth-of-field set, **When** rendered after the phase, **Then** it is byte-identical to the pre-change tree; the depth-of-field references that change are each inspected and approved by the maintainer, with no threshold widened and the noise measured by the visual-test noise method.

---

### Edge Cases

- **Meshes build their children lazily on first use** (`create`/`materialize`) and need a per-thread scratch arena from whoever is calling; with no hider type in the signature that arena arrives through the interface, and a call from the preview, which has no renderer pipeline, must be impossible to reach without one.
- **Region predicates** (`CRegionPredicate::accepts`/`classifyBound`) accept a context that "may be NULL"; the contract survives the change and the CSG classifier works with the same inputs.
- **Region coverage is computed from undisplaced `P` before displacement** (gotcha 16); the order of the dice stages does not change.
- **Displacement-driven tessellation under raytracing** keeps per-thread caches indexed by the thread number and one global lock around list maintenance; neither the lock nor the ordering may change what a ray sees (`tesselateMutex`).
- **A procedural expanded from a ray thread and one expanded from a dicing thread** must serialize on the same lock as today (`delayedMutex` semantics) and keep the frame reference that holds a DynamicLoad module (gotcha 27).
- **Float expression order in `estimateDicing`, `probe` and the cull test** decides grid sizes; they move only by relocating their inputs.
- **The photon hider and `trace.cpp` call geometry too**; they are consumers of the same ray-side interface, not special cases.
- **A preview path that constructs geometry and never calls dice** must still link every vtable without a stub.
- **The lens phase and determinism.** The lens slot belongs to spec 039's `CSampler::cameraSample`; `-L '^determinism$'` and the sweep must hold, `Motion_Replay_*` must not move, and a change that proves invasive, or that needs a sampler-stream or `PixelSamples` default change, stops and goes to the maintainer.
- **`CropWindow`**: `pixelLeft`/`pixelTop` enter dicing today; they keep entering it unchanged (#72 is not this spec's).

## Requirements *(mandatory)*

### Functional Requirements

**Geometry-owned interfaces**

- **FR-001**: No file under `src/ri/geometry/` MUST include a Hiders header or a Shading header (directly or through the `core/shading.h`/`core/shader.h` shims), name `CReyes`, `CShadingContext` or any hider type, or call into them. The 30 DB-006 allowlist lines are deleted.
- **FR-002**: Geometry MUST own a dicing interface through which a surface's `dice` (and `splitToChildren`) learns what it needs: the limits (grid size, eye splits), the flags (motion blur, focal blur), the projection and pixel constants, the frustum test, the lens values for the circle of confusion; draws objects, grids and points back to the caller; probes a surface (the `displaceEstimate` pattern, returning the probed varying data); and allocates per-thread scratch. Every hider that dices implements it (Reyes and the hiders that derive from it). The limits and constants dicing and ray tessellation share (grid size, eye splits, thread count, the pixel constants) live in one Geometry-owned frame-limits value that both interfaces expose, not duplicated on each; the plan designs it.
- **FR-003**: Geometry MUST own a ray-side interface through which `intersect`, `shade`, `checkRayGuard`, `countCrossings`, `cluster`, `setChildren`, `materialize`, the CSG region hooks and the tessellation patch reach what they need: the thread index and thread count, per-thread scratch, displacement-and-shade of a point set, save and restore of the shading state, and the delivery of ray hits for shading. Every hider that traces implements it. The tessellation lock is Geometry's own (it guards only Geometry's tessellation state) and keeps the scopes and ordering of today's `CRenderer::tesselateMutex`; it is not a member of the interface.
- **FR-004**: Geometry MUST own a procedural-expansion interface through which delayed objects and delayed instances expand during dicing or ray traversal, replacing the three `CRenderer::context->processDelayed*` calls with the same serialization, ordering and frame-held module lifetime (extending the existing `CGeometryHost`, not duplicating it, unless the plan shows a reason; whether the interface is also named `IProceduralExpander` as in #37, and likewise `IDicer` and `IRayContext`, is for the plan).
- **FR-005**: The shading-dimension vocabulary Geometry's calls name (`EShadingDim` and its values) MUST come from a header outside Shading (Geometry-owned or Infra); `VARIABLE_*`/`PARAMETER_*` already live in `common/rendererc.h`.
- **FR-006**: `CShadingContext`'s Geometry-facing forwarding entries (`shade(CSurface *, ...)`, `displace`, `displaceEstimate`) and the producer hook behind them MUST be retired; Geometry calls the ray-side or dicing interface, which the owning hider answers by building the point source and `ShaderGlobals` as `IShadingProducer` does today (gotcha 35).
- **FR-007**: The interfaces MUST be abstract, live in the Geometry directory, carry no Shading or Hider type in their signatures, and MUST NOT be implemented by Geometry. Passing the interface by reference makes a call without an implementer impossible to write.
- **FR-028**: `CObject::instantiate` MUST take a Geometry-owned object sink, `IObjectSink::addObject(CObject *)`, instead of `CRiInterface *`; `CRiInterface` derives from it, so `CRendererContext`, the preview and every caller pass `this` unchanged and the 24 `addObject` calls in 9 files keep their text. No Geometry file includes `riInterface.h` or `rendererContext.h`.

**Dispatch/Stub pairs**

- **FR-008**: The methods defined today in `render/geometryDispatch.cpp` (the `CObject`/`CSurface`/curve/point/delayed-object/CSG bodies, `cluster`, `checkRayGuard`, `regionCoverage`) MUST live in Geometry files compiled identically for the renderer and for `ribVector`; `render/geometryDispatch.cpp` and `vector/geometryDispatchStub.cpp` are deleted.
- **FR-009**: `geometry/surfaceCrossingDispatch.cpp` and its stub MUST be replaced by one real implementation compiled for both libraries; the CSG units compile in `ribVector` if the build needs them (they need nothing but the ray-side interface).
- **FR-010**: `render/rendererDeclarationsDispatch.cpp` and its stub MUST be deleted; `findCoordinateSystem` (for `object.cpp`) reaches the renderer core's answer through a Geometry-owned service (`CGeometryHost`), the variable lookup (for `blobby.cpp`) calls Graphics State's `CDeclarations::find` directly, and `notifyGlobalVariable` (Shading's per-thread state update) has no reason to be compiled into `ribVector`.
- **FR-011**: `ribVector` MUST build with no stub that stands in for a Geometry symbol, and the residual list `tests/architecture/ribvector-core-residual.txt` MUST lose the lines of every stub and Core file the change makes unnecessary, and gain none. (The irradiance stub and its line stay, spec 032's to delete; "without stubs" means without stubs for Geometry symbols.)

**Reyes on the shared layer**

- **FR-012**: Reyes and the hiders derived from it MUST read the camera rotation, the camera translation and the "camera has rotation / rotation only" flags from the per-frame `FrameParams` and compute the rotation through `motion::cameraRotationAt`, not through `CRenderer` statics or a second SLERP expression, producing the same bits (`Motion_CameraSlerpEquivalence` stays green). This is finished and proven byte-identical before the lens phase.
- **FR-013**: Until the lens phase, Reyes MUST read its lens values from `FrameParams`/`LensModel` with no `CRenderer::aperture`, `invFocaldistance`, `cocFactorSamples` or `cocFactorScreen` read and the same result. `FrameParams` gains `cocFactorSamples` (and the other fields Reyes needs) as stored copies of the core's values, never recomputed (gotcha 26), unless the lens phase replaces them.
- **FR-014**: Every motion site keeps its own expression, precision and grouping (gotcha 26): no site is rewritten to call a different `atTime*` form; the `Motion_Replay_*` recordings do not change and are not re-recorded.

**The lens phase (last phase; the maintainer's decision of 2026-11-07)**

- **FR-022**: The physically based thin-lens camera model MUST be the single optical reference for Reyes and the raytracer: one `LensModel` function gives the circle of confusion and drives both; neither keeps a private formula. The plan verifies the reading that the two differ by a factor `f / (f + aperture)` (Measured starting point) and corrects spec 008's D9 and `sampler.h:90-94` to what is finally true.
- **FR-023**: The circle-of-confusion diameter MUST be validated analytically, by new tests, across apertures, focus distances and object distances.
- **FR-024**: Reyes and the raytracer MUST produce consistent defocus geometry, including perspective and orthographic camera behaviour. Orthographic depth of field stays off in both hiders, as today (`renderer.cpp:447` forces the aperture to 0), so the orthographic behaviour both must agree on is no defocus; enabling an orthographic lens is out of scope (2026-11-07).
- **FR-025**: The raytracer's isolated bright samples, speckling and insufficiently reconstructed bokeh MUST be investigated and the cause reported; aperture sampling and filtering MUST be improved without artificially changing the physical blur diameter.
- **FR-026**: Physically meaningful foreground/background occlusion and highlight behaviour MUST be preserved, and Reyes's smooth defocus is the quality to keep: the raytracer's current noisy output is not the target, and the goal is photographic realism comparable to a real camera lens, not a match to either renderer's current output.
- **FR-027**: The implementation MUST be suitable for the future path tracer of spec 050 (no hider-specific state in the lens function or the aperture sampling).
- **FR-029**: The lens phase MUST come after every interface change has been proven byte-identical on macOS and on Linux against each platform's own baseline. Only depth-of-field scenes may then change (`dof-*`, `camera-dof-*`, `camera-motion-small+dof-*`, `dof-motion-*`; the plan finds the rest, and checks `sampling-frames.rib` and `camera-dof.rib`). Each changed reference is re-rendered, inspected and approved by the maintainer image by image, with no threshold widened and the noise measured by the visual-test noise method; the new analytic tests wait for the maintainer's approval at the test-approval pause.
- **FR-030**: The lens slot is spec 039's `CSampler::cameraSample`; the lens phase MUST keep `-L '^determinism$'` and the determinism sweep green and `Motion_Replay_*` unmoved. Anything that proves invasive, or that needs a sampler-stream or `PixelSamples` default change, stops and is put to the maintainer.
- **FR-031**: In the lens phase, the Reyes bucket-assignment bound of an object (`reyes.cpp:616-636`), which today expands by less than the circle of confusion the rasterizer scatters, MUST cover the unified circle of confusion on each sample axis, consistently with the grid-level bounds, verified by a test or a measurement named in the plan. A possible link to #112 (Reyes silhouette dropout) is reported without claiming a fix.

**Renderer-core reads from Geometry**

- **FR-015**: Geometry MUST NOT reference `CRenderer` or `CRendererContext` for any value dicing or ray-side tessellation uses (the 21 `surface.cpp` and `object.cpp` link lines and every `renderer.h`/`rendererContext.h`/`riInterface.h` include that has no remaining use), and `CObject::raytraced()` MUST NOT read `CRenderer::raytracingFlags`.
- **FR-016**: What stays registered in DB-004 MUST be named and counted: `blobbyRepeller.cpp`'s file search and texture-path reads (`renderer.h` include, `locateFile`, `texturePath`: 3 lines), and nothing else.

**Registry, guard, scope discipline**

- **FR-017**: The allowlist lines of every resolved or reduced entry MUST be deleted; `Architecture_DomainBoundaries` passes; DB-006 moves to Resolved with "spec 031" and the date, and DB-009 with the lens phase's date once its depth-of-field half is delivered; DB-017 stays Open, narrowed to the irradiance pair (its allowlist lines are all gone); DB-004, DB-011 and DB-025 carry history entries with measured counts, DB-025 staying Open for spec 032's 11 lines and moved by whichever of the two specs merges second (the Geometry and Hiders `CRenderer::` metric baselines lowered with the stated reason). No line is added to the allowlist. `surface.cpp -> dataviewer/debug.h` (DB-025) and its two dead `DEBUG_*` blocks are removed.
- **FR-018**: Nothing in this spec's requirements, tasks or success criteria MAY depend on the merge (no post-merge step, no merge-commit reference). Issues close through a `Closes #NN` line in the merge commit, which the maintainer writes.
- **FR-019**: The CSG hooks (`CRegionPredicate`, `regionCoverage`, `CCSGSolid`'s generic entry points) keep generic names; the interfaces never name CSG (gotcha 13). CSG classification still bypasses ray filters (gotcha 14).
- **FR-020**: No tolerance is widened, no test is disabled and no reference is re-recorded to get a green result; a red `Motion_Replay_*` means a site's expression moved.
- **FR-021**: The interfaces MUST leave room for surface sampling by a later spec (057) but implement none of it; no member is added for it speculatively. 030's registry, 032's cache split, 056's volume shaders and every Bug Tracker issue listed under Scope Boundaries stay out.

### Key Entities

- **Dicing interface** (Geometry-owned; implemented by Reyes and the hiders derived from it): the frame limits and flags dicing reads, the probe service, the object/grid/point sinks, the per-thread scratch.
- **Ray-side interface** (Geometry-owned; implemented by every hider that traces): thread identity, scratch, point-set displacement and shading delivery, shading-state save and restore, the tessellation lock.
- **Procedural-expansion service** (Geometry-owned; implemented by the renderer core): delayed objects, delayed instances, the serialization and the module's frame lifetime.
- **Coordinate-system and variable lookups**: the coordinate-system lookup `object.cpp` makes is a Geometry-owned service (`CGeometryHost`); the variable lookup `blobby.cpp` makes calls Graphics State's `CDeclarations::find` directly.
- **Object sink** (Geometry-owned, `IObjectSink`): the one method `instantiate` reports created objects through; `CRiInterface` derives from it.
- **Shared per-frame values** (Graphics State, from spec 028): `FrameParams`, `LensModel`, `motion::cameraRotationAt`; the lens phase makes `LensModel` the single circle-of-confusion function.
- **Registry entries and allowlist lines**: DB-004, DB-006, DB-009, DB-011, DB-017.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Through the end of the phase before the lens phase, every image the visual, parity and default determinism suites write is byte-identical to the pre-change tree's, on macOS and on Linux (GCC 13, `-Werror`), each against its own platform's baseline; no tolerance widened, no reference re-recorded. After the lens phase, every image outside the depth-of-field scenes is still byte-identical.
- **SC-002**: `Motion_Replay_*` (12,943 recorded calls) and `Motion_CameraSlerpEquivalence` pass unchanged, before and after the lens phase.
- **SC-003**: The architecture check reports zero Geometry-to-Hiders and Geometry-to-Shading include or link edges: the 30 DB-006 lines are deleted and DB-006 is Resolved.
- **SC-004**: `ribVector` builds with none of `geometryDispatchStub.cpp`, `surfaceCrossingDispatchStub.cpp`, `rendererDeclarationsDispatchStub.cpp`; the 78 DB-017 lines are deleted; `ribvector-core-residual.txt` shrinks by at least those three stubs plus `render/rendererDeclarations.cpp` (4 of 9 lines) and keeps the irradiance stub's line for spec 032; `Preview_*`, `SceneInterface_RibVectorResidual` and the orender-wire tests pass with unchanged output.
- **SC-005**: DB-004 falls from 52 allowlist lines to 3 (the `blobbyRepeller.cpp` lines); the Geometry `CRenderer::` metric falls from 48 references to 2 (`locateFile`, `texturePath`) and the baseline is lowered with that reason. (Confirmed at plan by static reading, with `photonMap.cpp` reading `FrameParams` so that `object.h -> renderer.h` can go; each phase re-measures.)
- **SC-006**: Up to 11 DB-011 lines (the camera and lens statics Reyes and the stochastic hider read) are deleted, plus the two `photonMap.cpp -> CRenderer::fromWorld/toWorld` lines (2026-11-07), and the Hiders `CRenderer::` metric falls with them.
- **SC-007**: The allowlist falls from 344 lines to 173, and only shrinks. Deleted: DB-006 30, DB-017 78, DB-004 49, DB-011 13 (11 + the 2 `photonMap.cpp` lines), DB-025 1, total 171 (30 + 78 + 49 + 13 + 1); 344 - 171 = 173, from `e83d005d`. If fewer lines go, the target is correspondingly higher and the plan records the measured figure; a line another spec of the round deletes first (the 2 `photonMap.cpp` lines, also spec 032's) is counted once.
- **SC-008**: `ctest -L architecture`, `-L visual`, `-L shading_parity`, `-L state`, `-L plugin_abi`, `-L '^determinism$'` (and the determinism sweep before merge) and the preview tests pass with no new failure; `Motion_*` and `test_disk_sampling` are unchanged; the depth-of-field scenes change only as FR-029 allows.
- **SC-009**: The analytic circle-of-confusion tests (FR-023) pass for perspective and orthographic cameras across the stated ranges of aperture, focus distance and object distance, and both Reyes and the raytracer take the circle of confusion from the one `LensModel` function.
- **SC-010**: Each changed depth-of-field reference is approved by the maintainer image by image; the noise of the raytracer's depth of field is measured before and after by the visual-test noise method, with no threshold widened, and the investigation's finding on isolated samples, speckling and bokeh reconstruction is reported.
- **SC-011**: No clarification marker remains in this spec, and no task depends on the merge of 030, 032 or 056 or on 031's own.

## Scope Boundaries

**In scope**: the three Geometry-owned interfaces and the object sink, and their hider implementations; the Geometry method bodies in `geometryDispatch.cpp` moved into Geometry; the `surfaceCrossingDispatch` and `rendererDeclarationsDispatch` pairs; the Reyes/stochastic camera reads and the lens phase (one physically based `LensModel` for Reyes and the raytracer, the analytic tests, the raytracer's depth-of-field quality investigation, and the depth-of-field reference changes); the DB-004 reads dicing and ray tessellation make; the registry, allowlist, residual list and baseline updates.

**Out of scope, with the spec that owns it**:

- **030** (hider and integrator registry, `hcshader` retirement, the `strcmp` chains, capability negotiation): no hider is registered here, and nothing in this spec edits `renderer.cpp:beginFrame()`'s hider selection.
- **032** (GI cache split): the irradiance pair and `CIrradianceCache` belong to it (its miss-path option A deletes the pair); 031 removes only the `surface.cpp -> debug.h` line of DB-025; 032 does not edit `surface.cpp` (its FR-016). Both delete the two `photonMap.cpp` DB-011 lines (032 in its photon-map split); whichever merges second drops its duplicate edit.
- **056** (volume shaders, touches `csgSolid.cpp`): conflicts only in `csgSolid.cpp`; nothing here adds volume behaviour.
- **057** (area sampling on the interfaces this spec leaves): see FR-021.
- **Orthographic depth of field**: stays off in both hiders (FR-024); enabling it is a possible follow-up, not this spec.
- **DB-004 remainder**: the file-search and texture-path services for `blobbyRepeller.cpp` (3 lines).
- **050** (path tracer): only the requirement that the lens function be reusable by it (FR-027); nothing of 050 is built.
- **039's sampler streams and `PixelSamples` defaults**: not changed here; a lens-phase need to change either is put to the maintainer.
- **The camera and motion expressions themselves**: spec 028's, except the lens expression the lens phase replaces. A bug found in a motion expression is filed, not fixed.

**Bug Tracker issues that overlap Geometry or Reyes, and are not absorbed** (a fix agent is moving master under them; if one lands first, the rebase takes its change as is): #62 (degenerate micropolygons, reyes/zbuffer `checkPixel`), #72 (Reyes crop dicing), #70 (raytrace crop corners), #106 (camera motion on moving objects, `addObject`), #107 (`time` variable scale), #108 (`CBilinearPatch` dPdtime), #110 (patch-grid dPdtime out of bounds), #112 (Reyes silhouette dropout), #117/#118 (hit modes), #119 (implicit hits shaded 0-D; the issue says "spec 031 owns Geometry", but it is a bug fix), #127 (Reyes grid data race), #128 (CPatch stats race), #132 (DLO shades 2D without u/v), #138 (trace hash sentinel cast), #144 (implicit surfaces in camera space), #149 (zbuffer sphere pole), #150 (#57 follow-ups), #17 (CSG Debug assertion), #135 (arena and stats injection). Where a signature this spec rewrites sits on one of those lines, the change is mechanical and the behaviour stays as it is.

## Assumptions

- Before the lens phase, dicing and ray tessellation read the same camera and sampling constants they read today; relocating them does not change a value.
- The existing `CGeometryHost` (spec 021) can carry the procedural-expansion service; the plan decides whether it extends or wraps it.
- `CFrameCamera`, `FrameParams`, `LensModel` and `motion.h` from spec 028 are sufficient inputs for Reyes's rotation and lens reads; adding fields to `FrameParams` is allowed (`cocFactorSamples` as a stored copy, the frame limits), changing existing ones is not; the lens phase may replace the lens fields.
- `libRIBVector` compiling more of Geometry (`surface.cpp`, `dlobject.cpp`, `implicitSurface.cpp`, the CSG units) does not grow `ribVector`'s link dependencies, since the interfaces are abstract; the clarify stage found this plausible by reading the includes, and the plan confirms it with an `nm -u` check on a build the maintainer approves.
- Linux validation follows the per-spec remote tree rules (ssh and remote `cpujob -j2`); builds and tests here go through `~/.claude/bin/cpujob <label> <command>` at `-j4` at most, determinism sweeps at `-j2`.
- The raytracer's depth-of-field noise comes from a cause the lens phase's investigation finds; the outcome is unknown, which is why anything invasive stops and asks the maintainer (FR-030).
- `CLAUDE.md` gotchas 13, 14, 17, 19, 26 and 35 describe structure this spec changes; the maintainer updates them at the end of the spec, not during it.

## Notes for the plan

No question is open. Points the plan decides: the member lists of the interfaces and of the shared frame-limits value; the names (`IDicer`, `IRayContext`, `IProceduralExpander` of #37 against extending `CGeometryHost`); the opaque scratch/thread handle the procedural expander needs in place of `CShadingContext *`; the inline pixel-transform helpers `surface.cpp` takes from `renderer.h`; the lens function's signature and what replaces `cocFactorSamples`; and how the pre-change byte baseline is kept per platform.
