# Contract: `RiHierarchicalSubdivisionMesh` Integration Surface (FR-007/FR-008/FR-009)

**Feature**: `010-full-subdivision-support`

Analogous to spec 009's `contracts/attribute-contract.md` (which documented the four-layer pattern a new attribute
must satisfy), this contract documents the seven-layer surface a new **primitive entry point** must satisfy —
`RiHierarchicalSubdivisionMesh[V]`, introduced as a new, parallel call alongside the existing `RiSubdivisionMesh`
per research.md R6, not a variant argument on it. Every layer below is new/parallel code; none of it may collapse
into `RiSubdivisionMeshV`'s existing implementation, since RISpec defines the two as structurally distinct calls.

## Layer 1 — RIB grammar/lexer

- **Files**: `src/ri/rib.y` (new production near `RIB_SUBDIVISION_MESH`, `rib.y:2390-2473`), `src/ri/rib.l:118`
  (new token, alongside `SubdivisionMesh`)
- **Contract**: a new `RIB_HIERARCHICAL_SUBDIVISION_MESH` token and a new grammar rule accepting the nested
  per-face/per-level tag-override argument shape. This is a **new production**, not a new alternative inside the
  existing `RIB_SUBDIVISION_MESH` rule — confirmed by direct read: `rib.y` has exactly three
  `RIB_SUBDIVISION_MESH` alternatives (full tags+params form at ~2401, tags-conditionally-zero-tags form at
  ~2426-2441, and a dead `/* REMOVED for non-standard */` form at ~2473), none of which is or could be repurposed,
  since the hierarchical primitive's override list is a structurally different argument shape (nested per-face,
  per-level tag data) than the flat single-level tag/args array `RiSubdivisionMesh` already parses.

## Layer 2 — RI entry point

- **Files**: `src/ri/ri.h`, `src/ri/ri.cpp`
- **Contract**: a new `RiHierarchicalSubdivisionMesh[V]` declaration and registration, parallel in shape to the
  existing `RiSubdivisionMeshV` declaration — same calling convention (token/args-array `V` variant), new symbol,
  no modification to `RiSubdivisionMeshV`'s own signature.

## Layer 3 — Renderer implementation

- **File**: `src/ri/rendererContext.cpp` (parallel to `RiSubdivisionMeshV` at `rendererContext.cpp:5348`)
- **Contract**: parses the base mesh (identical shape to `RiSubdivisionMeshV`'s own base-mesh parsing) plus the new
  override list, and stores the override list for the geometry layer to resolve. This function does **not**
  resolve overrides itself — resolution is Layer 4's job, kept out of the renderer-context layer to match how
  `RiSubdivisionMeshV` itself keeps tag interpretation inside `subdivisionCreator.cpp`'s `create()`, not inline in
  `rendererContext.cpp`.

## Layer 4 — Override resolution (geometry layer, FR-008)

- **Files**: `src/ri/subdivisionCreator.cpp`/`subdivision.h`, or a new sibling file (e.g.
  `subdivisionHierarchical.{h,cpp}`)
- **Contract**: resolves per-face/per-level overrides against the base mesh's tag state at subdivision-evaluation
  time. **This is the only layer permitted to contain override-resolution logic** — FR-008 requires it live
  entirely in the geometry layer, and `contracts/hider-invariant-contract.md` requires zero hider file ever touch
  it. An override targeting a `(face, level)` pair that does not exist on the base mesh is invalid (FR-009) and is
  skipped individually with a diagnostic naming the affected face/level — matching spec 009's fail-small precedent
  for malformed per-element data (one bad trim loop rejected, not the whole `NuPatch`) rather than rejecting the
  entire hierarchical primitive. An override targeting a `(face, level)` that exists but is never reached at a
  given render's effective subdivision depth is not an error (data-model.md's Hierarchical Subdivision Edit
  entity) — it simply has no visible effect for that render.

