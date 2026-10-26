# Validation: Graphics State Handles

**Plan**: [plan.md](plan.md) | **Quickstart**: [quickstart.md](quickstart.md)

This file holds the measured evidence, oldest first. The raw data for §0 is in [tools/](tools/).

## §0 Baselines on the starting tree, macOS (2026-10-26)

**Tree**:
- `build-025` was configured from the worktree at master `dfffba0`: Release, vcpkg
  (`arm64-osx-openrender`), Homebrew bison.
- At the time, the worktree held only the spec documents and the two new test files (§0.4, §0.5).
- No source under `src/` was changed. The measurement variants were built in a scratch copy
  outside the worktree.
- Every job ran through `~/.claude/bin/cpujob`, at `-j4` or lower.

### §0.1 Build

The clean configure and build succeeded (exit 0).

### §0.2 Architecture check

`ctest -L architecture` passed. The full report:

```
ALLOWED   DB-001 x 8 DB-004 x 65 DB-005 x 8 DB-006 x 33 DB-008 x 7 DB-010 x 35 DB-011 x 173
          DB-015 x 10 DB-017 x 78 DB-019 x 3 DB-021 x 1 DB-025 x 12 DB-026 x 3
METRIC    Geometry 78/19, Hiders 567/84, Infra 0/0, SceneInterface 6/6, Shading 0/0,
          Texturing 70/16, TOTAL 721/110 (all equal to metrics-baseline.txt)
RESULT    PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0
```

These are the "before" numbers for SC-001, SC-002 and SC-009: 7 DB-008 edges, and Graphics State
has no metric row.

### §0.3 Full suite

`ctest -LE determinism-sweep -j4`: **784/784 passed**, in 154 s.

Graphics State has no `CRenderer::` metric row, which confirms the spec's "no link edges from State
sources".

### §0.4 Part A of the parameter equivalence (FR-007)

The test is `tests/shading_parity/test_shader_parameter_equivalence.cpp` (`ShadingParity_ShaderParameterEquivalence`).

**On `dfffba0`: 64 passed, 0 failed.**
- The test covers these cases:
  - none (before `RiWorldBegin`);
  - the world defaults;
  - surface only;
  - atmosphere over the default surface;
  - all three;
  - all three plus a light;
  - the light switched off;
  - displacement plus surface;
  - the surface cleared;
  - after `ResourceRestore`.
- `defaultsurface` and `defaultlight` were both bound.

This shows the oracle reproduces today's `findParameter` and `usedParameters`.

**Mutation (scratch copy)**: `findParameter` was changed to search surface before displacement.
Result: **60 passed, 4 failed** (`foundDefault("shared") == shared` in the four cases that bind both
slots). So the test fails on a build where the answer differs.

### §0.5 Light order (FR-025)

**The test**: `State_LightOrder` (`tests/unit/state/test_light_order.cpp`).

| Build | Result |
|---|---|
| `dfffba0` | **red**: 5 passed, **4 failed**. Failing: "inside AttributeBegin", "after a copy-on-write clone", and the two checks that follow them. Passing: same block, nested double push, after AttributeEnd |
| `dfffba0` plus the fix sketch (`tools/lightorder-fix-sketch.diff`), scratch copy | **green**: 9/9 |

**Impact scan**: `tools/lightorder_scan.sh`, with `tools/lightorder-probe.diff` applied in the
scratch copy only.
- **Coverage**: all **489** `examples/rib/**/*.rib`. 487 reached `RiWorldEnd`; the other 2 are
  archive operands with no world block.
- **Result**: **82 scenes** have a different light-order hash with the fix
  (`tools/affected-ribs.txt`). The list is in plan.md, "FR-025 affected scenes and references".
- **Multi-frame scenes**: 16 were re-run frame by frame (`tools/lightorder_frames.sh`). They add no
  scene to the list.
  - In one run or the other, the known `CPhotonHider` race (BUGS.md) cut short the second frame of
    some `photon-solar0-*` scenes.
  - Where both runs produced a frame, its hash is identical, except for scenes already in the list.
- **Procedurals**: the probe does not see objects expanded at render time. 7 scenes contain
  procedurals, and 2 of them have two or more light statements:
  - `csg-procedural-open-cylinder-raytrace` is in the list, because its parse-time objects differ;
  - `csg-procedural-missing-archive` is not.

  Light order inside their render-time expansions is unobserved; the phase 7 full suite covers
  it.

**The tests that render an affected scene** (`tools/affected-tests.tsv`): 139.
- 35 `Visual_*`, 29 `Parity_*`, 2 `Sampling_ThreadSweep_*`, and 1 `NotRequired_*` plus 1 `Perf_*`
  (manual, not run);
- 71 `DeterminismSweep_*`.

**The 66 runnable non-sweep tests, run on both builds**:

| Measurement | Starting tree | With the fix |
|---|---|---|
| tests passed | 66/66 | 66/66 |
| `MaxBlockAvgDiff` per test (`tools/affected-diff-compare.tsv`) | — | **identical for all 63 tests that print one** |
| output images, byte-for-byte (`tools/affected-bitwise.tsv`) | — | **95/95 bit-identical** |

**Conclusion**:
- The fix changes the order in which 82 scenes accumulate light, a float summation order.
- No 8-bit output image of any test changes on macOS.
- **No reference needs re-rendering.** Phase 7 re-checks this, and so does Linux (§4 of the
  quickstart).

**The 71 determinism-sweep tests with the fix**: these compare configurations within one tree, so
light order cannot fail them. Result with the fix (scratch copy, `-j2`): **71/71 passed**.

**Architecture check, re-run** after the two new test files were built into `build-025`: 21/21
passed, unchanged.

### §0.6 Timing of the slowest three visual scenes (SC-005)

The three slowest `visual`-labelled tests in §0.3 were each run 5 times, alone, on the starting
tree:

| Test | Runs (s) | Min | Median | Max |
|---|---|---|---|---|
| `Visual_teapot-motion-raytrace` | 20.62, 19.65, 19.54, 19.41, 19.55 | 19.41 | 19.55 | 20.62 |
| `Csg_Seam_freeform_nurbs` | 4.16, 4.17, 4.10, 4.11, 4.15 | 4.10 | 4.15 | 4.17 |
| `Csg_Seam_spherecube_reyes_1600` | 4.22, 4.24, 4.17, 4.23, 4.24 | 4.17 | 4.23 | 4.24 |

**SC-005 check after the change**: each median falls within its min–max band.

### §0.7 libshader_shading flat-namespace imports (spec 024)

`nm -m libshader_shading.1.dylib | grep 'dynamically looked up'`: **7**
(`tools/libshader-dynlookup-before-macos.txt`). The seven are:
- `CPhotonMap::lookup`;
- `CTexture3d::resolve`;
- `CGatherBundle::CGatherBundle()`;
- `CShadingContext::trace` and `traceEx`;
- the `CTraceBundle` and `CTransmissionBundle` vtables.

This is spec 024's value. After 025 the count must still be 7.

### Not yet measured

Linux (GCC 13, `-Werror`) needs the maintainer's rsync + Maestri path; see quickstart §4.
