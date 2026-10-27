# Research: Camera and Motion Layer

**Plan**: [plan.md](plan.md) | **Spec**: [spec.md](spec.md)

Each item gives the decision, the rationale and the alternatives considered. Line numbers are on
master `4eb683b`.

## R1. Where `FrameParams`, `ICamera` and the evaluator live (FR-003, FR-012, Principle VIII)

**Decision**:
- `FrameParams` and its installation slot: **Graphics State**, `src/ri/state/frameParams.{h,cpp}`.
- `ICamera`, its value types and the shipped camera `CFrameCamera`: **Graphics State**,
  `src/ri/state/camera.{h,cpp}`.
- The motion evaluator: **Geometry**, `src/ri/geometry/motion.h` (header-only, inline).
- The core builds both per frame in a new core-private unit, `src/ri/render/rendererFrame.{h,cpp}`.

**Rationale**:
- The allowed-dependency table lets Geometry depend on Graphics State and Procedures only, and
  Hiders on Graphics State, Geometry, Shading and Display. Graphics State is the one domain both
  may reach, and it depends on nothing, so a State home adds no edge: `frameParams.h` and
  `camera.h` include only `common/` (Infra).
- D30 (1) puts the future integrators in a Transport domain that may depend on Graphics State,
  Geometry and Shading, but not on Hiders. A camera in Hiders would be out of the integrators'
  reach (specs 042 and 050); a camera in State is reachable by Hiders, Transport and, later,
  Shading (a #107 fix could read `FrameParams` without a new edge).
- Graphics State owns options and transforms. `FrameParams` is the frame's resolved camera and
  shutter options plus the world transform at shutter open and close. The camera is a pure
  function of that value; it holds no state and draws no random number (FR-005).
- The evaluator is Geometry-owned by FR-011, and Geometry is its main user. Hiders reach it under
  the table (Hiders -> Geometry) when spec 031 moves Reyes onto it.
- `ray.h` (`CRay`) is Infra (`src/ri/core`), so the evaluator's ray transform needs no new edge.
- `archcheck` classifies by directory. Every new file sits in an already classified directory, so
  `domain-map.txt` does not change.

**Alternatives considered**:
- *Infra (`src/common`)*: every domain may depend on it, but it is infrastructure shared by
  `libRIBCommon`, which must stay the only definer of process-wide state (gotcha 22) and hold no
  renderer concept. Rejected.
- *Camera in Geometry*: reachable by Hiders and Transport, but not by Shading, and a camera is
  not a primitive. Rejected.
- *Camera in Hiders*: unreachable from Transport (D30). Rejected.
- *A new "Camera" domain*: needs a constitution amendment for one interface. Rejected; the
  maintainer can still choose it (plan, open question Q1).

## R2. `FrameParams` and spec 039's `CSamplingConfig`: one structure or two (FR-002)

**Decision**: two structures, one containing the other. `FrameParams` embeds the frame's
`openrender::sampling::CSamplingConfig` by value, as its member `sampling`. Every field
`CSamplingConfig` has (backend, seed, frame, `nx`/`ny`, jitter, crop origin, screen-window origin)
lives only there; `FrameParams` adds the shutter, raster, lens and camera-motion values and none of
`CSamplingConfig`'s.

**Rationale**:
- FR-002 forbids duplicating a `CSamplingConfig` field. Embedding gives one definition of each
  value and lets the camera read the screen-window origin it needs (`screenLeft`, `screenTop`)
  from `fp.sampling`.
- `CSamplingConfig` is in `src/common/sampling.h` (Infra), consumed by the sampler library, its
  tests and `CShadingContext::setSamplingConfig()` (`shading.h:340`). Merging the two would edit
  `shading.h`, which 028 must not touch (FR-017, spec 027 overlap), and would put renderer
  concepts into `libRIBCommon`.
- The core builds the `CSamplingConfig` once (`renderer.cpp:775`) and copies it into
  `FrameParams`; the contexts keep receiving the same value through `setSamplingConfig()`. Copying
  a struct of integers and floats is exact.

**Alternatives considered**: one merged structure (rejected above); `FrameParams` holding a pointer
to the core's `CSamplingConfig` (rejected: the core's copy is a local in `beginFrame`, and a value
member keeps `FrameParams` self-contained and trivially copyable for tests).

