# Contract: new tests and edits to approved tests (for the maintainer's approval)

**Approved at plan level 2026-11-07 (P-Q13)**; each test is still shown at the TEST-APPROVAL pause before it is
written, and the pinned-test diffs (items 8-9) are shown before they are applied.

Constitution III: each new test is written first and shown red on the tree before its code lands. **None of the
tests below may be written before the maintainer approves this list** (the test-approval pause of the tasks stage).
No threshold is widened, no test is disabled, no reference, manifest or replay is re-recorded.

## New tests

| Test | Label | What it proves | Red on the starting tree because |
|---|---|---|---|
| `Geometry_DiceThroughDicer` | `geometry_contract` | A program linking **only `libRIBVector`** (and `libRIBCommon`) dices a sphere, a bilinear patch, a NURBS patch with a trim curve and a points primitive through a recording `IDicer` double (its `probe` samples the undisplaced surface; `inFrustum` answers TRUE; a `CGeometryFrame` is installed by the test, value-initialised with only the fields that survive the lens phase set, so the lens phase does not touch this test): the recorded `drawGrid`/`drawObject`/`drawPoints` calls are those the code's rules require (grid sizes within `maxGridSize`, the split count for a given shading rate). | `IDicer` does not exist; `CSurface::dice` is the stub in `libRIBVector` |
| `Geometry_IntersectThroughRayContext` | `geometry_contract` | The same program intersects a sphere, a polygon mesh (lazy `materialize`) and a displaced quadric (tessellation through `displace`, via a double that applies no shader) through a recording `IRayContext` double, and `shade` delivers the hits with `u`, `v`, `time`, `I`, `du` written into its varyings. | `IRayContext` does not exist; `CSurface::intersect` is the stub |
| `Geometry_ObjectSink` | `geometry_contract` | `instantiate` of a sphere, a polygon mesh, a CSG solid instance (renderer build) reports exactly the created objects to a recording `IObjectSink`. | `IObjectSink` does not exist |
| `Geometry_FrameDefault` | `geometry_contract` | With nothing installed `geometryFrame()` is all zero and `raytraced()` is false; with a frame installed the fields read back bit for bit; `camera2pixels(frame, ...)` equals `renderer.h`'s over the same values (renderer build); it names `cocFactorPixels` and `invFocaldistance`, so item 11 below edits it in the lens phase. **F1 sanity case (approved at PAUSE 2a, 2026-11-09, T026)**: a frame built after a raytrace hider's constructors (a `Hider "raytrace"` session past `RiWorldBegin`) carries `ATTRIBUTES_FLAGS_PRIMARY_VISIBLE` in `raytracingFlags`, the expected value read from `CRenderer::raytracingFlags`; this case installs its own frame, so it cannot detect where `beginFrame()` installs the core's, and that point is guarded by the mutation of tasks T045. | `geometryFrame.h` does not exist |
| `Geometry_ExpansionThroughHost` | `geometry_contract` | A delayed object and a delayed instance expand once when intersected from two threads and diced from a third, through a recording `CGeometryHost`; with no host, one `CODE_BUG` and no expansion. | the host has no expansion members |
| `Geometry_NoHiderShadingNames` | `architecture` | No file under `src/ri/geometry/` contains `CReyes`, `CStochastic`, `CZbuffer`, `CRaytracer`, `CPhotonHider`, `CHider`, `CShadingContext`, `IShadingProducer`, `CRenderer`, `CRendererContext` or `CRiInterface` (comments included), except `blobbyRepeller.cpp`'s two `CRenderer::` uses. A `cmake -P` scan. | today's files name them |
| `Geometry_RibVectorNoStubs` | `architecture` | `ribVector_obj`'s sources name no `vector/*Stub.cpp` except `irradianceDispatchStub.cpp`, and `nm -u` over its objects names no `CShadingContext`, `CReyes`, `CHider`, `IShadingProducer` symbol. | three stubs are compiled |
| `Camera_LensCircleOfConfusion` (lens phase) | `camera` | [lens-model.md](lens-model.md) "Analytic tests". | `circleOfConfusion` does not exist |
| `Camera_LensRayMatchesCoC` (lens phase) | `camera` | the raytracer's ray and the function agree. | same |
| `Hiders_ReyesBucketBoundCoversCoC` (lens phase, FR-031) | `hiders` (unit, renderer build) | every scattered vertex position at the extreme lens points lies inside the object's bucket-assignment bound and its grid bound, for grids on both sides of the focal plane | the bucket bound is narrower than the scatter by `sqrt(1 + (dSy/dSx)²)` |
| `DoF_BlurDiameter_reyes`, `DoF_BlurDiameter_raytrace` (lens phase, proposed) | `visual` | measured blur width against the analytic diameter within the measured noise bound. | Reyes blurs 1.35x too wide |

