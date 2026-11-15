# Validation record: spec 040 GI determinism

Written as the work lands (tasks.md T001). Section map: §0 orientation and starting-tree measurements, §1 architecture reports per
checkpoint, §2 red evidence, §3 checkpoint records (macOS and Linux, with the expected-image-drift lists), §4 images and sweeps,
§5 timing (FR-023), §6 issue evidence (#160, #161, #163, #164, #165), §7 departures and findings.

## 0. Orientation and starting-tree measurements (Phase 0)

### 0.1 The tree

- Worktree `/Volumes/Projects/Development/CLI/openrender-worktrees/040-gi-determinism`, branch `040-gi-determinism`.
- `git rev-parse HEAD` = `da88b83f9c6a32392a6d638f64b975ba20bc09a2` (docs: add spec 040 design and tasks), on top of master `278b0394`;
  the staged changes are `specs/040-gi-determinism/` files only (the analyze fixes of 2026-11-15 and the Phase 0 tools).
- `git diff --stat 278b0394 -- src tests CMake CMakeLists.txt shaders examples` prints nothing: the code is master's `278b0394`.
- Master has not moved: `git log 278b0394..master` is empty (checked 2026-11-15 in the main checkout).
- Build type Release, JIT on, Apple M2 (8 cores), macOS 27 SDK, Apple clang 21.0.0, vcpkg arm64-osx-openrender. `build` was configured from
  the worktree's `/Volumes/...` path and was up to date at `278b0394` (T003: nothing to compile).
- The `## Spec-kit agent rules` of `CLAUDE.md` were read from the worktree copy; the main checkout's copy is byte-identical (`diff` prints
  nothing) and neither has uncommitted changes, so the worktree copy is the one followed.

### 0.2 Overlap with concurrent work (T002)

Read-only. `gh issue view` on 2026-11-15: #160, #161, #163, #164, #165 and #170 are all **open**; none has changed since the plan stage
(last updates 2026-11-11 to 2026-11-14). `git log 278b0394..master -- src/ri/hiders/photon.cpp src/ri/core/map.h
src/ri/texture/photonStore.cpp src/ri/texture/irradianceCache.cpp src/libshader/shading/irradianceCalls.cpp
src/ri/render/remoteChannel.cpp` is empty. **No fix landed on master**; no duplicate edit to drop. No GitHub comment written.

### 0.3 Services version (T002a, H2)

`kRendererServicesVersion` is `{2, 3, 0}` on master `278b0394` and in every worktree (033, 040, 048, 057): none of the other unmerged specs
has edited it yet. What they claim: spec 033's tasks take **2.4.0** (additive `MatrixRequest::phase`, `MessageRequest::destBytes`) and
spec 057's take 2.4.0 first and **2.5.0** "in the 033 sync" (057's tasks.md lines 11, 171, 345); spec 048 names none. Rule (maintainer H2,
2026-11-15): 040 steps the version, taking **the next free MINOR on master at the time of the edit**, with the literal number written when
`rendererServices.h` is edited (T039a, extended at T062a and T079) and a history entry saying it is breaking within 2.x; the coordinator
re-checks the number at merge preparation. **The number free today is 2.6.0** (2.4.0 and 2.5.0 are spoken for, 2.3.0 is current). If 033 or
057 merge first with different numbers, the edit takes whatever is next free. No `CRendererServices` member is added or removed.

### 0.4 Starting tree, macOS (T003 to T006)

- T003: `build` was up to date at `278b0394` (nothing to compile). T004: `build-base` is a copy of it with every Mach-O's `LC_RPATH` rewritten from
  `build/...` to `build-base/...` (`install_name_tool -rpath`); `DYLD_PRINT_LIBRARIES` on a camera-dof render shows the 4 non-system libraries
  all inside `build-base` (`tools/render_in.py`, `LIBS=1`), and the render wrote `camera-dof.tif`. 663 MB.
- T005, the everyday suite (`ctest -LE 'determinism-sweep|gi_determinism_long' -j4`, 318 s of wall time): **1363 tests, 100 % passed, 0 failed,
  1 skipped** (`LibShader_Format_rslo_lower_minor`, skipped by its own rule). No intermittent failure appeared, so nothing was re-run alone.
  `tools/base-LastTest-macos.log` is ctest's log (the SC-008 reference).
