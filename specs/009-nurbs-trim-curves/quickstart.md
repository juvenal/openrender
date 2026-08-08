# Quickstart: Validating NURBS Trim Curves (RiTrimCurve)

This is a validation/run guide, not implementation code. It assumes the build already succeeds
(`cmake --build build --config Release`) and follows this project's standard run invocation.

## Prerequisites

```bash
export SHADERS="$(pwd)/openrender/shaders"
export ORENDERHOME="$(pwd)/openrender"
export DISPLAYS="$(pwd)/openrender/displays"
export GEOMETRIES="$(pwd)/openrender/geometry"
```

## Step 0 — Capture the untrimmed baseline FIRST (User Story 4 / SC-002, TDD-mandatory)

This step MUST happen before any trim implementation code is written, on unmodified `master`, per Constitution
Principle III and the plan's Constitution Check.

1. Wrap `geometry/vase.rib`'s `NuPatch` body into a full renderable scene (`Display`/`WorldBegin`/camera) at
   `examples/rib/tests/nupatch-vase-untrimmed.rib` (see `research.md` R7).
2. Render it and save the output as the reference image:
   ```bash
   build/src/orender/orender examples/rib/tests/nupatch-vase-untrimmed.rib
   cp <output>.tif examples/rib/tests/references/nupatch-vase-untrimmed.tif
   ```
3. Register it in `tests/visual/CMakeLists.txt` via `add_visual_test(...)`, matching the existing ~85-entry
   pattern (`CMakeLists.txt:86`).
4. Confirm it passes trivially against itself:
   ```bash
   ctest --test-dir build -L visual -R nupatch-vase-untrimmed --output-on-failure
   ```

## Step 1 — Single-loop trim (User Story 1)

1. Render `examples/rib/tests/nupatch-vase-trimmed-hole.rib` — the same vase body preceded by a `TrimCurve` loop
   cutting a hole.
2. Visually confirm (or via `ctest -R nupatch-vase-trimmed-hole`) the trimmed region is absent and the rest of the
   surface matches the untrimmed baseline outside the trimmed region (Acceptance Scenario 2).
3. Cross-hider check (Acceptance Scenario/SC-008): render the same scene under both the reyes-family hider and
   `Hider "raytrace"`, and confirm both outputs agree within the existing visual-regression threshold.
   ```bash
   build/src/orender/orender examples/rib/tests/nupatch-vase-trimmed-hole.rib          # reyes (default)
   build/src/orender/orender examples/rib/tests/nupatch-vase-trimmed-hole-raytrace.rib # Hider "raytrace" variant
   ```

## Step 2 — Sense inversion (User Story 3)

1. Render `examples/rib/tests/nupatch-vase-trimmed-sense.rib` — same trim loop, but with
   `Attribute "trimcurve" "sense" ["outside"]` set beforehand.
2. Confirm the kept/discarded regions are exact complements of Step 1's result (SC-003).

## Step 3 — Multi-loop composition (User Story 5)

1. Render `examples/rib/tests/nupatch-vase-trimmed-multiloop.rib` — three loops: one outer hole plus two disjoint
   holes, or one nested island-within-a-hole variant.
2. Confirm all loops resolve independently/correctly per the odd-crossing-count rule (SC-005).

## Step 4 — Attribute scoping (User Story 2)

1. Render a scene with `AttributeBegin` / `TrimCurve` / `NuPatch` / `AttributeEnd` followed by a sibling `NuPatch`
   with no trim curve declared, and confirm the sibling renders fully untrimmed (SC-004).
2. Render a scene with a single `TrimCurve` followed by two consecutive `NuPatch` calls with no intervening
   attribute scope change, and confirm both surfaces are trimmed identically.

## Step 5 — Malformed-loop diagnostics (Edge Cases, FR-017/019/020)

1. Render a scene with an unclosed trim loop; confirm the renderer does not crash, implicitly closes the loop, and
   emits exactly one diagnostic warning naming the affected `NuPatch`/loop.
2. Render a scene with a `w <= 0` control point in a trim loop; confirm the loop is rejected (surface renders as if
   untrimmed for that loop) and exactly one diagnostic warning is emitted.
3. Reference the same malformed geometry via multiple `ObjectInstance` calls; confirm the warning still appears
   exactly once, not once per instance (FR-020).

## Step 6 — RIB round-trip (FR-014, SC-006)

```bash
build/src/orender/orender -writerib out.rib examples/rib/tests/nupatch-vase-trimmed-hole.rib
grep -A5 TrimCurve out.rib   # confirm an equivalent TrimCurve statement is present
```

## Step 7 — Full regression sweep (SC-002, SC-007)

```bash
ctest --test-dir build -L visual --output-on-failure   # all ~85+ existing scenes plus the new ones must pass
```

No timing harness is introduced by this feature; SC-007's "no measurable regression" is validated by observing that
`ctest -L visual` wall-clock time for the pre-existing (non-trim) scenes is unchanged within normal run-to-run
variance, since those scenes take the FR-004 fast path (a single "no trim state" check).
