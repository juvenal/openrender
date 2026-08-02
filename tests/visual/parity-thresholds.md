# Cross-hider parity test thresholds

Rationale for the per-scene thresholds registered via `add_parity_test()` in
`tests/visual/CMakeLists.txt` (spec `008-hider-parity-convergence`, US1/Phase 3).
Each threshold is an 8x8 block-average diff limit (see `test_hider_parity.cpp`),
set to comfortably clear the scene's *known, currently-accepted* residual
divergence between `reyes` and `raytrace`, while still being tight enough to
catch a real regression (verified via T013 — see below).

| Scene         | Threshold | Measured worst-case | Known residual driving the gap |
|---------------|-----------|----------------------|---------------------------------|
| flatshade     | 20        | ~5.67                | Shading-interpolation model (D3/D4: REYES grid-interpolated shading vs. raytrace per-hit shading) — algorithmic, not closable by refactor per the audit. |
| dof           | 60        | ~43.05                | D2 pixel-jitter-constant drift (`0.5001011` vs `0.5`) + DOF occlusion model (D9: screen-space scatter vs. true lens rays) — D2 closes in Phase 4/T021 (shared `CSampler`); D9 is a bounded, documented residual. |
| aov           | 25        | ~10.08                | Same shading-interpolation residual as flatshade, carried through the "N" AOV channel. |
| depthdefault  | 500000    | 437495.8125           | D7: raytrace has no depth-filter modes / `zvisibilityThreshold` yet, so silhouette-edge pixels blend the ~1e30 background sentinel into an averaged z sample. Reyes cleanly separates real hits from background. Closes in Phase 6/S3; threshold re-measured/tightened in T045. |

## Background-sentinel handling (depth scenes)

Both hiders write a large sentinel for "no geometry hit" pixels in `z`-mode
depth output, but the exact bit pattern differs (reyes: exactly `1e30`;
raytrace: `1.0000004e30`). `compareDepth()`'s `clampDepth()` helper treats any
`|z| >= 1e6` as one canonical background value before diffing, so this
sentinel-representation difference — which is benign and expected — doesn't
by itself produce spurious noise. The real, measured worst-case above is what
remains after that clamp: it is the D7 edge-blending defect, not sentinel
noise.

## Harness self-test (T013)

The harness's discriminating power was verified by deliberately perturbing a
real, shared sampling constant (`raytracer.cpp`'s per-sample pixel-centering
offset, `raytracer.cpp:429-430`, temporarily shifted from `+0.5` to `+8.5`, an
~8px shift) and re-running `ctest -L parity`:

- `Parity_flatshade`, `Parity_dof`, `Parity_aov` — all correctly **failed**
  with large block-average diffs (order 100+, well past their thresholds).
- `Parity_depthdefault` — still **passed**. This is expected, not a harness
  gap: its threshold (500000) is already dominated by the much larger,
  pre-existing D7 residual, so an 8px spatial shift doesn't register against
  it. T013's acceptance criterion only requires that *at least one* scene
  detects the deliberate divergence, which it does.

The perturbation was fully reverted afterward and `ctest -L parity` was
re-run to confirm a clean return to 100% pass (see git history for this
spec's Phase 3 commit).

## Future work

- **T021** (Phase 4, shared `CSampler`): once D2's jitter-constant drift is
  unified, re-measure the `dof` scene's worst-case diff and tighten its
  threshold from 60 down toward the shading-residual floor (~5-10).
- **T045** (Phase 6, shared depth-filter modes / S3): once raytrace gains
  `zvisibilityThreshold` and depth-filter-mode support, re-measure
  `depthdefault` and tighten its threshold from 500000 down to a value
  reflecting only the remaining shading-interpolation residual.
- D3/D4 (shading-interpolation model) and D9 (DOF occlusion model) are
  **not** refactor targets per the audit (`hider-parity-audity-and-roadmap.md`)
  — they are bounded, permanent residuals. `flatshade`/`aov`'s thresholds
  are not expected to shrink much below their current measured values.
