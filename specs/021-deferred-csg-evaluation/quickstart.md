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
export SHADERS="$PWD/build/shaders" ORENDERHOME="$PWD/build" \
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

Every check below is a test registered with the `add_csg_metric_test` macro
(`tests/visual/CMakeLists.txt`). It renders the scenes through the
`tests/visual/csg_metric_test.cmake` driver, runs `test_csg_seam_metric` on the
output, and checks logs and statistics. Run the seam-metric tests:

```bash
ctest --test-dir build -R "CsgSeam_(hero|spherecube)" --output-on-failure
```

**Expected**:
- 0 stray pixels inside the clear through-hole disc. The master baseline was 213
  at 1600×1000 in raytrace.
- 0 see-through gap pixels outside it.
- Seam radial deviation ≤ 1 px from the exact intersection circles.
- Sphere-minus-cube: seams within 1 px of the projected exact sphere ∩ cube-face
  circles (`--projected-curve`), and no enclosed holes.
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

The CSG counters print at `Option "statistics" "endofframe" [2]` or higher,
labelled `CSG classifications:`, `CSG seam splits:`, `CSG instances resolved:`
and `CSG procedural expansions:` (zeros included whenever the frame has a solid).

```bash
ctest --test-dir build -R "Csg(OffscreenStats|PartialVisibilityStats|ResolutionScaling|TinySolidStats)" \
      --output-on-failure
```

**Expected**:
- `CsgOffscreenStats`: `csg-offscreen-stats.rib` (a hero solid behind the
  camera whose sphere operand is a procedural) reports 0 instances resolved,
  0 classifications and 0 procedural expansions.
- `CsgPartialVisibilityStats`: the half-framed hero (`csg-hero-partial-stats.rib`)
  needs at most 0.75× the seam splits of the fully framed one.
- `CsgResolutionScaling`: the REYES hero at 1600×1000 reports more seam splits
  than at 800×500 (precision follows resolution).
- `CsgTinySolidStats`: a hero solid covering under 4×4 pixels needs at most 16
  seam splits.

## 5. Settings and diagnostics (FR-019, FR-023, FR-025)

- A scene using `Attribute "solid" "tessellationtolerance"` several times prints
  exactly **one** notice and renders identically to the same scene without it.
- `examples/rib/tests/csg-malformed-*.rib` produce the same diagnostics as
  before.
- The former `csg-malformed-procedural-in-primitive.rib` is renamed
  `csg-procedural-missing-archive.rib`. Its procedural, which still points at a
  nonexistent archive, is now **accepted** at capture (FR-024). Expansion reports
  exactly one archive-open diagnostic, and the procedural contributes nothing
  (FR-025). The operand is then the block's own `Sphere 1 -1 1 360`, so **the
  render shows that sphere**. The old "cannot be declared inside a SolidBegin
  block" diagnostic must not appear.
- `CsgProcedural_sphere` and `CsgProcedural_open_cylinder` match their inline
  twins (SC-010). The open cylinder comes from the archive and still gets its
  caps. The archives are copied into each test's scratch working directory
  (`COPY examples/rib/tests/archives`).

## 6. Performance bound (SC-007)

Time each existing CSG test scene and the hero scene against the master
baselines. Hero baseline: raytrace 0.31 s / 1.42 s; REYES 0.19 s / 0.40 s;
Z-buffer 0.16 s / 0.34 s.

**Expected**: every scene stays under 10× its baseline.

## 7. Separation-of-concerns gate (FR-012, contract)

```bash
grep -rnHE '[A-Za-z0-9_]*(Csg|CSG|csg)[A-Za-z0-9_]*' src/ri/hiders src/libshader/shading \
  | grep -vE ':[0-9]+:[[:space:]]*(//|\*|/\*)'
```

**Expected**: no output. The first grep finds any identifier containing `csg`,
`Csg` or `CSG`: CSG headers, types, functions, and members such as
`numCsgClassifications`. The second drops comment lines. (`-H` keeps the
`file:line:` prefix that the comment filter relies on, even on a single file.)
The pattern does not match `maxSolidAngle` or the pre-existing
`ATTRIBUTES_FLAGS_SOLID_FRAGMENT` / `isSolidFragment` Interior/Exterior read at
`shading.cpp:1019`. The gate returns no output on master today.