- T006, `ctest -L architecture`: 36 tests pass; `archcheck`: `platform=macos objects=928 sources=211 edges=2502 (include=1680 link=822)`,
  `ALLOWED DB-004 x 3 DB-011 x 144`, `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0`; METRIC refs: Geometry 2, Hiders 489,
  Infra/SceneInterface/Shading/Texturing 0, total 491. `tests/architecture/allowlist.txt` has **168 lines** (the same as at `278b0394`).
  `tools/archcheck-base-macos.txt`.

### 0.5 GI scene images, margins, noise, timing (T008 to T010)

- T008: the 18 scenes of `determinism-scenes.cmake` ("GI scenes: not swept") rendered by `build-base` at `-t:1` from a scratch copy with
  `Quantize "rgba" 0 0 0 0` on its first line; every image is a 32-bit float TIFF (checked from the tags; the "rgb" Display of
  `sphere-ray-self-hit-*` too), 18 images (one Display each). Two complete runs are **byte-identical for all 18**: no scene has noise at `-t:1`
  (`tools/baseline-gi-images-macos.sha256`, `tools/baseline-gi-noise-macos.txt`). Images stay in `build-logs/gi-base-images/` (untracked).
  `sphere-occlusion-reyes` and `-slo` hash alike, as do `sphere-photonmap-reyes` and `-slo`, the two `sphere-ray-self-hit-reyes` variants and
  the two `sampling-probe-independence-*` scenes; `sphere-indirectdiffuse-reyes` and `-slo` do not (see 0.7 finding F1).
- T009 (`tools/baseline-gi-diffs-macos.txt`, MaxBlockAvgDiff against threshold): `sphere-occlusion-reyes(-slo)` 12.36 / 20, `sphere-photonmap-reyes(-slo)`
  5.97 / 20, `sphere-indirectdiffuse-reyes(-slo)` 0.00 / 20, `photon-parity(-slo)` 0.00 / **6**, `photon-solar0-{alone,after-distant}(-slo)` 0.00 / 20,
  `sphere-ray-self-hit-*` 0.00 / **0**, `subdiv-facevarying-seam-photon` 1.59, `-new-tags-` 1.88, `-hierarchical-override-` 0.44, `-loop-` 0.97, each / 20.
  `NotRequired_motion-subdiv-translate-photon` prints no diff. Widest margins to lose at Phase 7: `sphere-occlusion-reyes` (7.64) and the
  threshold-0 and threshold-6 tests.
