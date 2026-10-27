# Validation record: Texture System (spec 027)

Measurements and evidence, appended phase by phase. Commands are in [quickstart.md](quickstart.md).

## §0 Plan stage (macOS, 2026-10-27)

Tree: `027-texture-system` at `0f76934` (master `bc261b1` plus the spec), no source change. Build `build-027`,
configured from scratch in the worktree (vcpkg auto-detected, Release), built at `-j4` through `cpujob`: clean,
exit 0. Logs in `build-027-logs/` (gitignored).

### §0.1 Architecture check

```text
archcheck: platform=macos objects=256 sources=182 edges=2363 (include=1466 link=897)
ALLOWED   DB-001 x 8 DB-004 x 65 DB-005 x 8 DB-006 x 33 DB-008 x 7 DB-010 x 35 DB-011 x 173 DB-015 x 10 DB-017 x 78 DB-019 x 3 DB-021 x 1 DB-025 x 12 DB-026 x 3
METRIC    Texturing refs 70 (baseline 70)  distinct 16 (baseline 16)
METRIC    TOTAL refs 721 (baseline 721)  distinct 110 (baseline 110)
RESULT    PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0
```

Full lines: `tools/archcheck-bc261b1-macos.txt`. The spec's numbers (37 Texturing-origin lines: DB-010 27,
DB-021 1, DB-025 6, DB-026 3 edges / 5 lines; DB-010 35 in all) hold on the fresh build.

### §0.2 Texturing's renderer-core members

