# Research: Full Subdivision Surface Support

**Feature**: `010-full-subdivision-support` | **Date**: 2026-08-11 | **Spec**: [spec.md](./spec.md)

This document resolves every open technical question surfaced by `spec.md`'s Clarifications, Edge Cases, and
Assumptions before Phase 1 design. Unlike spec 009 (which needed a *new* shared abstraction because trim
classification could otherwise duplicate across two tessellation paths), most of this feature's work is a
correctness/completeness fix at the geometry layer — `CSubdivMesh : CObject` and `CSubdivision : CSurface` already
satisfy the hider-agnostic virtual-dispatch contract (confirmed: `src/ri/object.h:60-144` declares
`CObject::intersect()`/`dice()`/`instantiate()` and `CSurface::moving()`/`sample()`/`interpolate()`/`shade()` as the
complete contract every primitive — patches, quadrics, subdivision surfaces — already implements identically; no new
seam is required). Each entry below follows spec 009's Decision/Rationale/Alternatives-considered format.

## R1: Cross-hider motion blur — verification scope and evidence (User Story 1, FR-001/FR-002)

**Decision**: Treat User Story 1 as pure verification + documentation, not construction. Add cross-hider motion-blur
parity test scenes for subdivision surfaces (translate + rotate, REYES and ray-tracing) and a `HIDER_PARITY.md`
subdivision-surfaces section, per FR-018/SC-009. No renderer code changes are needed to satisfy FR-001/FR-002.

**Rationale**: The mechanism already exists, generically, at the shared ray-tracing/hider layer:
- `CTesselationPatch::sampleTesselation()` (`src/ri/surface.cpp:1393-1513`) samples any wrapped `CObject` twice —
  `context->displace(..., PARAMETER_BEGIN_SAMPLE)` at shutter time 0.0 and `PARAMETER_END_SAMPLE` at 1.0 — whenever
  `moving()` returns true, and `CTesselationPatch::intersect()` (`surface.cpp:1164-1319`) LERPs between the two
  stored tessellation grids at `cRay->time`.
- This is reached purely through `object.cpp:533-574`'s virtual dispatch — no primitive-type branch. Any primitive
  with `moving() == true` gets motion blur for free, including subdivision surfaces: `CSubdivision::moving()`
  (`subdivision.h:45`) returns `vertexData->moving`, and `CSubdivision::sample()` (`subdivision.cpp:149-188`)
  already selects the correct doubled-time-sample vertex buffer via the `PARAMETER_BEGIN_SAMPLE`/`PARAMETER_END_SAMPLE`
  flags passed to it by the same generic caller.
