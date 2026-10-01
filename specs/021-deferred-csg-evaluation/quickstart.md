# Quickstart: Validating Deferred CSG Evaluation

**Feature**: 021-deferred-csg-evaluation

This guide is a set of runnable checks proving the feature end to end. For the
interfaces, see [contracts/](contracts/); for the entities, see
[data-model.md](data-model.md).

## Prerequisites

- Worktree: `../openrender-worktrees/021-deferred-csg-evaluation` (branch
  `021-deferred-csg-evaluation`).
- vcpkg set up once via `scripts/setup-vcpkg-macos.sh`, with `VCPKG_ROOT` and
  `VCPKG_INSTALLED_DIR` exported. On Linux, system packages.
- Run heavy steps throttled (the maintainer's thermal preference): prefix with
  `nice -n 15`, and apply `taskpolicy -b -p <pid>` on macOS.

```bash
cd ../openrender-worktrees/021-deferred-csg-evaluation
nice -n 15 cmake -B build -S . -DCMAKE_BUILD_TYPE=Release
nice -n 15 cmake --build build -j4
```

Render environment used below:

```bash
export SHADERS="$PWD/build/shaders" ORENDERHOME="$PWD/openrender" \
       DISPLAYS="$PWD/build/src/display/file"
```

## 1. Hero scene: crack-free, smooth seams (US1, SC-001 to SC-003)

Render the hero scene in each hider at 800×500 and 1600×1000. The scenes are
registered under `examples/rib/tests/csg-hero-*.rib`, and each is a variant of:

```rib
SolidBegin "difference"
    SolidBegin "primitive"  Sphere 1.50 -1.50 1.50 360  SolidEnd
    SolidBegin "primitive"  Cylinder 0.65 -2 2 360      SolidEnd
SolidEnd
```

Run the seam-metric test:

```bash
ctest --test-dir build -R "CsgSeam_hero" --output-on-failure
```

**Expected**:
- 0 stray pixels inside the clear through-hole disc. The master baseline was 213
  at 1600×1000 in raytrace.
- 0 see-through gap pixels outside it.
- Seam radial deviation ≤ 1 px from the exact intersection circles.
- All of this holds in all three hiders, at both resolutions.

## 2. Cross-hider agreement (US2, SC-004)

```bash
ctest --test-dir build -L parity -R "csg" --output-on-failure
```

**Expected**: every CSG parity pair passes within the existing thresholds.

## 3. Nothing regressed (US3, SC-005, SC-008, SC-010)

```bash
ctest --test-dir build -R "CSG|Csg|csg" --output-on-failure
ctest --test-dir build -L visual --output-on-failure
```

Run the visual suite sequentially or with low `-j`. Under throttling, high `-j`
causes spurious timeouts.

**Expected**:
- The CSG unit suites pass.
- Every existing CSG visual scene passes against its deliberately regenerated
  reference.
- Every non-CSG visual scene is unchanged.
- The procedural-operand scene matches its inline twin.
- The untouched far side of the hero sphere matches a plain `Sphere` render.

The following are known flaky or already failing on master and must not be
misattributed:
- flaky: ImagerRegressionRender, Parity_transparency-matte-aov,
  ShadingParity_AmbientAccumulation, NotRequired_motion-subdiv-translate-photon
- already failing: ImagerGuardTests, ShaderCompilerImmutability

## 4. Work only where seen (US4, SC-006)

Render `examples/rib/tests/csg-offscreen-stats.rib` (a solid placed behind the
camera) with `Option "statistics" "endofframe" [1]`.

**Expected**: the CSG counters report 0 instances resolved, 0 classifications and
0 procedural expansions for that frame.

## 5. Settings and diagnostics (FR-019, FR-023, FR-025)

- A scene using `Attribute "solid" "tessellationtolerance"` several times prints
  exactly **one** notice and renders identically to the same scene without it.
- `examples/rib/tests/csg-malformed-*.rib` produce the same diagnostics as
  before, except that `csg-malformed-procedural-in-primitive.rib` now **renders**
  (FR-024). Its registration moves from the diagnostic tests to the visual tests.

## 6. Performance bound (SC-007)

Time each existing CSG test scene and the hero scene against the master
baselines. Hero baseline: raytrace 0.31 s / 1.42 s; REYES 0.19 s / 0.40 s;
Z-buffer 0.16 s / 0.34 s.

**Expected**: every scene stays under 10× its baseline.

## 7. Separation-of-concerns gate (FR-012, contract)

```bash
grep -rnE '#include "(csg[A-Za-z]*|solidObject)\.h"|\bCCSG|\bCSG[A-Z_]|\bcsg[A-Z]' \
  src/ri/hiders src/libshader/shading
```

**Expected**: no matches. The pattern targets CSG headers, CSG types and CSG
functions. It deliberately ignores comments that mention CSG, the unrelated
`maxSolidAngle`, and the pre-existing `ATTRIBUTES_FLAGS_SOLID_FRAGMENT` /
`isSolidFragment` Interior/Exterior read at `shading.cpp:1019`. The command
returns no matches on master today.
