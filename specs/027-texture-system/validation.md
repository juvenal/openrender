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

No file under `src/` changed in this phase (`git diff --stat 4eb683b -- src` is empty). What changed: tests, test
doubles, shaders (5 new), a RIB template, the characterization scenes and their scripts, CMake registrations,
5 new shader references (below), one reference file (`rgbe-display.hdr`, T029). Every job ran through `cpujob`.

### §2.1 Guard against a test that is red for the wrong reason (build failures)

The red state of every `EXCLUDE_FROM_ALL` test is a build failure, which looks the same for a typo and for a
missing API. So each new source was also compiled with `-fsyntax-only` and the project's `-Wall -Wextra
-Wpedantic -Werror` against **stub headers written from the contracts** (`textureSystem.h`, `textureResolver.h`
from contracts/texture-system.md; `rendererServices.h` with `textureSystem()` and the three 3D forwarders and
without the three removed getters; `texmake.h`/`brickmap.h`/`pointCloud.h` taking `const CTextureFileResolver *`;
a `textureCalls.h` declaring `texture3dAvailable`), in a scratch directory outside the worktree. All eight sources
(T020, T022/T022a, T022b x2, T023, T026, T027) compile clean, with and without `-DNDEBUG` and
`TEXTURE_SUBSTITUTE_HAVE_JIT`. So the first error of each real build below is the missing API, nothing else.

### §2.2 T033: the six `EXCLUDE_FROM_ALL` executables fail to compile

`cmake --build build-027 --target <t>` (CP-a with the option off is green: the main build is clean, exit 0):

| Target | First error |
|---|---|
| `test_texture_system_concurrent_first_load` | `texture_system_test_doubles.h:13:10: fatal error: 'ri/texture/textureResolver.h' file not found` |
| `test_texture_system_stream_cursor` | same header |
| `test_texture_system_info_miss` | same header |
| `test_texture_system_null_resolver` | `test_texture_system_null_resolver.cpp:24:10: fatal error: 'ri/texture/textureResolver.h' file not found` |
| `test_texture_substitute` | `recording_texture_system.h:35:10: fatal error: 'ri/texture/textureSystem.h' file not found` |
| `test_texture_substitute_3d` | same header |

### §2.3 T034: `TextureOnly_Standalone` in a scratch build

`build-027-red` (configured like `build-027` from its `CMakeCache.txt` plus `-DOPENRENDER_TEXTURE_ONLY_TESTS=ON`;
deleted afterwards; no archcheck there). `make -k` of `texturing_only_sources` and the executable:
- `texture.cpp`: `src/ri/texture/texture.h:30:10: fatal error: 'attributes.h' file not found`;
- `texmake.cpp`: `src/ri/texture/texmake.h:32:10: fatal error: 'options.h' file not found`
  (the two Graphics State headers Phase 4 takes out; `imageInput*.cpp` compile);
- `test_texture_only_standalone.cpp:28:10: fatal error: 'ri/texture/textureResolver.h' file not found`.
The library is named `texturing_only_sources` (as `state_only_sources`); tasks.md and research R15 now say so.

### §2.4 T035: the T-J tests (the known-red set)

`ctest -R 'ShadingParity_JitPL(_texture|_envshadow|Engaged_texture|Engaged_envshadow|DiagnosticsTexture)'`, run **three
times** (`027-p2-tjb1..3`): identical result every time, **46 tests: 32 failed, 14 passed**. The 32 failing names
are the known-red set, `tools/known-red-p2.txt`:
- all **17 parity cases** `ShadingParity_JitPL_{texture_{base,blur,width,swidth_twidth,samples,filter},envshadow_{base,blur,width,samples,bias,filter,raytrace_{base,samples_cone,bias,maxdist,blur}}}`
  (image difference: the JIT ignores the lists and requests a point lookup; even the two `base` scenes, which have
  no list, differ);
- **14 `_slo` engaged controls** (the JIT ignores the parameter, so base and case are identical);
- `ShadingParity_JitPLDiagnosticsTexture` (the JIT reports none of the three "Unknown parameter" lines).
The **14 `_rslo` engaged controls are green** (the interpreter honours the parameters): texture `blur`, `width`,
`swidth_twidth`, `samples`, `filter`; envshadow `blur`, `width`, `samples`, `bias`, `filter` and the raytrace
`samples_cone`, `bias`, `maxdist`, `blur` (five + five + four). No parity case passes on the starting tree, so there is
no characterization case. `ShadingParity_JitPLDiagnostics` and every #55 `ShadingParity_JitPL*` case stay green
(the `-L shading_parity` run's only failures are the known-red set).
- **`fill` dropped** (both builtins): the only image-level way to see `fill` is a missing map; a failed open is a
  CODE_NOFILE error that makes `orender` exit 65280 in both backends, so `test_render_compare` can never go green on
  it, before or after Phase 9. A black-wrap texture does not help (the interpreter's `lookup4` returns 0, not
  `fill`, outside [0,1]; only the point lookup, which the JIT uses, returns `fill`). Reason recorded in the table's
  comment in `tests/visual/CMakeLists.txt`; `fill` is covered at the services boundary (T026's no-system cases and
  the JIT's sentinel scratch). **Needs the maintainer's sign-off.**

### §2.5 T036: how T-C1's red is shown

(1) The build failure of T033 on the `BASE` tree (above); (2) at Phase 5's adapter step (T064) the assertion
failure (more than one `locate()`, unequal handles), before the system's own lock lands. A test through the
services on the starting tree would be green: the services' `shaderMutex` wrapper masks #20.

### §2.6 Characterization tests (green on the starting tree by design)

- `Display_RgbeBytes` (T028/T029): `tests/unit/image_input/rgbe-display.rib` (24x16, `-t:1`, 243 distinct pixel values, 9 exponents) rendered twice with the starting tree's `rgbe` driver gave identical bytes (1579 bytes);
  saved as `tests/unit/image_input/fixtures/references/rgbe-display.hdr`; the test passes (**needs the maintainer's approval of the reference**).
- The characterization scenes (T030a, `tools/scenes/`, `tools/characterization_scenes.sh`): `occlusion-` and
  `indirectdiffuse-envmap-{reyes,raytrace}` and the point-hierarchy pair, each as `.rslo` and `.slo` (14 renders). Proven
  to check something: with `"environmentmap" ""` instead of the map the four envmap images differ; the two
  point-hierarchy scenes (`maxsolidangle` 0.05 against 1.5) render different images. Rendering twice gave identical
  bytes (T035a noise re-check). Three new probe shaders were added for them (`char_*`).

### §2.7 T035a: the re-recorded baseline

No `src/` change since T010. The Phase 1 files are kept as `tools/baseline-images-p1.sha256`,
`baseline-diffs-p1.txt`, `baseline-image-noise-p1.txt`. Order: stamp, the suite, `texture_tile`, the 18 JIT probes, the
characterization scenes, `record`.
- `tools/baseline-images.sha256`: **833 images** (779 before: + the T-J scene outputs `tests/visual/jit-texture/`, the characterization
  scenes, `tests/shading_parity/cwd/*` outputs and the new tests' images); `tools/baseline-diffs.txt`: 389 tests with a `MaxBlockAvgDiff`
  (unchanged count: the new image tests are `test_render_compare`, which prints none).
- Suite: 885 registered; first run 33 failed (the 32 known-red and `ShaderCompilerImmutability`, see §2.8); `texture_tile` 9/9.
- **Noise re-check** (stamp, suite, tile, probes, scenes a second time): the `MaxBlockAvgDiff` list is identical; the
  only differing images are the JIT raytrace probes of §1.5 (3 of the 7 this time). **`tests/shading_parity/{ri,test}.tif` are stable
  since T032a**, and the T-J `.slo` scene images and the characterization scenes are stable. `tools/baseline-image-noise.txt` now lists
  **only the seven `.slo` raytrace probe images**; with it the diff is 833 images, 0 differing, 0 missing.
- The Phase 1 manifest would list the two old `tests/shading_parity/{ri,test}.tif` paths as MISSING: expected, replaced by this baseline.

### §2.8 T032a and the new shader references

- `tests/shading_parity/CMakeLists.txt` gives every test of that directory its own `cwd/<test name>` working directory (nothing a test asserts changes:
  the sources use absolute paths and `chdir` to their own `mkdtemp` directories). `ctest -L shading_parity`: the only failures are the known-red set.
- **`ShaderCompilerImmutability` requires a reference `.rslo` for every shader in `shaders/`.** The five shaders this phase adds
  (`jit_pl_texture_probe`, `jit_pl_envshadow_probe`, `char_occlusion_probe`, `char_indirectdiffuse_probe`, `char_bake_area`) had none, so it failed
  ("MISS [no reference]"). Their references were established the way the test says (copied from its scratch output): five **new** files in
  `tests/shaders/references/`; no existing reference changed. Needs the maintainer's approval like any reference.

### §2.9 T037: DISABLED tests

`ctest -R 'TextureSystem_|TextureOnly|ShadingParity_TextureSubstitute'` (only the nine disabled tests): all print `Not Run (Disabled)`, exit status 0.
In the label run `-L 'texture_tile|shading_parity|image_input|texture_only'` ctest lists them as "(Disabled)" but counts 32 failures out of 134,
and the failing set equals the known-red set; `Display_RgbeBytes` passes. (`TextureOnly_Standalone` does not exist with the option off.)

### §2.10 Checkpoint 2 results (CP-a, CP-b, CP-c)

- CP-a: `cmake --build build-027 --config Release -j4`: clean (exit 0).
- CP-b (final run after the references were added): `ctest -LE determinism-sweep -j4`: **885 registered, 32 failed = exactly `known-red-p2.txt`**, 9 DISABLED not run;
  `texture_tile` 9/9. A red test from the known-flaky set: none.
- CP-c: `ctest -L architecture` 21/21; `ALLOWED DB-001 x 8 DB-004 x 65 DB-005 x 8 DB-006 x 33 DB-010 x 35 DB-011 x 173 DB-015 x 10 DB-017 x 78 DB-019 x 3 DB-021 x 1 DB-025 x 12 DB-026 x 3`;
  `METRIC Texturing refs 70 distinct 16`, `TOTAL refs 721 distinct 110`; `RESULT PASS` (`tools/archcheck-p2-macos.txt`).

### §2.12 Checkpoint 2 approval (maintainer, 2026-10-27)

Approved: all Phase 2 tests, doubles and registrations, **including T032a's per-test working directories**; the known-red set of 32
(`tools/known-red-p2.txt`); **the drop of the JIT `fill` cases** (decision recorded: `fill` stays covered by T026's no-system cases and the
JIT sentinel scratch; reason in §2.4); and **the six new reference files**: the five shader compile references (`jit_pl_texture_probe`,
`jit_pl_envshadow_probe`, `char_occlusion_probe`, `char_indirectdiffuse_probe`, `char_bake_area`) and
`tests/unit/image_input/fixtures/references/rgbe-display.hdr`. The code-derived JIT pins of T026/T027 stand until T082, where any mismatch
goes back to the maintainer (no pin is edited to match).

### §2.11 Mutation plan (each at its green point, reverted, `git diff` clean)

| Test | Green at | Mutation |
|---|---|---|
| `TextureSystem_NullResolver` | T050a | drop the null check in the resolver wrapper |
| `TextureSystem_InfoMiss` | T063a | cache the `nullptr` of an Info miss |
| `TextureSystem_ConcurrentFirstLoad` (+ ExternalMutex) | T064 / T071a | remove the lock (the earlier assertion failure is the proof); the re-entry death subtest runs once in a scratch Debug build (T071a b2) |
| `TextureSystem_StreamCursor` | T081 | one extra `opt.stream.next()` in `CMadeTexture::lookup4` |
| `ShadingParity_TextureSubstitute` | T082 | drop one field (`blur`) from `textureOptions()` |
| `ShadingParity_TextureSubstituteHiderNoSystem` | T082 | make `lookupEnvironment()` call a null handle (no fill return) |
| `ShadingParity_TextureSubstitute3D` | T089 | `texture3dResolve` path ignoring the returned size, or dropping `radius` |
| `TextureOnly_Standalone` | T094 | perturb one lookup |
| `Display_RgbeBytes` | T043 | change one codec byte |
| `Common_StateOwnership` | T043 | keep `rgbe.cpp` in `ri_sources` |
| T-J tests | red at T035 | two scratch mutations at Phase 9 (T106) |
| characterization scenes | T080a / T088a | skip `commitTextureDraws()` (fallback: negate `D0`/return `fill`); wrong `maxSolidAngle` accessor |

## §3 Phase 3: the RGBE codec (US4, FR-017, DB-026)

**T039.** The 5 DB-026 lines and their section header (7 lines with the blank) are deleted from `tests/architecture/allowlist.txt`; nothing added.
`ctest -L architecture`: **red, exactly 3 `VIOLATION`s on macOS**, all from `imageInputRgbe.cpp`:
`RGBE_ReadHeader(__sFILE*, ...)` and `RGBE_ReadPixels(__sFILE*, ...)` links, and the `src/display/rgbe/rgbe.h` include (`Texturing -> Display`); `stale=0`.

**T040.** The section `# --- Spec 027: the RGBE codec ---` with the six `RGBE_*` names was added to `tests/unit/common/owned-symbols.txt` before the move:
`Common_StateOwnership` **red**: `NOT DEFINED RGBE_WriteHeader ... RGBE_ReadPixels_RLE (expected exactly one definer: libRIBCommon)`, "6 of 270 symbols are not defined once"
(the names were still C++-mangled, and `ribRender`, `ribVector` and `rgbe` each defined the codec).

**T041.** `git mv src/display/rgbe/rgbe.{h,cpp} src/common/` (body unchanged); the six declarations are wrapped in `extern "C"`; `rgbe.cpp` joined `openrendercommon_obj`;
`../display/rgbe/rgbe.cpp` left `ri_sources`; `file.cpp`, `imageInputRgbe.cpp` and `rgbe.cpp` include `common/rgbe.h`; the stale comments in `imageInputRgbe.h`,
`test_image_input_rgbe.cpp`, `FIXTURES.md` and `DEVNOTES.md` (the spec 018 note) now name the new location. `grep -rn 'display/rgbe/rgbe' src tests DEVNOTES.md CLAUDE.md` finds only
the DB-026 evidence text in `DOMAIN_BOUNDARIES.md`. `dsply.h` and `src/display/rgbe/CMakeLists.txt` are untouched. The build is clean (`-Werror`).

**T042.** `Common_StateOwnership`, `Display_RgbeBytes` (**same bytes as the starting tree's reference**), `ImageInput_Rgbe`, `Parity_{texture,unbaked}-rgbe` and the four
`DeterminismSweep_parity-*-rgbe-*`: 9/9 green. `nm -gU`: the six `RGBE_*` names are defined in `libRIBCommon` only; `libRIBRender`, `libRIBVector` and `rgbe.dsply` define none.

**T043 mutations** (each reverted; `git diff` of those files is clean):
(a) `rgbe[3] = (unsigned char)(e + 129)` in `float2rgbe` (one byte the codec writes): `Display_RgbeBytes` **red** ("differ: char 47");
(b) `../common/rgbe.cpp` put back into `ri_sources`: `Common_StateOwnership` **red** (4 definers: `libRIBRender`, `libRIBVector`, `test_common_crosslib_static`, `libRIBCommon`).

**T044.** DB-026 moved to Resolved ("Resolved by spec 027 (`specs/027-texture-system/`), 2026-10-27"), tracking line `#33` only; evidence kept, where the codec lives now added. The file quotes no registry-wide allowlist total.

**Checkpoint 3 (T045), macOS.**
- CP-a: `cmake --build build-027 --config Release -j4`: clean.
- CP-b: `ctest -LE determinism-sweep -j4`: 885 registered, **32 failed = exactly `tools/known-red-p2.txt`**; `texture_tile` 9/9.
- CP-c: `ctest -L architecture` 21/21; `ALLOWED DB-001 x 8 DB-004 x 65 DB-005 x 8 DB-006 x 33 DB-010 x 35 DB-011 x 173 DB-015 x 10 DB-017 x 78 DB-019 x 3 DB-021 x 1 DB-025 x 12` (**no `DB-026`**, nothing added); Texturing 70/16, TOTAL 721/110; `RESULT PASS`, no `STALE`, no `VIOLATION`.
- CP-d: after the suite, the probes and the characterization scenes, `image_manifest.sh diff` against the T035a baseline with the noise list (the seven `.slo` raytrace probes): **833 images, 0 differing, 0 missing**;
  every `MaxBlockAvgDiff` equals `baseline-diffs.txt`; `jit_texture_divergence.sh`: 17 of the 18 rows equal `jit-texture-divergence-base-macos.tsv`, all seven `.slo` raytrace rows included except one
  one-off: `unbaked-rgbe-raytrace` read 84.94 in the checkpoint run (baseline 74.11) and **74.11 in each of four immediate re-runs**. That probe is one of the unstable `.slo` raytrace renders
  of §1.5 (the divergence is `.slo` against `.rslo`; the `.slo` side moves), and its `.rslo` side and every other RGBE row (`texture-rgbe` both hiders, `unbaked-rgbe-reyes`) are unchanged, so it is the known
  noise, not an effect of the codec move.
  The maintainer approved this reading at Checkpoint 3 (2026-10-27): the one-off 84.94 is the known `.slo` raytrace noise.

## §4 Phase 4: file search through a resolver, State includes dropped (US1, FR-007, FR-019)

**T046.** The 13 DB-010 lines of the contract's phase-4 table were deleted by key (22 DB-010 lines left, nothing else touched).
`ctest -L architecture`: **red, exactly 13 `VIOLATION`s**, all under `src/ri/texture`: includes `brickmap.cpp` and `texmake.cpp` -> `renderer.h`,
`texmake.h` -> `renderer.h` and `options.h`, `brickmap.h`, `pointCloud.h`, `pointHierarchy.h` -> `options.h`, `texture.h` -> `attributes.h`,
`texture3d.h` -> `xform.h`; links `brickmap.cpp`, `texmake.cpp`, `texture.cpp` -> `CRenderer::locateFile`, `texture.cpp` -> `CRenderer::texturePath`
(`stale=0`; `failing-cycles=1` is the consequence).

**T047-T053.**
- `src/ri/texture/textureResolver.h`: `CTextureFileResolver` (includes only `common/`), plus the inline `textureLocate(resolver, result, name)`, which
  calls the resolver, or, for a `nullptr` resolver, uses the name as given when the file exists (below).
- `src/ri/render/searchPathResolver.h` and `CSearchPathResolver::locate()` in `rendererFileSearch.cpp` (compiled into `ribRender` and `ribVector`): `CRenderer::locateFile(result, name, path)`.
- `makeTexture`, `makeSideEnvironment`, `makeCubicEnvironment`, `makeSphericalEnvironment`, `makeCylindericalEnvironment`, `makeBrickMap` (and `pointCloud.h`'s friend declaration) take `const CTextureFileResolver *`;
  `CRenderer::textureLoad`/`environmentLoad` and `writeTextureCacheEntry` take it too (the disk-cache writer passes it to `makeTexture`).
- `rendererFiles.cpp` (`getTexture`, `getEnvironment`, `getTextureInfo`) and the six `RiMake*V` functions in `rendererContext.cpp` build a `CSearchPathResolver` on the stack over the texture path.
  No other caller of the `make*` functions exists in `src` (the preview test calls `makeBrickMap` with `NULL`).
- Dropped: `attributes.h` from `texture.h`, `xform.h` from `texture3d.h`, `options.h` from `pointHierarchy.h`, `brickmap.h`, `pointCloud.h`, `texmake.h`; `renderer.h` from `texmake.{h,cpp}` and `brickmap.cpp`.

**T054 sweep.** The only includes the tree needed were infrastructure: `common/algebra.h` added to `texture3d.h` and `texmake.cpp` (the dropped State headers had pulled in `matrix` and the vector macros).
No Shading, Hiders, Graphics State or core header was needed. Clean build under `-Werror` (`ribVector`, `ribOut`, the preview and the tests included).

**T050a: null resolver; two corrections to the plan.**
- The plan said no caller relies on the core's temporary-directory fallback and that `tests/preview/test_data_keys.cpp` passes a path with a separator. **Wrong**: it passes the bare name
  `test_data_keys_scratch.ptc` with a NULL path, and `locateFile` finds it through `temporaryPath` (empty in that process, i.e. the working directory). The first version of the null branch
  ("a bare name is not found") turned `test_data_keys` red. The null branch now uses the name **as given** when the file exists (a path, or relative to the working directory); nothing else is searched.
  contracts/texture-system.md and T050a say so. `test_data_keys` passes unchanged.
- `TextureSystem_NullResolver` enabled (`ENABLED` keyword of `add_texture_system_test`) and green with `test_data_keys`, `PointCloud` and `data` tests. **Two setup fixes to the approved test** (no assertion changed):
  (1) `globalArena.init()`/`tini()` around the calls: `makeTexture` reads its source into the process-wide arena (`arenaBegin(globalArena)`), which crashed with no `RiBegin` (the same line was added to
  `TextureSystem_StreamCursor`, which builds maps through the `make*` functions); (2) one throwaway `makeTexture` before the two compared bakes: **the first `makeTexture` of a process writes a different, smaller file
  than every later one** (1878 against 2142 bytes for `tiny_rgb8.png`), independent of the resolver (process state, present on the starting tree); the comparison needs both bakes in the steady state.
  The difference is reported, not fixed (behaviour-preserving phase).
- **Approved by the maintainer at Checkpoint 4 (2026-10-27)**: the null-resolver semantics above (the name as given, a path or relative to the working directory, when the file exists; nothing else
  searched), and the two setup fixes to the approved tests (`globalArena.init()/tini()` in `TextureSystem_NullResolver` and `TextureSystem_StreamCursor`; the throwaway first `makeTexture`).
  The first-bake size difference is filed as GitHub **#109** and cited in the test's comment.
- **Mutation**: `textureLocate` dereferencing the resolver without the null check: `TextureSystem_NullResolver` **and** `test_data_keys` red (segfault). Reverted.

**T055.** `grep '#include ".*\(renderer\|options\|attributes\|xform\)\.h"' src/ri/texture` finds only `texture.cpp` (until Phase 5) and `texture3d.cpp` (FR-015).

**Checkpoint 4 (T056), macOS.**
- CP-a: clean build. CP-b: 886 registered, **32 failed = exactly `tools/known-red-p2.txt`**; `texture_tile` 10/10 (the nine plus `TextureSystem_NullResolver`).
- CP-c: `ctest -L architecture` 21/21, `RESULT PASS`, no `STALE`, no `VIOLATION`; `ALLOWED DB-010 x 22` (13 gone, nothing added), the other IDs unchanged;
  `METRIC Texturing refs 60 distinct 14` (baseline 70/16: `locateFile` 9 and `texturePath` 1 left), `TOTAL 711 / 110` (the baseline file is lowered by T114).
- CP-d: 833 images, 0 differing, 0 missing against the T035a baseline (noise list: the seven `.slo` raytrace probes); every `MaxBlockAvgDiff` equals `baseline-diffs.txt`;
  `jit_texture_divergence.sh` (absolute output path): **all 18 rows equal** `jit-texture-divergence-base-macos.tsv`, the seven `.slo` raytrace rows included.

## §5 Phase 5: the texture system owns the state (US1; FR-001, FR-002, FR-006, FR-016, FR-018(c); GitHub #20)

**T057.** The 10 phase-5 DB-010 lines were deleted by key (12 DB-010 lines left): `texture.cpp -> render/renderer.h` and its links `numThreads`, `textureMutex`, `synthesizeMutex`,
`textureMaxMemory`, `textureRefNumber`, `textureUsedBlocks`, `textureUsedMemory`, `textureCacheEnabled`, `textureCacheDirectory`. `ctest -L architecture`: **red, exactly 10 `VIOLATION`s**, all from `texture.cpp`; `stale=0`.

**T058-T062.**
- `src/ri/texture/textureSystem.h` (contract, without the lookup virtuals): `TextureHandle`/`TexturePerthread` (opaque), `ETextureKind`, `TextureSampleStream`, `TextureOpt`, `TextureInfo`, `CTextureSystemSettings`, `CTextureSystem`
  (`handle()`, `perthread()`, `describe()`), `createTextureSystem()` (`nullptr` and `CODE_BUG` unless `numThreads >= 1` and `resolver != nullptr`). Includes `common/` only.
- `src/ri/texture/textureInternal.h` (Texturing-private): `CTextureCache` (the used-blocks list, the registry/eviction mutex that was `CRenderer::textureMutex`, the per-thread array) and `TexturePerthread`
  (`thread`, `refNumber` as one separate `int` per thread, `usedMemory`, `maxMemory = ceil(total / numThreads)`, `scratch` = the system's own `CMemPage`); `textureLoadTexture` / `textureLoadEnvironment`.
  The pyramid-synthesis lock is a function-local `std::mutex` in `texture.cpp`.
- `texture.cpp`: every block carries its `CTextureCache *` (set at registration, given to each layer and deep-shadow tile at construction); `textureMemFlush`, `textureAllocateBlock`, register/unregister, the three reference-number updates and the
  deep-shadow tile lock use the cache and `TexturePerthread` instead of the `CRenderer` statics. Eviction order, locks and per-block mutexes are moved, not changed (#19 stays out). `CRenderer::textureLoad`/`environmentLoad`/`initTextures`/`shutdownTextures`
  became `textureLoadTexture`/`textureLoadEnvironment` and the cache's constructor and destructor. The lookups still take `CShadingContext *` and use `context->thread`; the eviction scratch memory is now the thread's own `CMemPage` instead of `context->threadMemory`.
- `src/ri/texture/textureSystem.cpp` (in `ri_sources`, so `ribVector` has it): `CTextureSystemImpl` with the texture-family `CTrie<CFileResource*>` (split out of `frameFiles`, which keeps photon maps, irradiance caches and 3D textures), `handle()` reproducing `getTexture`, `getEnvironment`
  and `getTextureInfo` exactly (substitutes and the two `CODE_NOFILE` texts; an `Info` miss returns `nullptr` and caches nothing), `perthread()`, `describe()`.
- Core: `renderer.h` lost the six texture statics, `textureMutex`, `synthesizeMutex` and the members `textureLoad`, `environmentLoad`, `getTexture`, `getEnvironment`, `getTextureInfo`, `initTextures`, `shutdownTextures` (their definitions in
  `rendererStatics.cpp`, `rendererMutexes.cpp` and `rendererFiles.cpp` are gone); it gained `CRenderer::textureSystem` and `CRenderer::textureResolver` (a `CSearchPathResolver` over `texturePath`). `beginFrame()` constructs the system where `initTextures()` ran;
  `endFrame()` deletes it (and the resolver) where `shutdownTextures()` ran, after `frameFiles->destroy()`. The services' three texture getters keep their `shaderMutex` wrapper and forward to `CRenderer::textureSystem->handle()` (a temporary adapter).
- **T063.** Clean build under `-Werror`; `ctest -L architecture` 21/21, **no `VIOLATION`**, no `STALE`; `texture_tile` green with the nine existing tests.

**T063a.** `TextureSystem_InfoMiss` enabled and green. **Test fix (for approval)**: it expected one `locate()` per `Info` probe; today's `getTextureInfo` tries an environment and then a texture, and each loader locates, so one probe is **two** locates (messages unchanged, which the plan requires).
The assertion now reads: the second probe locates again by the same amount as the first (`2 * perProbe`), i.e. nothing was cached. **Mutation**: caching a substitute for an `Info` miss: the test is **red** (`handle(missing, Info) == nullptr` fails on the second call). Reverted.

**T064.** `TextureSystem_ConcurrentFirstLoad` enabled (the external-mutex entry stays disabled). With the moved find-load-insert and **no lock**: **red by assertion**, in the first repeat:
`CHECK FAILED: existing[i] == existing[0] ... test_texture_system_concurrent_first_load.cpp:94 / failed in repeat 0` (unequal handles). This is FR-018(c)'s red, with T033's build failure (§2.2).

**T065.** `handle()` holds `settings.loadMutex` when non-null, else the system's own mutex (`loadMutex` is `nullptr` in this phase: the services' getters still hold `CRenderer::shaderMutex`, a plain mutex), across find, load and insert.
`TextureSystem_ConcurrentFirstLoad` (4 threads, 200 repeats, one handle, one locate, one `CODE_NOFILE`): **green, three runs in a row at `-j2`**.

**Approved by the maintainer at Checkpoint 5 (2026-10-27):** the corrected `TextureSystem_InfoMiss` assertion (a second `textureinfo()` probe locates again by the same amount as the first, `2 x perProbe`); and the more frequent `.slo` raytrace probe noise is
accepted as the uninitialized-scratch noise Phase 9 removes (no comparison tree is built).

**Checkpoint 5 (T066), macOS.**
- CP-a: clean build. CP-b: 888 registered, **32 failed = exactly `tools/known-red-p2.txt`**; `texture_tile` at `-j2`: 12/12 (the nine, `TextureSystem_NullResolver`, `TextureSystem_InfoMiss`, `TextureSystem_ConcurrentFirstLoad`; the external-mutex entry is disabled).
- CP-c: `ctest -L architecture` 21/21, `RESULT PASS`, no `STALE`, no `VIOLATION`; `ALLOWED DB-010 x 12` (the 8 hider-origin lines and 4 Texturing lines: `texture.cpp` and `pointHierarchy.cpp` -> `core/shading.h`, `texture3d.cpp` -> `renderer.h` and `retrieveDisplayChannel`), nothing added.
  `METRIC Texturing refs 1 distinct 1` (baseline 70/16) and `TOTAL refs 652 distinct 98` (baseline 721/110): the end figures of the spec, already reached (the baseline file is lowered at T114).
- CP-d: 833 images, 0 differing, 0 missing (noise list: the seven `.slo` raytrace probes); every `MaxBlockAvgDiff` equals `baseline-diffs.txt`. The `Parity_{texture,unbaked}-*`, `Parity_env`, `Parity_shadow` scenes, the nine `texture_tile` tests and the determinism tests are in that comparison and unchanged.
- **Divergence rows** (`jit_texture_divergence.sh`, absolute output path): in the checkpoint run `texture-png-raytrace` read 91.55 (86.20) and `unbaked-exr-raytrace` 84.91 (74.38); four immediate re-runs gave all 18 equal once and otherwise one to three deviating rows from the set
  {`texture-png-raytrace` 91.55, `unbaked-exr-raytrace` 84.91, `unbaked-rgbe-raytrace` 84.94, and once `unbaked-rgbe-reyes` 81.91}. Six runs of `jit_texture_probes.sh` changed only `.slo` **raytrace** probe images (the `.slo` reyes images and every `.rslo` image were identical every time).
  These are the `.slo` renders of §1.5, whose options are uninitialized scratch memory (research R10): the deviating values come back as the same few numbers (84.94 first appeared at Checkpoint 3, before this phase), and the noise shows more often after this phase's allocation changes. Not an interpreter change.
  Reported for the maintainer; Phase 9 removes the cause (T107a).

## §6 Phase 6: the option block and the services (US1, US2, US3; FR-003, FR-005, FR-009, FR-010, FR-012, FR-013)

**Checkpoint 5 approvals** are recorded in §5. This phase is the compile-broken span T069 to T080: the compiler was the work list, and one `-k` build after the edits was clean.

**T067.** The DB-010 line `texture.cpp -> core/shading.h` was deleted by key (11 DB-010 lines left). `ctest -L architecture`: **red, exactly 1 `VIOLATION`** (`texture.cpp:33`).

**T068-T070.** `textureSystem.h` gained `texture()`, `texturePoint()` and `environment()`; `CTextureSystemImpl` dispatches to the handle's object. `CTexture::lookupz/lookup/lookup4` and `CEnvironment::lookup` take `(..., TextureOpt &opt, TexturePerthread *pt)`; the layer-level
functions (`lookupPixel`, `textureLoadBlock`, `textureAllocateBlock`, `textureMemFlush`, `CDeepShadow::loadTile`) take the `TexturePerthread *`. Options come from `opt.{filter,blur,fill,samples,traceSamples,bias}`, draws from `opt.stream.next2/next4` (same dimensions in the same order),
`pt->thread` and `pt->scratch` replace the context reads; the nested side lookups of the spherical and cylindrical environments and of `CShadow` share the caller's `opt`. `texture.h`/`texture.cpp` no longer include `shading.h` (T083 below).

**T071.** `rendererServices.h`: `getTexture`, `getEnvironment`, `getTextureInfo` and the forward declarations of `CTexture`, `CEnvironment`, `CTextureInfoBase` are gone; `CTextureSystem *textureSystem()` is in. `rendererServicesImpl.h` returns `CRenderer::textureSystem` and the three getters
and their `shaderMutex` wrapper are deleted. **In the same step** `beginFrame()` passes `&shaderMutex` as `CTextureSystemSettings::loadMutex`. Reading every remaining `shaderMutex` holder: only `getPhotonMap`, `getCache` and `getTexture3d` (`rendererServicesImpl.h:64-81`)
hold it, and none reaches a texture handle: no code that holds the mutex calls `handle()`.

**T071a.** (a) `CTextureSystemImpl::handle()` has a per-thread depth counter (`thread_local`, compiled only where `NDEBUG` is undefined) and an `assert` that a thread already inside `handle()` does not enter it again. (b) `TextureSystem_ConcurrentFirstLoadExternalMutex` is enabled:
`ctest -R ConcurrentFirstLoad -j2`, **three runs, both entries green every time** (the re-entry subtest reports "skipped (NDEBUG)" in this Release build). (b2) **Debug run, once**: a scratch tree `build-027-dbg` configured like `build-027` (same generator, toolchain, `VCPKG_*`, bison and deployment target)
with `-DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_CXX_FLAGS_RELWITHDEBINFO="-O0 -g"` (no `NDEBUG`; vcpkg's libraries are release-only, so a `Debug` configuration has nothing to map to) and `-DOPENRENDER_ENABLE_JIT=OFF` (at `-O0` the test executable, which links no LLVM, otherwise failed to load: `symbol not found in flat namespace '__ZTIN4llvm13ErrorInfoBaseE'`;
the subtest has nothing to do with the JIT). Built only `test_texture_system_concurrent_first_load` through `cpujob 027-p6-dbg2`, then `cpujob 027-p6-dbg-run ctest --test-dir build-027-dbg -R ConcurrentFirstLoadExternalMutex -j2 -V`:
`Assertion failed: (t_handleDepth == 0 && "CTextureSystem::handle() re-entered: the load lock is not recursive"), function handle, file textureSystem.cpp, line 89.` then `re-entry assertion fired (child ended with SIGABRT)` and the 200-repeat, 4-thread assertions: **Passed**. `build-027-dbg` deleted (not staged).
(c) **Core-lock coupling** (for T115 and T117 to copy): *the core passes `&CRenderer::shaderMutex` as `loadMutex`; it is a plain non-recursive mutex; `handle()` must never be called by code that already holds it; the services getters that hold it (`getCache`, `getTexture3d`, `getPhotonMap`) never reach a texture handle; a new caller that does is a self-deadlock*.

**T072.** `CForwardingServices`, `CRecordingServices` and `CStubServices` follow the interface (`textureSystem()`, `CStubServices` returns `nullptr`; the three getters dropped). No other implementation of `CRendererServices` exists besides `CRendererServicesImpl` and the test double `CTextureServices`. No assertion changed.

**T073-T079.**
- New `src/libshader/shading/textureCalls.{h,cpp}` (namespace `libshader`): `textureSystemOf()`, `textureHandle(context, name, kind, builtin)` (null with no services or no system; one `CODE_NOTSTARTED` warning per builtin per process, a `std::atomic<bool>` each for `texture`, `environment`, `shadow`, `textureinfo`, the messages name the builtin) and `texture3dAvailable()` (Phase 7).
- `shading.h`/`shading.cpp`: `CShadingContext::textureOptions()` (the scratch's texture and trace parameters and `{sampling.backend, drawCursor, drawCursorDim}`), `commitTextureDraws()` (`drawCursorDim = opt.stream.dim`), `lookupEnvironment()` (fill when the handle or the system is null); `rendererGetTexture/Environment/TextureInfo` removed.
- `shaderPl.h`: `CTextureLookup::map`, `CEnvironmentLookup::map`, `CMapInfoLookup::map`, `COcclusionLookup::environment` and `occlusionParams.environment` are `TextureHandle *`.
- `shaderFunctions.h`: the four `texture()` forms use `TEXTURE_LOOKUP` (options per point, `texture()`, commit; fill with no handle); `environment()`/`shadow()` decide ray tracing **by name** (`traced`: `"raytrace"` or the shader's own name) and take the handle otherwise (`ENVIRONMENT_LOOKUP`); `textureinfo()` goes through `ETextureKind::Info` and `describe()`
  (`found = hasViewMatrix/hasProjectionMatrix`, a null handle writes nothing). `giFunctions.h`: the occlusion/`indirectdiffuse` environment handle through `textureHandle`.
- `shading.cpp`: `jitTextureF/C` (point `texturePoint`), `jitEnvironmentF/C` (`d,d,d,d`), `jitShadowF` (`p,p,p,p`), `jitTextureInfo` and the occlusion environment keep today's requests; options from `textureOptions()`; a null handle writes `opt.fill` (textureinfo: nothing, found 0).
- `rslBuiltins.cpp` `rsl_texture_c_scalar` (no caller) routes through `textureCalls`; `execute.cpp` includes only; `irradianceDispatch.cpp` calls `context->lookupEnvironment(...)` at both sites around its unchanged `traceParams.samples = 1` trick, with no new include.

**T080.** The sweep: one `-k` build after the edits found five `-Wunused-parameter` errors in `texture.cpp` (parameters that no longer have a use: the two `lookupz` overrides and the three dummies), fixed by dropping the names; then clean under `-Werror`. Includes added: `textureSystem.h` in `shading.h` and `texture.h`; `textureCalls.h` in `shading.cpp`, `execute.cpp` and
`rslBuiltins.cpp` (replacing `texture.h` in the last two). `shading.cpp` still includes `texture.h` and `texture3d.h` (3D, Phase 7). No Texturing unit needed a forbidden header.

**T080a.** `jit_texture_probes.sh` and `characterization_scenes.sh` rendered (14 characterization scenes, 18 probe images): `image_manifest.sh diff` over the 32 fresh images: **0 differing** (7 noise-listed). **Mutation**: `commitTextureDraws()` skipped in `lookupEnvironment()`: **8 differing**, all the `occlusion-envmap-*` and `indirectdiffuse-envmap-*` scenes (`reyes` and `raytrace`, `.rslo` and `.slo`). The first mutation sufficed (the scenes do draw after the environment lookup); the fallback was not needed. Reverted (byte-identical to the saved copy).

**T081.** `TextureSystem_StreamCursor` enabled; **green** (baked 2D 6, unbaked 2D 6, point 0, `CShadow` 16, `CDeepShadow` 16, cubic 8, spherical 6, cylindrical 6, substitutes 0, the counts in the test). **Mutation**: one extra `opt.stream.next()` in `CMadeTexture::lookup4`: **red** (`baked 2D lookup4: consumed 9 dimensions, expected 6`, and the unbaked, spherical and cylindrical rows). Reverted.
**Two fixes to the approved test, approved by the maintainer at Checkpoint 6** (neither touches an assertion's meaning; both found when the test first ran):
1. The test baked its cubic, spherical and cylindrical environments from `examples/rib/tests/env-source.tex`, which is itself a **baked** environment texture; `makeTexture`-family functions refuse a baked source (`Unsupported or unrecognized source image format`, a 69-byte unusable cubic file, `Failed open environment`). `otexmake -envlatl` on `tests/unit/image_input/fixtures/medium_rgb.tif`
   is how `env-source.tex` was made (the same 31178 bytes), so the source is now `medium_rgb.tif`, found through the same resolver.
2. The "unbaked 2D with blur: 4 S" row did not hold: a PNG is synthesized into a pyramid (`CMadeTexture`, where blur only picks the level), and the class that draws a second pair per sample with `blur > 0` is `CRegularTexture`, which a **plain TIFF** reaches (data-model.md section 5 and tasks.md T-K1 named the PNG). The test now pins both: the PNG with blur draws 2 S, and
   `tiny_rgb.tif` (a plain TIFF, already in the image-input fixtures) draws 2 S without and 4 S with blur. data-model.md section 5 states the two classes.

**T082 (green after the maintainer's Checkpoint 6 decision, 2026-10-28).** `ShadingParity_TextureSubstituteHiderNoSystem` is green (the first point at which the null-handle `lookupEnvironment()` fallback exists). The first run of `ShadingParity_TextureSubstitute` had **92 failed assertions of 1020, every one in the `.slo` half's result pins**; the interpreter half (handle requests, the four corners,
every option of every probe including the parameter-list probes, the canned colors, the textureinfo result) and the JIT's *requests* (handle, point lookups, the repeated direction of `environment()`, the scratch options) passed. The causes, from a debug print in a scratch copy of the test:
- `tex_c` and `env_c` `.slo`: the JIT writes **one** channel (`ci = 0.125 -1 -1`; the harness pre-fills `-1`): research R22's second row (color forms take the float-channel entry point).
- `shadow_f` `.slo`: the recorded points `P0..P3` are garbage (`0, 8.4e-39, 1.4e-45`): **new**, the `shadow` emitter branch (`getVar(ins, 1)`) passes operand 1, the channel, as `Ps` (operand 2). Added to research R22.
- `textureinfo` `.slo`: `found` right, resolution values garbage (`3.2e-35, 1.4e-45`); cause to be traced in Phase 9. Added to research R22.
None comes from this phase (the `jit*` bodies make the same requests as before; the emitter and the `op_*` entry points are untouched). **Decision (maintainer):** these JIT result pins move to Phase 9 with T104's switch. The test now guards exactly them with `kJitResultPins` (compiled, skipped, with a printed note) unless `TEXTURE_SUBSTITUTE_JIT_RESULTS` is defined, which T104 does:
the `.slo` color results of `tex_c`/`env_c`, `shadow_f`'s recorded `P` (its options stay pinned), the `textureinfo` resolution values, the cross-backend color equality of `tex_c`/`env_c`/`textureinfo`, and the no-system fill of the JIT color forms. No other assertion changed. Result: both tests **Passed**. The `textureOptions()` mutation (no `blur`) was red before the restructure (92 to 124 failed assertions, all the `_pl` blur option checks of the interpreter half), and those checks are untouched.
R22, the T105 list, T104 and the plan name the two new findings.

**T083.** `grep -rnE '\b(CTexture|CEnvironment|CTextureInfoBase)\b' src/libshader`: nothing. `grep -rnE '(->|\.)(lookup4|lookupz)\(' src/libshader`: nothing. `grep -rn CShadingContext` over `texture.h`, `texture.cpp`, `textureSystem.*`, `textureInternal.h`: nothing. The Shading -> Texturing calls that still carry a context are the two
irradiance-cache lookups (`giFunctions.h:235`, `shading.cpp:3443`, `cache->lookup(..., this)`), SC-005's 3D exception, plus the 3D texture and point-cloud calls of Phase 7.

**Checkpoint 6 (T084), macOS** (`cpujob 027-p6-*`, logs in `build-027-logs/`).
- CP-a: clean build, `-Werror`. CP-b: 892 registered (2 disabled: `ShadingParity_TextureSubstitute3D{,NoServices}`, 1 skipped), **33 failed = `tools/known-red-p2.txt` (32) + `ShadingParity_TextureSubstitute`** (T082 above). `texture_tile` at `-j2`: 14/14 (the nine, `NullResolver`, `InfoMiss`, `ConcurrentFirstLoad` and its external-mutex entry, `StreamCursor`).
- CP-c: `ctest -L architecture` 21/21, `RESULT PASS`, no `STALE`, no `VIOLATION`; `ALLOWED DB-010 x 11` (the 8 hider-origin lines and 3 Texturing lines), nothing added. `METRIC Texturing refs 1 distinct 1`, `TOTAL refs 652 distinct 98`. Shading's flat-namespace imports: **7**, the same seven symbols as `libshader-dynlookup-before-macos.txt`
  (`CTexture3d::resolve` leaves in Phase 7).
- CP-d: 833 images, **0 differing, 0 missing** (noise list: the seven `.slo` raytrace probes); all 389 `MaxBlockAvgDiff` equal `baseline-diffs.txt`. In that comparison and unchanged: `TextureSystem_StreamCursor`, the determinism tests, the occlusion and indirectdiffuse `ShadingParity_JitPL*` tests, `Visual_sphere-{occlusion,texture3d-bake3d,textureinfo}-reyes{,-slo}`, the `Parity_{texture,unbaked}-*`, `Parity_env`, `Parity_shadow` scenes.
- **Divergence rows** (`jit_texture_divergence.sh`, absolute output path): 17 of 18 equal to §0.7; `texture-png-raytrace` read 91.55 (86.20), the known noise value of §5 (the `.slo` raytrace renders read uninitialized scratch; Phase 9, T107a).

## §7 Phase 7: 3D channel operations through the services, the point hierarchy's accessor (US1, US3; FR-009, FR-011, FR-014)

**Checkpoint 6 decisions applied first** (maintainer, 2026-10-28): the JIT result pins moved to Phase 9 behind `kJitResultPins` / `TEXTURE_SUBSTITUTE_JIT_RESULTS` (§6 T082; no other assertion changed; both substitute tests Passed); R22, T104, T105 and the plan name the `shadow` operand and `textureinfo` findings; T-K1's wording is fixed in tasks.md; the `TextureSystem_StreamCursor` source change, its blur rows and the `TEXTURE_SUBSTITUTE_3D` guard were approved (the guard is removed again below).

**T085.** The DB-010 line `pointHierarchy.cpp -> core/shading.h` was deleted (10 left). `ctest -L architecture`/`archcheck`: **red, exactly 1 `VIOLATION`** (`pointHierarchy.cpp:32 -> core/shading.h`).

**T086.** `rendererServices.h`: `texture3dResolve` (returns the data size), `texture3dLookup`, `texture3dStore`. `rendererServicesImpl.h` calls `t->resolve(...)` then returns `t->dataSize`, `t->lookup(...)`, `t->store(...)`, unlocked. `CForwardingServices` and `CRecordingServices` forward to the wrapped services, `CStubServices` returns 0 / nothing.
`CShadingContext::rendererTexture3dResolve/Lookup/Store` (null-safe: nothing without services or a handle) are the one place Shading calls them.

**T087.** `texture3d()` and `bake3d()` (interpreter macros) and `jitTexture3d`/`jitBake3d` call `texture3dAvailable` before `getTexture3d` (no services: result 0, nothing written, one `CODE_NOTSTARTED` warning per kind) and the forwarders for resolve, lookup and store. A null handle writes result 0 (it used to crash in `tex->resolve`). The occlusion and `indirectdiffuse` paths
(`giFunctions.h`, `jitOcclusionBatch`) resolve their irradiance cache through `rendererTexture3dResolve` as well: every call of `CTexture3d::resolve` in Shading had to move, or the import would stay. `getTexture3d`, `getCache` and the two six-argument irradiance-cache `lookup(..., this)` calls are unchanged.

**T088.** `CPointHierarchy(name, from, to, in, const Options &)` with `Options { float (*maxSolidAngle)(const CShadingContext *) }`; `lookup()` reads the limit through it; `pointHierarchy.cpp` no longer includes `shading.h` (it now includes `ri_config.h` for `ALLOCA_MAX_ITEMS` and `POINTHIERARCHY_STACK_SIZE`, which came in through `shading.h`). `rendererFiles.cpp` defines `pointHierarchyMaxSolidAngle()` (reads `scratch.occlusionParams.maxSolidAngle`) and passes it; it includes `shading.h` for the complete type. No hider code changed.

**T088a.** `characterization_scenes.sh` + `image_manifest.sh diff`: the 32 fresh images (14 characterization scenes, 18 probes): **0 differing**. **Mutation**: the accessor returns 0: **3 differing** (`pointhier-occlusion-default-reyes`, `pointhier-occlusion-wide-reyes` and its `-slo`), and the script's own `maxsolidangle` proof fails as it should. Reverted (byte-identical to the saved copy).

**T089.** `ShadingParity_TextureSubstitute3D` and `...3DNoServices` enabled (`ENABLED` in `tests/shading_parity/CMakeLists.txt`; the `TEXTURE_SUBSTITUTE_3D` define and the header guard are gone). `ctest -R TextureSubstitute`: **4/4 Passed**.
**Setup change for approval**: with the forwarders on the services, the `getCache` handle that an `occlusion()` call resolves is routed through them too, and `CTextureServices` recorded it (`ShadingParity_TextureSubstituteHiderNoSystem` went red on `noSystem.calls3d.empty()`) and would have answered it with canned values instead of the cache's. `CTextureServices` now records and answers only calls made with
its own token (`token3d()`, what `getTexture3d` returns, now a member) and forwards every other handle to the wrapped services. No assertion changed; the hider test is green again.
**Mutation**: `rendererTexture3dLookup` passes `N` and `P` swapped: `ShadingParity_TextureSubstitute3D` **red** (48 failed assertions). Reverted. (The suggested "ignore the returned size" does not apply: Shading reads `dataSize` from the handle as before; the swap is the mutation.)

**T090.** Flat-namespace imports of `libshader_shading.dylib`: **6**, a subset of `libshader-dynlookup-before-macos.txt`: `CPhotonMap::lookup`, `CGatherBundle::CGatherBundle`, `CShadingContext::trace`, `CShadingContext::traceEx`, the vtables of `CTraceBundle` and `CTransmissionBundle`; `CTexture3d::resolve` is gone. `ALLOWED DB-001 x 8` unchanged.

**Checkpoint 7 (T091), macOS** (`cpujob 027-p7-*`, logs in `build-027-logs/`).
- CP-a: clean build, `-Werror`. CP-b: 894 registered (1 skipped), **32 failed = exactly `tools/known-red-p2.txt`**; `texture_tile` at `-j2`: 14/14.
- CP-c: `ctest -L architecture` 21/21, `RESULT PASS`, no `STALE`, no `VIOLATION`; **`ALLOWED DB-010 x 10`**, nothing added; `METRIC Texturing refs 1 distinct 1`, `TOTAL refs 652 distinct 98`; Shading's imports **6**.
- CP-d: 833 images, **0 differing, 0 missing** (noise list: the seven `.slo` raytrace probes); all 389 `MaxBlockAvgDiff` equal `baseline-diffs.txt`. Named and unchanged: `TextureSystem_StreamCursor`, `ShadingParity_TextureSubstitute{,HiderNoSystem,3D,3DNoServices}`, the determinism tests, the occlusion `ShadingParity_JitPL*` tests, `Visual_sphere-{occlusion,texture3d-bake3d,textureinfo}-reyes{,-slo}`.
- Divergence rows: all 18 equal §0.7 in this run (no noise deviation).

**Approved by the maintainer at Checkpoint 7 (2026-10-28):** the `recording_texture_system.h` change (`CTextureServices` records and answers 3D forwarder calls only for its own token and forwards every other handle to the wrapped services; no assertion changed) and the `shaderFunctions.h` reformat.

## §8 Phase 8: Texturing stands alone (US1; FR-007, FR-018(a), SC-002, SC-003)

**T092.** `tests/unit/texture_only/CMakeLists.txt`: `textureSystem.cpp` appended to `texturing_only_sources`; `OPENRENDER_TEXTURE_ONLY_TESTS` defaults to **ON**; `EXCLUDE_FROM_ALL` dropped from the library and from `test_texture_only_standalone`; `DISABLED` dropped from `TextureOnly_Standalone`. `build-027` reconfigured with the explicit `-DOPENRENDER_TEXTURE_ONLY_TESTS=ON`
(a cached OFF does not follow a changed default). The build, with the Texturing sources compiled a second time on the narrow include path, was clean under `-Werror` at the first try.

**T093.** First run: **SEGFAULT** in `buildSynthesizedPyramid` (lldb: a null write through the process-wide arena). Synthesizing an unbaked source decodes into `globalArena`, which `RiBegin` initializes in a renderer and nothing else does; `TextureSystem_StreamCursor` already calls `globalArena.init()` for the same reason. **Test setup fix, for approval**: `test_texture_only_standalone.cpp` calls
`globalArena.init()` before the system is made and `globalArena.tini()` after it is deleted (and includes `common/memory.h`); no expectation changed. (This is a requirement of the Texturing-only program that the production code leaves to the renderer: a consumer of the texture system must initialize libRIBCommon's arena. A note for the registry/docs task.) Then
`TextureOnly_Standalone`: **Passed**, all 15 lookups equal `tools/master-lookup-values.txt`. Link check: `otool -L` shows only `libRIBCommon.1.dylib`, `libSystem` and `libc++`; the 266 undefined symbols are libc/libc++ and libRIBCommon's (RGBE codec, RI tokens, memory, os, sampling, `error`, `ropen`, `CFileResource`, `RiBoxFilter`); none matches `CRenderer`, `CShadingContext`, `CAttributes`, `COptions`, a hider or `libshader`. T034's compile errors are gone.

**T094.** Mutation (T-T1): `CMadeTexture::lookup4` returns `1 - value` in channel 0 (the footprint lookup the test uses; `lookup` is the point form): `TextureOnly_Standalone` **red** (`baked A (0.135373,...)` against `(0.864627,...)`, every baked and the non-power-of-two rows). Reverted (byte-identical; the file touched after a pause, since `make` 3.81 compares whole-second mtimes).

**T095.** The first `archcheck` run with the option ON: `ctest -L architecture` 21/21, **no exit 2, `notbuilt=0`**, `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0`; the staged `ri_config.h` copy is not reported; `ALLOWED` and `METRIC` are **identical** to Checkpoint 7 (`DB-010 x 10`, `Texturing refs 1 distinct 1`, `TOTAL refs 652 distinct 98`): the second compile of the Texturing units adds nothing.

**T096.** `python3 -I tools/core_usage.py . src/ri/texture`: `refs 1 distinct 1`, `CRenderer::retrieveDisplayChannel` (file `texture3d.cpp`: 1). `METRIC Texturing refs 1 distinct 1`; Shading `refs 0 distinct 0`.

**T097.** `grep -rn CShadingContext src/ri/texture/`: `texture3d.h` (forward declaration and the pure virtual six-argument `lookup`), `pointHierarchy.h/.cpp` (the `Options` accessor and the override), and the assertion overrides of `pointCloud.h:65` and `brickmap.h:206`: nothing else.
`grep -rnE 'CRenderer::(texture|initTextures|shutdownTextures|environmentLoad|synthesizeMutex)' src` has **no live reference to a removed member**. The literal "returns nothing" does not hold because the pattern's `texture` prefix also matches what stays: `CRenderer::texturePath` (the core's option and its definition, `blobbyRepeller.cpp`'s `locateFile` call, a DB-004 allowlisted edge),
`CRenderer::textureResolver` (the core, Phase 4) and comments that say "was `CRenderer::textureMutex`/`initTextures`/`shutdownTextures`/`textureLoad`" (history in `textureInternal.h`, `texture.cpp`, `tileSource.h`, `rendererContext.cpp`, `blobbyRepeller.h`). `grep -rn 'display/rgbe/rgbe.h\|rgbe/rgbe.cpp' src`: nothing.

**Checkpoint 8 (T098), macOS** (`cpujob 027-p8-*`, logs in `build-027-logs/`).
- CP-a: clean build, `-Werror`, option ON. CP-b: 895 registered (`TextureOnly_Standalone` included, label `texture_only`: 1/1), **32 failed = exactly `tools/known-red-p2.txt`**; `texture_tile` at `-j2`: 14/14.
- CP-c (option ON): `ctest -L architecture` 21/21, `RESULT PASS`, no `STALE`, no `VIOLATION`; `ALLOWED DB-010 x 10`, nothing added; Shading's imports 6 (unchanged).
- CP-d: 833 images, **0 differing, 0 missing** (noise list: the seven `.slo` raytrace probes); all 389 `MaxBlockAvgDiff` equal `baseline-diffs.txt`. Divergence rows: all 18 equal §0.7 (no noise deviation in this run).

**Approved by the maintainer at Checkpoint 8 (2026-10-28):** the `globalArena.init()`/`tini()` setup fix in `TextureOnly_Standalone` (no expectation changed). Note carried to T115 and T117: a consumer of the texture system must initialize libRIBCommon's arena (`globalArena.init()`), which `RiBegin` does in a renderer; synthesizing an unbaked source decodes into it.

## §9 Phase 9: JIT parity of `texture()`, `environment()` and `shadow()` (US3; FR-010, FR-022, SC-008, SC-012) - the only output-changing phase, `.slo` only

**T099.** The footprint arithmetic is in two inline helpers in `textureCalls.h` (`libshader::textureCorners`, `environmentCorners`), and the interpreter macros (`TEXTUREFEXPR`, `TEXTURECEXPR`, `ENVIRONMENTEXPR`, `SHADOWEXPR`) call them; the JIT was not touched yet. Mid-phase check (`cpujob 027-p9a-*`, the whole Checkpoint-8 procedure): **0 differing** of 833 images against `baseline-images.sha256`, every
`MaxBlockAvgDiff` equal, 32 failed = the known-red set, divergence rows as before (`unbaked-rgbe-raytrace` read 84.94, a known noise value). The interpreter's output is unchanged by the helper.

**T100-T103.**
- `llvmEmitter.cpp`: one branch for `texture`/`environment`/`shadow`, the form from the **prototype** (`c=`/`f=`; for `texture()` the run of `f` after `SF` counts the coordinates, 2 or 8, anything else fails the emit), the positional operands name, channel, coordinates (or `D`/`Ps` as **operand 2**), and the rest through `emitParamList()`; `n` and `tags` are the raw `numVerts`/`tags` as for `trace()`.
- `rslOps.{h,cpp}`: `op_texture_{f,c}_v3`, `op_texture_full_{f,c}_v3`, `op_environment_{f,c}_v3`, `op_shadow_{f,c}_v3`; the `_v2` names are deleted (`grep -rn "op_texture_.*_v2\|op_environment_.*_v2\|op_shadow_f_v2" src tests` shows none).
- `shading.cpp`: `jitTexture()` and `jitEnvironment()` replace `jitTextureF/C`, `jitEnvironmentF/C`, `jitShadowF`: the list is bound by `jitParameterList<CTextureLookup>()` / `<CEnvironmentLookup>()` and its varying values copied per vertex by `jitParameterListReady()`; `swidth`/`twidth`, the grid derivatives, the footprint, the options, the handle cached in `lookup->map`, the draws and the results (float `environment()` is channel 0, float `shadow()` the mean, the eight-coordinate float form channel 0) are the interpreter's. `"raytrace"` and the shader's own name trace, **by the name alone at
  run time**, with the interpreter's `CTraceLocation` batch through `traceReflection()`/`traceTransmission()` (results as `ENVIRONMENTEXPR_POST`/`SHADOWEXPR_POST`); the `Failed open environment "raytrace"` error is gone under the JIT.
- Three defects found on the way and fixed (research R22): the `shadow` branch passed operand 1 (the channel) as `Ps`; **`textureinfo()` took the second-to-last letter of its prototype as the result type, but the IR text carries `f=SSF` with no `!`, so every `.slo` `textureinfo()` was a string result** (the garbage resolution values; the letter is now the last one once an optional `!` is stripped); and **`buildVarTable` wrote the RSL globals over a shader's own parameters or locals of the
  same name**, so the probe's `uniform float width` was the vector global `width` (the parameter list was bound varying: "width parameter was expected to be uniform"). The user's variable now shadows the global, as in the language and in the interpreter. No shipped shader has a parameter or local named like a global (checked over every compiled `.rslo` in `build-027/shaders`).

**T104.** `test_texture_substitute.cpp`: the `.slo` half now calls the same `checkInterpreterTexture`/`checkInterpreterEnvironment` checks as the `.rslo` half (the handle request, the four corners, every option, the canned colors), the parameter-list probes (`tex_c_pl`, `tex_c8_pl`, `env_c_pl`, `shadow_f_pl`) run under the JIT too, the cross-backend equality covers all ten probes, the no-system fill is the interpreter's. The scratch sentinels, `checkJitOptions`, `setJitSentinels` and the `kJitResultPins` gate (Phase 6) are
removed: every pin that was moved is unconditional now, so `TEXTURE_SUBSTITUTE_JIT_RESULTS` is not defined anywhere. `test_texture_substitute_3d.cpp` needed no change. This is the one sanctioned assertion change; it tightens.

**T105.** `cpujob 027-p9-jitpl ctest -R 'ShadingParity_(JitPL|TextureSubstitute)' -j4`: **89/89**: every `ShadingParity_JitPL_{texture,envshadow}_*` case including `envshadow_raytrace_*`, every `_rslo` and `_slo` engaged control, `ShadingParity_JitPLDiagnosticsTexture`, `ShadingParity_TextureSubstitute` and `...3D`, and #55's own cases. The first run had 7 failures (the `width`, `swidth_twidth` cases and the substitute test's textureinfo rows); the two defects above were the cause.

**T106.** Mutations (each reverted byte-identical, the file touched after a pause): (a) `jitParameterList` skips the pair named `blur`: `JitPL_texture_blur`, `JitPLEngaged_texture_blur_slo`, `JitPL_envshadow_blur`, `JitPLEngaged_envshadow_blur_slo` **red** (the `_rslo` controls stay green). (b) the traced branch off (`traced = false`): all nine `envshadow_raytrace_*` parity and `_slo` engaged tests **red**, the `_rslo` ones green. (A first attempt of (b) did not compile (`-Wlogical-op-parentheses`) and its run was discarded; it was redone.)

**T107.** `tools/jit-texture-divergence-phase9-macos.tsv`: **all 18 scenes `identical`, MaxBlockAvgDiff 0.00** (the starting tree: 16 of 18 differ, 34.55-252.52). (The script needs absolute build and output paths.)

**T107a.** `tools/jit_texture_stability.sh` (new: the seven raytrace probes only, 7 renders per run, 28 in all): four runs, `cpujob 027-p9-stab1..4`, all four **identical** and each equal to the `.rslo` render of the same scene (T107's `rslo` images):
  - `texture-png-raytrace` `67023d53dda11454bcc84c1ace7608b4e3a9162ee56133c2eb6582615141060b`
  - `texture-exr-raytrace` `398f123dedd3e84ef85dce76aa910fd1a83bbf523f96489756655b1e45948085`
  - `texture-rgbe-raytrace` `ae04e3a53f810b5e0faf8350e78f0c871040163280084f1fb6920a917d5df060`
  - `unbaked-png-raytrace` `99d0ed56267f47a5a67f641b93f5332ca7ea09c0fcbb9712562fc12a2c8fca1a`
  - `unbaked-nonpot-raytrace` `b80626974af68a4bcb6ee8082987cfbcf917e9b96053c048440b44bd51b762f1`
  - `unbaked-exr-raytrace` `65eb3e6812597c48a654296eb29eb8fce81205e095072857bca4d06923443911`
  - `unbaked-rgbe-raytrace` `923b90c1f06991f74f59103727bc2dcebe3de6f78c652cced5fada5564cc2c42`

**T108/T109.** `cpujob 027-p9b-*` (build, `-LE determinism-sweep -j4` suite, `texture_tile -j2`, probes, characterization scenes, manifest diff against `baseline-images.sha256` with the old noise list):
- **895/895 tests pass: the known-red set is empty.** `texture_tile` 14/14, architecture 21/21.
- 26 images differ, **all spec-027 JIT images**: the nine `.slo` probe images of the non-noise scenes (`texture-{png,exr,rgbe}-reyes`, `unbaked-{png,nonpot,exr,rgbe}-reyes`, `env-reyes`, `env-raytrace`; the shadow probes are unchanged) and 17 `.slo` renders of the T-J scenes (`tests/visual/jit-texture/jittex-*-slo.tif`); the seven noise-listed raytrace probes changed too (33 changed in all against the baseline manifest).
  Every one of them equals its `.rslo` render (T107). **Every `.rslo` image, every characterization scene, every image of a scene that does not call these builtins, and all 389 `MaxBlockAvgDiff` are identical to the baseline.** In particular the existing `Visual_sphere-textureinfo-reyes-slo` is unchanged although the `textureinfo()` result type was corrected.
- **T109 outcome: no existing (pre-027) test result, `MaxBlockAvgDiff`, image or reference changed.** No reference was regenerated and no threshold touched. No STOP.

**T110.** `tools/baseline-image-noise.txt` no longer lists the seven probes (a copy: `baseline-image-noise-p9.txt`, the same text); `tools/baseline-images-p9.sha256` (833 images) and `tools/baseline-diffs-p9.txt` (389 lines) recorded right after the clean run above. Then the suite, probes and characterization scenes once more (`cpujob 027-p9c-*`): 895/895, `texture_tile` 14/14, `^determinism$` **155/155**, architecture 21/21,
`image_manifest.sh diff baseline-images-p9.sha256`: **833 images, 0 differing, 0 missing, noise-listed 0**; `diffs` equal to `baseline-diffs-p9.txt` (and to the original `baseline-diffs.txt`). `ALLOWED DB-010 x 10` unchanged, `RESULT PASS`.

**Checkpoint 9 (T111), macOS.**
- CP-a: clean build, `-Werror`. CP-b: **895/895, the known-red set is empty**; `texture_tile` 14/14; determinism 155/155.
- CP-c: architecture 21/21, `RESULT PASS`, `ALLOWED` unchanged from Phase 8 (`DB-010 x 10`), Texturing metric 1/1, TOTAL 652/98.
- CP-d9: (1) every `.rslo` image, every characterization scene and every image of a scene that does not call these builtins is bit-identical to the baseline; (2) the JIT texture probes changed and are bit-identical to their `.rslo` renders, and the seven raytrace ones are bit-stable over four runs; (3) every `ShadingParity_JitPL_*` case and `_slo` control passes; (4) every `MaxBlockAvgDiff` equals the baseline; (5) the known-red set is empty.


**Approved by the maintainer at Checkpoint 9 (2026-10-28)**, including the `buildVarTable` fix, which is outside the spec's texture scope: **a shader's own parameter or local now shadows a same-named RSL global under the JIT** (`.slo`). This is a JIT behaviour change for any shader that declares such a name (none of the shipped shaders does, which is why no existing image moved); it is carried into NEWS.md and ChangeLog.md (T118a).

## §10 Phase 10: registry, docs, measurements (macOS), then Linux

**Rebuild first.** The staged clang-format touch-up of `textureCalls.h` (Phase 9) was compiled by a clean `-Werror` build before this phase's work (`cpujob 027-p10-build`, 0 errors; only the linker's duplicate-library notes).

**T112, T113.** `tests/architecture/allowlist.txt`: the two `texture3d.cpp` lines re-tagged DB-010 -> DB-021 (keys unchanged, no line added): the `renderer.h` include and the `retrieveDisplayChannel` link (`_ZN9CRenderer22retrieveDisplayChannelEPKc`; no `FILE *`, so one line covers both platforms); headers now `DB-010: The hiders' GI caches use Texturing (8)` and `DB-021: ... (3)`.
`git diff 4eb683b -- tests/architecture/allowlist.txt | grep -c '^+[A-Z]'` shows only the two re-tagged lines. `ctest -L architecture` 21/21 and `archcheck`: `RESULT PASS violations=0 stale=0 badid=0`, `ALLOWED DB-001 x 8 DB-004 x 65 DB-005 x 8 DB-006 x 33 DB-010 x 8 DB-011 x 173 DB-015 x 10 DB-017 x 78 DB-019 x 3 DB-021 x 3 DB-025 x 12`, no `DB-026`.

**T114.** `tests/architecture/metrics-baseline.txt`: Texturing `1 1`, TOTAL `652 98` (as reported), with a spec-027 paragraph in the header; no class raised. `METRIC` now reads `(baseline 1)` and `(baseline 652)`.

**T115, T116, T118, T118a.** `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`: DB-010 retitled "The hiders' GI caches use Texturing" (evidence reduced, 8 edges: 5 include, 3 link, direction spec 032, History 35 -> 8), DB-021 (3 edges: 2 include, 1 link, spec 037, History), DB-001 (7 -> 6 imports, `CTexture3d::resolve` gone; `tools/libshader-dynlookup-after-macos.txt` is the new list), and the texture-system notes (the core-lock coupling and the arena note) under DB-010. `DEVNOTES.md`: a status row for spec 027 (Linux validation marked pending) and the
`textureLoad` mention updated. `DEVNOTES_DETAILS/OSHADER_UPDATES.md`: a JIT ABI note for the `_v3` texture entry points. `NEWS.md` and `ChangeLog.md` (Changed and Fixed): the user-visible `.slo` change, including that a shader's own parameter or local now shadows a same-named RSL global under the JIT and that a `.slo` compiled before must be rebuilt. Grep of `DEVNOTES_DETAILS/`, `docs/` and the root docs for the old names
(`CRenderer::textureLoad`, `getTexture`, `getEnvironment`, `getTextureInfo`, `initTextures`, `synthesizeMutex`, `textureMutex`, `op_*_v2`, `rgbe/rgbe`): only the DB-026 Resolved history and the spec-020 status row (updated) mention them.

**T117.** `CLAUDE.md` was first left unedited (no authorization); the maintainer authorized it after the Phase 10 checkpoint and parts (1)-(6) of the proposed text were applied to the worktree's `CLAUDE.md`, shown below.

**T119.** Guards against `BASE` (`4eb683b`): `git diff --stat` of `src/ri/texture/texture3d.cpp`, `src/ri/hiders/radiance.cpp` and `src/display/dsply.h` is empty; the two `test_tile_source_*_info.cpp` tests are unchanged (not even an include path); no `shading.h`/`renderer.h`/`options.h`/`attributes.h` include in `src/ri/texture` outside `texture3d.cpp`.

**T120.** No-JIT build (`-DOPENRENDER_ENABLE_JIT=OFF`, scratch `build-027-nojit`, deleted afterwards): builds clean under `-Werror`; `ctest -L "architecture|texture_tile|texture_only|shading_parity|image_input|common"`: **105 run, 105 passed, 65 `Not Run (Disabled)`**. All 65 are the `.slo`-dependent tests that were already registered `DISABLED TRUE` without LLVM: #55's and spec 017's JIT tests (`ShadingParity_JitPL*`, `...Engaged_*_slo`, `PrintfJit`,
`UniformConditionalJit`, `MatrixTransformJit`, `OlWiring`, `UsedParametersOracle`, `JitPLLabel_*`) plus this spec's T-J cases (`JitPL_{texture,envshadow}_*`, their `_slo` engaged controls, `ShadingParity_JitPLDiagnosticsTexture`). The interpreter halves of `ShadingParity_TextureSubstitute{,HiderNoSystem,3D,3DNoServices}`, the `_rslo` engaged controls, `TextureOnly_Standalone` and the rest ran and passed.

**T121.** SC-010: `tools/timing-final-macos.tsv` against `tools/timing-base-macos.tsv` (10 runs each, same script and machine, medians in seconds, [min, max]): `concurrency-scene` default 0.460 [0.418, 0.698] -> 0.393 [0.377, 1.762]; `-t:1` 0.702 [0.636, 0.780] -> 0.716 [0.608, 1.624]; `texture-png-raytrace` default 0.140 [0.124, 0.184] -> 0.117 [0.115, 0.126]; `-t:1` 0.197 [0.191, 0.243] -> 0.209 [0.194, 0.246]. No median is slower than the baseline's spread; the two default-thread medians are
faster, below the baseline minimum. One run of `concurrency-scene` in each thread setting of the finished tree was an outlier (1.6-1.8 s, the max column only; the medians are unaffected and the cause was not investigated).

**T122.** Final macOS pass (`cpujob 027-p10-*`): CP-a clean build; CP-b **895/895** (1 skipped, `LibShader_Format_rslo_lower_minor`, as before this spec), `texture_tile` 14/14, `^determinism$` 155/155, `texture_only`, `shading_parity`, `image_input`, `common` and `visual` inside the suite; **zero** `Not Run (Disabled)` lines in the run (and none at all before the no-JIT build); CP-c `ALLOWED` as above; CP-d against `baseline-images-p9.sha256` / `baseline-diffs-p9.txt`: **833 images, 0 differing, 0 missing, noise-listed 0**, `diffs` equal;
imports **6**; the T097 greps unchanged; `nm -gU` shows the six `RGBE_*` functions defined only in `libRIBCommon` (and its copies in the `orender-wire` bundle).

**T123.** The opt-in sweep on the finished tree: `cpujob 027-fin-sweep ctest -L '^determinism-sweep$' -j2`: **419/419 passed**, equal to T013's pass set.

### T124: Linux validation (Ubuntu 24.04, GCC 13.3, `-j2`)

Host `juvenal@ubuntu-2404-desktop.v2-labs.io`, own tree `~/openrender-027` (no other tree there was touched).
- Sync (bash, `${H}:path`): `git ls-files -z | rsync -a --from0 --files-from=- ./ "${H}:openrender-027/"` from the worktree (the staged tree, no build artefacts), then `sha256sum -c specs/027-texture-system/tools/027-linux-manifest.sha256` on the host (156 changed files against `BASE`): **156 OK, 0 not OK**.
- Build: `cmake -S . -B build-027 -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release` and `nice -n 19 cmake --build build-027 -j2` (GCC 13.3, LLVM 18, `-Werror`).

**Linux results** (2026-10-28; logs in `~/openrender-027` on the host: `build-027.log`, `suite.log`, `tile.log`, `arch*.txt`, `jitpl.log`, `det.log`, `sweep.log`, `jit-divergence-linux.tsv`):
- **Build**: clean under GCC 13.3 `-Werror`, 0 warnings (the watch list of T124 raised nothing: `-Wmissing-field-initializers`, `-Wstringop-overflow`/`-Warray-bounds`, `-Wnull-dereference`, `-Wdeprecated-enum-enum-conversion`; the RGBE functions are `extern "C"`, no `_IO_FILE`-mangled symbol is left).
- **Suite** (`-LE determinism-sweep -j2`): 896 registered, **889 passed, 7 failed, 0 disabled, 1 skipped** (`LibShader_Format_rslo_lower_minor`). The 7 failures are the pre-existing Linux ones recorded in `specs/025-graphics-state-handles/validation.md` section 9.6 with the same signatures, and **nothing else fails**: the five `TextureTile_*` of #45/#46 (`RenderByteIdentical`: byte 5 of the
  reyes renders differs from the macOS reference; the four `Cache*`: `orender` aborts), `LibShader_McompConsistency` (#44: the test executable cannot load `libshader_shading.so.1` because of `_ZTV19CTransmissionBundle` and the other Hiders vtables and symbols) and `CoverageAnalytic_alpha-sides2-tjunction-r4_reyes` (pixel (99, 46) 0.875 against 0.9375, #57). No Linux BASE run of this spec's tests exists; the comparison is with spec 025's Linux record of the same host and the same 7.
  `Common_StateOwnership`, `Common_VocabularyConsistency`, `TextureSystem_*` (all five, the external-mutex entry included), `TextureOnly_Standalone`, `ImageInput_*`, `Display_RgbeBytes` and the 4 substitute tests passed.
- `texture_tile -j2`: 9 passed, 5 failed (the same five). `^determinism$` -j2: **155/155**. The sweep `-j2`: **419/419** (1388 s).
- **Architecture**: `ctest -L architecture` 21/21; `archcheck` `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`, `ALLOWED` identical to macOS (`DB-010 x 8`, `DB-021 x 3`, no `DB-026`), `METRIC Texturing refs 1 distinct 1`, TOTAL 652/98. **No Linux-only variant** of the re-tagged `texture3d.cpp` lines appeared (nothing to re-tag, nothing added).
- **Imports** (spec 024's method, `nm -D --undefined-only` intersected with `LD_DEBUG=bindings` to `libRIBRender.so.1`): **6**, all Hiders (`CPhotonMap::lookup`, `CGatherBundle::CGatherBundle`, `CShadingContext::trace`/`traceEx`, the vtables of `CTraceBundle` and `CTransmissionBundle`); `CTexture3d::resolve` is gone (SC-004). The undefined-symbol list of `libshader_shading.so.1` has 459 entries (spec 025's tree: 466).
- **Phase 9 on Linux**: `ctest -R 'ShadingParity_(JitPL|TextureSubstitute)' -j2`: **89/89**. `jit_texture_divergence.sh` (absolute paths): 15 of 18 scenes `identical` (0.00); `texture-exr-reyes`, `texture-rgbe-raytrace` and `unbaked-nonpot-reyes` read **MaxBlockAvgDiff 0.02** between `.slo` and `.rslo` (macOS: 0.00 in all 18). The cause is the specular highlight (float against double normalization in the JIT and the interpreter), not the texture code, and it predates this spec: see "Investigation: the 0.02 difference" below. It is far below any test threshold and no test depends on it; on Linux the JIT probes equal their `.rslo` renders to 0.02 block-average, and on macOS the 8-bit images happen to agree.
- No existing result differs from spec 025's Linux record other than by the new tests, so there is no STOP.

**Approved by the maintainer at the Phase 10 checkpoint (2026-10-28)**, and T117 authorized afterwards: parts (1)-(6) were applied to this worktree's `CLAUDE.md` (the "Spec-kit agent rules" section untouched), with one added sentence in gotcha 25 about the `.slo`/`.rslo` specular difference.

### Investigation: the 0.02 `.slo`/`.rslo` difference on Linux (2026-10-28)

**Cause.** `specular()` is computed by two independent implementations that differ in the last bits. The interpreter's `SPECULAREXPR` (`shaderFunctions.h`) normalizes with `normalizev`, which works in **double** (`1 / sqrt(double)`, the product in double, the result cast to float); the JIT's `CShadingContext::callSpecular` (`shading.cpp`) normalizes the light vector and the halfway vector in **float** (`sqrtf`, then per-component division). The exponent `10 / roughness` (100 for `plastic`)
multiplies the relative error of the dot product by about 100 in the highlight, so the results differ by 1e-8 to 1e-7 there, and only there. It has nothing to do with the texture builtins, with FMA contraction (the Linux host is an x86-64 QEMU CPU without FMA) or with the EXR/RGBE/unbaked decode paths.

**Evidence** (float renders: `Quantize "rgba" 0 0 0 0`, `-t:1`, `.slo` against `.rslo`; tool: a 40-line libtiff comparer, scratch only):
- The three scenes differ in exactly one 8-bit value each: `texture-exr-reyes` pixel (351,216) channel 1: 123 vs 122; `texture-rgbe-raytrace` (349,229) channel 2: 51 vs 52; `unbaked-nonpot-reyes` (348,218) channel 0: 173 vs 174. In float the same scenes differ in 2 400-2 600 values by 1.9e-9 to 1.2e-7, inside a box of about 45 x 50 pixels at x 317-362, y 196-248 (the highlight).
  A difference of one part in 255 at one pixel is MaxBlockAvgDiff 0.02.
- **Every** one of the 18 texture parity scenes differs in float on Linux (1 578 to 7 986 values each); the other 15 simply do not cross an 8-bit rounding edge. On macOS the float renders differ as well (`texture-exr-reyes` 2 539 values, `texture-png-reyes` 5 324, `env-reyes` 1 729, `unbaked-nonpot-reyes` 2 655; `shadow-reyes` 0): the macOS 0.00 is the 8-bit image happening to agree, not bit identity of the arithmetic.
- Bisecting the shader on Linux (the same sphere scene): `matte` **0** differing values, `paintedplastic` with `"Ks" [0]` **0**, `plastic` (no texture at all) **2 058**, `paintedplastic` 2 413. Only a shader that calls `specular()` differs, and only inside the highlight.
- **Probe** (macOS, scratch edit of `callSpecular` to use `normalizev`/`addvv`/`dotvv` exactly as the interpreter does, reverted afterwards): `plastic` and `paintedplastic` go from thousands of differing values to **0**; the whole suite stays 895/895.
- **It predates spec 027.** `git diff 4eb683b` touches neither `SPECULAREXPR` nor `callSpecular`, and the plastic scene calls no texture builtin. Phase 9 did not introduce it; it made the divergence visible in this spec's 18 `.slo` probe renders, which before Phase 9 differed by 34-252 for other reasons.

**Proposed fix (not applied).** Make the JIT's `callSpecular` use the interpreter's arithmetic: `normalizev(Ltmp, L)`, `addvv(halfway, V, Ltmp)`, `if (dotvv(halfway, halfway) > 0) normalizev(halfway)`, `tmp = (1 - ns) * dotvv(N, halfway)`, then `pow` as it does today (the exponent and the `pow` precision were not the cause). The cleanest form is one shared helper (a function both the `SPECULAREXPR` macro and `callSpecular` call, as Phase 9 did for the texture footprint), so the two cannot drift.
Note that `callSpecular` also ignores a light's `__nonspecular` factor `(1 - ns)` that the interpreter applies; sharing the helper would fix that too, and it is a second, larger `.slo` change for scenes with non-specular lights.

**Blast radius, measured with the probe build** (macOS, full suite and image manifest against `baseline-images-p9.sha256`): 895/895 pass; **no `.rslo` image changes** (the interpreter is not touched); of 833 images only **3 `.slo` renders change**: `tests/visual/teapot-blue_marble-reyes-slo`, `teapot-wood-reyes-slo` and `teapot-wood-raytrace-slo`; every `MaxBlockAvgDiff` is equal to `baseline-diffs-p9.txt`; no reference image or threshold is involved (these three are outputs compared to their `.rslo` renders under their existing thresholds). On Linux the same fix would move
the Linux `.slo` images of the same shaders and would make the three 0.02 scenes bit-identical. The non-specular-light correction, if included, was not measured. The fix belongs in its own change (a JIT/interpreter specular parity fix): spec 027 does not need it, and SC-012's bit identity is, until then, an 8-bit statement that holds on macOS and holds to 0.02 on Linux.

### T117: the `CLAUDE.md` text (applied after the maintainer's authorization)

```
(1) Architecture: insert after "### Graphics State handles (spec 025)", before "### Coordinate / matrix conventions":

### Texture system (spec 027)
`CTextureSystem` (`src/ri/texture/textureSystem.h`) owns Texturing's state for one
frame: the tile cache and the per-thread budgets (`TexturePerthread`), the file
families, and the load lock. The core creates it in `beginFrame()`
(`CRenderer::textureSystem`, with a `CSearchPathResolver` over `texturePath` as its
`CTextureFileResolver`) and deletes it in `endFrame()`. Handles are opaque
(`TextureHandle`); a lookup takes a caller-built `TextureOpt` (the options and the
position in the sample stream, which the caller writes back with
`commitTextureDraws()`) and the thread's `TexturePerthread`, never a shading context.
Shading reaches textures only through `CRendererServices::textureSystem()` and
`libshader/shading/textureCalls.{h,cpp}`; the 3D channel operations go through
`texture3dResolve/Lookup/Store` on the services. The RGBE codec lives in
`libRIBCommon` (`src/common/rgbe.{h,cpp}`). See gotcha 25.

(2) Known gotchas: append after gotcha 24:

25. **The texture system is per frame, and its load lock is the core's
   `shaderMutex` (spec 027).** `CRenderer::textureSystem` exists from
   `beginFrame()` to `endFrame()`; `textureSystem()` of the services is `nullptr`
   between frames, and with none installed a lookup returns the `fill` value and
   warns once per builtin. The core passes `&CRenderer::shaderMutex` as the system's
   `loadMutex`. It is a plain, non-recursive mutex, so **nothing that holds it may
   call `handle()`**: the services getters that hold it (`getCache`, `getTexture3d`,
   `getPhotonMap`) never reach a texture handle, a new caller that does is a
   self-deadlock, and a debug build asserts a re-entry of `handle()`. A consumer
   outside a renderer must call `globalArena.init()` first (`RiBegin` does it):
   synthesizing an unbaked PNG/EXR/RGBE source decodes into it, behind a
   process-wide function-local mutex in `texture.cpp`. `environment()` and `shadow()`
   trace **by name** (`"raytrace"` or the shader's own `"reflection"`/`"shadow"`),
   never because a handle is missing. Under the JIT, `texture()`, `environment()` and
   `shadow()` bind their optional parameter lists with
   `jitParameterList<CTextureLookup/CEnvironmentLookup>` and build their footprint
   with the helpers the interpreter's macros call (`libshader::textureCorners`,
   `environmentCorners`); the entry points are `op_*_v3`, so a `.slo` compiled before
   spec 027 must be rebuilt. A shader's own parameter or local now shadows an RSL
   global of the same name in the JIT's variable table (it used to lose to the
   global). The only `CShadingContext` left in `src/ri/texture` is the 3D family
   (`texture3d.h`, `pointHierarchy`, the `brickmap`/`pointCloud` assertion
   overrides), for spec 032.

(3) Where the compiled shaders live: "108 `.rslo` (+ 108 `.slo` when the JIT is on)" -> "127 `.rslo` (+ 127 `.slo` when the JIT is on)" (recounted in build-027; the 108 was already stale before this spec, and this spec adds two probe shaders, `jit_pl_texture_probe` and `jit_pl_envshadow_probe`).

(4) Testing block: add two lines after the `shading_parity` line:
ctest --test-dir build -L texture_tile -j2           # texture system: cache, resolver, concurrency, sample-stream cursor (opt-in disk-cache scripts included)
ctest --test-dir build -L texture_only               # Texturing-only program: links libRIBCommon alone and reproduces the starting tree's lookups

(5) Active Technologies: append
- C++20, CMake ≥3.19, `-Werror`; no new dependencies — Texturing owns its cache
  in a per-frame `CTextureSystem`, Shading reaches it through `CRendererServices`,
  the RGBE codec moves to `libRIBCommon` (027-texture-system)
- Storage: N/A — memory only; no new persistent state (027-texture-system)

(6) Recent Changes: insert at the top of the list:
- 027-texture-system: Texturing's state (tile cache, per-thread budgets, file
  families) lives in a per-frame `CTextureSystem` behind opaque handles and a
  caller-built `TextureOpt`; the core supplies a file resolver; Shading reaches it
  through `CRendererServices::textureSystem()` and `textureCalls` (flat-namespace
  imports 7 → 6, DB-010 35 → 8 edges, DB-026 resolved); the RGBE codec moved to
  `libRIBCommon`; under `.slo`, `texture()`/`environment()`/`shadow()` now honour
  their parameter lists and match the interpreter (the only output change; `.rslo`
  renders are byte-identical), and a JIT shader's own parameter or local shadows a
  same-named RSL global (see spec 027).

The "Spec-kit agent rules" section is not touched.
```

### Success criteria (macOS unless stated)
SC-001 met (macOS and Linux: PASS, DB-026 gone, DB-010 35 -> 8, DB-021 3). SC-002 met (1 reference, 1 member; Shading 0/0). SC-003 met (`TextureOnly_Standalone`, 15 lookups). SC-004 met (6 imports on macOS and on Linux, a subset of the starting set). SC-005 met (T083, T097). SC-006 met (833 images unchanged apart from the JIT images of Phase 9; the determinism sweep 419/419). SC-007 met (macOS 895/895 and the sweep; Linux the same 7 as spec 025's record, the five of #45/#46
among them). SC-008 met (`ShadingParity_TextureSubstitute{,3D}` pass under both backends with equal recorded calls; red before Phase 6). SC-009 met (`Common_StateOwnership`, `Display_RgbeBytes`, `ImageInput_Rgbe`). SC-010 met (T121). SC-011 met (DOMAIN_BOUNDARIES). SC-012 met on macOS in 8-bit (bit-stable over four runs and equal to `.rslo`, 18 of 18 identical); on Linux three scenes read 0.02, one 8-bit value each, from the float-versus-double normalization in the specular code of the JIT and the interpreter (investigation above, which predates this spec).


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

## §11 Phase 11: convergence

**T126.** The three source comments that still named members removed from `CRenderer` now name their owners (comment text only; `git diff` for the three files shows comment lines only; no build or run is needed): `src/ri/render/rendererContext.cpp` (the `Option "texturecache"` comment: `textureLoadTexture()`, `texture.cpp`), `src/ri/texture/tileSource.h` (`textureLoadTexture()`) and `src/ri/geometry/blobbyRepeller.h` (the shadow-map composition: `textureLoadEnvironment()`, `texture.cpp`).
`grep -rnE "CRenderer::(textureLoad|environmentLoad|initTextures|shutdownTextures|getTexture\b|getEnvironment|getTextureInfo)" src` now finds only the two historical "was CRenderer::..." notes in `texture.cpp` (`initTextures`, `shutdownTextures`). The converge agent's reading of `CPointHierarchy::Options::maxSolidAngle` stands (not a gap, no task added).