`tools/core_usage.py` (archcheck's comment and literal blanking, re-implemented to name the members) on
`src/ri/texture`: **refs 70, distinct 16**, matching archcheck. Output: `tools/core-usage-texturing-bc261b1.txt`.

| Member | Refs | | Member | Refs |
|---|---|---|---|---|
| `textureRefNumber` | 11 | | `synthesizeMutex` | 2 |
| `textureUsedBlocks` | 10 | | `textureCacheEnabled` | 2 |
| `locateFile` | 9 | | `textureCacheDirectory` | 2 |
| `numThreads` | 9 | | `environmentLoad` | 1 |
| `textureUsedMemory` | 8 | | `initTextures` | 1 |
| `textureMaxMemory` | 6 | | `shutdownTextures` | 1 |
| `textureMutex` | 5 | | `textureLoad` | 1 |
| | | | `texturePath` | 1 |
| | | | `retrieveDisplayChannel` | 1 |

By file: `texture.cpp` 62, `texmake.cpp` 6, `brickmap.cpp` 1, `texture3d.cpp` 1. Other domains reference four of
them: Geometry `locateFile` 1, `numThreads` 8, `texturePath` 1; Hiders `numThreads` 6.

### §0.3 Shading's flat-namespace imports

7, identical to `tools/libshader-dynlookup-before-macos.txt` (`diff` empty). Fresh list:
`tools/libshader-dynlookup-bc261b1-macos.txt`. The one Texturing symbol is `CTexture3d::resolve(int, char const**,
int*, int*)`.

### §0.4 Texture-related tests

| Run | Result |
|---|---|
| `ctest -L texture_tile -j2` | 9/9 passed (5.8 s) |
| `ctest -L image_input -j4` | 6/6 passed |
| `ctest -R "Parity_(texture\|unbaked\|env\|shadow)\|textureinfo\|texture3d\|occlusion\|Rgbe\|rgbe\|pointcloud\|brick\|ptc" -LE determinism-sweep -j4` | 40/40 passed |

### §0.5 Code findings that change the plan (details in research.md)

- `CRendererServicesImpl` holds `CRenderer::shaderMutex` around every texture getter (`rendererServicesImpl.h:62-97`):
  #20's race is not reachable through a production path on `bc261b1` (R6, plan Q3).
- The JIT's `texture()`/`environment()`/`shadow()` take no parameter list and request a point or zero-width lookup
  (R10, plan Q1).
- No test writes through the `rgbe` display driver (R14).
- No `-slo` test renders `texture()`/`environment()`/`shadow()` (R16): the plan adds JIT probe renders to the image
  manifest.
- `src/ri/hiders/radiance.cpp` is not compiled (R21, plan Q5).

### §0.7 JIT against interpreter on the texture parity scenes (2026-10-27, after the maintainer's answer to Q1)

`tools/jit_texture_divergence.sh build-027` through `cpujob` (`-t:1`; each scene rendered with the default `.rslo`
shaders and with `Option "shaderformat" "default" ["slo"]`; MaxBlockAvgDiff of `.slo` against `.rslo` by
`test_visual_render`). Raw: `tools/jit-texture-divergence-bc261b1-macos.tsv`.

| Scene | reyes | raytrace |
|---|---|---|
| `texture-png` | 34.55 | 86.20 |
| `texture-exr` | 133.03 | 120.31 |
| `texture-rgbe` | 252.52 | 249.00 |
| `unbaked-png` | 147.34 | 144.81 |
| `unbaked-nonpot` | 156.39 | 153.62 |
| `unbaked-exr` | 73.78 | 74.38 |
| `unbaked-rgbe` | 73.45 | 74.11 |
| `env` | 244.11 | 244.14 |
| `shadow` | 0.00 (identical bytes) | 0.00 (identical bytes) |

A probe shader with all five forms compiled with the fresh `oshader --jit` references only `op_texture_f_v2`,
`op_environment_f_v2` and `op_shadow_f_v2`: color results take the float-channel entry point (R22).

Scan for existing `.slo` outputs the fix could change: 60 scenes in `examples/rib` select `.slo`; none binds a shader
that calls `texture()`, `environment()` or `shadow()`, and no `.sl` under `tests/`, `src/` or `examples/` calls them.
Expected changed existing outputs and references: **none**.

### §0.6 Not measured at the plan stage

The full suite pass list, the image manifest and its noise, the JIT probes, the lookup values for SC-003, the
RGBE driver reference and the timing: the first measurement task takes them on a fresh build (maintainer
decision 4). Linux: not measured (no access); quickstart §5.

## §1 Baselines (phase 1)

### §1.0 T005a: spec 025 brought in (2026-10-27)

- **`BASE`** = master `4eb683b9ad57d61ea0ce049206c10773c9ae640a` (spec 025 merged; 11 commits after `bc261b1`).
  Every `git diff $BASE` guard of tasks.md reads this value.
- **Master check**: `git show master:tests/architecture/allowlist.txt | grep -c DB-008` = `0` (`bc261b1`: 8 lines).
- **Merge**: `git merge --no-ff --no-commit master` in the 027 worktree; not committed (the maintainer commits it).
  - The first attempt was refused ("local changes ... would be overwritten") because the index held the staged
    spec-stage edits. They were saved as a binary patch, `git stash push`ed, the merge was run on the clean tree,
    and the patch was re-applied with `git apply --index` (verified byte-identical to the saved patch; the stash
    was dropped). Result: 97 non-spec files staged (58 added, 39 modified), the spec files exactly as before,
    nothing unstaged, nothing unmerged.
  - One conflict, the expected one: `.specify/feature.json` (this branch `specs/027-texture-system`, master
    `specs/025-graphics-state-handles`). Resolved to keep `specs/027-texture-system`; the file now equals `HEAD`'s.
- **Allowlist**: `git diff bc261b1 master -- tests/architecture/allowlist.txt` is 9 deletions and nothing else: the
  DB-008 block (its header comment, the 7 DB-008 lines, one blank line). The merged `allowlist.txt` equals master's.
- **Lines 025 moved** (re-read; shading.h, rendererServicesImpl.h, giFunctions.h, shaderFunctions.h, rendererFiles.cpp,
  rendererMutexes.cpp, rendererServices.h and irradianceDispatch.cpp's `:405`/`:545` are unchanged):
  `shading.cpp` +1 (`3393`->`3394`, `3346`->`3347`, `736-741`->`737-742`, `2970-3053`->`2971-3054`);
  `renderer.cpp` +1 (`749`->`750`, `779-806`->`780-807`, `731-738`->`732-739`, `826-831`->`827-832`, `851-866`->`852-867`,
  `873`->`874`); `renderer.h` (`396`->`393`, `286-301`->`283-298`); `rendererStatics.cpp` (`272-275`->`270-273`,
  `147-148`->`145-146`); `rendererContext.cpp` (`6155-6206`->`6134-6185`). Corrected in research.md, plan.md,
  data-model.md and tasks.md.
- **FR-015**: `CRenderer::retrieveDisplayChannel` is still an out-of-line forwarder (`renderer.h:270` declares it,
  `rendererDeclarations.cpp:515` defines it); `texture3d.cpp:119` is its only Texturing caller.
- The `ALLOWED` line, the TOTAL metric and the end figures need a built tree: T007 completes them.

### §1.1 Implementation log (T001)

- **Code base**: `BASE` = master `4eb683b` (spec 025 merged). Plan base `bc261b1`. Worktree HEAD at the start of
  the baselines: `d75ead8` (the maintainer's merge commit). Build tree `build-027`, logs `build-027-logs/` (gitignored).
- **Expected-red rule**: the only allowed failures from Phase 2 to Phase 8 are the known-red T-J tests listed in
  tasks.md; **Phase 9 is the only output-changing phase** (`.slo` only).
- Every job ran through `~/.claude/bin/cpujob`, at `-j4` (`-j2` for `texture_tile`, the determinism sets and #20's
  later test). Spec 028's plan agent shared the lock.

| Phase | Build | Suite totals | Architecture | Image diff |
|---|---|---|---|---|
| 1 (baseline, §1.2-§1.9) | clean, exit 0 | 838 registered: 837 passed, 1 skipped, 0 failed; `texture_tile` 9/9; determinism 155/155; sweep 419/419 | 21/21; `ALLOWED` without DB-008; PASS | baseline recorded: 779 images; noise list = 7 JIT raytrace probes + 2 `shading_parity` files (§1.5) |
| 2-10 | | | | |

### §1.2 T006: build on the unchanged tree

`git diff --stat 4eb683b -- src tests shaders examples CMakeLists.txt CMake` is empty (the merged tree is master's).
`cpujob 027-base-build cmake --build build-027 --config Release -j4` reconfigured by itself (CMake files changed
with 025) and built clean, exit 0, no compiler warning. The log has only linker notes that predate this spec:
"ignoring duplicate libraries" (Imath, LLVM component archives) and the Swift package `orender-wire-macos` `ld: warning`.

### §1.3 T007: architecture, members, imports (on `BASE`)

```text
archcheck: platform=macos objects=270 sources=185 edges=2419 (include=1506 link=913)      [plan stage: 256 / 182 / 2363]
ALLOWED   DB-001 x 8 DB-004 x 65 DB-005 x 8 DB-006 x 33 DB-010 x 35 DB-011 x 173 DB-015 x 10 DB-017 x 78 DB-019 x 3 DB-021 x 1 DB-025 x 12 DB-026 x 3
METRIC    Texturing refs 70 (baseline 70)  distinct 16 (baseline 16)
METRIC    TOTAL refs 721 (baseline 721)  distinct 110 (baseline 110)
RESULT    PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0
```

- `ctest -L architecture`: 21/21. The only change against §0.1 is that `DB-008 x 7` is gone (spec 025).
- **TOTAL is unchanged by 025** (721 / 110), so the end figures stay **652 / 98**; contracts/allowlist-end-state.md,
  research R17 and T114 now say so.
- `core_usage.py`: refs 70, distinct 16, byte-identical to `tools/core-usage-texturing-bc261b1.txt`.
- Shading's flat-namespace imports: **7**, identical to `tools/libshader-dynlookup-before-macos.txt`.
- Files written: `tools/archcheck-base-macos.txt` (the full report), `tools/core-usage-texturing-base.txt`,
  `tools/libshader-dynlookup-base-macos.txt`. The `tools/*bc261b1*` files are untouched (historical).

### §1.4 T008-T010: the suite, the probes, the baseline

- `ctest -LE determinism-sweep -j4` (log `build-027-logs/027-base-suite.log`): **838 registered, 837 passed, 1 skipped
  (`LibShader_Format_rslo_lower_minor`), 0 failed**, 286 s. No known-flaky test went red.
  The sorted name/status list is `tools/baseline-pass-list.txt` (CP-b's reference). A second run (T011) gave the same list.
- `ctest -L texture_tile -j2`: 9/9 passed (4.1 s).
- JIT probes (`jit_texture_probes.sh`): 18/18 scenes rendered.
- `tools/baseline-images.sha256`: **779 images** (the suite's plus the 18 JIT probe images, the four
  `imager_*_test.tif` and `tests/imager/guard/ri.tif`); `tools/baseline-diffs.txt`: **389 tests** with a `MaxBlockAvgDiff`.
- Order used: stamp, suite, copy `LastTest.log`, `texture_tile`, probes, `record`.

### §1.5 T011: run-to-run noise on the unchanged tree

Same sequence a second time (stamp, suite, `texture_tile`, probes), `image_manifest.sh diff` against the baseline
with an empty noise list: 779 images, **6 differing, 0 missing**. The `MaxBlockAvgDiff` list of the second run is
identical to `baseline-diffs.txt`.
- `tests/imager/ri.tif` does **not** differ: 025's race fix (`tests/imager/guard`) arrived with the merge, as expected.
- **Surprise: the JIT raytrace probes are not stable.** `tests/jit-texture-probes/{texture-png,texture-exr,unbaked-nonpot,
  unbaked-png}-raytrace` differed in run 2; two further probe runs differed from each other in six images
  (texture-png, texture-exr, texture-rgbe, unbaked-exr, unbaked-png, unbaked-rgbe). Over four runs all seven
  `texture-*`/`unbaked-*` raytrace probes differed at least once. The `.slo` + reyes probes and `env`/`shadow` in both hiders were
  identical every time. The divergence of §1.9 (JIT against interpreter) is stable to two decimals. Cause not
  investigated (likely the JIT reads texture options from scratch it does not fill in; Phase 9 removes that). **All
  seven are in `tools/baseline-image-noise.txt`**, so CP-d cannot byte-check them; their Phase 9 comparison needs
  `MaxBlockAvgDiff` against the interpreter, as T016's script does.
- `tests/shading_parity/ri.tif` and `tests/shading_parity/test.tif` (several parity tests write the same name into one
  directory) are also in the noise list. With the list, `diff` reports **0 differing, 0 missing**
  (113 fresh determinism images are "not in manifest", information only).

### §1.6 T012: determinism set

`ctest -L '^determinism$' -j2`: **155/155 passed** (161.5 s). CLAUDE.md says 148; that figure dates from `2df9e4c`
(2026-10-19) and the label has grown since (no determinism-labelled test comes from specs 024/025's test changes). The pass
list is in `build-027-logs/027-base-det.log`.

### §1.7 T013: the opt-in sweep

`ctest -L '^determinism-sweep$' -j2`: **419/419 passed**, 718.6 s (about 12 minutes). Log `build-027-logs/027-base-sweep.log`.

### §1.8 T014: values for the Texturing-only program (SC-003)

`tools/lookupprobe.sl` compiled with `build-027/src/oshader/oshader`; `tools/lookup-probe.rib` rendered with `-t:1`
twice: byte-identical raw output (60 lines = 4 shading points x 15 values). `tools/master-lookup-values.txt` holds the
15 distinct lines (3 fixtures x footprints A-E), one value per key, none zero, and no "Failed open texture" on stderr.
The probe was cut down to what a caller-built `TextureOpt` can reproduce: eight-coordinate `texture()`, box filter,
`"samples" 1`; no blur, width, per-channel or other filter. (A first run with a wrong `TEXTURES` path printed zeros
and "Failed open texture"; it was discarded.)

### §1.9 T015, T016: timing and JIT divergence

SC-010 baseline (`tools/timing-base-macos.tsv`, 10 runs per scene, seconds):

| Scene | Threads | min | median | max |
|---|---|---|---|---|
| `concurrency-scene` | default | 0.418 | 0.460 | 0.698 |
| `concurrency-scene` | 1 | 0.636 | 0.702 | 0.780 |
| `texture-png-raytrace` | default | 0.124 | 0.140 | 0.184 |
| `texture-png-raytrace` | 1 | 0.191 | 0.197 | 0.243 |

Both scenes are short (0.1-0.8 s), so the spread is a large fraction of the value (up to 50% for `concurrency-scene` at
default threads): the noise bar for T121 is the max/min above.
JIT divergence (`jit_texture_divergence.sh`): **identical, line by line, to `tools/jit-texture-divergence-bc261b1-macos.tsv`**
(16 of 18 scenes differ, MaxBlockAvgDiff 34.55-252.52; `shadow` identical in both hiders).

## §2 Red evidence (phase 2)

## §3–§9 Phase checkpoints

## §10 Linux

### Baseline hand-off (T017, non-blocking)

For the maintainer's rsync + Maestri path (quickstart §5, first run). The tree to test is **`BASE`** (master `4eb683b`,
spec 025 merged), i.e. this worktree at `d75ead8` or master itself: nothing under `src/` differs.
1. rsync the tree to the Linux checkout excluding `build*/` (quote hosts as `${H}:path`), verify a `sha256sum` manifest remotely.
2. Fresh `build-027`, Release, GCC 13, `-Werror`: `nice -n 19 cmake --build build-027 -j4`.
3. Suite: `ctest --test-dir build-027 -LE determinism-sweep -j4`. Expected failures on Linux (known): `TextureTile_RenderByteIdentical`
   (#45) and `TextureTile_CacheByteIdentical`, `TextureTile_CacheDefaultDirectory`, `TextureTile_CacheStaleDetection`,
   `TextureTile_CacheConcurrentWrite` (#46). Then `ctest -L texture_tile -j2`.
4. `ctest -L architecture` (the two `linux` DB-026 lines are evaluated only there).
5. The Linux flat-import count by spec 024's method (`comm -12` of strong `U` symbols of `libshader_shading.so.1` and
   `LD_DEBUG=bindings` bindings to `libRIBRender.so.1`); macOS reads 7.
6. `ctest -L '^determinism$' -j2` (155 tests on macOS) and the sweep `ctest -L '^determinism-sweep$' -j2` (419 tests on macOS).
Report the numbers; they are appended here.