The `geometry_contract` programs live in `tests/geometry_contract/` and join the everyday run
(`-LE determinism-sweep`). The temporary option `OPENRENDER_031_RED_TESTS` (as spec 029's DEP-2) keeps them out of
the normal build until their phase, and is removed in the last interface phase.

## Edits to approved tests

Mechanical (types and names only; no assertion, threshold or recording changes), inventoried at Phase 1 by the
include-fallout probe into `tools/test-edit-inventory.txt` and applied in the phase that changes the signature:

1. `instantiate(..., CRiInterface *)` -> `IObjectSink *` (7 files): `tests/unit/csg/test_boundary_rule.cpp:56`,
   `test_lazy_resolution.cpp:170`, `test_region_inheritance.cpp:32`, `tests/shading_contract/rendered_harness.h:45`,
   `tests/shading_parity/test_ambient_accumulation.cpp:75`, `texture_substitute_harness.h:63`,
   `test_false_if_constructs.cpp:82`.
2. `intersect(CShadingContext *, CRay *)` -> `intersect(IRayContext &, CRay *)`: `test_lazy_resolution.cpp:169`,
   `test_region_inheritance.cpp:31`, and the `CSurface` subclasses of the harnesses in item 1 if they override it.
3. `CShadingContext *` -> `IRayContext *` in region and crossing overrides: `test_region_inheritance.cpp:37-38`,
   `test_boundary_rule.cpp:44`.
4. `setChildren(NULL, x)` -> `setChildren(x)`: `test_region_inheritance.cpp:59`, `:72`, `:81`, `:93`, `:97`.
5. The context helpers: `tests/unit/csg/csgTestUtils.h:158` (`CShadingContext *ctx()` -> `IRayContext *ctx()`,
   `&CRenderer::contexts[0]->shadingContext()` -> `CRenderer::contexts[0]`), `:180-214` (`materialize(*ctx)`), and the
   `CShadingContext *` parameters of `test_count_crossings.cpp`, `test_classify_quadrics.cpp`,
   `test_parity_robustness.cpp`, `test_classify_determinism.cpp`; `resolve(*s.ctx())` in `test_procedural_operand.cpp`,
   `test_lazy_resolution.cpp`, `test_interior_exterior_selection.cpp`; `tests/unit/sampling/scene_probe.h:134`,
   `test_object_ids.cpp:237`.
6. The motion replay harness, never its recordings: `tests/unit/motion/replay_capture.h:166`, `:184-185`;
   `motion_replay.cpp:67` (the member-pointer type `void (CCurveMesh::*)(IRayContext &)`), `:503`, `:529`, `:580`,
   `:691-702`.
7. Added `#include` lines in test files that relied on `object.h`/`points.h`/`curves.h` for `renderer.h` or
   `shading.h` (list from the Phase 1 probe).

Not mechanical (each shown to the maintainer as a diff):

8. `tests/unit/motion/test_frame_params_equal_core.cpp`: two `bits(...)` checks for `FrameParams::cocFactorScreen`
   and `cocFactorSamples` (Phase 7), removed again in the lens phase with the fields.
9. `tests/unit/camera/test_camera_lens_model.cpp` (lens phase): the `cocFactorScreen()` check (which pins the Reyes
   expression `ip·A·f/(f + A)`) is replaced by `circleOfConfusion` checks; the other checks stay.
10. Depth-of-field references (lens phase), image by image: research R11.
11. `tests/geometry_contract/test_frame_default.cpp` (`Geometry_FrameDefault`, lens phase, tasks T115a): the two
    read-back checks of `cocFactorPixels` and `invFocaldistance` become a bit-for-bit read-back of the new `lens`
    member when `CGeometryFrame` replaces them; shown as a diff first, own pause. (`Geometry_DiceThroughDicer` needs
    no edit; spec 028's `tests/unit/camera/test_frame_default.cpp` names neither field and is only re-checked.)
