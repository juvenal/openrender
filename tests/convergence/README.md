# Convergence harness (spec 039)

Checks that each sampler backend converges toward a high-sample-count reference as the sample count
grows (FR-033, SC-003 part b).

- `test_convergence.cpp` renders one scene template at the ladder 1, 4, 16, 64, 256 with the `sobol`,
  `pmj02` and `independent` samplers, 8 seeds per rung, computes the RMSE against the reference over all
  channels, fits the log-log slope by least squares, prints the table, and checks the bar of the default
  (`sobol`) sampler against `independent`: `--bar render` needs a slope 0.15 steeper and no higher RMSE at
  the top rung, `--bar nonworse` needs a slope and a top-rung RMSE no worse. Its header comment documents
  every option and the template placeholders (`@SAMPLER@`, `@SEED@`, `@SPP@`, `@SAMPLES@`).
- `scenes/*.rib.in` are the six scenes, 64x64 with float output: `dof-reyes`, `dof-raytrace`,
  `motion-reyes` (knob `spp`, `PixelSamples @SPP@`) and `arealight-raytrace`, `occlusion-raytrace`,
  `glossy-raytrace` (knob `samples`, the sample count of the shading-side integral; they never vary
  `PixelSamples`). The area light and the glossy mirror use `shaders/convergence_arealight.sl` and
  `shaders/convergence_glossy.sl`.
- `reference/*.tif` are the checked-in references.
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
`specs/039-sampling-foundation/validation.md` (M4): about 3 minutes for all six scenes. A reference of
only 4096 samples (the minimum the spec asks for) has the noise of the top rung's own error and bends the
fitted slope of a stratified sampler, so a larger one is used.