- `DEVNOTES_DETAILS/HIDER_PARITY.md:10` (D10, closed 2026-08) already documents this exact mechanism as verified for
  patches/polygons/quadrics via 7 named cross-hider parity scenes (`Parity_motion-{patches,polygons,quadrics}-
  {translate,deform}` plus `Parity_dof-motion`), registered as **9** ctest targets in
  `tests/visual/CMakeLists.txt` — the discrepancy between "7" (D10's prose, counting distinct scene *definitions*)
  and "9" (the actual registration count) is explained precisely: two of the seven scenes
  (`motion-patches-translate`, `motion-quadrics-translate`, lines 621/656) also register a `-correlated` ctest
  variant exercising `VISUAL_ENV_CORRELATED` (the unified sample-table option from spec 008/US9), which is an
  additional *registration* of the same underlying scene pair under a different sample-correlation setting, not a
  distinct eighth/ninth scene. Both counts are true depending on what is being counted; this document states both
  precisely rather than picking one.
- All 7 scenes passed with **no code changes required in the intersection kernels** (`HIDER_PARITY.md:10`) — direct
  precedent that a new primitive type reaching this same pathway needs no new code either, only new test coverage.

**Alternatives considered**:
- *Build a subdivision-specific motion-blur code path in `raytracer.cpp`* — rejected outright: this would violate
  FR-003/FR-012 (no hider file may special-case an effect or a primitive type) and duplicate a mechanism that
  already works, for no benefit.
- *Skip test coverage and declare FR-001 satisfied by code inspection alone* — rejected: FR-001 explicitly requires
  "verified, via dedicated test scenes," and `HIDER_PARITY.md`'s own D10 entry has zero scenes naming subdivision
  surfaces specifically today (confirmed by grep: no `subdiv` token appears anywhere in `HIDER_PARITY.md`).

## R2: Camera-rotation SLERP vs. ray-tracing primary rays (Edge Cases, FR-002's "research question")

**Decision**: No new work is required for this feature. Document as a closed verification note in `HIDER_PARITY.md`'s
subdivision-surfaces section: `Visual_camera-motion-small-dof-raytrace` already exists and already passes in the
current visual suite, and `HIDER_PARITY.md`'s own "Possible Optimization" section already documents a working
camera-motion lerp path in the ray-tracer. Object-authored rotation motion (the case User Story 1/FR-002 actually
scopes) is unaffected by this question entirely, since it goes through the two-time-sample tessellation LERP
described in R1, not the camera-transform SLERP path.

**Rationale**: Spec.md's Edge Cases section is explicit that this is "an open verification question addressed in
research.md, not a requirement this feature commits to sight-unseen." The two mechanisms are architecturally
distinct: `CRenderer::cameraHasRotation`-gated SLERP (`CLAUDE.md` Known Gotcha #7, `slerpq()` in
`common/mathSpec.h`) interpolates the *camera-to-world transform itself*, while User Story 1's scope is *object*
motion (a primitive moving within camera space). A scene combining both (a rotating camera photographing a
translating/rotating subdivision surface) exercises two independent, already-working mechanisms simultaneously; no
interaction bug is hypothesized or has been reported, so this feature does not need to build a combined test for it
beyond noting the existing `Visual_camera-motion-small-dof-raytrace` scene as an existence proof that the camera
side already works on the ray-tracing hider.

**Alternatives considered**:
- *Build a dedicated combined-motion (camera rotation + subdivision object motion) test scene* — rejected as
  out-of-scope scope creep: no requirement in spec.md asks for this, and Assumptions explicitly limits User Story
  1 to verifying the existing mechanism, not exploring untested combinations no one has reported a problem with.
- *Leave the question fully open with no finding* — rejected: the Edge Case explicitly asks research.md to address
  it, and the existing passing scene plus documented lerp path are sufficient evidence to close it without new code.

## R3: Facevarying pointer-collapse fix design (User Story 2, FR-004)

**Decision**: Extend `CSVertex`'s existing per-incident-face bookkeeping (`CVertexFace`, `subdivisionCreator.cpp:
104-109`, already a linked list of `{CSFace *face; CVertexFace *next;}` nodes built during topology construction) to
carry its own `float *facevarying` slot, populated once per `(face, corner)` pair in the existing assignment loop
(`subdivisionCreator.cpp:1858-1860`) — which already has both `faces[i]` and the corner index `j` in scope, so no
new parameter needs to be threaded in *at that specific site*. `CSVertex::computeVarying()` (`subdivisionCreator.cpp:
1237-1252`) gains a requesting-face parameter to select the matching `CVertexFace` node's value instead of reading
the single collapsed `this->facevarying` pointer.

**Rationale**: Read the actual bug and its full consumer chain directly (not inferred):
- **Bug site**: `subdivisionCreator.cpp:1858-1860` — `faces[i]->vertices[j]->facevarying = data.facevaryingData +
  (k + j) * data.facevaryingSize;` executes once per `(face i, corner j)`, and because `CSVertex` stores a single
  `float *facevarying` field (declared `subdivisionCreator.cpp:157`, initialized `NULL` at `subdivisionCreator.cpp:
  143`), a vertex shared by N incident faces keeps only the last-processed face's value.
- **Read/consumer chain**: `CSVertex::computeVarying()` (`1237-1252`) is the leaf case of a three-way recursive
  dispatch (`CSVertex`/`CSEdge`/`CSFace::computeVarying()`, at `1237`, `1387`, `1433` respectively) that evaluates
  facevarying values for newly-synthesized limit-surface control points during recursive subdivision.
  `CSFace::computeVarying()` (`1433-1467`) already has a natural face-context (`this`) at the exact point it calls
  `vertices[j]->computeVarying(varying1, facevarying1)` (`1450`) — this is the natural place to pass a requesting-
  face identity down without inventing a new parameter-threading path.
- **Where original (non-synthesized) vertices are consumed as differently-cornered control points**: in the
  "ordinary patch" branch of `create()`'s tessellation-splitting pass (`subdivisionCreator.cpp:614-616`), the
  original, unsplit `CSVertex` objects at valence-4 ordinary vertices are used directly as one corner of each
  incident face's own `CBicubicPatch` control grid (e.g. `v[1*4+1] = vertices[(extraordinary+0)&3];`) — the *same*
  `CSVertex` object is reused as a *different* corner across each incident face's separately-gathered grid. This is
  exactly where the collapsed single-pointer value is wrong today, and exactly why the fix must be keyed by
  requesting face, not by vertex alone.
- The 9 `gatherData()` call sites (`subdivisionCreator.cpp:569, 614, 669, 708, 714, 720, 742, 752`) were read in
  full surrounding context. All 9 operate on the tessellation-splitting pass over *already-synthesized* geometry
  (irregular-patch grids, B-spline strips, extraordinary-vertex rings) — several calls removed from the original
  per-face RIB input, gathering data for freshly-created `CSubdivision`/`CBicubicPatch`/`CPatchGrid`/
  `CBSplinePatchGrid` sub-primitives. None of them is the right place to thread a "requesting face" parameter: by
  the time execution reaches `gatherData()`, the routine has already collected a `CSVertex **` ring via
  `vertices[k]->sort(ring, edges[k], this, N)` calls (e.g. `subdivisionCreator.cpp:614` and siblings) where `this`
  (the enclosing `CSFace`) is *already* passed into `sort()` — face identity is available one call-frame earlier
  than `gatherData()`, at the `sort()` call sites, which is where the original design mistakenly assumed carrying
  it further was unnecessary. The fix therefore threads face identity through the existing `sort()`-call sites and
  into `computeVarying()`'s recursive chain; `gatherData()` itself needs no signature change.
- `CSVertex`, `CVertexFace` already exist as the natural per-face storage key (`subdivisionCreator.cpp:104-113`)
  — no new bulk data structure needs to be invented.

**Alternatives considered**:
- *Store a `float **facevaryingPerFace` array on `CSVertex`, indexed 0..`fvalence`-1 in the same order as the
  `faces` linked list* — functionally equivalent to extending `CVertexFace` itself, but requires an extra parallel
  index to stay in sync with list order across `split()`/`sort()`, whereas attaching the slot directly to the
  existing `CVertexFace` node keeps face-and-value co-located. Rejected in favor of the simpler, already-structured
  approach.
- *Duplicate `CSVertex` objects per incident face (never share one topological-vertex object across faces)* —
  rejected: this would break `valence`/`fvalence`/`sort()`'s half-edge adjacency logic that crease/extraordinary-
  vertex detection and every other consumer of `CSVertex` depend on being one object per topological vertex, a far
  larger and riskier change than the targeted fix above.
- *Fix only at `gatherData()`'s 9 call sites by threading a new parameter through each* — this was the original
  working hypothesis before reading the call sites in full context; rejected once the read confirmed those 9 sites
  operate on synthesized sub-primitives already several steps removed from original face identity, making them the
  wrong layer for the fix (face identity is available strictly upstream of them, at the `sort()` calls and in
  `CSFace::computeVarying()`).
- This fix is entirely confined to `subdivisionCreator.cpp`/`subdivisionCreator.h` (the geometry layer) — no hider
  file is touched, consistent with FR-012.

## R4: New subdivision tags (User Story 3, FR-005)

**Decision**: Add `RI_FACEVARYINGINTERPOLATEBOUNDARY`, `RI_FACEVARYINGPROPAGATECORNERS`, and `RI_CREASEMETHOD` as
new token constants alongside the four existing tags, and add matching dispatch arms in the tag-recognition chain
in `subdivisionCreator.cpp`'s `create()` (the same function containing the `FACE_INTEPOLATEBOUNDARY` flag handling
at lines 1692-1720 and the existing tag-parsing loop feeding it), replacing three of today's fall-through paths to
`error(CODE_BADTOKEN, ...)` with tag-specific value validation and flag storage on `CSubdivData`.

**Rationale**: Confirmed by direct read: `RI_HOLE`, `RI_CREASE`, `RI_CORNER`, `RI_INTERPOLATEBOUNDARY` are declared
as the complete existing tag vocabulary (`src/ri/ri.h:231-234`, defined `src/ri/ri.cpp:160-163`), and any tag
outside this set reaches a hard `CODE_BADTOKEN` in the tag dispatch chain in `subdivisionCreator.cpp`. This is a
pure geometry-layer addition (new tokens + new dispatch arms + new `CSubdivData` flags consumed inside `create()`'s
existing crease/boundary logic) — no hider file, no RIB grammar change (tags already parse as a generic string
array per `rib.y`'s existing production), no RI-entry-point signature change. `facevaryinginterpolateboundary` and
`facevaryingpropagatecorners` interact directly with the R3 facevarying-per-corner fix (both control facevarying
boundary/corner behavior), so this tag work is sequenced after R3 lands, per spec.md grouping both as P1.

**Alternatives considered**:
- *Implement these as `CAttributes`-level attributes (the four-layer pattern from spec 009's `RI_SHADERFORMAT`
  precedent)* — rejected: unlike `shaderformat` (a per-primitive attribute independent of mesh topology), these are
  RISpec-defined `RiSubdivisionMesh` **tags** (parsed from the tag/args string array on the call itself, like
  `hole`/`crease`/`corner` already are), not push/pop attribute-block state. Following the existing tag precedent
  keeps the new tags consistent with the four they sit alongside, rather than introducing a second, inconsistent
  mechanism for conceptually identical RISpec constructs.

## R5: Crease-quality reproduction and root-cause approach (User Story 4, FR-006)

**Decision**: Build one new test scene — a subdivision mesh with multiple crease edges of varying sharpness values
converging at a single shared vertex — rendered under the ray-tracing hider (shading ground truth per FR-016).
Evaluate it against the qualitative bar fixed by the spec's Clarifications: a visible artifact, or a noticeably
slower render relative to a comparable lightly-creased control mesh — no numeric threshold. Document the outcome
(reproduced-and-fixed, reproduced-and-deferred, or not-reproduced) directly in `HIDER_PARITY.md`'s new subdivision-
surfaces section (R9) and, if a fix lands, note explicitly whether it shares a root cause with R3's facevarying fix.

**Rationale**: `DEVNOTES.md:42-43` lists both reports as open and unreproduced (`- [ ] Efficient subdivision surface
creases`, `- [ ] Subdivision highly creased surface issues`) — there is no existing scene, benchmark, or diagnostic
output to build on. The crease-sharpness-accumulation logic (`CSVertex::compute()`'s crease/corner rules, adjacent
to the facevarying code read for R3 in the same file) is a plausible shared root cause with R3 only in the sense
that both are per-vertex accumulation bugs in the same class hierarchy — this is a hypothesis to test during
root-causing, not a conclusion; it is not asserted here. Per spec.md's explicit gating (User Story 4 "may conclude
with reproduced, root-caused, fix deferred with written rationale... acceptable"), this feature's Phase 1/2 planning
does not presuppose which outcome will occur.

**Alternatives considered**:
- *Commit to implementing a specific crease-quality fix now, based on hypothesized cause* — explicitly disallowed
  by spec.md's Clarifications ("no fix should be committed sight-unseen against an unreproduced report").
- *Use a numeric performance threshold (e.g. a hard wall-clock regression percentage) to define "reproduced"* —
  rejected per the spec's own Clarification: qualitative confirmation only, since no existing timing harness exists
  in this codebase to make a numeric bar meaningful project-wide.

## R6: Hierarchical subdivision mesh — integration surface (User Story 5, FR-007/FR-008/FR-009)

**Decision**: Introduce `RiHierarchicalSubdivisionMesh[V]` as a new, parallel entry point (not a variant argument
on the existing `RiSubdivisionMesh`), touching every layer `RiSubdivisionMesh` itself touches today, plus new
override-resolution logic confined to the geometry layer:

| Layer | File(s) | Change |
|---|---|---|
| RIB grammar/lexer | `src/ri/rib.y` (new production near `RIB_SUBDIVISION_MESH`, `rib.y:2390-2473`), `src/ri/rib.l:118` (new token, alongside `SubdivisionMesh`) | New `RIB_HIERARCHICAL_SUBDIVISION_MESH` token + grammar rule accepting per-face/per-level tag-override syntax |
| RI entry point | `src/ri/ri.h`/`ri.cpp` | New `RiHierarchicalSubdivisionMesh[V]` declaration + registration, parallel to the existing `RiSubdivisionMeshV` |
| Renderer implementation | `src/ri/rendererContext.cpp` (parallel to `RiSubdivisionMeshV` at `rendererContext.cpp:5348`) | New entry point body: parses base mesh + override list, stores overrides for the geometry layer to resolve |
| Override resolution | `src/ri/subdivisionCreator.cpp`/`subdivision.h` (or a new sibling file, e.g. `subdivisionHierarchical.{h,cpp}`) | Resolves per-face/per-level overrides against the base mesh's tag state at subdivision time; entirely geometry-layer, per FR-008 |
| RIB output round-trip | `src/ri/ribOut.cpp:1288,1304` (`CRibOut::RiSubdivisionMeshV`) | New parallel `CRibOut::RiHierarchicalSubdivisionMeshV` serializer |
| Preview/wireframe viewer | `src/preview/libribpreview/ribGeometryContext.cpp:687,706` / `.h:122` (`RiSubdivisionMeshV`), `previewContext.cpp:92` | New parallel `RiHierarchicalSubdivisionMeshV` in the preview geometry context; the override-resolution logic itself stays out of the preview layer (preview only needs base-mesh topology to draw a wireframe, not evaluated overrides) |
| Scripting bindings | `src/lua/prman.lua:568,573` (`Ri:SubdivisionMesh`) | New parallel `Ri:HierarchicalSubdivisionMesh` Lua binding; Python bindings if present follow the same pattern |

Invalid overrides (FR-009) are detected during resolution and skipped individually with a diagnostic naming the
affected face/level, matching spec 009's fail-small precedent for malformed per-element data (one bad trim loop
rejected, not the whole `NuPatch`) rather than rejecting the entire hierarchical primitive.

**Rationale**: Grammar confirmed empty for this primitive: `src/ri/rib.y` has exactly three `RIB_SUBDIVISION_MESH`
alternatives (full tags+params form at line ~2401, a tags-conditionally-zero-tags form at ~2426-2441, and a
`/* REMOVED for non-standard */`-commented dead form at ~2473) — none of them is, or could be repurposed as,
`RiHierarchicalSubdivisionMesh`, since the hierarchical primitive's override list is a structurally different
argument shape (nested per-face/per-level tag data), not a variant of the flat single-level tag/args form. A new
grammar production, not a new branch in the existing one, is required. `previewContext.cpp:92`'s existing
`dynamic_cast<CSubdivMesh *>` is a **preview-tool-side** downcast (`src/preview/`, not a hider under
`src/ri/{stochastic,reyes,zbuffer,raytracer,trace,photon,show}.cpp`) — it is a legitimate, pre-existing, out-of-
scope consideration for FR-012/FR-013's regression check, which is scoped specifically to hider files; this
distinction is called out explicitly here so FR-013's grep-based check is written to exclude `src/preview/` and not
misfire on it.

**Alternatives considered**:
- *Extend `RiSubdivisionMeshV`'s existing signature with optional override parameters instead of a new entry
  point* — rejected: RISpec defines `RiHierarchicalSubdivisionMesh` as its own distinct interface call with its own
  argument shape (nested per-level tag lists), not an optional-argument variant of `RiSubdivisionMesh`; conflating
  the two would produce a non-conformant, harder-to-round-trip RIB grammar.
- *Resolve overrides inside the preview layer as well (full override-aware preview rendering)* — rejected as scope
  creep beyond FR-007's ask; the preview tool draws wireframes from base topology today and this feature does not
  require it to visualize resolved per-level overrides, only to parse/round-trip the new primitive without crashing
  or misrendering the base mesh.

## R7: Loop subdivision scheme — integration depth (User Story 6, FR-010/FR-011)

**Decision**: Implement Loop subdivision as a second algorithm inside the geometry layer (new sibling source, e.g.
`subdivisionLoop.{h,cpp}`, alongside the existing Catmull-Clark `subdivisionCreator.{h,cpp}`/`subdivision.{h,cpp}`),
selected by the existing scheme-string dispatch, and reaching only the same integration depth Catmull-Clark already
has — REYES dicing via the existing `CObject::dice()`/`CSurface` contract, and the generic ray-tracing tessellation
fallback (`CTesselationPatch`, same as every other primitive, no Loop-specific ray-tracing code). This feature does
**not** build a Loop-specific build-time eigenbasis generator analogous to `src/precomp/precomp.cpp`'s
Catmull-Clark-specific, valence-indexed `CEigenBasis`/`basisData[]` (`K = 2*N+8` eigenstructure size, confirmed by
direct read of `precomp.cpp:1-40,260-330`); exact extraordinary-vertex limit evaluation for Loop is not required by
FR-010, which explicitly bounds Loop's scope to Catmull-Clark's existing integration depth, not additional
capability.

**Rationale**: `rendererContext.cpp:5364-5366` is confirmed as the sole rejection site — `error(CODE_INCAPABLE,
"Unknown subdivision scheme: %s\n", scheme);` inside `RiSubdivisionMeshV` — that must instead accept `"loop"` and
route to the new algorithm. Spec.md's own Assumptions section is explicit that Loop "only needs to reach the same
hider-integration depth Catmull-Clark already has... not additional features Catmull-Clark itself is still
missing," which directly resolves what would otherwise be a `NEEDS CLARIFICATION` on evaluation exactness: an
iterative/uniform Loop subdivision implementation (repeatedly applying Loop's edge/vertex smoothing masks per
dicing level, without a closed-form extraordinary-vertex eigenbasis) satisfies "smooth limit surface, no crashes"
(User Story 6's Independent Test) and "successfully dice/tessellate and shade" (SC-007) without needing
`precomp`-style exact-limit machinery. `CObject : public CSurface` provides `dice()`/`intersect()`/`sample()`/
`interpolate()` as the complete contract (`object.h:75-130`) Loop must implement identically to Catmull-Clark's
`CSubdivMesh`/`CSubdivision` — no hider file needs to know which scheme produced the geometry it's dicing/
intersecting.

**Alternatives considered**:
- *Build a Loop-specific eigenbasis generator mirroring `precomp.cpp`'s Catmull-Clark machinery* — rejected as
  disproportionate to FR-010's stated bar; this would be a substantially larger effort (a new numerical eigenbasis
  derivation, a new build-time data table, new precomputed constants) for a P4 feature whose acceptance criteria
  explicitly do not require exact limit-surface evaluation, only correct dicing/tessellation/shading.
- *Reuse Catmull-Clark's existing eigenbasis machinery for Loop by approximation* — rejected: Loop's subdivision
  masks and valence-dependent weights are mathematically distinct from Catmull-Clark's; approximating with the
  wrong basis would produce incorrect (not just different-by-design) limit surfaces, undermining the "smooth limit
  surface" acceptance bar rather than satisfying it.

## R8: `geometry/killeroo.rib` coverage — correction to spec.md's Assumption

**Decision**: Record a factual correction discovered during planning, without altering spec.md (which is frozen for
this feature per the spec-kit workflow's separation between specification and planning): `geometry/killeroo.rib`'s
466 `SubdivisionMesh "catmull-clark"` calls use tags `{"hole", "interpolateboundary"}` (confirmed via
`grep -oh '"\(hole\|crease\|corner\|interpolateboundary\)"' geometry/killeroo.rib | sort -u`), not "plain untagged
quads" as spec.md's Assumptions section states. It still has **zero** `facevarying` occurrences and **zero**
`MotionBegin` occurrences (also independently re-confirmed by grep), so the substance of the assumption — this file
provides no coverage for facevarying, motion blur, new tags beyond `hole`/`interpolateboundary`, hierarchical
overrides, or the Loop scheme — is unaffected and remains the basis for FR-014's "supplemented, not relied upon"
requirement. The correction only changes which *existing* tags already have implicit regression coverage via this
file: `hole` and `interpolateboundary` do (466 calls' worth); `crease`, `corner`, and all three new User-Story-3 tags
do not.

**Rationale**: Direct grep is authoritative over the spec's prose description, and Phase 1 test-scene planning
(`data-model.md`/`quickstart.md`) should not assume `hole`/`interpolateboundary` are completely uncovered when in
fact killeroo.rib already exercises them at scale — new test scenes for User Story 3 should focus their `hole`/
`interpolateboundary`-adjacent assertions on the *new* tags' interaction with the existing two, rather than treating
all five tags as equally novel.

**Alternatives considered**:
- *Silently treat the spec's assumption as correct and design test scenes as if killeroo.rib provides zero tag
  coverage at all* — rejected: this would risk redundant, lower-value test scenes duplicating existing coverage for
  `hole`/`interpolateboundary` instead of concentrating new-scene design on the tags/capabilities genuinely
  uncovered.

## R9: Test-scene and documentation conventions to reuse

**Decision**: Follow existing conventions exactly — no new test infrastructure is introduced.

**Rationale**: `add_visual_test` (`tests/visual/CMakeLists.txt:86`) and `add_parity_test` (`CMakeLists.txt:133`,
signature `SCENE_NAME RIB_A OUTPUT_A RIB_B OUTPUT_B [THRESHOLD] [ENV_LIST]`) are the two existing macros; new
subdivision-surface scenes register via whichever macro matches the comparison being made (single-hider regression
vs. cross-hider parity), following the `Visual_`/`Parity_` ctest-name prefix convention already used by every
existing entry (e.g. the 9 motion-parity registrations at lines 609-679).

**Alternatives considered**: None — reusing the existing macros/conventions is the only option consistent with
Constitution Principle V (minimal dependencies) and Principle I (consistent structure/formatting).
