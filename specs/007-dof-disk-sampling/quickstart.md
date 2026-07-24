# Quickstart: Validating the DOF Lens-Sampling Fix

Prerequisites: repo built per `COMPILING.txt`/`INSTALL.md`; standard build directory `build/`.

## 1. Build

```bash
cmake --build build --config Release
```

## 2. Unit test: sampleDisk() (TDD Red → Green)

Before `sampleDisk()` exists, this test fails to build/link (Red). After it's implemented in
`src/ri/random.h`, it should pass (Green):

```bash
ctest --test-dir build -R DiskSampling --output-on-failure
```

Expected: all generated samples satisfy `x² + y² < 1` and are finite (no NaN/Inf), and the
`r²` distribution passes the area-uniformity check (see research.md §6).

## 3. REYES stability check (proves the refactor didn't alter REYES's output)

```bash
ctest --test-dir build -L visual --output-on-failure -R "camera-dof-reyes|camera-motion-small-dof-reyes"
```

Expected: both pass against their **existing, unmodified** reference images — zero diff
beyond the existing threshold. If either fails, the `stochastic.cpp` refactor did not
faithfully preserve REYES's algorithm/sequence and must be fixed before proceeding.

## 4. Raytrace fix visibility check (expected to fail against OLD references)

```bash
ctest --test-dir build -L visual --output-on-failure -R "camera-dof-raytrace|camera-motion-small-dof-raytrace"
```

Expected: **fails** against the pre-fix reference images at this point — that failure is the
evidence the center-bias fix actually changed the raytracer's output. (If these pass
unchanged, the fix didn't take effect.)

## 5. Regenerate raytrace references, cross-checked against REYES

```bash
# Render both fixed scenes with each hider
SHADERS="$(pwd)/openrender/shaders" ORENDERHOME="$(pwd)/openrender" \
DISPLAYS="$(pwd)/openrender/displays" GEOMETRIES="$(pwd)/openrender/geometry" \
build/src/orender/orender examples/rib/tests/camera-dof-raytrace.rib
build/src/orender/orender examples/rib/tests/camera-dof-reyes.rib

# Cross-check the new raytrace candidate's radial energy distribution against REYES's
# converged output for the same scene (FR-006 / Clarification Q1)
build/tests/visual/test_radial_histogram \
    camera-dof-raytrace.tif camera-dof-reyes.tif \
    --center <x> <y> --radius <r> --bins 16

# Repeat for camera-motion-small+dof-{raytrace,reyes}.rib
```

Expected: the tool reports both curves' `energy/annulus_area` matching within ±20% per bin
(excluding the innermost, noise-dominated bin) — flat, not center-peaked. Only once this passes, copy the new raytrace TIFs into
`examples/rib/tests/references/`, replacing the two stale (buggy-baseline) files.

## 6. Full regression suite

```bash
ctest --test-dir build -L visual --output-on-failure
```

Expected: 100% pass, including both raytrace DOF scenes against their newly-committed
references (SC-003).

## 7. Distribution-shape sanity check (direct visual confirmation of SC-001)

```bash
build/tests/visual/test_radial_histogram camera-dof-raytrace.tif --center <x> <y> --radius <r> --bins 16
```

Expected: `energy/annulus_area` roughly constant across bins (flat curve), not decaying with
radius (which would indicate the pre-fix center-bias is still present).

## 8. Performance check (SC-005: ≤1% regression)

```bash
time build/src/orender/orender examples/rib/tests/camera-dof-raytrace.rib
```

Compare against a pre-fix timing baseline (e.g. `git stash` back to the prior commit and
re-time, or a recorded baseline). Expected: render time within 1% of the pre-fix baseline.

## 9. Documentation

Confirm `DEVNOTES_DETAILS/HIDER_PARITY.md` has a new `[x]` entry for DOF lens-sampling parity
(FR-008), replacing the previously-absent/gap state.
