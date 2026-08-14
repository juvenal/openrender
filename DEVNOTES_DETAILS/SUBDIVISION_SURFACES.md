# Subdivision Surfaces

Cross-hider parity/status document for Catmull-Clark and Loop subdivision
surface support, tracking the same information the geometry-type-specific
sections of [HIDER_PARITY.md](HIDER_PARITY.md) track for other primitive
types, split into its own file because of the size of the spec
`010-full-subdivision-support` work (P0-P4 across six user stories). See
also [GEOMETRY_STATEMENT.md](GEOMETRY_STATEMENT.md) for the unrelated
`Geometry` RIB statement, and [BUGS.md](BUGS.md) for a pre-existing
subdivision boundary-tag defect found (not fixed) during this work.

## Overview

`CSubdivMesh : CObject` / `CSubdivision : CSurface` already satisfy the
generic `CObject`/`CSurface` virtual-dispatch contract (`src/ri/object.h:
60-144`) by construction — no hider file contains a subdivision-specific
branch, and this feature's own regression check (FR-013,
`contracts/hider-invariant-contract.md`) verifies that stays true. This
document tracks cross-hider parity/status for subdivision-surface
capability specifically, the same way `HIDER_PARITY.md`'s sections track it
for the shading pipeline generally.

Spec: `specs/010-full-subdivision-support/`.

## Motion Blur

