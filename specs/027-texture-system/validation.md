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

## §6–§9 Phase checkpoints

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

