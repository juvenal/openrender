# Quickstart: Reyes/Raytrace Hider Parity Convergence

Validation guide for the Story 1 parity harness — the deliverable that must
land and pass before any R1-R4/S1-S5 refactor is considered complete. This
does not include implementation code; see `contracts/` for interface shapes
and `data-model.md` for entity definitions.

## Prerequisites

- Built renderer: `cmake --build build --config Release`
- Env vars set (per repo CLAUDE.md):
  ```bash
  export SHADERS="$(pwd)/openrender/shaders"
  export ORENDERHOME="$(pwd)/openrender"
  export DISPLAYS="$(pwd)/openrender/displays"
  export GEOMETRIES="$(pwd)/openrender/geometry"
  ```
- `ctest` available (bundled with the CMake build)

## Running the existing suites (baseline, before any change)

```bash
# Full existing visual-regression suite — must stay green throughout this feature
ctest --test-dir build -L visual --output-on-failure

# Existing lens-sampling regression gate — FR-008 requires this keep passing
# unmodified as R2/S1 land. (test_radial_histogram is a manual diagnostic
# binary from spec 007, not wired into ctest — see tests/visual/CMakeLists.txt;
# it is not part of this automated gate.)
ctest --test-dir build -R DiskSampling --output-on-failure
```

## Running the new parity harness (once Story 1 lands)

```bash
# New -L parity label, added alongside the existing -L visual/-L libshader labels
ctest --test-dir build -L parity --output-on-failure
```

Each parity test:
1. Renders one Parity scene pair's `rib_reyes` and `rib_raytrace` variants
   via `orender` (using the `test_hider_parity` driver, which duplicates
   `test_visual_render.cpp`'s TIFF read/diff code — see
   `contracts/` and `research.md`'s Test-infrastructure section).
2. Diffs the two fresh outputs against each other with the scene's
   `effect_tag`'s configured `max_block_diff` (data-model.md entity 2).
3. Reports PASS/FAIL per scene pair with no manual image inspection
   required (SC-001).

## Manually rendering one parity scene pair (for debugging a failure)

```bash
build/src/orender/orender examples/rib/tests/parity/<scene>-reyes.rib
build/src/orender/orender examples/rib/tests/parity/<scene>-raytrace.rib
# Compare the two output TIFs with the same tool ctest uses:
build/tests/visual/test_hider_parity <output-a>.tif <output-b>.tif <threshold>
```

## Validating a specific refactor lands cleanly

After each of R1/R2/R3/R4/S1-S5 (per the audit's recommended order,
research.md and plan.md Summary):

```bash
# 1. Full visual-regression suite must not regress (FR-024), except for the
#    documented S2 displacement-default reference regeneration
ctest --test-dir build -L visual --output-on-failure

# 2. Full parity suite must not regress
ctest --test-dir build -L parity --output-on-failure

# 3. Lens-sampling gate (R2/S1 specifically)
ctest --test-dir build -R DiskSampling --output-on-failure

# 4. Performance regression guard (FR-030/SC-007) — manual timing comparison,
#    both hiders, both scenes, before/after this specific change:
time build/src/orender/orender examples/rib/camera-dof.rib
time build/src/orender/orender examples/rib/tests/motion-1-reyes.rib
time build/src/orender/orender examples/rib/tests/motion-1-raytrace.rib
```

## Expected outcomes at feature completion

- `ctest -L visual`: 100% pass (SC-005), except scenes whose reference was
  intentionally regenerated for the S2 displacement-default change.
- `ctest -L parity`: 100% pass for flat-shading/matte/depth-filter scenes
  (SC-002) and for dof/motion/transparency scenes within their
  residual-adjusted thresholds (SC-003).
- `ctest -R DiskSampling`: 100% pass, unmodified pass/fail intent (FR-008).
- `DEVNOTES_DETAILS/HIDER_PARITY.md`'s Alignment Status checklist: all items
  checked (Motion Blur Implementation, Shading Interpolation & Derivatives
  documented-residual note, Displacement Parity, Transparency Handling).
- After Option B lands: re-run `ctest -L parity` with at least one threshold
  tightened (FR-026/SC-008) and confirm zero new failures against
  unmodified code.
