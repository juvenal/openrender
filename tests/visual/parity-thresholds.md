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
| dof           | 60        | ~42.5-44.7 (noise band, post-T021) | D9 DOF occlusion model (screen-space scatter vs. true lens rays) + D3/D4 shading-interpolation residual, both bounded/documented, non-closable by refactor. D2 pixel-jitter-constant drift was closed in Phase 4 (shared `CSampler`, T017/T018) but its contribution could not be isolated from run-to-run RNG noise — see T021 note below. |
| aov           | 25        | ~10.08                | Same shading-interpolation residual as flatshade, carried through the "N" AOV channel. |
| depthdefault  | 500000    | 437495.8125           | D7: raytrace has no depth-filter modes / `zvisibilityThreshold` yet, so silhouette-edge pixels blend the ~1e30 background sentinel into an averaged z sample. Reyes cleanly separates real hits from background. Closes in Phase 6/S3; threshold re-measured/tightened in T045. |

## Spec 021: solid (CSG) scenes (T058)

Every `Parity_csg-*` pair uses the default threshold (`VISUAL_THRESHOLD`, 20):
no CSG scene needs a wider one. These are the worst 8x8 block-average differences
measured when the tests were added (2026-10-03, throttled, sequential):

| Pair | Measured | Pair | Measured |
|---|---|---|---|
| sphere-cube-union raytrace-reyes / -zbuffer | 4.84 / 5.02 | hero-800 raytrace-reyes / -zbuffer | 9.08 / 7.23 |
| sphere-cube-difference raytrace-reyes / -zbuffer | 9.11 / 6.94 | hero-1600 raytrace-reyes / -zbuffer | 9.33 / 7.47 |
| sphere-cube-intersection raytrace-reyes / -zbuffer | 5.84 / 5.89 | coincident-union raytrace-reyes / -zbuffer | 1.86 / 1.70 |
| sphere-cube-difference-800 raytrace-reyes / -zbuffer | 7.58 / 8.38 | coincident-difference raytrace-reyes / -zbuffer | 14.30 / 14.86 |
| sphere-cube-difference-1600 raytrace-reyes / -zbuffer | 8.48 / 9.05 | open-quadric-caps raytrace-reyes / -zbuffer | 8.77 / 8.73 |
| hero-two-shaders raytrace-reyes | 9.16 | hero-displaced raytrace-reyes | 11.11 |
| hero-translucent raytrace-reyes | 5.52 | secondary-rays raytrace-reyes | 12.44 |
| polymesh-box-difference raytrace-reyes | 2.98 | nurbs-rational-sphere-union raytrace-reyes | 2.14 |
| subdivision-sphere-union raytrace-reyes | 3.06 | | |

The largest residual is the coincident difference (14.86). Its source has not
been isolated; it still leaves a 5-unit margin.

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

- **T021** (Phase 4, shared `CSampler`) — attempted, inconclusive: after D2's
  jitter-constant drift was unified (T017/T018), `ctest -R Parity_dof` was
  re-run 3 times with zero intervening code changes and measured
  `MaxBlockAvgDiff` of 44.69, 42.52, 44.05 — a ~2.2-unit spread purely from
  RNG noise. This is statistically indistinguishable from the previously
  documented ~43.05 baseline, so **D2's contribution cannot be isolated from
  noise with the current harness** and the threshold is **not** tightened.
  Root cause: reyes and raytrace draw lens/jitter samples from independent,
  uncorrelated RNG streams (reyes: per-pixel `CSobol<2>`; raytrace: `urand()`
  calls in a different order) — even a fixed seed on both wouldn't correlate
  them, since the streams are consumed in different sequences. Closing D2
  removed one *source* of divergence, but the residual is now dominated by
  the non-closable D3/D4 (shading-interpolation) and D9 (DOF occlusion model)
  divergences the audit already flagged as bounded/permanent. Meaningful
  re-measurement and threshold-tightening requires **Option B**'s unified
  per-bucket sample table (Phase 11, T080-T086), which generates one sample
  set consumed verbatim by both hiders — only then will the two hiders' noise
  patterns correlate enough for a before/after diff comparison to mean
  anything.
- **T045** (Phase 6, shared depth-filter modes / S3): once raytrace gains
  `zvisibilityThreshold` and depth-filter-mode support, re-measure
  `depthdefault` and tighten its threshold from 500000 down to a value
  reflecting only the remaining shading-interpolation residual.
- D3/D4 (shading-interpolation model) and D9 (DOF occlusion model) are
  **not** refactor targets per the audit (`hider-parity-audity-and-roadmap.md`)
  — they are bounded, permanent residuals. `flatshade`/`aov`'s thresholds
  are not expected to shrink much below their current measured values.