## R3. How Geometry and the raytrace hider receive the frame values (FR-001, FR-003)

**Decision**:
- **Geometry** reads `openrender::frameParams()`, a State-owned accessor over a slot the core
  fills with `installFrameParams(&fp)` in `beginFrame` and clears with `installFrameParams(nullptr)`
  in `endFrame`, after the contexts are deleted. With nothing installed the accessor returns a
  default `FrameParams` (`invShutterTime = 1`, no camera motion, pinhole), never a null.
- **The raytrace hider** receives the camera by constructor injection:
  `new CRaytracer(i, frameCamera())`. It never reads `FrameParams` itself.

**Rationale**:
- Every Geometry reader is reached from a call that carries no frame context: `sample()` is called
  by `shading.cpp` (which 028 must not edit) with `(start, numVertices, varying, locals, up)`, and
  `intersect()` carries only `CShadingContext*` and `CRay*`. Changing either signature touches
  every primitive and `shading.cpp`.
- A registration slot is a declared interface (rule 1: "a registration/factory hook"), owned by
  the lower domain and filled by the composition root, the pattern of spec 024's
  `setDefaultServices()` and spec 025's `shaderParameterHooks.h`. The data is immutable for the
  frame, written before any render thread starts and read-only afterwards, so it needs no lock.
- Per-frame lifetime gives session reuse for free: each `beginFrame` installs a fresh value, and
  `endFrame` clears it (edge case "session reuse", gotcha 20).
- The slot lives in `ri_sources`, so `ribRender` and `ribVector` each hold one, exactly like the
  `CRenderer` statics; a process loads one of the two.
- The camera is built from the same `FrameParams` and is shared by every thread (all methods are
  `const`).

**Alternatives considered**:
- A Geometry-owned `CGeometryHost::frameParams()`: a virtual call per vertex batch on a host that
  exists to capture objects, and the hider could not use it without depending on that host.
  Rejected.
- Passing `FrameParams` through `CRay` or `CShadingContext`: touches `shading.h` or every ray.
  Rejected.
- Leaving `CRenderer::invShutterTime` behind a Geometry-owned accessor implemented by the core
  (link-time substitution): that is a DB-017 pattern. Rejected.

## R4. What `FrameParams` holds, and why its values are bit-identical (FR-001, decision 5)

**Decision**: the core fills `FrameParams` by **copying** the `CRenderer` statics after
`beginFrame` has computed them (`renderer.cpp:295-336, 370-517`); it computes nothing itself. The
shutter scale is the same reciprocal `invShutterTime` (`renderer.cpp:459-463`), so
`dPdtime = difference * invShutterTime` keeps its exact bits (maintainer decision 5).

**Rationale**: a copy of a float is exact, so every value equals the one `CRenderer` holds today
(FR-001) without re-deriving any formula. The statics stay, because Reyes (spec 031), the
renderer core, `addObject` (R10) and Shading's service (FR-017) still read them.

The field list is in [data-model.md](data-model.md). It omits the circle-of-confusion factors
(`cocFactorScreen`/`Samples`/`Pixels`), which are Reyes's model: the lens model exposes their
inputs (R6).

## R5. The `ICamera` API (FR-004 to FR-010)

**Decision**: see [contracts/camera.md](contracts/camera.md). In brief:

```cpp
CameraRay generateRay(float x, float y, float t, const float lensUV[2]) const;   // pure virtual
CameraRay sampleRay(const CameraSample &s) const;                                 // non-virtual, forwards
virtual LensModel lensModel() const;                                              // default: pinhole
virtual CameraSupport evaluateImportance(const CameraRay &, CameraImportance &) const;          // reserved
virtual CameraSupport sampleConnection(const float ref[3], const CameraSample &, CameraConnectionSample &) const; // reserved
```