## Layer 5 — RIB output round-trip

- **File**: `src/ri/ribOut.cpp:1288,1304` (`CRibOut::RiSubdivisionMeshV`)
- **Contract**: a new, parallel `CRibOut::RiHierarchicalSubdivisionMeshV` serializer, so a hierarchical mesh
  written to RIB and re-parsed reproduces the same base mesh + override list (round-trip fidelity), matching the
  existing `RiSubdivisionMeshV` serializer's fidelity guarantee.

## Layer 6 — Preview/wireframe viewer

- **Files**: `src/preview/libribpreview/ribGeometryContext.cpp:687,706` / `.h:122` (existing `RiSubdivisionMeshV`
  handling), `previewContext.cpp:92`
- **Contract**: a new, parallel `RiHierarchicalSubdivisionMeshV` handler in the preview geometry context, parsing
  and drawing the **base mesh's topology only** — the preview tool does not need to visualize resolved per-level
  overrides, only to parse/round-trip the new primitive without crashing or misrendering the base mesh. Override
  resolution itself (Layer 4) stays entirely out of the preview layer.
- **Note on `previewContext.cpp:92`**: its existing `dynamic_cast<CSubdivMesh *>` is a **preview-tool-side**
  downcast, not a hider (it lives under `src/preview/`, not `src/ri/{stochastic,reyes,zbuffer,raytracer,trace,
  photon,show}.cpp`). It is a legitimate, pre-existing, out-of-scope consideration for
  `contracts/hider-invariant-contract.md`'s FR-013 regression check, which is scoped specifically to hider files —
  called out here explicitly so that check's grep is written to exclude `src/preview/` and does not misfire on it.

## Layer 7 — Scripting bindings

- **File**: `src/lua/prman.lua:568,573` (`Ri:SubdivisionMesh`)
- **Contract**: a new, parallel `Ri:HierarchicalSubdivisionMesh` Lua binding, following the existing
  `Ri:SubdivisionMesh` binding's argument-marshalling shape. Python bindings, if present in the checked-out tree,
  follow the same pattern; if absent, this layer is Lua-only (consistent with `RiSubdivisionMeshV` itself, which
  has no Python binding today either).

## What must NOT happen at any layer

- No layer may fold hierarchical-mesh handling into `RiSubdivisionMeshV`'s own code path via an optional-argument
  variant — RISpec defines `RiHierarchicalSubdivisionMesh` as its own distinct interface call with its own argument
  shape (nested per-level tag lists), not an optional-argument variant of `RiSubdivisionMesh`; conflating the two
  would produce a non-conformant, harder-to-round-trip RIB grammar (research.md R6, rejected alternative).
- No hider file (`stochastic.cpp`, `reyes.cpp`, `zbuffer.cpp`, `raytracer.cpp`, `trace.cpp`, `photon.cpp`,
  `show.cpp`) may reference a hierarchical-edit type or resolution function — the resolved mesh reaches every
  hider through the same `CObject`/`CSurface` dispatch every other primitive uses (`contracts/
  hider-invariant-contract.md`).
- The preview layer (Layer 6) must not grow full override-aware rendering — that is scope creep beyond FR-007's
  ask (research.md R6, rejected alternative); it draws wireframes from base topology today and this feature does
  not require more.

## Verification

- **Round-trip test**: author a hierarchical mesh via RIB, render, re-serialize via `ribOut`, re-parse, re-render —
  confirm visual parity within the existing block-average threshold between the two renders (quickstart.md).
- **Invalid-override test**: author an override targeting a nonexistent face/level — confirm the mesh still
  renders (base mesh intact) and a diagnostic is emitted naming the specific invalid override, not a hard failure
  of the whole primitive (FR-009, Acceptance Scenario coverage).
- **Cross-hider parity**: render the same hierarchical mesh on REYES and ray-tracing hiders — confirm parity within
  the same block-average threshold used elsewhere in the suite (SC-005).
