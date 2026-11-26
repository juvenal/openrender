# RenderMan Spec 3.2 — Implementation Gaps

The delta between RI Spec 3.2 and openRender, in two sections, because the
spec itself draws the line in two different places:

- **§1.1.2 Advanced Capabilities** — the eleven optional capabilities the spec
  names, and by which a renderer is expected to describe itself. These are the
  claims that belong in a `##CapabilitiesNeeded` RIB header.
- **Per-request gaps** — individual statements or parameters that are absent
  or partial but are not one of those eleven. A gap here does not cost a
  capability claim; it is still a gap.

`[x]` implemented, `[~]` partial (the entry says exactly what is and is not
there; none is partial among the eleven today), `[ ]` absent. Every status below was checked against the source at the
date in the footer, not carried forward.

---

## §1.1.2 Advanced Capabilities

**9 of 11 implemented, 0 partial, 2 absent.**

- [x] **Solid Modeling** — `RiSolidBegin`/`RiSolidEnd`
  (`rendererContext.cpp`). All three set operations plus `"primitive"`, over
  arbitrarily nested trees. Since spec 021 the solid is evaluated **lazily and
  on demand**: `RiSolidEnd` builds an immutable `CCSGDefinition`
  (`src/ri/geometry/csgDefinition.{h,cpp}`) and adds one `CCSGSolid`
  (`csgSolid.{h,cpp}`) that resolves on its first `dice()`/`intersect()`.
  Composition is spatial trimming: every operand leaf carries a region
  predicate (`csgClassify.{h,cpp}`) that the intersectors and the REYES dicer
  honour, so seams are exact to pixel precision in raytrace, REYES and
  Z-buffer alike (spec 013's BSP boolean of tessellated meshes is retired).
  Operands may be quadrics, polygon and patch meshes, NURBS, subdivision
  meshes, blobbies and procedurals (expanded lazily into their operand). A
  single open quadric is closed by implied caps; any other open operand is
  empty, with a warning. Implicit (`Geometry "implicit"`), DSO (`"dlo"`),
  point, curve and object-instance primitives are not supported as operands
  (dropped with a warning). `Attribute "solid" "tessellationtolerance"` is
  accepted but inert (one notice per frame). Malformed input (an unmatched
  `SolidEnd`, nesting inside `"primitive"`, an unknown operation) is
  diagnosed rather than crashed. See `specs/021-deferred-csg-evaluation/`,
  `specs/013-solid-csg-operations/` and the manual's *Solid CSG Operations*.

- [x] **Level of Detail** — `RiDetail` projects the given bound to screen
  space and stores `lodSize` (`rendererContext.cpp:2202`); `RiDetailRange`
  sets the discard flag outside `[minvis, maxvis]` and an `lodImportance`
  fade across the transition bands (line 2225). `RiRelativeDetail` scales the
  comparison (line 1430).

- [x] **Motion Blur** — transformation and deformation motion across
  `MotionBegin`/`MotionEnd`, converged between the REYES and ray-trace hiders
  by the shared `CSampler` time-stratum kernel. Camera rotation uses SLERP
  rather than LERP of vertex positions (`slerpq()` in `common/mathSpec.h`),
  gated by `CRenderer::cameraHasRotation`. **Two motion samples**, a property
  of `CPl`'s `data0`/`data1` shared by every primitive. The z-buffer hider
  does no time sampling at all and renders every primitive unblurred — a
  hider limitation, not a capability gap. See [HIDER_PARITY.md](HIDER_PARITY.md).

- [x] **Depth of Field** — `RiDepthOfField`, with concentric-disk lens
  sampling shared by both camera hiders (spec 007). Since spec 031 both hiders
  also share one circle-of-confusion function (`LensModel::circleOfConfusion`),
  so D9, the old DOF blur-size mismatch, is narrowed (`Parity_dof` 40.45 to 9.92)
  and the remaining difference is the form of one lens, not an open item — see [HIDER_PARITY.md](HIDER_PARITY.md).

- [ ] **Special Camera Projections** — `RiProjectionV`
  (`rendererContext.cpp:960`) accepts `"perspective"` and `"orthographic"`
  only; anything else is `CODE_BADTOKEN`. No spherical, Omnimax, or
  user-supplied projection. Note the spec's default when no `Projection`
  statement appears is **orthographic**, and openRender follows it
  (`ribGeometryContext.h:151`) — a frequent source of "my render is empty".

- [x] **Displacements** — `RiDisplacement`, with displacement bounds. On by
  default for the ray-trace hider too, opt-out via
  `Attribute "trace" "int displacements" [0]`.

- [ ] **Spectral Colors** — **converted to RGB on entry, not carried.**
  `RiColorSamples` validates `N` and stores `nColorComps` and the two
  conversion matrices on `COptions` (`rendererContext.cpp:1678`); `CRenderer`
  copies them at `beginFrame` (`renderer.cpp:237`). Since GitHub #269 `nRGB`
  (n by 3, RISpec 3.2) converts the n-sample `Color` and `Opacity`
  (`COptions::convertColor`), and the RIB `Color`/`Opacity` statements and the
  RIB writer carry n floats, n being the `ColorSamples` count of the frame
  (tracked in `ri.cpp`, restored by `FrameEnd`). `RGBn` (3 by n) is stored and
  still never read, `CRenderer::nColorComps/toRGB/fromRGB` are read by nothing,
  and `Cs` and other colour primitive variables are still three floats
  whatever n is: color is three components throughout the renderer, as the
  spec allows an implementation without the capability to do.

- [x] **Volume Shading** — `RiAtmosphere`, `RiInterior` and `RiExterior` all
  execute (spec 056, 2026-11-10; the earlier "parsed, stored and never
  executed" claim for the last two is gone). `RiInteriorV`/`RiExteriorV`
  (`rendererContext.cpp:2372`, `:2385`) bind the shader on `CAttributes`; the
  rule, on every camera hider and under both the interpreter and the JIT:
  - **Spawning surface.** A ray built by `trace()` (and
    `environment("raytrace")`, the same entry point), `gather()`,
    `transmission()` or a ray-traced `shadow()` is attenuated by the volume
    shader of the surface whose shader is running the builtin, not of the
    surface it hits. The side is the sign of the ray direction against that
    surface's global `N` at the call: positive reads `Exterior`, anything else
    (a tie included) `Interior` (`executeMisc.cpp:942` for `trace()`, `:773`
    for `transmission()`, `shading.cpp:4599` splits a `gather()` bundle and
    `:4615`/`:4622` attach the shader). A side with no shader attenuates
    nothing. `visibility()`, the float-valued
    `trace()` and rays a volume shader itself spawns get no volume step.
  - **Segments, hits and misses.** The shader runs once per ray segment
    (`CSegmentBundle::attenuateHits`/`attenuateMisses`, `bundles.cpp:59`,
    `:82`, through `shadeVolumeSegments()`, `shading.cpp:654`), from the ray's
    origin to its endpoint, for every surface (CSG fragments are no special
    case) and whether the endpoint is a hit, a transparent hit that the ray
    continues through, or a miss. A miss ends at `maxdist` for `gather()`
    where finite, at the light or destination for `transmission()`/`shadow()`,
    else where the ray leaves the frame's world bound. It sees `I` as the
    unnormalised origin-to-endpoint vector, `P` as the endpoint, and `Ci` and
    `Oi` as the hit's colour and opacity (zero for a miss), and changes colour
    and opacity only; a `gather()` miss stays a miss and only the
    `surface:Ci`/`surface:Oi` outputs record the step.
  - **Atmosphere** is unchanged: camera rays only, never on a matte object
    (`shading.cpp:1598`-`1603`). A camera ray has no spawning surface and
    gets no volume step; a secondary ray gets no atmosphere.
  - **Accepted consequence (Q3).** The side comes from the shader's *global*
    `N` at the call, so a surface shader that rewrites `N` (for instance
    `N = faceforward(N, I)`) before tracing changes which volume applies.
  - `incident()`/`opposite()` follow the same side, chosen once per call from
    vertex 0's `dot(I, N)` (`shading.cpp:1907`).
  Not changed: `trace()` ignores `maxdist` (GitHub #98). RISpec §4.2.6 names
  only rays spawned by `trace()`; `gather()`, `transmission()` and ray-traced
  `shadow()` follow the same rule here, which is this renderer's reading.
  *(Corrected 2026-08-29: this was previously recorded as blocked on missing
  CSG support. That was never the reason. Corrected again 2026-11-10: the
  entry was `[~]` with Interior/Exterior never executed; spec 056 implemented
  them.)*

- [x] **Ray Tracing** — `CRaytracer` hider plus the RSL `trace`, `gather`,
  `occlusion` and environment/shadow families. See the manual's *Raytracing
  in SL*. One parameter gap is listed below (trace subsets).

- [x] **Global Illumination** — photon-map pass (`CPhotonHider`), point-based
  occlusion and colour bleeding, and irradiance caching. Accuracy of the
  irradiance cache is a known open issue (DEVNOTES *Open Issues*).

- [x] **Area Light Sources** — `AreaLightSource` binds the geometry created in
  its attribute block to the light (spec 057). The light shader runs once per
  sample, 0-D, on a point drawn uniformly by area over the light's emitters,
  with the light-surface globals (`P`, `N`, `Ng`, `dPdu`, `dPdv`, `u`, `v`,
  `s`, `t`) of that point and the lit point's `Ps`; the renderer weights what
  it returns by exactly `V / (n p_A)` (`V` the binary occlusion probe, `n` the
  sample count, `p_A` the density by area) and adds no emitter cosine or
  distance falloff: the shader owns both. Reyes, zbuffer and raytrace hiders,
  interpreter and JIT, bit-identical at any thread count. Emitters: polygons,
  polygon meshes and subdivision surfaces; the seven quadrics (exact for the
  sphere, disk, cylinder and cone under a rotation, translation and one
  uniform scale, a 20 x 20 table otherwise); bilinear patches (exact when
  planar), bicubic patches, patch meshes and NURBS patches (a trimmed patch's
  cells are weighted by their accepted fraction and every drawn point is
  tested with the surface's own trim: the area is approximate along a trim
  edge); the leaves of procedurals, delayed archives, instances and CSG solids
  (a CSG leaf emits whole, without its solid's region). A bound light emits
  photons from points of its emitters. Controls: `Option`/`Attribute
  "arealight"` `"int samples"` (default 16) and `"string mode"` (`full`,
  `fast`, `off`). Only objects with `Attribute "visibility" "int
  transmission" [1]` shadow a bound light. See `RIB_GUIDE.md`, the site manual
  page "Area Lights" and `specs/057-area-light-geometry/`.
  **Not built:** points, curves, implicit surfaces and shader operands as
  emitters (each reports once per light and stays ordinary geometry; a
  blobby emits as the polygon mesh `RiBlobby` derives from it); two-sided emission (the shipped `arealight` emits into the side of
  `N` only); displacement of an emitter (it is sampled undisplaced); volume
  attenuation of the shadow probe; extending network rendering (a few scenes
  rendered over `-P:2` matched the local render byte for byte; the cases were
  not enumerated). A block with no emitting geometry contributes nothing.
  `quadlight.sl` and `spherelight.sl` are deprecated in favour of this.
  *(Corrected 2026-11-21: this entry was `[~]`: the statement was accepted
  and returned a handle, but the block's geometry was not bound to the light
  and it behaved as an ordinary light; spec 057 implemented the binding. The
  old source line citation is gone: the function is named, not numbered.)*

---

## Per-request gaps

Not §1.1.2 capabilities. Each is a specific statement, parameter or primitive.

- [ ] **Trace subsets.** `trace()` and the ray-tracing built-ins do not filter
  by the `subset` parameter. (The `subset` handling at
  `rendererContext.cpp:6017` is `RiResource`'s, an unrelated use of the same
  word.)

- [ ] **OpenEXR texture input.** EXR *output* is supported by the display
  plugin; the texture system cannot read EXR back in.

- [ ] **Patch crack stitching.** Currently mitigated through displacement
  bounds rather than stitched.

- [x] **Blobby implicit surfaces (`RiBlobby`, §5.6).** All four primitive-field
  opcodes — 1000 constant, 1001 ellipsoid, 1002 segment (a genuine convolution,
  so abutting segments sum correctly), 1003 repelling ground plane — and all
  eight combining opcodes with analytic gradients. Per-blob primvar blending,
  `mpoint` reference space, `Attribute "blobby" "float tolerance"`,
  `Option "blobby" "string opcodeorder"`, RIB round-trip, two-sample motion
  blur. The surface is derived once at `RiBlobby` time into a `CPolygonMesh`
  handed to `addObject()`, so there is no hider-specific blobby code and CSG
  operand use and instancing came for free. **Note the spec's Table 5.3 and
  PRMan Application Note #31 contradict each other on opcodes 4 and 5;**
  openRender follows the spec (4 = subtract), which the note's own `dent.rib`
  figure confirms is what the shipping renderer did. See
  [BLOBBY_SURFACES.md](BLOBBY_SURFACES.md) and the manual's *Blobby Implicit
  Surfaces*.

- [x] **NURBS trim curves (`RiTrimCurve`, §5.1.3).** Attribute-scoped trim-loop
  state (`rendererContext.cpp:4269`) consumed by `CNURBSPatchMesh::create()`;
  one shared odd-winding classification test applied at both the REYES
  (`CPatch::dice()`) and ray-trace (`CTesselationPatch`) tessellation paths, so
  the two hiders cannot disagree about which side is trimmed. Adds the
  non-standard `Attribute "trimcurve" "string sense"` for inside/outside
  inversion. Round-trips through `CRibOut`. Trim Curves is one of the
  `##CapabilitiesNeeded` names in §7 even though it is not a §1.1.2 capability.
  See `specs/009-nurbs-trim-curves/` and the manual's *NURBS Trim Curves*.

- [x] **Subdivision surfaces (`RiSubdivisionMesh`, §5.4).** Not an optional
  capability — a required primitive, and complete. Catmull-Clark and Loop
  schemes (`rendererContext.cpp:5505`); the `hole`, `crease`, `corner`,
  `interpolateboundary`, `facevaryinginterpolateboundary`,
  `facevaryingpropagatecorners` and `creasemethod` tags, with an unknown tag
  diagnosed rather than ignored (`subdivisionCreator.cpp:2059`);
  `RiHierarchicalSubdivisionMesh[V]` with per-face overrides. Cross-hider
  motion blur with no hider-specific subdivision code. One documented
  architectural limitation: a hierarchical override at level > 0 is validated
  but has no rendering effect, because deeper levels are evaluated as a
  closed-form limit surface rather than materialized as meshes. See
  [SUBDIVISION_SURFACES.md](SUBDIVISION_SURFACES.md) and the manual's
  *Subdivision Surfaces*.

- [x] **Imager shaders (`RiImager`).** All seven spec variables, thread-safe,
  and in the spec's pipeline order (Exposure → Imager → Quantize). See
  [OSHADER_UPDATES.md](OSHADER_UPDATES.md) and
  `specs/005-imager-shader-support/`.

- [x] **Ray-traced motion blur.** Verified 2026-08 (spec 008 Phase 8/US6):
  `CRaytracer`'s tessellation-path intersection kernels already interpolated
  geometry on the ray's shutter time; seven cross-hider parity scenes confirm
  convergence with the REYES hider. Listed here because it was long recorded
  as a gap. See [HIDER_PARITY.md](HIDER_PARITY.md).

---

## How to re-check this file

Statuses rot silently, and three of the entries above were wrong for a year
because nobody re-read the code behind them. Each entry cites the file and
line that decides it, so the check is mechanical:

- A `[ ]` that names a stub should still be a stub. `RiBlobby` and
  `RiSolidBegin` were both recorded as returning `CODE_INCAPABLE` /
  `CODE_OPTIONAL` at line numbers that had not stubbed anything for months.
- A `[~]` should say which half works. "Partial" without that is not a status.
- A **rationale** is a claim too. "Unimplemented because X" is worth
  re-checking when X changes — the volume-shading entry blamed missing CSG,
  which was never the reason (and the entry is now implemented, spec 056).

Last verified against the source: **2026-08-29** (branch
`015-blobby-implicit-surfaces`); the Volume Shading entry **2026-11-10**
(branch `056-volume-shader-segments`); the Area Light Sources entry
**2026-11-21** (branch `057-area-light-geometry`).