**Closed — User Story 1 (P1), verification-only.** `CSubdivMesh : CObject` /
`CSubdivision : CSurface` reach the raytracer's tessellation path
(`CTesselationPatch::sampleTesselation()`/`intersect()`,
`surface.cpp:1393-1513,1164-1319`) through the same generic virtual dispatch
as `CPatchMesh`/`CBicubicPatch`, and object (non-camera) motion blur across
REYES and raytrace was already closed for that shared path (see
`HIDER_PARITY.md`'s Object/Surface Motion Blur entry). No new renderer code
was needed to extend that to subdivision surfaces (research.md R1/R2) —
this section documents the confirming evidence, not a fix.

Evidence is the 9 pre-existing non-subdivision `add_parity_test` entries at
`tests/visual/CMakeLists.txt:609-679` (`motion-patches-translate`,
`motion-patches-translate-correlated`, `motion-patches-deform`,
`motion-polygons-translate`, `motion-polygons-deform`,
`motion-quadrics-translate`, `motion-quadrics-translate-correlated`,
`motion-quadrics-deform`, `dof-motion`) — which already prove the mechanism
is generic across primitive types — **plus** this feature's 2 new
subdivision-specific entries registered immediately after them:

- `motion-subdiv-translate` (transform-motion via a `MotionBegin`-wrapped
  `Translate` around a `SubdivisionMesh`) — measured ~4.48 max block-avg
  diff, well within the standard threshold of 20, consistent with the six
  non-deform scenes above.
- `motion-subdiv-rotate` (transform-motion via a `MotionBegin`-wrapped 75°
  `Rotate`, exercising `CSubdivision::sample()`'s two-time-sample eigen-basis
  path, `subdivision.cpp:149-188`) — measured ~37.8-38.3 max block-avg diff
  across repeated runs, confirmed not an undersampling artifact (doubling
  `PixelSamples` to 8x8 moved the measurement by <0.5). A genuinely rotating
  Catmull-Clark limit surface sweeps more silhouette per shutter than the
  translate/deform cases, producing a larger (but still bounded, legitimate)
  REYES-dicing-vs-raytrace-tessellation antialiasing residual. Registered at
  threshold 45 (~1.2x measured), following the same margin convention as
  `dof-motion`'s threshold of 40.

This is 11 total registrations proving the mechanism generically, not a
"7-vs-9" or subdivision-only split — the 9 pre-existing scenes already
closed the generic case; the 2 new ones extend coverage to subdivision
surfaces specifically.

Per FR-017, subdivision motion blur under the photon hider is explicitly
*not* extended by this feature (the photon hider has no motion-aware
tracing path) — `parity/motion-subdiv-translate-photon.rib` documents this
boundary as an authored-but-not-required `"not-required"`-labeled scene
(`tests/visual/CMakeLists.txt:989-990`) rather than a gap: it runs to
completion (exit 0) but is not asserted against a reference image.

## Facevarying

**Closed — User Story 2 (P1).** Before this fix, `CSVertex` stored one
`facevarying` pointer per topological vertex, and the per-face assignment
loop in `create()` (`subdivisionCreator.cpp`) overwrote that single pointer
once per incident face — a vertex shared by N faces silently kept only the
last-processed face's corner value, collapsing UV-seam discontinuities that
facevarying data exists to represent.

The fix replaces the single pointer with a per-incident-face record: `CSVertex`'s
private `CVertexFace` struct (`subdivisionCreator.cpp:122-127`) now carries
its own `facevarying` slot alongside the `face`/`next` links, and
`CSVertex::setFacevarying(CSFace *face, float *fv)`
(`subdivisionCreator.cpp:223-232`) assigns a value to the specific
`CVertexFace` record matching that face, rather than to the vertex as a
whole. The call site (`subdivisionCreator.cpp:2163`,
`faces[i]->vertices[j]->setFacevarying(faces[i], data.facevaryingData +
(k+j)*data.facevaryingSize)`) runs once per face-corner as before, but now
each corner's value survives independently.

`CSVertex::computeVarying()` (`subdivisionCreator.cpp:1317-1414`) does the
seam classification: a vertex is a genuine facevarying seam only if two
incident `CVertexFace` records carry *distinct* non-NULL pointers
(`subdivisionCreator.cpp:1338`); non-seam vertices (all incident corners
sharing one value) still take the cheap single-value path. At a real seam,
resolution follows the `facevaryinginterpolateboundary` mode
(`fvarBoundaryMode`, see New Tags below) — mode 2 ("edges and corners")
returns the exact incident value for the requesting face's own corner
(`subdivisionCreator.cpp:1378-1379`) instead of averaging it away; other
modes fall through to averaging every distinct incident value
(`subdivisionCreator.cpp:1384-1414`). Facevarying assignment must run before
`computeVarying()`'s seam detection can see two distinct incident values —
ordering note preserved in the comment at `subdivisionCreator.cpp:2144-2150`.

Verified with a shared-vertex regression scene where adjacent faces
disagree on their per-corner UV at a common vertex — `subdiv-facevarying-seam-{reyes,raytrace}.rib`
(`add_visual_test`, `tests/visual/CMakeLists.txt:723-731`) plus the
`subdiv-facevarying-seam` `add_parity_test` cross-checking REYES against
raytrace within the standard block-average threshold. All three pass. Per
FR-014, `subdiv-facevarying-seam-photon.rib` extends this to the photon
hider as a required-to-pass scene (two-`FrameBegin` RIB: photon emission/
storage against the seam mesh, then a raytrace pass reusing the raytrace
scene's reference TIFF) — passes.

## New Tags

**Closed — User Story 3 (P1).** Three subdivision tags previously hit
`error(CODE_BADTOKEN, ...)` as unrecognized (`subdivisionCreator.cpp`'s tag
dispatch, pre-existing default branch) because no case existed for them.
All three are now declared as `RtToken` constants in `ri.h` and parsed in
`CSubdivMesh::create()`'s tag loop (`subdivisionCreator.cpp:2045-2059`),
each defaulting safely (with a `warning(CODE_BADTOKEN, ...)`, not a hard
error) on a malformed argument count/type rather than aborting the mesh:

- **`facevaryinginterpolateboundary`** — one integer in `[0,2]`, stored as
  `data.fvarBoundaryMode` (default `2`, "edges and corners": preserve every
  distinct facevarying corner value). Consulted by
  `CSVertex::computeVarying()`'s seam-resolution logic described above.
- **`facevaryingpropagatecorners`** — one boolean, stored as
  `data.fvarPropagateCorners` (default `0`). Only consulted when
  `fvarBoundaryMode == 1`, per `subdivisionCreator.cpp:81-85`.
- **`creasemethod`** — one integer (`0` = normal/uniform sharpness decay,
  `1` = chaikin/neighbor-weighted decay), stored as `data.creaseMethod`
  (`subdivisionCreator.cpp:1569-1576`).

Both `facevaryinginterpolateboundary`/`facevaryingpropagatecorners` and
`creasemethod` are also overridable per-face via the hierarchical-edit
mechanism (see Hierarchical Edits below) — `fvarBoundaryModeOverride`,
`fvarPropagateCornersOverride`, `creaseMethodOverride` on `CSFace`, each
defaulting to `-1` ("no override, use the mesh-wide default").

Coverage: `subdiv-new-tags-raytrace.rib` (all three tags together on one
mesh), `subdiv-tag-facevaryinginterpolateboundary-raytrace.rib`,
`subdiv-tag-facevaryingpropagatecorners-raytrace.rib`,
`subdiv-tag-creasemethod-raytrace.rib` (one tag isolated per scene),
`subdiv-new-tags-badvalue-raytrace.rib` (malformed-argument fallback path),
and `subdiv-new-tags-with-hole-raytrace.rib` (interaction with the
pre-existing `hole` tag) — `tests/visual/CMakeLists.txt:745-778`. All six
pass. Per FR-014, `subdiv-new-tags-photon.rib` extends this to the photon
hider as a required-to-pass scene (`tests/visual/CMakeLists.txt:944-947`) —
passes.

## Crease Quality

**Not reproduced.** `DEVNOTES.md`'s Open Issues section lists two open,
previously-unverified reports — "Efficient subdivision surface creases"
(performance) and "Subdivision highly creased surface issues" (visual
quality) — with no existing test scene. Per FR-006/research.md R5, no fix
was committed sight-unseen; instead four raytrace test scenes isolate
crease sharpness magnitude from crease-convergence count on the same base
topology (a 3x3-face grid with one displaced interior vertex), so the two
axes of the original reports can be told apart:

- `subdiv-crease-convergence-control-raytrace.rib` — one edge, sharpness 1.5
- `subdiv-crease-shallow-convergence-raytrace.rib` — three edges converging
  at one vertex, sharpness 1.5 each
- `subdiv-crease-deep-single-raytrace.rib` — one edge, sharpness 12.0
- `subdiv-crease-convergence-raytrace.rib` — three edges converging at one
  vertex, sharpness 6.0/9.0/12.0 (the original reproduction attempt)

**Performance**: `/usr/bin/time -l` across repeated runs of all four scenes
showed ~0.5s user CPU and 22-24MB peak RSS uniformly, with no scene an
outlier — including the highest-convergence/highest-sharpness case. A single
earlier 11.5s measurement did not reproduce and is attributed to run-to-run
noise (thermal/scheduling), not a crease-count or sharpness effect. Not
reproduced.

**Visual quality**: qualitative inspection of the convergence scene initially
suggested a dark facet distinct from the control. Two independent checks
overturned that reading:
1. A diagnostic shader visualizing `abs(normalize(N))` as RGB (bypassing
   lighting entirely) returned finite, well-formed, near-identical colors at
   the flagged coordinates across all sharpness configurations tested
   (1.5/1.5/1.5, 6.0/9.0/12.0, 12.0/12.0/12.0 uniform) — ruling out a
   degenerate/NaN shading normal.
2. A full-frame (320x240) per-pixel diff between the convergence and
   shallow-convergence renders gave a mean diff of 0.33/765 — the images are
   effectively identical overall — and the flagged "dark facet" region reads
   nearly identically dim in **all four scenes, including the single-crease
   control**, meaning it is ordinary Lambertian falloff on this mesh/light
   rig, not a crease-specific artifact.

The only genuine localized divergence found (full-frame diff) is a single
brighter spot at ~(145-146, 108) where the high-sharpness scenes are
*brighter* than the low-sharpness ones (diff magnitude 170-194) — consistent
with a sharp crease producing a tighter geometric fold that catches the key
light differently than a smoothly-rounded low-sharpness surface, i.e. the
crease mechanism working as intended, not a defect.

**Conclusion (T031)**: not reproduced; no fix landed. This is a documented
negative result, not a deferral — deferral implies a confirmed defect being
punted, and no defect was confirmed. `CSVertex::compute()`'s `numSharp > 2`
corner-freeze branch (the leading hypothesis going in) was not implicated by
any of the above evidence and was left unchanged. Method note for future
crease/quality investigations in this codebase: qualitative visual inspection
alone produced a false positive here; a control scene plus a full-frame
pixel diff is what actually discriminated signal from ordinary shading
falloff.

## Hierarchical Edits

**Closed — User Story 5 (P3).** `RiHierarchicalSubdivisionMesh[V]`
(per-face, per-level tag overrides layered on top of a base
`SubdivisionMesh`) is a new, structurally distinct RenderMan interface call
— not a variant argument bolted onto `RiSubdivisionMeshV` — per RISpec and
per `contracts/hierarchical-subdivision-contract.md`. It lands across seven
parallel integration layers, mirroring how spec 009's
`contracts/attribute-contract.md` documented the four-layer pattern a new
*attribute* must satisfy, scaled up for a new *primitive*:

1. **RIB grammar/lexer** — new `RIB_HIERARCHICAL_SUBDIVISION_MESH` token
   (`rib.l:119`) and a new grammar production (`rib.y:413-439,2497-2579`)
   parsing the nested per-face/per-level override argument shape; not an
   alternative folded into the existing `RIB_SUBDIVISION_MESH` rule.
2. **RI entry point** — `RiHierarchicalSubdivisionMesh[V]` declared in
   `ri.h:659-660`, registered in `ri.cpp`/`riInterface.{h,cpp}`, parallel in
   shape to `RiSubdivisionMeshV`.
3. **Renderer implementation** — `CRendererContext::RiHierarchicalSubdivisionMeshV`
   (`rendererContext.cpp:5414-5452`), parallel to `RiSubdivisionMeshV` at
   `rendererContext.cpp:5348`. Parses the base mesh plus the override list
   and stores the overrides for the geometry layer; does not resolve them
   itself.
4. **Override resolution (geometry layer, FR-008)** — `subdivisionHierarchical.{h,cpp}`:
   `CHierarchicalOverride` (`subdivisionHierarchical.h:37-43`) is a
   singly-linked `(faceIndex, level, tagName, value)` node layered on top of
   a base `CSubdivMesh`'s default tags, never replacing the base mesh's own
   tag storage. Per-face override fields
   (`fvarBoundaryModeOverride`/`fvarPropagateCornersOverride`/`creaseMethodOverride`
   on `CSFace`, `subdivisionCreator.cpp:388-407`, each defaulting to `-1` =
   "no override") are consulted ahead of the mesh-wide default at the exact
   points those tags are read — e.g. `CSVertex::computeVarying()`'s
   `fvarBoundaryMode`/`fvarPropagateCorners` lookup
   (`subdivisionCreator.cpp:1357-1362`) and the crease-method dispatch
   (`subdivisionCreator.cpp:1569-1576`) — rather than being pre-resolved
   into a flattened copy of the base mesh. An override targeting a
   `(face, level)` pair absent from the base mesh is invalid (FR-009) and is
   skipped individually with a diagnostic, matching spec 009's fail-small
   precedent (one bad trim loop rejected, not the whole `NuPatch`) rather
   than rejecting the entire hierarchical primitive; an override targeting a
   `(face, level)` that exists but isn't reached at a given render's
   effective subdivision depth is not an error — it simply has no visible
   effect for that render.
5. **RIB output round-trip** — a new `CRibOut::RiHierarchicalSubdivisionMeshV`
   serializer (`ribOut.cpp`), parallel to the existing `RiSubdivisionMeshV`
   serializer, so a hierarchical mesh written to RIB and re-parsed
   reproduces the same base mesh + override list.
6. **Preview/wireframe viewer** — a new `RiHierarchicalSubdivisionMeshV`
   handler in `src/preview/libribpreview/ribGeometryContext.{h,cpp}`,
   parallel to the existing `RiSubdivisionMeshV` handling, drawing the base
   mesh's topology only (override resolution stays out of the preview
   layer). `previewContext.cpp:92`'s pre-existing `dynamic_cast<CSubdivMesh
   *>` lives under `src/preview/`, not a hider file, and remains the
   explicitly out-of-scope, legitimate exception `contracts/hider-invariant-contract.md`'s
   FR-013 grep check already excludes.
7. **Scripting bindings** — a new `Ri:HierarchicalSubdivisionMesh` Lua
   binding (`prman.lua:568,573`), following `Ri:SubdivisionMesh`'s existing
   argument-marshalling shape (Lua-only, matching `RiSubdivisionMeshV`
   itself, which has no Python binding either).

No hider file references a hierarchical-edit type or resolution function —
resolved geometry reaches every hider through the same generic
`CObject`/`CSurface` dispatch every other primitive uses, confirmed by the
FR-013 regression check (T060, zero matches).

Coverage: `subdiv-hierarchical-override-raytrace.rib` (base mesh with
per-face/per-level tag overrides applied), `subdiv-hierarchical-override-invalid-raytrace.rib`
(an override targeting a nonexistent face/level, exercising the
skip-individually-with-diagnostic path), `subdiv-hierarchical-tag-override-precedence-raytrace.rib`
(confirms a per-face override wins over the mesh-wide default) —
`tests/visual/CMakeLists.txt:823-840` — plus the `subdiv-hierarchical-override`
`add_parity_test` (`:851`) cross-checking REYES against raytrace. All four
pass. Per FR-014, `subdiv-hierarchical-override-photon.rib` extends this to
the photon hider as a required-to-pass scene
(`tests/visual/CMakeLists.txt:949-952`) — passes.

## Loop Scheme

**Closed — User Story 6 (P4).** Before this feature, `RiSubdivisionMeshV`
rejected any `scheme` other than `"catmullclark"` outright
(`error(CODE_INCAPABLE, "Unknown subdivision scheme: %s\n", scheme)` in
`rendererContext.cpp`). Loop is now implemented as a second scheme,
dispatched at mesh-creation time in `rendererContext.cpp` alongside the
existing Catmull-Clark path, on an all-triangle-mesh precondition Loop
requires and Catmull-Clark does not.

`CLoopSubdivMesh` (`subdivisionLoop.h`, `subdivisionLoop.cpp`, 473 lines)
takes a materially different integration shape than `CSubdivMesh`'s
eigenbasis-based Catmull-Clark limit surface (research.md R7): Loop is
implemented as iterative/uniform mask-based refinement — no
extraordinary-vertex eigenbasis evaluation — and the refined triangle mesh
is wrapped in a plain `CPolygonMesh` child (`subdivisionLoop.h:25-29`), so
every hider reaches it through the *same* generic dispatch any other
polygon mesh already uses. There is no Loop-specific dicing or
ray-intersection code anywhere below `CLoopSubdivMesh`: it implements only
the `CObject` contract (`intersect()`/`dice()`/`instantiate()`/`moving()`)
as a container, exactly as `CSubdivMesh` does — the actual per-leaf
`sample()`/`interpolate()` CSurface hooks are supplied by the
`CPolygonTriangle` leaves the `CPolygonMesh` child produces, not by
`CLoopSubdivMesh` itself. This keeps the scheme choice entirely inside the
geometry layer, satisfying the same "never leak into hider code" constraint
Catmull-Clark already satisfies (verified by the unchanged FR-013 grep
check, T060).

Two defects were found and fixed while landing this scheme:

- A framebuffer/shader-reuse stabilization bug (member-field shadowing
  affecting bounding-box state) surfaced during Loop scheme testing and was
  fixed alongside the algorithm (commit `3a859bb`).
- Malformed-input rejection (`CLoopSubdivMesh::create()`,
  `subdivisionLoop.cpp:388,407` — a non-all-triangle mesh, or a mesh with
  no vertex-class parameters) originally used `error(CODE_RANGE, ...)`
  before falling back to an empty mesh via `loopMakeEmptyFallback()`. Since
  `error()`-severity diagnostics set `RiLastError`, and `orender`'s `main()`
  fails the whole process (`orender.cpp:916`,
  `return (RiLastError != RIE_NOERROR) ? -1 : 0;`) whenever `RiLastError` was
  ever set during the run — independent of whether the render itself
  otherwise completed and produced correct output — this caused a process
  exit code of 255 despite the mesh being correctly rejected and replaced by
  its documented empty-mesh fallback. Changed both sites to
  `warning(CODE_RANGE, ...)`, matching the established convention already
  used throughout `subdivisionCreator.cpp` for this exact class of
  recoverable subdivision-input validation (bad tag values, invalid
  hierarchical-override targets, degenerate/trivial mesh detection — all use
  `warning()`, never `error()`, specifically so the render still succeeds
  with a diagnostic rather than aborting).

Coverage: `subdiv-loop-reyes.rib`/`subdiv-loop-raytrace.rib` (a valid
all-triangle Loop mesh, REYES and raytrace) and
`subdiv-loop-mixed-invalid-raytrace.rib` (a mesh mixing a quad face with
triangle faces, exercising the malformed-input rejection/fallback path
above — confirmed exit code 0 with the diagnostic still printed) —
`tests/visual/CMakeLists.txt:863-879`. All three pass, and the full
71-scene visual suite plus 25-scene parity suite were re-run after landing
Loop scheme to confirm zero regressions to any pre-existing Catmull-Clark
or other-primitive-type scene. Per FR-014, `subdiv-loop-photon.rib` extends
this to the photon hider as a required-to-pass scene
(`tests/visual/CMakeLists.txt:954-957`) — passes; the visual suite now
stands at 75 scenes total (see the CShow note below).

## Debug/Visualization Hider (CShow) and Remaining Photon Coverage

**Authored, not a gate — matches spec.md's Edge Cases.** Per the plan-mode
brief and spec.md, the `CShow` (`oshow:`-prefixed) hider is a pre-existing,
documented non-functional gap in this codebase (the legacy GUI/OpenGL
module it depended on was removed — see repository root `CLAUDE.md`'s
"REMOVED" note on `src/gui`) that this feature does not fix. One
`CShow`-targeting scene was authored per user story (US1-US6, six scenes
total under `examples/rib/tests/`, plus `parity/motion-subdiv-translate-oshow.rib`
for US1) and registered under a new `"not-required"` ctest label
(`add_not_required_test` macro, `tests/visual/CMakeLists.txt:169-203` — no
prior "authored-not-required" convention existed to copy, so this feature
introduces it). Direct execution of all seven confirms deterministic,
expected failure: exit code 255, stderr `"Opengl wrapper not found..."` —
not a hang or an ambiguous crash, consistent with the documented gap.

The same `"not-required"` label also carries `parity/motion-subdiv-translate-photon.rib`
(FR-017's photon-motion-blur exception, see Motion Blur above) — that scene
differs from the CShow scenes in that it *does* run to completion (exit 0);
it is unasserted against a reference image only because photon+motion
correctness is explicitly out of scope, not because it fails to run.

Together with the four required-to-pass photon scenes noted inline above
(facevarying, new tags, hierarchical overrides, Loop), this closes FR-014
(every fixed/new capability gets photon-hider coverage where the capability
applies) and the CShow deliverable from spec.md's Edge Cases, without
promoting CShow itself to a merge gate.

## Regression invariant

`contracts/hider-invariant-contract.md` (FR-013) is a grep-verifiable check
that no file under `src/ri/{stochastic,reyes,zbuffer,raytracer,trace,photon,
show}.cpp` references any subdivision-specific type (`CSubdivMesh`,
`CSubdivision`, `CLoopSubdivMesh`, `CHierarchicalOverride`, `CSFace`,
`CSVertex`, `CSEdge`). Re-run after every tier above landed (T060); zero
matches each time. The one legitimate, explicitly-excluded exception is
`src/preview/libribpreview/previewContext.cpp:92`'s `dynamic_cast<CSubdivMesh
*>` — the preview/wireframe viewer is not a hider.

## Known pre-existing, unrelated defect

`CSFace::create()` (`src/ri/subdivisionCreator.cpp:512`, ~line 540)
silently produces zero geometry for any mesh where every face touches a
boundary vertex, unless `interpolateboundary` is tagged; a secondary effect
is `CSubdivMesh::children` staying NULL, causing `create()` to re-run its
full topology-build/tag-processing body on every subsequent
`intersect()`/`dice()` call for that object's lifetime. Discovered during
this feature's US2 test authoring (T047), predates it, and is independent
of the hierarchical-override work — worked around in the affected test
scene rather than fixed at the source. Logged in
[BUGS.md](BUGS.md).

## Explicitly out of scope

`CSubdivMesh::dice()` duplicates `CObject::dice()`'s loop instead of
calling it (`subdivisionCreator.cpp:1627-1642`) — pre-existing, purely
cosmetic, no behavior change. Left untouched per spec.md's Out of Scope
section; do not fold into a future subdivision change without a separate
justification.

## Future Work

- **Pixar OpenSubdiv.** The maintainer intends to study Pixar's
  [OpenSubdiv](https://github.com/PixarAnimationStudios/OpenSubdiv) project
  in a future session — **not** as a dependency to import or link against,
  but as a reference to selectively pick algorithms/techniques from and
  adapt into openRender's own from-scratch implementation. Candidate areas:
  optimizing the Loop scheme's iterative mask-based subdivision
  (`subdivisionLoop.cpp`) and improving RISpec-standard conformance of the
  Catmull-Clark eigenbasis limit-surface evaluation (`subdivision.cpp`),
  particularly around crease/boundary handling. Any adapted technique gets
  reimplemented in openRender's own idiom, not vendored or wrapped, and must
  preserve this feature's hard architectural constraint: the subdivision
  algorithm stays entirely inside the geometry layer (`CObject`/`CSurface`
  contract), with zero hider-specific code. Not started; no code has been
  evaluated or adapted yet.
- Crease-quality issues (see Crease Quality above) remain unreproduced.
  Should a genuine defect surface later (e.g. under different mesh
  topology, sharpness values, or lighting), re-open with a new control +
  full-frame-diff test scene following the method note above, rather than
  reasoning from qualitative visual inspection alone.
