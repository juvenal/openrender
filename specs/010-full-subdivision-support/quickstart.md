# Quickstart: Validating Full Subdivision Surface Support

This is a validation/run guide, not implementation code. It assumes the build already succeeds
(`cmake --build build --config Release`) and follows this project's standard run invocation.

## Prerequisites

```bash
export SHADERS="$(pwd)/openrender/shaders"
export ORENDERHOME="$(pwd)/openrender"
export DISPLAYS="$(pwd)/openrender/displays"
export GEOMETRIES="$(pwd)/openrender/geometry"
```

## Step 0 — Capture baselines FIRST (TDD-mandatory, Constitution Principle III)

Before any fix lands, capture what exists today so each fix has a before/after comparison:

1. Render `geometry/killeroo.rib` (wrapped in a full scene, if not already) on both hiders — this is the existing
   466-call `hole`/`interpolateboundary` regression coverage identified in research.md R8. Save as the pre-existing
   baseline; it must be bit-for-bit unaffected by every tier below except where a fix specifically targets a bug it
   happens to also exercise (it does not exercise facevarying, motion, or any new tag, per R8).
   ```bash
   build/src/orender/orender geometry/killeroo.rib
   ```
2. Author (but do not yet fix) a facevarying-seam test scene with a vertex shared by ≥3 faces carrying visibly
   distinct per-corner UV values (User Story 2's Independent Test) — render it against **current** `master` first
   and confirm the seam is visibly wrong (collapsed to one value), proving the bug reproduces before the fix lands.
   ```bash
   build/src/orender/orender examples/rib/tests/subdiv-facevarying-seam-raytrace.rib
   ```

## Step 1 — Cross-hider motion blur verification (User Story 1, FR-001/FR-002, P1)

Per research.md R1/R2, this is pure verification — no renderer code changes are expected. Confirm the existing
generic mechanism (`CTesselationPatch::sampleTesselation()`/`intersect()`, `surface.cpp:1393-1513,1164-1319`)
already covers subdivision surfaces the same way it covers patches/quadrics:

1. Author and render a subdivision-surface translate scene on both hiders:
   ```bash
   build/src/orender/orender examples/rib/tests/parity/motion-subdiv-translate-reyes.rib
   build/src/orender/orender examples/rib/tests/parity/motion-subdiv-translate-raytrace.rib
   ```
2. Author and render a subdivision-surface rotate scene on both hiders (exercises `CSubdivision::sample()`'s
   two-time-sample path, `subdivision.cpp:149-188`):
   ```bash
   build/src/orender/orender examples/rib/tests/parity/motion-subdiv-rotate-reyes.rib
   build/src/orender/orender examples/rib/tests/parity/motion-subdiv-rotate-raytrace.rib
   ```
3. Confirm both pairs agree within the existing block-average parity threshold (`add_parity_test`, mirroring the
   9 existing motion-parity registrations at `tests/visual/CMakeLists.txt:609-679`).
4. Add the `HIDER_PARITY.md` subdivision-surfaces section documenting this as closed (R1/R2), including the
   7-vs-9 scene/registration-count note.

## Step 2 — Facevarying seam fix (User Story 2, FR-004, P1)

1. With the R3 fix landed (`CVertexFace.facevarying` slot + requesting-face-aware `computeVarying()`), re-render
   Step 0's facevarying scene:
   ```bash
   build/src/orender/orender examples/rib/tests/subdiv-facevarying-seam-raytrace.rib
   ```
2. Confirm the seam now shows distinct per-corner values instead of one collapsed value (Acceptance Scenario 1).
3. Render a single-incident-face-per-vertex control scene (or reuse `geometry/killeroo.rib`, which has zero
   facevarying data) and confirm it is bit-for-bit unaffected (Acceptance Scenario 3 — behavior for vertices with
   only one incident face, or no facevarying data at all, is unchanged).
4. Cross-hider check: render the seam scene on REYES too, and confirm parity within threshold.
   ```bash
   build/src/orender/orender examples/rib/tests/subdiv-facevarying-seam-reyes.rib
   ```

## Step 3 — New subdivision tags (User Story 3, FR-005, P1)

1. Render a scene using all three new tags (`facevaryinginterpolateboundary`, `facevaryingpropagatecorners`,
   `creasemethod`) together with existing tags, confirming no `CODE_BADTOKEN` error (Acceptance Scenario 1):
   ```bash
   build/src/orender/orender examples/rib/tests/subdiv-new-tags-raytrace.rib
   ```
2. Render a scene supplying an out-of-range value for one of the three new tags, confirming a diagnostic naming
   the tag and value is emitted, and the mesh still renders using a documented fallback (Acceptance Scenario 2).
3. Render a scene combining a new tag with the existing `hole`/`interpolateboundary` tags already exercised at
   scale by `geometry/killeroo.rib` (R8), confirming interaction correctness, not just new-tag-in-isolation
   correctness.

## Step 4 — Crease-quality reproduction (User Story 4, FR-006, P2)

1. Author and render the crease-convergence scene (multiple crease edges of varying sharpness converging at one
   shared vertex) under the ray-tracing hider (shading ground truth, per FR-016):
   ```bash
   build/src/orender/orender examples/rib/tests/subdiv-crease-convergence-raytrace.rib
   ```
2. Compare visually against a lightly-creased control mesh of comparable complexity; record in
   `HIDER_PARITY.md`'s subdivision-surfaces section whether a visible artifact or a noticeably slower render is
   observed (the qualitative bar fixed by spec.md's Clarifications — no numeric threshold).
3. If reproduced and root-caused: document the fix and whether it shares a root cause with Step 2's facevarying
   fix (research.md R5 flags this as a hypothesis to test, not a conclusion). If reproduced-but-deferred or
   not-reproduced: document the outcome and rationale. Either outcome satisfies User Story 4 per its explicit gate.

## Step 5 — Hierarchical subdivision edits (User Story 5, FR-007/FR-008/FR-009, P3)

Per `contracts/hierarchical-subdivision-contract.md`'s seven layers:

1. Render a base mesh with a per-face, per-level tag override via the new `RiHierarchicalSubdivisionMesh`:
   ```bash
   build/src/orender/orender examples/rib/tests/subdiv-hierarchical-override-raytrace.rib
   ```
2. Confirm the override's effect is visible only at its targeted face/level, and the base mesh's own default tags
   are otherwise unaffected (Acceptance Scenario 1).
3. Render a scene with an override targeting a nonexistent face/level; confirm the mesh still renders (base mesh
   intact) and exactly one diagnostic names the invalid override (FR-009) — not a hard failure of the whole
   primitive.
4. RIB round-trip (Layer 5):
   ```bash
   build/src/orender/orender -writerib out-hierarchical.rib examples/rib/tests/subdiv-hierarchical-override-raytrace.rib
   grep -A5 HierarchicalSubdivisionMesh out-hierarchical.rib   # confirm an equivalent statement is present
   ```
5. Cross-hider parity: render the same scene on REYES and confirm parity within threshold (SC-005).
6. Preview sanity (Layer 6, base-topology-only): load the RIB in `orender-wire` and confirm it parses and draws
   the base mesh wireframe without crashing (override visualization is explicitly out of scope for the preview
   layer, per `contracts/hierarchical-subdivision-contract.md`).

## Step 6 — Loop subdivision scheme (User Story 6, FR-010/FR-011, P4)

1. Render an all-triangle mesh with `scheme="loop"`, confirming it dices/tessellates and shades correctly on both
   hiders (Independent Test: "renders a smooth limit surface with no crashes"):
   ```bash
   build/src/orender/orender examples/rib/tests/subdiv-loop-reyes.rib
   build/src/orender/orender examples/rib/tests/subdiv-loop-raytrace.rib
   ```
2. Render a mixed triangle/non-triangle mesh with `scheme="loop"`; confirm it is rejected with a diagnostic
   identifying the mesh as unsuitable, not a crash or silently-degenerate geometry (Edge Cases).
3. Confirm `scheme="catmullclark"` scenes (all existing coverage, including `geometry/killeroo.rib`) are
   bit-for-bit unaffected by the new scheme-dispatch branch (SC-007's "no regression to existing capability").

## Step 7 — Hider-invariant regression check (FR-012/FR-013, SC-008)

Run the grep from `contracts/hider-invariant-contract.md` after every tier above lands, and again at the end:

```bash
grep -rln 'CSubdiv\|CLoopSubdiv\|CHierarchical' \
  src/ri/stochastic.cpp src/ri/reyes.cpp src/ri/zbuffer.cpp \
  src/ri/raytracer.cpp src/ri/trace.cpp src/ri/photon.cpp src/ri/show.cpp
```

Zero output required. (`src/preview/libribpreview/previewContext.cpp:92`'s existing `dynamic_cast<CSubdivMesh *>`
is intentionally excluded — it is preview-tool-side, not a hider; see research.md R6/`contracts/
hierarchical-subdivision-contract.md`.)

## Step 8 — Full regression sweep (SC-002 through SC-009)

```bash
ctest --test-dir build -L visual --output-on-failure   # all existing scenes plus every new scene above must pass
ctest --test-dir build -L libshader --output-on-failure
```

`CShow`-targeting scenes authored for this feature (per spec.md's Clarifications and Edge Cases) are expected to
be registered but are **not** required to pass — `CShow` itself is a separate, pre-existing non-functional gap
this feature does not fix. Photon-hider motion-blur scenes are similarly authored-not-required, matching the
`CShow` treatment; every other photon-hider capability (facevarying, new tags, hierarchical edits, Loop scheme)
must still pass under the photon hider like any other capability.