- T010 (`tools/timing-base-macos.txt`, build-base against build-base, 5 interleaved series, SCALES `perf-occlusion-cache:format=12,shadingrate=3
  perf-indirect-photon:format=6,shadingrate=3,emit=10 perf-photon-pass:emit=75`; `perf-photon-pass` on 1 thread by the script's default):

  | scene | median start / final (s) | spread start / final (s) | diff of medians |
  |---|---|---|---|
  | perf-occlusion-cache | 4.41 / 4.43 | 0.44 / 0.42 | +0.02 s (+0.4 %) |
  | perf-indirect-photon | 5.20 / 5.23 | 0.49 / 0.37 | +0.03 s (+0.6 %) |
  | perf-photon-pass | 3.62 / 3.67 | 1.01 / 0.78 | +0.05 s (+1.5 %) |

  No scene is under the 3 s floor. The noise bar for Phase 3 and Phase 7 is the larger spread: **0.44 s, 0.49 s and 1.01 s**. Thermal state: no
  reading is available from the shell; the job ran alone under the machine-wide lock right after the 16-series crash job (about 5 minutes of
  full-core load), and the fanless M2's spread is the number that matters. `perf-photon-pass` has the widest relative spread (28 %).

### 0.6 The `indirectdiffuse` counters (T011), record counts (T012, T013)

**What each counter counts** (all in `CIrradianceSampler::sample`, `src/ri/hiders/irradianceSampler.cpp`; context members, flushed into `CStats` in
`~CShadingContext`, `shading.cpp:540`; printed by `stats.cpp:218-242`):

- `numIndirectDiffuseSamples` is `++` once per call of the sampler on its indirectdiffuse branch (line 400), that is once per **cache miss** that
  makes a hemisphere; `numIndirectDiffuseRays` is `+= numSamples` (`nt * np`, line 399) on the same branch. Both are incremented together and
  flushed together; `numOcclusionSamples`/`numOcclusionRays` are the occlusion branch's twins (lines 286-287). "Total Traced" is a different
  counter (`numTracedRays`, through `traceRay`).
- Occlusion behaves as the code says: `perf-occlusion-cache` reports 1472 samples and 17664 rays = 1472 x 12, Total Traced 17664; with
  `maxerror 0` it reports 26372 samples, 316464 rays (= 26372 x 12).
- **Measured on the indirectdiffuse scenes** (`-x 3 -t:1`, `measure_gi.py stats`, each with the attribute left alone, `maxerror 0.5` and `maxerror 0`;
  the default `maxerror` is 0, so "left alone" is cache OFF): `suite-indirect` (`sphere-indirectdiffuse-reyes.rib`): **26372 samples, 0 rays, Total
  Traced 0**, identical in all three; `perf-indirect-photon` (frame 2): **575248 samples, 0 rays**, Total Traced 24490 = the photon pass's own
  24490, identical in all three.

**Explanation.** The counters are consistent and are not a statistics defect: rays is zero because `numSamples` is zero on every call. `nt = (int)(sqrt(samples /
pi) + 0.5)` is 0 whenever the sampler sees `traceParams.samples < 0.785`, so `indirectdiffuse()` in these probe scenes **traces no ray at all** (Total
Traced is 0 in the suite scene; 575248 and 26372 are the number of lookups, one sampler call per shading point, whatever `maxerror` is, so the cache
never reuses a record: 26372 is also the no-cache occlusion count). Consequences measured: the baseline image of `sphere-indirectdiffuse-reyes` has
**6504 NaN pixels in each of R, G and B and zero elsewhere** under the interpreter (`1 / numSamples` is inf, times a zero sum), and all-zero RGB under
the JIT (`-slo`), alpha unchanged; the written cache of `perf-indirect-photon` (`maxerror 0.5`) holds **0 records** (a 96-byte file). The checked-in
8-bit reference compares at 0.00, so it carries the same zero colour. How the `12` of `indirectdiffuse(P, Nf, 12)` fails to reach the sampler as
`samples` was not found by reading (`IDEXPR` sets `scratch->traceParams.samples = *op3` exactly as occlusion's does, and occlusion's sampler call sees
12); it is reported as a finding (F1), not fixed. **No later task relies on an `indirectdiffuse` count.** T024 `GiDet_IndirectRayCount` must count
through the recording sampler double or through `Total Traced`, and its red reason is "0 rays traced" before it is "a jittered grid".

**T012**: `tools/icache_dump.py` parses `.icache` files (header, `maxDepth`, nodes, 56-float records, `hasChild[8]`; Python standard library only). The fixture
cache of `GiFixture_ICache` (25348 bytes) parses to the last byte: maxDepth 8, 86 nodes, **93 records**, equal to the 93 records the
`GiRemote_LoopbackCache` sender reads (`cache: 93 records sent` in `base-LastTest-macos.log`).

**T013** (`tools/records-base-macos.txt`, build-base, `-t:1`, unscaled): `perf-occlusion-cache` writes **258 records** (331 nodes, 75048 bytes) for 1472
sampler calls (R2 gave 1472 as "records": it is the sampler-call count, so the Phase 5 comparison base is **258**; why 1214 of the 1472 samples are not
in the file was not investigated); `perf-indirect-photon-cache` (new scratch scene, `maxerror 0.5`) writes **0 records** (F1).

### 0.7 Crash counts, frame-2 series, ASan for #164 (T014, T014a, T015)

**T014** (`tools/photon-crash-base-macos.txt`; 30 fresh-directory processes per series on build-base; the default thread count is the machine's 8):

| scene | default threads: crashed / distinct maps | -t:4: crashed / distinct maps |
|---|---|---|
| photon-parity | 1 / 29 | **11** / 19 |
| photon-parity-slo | 3 / 27 | 7 / 23 |
| photon-solar0-alone | 10 / 20 | 9 / 21 |
| photon-solar0-alone-slo | 9 / 21 | 11 / 19 |
| photon-solar0-after-distant | 10 / 20 | 11 / 19 |
| photon-solar0-after-distant-slo | 5 / 25 | 11 / 19 |
| photon-solar0-before-distant | 8 / 22 | 7 / 23 |
| photon-solar0-before-distant-slo | 8 / 22 | 5 / 25 |

Signals: SIGSEGV (-11) most, SIGABRT (-6) and SIGTRAP (-5) the rest. **Every completed run of every series wrote a different map** (distinct maps =
completed runs, for all 16 series), `numItems` 46273 / 8459 / 22408 / 22317 except one `photon-solar0-alone-slo -t:4` run with 8458. The plan-stage
"9 of 30 at -t:4" is 11 of 30 on this run (same order). The solar0 scenes' own comment says "render at -t:1": they crash at the default thread count.

**T014a** (`tools/photon-frame2-base-macos.txt`; frame 1 removed, float image, the map a `-t:1` run of the full scene left, copied unchanged; build-base):
`photon-parity` frame 2 alone: **-t:4: 29 distinct md5s in 30 runs (0 failed); -t:1: 1 md5 in 30 runs.** `subdiv-loop-photon` frame 2 alone: **1 md5 at
-t:4 and 1 md5 at -t:1 (the same one)**. So H1 holds for `photon-parity`, `photon-parity-slo` and presumably the `photon-solar0-*` scenes: their
second frame depends on lookup order through the memo, with the map fixed. It does **not** hold for the `subdiv-*-photon` and
`motion-subdiv-translate-photon` scenes: their frame 2 has no `Attribute "photon" "string globalmap"` and shades with `matte` or `show_st` on the
raytrace hider (grep of all seven), so their images do not read a map and are the same whatever the photon pass wrote; only their `.gpm` files
(`FILES`) can differ (F2).

**T015** (`tools/asan-164-tally-macos.txt`, `asan-164-report-macos.txt`, `asan-164-doublefree-report-macos.txt`): `build-asan` (RelWithDebInfo, JIT off,
`-fsanitize=address -fno-omit-frame-pointer`, configure and build through the lock; 189 MB; not deleted). `photon-parity -t:4`: the first run reported.
A 30-run series: **13 of 30 runs reported, 7 heap-buffer-overflow and 6 double-free.** The overflow: WRITE of size 8 at `CPhotonStore::store()`
`photonStore.cpp:547` (`keys[numItems] = key`) into the 8-byte array `ensureKeys()` (`photonStore.cpp:567`) allocated for `maxItems + 1 == 1`:
`maxItems` was 0 while `numItems` was 1. The double-free: `CMap<CPhoton>::reset()` `map.h:156` (`delete items`) from `CPhotonStore::reset()`
(`photonStore.cpp:186`) from `CPhotonHider::tracePhoton()` `photon.cpp:638`, on two threads; the block came from `CMap::balance()` in the
constructor of the empty map. **R5 is confirmed and sharpened**: the cause is the unlocked check-then-act at `photon.cpp:633-638` (`isModifying()`,
`setModifying(TRUE)`, `resetPhotons()`: several threads pass the test and each resets the same map); it shows as a double free of `items`, or
(because `reset()` zeroes `maxItems`/`numItems` and frees `keys` under a thread in `store()`) as an overflow of the side key array that spec 032 added.
It is not only "`reset()` while another thread is in `store()`". No TSAN run was needed. #165 (`delete` for `new[]`) is visible in the same frame.

### 0.8 Linux (T016, T017, T018)

Tree `~/Projects/Development/CLI/openrender-040` on `ubuntu-2404-desktop` (2 cores, GCC 13.3, cmake 3.28.3, LLVM 18; Release, JIT on,
`INSTALL_SELFCONTAINED=OFF`, `-j2` through the remote `cpujob`). `linux_sync.sh push` sent 4348 files, `verify`: **4348 files, 0 differences**
(re-pushed as tools were added; last verify 0 differences; the host's other trees were not touched). The first build finished in one cpujob (about
11 minutes, exit 0; the default 2700 s timeout was enough, so the 7200 s variable was not needed). `build-base` was frozen by `cp -a build build-base`; the
binaries' RUNPATH still names `build/`, so **a Linux `build-base` resolves its libraries inside itself only through `LD_LIBRARY_PATH`**, which
`measure_gi.py`, `image_manifest.sh` and `timing_sc007.sh` set from the tree's own `src` directories (`ldd` with that variable shows all four shared
libraries under `build-base`, without it under `build`). Every Linux baseline below was taken that way.

- Suite (`ctest -LE 'determinism-sweep|gi_determinism_long' -j2`): **1364 tests, 1362 passed, 2 failed, 1 skipped** (`LibShader_Format_rslo_lower_minor`).
  (a) `TextureTile_RenderByteIdentical` fails in 6 of its cases ("output differs from reference", byte 5); it already failed in spec 032's Linux
  baseline (`base-LastTest-linux.log` of 032, test 1197) and passes on macOS: **known Linux failure, not new**. (b) `Registry_IntegratorImageIdentity`
  fails ("photon: control: two renders with no statement are identical"): it renders the photon frame twice and compares, so it is **#163 itself**; it passed on macOS in
  this run by chance (research.md R1: 2 distinct maps in 30 runs), so it can flap on either platform until Phase 3. `tools/base-LastTest-linux.log`.
- Architecture: 36 passed; `tests/architecture/allowlist.txt` 168 lines (`archcheck-base-linux.txt`: the ctest summary; archcheck on Linux: `objects=943 sources=214 edges=2507`, `ALLOWED DB-004 x 3 DB-011 x 144`, `RESULT PASS violations=0 stale=0`, the same as macOS).
- Images (T008): the same 18 scenes, two runs **byte-identical** (`baseline-gi-images-linux.sha256`, `baseline-gi-noise-linux.txt`). 8 scenes hash like macOS
  (the `sampling-probe-independence-*`, `sphere-photonmap-*`, `sphere-ray-self-hit-*` and `sphere-indirectdiffuse-reyes-slo`); the rest differ between platforms
  as expected (libm, FMA). T009 (`baseline-gi-diffs-linux.txt`): the same numbers as macOS except `photon-parity(-slo)` **0.92** / 6, `photon-solar0-alone(-slo)` 0.12,
  `photon-solar0-after-distant(-slo)` 1.52.
- Counters (T011) and fixture: identical to macOS (26372 samples / 0 rays; 575248 / 0; occlusion 1472 / 17664 and 26372 / 316464).
- Records (T013, `records-base-linux.txt`): `perf-occlusion-cache` **242 records** (319 nodes, 70840 bytes; macOS 258), `perf-indirect-photon-cache` **0** (F1).
- Crash counts (T014, `photon-crash-base-linux.txt`; 30 fresh runs each; every crash is SIGABRT (glibc); the default thread count is 2): `photon-parity` 3 (default) and
  **0** (-t:4); `-slo` 0 and 1; `solar0-alone` 7 and 1; `-alone-slo` 4 and 0; `-after-distant` 9 and 1; `-after-distant-slo` 7 and 0; `-before-distant` 4 and 3;
  `-before-distant-slo` 3 and 4. **Every completed run of every series wrote a different map** (distinct maps = completed runs, in all 16 series).
  Linux crashes less often than macOS and the plan-stage figure ("9 of 30 at -t:4") is a macOS figure.
- Frame 2 (T014a, `photon-frame2-base-linux.txt`): `photon-parity` frame 2 alone: **30 distinct md5s in 30 runs at -t:4**, 1 at -t:1; `subdiv-loop-photon`: 1 md5 at both.
- Timing (T010, `timing-base-linux.txt`, base against base, 5 series; Linux 2 cores, so slower): `perf-occlusion-cache` 9.04 / 9.10 s (spread 0.27 / 0.30),
  `perf-indirect-photon` 13.83 / 13.84 s (0.44 / 0.30), `perf-photon-pass` 7.99 / 7.94 s (0.35 / 0.39); all within spread. Noise bar: **0.30, 0.44 and 0.39 s**.

### 0.9 Checkpoint 0 summary

`git add specs/040-gi-determinism` done. Nothing contradicts the plan except: F1 (`indirectdiffuse()` traces no ray), F2 (only `photon-parity(-slo)` read their map in frame 2),
F3 (`Registry_IntegratorImageIdentity` flaps, #163), the R5 cause being a check-then-act race with two ASan manifestations, R2's "1472 records" being 258 records,
and the default `maxerror` being 0. No fix landed on master; no baseline is unstable (all float images repeat at `-t:1`).

## 1. Architecture reports per checkpoint

| checkpoint | `ctest -L architecture` | `allowlist.txt` lines | note |
|---|---|---|---|
| 0, macOS | 36 passed; `RESULT PASS violations=0 stale=0 badid=0 failing-cycles=0 notbuilt=0` | 168 | `tools/archcheck-base-macos.txt`; ALLOWED DB-004 x 3, DB-011 x 144 |
| 0, Linux | 36 passed (`archcheck-base-linux.txt`) | 168 | |

## 2. Red evidence

(Phase 1)

## 3. Checkpoint records

### Checkpoint 0 (Phase 0)

(see §0; the expected-image-drift list is empty: no code changed)

## 4. Images and sweeps

`tools/baseline-gi-images-macos.sha256` (18 float images, two identical runs). The sweep is not run in Phase 0.

## 5. Timing (FR-023)

Baseline: §0.5 (T010). Phase 3 and Phase 7 compare against it.

## 6. Issue evidence

(Phase 2 onward; baselines: #164 reproduced under ASan §0.7; #163 reproduced in research.md R1)

## 7. Departures and findings

**Findings for the maintainer (nothing here was fixed or filed):**

- **F1 (out of spec, to file; touches FR-012, T024, Phase 5):** `indirectdiffuse()` in the probe scenes traces no ray: `CIrradianceSampler::sample` runs once per shading point with `numSamples == 0`
  (26372 and 575248 sampler calls, 0 rays, Total Traced 0 outside the photon pass), whatever `maxerror` is, on both platforms. The interpreter's `sphere-indirectdiffuse-reyes` image has 6504 NaN
  pixels per colour channel (`1 / numSamples`), the JIT's is all zero, the written cache has 0 records, and the visual test passes at 0.00 against a reference that carries the same black.
  Root cause not found (the samples argument is set the same way as occlusion's). Consequences: `GiDet_IndirectRayCount` cannot be red for "a jittered grid" first, it is red for "0 rays"; Phase 5's quality gate and
  record comparison for `indirectdiffuse` have nothing to measure until it works; spec 040's own rewrite of the sampler (FR-012) may fix it, but the starting tree has no working indirect baseline.
- **F2 (H1 premise):** only `photon-parity(-slo)` and the `photon-solar0-*` scenes read the map in frame 2 (all use `photonparity_view "string mapname"`; T014a measured `photon-parity` only). The five `subdiv-*-photon` / `motion-subdiv-translate-photon` scenes' frame 2 has no `globalmap` and shades with `matte` or `show_st`: their images do not depend on the photon pass (T014a control: 1 md5 under both thread counts on both platforms). Their sweep entries are decided by the `.gpm` bytes only, so the Phase 3 guard of H1 is needed for the photon-parity and solar0 entries, not for those five.
- **F3:** `Registry_IntegratorImageIdentity` is #163 and flaps on the starting tree (red on Linux, green on macOS in the same state). The Phase 1 to 2 checkpoints must not call it a regression.
- **F4 (R5 confirmed and sharpened):** see 0.7: an unlocked check-then-act at `photon.cpp:633-638`; manifests as a double free in `CMap::reset()` and as a `keys` overflow in `CPhotonStore::store()`. The photon pass also crashes at the default thread count in all 8 scenes, not only at `-t:4` (macOS 1 to 10 of 30; Linux 0 to 9 of 30).
- **F5:** R2's "1472 records" for `perf-occlusion-cache` is the sampler-call count; the file holds 258 (macOS) / 242 (Linux) records. Phase 5 compares against these. The default `maxerror` is 0 (cache off): `perf-indirect-photon.rib` runs with the cache off, so `perf-indirect-photon-cache.rib` adds `maxerror 0.5`.
- **F6:** `TextureTile_RenderByteIdentical` fails on Linux in the starting tree (already so in spec 032's baseline).

**Departures from tasks.md:**

- T015 built three explicit targets (`orender file openrender_shaders`), not the default targets (the test executables are not needed to render); the ASan configure, build and runs went through `cpujob` and ran under the coordinator's scoping message (not a separate ask). `build-asan` (189 MB) is kept. No source file was edited.
- T008: `image_manifest.sh` gained the modes `gi-render` and `gi-hash` (spec 032's modes are unchanged); the float line is put on the first line of the scratch copy as the harness does.
- T014, T011, T013 and T014a run through `tools/phase0_jobs.sh <step>` (one cpujob per series or render, the same script on both platforms); `measure_gi.py` gained the `M040_SEED` variable (T014a). `phase0_asan.sh`, `phase0_asan_run.py` and `phase0_linux_chain.sh` are the ASan and Linux drivers. The first macOS runs of T011 and T013 used two equivalent scratch scripts (`build-logs/`), then were re-run with the `maxerror` variants through `phase0_jobs.sh`.
- T007: the spec's `Sampling_Occlusion*` does not exist; `OcclusionSampler_*`, `OcclusionGolden_*` and `SamplingJit*` stand for it. `gi-test-set.txt` lists regular expressions, 25 patterns, 95 tests.
- T017: the Linux first build used the default 2700 s timeout (it finished in about 11 minutes).
- `tools/base-LastTest-macos.log` and `base-LastTest-linux.log` are force-added (`*.log` is ignored), as spec 032 did.