## T084: Option B (correlated sample table) re-measurement — mixed results

Phase 11's `OPENRENDER_CORRELATED_SAMPLE_TABLE=1` gates both hiders onto one
per-bucket sample table (T080-T083), resolving the "independent, uncorrelated
RNG streams" root cause the T021 note above identified as blocking
meaningful re-measurement. Once the mechanism existed, re-measuring showed
this does **not** uniformly tighten thresholds — the effect is scene-
dependent, and the deciding factor is *what kind* of residual dominates:

- **Same-model/different-RNG-stream residuals close for real.** Two scenes
  showed a large, stable improvement across repeated runs:
  - `motion-quadrics-translate`: ~4.05 uncorrelated → 2.16/2.19/2.20/2.31
    correlated (4 runs). New `Parity_motion-quadrics-translate-correlated`
    test added, threshold 6.
  - `motion-patches-translate`: ~5.81 uncorrelated → 2.95/3.12/3.00
    correlated (3 runs). New `Parity_motion-patches-translate-correlated`
    test added, threshold 8.

  These are registered as **additional** tests (`VISUAL_ENV_CORRELATED` in
  `tests/visual/CMakeLists.txt`), not replacements for the uncorrelated
  ones — Option B is opt-in, not the hiders' default sampling path, so the
  uncorrelated tests still validate the shipping behavior.

- **Different-model/same-input residuals don't close, and can get worse.**
  `dof` got measurably *worse* under correlation: 53.62/53.70/53.62
  correlated (3 runs, very low run-to-run spread — confirms the table
  mechanism itself is deterministic) vs. the ~42.5-44.7 uncorrelated
  baseline. Forcing both hiders to consume the *same* lensU/lensV draw
  doesn't help when D9 routes that same number through two structurally
  different DOF formulas (reyes: screen-space disk scatter; raytrace: true
  lens-ray origin offset) — the result is a more spatially coherent
  (structured) error per 8x8 block instead of independent noise that
  partially cancels under block-averaging. `dof`'s threshold (60) is left
  unchanged; no correlated variant was added for this scene.

- **A scene can also be flat within noise.** `motion-polygons-translate`
  went ~3.30 uncorrelated → 4.08 correlated (single runs each). Per T021's
  own precedent (a ~2.2-unit RNG spread on `dof` with zero code changes),
  a ~0.8-unit delta from single-run measurements on a different scene isn't
  distinguishable from noise. No correlated variant was added; re-measuring
  with repeated runs is possible future work but not required for T084.

**Takeaway for future scenes**: before adding a `*-correlated` parity
variant, confirm the scene's cross-hider residual is dominated by an RNG-
stream mismatch (same underlying model, different draw order) rather than
a structural divergence (D3/D4/D9) — only the former closes under Option B.

## Backend parity (spec 024)

These pairs compare a `.slo` (LLVM JIT) render against the `.rslo` (interpreter) reference of the
same scene, not two hiders against each other. They are registered as `Visual_*` tests with
`add_visual_test()` in `tests/visual/CMakeLists.txt`.

| Scene | Threshold | Why |
|---|---|---|
| `photon-parity`, `photon-parity-slo` | **6**, rendered at `-t:1` | See below. |
| `solar0`, `solar0-slo` | 20 (the default) | A raytrace of a light whose only construct is `solar()` with no arguments. Before the fix the JIT never set `L` for it and the diff was 201.62. |

The photon scene is a photon pass (point, spot and distant lights) followed by a raytrace of the
global map the pass wrote, so the image depends on the photons actually stored; an empty map
renders black.

- **D = 2.91.** The Linux render differs from the macOS one by a constant 2.91 (30 of 30 runs).
  The macOS run-to-run diff at `-t:1` is 0.00. The threshold is max(1, ceil(2 x D)) = **6**.
- **Empty-map margin.** A JIT variant that stores no photons misses the reference by 255.00,
  which is at least 18 (3 x the threshold), so the threshold cannot hide a missing map.
- **`-t:1`.** At the default thread count the photon pass crashed in 4 of 30 runs on macOS (the
  `CPhotonHider` race, `DEVNOTES_DETAILS/BUGS.md`, Open), so these scenes render single-threaded.
- **Measured with the fix:** `Visual_photon-parity-slo` against the interpreter reference gives
  MaxBlockAvgDiff 0.00, and its map is 2 009 546 bytes, the same as the interpreter's.

The numbers come from `specs/024-shading-services-routing/research.md` R4.