- `x`, `y` are full-image pixels (the hider's `xAbs`/`yAbs`, spec 039 T117), `t` the shutter
  fraction, `lensUV` the unit-disk lens point. The camera returns camera-space `from`, `dir`
  (normalized), `time = t`, and the differentials `da`, `db`.
- **Projection and lens**: perspective or orthographic from `fp.projection`; pinhole when
  `fp.aperture == 0`, thin lens otherwise, exactly the branch `computeSamples` takes
  (`raytracer.cpp:626`). Orthographic always has aperture 0 (`renderer.cpp:438-441`).
- **Bit-identity** (FR-006, FR-014): `CFrameCamera` reproduces `computeSamples`'s arithmetic
  expression by expression: `fullImagePixels2camera()` (`renderer.h:618-631`) with `z = 0` and
  `z = imagePlane` (pinhole) or `z = focaldistance` (thin lens), `from[X] += lensU * aperture`
  written as a compound assignment, `subvv`, `normalizev`, and `da = dxdPixel / imagePlane` or
  `db = dxdPixel` (`raytracer.cpp:684-701`). R9 explains why the expression shapes matter.
- **Crop invariance** (FR-006): the camera maps through the screen window's own origin
  (`fp.sampling.screenLeft/Top`), never the rendered window's, so a film position gives the same
  ray in a crop and in the full frame by construction; `Camera_CropInvariance` pins it.
- **The camera sample** (FR-005): the hider keeps calling `CSampler::cameraSample()` and keeps the
  film mapping to `xAbs`/`yAbs` (sample identity is spec 039's, in Hiders); it hands the result to
  `generateRay`. The camera holds no random state.
- **§7.4's `sampleRay(CameraSample)`** maps onto `generateRay`: `CameraSample` is `{x, y, time,
  lensU, lensV}`, and `sampleRay` is a non-virtual inline forwarder, so the reference doc's name
  exists with no second implementation and no implementer has to provide it.
- **Reserved BDPT room** (FR-009, §7.4): `evaluateImportance` and `sampleConnection` are virtual
  with default bodies that return `CameraSupport::NotSupported` and do not touch their output.
  Their result types `CameraImportance` and `CameraConnectionSample` are declared but not
  defined: the BDPT spec defines them (and decides their spectral type under D18) without changing
  `generateRay`, the vtable order of the existing entries, or any implementer that provides only
  `generateRay`.
- **Lens model** (FR-007, US5): `LensModel` carries the projection, image plane, aperture radius,
  focal distance and its reciprocal, focal length and f-stop, and an inline `cocFactorScreen()`
  with `renderer.cpp:447`'s expression, so spec 031 can derive Reyes's circle of confusion from the
  camera. 028 does not call it from Reyes.
- `ICamera` is an internal C++ interface, not a plug-in boundary, so rule 4's Semantic Versioning
  does not apply (DB-003 untouched).

**Alternatives considered**:
- Filling a `CRay&` directly: saves a copy of eight floats, but couples the camera's contract to
  the hider's ray type and makes the analytic tests depend on `CRay`'s layout. Rejected; the hider
  copies the eight fields.
- A batch `generateRays(span)`: no measured need; one virtual call per camera sample is accepted by
  the spec if SC-008 holds (R12).
- Defining `CameraImportance` now: would pre-decide D18's spectral type. Rejected.

## R6. The evaluator API (FR-011, FR-012, FR-014, decision 3)

**Decision**: see [contracts/motion-evaluator.md](contracts/motion-evaluator.md). One header,
`src/ri/geometry/motion.h`, namespace `openrender::motion`:

- **The one definition of the two-key model**:
  `template <class S> inline S atTime(S key0, S key1, S t) { return key0 * (S(1) - t) + key1 * t; }`.
  Linear, unclamped (extrapolates outside 0..1 as today), `t = 0` is shutter open and `t = 1`
  shutter close.
- **Per-site precision entry points**, all instantiations of that template (decision 3):
  - `atTimeD(float, float, float) -> float`: evaluated in double, stored in float. The quadrics'
    parameters, `interpolatev` (`mathSpec.h:126`), the patch, polygon, patch-grid and tessellation
    keys.
  - `atTime<double>(double, double, double) -> double`: the polygons' fused bilinear-temporal
    expressions, where the two operands are themselves double expressions
    (`polygons.cpp:312-351, 882-922`).
  - `atTimeF(float, float, float) -> float`: evaluated in float. `subdivision.cpp:186` (the one live
    float site; see R14).
  - `atTimeV(float dst[3], const float k0[3], const float k1[3], float t)`: three `atTimeD`, equal to
    `interpolatev`.
  - `keyAtTime(float scratch[3], const float *k0, const float *k1, float t) -> const float *`: returns
    `k0` itself when `k1` is null (a static object: its own data, unchanged, US2 AS2), else `scratch`.
- **Shutter keys and rate**: `sampleKey(unsigned int up) -> int` (0 for `PARAMETER_BEGIN_SAMPLE`,
  1 for `PARAMETER_END_SAMPLE`), `shutterRate() -> float` (`frameParams().invShutterTime`) and
  `toShutterRate(float v[3])` (`mulvf(v, shutterRate())`), the one place the `dPdtime` scale is
  read (FR-016).
- **Grid transforms at time**: `transformAtTime`, `vtransformAtTime`, `ntransformAtTime`, moved
  verbatim from `objectMisc.h:66-139`.
- **The ray transform** (FR-019): `transformRay(oFrom, oDir, xform, ray)`, moved verbatim from
  `objectMisc.h:178-232`, with `CRenderer::cameraHasRotation`, `relRotQ` and `toWorld` read from
  `frameParams().camera`. Its SLERP is `cameraRotationAt(M, q, t)`
  (`slerpq(identity, q, t)` then `qtoR`), the primitive the equivalence test pins (R8).
- **Plug-in time**: `pluginTime(const CRay &) -> float`, the shutter fraction handed to an implicit
  plug-in (R11).

**Rationale**:
- The families store their keys four different ways: a parameter block (`nextData`, quadrics),
  interleaved vertex keys (`vertex + 4 * vertexSize`, bilinear patch; `vertex + vertexSize`,
  curves), separate key arrays (polygons) and key grids (tessellation, patch grids). A
  `CObject::atTime(t)` returning an object state would need a per-family copy of the data at `t`:
  an allocation in the ray loop and a different order of arithmetic.
- Bit-identity (FR-014) forbids changing the order of operations, and the polygons fuse the
  bilinear and the temporal interpolation in one double expression. Only an expression-level
  primitive keeps those bits: the site computes its two spatial operands and the evaluator applies
  the two-key model to them.
- So "one evaluator" means one definition of the model (the template), one place for the shutter
  scale, one ray transform with its camera SLERP, and one set of grid transforms. The precision
  stays per site and is named at the call (`atTimeD` versus `atTimeF`), as decision 3 allows.
  `Motion_TwoKeyModel` pins that every entry point is the template.
- Header-only and inline: no call overhead in the per-ray and per-vertex loops (SC-008), and the
  isolation test needs no Geometry object file.

**Alternatives considered**: a virtual `CObject::atTime(t)` (rejected above); a `CMotionKeys<T>`
view type for every family (rejected for 028: it fits only the vertex-key families and would not
change any arithmetic; spec 031 or a later precision-unifying phase can add it); unifying the
precision now (decision 3: a later, approved, output-changing phase).

## R7. Phase order and one family per phase (FR-013, decision 2)

**Decision**, after the baseline, red-test and foundational phases:

| Phase | Family | Allowlist lines deleted first |
|---|---|---|
| 4 | camera: `CRaytracer::computeSamples` onto `ICamera` | 7 DB-011 (`raytracer.cpp` -> `imagePlane`, `invImagePlane`, `projection`, `focaldistance`, `aperture`, `dxdPixel`, `dydPixel`) |
| 5 | the ray transform shared by `CDLObject` and the quadrics | 6 DB-004 (`dlobject.cpp`, `quadrics.cpp` -> `cameraHasRotation`, `relRotQ`, `toWorld`) |
| 6 | quadrics (7): intersection parameters, `sample()`, the grid transforms, `dPdtime` | 1 DB-004 (`quadrics.cpp -> invShutterTime`) |
| 7 | patches: `CBilinearPatch::intersect`, the three `sample()` in `patches.cpp` | 1 (`patches.cpp -> invShutterTime`) |
| 8 | polygons: `CPolygonTriangle`/`CPolygonQuad` intersection and `sample()` | 1 (`polygons.cpp -> invShutterTime`) |
| 9 | implicit surface: `CImplicit::intersect` | none |
| 10 | tessellation: `CTesselationPatch::intersect` (`surface.cpp:1451-1610` only) | none |
| 11 | grid-only: `curves`, `points`, `patchgrid`, `bsplinePatchgrid` (plus `subdivision`, Q2) | 4 (`curves`, `points`, `patchgrid`, `bsplinePatchgrid` -> `invShutterTime`) |

**Rationale**:
- The camera goes first: it is US1 (P1), touches one hider function, and its byte-identity covers
  every raytrace scene at once.
- The ray transform is one family by FR-013, and its six link edges are attributed to the two
  objects that instantiate the inline `transform()` (`dlobject.o`, `quadrics.o`), so they go
  together. Moving it before the quadrics phase keeps phase 6 to the quadrics' own motion.
- Each intersection family moves its own `sample()` in its phase (decision 2). The grid-only
  families have no intersection and come last.
- Deleting a phase's allowlist lines first makes `ctest -L architecture` red with exactly those
  `VIOLATION` lines; the phase is done when it is green again and nothing else changed (CP-c).

**Alternatives considered**: quadrics before the ray transform (rejected: the quadrics phase would
carry two families' edges); one phase for all `sample()` paths (rejected by decision 2).

## R8. The SLERP equivalence test (FR-021(a))

**Decision**: `Motion_CameraSlerpEquivalence` (label `motion`), linking only the State-only library
and `libRIBCommon`. For a fixed set of relative rotations (identity, 1e-4 rad, 5 degrees, 45
degrees, 90 degrees about each axis and an oblique axis, 179 degrees) and times (0, the strata
1/16 .. 15/16, 0.25, 0.5, 0.75, 1), it compares **bitwise** (`memcmp`):
1. `motion::cameraRotationAt(M, q, t)` against Reyes's sequence `slerpq(Rjt, identQ, q, t);
   qtoR(M, Rjt);` (`reyes.cpp:1433,1604,1643`, `stochasticQuad.h:520,884`, `rendererContext.cpp:740`),
   with `q = relRotQ`;
2. the same against the conjugate form of the pure-rotation fast path (`stochastic.cpp:166-221`)
   and of the ray transform (`objectMisc.h:209-214`), `q = conj(relRotQ)`;
3. `motion::transformRay` against a verbatim copy of `objectMisc.h:178-232` for camera-motion
   xforms with and without rotation and for object-motion xforms.

**Rationale**: Reyes's copies are inline code inside hider functions and cannot be called from a
test. 028 does not edit any Reyes file (FR-018; checked by `git diff --stat` in quickstart §3), so a
verbatim copy cited by file and line stays exact for the spec's lifetime. When spec 031 replaces
those copies with `cameraRotationAt`, the test becomes its regression guard.

## R9. Floating-point contraction and per-platform evidence (FR-014, decision 1)

**Finding**: the macOS build is Apple Clang 21 on arm64 with `-O3` and no `-ffp-contract` flag
(`build-028/src/ri/CMakeFiles/ribRender_obj.dir/flags.make`). Clang's default for C++ is
`-ffp-contract=on`: a multiply and an add **in one source expression** may fuse into an FMA, which
rounds once instead of twice. GCC 13 with `-std=c++20` defaults to `-ffp-contract=fast` and fuses
across expressions on targets with FMA (aarch64; not baseline x86-64).

**Decision**:
- The evaluator and the camera keep each site's **exact expression shape**: operand order,
  parenthesization and compound assignments (`key0 * (1 - t) + key1 * t`, never
  `key1 * t + key0 * (1 - t)`; `from[X] += lensU * aperture`). Clang forms the same fused
  operation for the same expression whether it is written at the site or inside an inline
  function, because the contraction decision is made per source expression.
- The camera and evaluator unit tests compare against verbatim copies compiled in the test's own
  translation unit, so they see the same contraction regime on each platform.
- The replay files and image manifests are per platform (decision 1). A difference on either
  platform stops the phase and is reported; a replay file is never re-recorded from a changed
  tree.

## R10. Camera-motion synthesis in `addObject` (FR-020, FR-021(b))

**Decision**: it stays in the renderer core, unchanged, reading the core's own statics
(`rendererContext.cpp:686-760`). `FrameParams` copies the same values, so the evaluator composes
exactly what `addObject` synthesizes. An object with its own transform motion keeps receiving no
camera motion (`o->xform->next == NULL` test at `:692`, GitHub #106 preserved).

**Rationale**: the core may read its own statics; the code is shared with Reyes (spec 031); moving
it gains no allowlist line; and leaving it untouched removes the risk of changing #106's behaviour.
The replay test pins #106: an object-motion primitive under a moving camera is recorded on the
starting tree and must replay unchanged.

## R11. The implicit surface (FR-013)

**Finding**: `CImplicit` stores no motion keys and has no transform motion: it passes the ray's
shutter fraction to the plug-in's `implicitEval`/`implicitEvalNormal` (`implicitSurface.cpp:142,
155,176`; `implicit.h:39-40`) and marches the ray as given. Its "motion" is the plug-in's own.

**Decision**: its phase routes the three reads through `motion::pluginTime(*ray)`, which documents
the convention (the unclamped shutter fraction, the same value the ray carries), and pins it with
the replay test and a moving-sphere fixture plug-in. No arithmetic moves, and the Procedures ABI
(`implicit.h`) is unchanged, so rule 4 does not apply.

**Alternative considered**: declaring `CImplicit` already compliant and skipping its phase.
Rejected: FR-013 lists it, and it has no test today (no shipped scene uses `Geometry "implicit"`).

## R12. Hot-path cost (SC-008)

- `generateRay` is one virtual call per camera sample, against two `fullImagePixels2camera` calls,
  a normalization and a trace per sample. The evaluator is inline. `frameParams()` is one load of
  an installed pointer; per-vertex loops hoist `shutterRate()` before the loop (same value, same
  bits).
- SC-008 is measured on the three slowest camera, motion and DOF raytrace scenes, five runs each
  on the starting tree (validation.md §0.5), and re-measured after phases 4 and 11.

## R13. Replay tests (US2 Independent Test, FR-024, decision 1)

**Decision**: one tool, `motion_replay` (`tests/unit/motion/`), with `--record <file>` and
`--check <file>`; the replay data is `tests/unit/motion/replay/<platform>.txt`, one file per
platform. Format and scenes: [contracts/replay-format.md](contracts/replay-format.md).
- The tool uses **only starting-tree interfaces** (the Ri API, `CObject::intersect()`,
  `CObject::sample()`, a capture context modelled on `tests/unit/csg/csgTestUtils.h`), so it builds
  and records on the unchanged tree on each platform.
- Its capture context calls `CRendererContext::addObject()` first, so camera-motion synthesis runs
  exactly as in production, then records the object.
- It intersects a fixed set of camera-space rays at times -0.25, 0, 0.25, 0.5, 0.75, 1 and 1.25 with
  every leaf of each family, and calls `sample()` at the same times and with
  `PARAMETER_BEGIN_SAMPLE`/`END_SAMPLE`, requesting P, Ng, dPdu, dPdv and dPdtime. Every float is
  written as a C99 hex float (`%a`), so a check is bitwise.
- Shutters `0 1`, `0 0.5` (`invShutterTime = 2`) and `0.25 0.75` exercise the reciprocal; `0 1`
  alone would multiply by 1 and hide a wrong scale.
- Families: the seven quadrics, the bilinear patch, polygon triangle and quad, the implicit
  surface (fixture plug-in), the tessellation patch (a moving bicubic patch and a moving NURBS,
  intersected through `CSurface::intersect`), `CDLObject` (fixture plug-in), and the grid-only
  `sample()` families (curves, points, patch grids, subdivision). Camera translation, rotation and
  rotation-only, and an object-motion primitive under a moving camera (#106).
- ctest registers one `Motion_Replay_<family>` test per family, each running `--check` with a
  family filter, and `Motion_ReplayRecord`, which records into the build tree only (the tracked
  file is copied from it on the starting tree, never overwritten by a test run).

**Red evidence**: a replay test is a characterization test: recorded on the starting tree, it is
green there by construction. Its red evidence is a mutation in a scratch copy (swap the operands
of one `interpolatev` call; drop the `(double)` of one quadric parameter), shown red per family.
This is spec 025's T-P1a precedent; FR-024's wording asks for "shown failing on the starting tree",
which a replay cannot do (plan, open question Q3).

## R14. Findings made while planning

1. **`interpolate()` and `interpolate3()` in `objectMisc.h:145-172` have no caller.** The spec's
   Measured starting point cites them as the float-precision sites; the one live float site is
   `CSubdivision::sample()` (`subdivision.cpp:176-191`, `vertex0[j] * (1 - ctime) + vertex1[j] *
   ctime` with `const float ctime`). The two dead helpers are deleted in phase 11 (no caller, so no
   output change).
2. **`subdivision.cpp`'s time interpolation is not in decision 2's grid-only list** and has no
   `invShutterTime` read (it computes no `dPdtime`). The plan includes it in phase 11 (FR-018,
   "each family's `sample()`"), pending the maintainer (Q2).
3. **`CBilinearPatch::sample()`'s `dPdtime` has a precedence error** (`patches.cpp:477`):
   `a - b * (1 - cv) + c * cv` subtracts only the first half of the shutter-open position, where
   `a - (b * (1 - cv) + c * cv)` is meant. Preserved bit for bit (FR-014); to be filed as an
   issue by the maintainer (no GitHub write from this stage).
4. **`CImplicit` and `CDLObject` have no test and no shipped scene** (R11); the replay fixtures
   are their first coverage.
5. **The spec's Measured starting point lists `CTesselationPatch` among the intersection families**;
   its intersection lives in `surface.cpp:971-1610`, a file whose DB-004 lines belong to spec 031.
   Phase 10 edits only `CTesselationPatch::intersect`'s moving branch (`surface.cpp:1451-1610`) and
   no allowlist line.
6. **DB-009's evidence** cites `stochastic.h:81,108` and `raytracer.cpp:612-621`; the thin lens is
   at `raytracer.cpp:650-665`, and Reyes's circle of confusion is
   `CSampler::circleOfConfusion` (`sampler.cpp:23`) with 7 call sites in `reyes.cpp`.

## R15. Overlap with spec 027 (FR-017, Assumptions)

- 028 does not edit `src/ri/texture/`, `rendererServicesImpl.h`, `rendererServices.h` or
  `shading.{h,cpp}`.
- Shared files: `renderer.cpp` (027 edits the texture init and shutdown in `beginFrame`/`endFrame`;
  028 adds two calls and one constructor argument near `renderer.cpp:775-810` and in `endFrame`),
  `src/ri/CMakeLists.txt` (different list entries), `tests/architecture/allowlist.txt` (disjoint
  lines), `metrics-baseline.txt` (the Geometry, Hiders and TOTAL rows; 027 changes Texturing and
  TOTAL) and `DOMAIN_BOUNDARIES.md` (different entries). To keep the `renderer.cpp` hunks small,
  the frame-parameter code lives in the new `rendererFrame.cpp`, and 028 does not edit
  `renderer.h` or `rendererStatics.cpp`.
- No 028 task depends on 027. Whichever branch merges second resolves the textual conflicts in
  those files and re-runs `ctest -L architecture`; that is the maintainer's merge, not a task.

## R16. Doubly compiled sources and `archcheck` (spec 025 R11 precedent)

The camera and isolation tests compile `src/ri/state/frameParams.cpp` and `camera.cpp` a second
time on a narrow include path. `archcheck` exits 2 on any missing in-scope object, including an
`EXCLUDE_FROM_ALL` target's, so those test targets are defined only behind
`OPENRENDER_CAMERA_MOTION_ONLY_TESTS`, **off by default until phase 3** creates the sources, then
switched on. Red evidence for them is taken in a scratch build directory configured with the
option on. The replay tool, its fixtures and `Frame_ParamsEqualCore` compile no `src/` source and
need no option.
