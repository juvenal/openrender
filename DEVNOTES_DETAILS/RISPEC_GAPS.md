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
there), `[ ]` absent. Every status below was checked against the source at the
date in the footer, not carried forward.

---

## §1.1.2 Advanced Capabilities

**7 of 11 implemented, 2 partial, 2 absent.**

- [x] **Solid Modeling** — `RiSolidBegin`/`RiSolidEnd`
  (`rendererContext.cpp:5738`). All three set operations plus `"primitive"`;
  arbitrarily nested trees (`src/ri/csgTree.{h,cpp}`) resolved by interval
  boolean classification over ray spans (`src/ri/csgBoolean.{h,cpp}`).
  Operands may be any primitive that reaches `addObject()`, which includes
  quadrics, polygon meshes, NURBS, subdivision meshes and blobbies — all four
  combinations have scenes. Malformed input (an unmatched `SolidEnd`, a
  nesting or procedural inside `"primitive"`, an unknown operation) is
  diagnosed rather than crashed. See `specs/013-solid-csg-operations/` and
  the manual's *Solid CSG Operations*.

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
  sampling shared by both camera hiders (spec 007). D9, the DOF *occlusion*
  model, is a permanent documented residual between the hiders rather than an
  open item — see [HIDER_PARITY.md](HIDER_PARITY.md).

- [ ] **Special Camera Projections** — `RiProjectionV`
  (`rendererContext.cpp:960`) accepts `"perspective"` and `"orthographic"`
  only; anything else is `CODE_BADTOKEN`. No spherical, Omnimax, or
  user-supplied projection. Note the spec's default when no `Projection`
  statement appears is **orthographic**, and openRender follows it
  (`ribGeometryContext.h:151`) — a frequent source of "my render is empty".

- [x] **Displacements** — `RiDisplacement`, with displacement bounds. On by
  default for the ray-trace hider too, opt-out via
  `Attribute "trace" "int displacements" [0]`.

- [ ] **Spectral Colors** — **parsed and discarded.** `RiColorSamples`
  validates `N`, stores `nColorComps` and the two conversion matrices on
  `COptions` (`rendererContext.cpp:1407`), and `CRenderer` copies
  `nColorComps` at `beginFrame` (`renderer.cpp:401`). Nothing else in the
  renderer or the shading engine reads it: color is three components
  throughout. The statement is accepted silently, so a scene using it renders
  as if it had not — worth a diagnostic it does not currently emit.

- [~] **Volume Shading** — `RiAtmosphere` works: the shader is attached to
  `CAttributes` and executed per shading point
  (`libshader/shading/shading.cpp:1049`, skipped for non-camera rays).
  `RiInterior` and `RiExterior` are **parsed, stored on `CAttributes`
  (`rendererContext.cpp:2114`/`2131`), and never executed** — no site in the
  shading pipeline references either. Implementing them needs ray-segment
  integration between surface hits, which the ray-trace hider's span
  machinery could now carry.
  *(Corrected 2026-08-29: this was previously recorded as blocked on missing
  CSG support. That was never the reason, and CSG now exists.)*

- [x] **Ray Tracing** — `CRaytracer` hider plus the RSL `trace`, `gather`,
  `occlusion` and environment/shadow families. See the manual's *Raytracing
  in SL*. One parameter gap is listed below (trace subsets).

- [x] **Global Illumination** — photon-map pass (`CPhotonHider`), point-based
  occlusion and colour bleeding, and irradiance caching. Accuracy of the
  irradiance cache is a known open issue (DEVNOTES *Open Issues*).

- [~] **Area Light Sources** — `RiAreaLightSourceV`
  (`rendererContext.cpp:2045`) is accepted and returns a light handle that
  `RiIlluminate` can switch, but its body is otherwise identical to
  `RiLightSourceV`: the shader is added to the attribute state and the
  geometry emitted inside the block is **not** bound to the light. An area
  light therefore behaves as an ordinary light source whose emitting geometry
  is rendered as ordinary geometry. Nothing is dropped and nothing errors,
  which is why this reads as working until the soft shadow does not appear.

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
  which was never the reason and is now doubly false.

Last verified against the source: **2026-08-29** (branch
`015-blobby-implicit-surfaces`).
