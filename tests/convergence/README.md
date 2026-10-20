# Convergence harness (spec 039)

Checks that each sampler backend converges toward a high-sample-count reference as the sample count
grows (FR-033, SC-003 part b).

- `test_convergence.cpp` renders one scene template at the ladder 1, 4, 16, 64, 256 with the `sobol`,
  `pmj02` and `independent` samplers, 8 seeds per rung, computes the RMSE against the reference over all
  channels, fits the log-log slope by least squares, prints the table, and checks the bar of the default
  (`sobol`) sampler against `independent`: `--bar render` needs a slope 0.15 steeper and no higher RMSE at
  the top rung, `--bar nonworse` needs a slope and a top-rung RMSE no worse, and `--bar gain` needs a slope
  no worse and a top-rung RMSE at most the baseline's divided by `--gain` (1.5). Its header comment documents
  every option and the template placeholders (`@SAMPLER@`, `@SEED@`, `@SPP@`, `@SAMPLES@`).
- `scenes/*.rib.in` are the eight scenes, 64x64 with float output: `dof-reyes`, `dof-raytrace`,
  `motion-reyes` (knob `spp`, `PixelSamples @SPP@`) and `arealight-raytrace`, `arealight-cone-raytrace`,
  `occlusion-raytrace`, `glossy-raytrace`, `glossy-mirror-raytrace` (knob `samples`, the sample count of
  the shading-side integral; they never vary `PixelSamples`). Bars: `render` for the DOF scenes,
  occlusion and the two single-integral scenes, `nonworse` for motion blur, `gain` for
  `arealight-raytrace` and `glossy-raytrace` (see below). The area light and the glossy mirror use `shaders/convergence_arealight.sl` and
  `shaders/convergence_glossy.sl`.
- `reference/*.tif` are the checked-in references.

## Why there are two area-light and two glossy scenes

A `trace()` or `transmission()` ray draws three pairs of dimensions by sample index: pair 0 spreads its
origin over the shaded point's footprint, pair 1 spreads its direction by the direction's footprint, pair
2 places it in the cone (`samplecone`). Each pair is padded: it has its own index shuffle and scramble, so
it is stratified by itself and not jointly with the others. When two of them move the same edge, the
cross term is not stratified by any backend and the fitted slope decays towards -0.5 at the top rungs.
That is a known property of the sampler design, not a defect of a call (measured in
`specs/039-sampling-foundation/validation.md`, "Phase 8 follow-up").

- `arealight-raytrace` and `glossy-raytrace` keep that coupling (the footprint and the cone both move the
  shadow edge or the reflected edge). They are checked by the `gain` bar: the default sampler must be no
  worse in slope and at least 1.5 times better at the top rung (measured about 2.1 to 2.2 times).
- `arealight-cone-raytrace` (`"samplebase" [0]`: the shadow rays start at the point) and
  `glossy-mirror-raytrace` (a flat mirror under an orthographic camera: the mirror direction has no
  footprint) integrate the cone alone, one active pair, and are checked by the full `render` bar. They
  exist to test that the backend stratifies the integral it is given.
- Possible future work: draw the footprint and the cone as one 4D point from a real 4D net, which would
  stratify the cross term (a sampler API addition).
- The `convergence` label is opt-in in practice (a few minutes in all); run it with
  `ctest --test-dir build -L convergence`. Use `-LE determinism-sweep` for the everyday full run.

## Regenerating the references

Not a test and not part of `all`. Serially, under nice:

```sh
nice -n 19 cmake --build build --target convergence-reference
```

For every scene this runs (with `SHADERS`, `ORENDERHOME`, `DISPLAYS` and `GEOMETRIES` set as the tests set
them, see `CMakeLists.txt`):

```sh
build/tests/convergence/test_convergence --make-reference \
    --orender build/src/orender/orender \
    --template tests/convergence/scenes/<scene>.rib.in \
    --reference tests/convergence/reference/<scene>.tif \
    --knob <spp|samples> --ref-knob 256 --ref-renders 256 \
    --scratch build/tests/convergence/<scene>-reference
```

The reference is the mean of 256 renders at 256 samples with the `independent` sampler and seeds 100000
and up (no rung uses them), that is 65536 independent samples per pixel. The cost of that is recorded in
`specs/039-sampling-foundation/validation.md` (M4): about 3 to 4 minutes for all eight scenes. A reference of
only 4096 samples (the minimum the spec asks for) has the noise of the top rung's own error and bends the
fitted slope of a stratified sampler, so a larger one is used.
