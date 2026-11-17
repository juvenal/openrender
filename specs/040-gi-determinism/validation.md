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
12); it is reported as a finding (F1 = GitHub #173), not fixed. **No later task relies on an `indirectdiffuse` count.** T024 `GiDet_IndirectRayCount` must count
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
| 1, macOS | 36 passed | 168 | no new edge (tests/ only) |
| 1, Linux | 36 passed | 168 | |

## 2. Red evidence

### 2.0 Phase 1 as written before PAUSE 1a (nothing applied, nothing built red)

New tests (all in `tests/gi_determinism/`, defined only with `-DOPENRENDER_040_RED_TESTS=ON`; targets `EXCLUDE_FROM_ALL`): the expected red reason of each, from its header comment.

| test | source | expected red reason on the starting tree |
|---|---|---|
| GiDet_PhotonClaim | test_gi_photon_store.cpp (GIDET_CLAIM) | build failure: `CPhotonStore::storePhotonForFrame` |
| GiDet_PhotonFileBytes | test_gi_photon_store.cpp | assertion (undefined padding bytes, item 0, two writes differ; run with MallocPreScribble / MALLOC_PERTURB_) |
| GiDet_PhotonLookupPure | test_gi_photon_store.cpp | assertion (the memo, `searchRadius`) |
| GiDet_PhotonInMemoryLookup | test_gi_photon_store.cpp | crash (`insert()` dereferences a null root, #160) |
| GiDet_PhotonRoundTrip | test_gi_photon_store.cpp | crash (in-memory lookup) |
| GiDet_PhotonRepeat, _PhotonRepeatReuse, _PhotonThreads (+ the 30-run `gi_determinism_long` forms) | test_gi_photon_runs.cpp | assertion (2 / 28 distinct maps; a crash of a child is a failure) |
| GiDet_LookupPure, _CommitOrder, _CommitDedupe, _CacheRoundTrip, _AcceptPure, _BatchClamp | test_gi_cache_store.cpp | build failure: `addPending`, `commitPending`, `clampAgainstFixed`, the batch-view `lookupCached`, `irradianceRecordIdentity`, `irradianceAcceptValue`/`IrradianceAcceptState`, `irradianceBatch.h` |
| GiDet_MissIndependence_{rslo,slo} | test_gi_miss.cpp | assertion (records differ with 0, 10, 1000 fixed records; the precondition, that the lookup visits every fixed record and accepts none, is checked in the child) |
| GiDet_IndirectRayCount_rslo | test_gi_miss.cpp | assertion: 0 rays traced (GitHub #173) |
| GiDet_IndirectRayCount_slo | test_gi_miss.cpp | **expected green on the starting tree**: the scene run through orender's statistics gives Total Traced 624 = 12 x 52 sampler calls under the JIT (the rslo run: 0 of 102). #173 is interpreter-only. The case checks the number of rays, not that each direction is keyed (the starting tree draws a jittered grid of 12): the keyedness of the directions is covered by `GiDet_MissIndependence` and the sweep, not here |
| GiDet_RepeatedPoint_{rslo,slo} | test_gi_miss.cpp | assertion at -t:4 |
| GiKeys_IrradianceRecordsThreads | gi_split patch | assertion at -t:4 (or equal if the premise of M2 already holds: reported either way) |
| GiRemote_LoopbackCommit | gi_split patch | build failure: `addPending`, `commitPending` |
| DeterminismHarness_FilesDiffer / _FilesSame | harness patch | exit 2 (the options are unknown) |
| DeterminismSweep_gi-* (34 entries) | sweep patch | per entry in `tools/sweep-entries-red-table.txt` |

**Names the tests propose for the code that does not exist yet** (the tests are the contract; PAUSE 1a approves them with the tests):
`CPhotonStore::storePhotonForFrame(P, N, I, C, key) -> bool`; Texturing, `texture/irradianceStore.h`: `IrradianceAcceptFn = float (*)(const void *acceptState, uint64_t recordIdentity)`, `struct CIrradianceBatchView {const CIrradianceRecord *records; int count;}`, `uint64_t irradianceRecordIdentity(const CIrradianceRecord &)` (the key, or a hash of the bits of P and N when the key is 0), the new `lookupCached(C, P, N, maxError, accept, acceptState, batch, Pc, Nc)`, `addPending(record, maxError)`, `clampAgainstFixed(record) const`; `CIrradianceCache::commitPending()`, `forEachPending(F)`, `receivePending(const CCacheSample &, float maxError)`; Shading: `libshader::IrradianceAcceptState {SubstreamKey call}` and `libshader::irradianceAcceptValue(const void *, uint64_t)` in `irradianceCalls.h`, and the new header `libshader/shading/irradianceBatch.h` with `libshader::IrradianceBatchEntry {P[3], raw, radius}` and `float clampAgainstBatch(IrradianceBatchEntry *earlier, int count, const float *P, float raw, float radiusAgainstFixed)`. "The bytes" of a record for the dedupe order are the 56 floats in file order compared with `memcmp`.

**Compile check before PAUSE 1a** (`-fsyntax-only` through cpujob, the project flags with `-Werror`, nothing built or run): against the starting tree's headers the new sources compile except for the missing members (`test_gi_photon_store.cpp` with `GIDET_CLAIM`: `storePhotonForFrame`; `test_gi_cache_store.cpp`: the missing header `irradianceBatch.h`); `test_gi_photon_runs.cpp` and `test_gi_miss.cpp` (starting-tree hook) compile clean, so they are red by assertion. Against scratch stub declarations of the new members (`build-logs/040-p1-stubs`, not in the tree) `test_gi_cache_store.cpp`, `test_gi_miss.cpp` with `GIDET_RECORD_HOOK_ADDPENDING`, and the patched `test_gi_remote_loopback.cpp` compile clean: no unrelated error is hiding behind a missing member. The patched `test_gi_keys.cpp` compiles clean against the starting tree.

### 2.1 Red evidence (T033), macOS, starting tree `278b0394`, 2026-11-15

`build` configured with `-DOPENRENDER_040_RED_TESTS=ON`; each red target built alone through cpujob; `ctest -R` through cpujob. Logs: `build-logs/040-red-*.log`, `040-p1-red.log`, `040-p1-sweep.log`.

| test | result | the first line of its failure |
|---|---|---|
| DeterminismHarness_FilesDiffer / _FilesSame (before step 2 of T031) | red, exit 2 | `Unknown option --files` |
| GiDet_PhotonClaim | build failure | `test_gi_photon_store.cpp:298: no member named 'storePhotonForFrame' in 'CPhotonStore'` |
| GiDet_PhotonFileBytes | **green on the first run (finding)**, red after the test was strengthened (see below) | `FR-004: a byte of the first map is not defined: item 0 byte 0 is 0xa5` |
| GiDet_PhotonLookupPure | **green on the first run (finding)**, red after the test was strengthened | `FR-005: lookups on one store differ from lookups on a fresh store each (overload (C, P, N, maxFound))` and the same for `(C, P, maxFound)` |
| GiDet_PhotonInMemoryLookup | red, SEGFAULT | (the null root, #160) |
| GiDet_PhotonRoundTrip | red, SEGFAULT | (the in-memory lookup) |
| GiDet_PhotonRepeat | red | `SC-002: 2 distinct maps in 5 runs (one expected)` |
| GiDet_PhotonRepeatReuse | red | `5 distinct maps in 5 runs` |
| GiDet_PhotonThreads | red | `FR-001: 5 distinct maps across -t:1, -t:2, -t:4, -t:8 and the default` |
| GiDet_LookupPure, _CommitOrder, _CommitDedupe, _CacheRoundTrip, _AcceptPure, _BatchClamp | build failure | `test_gi_cache_store.cpp:40: 'libshader/shading/irradianceBatch.h' file not found` (then the other missing members) |
| GiDet_MissIndependence_rslo / _slo | red | `FR-011: the record the miss makes differs with 10 other records in the fixed part` (the precondition printed first: 10 and 1000 fixed records visited, none accepted) |
| GiDet_IndirectRayCount_rslo | red | `Total Traced is 0, 12 x 102 expected (indirectdiffuse() traced no ray at all)` (#173) |
| GiDet_IndirectRayCount_slo | **green, as expected** (the maintainer's note on #173) | 624 = 12 x 52 |
| GiDet_RepeatedPoint_rslo / _slo | **green (finding)** | the recording cache saw 48 records at reyes and 7 at raytrace, **0 repeated keys**, none shared P and N: the scene does not reproduce a point shaded twice on the starting tree, so M9's case is a guard here, not a red test |
| GiKeys_IrradianceRecordsThreads | **green (finding)** | the (key, record) multisets at -t:1 and -t:4 are identical for reyes, zbuffer and raytrace, `.rslo` and `.slo`: the premise of mechanism M2 (research.md R4) holds on the starting tree |
| GiRemote_LoopbackCommit | build failure | `test_gi_remote_loopback.cpp:359: no member named 'addPending' in 'CIrradianceCache'` |

Strengthened tests (two edits to my own Phase 1 sources, shown as `tools/test-edit-patches/pause1-photon-store-fixes.patch` for approval at PAUSE 1): the first run of `GiDet_PhotonFileBytes` was green because the heap pages were zero (the test's heap churn did not reach the arrays it checks), and the first run of `GiDet_PhotonLookupPure` was green because its queries were far apart and compared an order against the same store's memo. Now the executable replaces `operator new`/`new[]` with a 0xA5-filling version (so the bytes the store never assigns are not zero on any platform) and lookup-pure compares each answer with the answer of a fresh store, with neighbouring queries, for both lookup overloads, and requires that most queries found photons. Neither change reads a result before it is made; the original runs are recorded above.

A defect of the Phase 1 patch as approved, and its fix (the diff is in `gi-remote-loopback-commit.patch` and `gi-split-cmake.patch`, regenerated against HEAD): the new `commit` case calls `addPending`, so compiling it into `test_gi_remote_loopback` (a target of `all`) broke the everyday build with the option OFF. The case is now compiled only with `GIDET_COMMIT_CASE`, into a second executable `test_gi_remote_commit` (EXCLUDE_FROM_ALL, in the red block) that carries `GiRemote_LoopbackCommit`; `test_gi_remote_loopback` is unchanged in behaviour and builds.

DeterminismSweep_gi-* (34 entries, step 2 of T031 applied so that the options exist; `-j2`): **18 red, 16 green** (the fork's expectation was 20 and 14). Red: the photon entries `gi-subdiv-hierarchical-override-photon` (`.gpm` byte 42 differs), `gi-subdiv-loop-photon`, `gi-photon-parity` and `gi-photon-parity-slo` (maps and about 19300 image values each), and `gi-photon-then-indirect-reyes-slo` (cache bytes and image); the two `sphere-occlusion-reyes(-slo)` entries (**images** differ between thread counts: 8571 to 10974 values, the cache is off there); the cached `gi-occlusion-*` (6), `gi-indirectdiffuse-*-slo` (3) and `gi-cache-extend-*` (2) entries (the `.icache` bytes differ at `-P:2`; their images agree except `gi-cache-extend` at `-P:2`). Green where red was expected: `gi-motion-subdiv-translate-photon`, `gi-subdiv-facevarying-seam-photon`, `gi-subdiv-new-tags-photon` and `gi-photon-then-indirect-reyes` (the 2000-photon races happen with a probability below 1: these are flaky greens, not fixed ones), and the two `gi-sphere-occlusion-reyes(-slo)` went red where green was expected. The five photon entries that leave the guard in Phase 2 are the ones to watch: three of them can pass by luck on the starting tree.


## 3. Checkpoint records

### Checkpoint 5 (Phase 5: the cache's fixed part, batch scope and canonical commit, M2), macOS, 2026-11-17

Build `cpujob 040-p5-build*` (exit 0, no first-party warning). Sources: `irradianceStore.h` (`clampAgainstFixed`, `addPending`; `insertRecord` removed), `irradianceCache.{h,cpp}` (`CPending`, `addPending`, `commitPending`, `forEachPending`, the kd-tree index of the fixed part, `readNode` appends, the commit drops the key), `libshader/shading/irradianceBatch.h` (new), `irradianceCalls.cpp`, `shading.{h,cpp}` (`CShadingState::irradianceBatch`, reset with the draw occurrences), `rendererServices.h` (history entry extended). The key-0 rule (maintainer): every key-0 pending record is kept (no dedupe); only a non-zero key merges.

| test | result |
|---|---|
| `GiDet_CommitDedupe`, `_BatchClamp`, `_AcceptPure`, `_MissIndependence_*`, `_IndirectRayCount_slo`, `_RepeatedPoint_{rslo,slo}` | pass (after the key-0 correction too) |
| `GiKeys_IrradianceRecords`, `GiKeys_IrradianceRecordsThreads` (the batch-identity gate: `-t:1` against `-t:4`, reyes, zbuffer, raytrace, `.rslo`/`.slo`: 37 records each, identical) | pass |
| `TextureOnly_*`, `GiSplit_Boundary`, `GiRemote_LoopbackLookup` | pass |
| cache sweep `^DeterminismSweep_gi-` (34 entries, `-j2`: float colour, alpha, depth and every `.icache` byte-identical at `-t:1`, default, `-t:2`, `-t:4`) | 34 of 34, before and after the key-0 correction |
| `GiDet_LookupPure`, `_CommitOrder`, `_CacheRoundTrip` | **fail on the tests' own "more than 20 hits" preconditions, not on the code** (hit rate ~1 in 100 with normals uniform on the sphere; a simulation of the test's generator gives about 5 hits with no clamp at all). A scratch build of `test_gi_cache_store.cpp` with the proposed normals patch (`tools/test-edit-patches/phase5-store-test-normals.patch`, /tmp/p5b) passes all six modes, 100 `CommitOrder` permutations included. Held for the maintainer |
| `-L architecture` | 36 of 36 |
| `-L '^link$'`, `shading_contract` | 76 of 78: the two `ShadingContract_TraceReplay_*` (Phase 4's reference change, T093a) |
| everyday suite (1385) | 13 failures: the ten of Phase 4 (`Preview_DataSceneUnchanged`, `Visual_sphere-occlusion-reyes(-slo)`, `Visual_photon-parity(-slo)`, `OcclusionGolden_sobol_cache`, `GiFiles_Unchanged`, `GiRemote_LoopbackCache`, the two replays) and the three above |

**Drift (T076, `tools/drift-p5-macos.txt`, nothing adjusted):** the GI visual set fails the same tests as Phase 4; `Visual_sphere-occlusion-reyes(-slo)` 12.36 at the start, 38.78 after Phase 4, 30.72 now against 20; `Visual_photon-parity(-slo)` 7.06 against 6. All stay on the Phase 7 list.

**Quality gate (T074, §4; `tools/quality-m2-macos.txt`):** the M2 cached image against the same 256-sample truths is no worse than the starting tree's in every scene with signal: `gi-occlusion-raytrace` 5.63 to 1.59, `gi-occlusion-reyes` 2.43 to 1.72, `gi-occlusion-zbuffer` 2.77 to 2.33, `gi-cache-extend-reyes` 0.76 to 0.24, `gi-photon-then-indirect-reyes-slo` 17.17 to 1.99; `sphere-occlusion-reyes(-slo)` (cache off by default) 31.08 to 33.87 inside its seed-1 truth noise 7.31. The indirectdiffuse rows carry no signal (#173); `gi-photon-then-indirect-reyes` (rslo) 0 to 17.84 is the #173 artifact (its truth became a real image).
**Record counts** (`icache_dump.py`, one render at `-t:1`): `perf-occlusion-cache` 258 to 513; `perf-indirect-photon-cache` 0 to 0 (#173); the cached scenes, reyes and zbuffer identical for a scene (occlusion 120 to 145, cache-extend 117 to 147), raytrace 24 to 31 (its batch size, M5).

**Final macOS re-run (after the normals patch and the key-0 correction; `build-logs/040-p5-chain3.log`):** everyday suite 1385 tests, 11 failures: the ten of Phase 4 and `ShadingParity_JitPL_trace_samples_cone`, a **timeout under load** (my suite ran at -j4 while spec 033's suite held the other jobs); re-run alone through cpujob it passes in 47.7 s (`040-p5-rerun-timeout`), no timeout changed. `-L architecture` 36 of 36, `-L '^link$'` pass. The three GiDet tests the normals patch fixes pass.

**Linux CP-L (T078, `tools/phase5_linux_chain.sh`, GCC 13 -Werror, remote cpujob -j2, `build-logs/040-l-p5-*.log`):** build exit 0 (no warning-as-error). Unit set 24: only `GiFiles_Unchanged` and `GiRemote_LoopbackCache` fail (the Phase 4 reference changes). Cache sweep 34 of 34 pass. GI test set 86 of 96: the ten macOS failures. `-L architecture` 36 of 36. `link` + `shading_contract` 76 of 78 (the two replays). Suite 1386 tests, 11 failures: the ten and `TextureTile_RenderByteIdentical` (the known Linux failure). The `.icache` line of `files.txt` on Linux: size 52016 to 96528 (as on macOS: the same 359 records), SHA-256 `12c12859...` to `f22a92fe...`, the two recordings identical (`build-logs/040-files-linux-p5-{1,2}.txt`); it joins the Phase 7 list. **Linux quality gate: skipped (maintainer, 2026-11-17): the image quality is algorithmic, so it was measured on macOS against the macOS starting tree, and Linux determinism is covered by the 34 of 34 cache sweep.** (Detail: not run.) `quality_run.py` needs `gi_quality` and the test-local `_256` probe shaders built on the Linux host, and T029 produced the starting-tree numbers on macOS only (`quality-base-macos.txt`), so there is no Linux starting-tree figure to compare with; the macOS table stands. (The chain's quality step exited 1 on the missing `gi_quality`, nothing else.)

**Cost so far (T077, §5, `tools/timing-p5-macos.txt`, `timing_sc007.sh build-base build 3`):** `perf-occlusion-cache` 4.64 to 4.18 s (-10 %), `perf-indirect-photon` 5.14 to 5.16 s (+0.4 %), `perf-photon-pass` 3.69 to 4.04 s (+9.5 %): all within spread. The formal measurement is Phase 7.

### Checkpoint 8 (Phase 8: the closing validation, T105), macOS, 2026-11-17

One build (`040-final-build`), each step its own cpujob (`tools/phase8_mac_chain.sh`, `build-logs/040-final-*.log`): the everyday suite (`-LE 'determinism-sweep|gi_determinism_long'`) **1386 of 1386**; the full determinism sweep (`-L '^determinism-sweep$'`, 616 entries) **3 of 3 repetitions, 616 of 616 each**; `-L '^determinism$'` 164 of 164; `-L '^gi_determinism$'` 20 of 20; `-L '^gi_determinism_long$'` 3 of 3 (the 30-run photon forms); `-L '^gi_split(_threads)?$'` 11 of 11; `-L '^shading_parity$'` 136 of 136; `-L '^texture_only$'` 3 of 3; `-L '^state$'` 7 of 7; `-L '^plugin_abi$'` 71 of 71; `-L '^link$'` 5 of 5; `-L architecture` 36 of 36 with **0 lines added** to `tests/architecture/allowlist.txt` against `278b0394` (`build-logs/040-final-allowlist-added.txt` is empty); `Registry_IntegratorImageIdentity --repeat until-fail:30` passes (SC-002).

**Linux (T104, T106), the closing chain** (`tools/phase8_linux_chain.sh`, Ubuntu 24.04, GCC 13 `-Werror`, remote cpujob -j2, one chain; logs `build-logs/040-linux-final/040-l-final-*.log`, pulled): build exit 0. **T104:** the 30-run photon series (`tools/photon-crash-after-linux.txt`): 16 series (the eight scenes at the default thread count and at `-t:4`, 30 fresh runs each, 480 runs): **0 crashes and one `.gpm` md5 per scene, the same at the default thread count and at `-t:4`** (e.g. `photon-parity` `abd14077...` 30 of 30 in both); `Registry_IntegratorImageIdentity --repeat until-fail:30` passes (SC-002); `-L '^gi_determinism_long$'` 3 of 3. **T106:** the everyday suite 1386 of 1387, the one failure `TextureTile_RenderByteIdentical`, the known Linux failure of the starting tree (F6, spec 032's baseline; nothing else fails); the full 616-entry determinism sweep **3 of 3 repetitions, 616 of 616 each**; `-L '^determinism$'` 164 of 164; `-L '^gi_determinism$'` 20 of 20; `gi_split(_threads)` 11 of 11; `shading_parity` 136 of 136; `texture_only` 3 of 3; `state` 7 of 7; `plugin_abi` 71 of 71; `link` 5 of 5; `-L architecture` 36 of 36. The overhead series (T097) on Linux is Phase 7's (section 5): the code has not changed since.

### Checkpoint 7 (Phase 7: pins, references, thresholds, overhead), 2026-11-17

Applied after the maintainer's approval (PAUSE 7a and 7b): `phase7-pins.patch`, `phase7-default-set.patch` (the default `determinism` set 160 to 164), `phase7-thresholds.patch`, and the references of `rerendered-references.md` on both platforms. `thread-pins.md` is complete (T090).

| step | macOS | Linux (GCC 13 -Werror, -j2) |
|---|---|---|
| T091/T104: photon pins, 30 fresh runs x 16 series at the default thread count and `-t:4` | 0 crashes, one md5 per scene (`tools/photon-crash-after-macos.txt`) | not run in this phase; run in Phase 8 (T104): 0 crashes in 480 runs, see Checkpoint 8 |
| visual group at the new thresholds and references (`Visual_*` GI tests, `OcclusionGolden`, `Preview_DataSceneUnchanged`, `GiFiles_Unchanged`, `GiRemote_*`, both replays) | all pass | all pass |
| GI test set (`gi-test-set.txt`, 95 tests) | all pass | n/a (covered by the suite) |
| everyday suite (`-LE 'determinism-sweep|gi_determinism_long'`) | **1386 of 1386** (no failure, no known failure left) | 1386 of 1387: only `TextureTile_RenderByteIdentical`, the known Linux failure |
| `-L architecture` | 36 of 36 | 36 of 36 |
| the 34 GI sweep entries, three times (SC-001) | 3 of 3, 34 of 34 each | 3 of 3, 34 of 34 each |
| default `determinism` set | 164 of 164 | 164 of 164 |

### Checkpoint 6 (Phase 6: the `-P:N` wire, #161), macOS, 2026-11-17

Build exit 0. Sources: `remoteChannel.{h,cpp}` (the pending-only frame, `CHANNELFLAG_READ`), `irradianceCache.{h,cpp}` (`receivePending`, `openFlags`; `insertReceived` removed). Tests: the two approved patches (T080) and `determinism-scenes-phase6.patch` (T086).

| test | result |
|---|---|
| `GiRemote_LoopbackCommit` (#161: the receiver's committed tree and file equal the one-process commit, each record placed with its own K) | passes |
| `GiRemote_LoopbackLookup` (receiver against a one-process cache: `differ == 0`, equal hits and records visited) | passes |
| `GiRemote_LoopbackPointCloud`, `GiCache_KindMismatch`, `GiDet_*`, `GiKeys_*` | pass |
| `GiRemote_LoopbackCache` | fails on the reference only (frame and recvfile sections; setup identical): `loopback-cache.bin` is re-recorded in Phase 7 (T093a) |
| cache sweep `^DeterminismSweep_gi-` with `-P:2` (34 entries, `-j2`) | 34 of 34, the `gi-cache-extend-*` read-write scenes among them (the client holds the same fixed part as its servers) |
| `-L architecture` | 36 of 36 |
| everyday suite (1386) | 10 failures, all Phase 4's: `Preview_DataSceneUnchanged`, `Visual_sphere-occlusion-reyes(-slo)`, `Visual_photon-parity(-slo)`, `OcclusionGolden_sobol_cache`, `GiFiles_Unchanged`, `GiRemote_LoopbackCache`, the two `ShadingContract_TraceReplay_*` |

**Linux CP-L (T088, `tools/phase6_linux_chain.sh`, GCC 13 -Werror, remote -j2):** build exit 0; unit set (33) fails only `GiFiles_Unchanged` and `GiRemote_LoopbackCache` (references); `GiRemote_LoopbackCommit` and `GiRemote_LoopbackLookup` pass (#161's evidence on Linux); the `-P:2` cache sweep passes 34 of 34; GI set 87 of 97 (the ten macOS failures); `-L architecture` 36 of 36; `link`/`shading_contract` 76 of 78 (the two replays); suite 1387 with 11 failures: the ten and `TextureTile_RenderByteIdentical` (the known Linux failure). `loopback-cache.bin` on Linux, recorded twice (`build-logs/040-loopback-linux-{1,2}.bin`, identical): 33248 bytes against the reference's 48828, the same sizes as macOS's new file (the contents differ from macOS's as the starting references do, per platform); `files.txt` unchanged since Phase 5. Both loopback references and the Linux `.icache` line join the Phase 7 re-record (T093a).

### Phase 6, T081 (read-only, before any edit): how a `-P:N` client gets the fixed part and the root

Read, nothing run. **Server** (`CRenderer::getCache`, `rendererFiles.cpp:196-280`): mode `"r"` or `"rw"` opens the file (`CACHE_READ`), so the server's `F` is the file's tree and its root is the file's root; a fresh cache builds its root from the server's `CRenderer::worldBmin/Bmax`. When the render is a net server and the mode writes, the server clears `CACHE_WRITE` (it never flushes) and registers a `CRemoteICacheChannel` (`requestRemoteChannel`, `remoteChannel.cpp:50-90`): `NET_CREATE_CHANNEL` carries the channel's type and name only. **Client** (`CRenderer::processChannelRequest`, `remoteChannel.cpp:120-215`): for `CHANNELTYPE_ICACHE` it creates its cache with `getCache(channelName, "w", ...)`, and mode `"w"` is `CACHE_WRITE | CACHE_SAMPLE` with **no `CACHE_READ`**: the client's cache starts empty, whatever mode the server opened it in; `setup()` then overwrites only the root's centre and side with the server's (`sendSetupData`). The client never opens the file.

So the two parts of the plan's check come out as follows.
(b) The root: for a file (`"r"`/`"rw"`) the server's root is the file's, for a fresh cache it is the server's world-bound root; either way `setRoot` makes the client's root the server's, bit for bit. Whether the server's root equals the one-process root of the same scene (the world bound each side computes) is decided by the `-P:2` sweep entries (the root centre and side are in the written `.icache`), not by reading.
**(a) is a defect to report before the wire work continues.** On the starting tree it was hidden: each server sent its **whole tree**, file records included, so the client's empty cache received `F` from the server (and from every server, so `-P:N` writes `F` N times). Under D4-A (the server sends only its pending set) a client that never reads the file writes **only the new records**: a `-P:N` render in `rw` mode (the `gi-cache-extend` scenes, a frame that extends frame 1's cache) would lose `F` from the file. The client needs the same `F` as the servers. Options (the maintainer decides): (1) the channel request carries the server's open mode, as one more flag bit in the type word (`(type << 8) | BOM`, a protocol change: client and server are the same build), and the client opens its cache with `CACHE_READ` when the server did, reading the file itself; (2) the client reads the file whenever one exists under that name (wrong for a `"w"` render with a stale file); (3) keep sending whole trees (the starting tree's wire) and dedupe by identity at the client, which drops the point of D4. I recommend (1).

**Decision (maintainer, 2026-11-17: establish from the code which case holds; option 1 if the client reads what the servers read, else the variant).** The case that holds: *the client is the origin of every file a server reads by name*, so the client can always read the same file. Evidence: `ropen` is a plain `fopen` (`common/fileResource.cpp:66`), and `CRenderer::locateFile` (`rendererFileSearch.cpp:71-125`) searches the server's own search path and temporary directory first and, if the file is not there, **downloads it from the client by name** (`getFile`, `NET_SEND_FILE`, `rendererNetwork.cpp:371-440`, into the server's temporary directory; `registerFrameTemporary` deletes it after the frame, `rendererFiles.cpp:237-241`). So servers on other hosts without a shared path get `F` from the client's file, and a server with a local copy has the client's content by the same assumption textures and shaders already make; the client never lacks a file its servers read, because that is where they get it from. The variant (the server ships `F` at channel creation) was therefore not needed. Implemented: option 1. The channel request's type word (`(type << 8) | BOM`) carries `CHANNELFLAG_READ` in its low byte when the server opened the cache with `CACHE_READ`; the client creates its cache with `getCache(name, "rw")` in that case and with `"w"` otherwise (`remoteChannel.cpp`). Remaining edge, unchanged from the starting tree's assumptions and not handled: a server that has a *local* cache file the client does not have would send only its new records, and the client would write a file without that file's records.

### Phase 5 decision record (maintainer, 2026-11-17)

**Key 0 and the commit's dedupe.** The commit merges pending records only by a non-zero `key` (equal keys keep the smallest under the byte order). A record with key 0 is never deduped: every one is kept, as `insertRecord` kept them. Recorded in data-model.md section 5 and contracts/irradiance-store.md. The pending radius is the raw one, the batch radius is Shading's and is never stored, and `insertReceived` is unchanged until Phase 6.

### Checkpoint 4 (Phase 4: the accept rule and the indexed sampler, D1-A), macOS, 2026-11-16

Build `cpujob 040-p4-build` (exit 0, no first-party warning). Sources: `src/common/giHash.h` (new, Infra: `giMix`, `giAcceptValue`, `giRecordIdentity`), `irradianceStore.h` (`IrradianceAcceptFn`, `CIrradianceBatchView`, `irradianceRecordIdentity`, the final `lookupCached`), `irradianceCache.{h,cpp}` (a generic `consider` lambda over a cache sample and a batch record; the batch is walked after the fixed part, empty until Phase 5), `irradianceCalls.{h,cpp}` (`IrradianceAcceptState`, `irradianceAcceptValue`; `drawNextOf` removed; the cursor is not touched), `irradianceSampler.cpp` (draws by index from a copy of the bound sub-stream with `rayDraw2D`; cell `k = i * np + j`; both functions with the cache off take `samples` keyed directions). `rendererServices.h`: the 2.6.0 entry extended (no second step). Test edits: the three approved patches (PAUSE 4a).

| test | result |
|---|---|
| `GiDet_AcceptPure` | passes |
| `GiDet_MissIndependence_{rslo,slo}` | pass |
| `GiDet_IndirectRayCount_slo` | passes |
| `GiDet_IndirectRayCount_rslo` | **fails, #173 not fixed** (below); in the red block from 2026-11-17 |
| `TextureOnly_IrradianceDrawOrder` (rewritten), `TextureOnly_GiStores`, `GiRemote_LoopbackLookup`, `GiKeys_*`, `GiCache_KindMismatch` | pass |
| `-L architecture` | 36 of 36, no allowlist line |
| `-L '^link$'` and `shading_contract` | 76 of 78: the two replays below |
| `-L sampling` | 115 of 116: `OcclusionGolden_sobol_cache` |
| full suite (`-LE 'determinism-sweep\|gi_determinism_long'`, 1378) | 1367 pass; the 11 failures are the ones in the tables below |

**#173 (GiDet_IndirectRayCount_rslo).** The rewrite does not make it green. Evidence (a temporary diagnostic in `sample()`, removed): under `.rslo` the sampler sees `maxError = 0.6`, `samples = 3.67e-40` (a denormal, the bits 0x00040000, not 12), so `nt = np = 0` and `numSamples = 0`: 102 sampler calls, 0 rays. Under `.slo` the same scene traces 648 = 12 x 54. So the interpreter hands the sampler a wrong `samples` (and the scene's cache is on, `maxError` 0.6, although Phase 0's counters step found the default 0 for other scenes), which is upstream of the sampler: `IDEXPR` reads `*op3` for the `samples` argument. Left alone, as decided.

**Reference changes this phase forces (none edited, each needs REFERENCE-APPROVAL; none was foreseen in tasks.md):**

| test | reference | what changed |
|---|---|---|
| `GiFiles_Unchanged` | `tests/gi_split/reference/macos/files.txt` line 26 (the cached occlusion `.icache`) | 52016 to 46888 bytes: other records |
| `GiRemote_LoopbackCache` | its reference | the frame and recvfile sections differ (22212 to 21752, 27980 to 27012 bytes) |
| `Preview_DataSceneUnchanged` | `tests/preview/reference/macos/datascene.txt` | the cache's records changed |
| `OcclusionGolden_sobol_cache` | `tests/visual/golden/occlusion-sobol_cache.tif` (spec 039's golden, bit for bit) | 3315 of 9216 values differ |
| `ShadingContract_TraceReplay_occlusion`, `_indirectdiffuse` | `tests/shading_contract/replay/macos.txt` | request 0 ray 0 `from` differs (the origin jitter now comes from pair 1 sample `k`) |

**Decisions (maintainer, 2026-11-17).** (1) `GiDet_IndirectRayCount_rslo` moved into the red block (`IndirectRayCount_slo` stays unconditional); the #173 diagnosis above is a comment on #173, which is left alone, and there is no `Closes` line. (2) The six reference changes above are re-recorded in Phase 7 together with the images, each shown as a diff first (tasks.md T093a); none is edited before. (3) The Linux-only `Visual_sphere-texture3d-bake3d-reyes-slo` segfault (below) is an unexplained intermittent; it is filed only if it recurs.

**Expected drift (T061; `tools/drift-p4-macos.txt`, MaxBlockAvgDiff against the starting tree's, nothing adjusted):** `Visual_sphere-occlusion-reyes` and `-slo` 12.36 to 38.78 against 20 (**over threshold**, the cached occlusion image); `Visual_photon-parity(-slo)` 7.06 against 6 (Phase 3's, unchanged); `Visual_photon-solar0-*` as Phase 3; the others unchanged. `Visual_sphere-indirectdiffuse-reyes(-slo)` stays 0.00: that scene reaches no sampler ray under the interpreter (#173). All go on the Phase 7 re-render list.

**Linux CP-L (Phase 4, `tools/phase4_linux_chain.sh`, GCC 13, remote cpujob -j2; `build-logs/040-l-p4-*.log`).** Build exit 0. Unit set 17 tests: the same three failures as macOS (`GiFiles_Unchanged`, `GiRemote_LoopbackCache`, `GiDet_IndirectRayCount_rslo`), everything else passes (`GiDet_AcceptPure`, `MissIndependence_*`, `IndirectRayCount_slo`, `TextureOnly_*`, `GiKeys_*`). GI test set 85 of 95: the ten macOS failures, the same ones (`Preview_DataSceneUnchanged`, `Visual_sphere-occlusion-reyes(-slo)`, `Visual_photon-parity(-slo)`, `OcclusionGolden_sobol_cache`, `GiFiles_Unchanged`, `GiRemote_LoopbackCache`, the two `ShadingContract_TraceReplay_*`). `-L architecture` 36 of 36. `-L sampling` 115 of 116 (`OcclusionGolden_sobol_cache`); `link`/`shading_contract` 76 of 78 (the two replays). Full suite 1366 of 1379: the 11 macOS failures plus two Linux-only: `TextureTile_RenderByteIdentical` (the known Linux failure) and `Visual_sphere-texture3d-bake3d-reyes-slo`, which **segfaulted once** in the parallel run ("Segmentation fault (core dumped)", 3.16 s) and passed in 3 of 3 runs alone (`040-l-p4-bake`); the Phase 3 Linux suite did not show it. It does not touch the irradiance cache (a texture3d bake), so I treat it as an intermittent crash under load, not reproduced, and report it rather than explain it. The Linux references (`tests/**/reference/linux`, the linux golden and replay) change in the same places as macOS's and are likewise not edited.

### Checkpoint 3 (Phase 3: the photon lookup, D2-A), 2026-11-16

**Change** (`src/ri/texture/photonStore.{h,cpp}`): `PHOTON_LOOKUP_CACHE`, `probe()`, `insert()`, the memo octree, `root`, `maxDepth` and the member `searchRadius` are gone; both `lookup()` overloads are the kd-tree search, the sum and the normalisation, with `searchRadius` a local; nothing outside the store named a removed member (the data viewer, `CPhotonMap` and the tests were checked). One more change the round-trip test needed: **`maxPower` is the largest photon power itself**, kept as such at every `store()` (it was the squared power until `write()` took the square root in place, so a map's lookups before and after a write used different search radii: the in-memory and the reloaded map answered differently, SC-005). The file holds the same number as before (`GiFiles_Unchanged` and `TextureOnly_GiStores` still pass unchanged). `GiDet_PhotonLookupPure`, `_PhotonInMemoryLookup` (#160) and `_PhotonRoundTrip` left the red block; `determinism-scenes-phase3.patch` applied (the two `photon-parity` entries are unguarded).

**#160 evidence.** `GiDet_PhotonInMemoryLookup`: SEGFAULT on the starting tree (`insert()` dereferenced the null root), passes now on both platforms; `GiDet_PhotonLookupPure` (lookups equal those of a fresh store, both overloads) and `GiDet_PhotonRoundTrip` pass on both platforms.

**Photon-parity frame 2 (T014a on the finished tree, `photon-frame2-p3-<platform>.txt`).** `photon-parity` frame 2 alone: **1 md5 in 30 runs at -t:4 and 1 md5 at -t:1, and the same md5 in both** on macOS and on Linux (the starting tree: 29 distinct md5s at -t:4 on macOS, 30 on Linux). `subdiv-loop-photon`: unchanged, one md5 in each.

**Sweep** (`DeterminismSweep_gi-(photon-parity|subdiv|motion-subdiv)`: float colour, alpha and depth bit-identical under `-t:1`, the default, `-t:2` and `-t:4`, every `.gpm` byte-identical): 7 of 7 pass on both platforms, the two `photon-parity` entries (frame-2 images included) among them.

**Everyday suite.** macOS 1373 tests, 2 failures: `Visual_photon-parity` and `-slo` (expected drift, below; 7.06 against 6 stays on the Phase 7 re-render list: no threshold or reference moves now, maintainer 2026-11-16). Linux 1374 tests, 3 failures: the same two and the known `TextureTile_RenderByteIdentical`. `ctest -L architecture` 36 of 36 on both; allowlist 168 lines.

**Drift list (D2-A: every image that reads the photon map changes once; none is adjusted; each goes on the Phase 7 re-render list)** (`tools/drift-p3-macos.txt`, `tools/drift-p3-linux.txt`, MaxBlockAvgDiff against the starting tree's):

| test | before (macOS / Linux) | after (both) | threshold | verdict |
|---|---|---|---|---|
| `Visual_photon-parity` | 0.00 / 0.92 | 7.06 | 6 | **over threshold** |
| `Visual_photon-parity-slo` | 0.00 / 0.92 | 7.06 | 6 | **over threshold** |
| `Visual_photon-solar0-alone` and `-slo` | 0.00 / 0.12 | 11.09 | 20 | changed, within |
| `Visual_photon-solar0-after-distant` and `-slo` | 0.00 / 1.52 | 8.77 | 20 | changed, within |

Every other GI visual test is unchanged (`sphere-photonmap-reyes` 5.97 included: its `.pm` does not exist). The 18-scene float manifest at `-t:1` (`tools/after-p3-gi-images-macos.sha256` against `baseline-gi-images-macos.sha256`): **2 of 18 images change**, `photon-parity` and `photon-parity-slo`.

**Cost of D2-A (T052, §5).** `timing_sc007.sh build-base build 5`, interleaved series, macOS (`tools/timing-p3-macos.txt`): `perf-occlusion-cache` 5.09 / 5.03 s, `perf-indirect-photon` 5.87 / 5.79 s, `perf-photon-pass` 4.12 / 4.47 s: all within spread (and `perf-indirect-photon` does not read the photon map under the interpreter, GitHub #173, so it cannot show the cost). **On a render that does read it** (`tools/perf-photon-lookup.rib`: the photon pass, then a 2560 x 1920 raytrace with 2 x 2 samples and one `photonmap()` lookup per hit, 4 threads; `tools/timing-p3-lookup-macos.txt`): **2.78 s with the memo, 22.78 s without: +720 %, beyond the spread** (spread 2.2 and 4.0 s; at 640 x 480: 0.26 s against 1.52 s, +480 %). **Linux** (`tools/timing-p3-lookup-linux.txt`, the same scene at format x3, 5 interleaved series): **3.24 s with the memo, 22.57 s without: +597 %, beyond the spread** (spread 1.7 and 2.4 s; the baseline is over the 3 s floor there). The memo made a lookup near a previous one nearly free; the pure kd-tree search does the full 24-photon search every time. This is the cost the maintainer asked to see; **it is reported, not optimised away** (accepted by the maintainer, 2026-11-16; the follow-up is GitHub #178) (R6 option C, precomputed irradiance at the photons, is the later spec; the raise of the factor to make the baseline pass the 3 s floor would only widen the ratio).

### Checkpoint 2 (Phase 2), 2026-11-16

PAUSE 2a and 2b answered (maintainer): the D7 patch, the two photon references of each platform and the canonical order of 38 bytes (P, N, C, theta, phi; `flags` and the padding excluded; `data-model.md` and `tasks.md` corrected from "42"). The four references were copied into `tests/gi_split/reference/{macos,linux}/files.txt` and `tests/preview/reference/{macos,linux}/datascene.txt` (`rerendered-references.md`: approved). Re-run after the copy:
- macOS: `GiFiles_Unchanged` and `Preview_DataSceneUnchanged` pass (3 of 3 with their fixture); the everyday suite **1370 of 1370 pass** (1 skipped as in Phase 0; `Registry_IntegratorImageIdentity` passed); `ctest -L architecture` 36 of 36, the allowlist unchanged at 168 lines, no include edge added (`photonMapAccess.h` is still Shading's; Hiders include no Texturing header).
- Linux: the two references pass; the everyday suite 1371 tests, **1 failure: `TextureTile_RenderByteIdentical`**, the known Linux failure that was red in the starting tree and in spec 032's baseline; `ctest -L architecture` 36 of 36.

**Issue evidence (§6).**

| issue | test(s) | macOS | Linux |
|---|---|---|---|
| #165 `CMap::reset()` `delete` for `new[]` | `GiDet_PhotonClaim` (8 threads, one claim, `numItems` exact), the 30-run forms; the ASan double-free frame `map.h:156` of T015 | pass | pass |
| #164 the photon pass crashes with several threads | the 16 crash series (8 scenes, default threads and `-t:4`, 30 runs: `photon-crash-p2-<platform>.txt`), `GiDet_PhotonThreads4x30` | 0 crashes in 480 runs | 0 crashes in 480 runs |
| #163 the photon map differs from run to run | `GiDet_PhotonRepeat(Reuse)`, `GiDet_PhotonThreads`, the 30-run forms, the 16 series (one md5 each), `GiKeys_PhotonsThreads` with `flags`, `Registry_IntegratorImageIdentity` 30 of 30 | pass | pass |
| #160 a map filled in memory cannot be queried | `GiDet_PhotonInMemoryLookup`, `_PhotonRoundTrip` | Phase 3 (red until the memo is removed) | Phase 3 |
| #161 | `GiRemote_LoopbackCommit` | Phase 5 | Phase 5 |

The photon-map account is in "Phase 2 results" below. The Phase 2 files are staged by `git add src/ri/core/map.h src/ri/texture/photonStore.h src/ri/texture/photonStore.cpp src/libshader/shading/photonMapAccess.h src/libshader/shading/rendererServices.h src/ri/render/photonMap.h src/ri/hiders/photon.cpp tests/gi_determinism tests/gi_split tests/preview tests/visual specs/040-gi-determinism`. No `Closes` line is written.

### Phase 2 results (T037 to T047), 2026-11-16, before PAUSE 2b

Source changes (staged): `src/ri/core/map.h` (`delete[]` in `reset()`, #165; `new T[n]()` in `read()`, both growth paths of `store()` and `balance()`'s `finalitems`); `src/ri/texture/photonStore.{h,cpp}` (`storePhotonForFrame()` with the test, the reset, the mark and the store in one critical section; `reset()` zeroes `maxPower`; `balance()` sorts the photons and their keys into the canonical order (key, then the 38 bytes of P, N, C, theta, phi: the plan's "42" counted `flags`, which balance() assigns and which is not part of a photon's identity) before the kd-tree balance and stores the empty map's dummy with `theta = phi = 0`; `write()` rebuilds every item in a zeroed buffer and copies it back); `src/libshader/shading/photonMapAccess.h` (`IPhotonStore` is `storePhotonForFrame` and `writePhotons`: `resetPhotons`, `balancePhotons`, `isModifying`, `setModifying` and also `storePhoton`, whose only caller was replaced, are gone); `src/ri/render/photonMap.h`; `src/ri/hiders/photon.cpp` (the global and the caustic map: `if (map->storePhotonForFrame(...)) balanceList.push(map);`); `src/libshader/shading/rendererServices.h` (`kRendererServicesVersion` **2.6.0**, the history entry above 2.3.0; 2.4.0 and 2.5.0 are 033's and 057's, to be re-read at merge preparation; no `CRendererServices` member changed). Tests: the D7 patch applied to `tests/gi_split/test_gi_keys.cpp`; the Phase 2 tests of `tests/gi_determinism` are defined unconditionally (`GiDet_PhotonClaim`, `_PhotonFileBytes`, `_PhotonRepeat`, `_PhotonRepeatReuse`, `_PhotonThreads` and the three `gi_determinism_long` forms), `GiDet_PhotonLookupPure`, `_PhotonInMemoryLookup` and `_PhotonRoundTrip` stay in the red block; `determinism-scenes-phase2.patch` applied (the five subdiv and motion photon entries left the guard).

| item | macOS | Linux (GCC 13, `-Werror`) |
|---|---|---|
| build | clean | clean |
| `GiDet_PhotonClaim`, `_PhotonFileBytes`, `_PhotonRepeat`, `_PhotonRepeatReuse`, `_PhotonThreads`, `GiKeys_Photons`, `GiKeys_PhotonsThreads` (now comparing `flags`; 953 photons identical at -t:1 and -t:4) | 7 of 7 pass | 7 of 7 pass |
| the 30-run forms (`gi_determinism_long`): 30 fresh, 30 reuse, 30 at -t:4 | each one md5, 0 failed | the same |
| `Registry_IntegratorImageIdentity --repeat until-fail:30` | 30 of 30 pass (the starting tree passed about 1 in 6) | 30 of 30 pass |
| the 16 crash series of T014 (8 scenes x default threads and -t:4, 30 runs, `photon-crash-p2-<platform>.txt`) | **0 crashes, 1 distinct map in every series**, numItems as on the starting tree (46273 / 8459 / 22408 / 22317) | **0 crashes, 1 distinct map in every series** |
| `DeterminismSweep_gi-(subdiv|motion-subdiv)` (the five entries that left the guard) | 5 of 5 pass | 5 of 5 pass |
| everyday suite | 1370 tests: **2 failures, both references that change on purpose** (below); everything else passes (1 skipped) | the GI set: the same two |
| `ctest -L architecture` | 36 of 36; allowlist 168 lines | 36 of 36 |
| GI test set (95 tests): visual tests | none exceeds its threshold: no image drift in this phase | same |

The photon maps themselves, old against new (`tools/gpm_multiset_check.py`; the starting tree's `-t:1` map of `photon-parity` against the new tree's): numItems 46273 and the trailer (bound, matrices, `maxPower`) identical, the multiset of the defined fields (P, N, C, theta, phi) of the 46273 photons **identical**, 33798 photons at another index and 6647 with another `flags` value at the same index (the canonical order and the kd-tree built on it), item 0 and every padding byte zero; `subdiv-loop-photon` (176 photons) is byte-for-byte the same order.

**#163, #164, #165.** #163: 30 fresh runs of `identity-photon.rib` give one file, 30 reuse runs give the same file, and the 16 series above give one map each (the starting tree: 2 and 28 distinct maps in the 30-run forms, every completed map different in the crash series). #164: 0 crashes in 16 series of 30 at both thread settings on both platforms (the starting tree: 1 to 11 of 30 crashed in every series); the account of T015 (an unguarded check-then-act at `photon.cpp:633-638` that reset one map from several threads: a double free of `items` and an overflow of the `keys` array after `reset()` zeroed `maxItems`) is what `storePhotonForFrame()` removes: the test, the reset and the first store are one critical section, so one thread resets and no store runs between. #165: `CMap::reset()` frees with `delete[]`; the ASan "double-free map.h:156" frame of T015 is gone with the unguarded resets, and `GiDet_PhotonClaim` (8 threads storing into one store, one claim, `numItems` exact) plus the 30-run forms are the tests that would catch it again. No `Closes` line.

**References that change on purpose (REFERENCE-APPROVAL, PAUSE 2b)** (recorded into `build-logs/` with the tests' own `--record` through `gi-split-capture-references-scratch`, twice per platform with identical results; nothing copied into `tests/`; proposals in `tools/proposed-p2-*`):
1. `tests/gi_split/reference/{macos,linux}/files.txt` (`GiFiles_Unchanged`): 12 changed lines per platform, all of them the six `field <name> sha256` lines of `[photon-parity.gpm]` (P, N, flags, C, theta, phi: hashes of the whole column in item order); the header, `numItems`, `maxItems`, the first eight item lines (the root of the balanced tree: the same photons in the same places) and the `[gi-files.icache]` SHA-256 are unchanged (`tools/proposed-p2-files-<platform>.diff`, `tools/files_multiset_check.py`; the check on the full maps is the paragraph above).
2. **Not in the task text:** `tests/preview/reference/{macos,linux}/datascene.txt` (`Preview_DataSceneUnchanged`, the data viewer's scene) also changes: 585 of 1067 lines per platform, every one a photon-map point-hash line (`points count=46273 hashVerts hashCols` five times, and the per-tile `points n=.. hashP hashC` lines): hashes of the same points in the new item order. Nothing else in the file changes (counts, bounds, cache lines). The test fails on this tree until the reference is re-recorded. The loopback references (`loopback-cache.bin`, `loopback-pointcloud.bin`) re-record byte-identical, as the plan expects.

### Phase 2 before PAUSE 2a (T035, T036), 2026-11-16

T035: master is still `278b0394` (`git log 278b0394..master` empty); #160, #161, #163, #164 and #165 are open with no new comment; `kRendererServicesVersion` is `{2, 3, 0}` on master and in the 033, 048 and 057 worktrees (none has edited it): 2.6.0 stays the free number, to be re-read at T039a. T036: `tools/test-edit-patches/gi-keys-photons-threads-flags.patch` (D7: `dropFlags()` and its two calls removed, the header paragraph reworded; the label `gi_split_threads` stays). The removed `IPhotonStore` members (`resetPhotons`, `balancePhotons`, `isModifying`, `setModifying`) are named only by `src/libshader/shading/photonMapAccess.h`, `src/ri/render/photonMap.h` and `src/ri/hiders/photon.cpp`: no test or other file names them. `CPhotonStore::modifying` (a public data member) stays: `TextureOnly_GiStores`, `GiFiles_Unchanged`'s writer and the Phase 1 tests set it before `write()`.

### Checkpoint 1 (Phase 1), 2026-11-15

macOS, option OFF (`build` configured back, rebuilt): `ctest -N -R 'GiDet_|GiKeys_IrradianceRecordsThreads|GiRemote_LoopbackCommit|DeterminismSweep_gi-'` lists **0** tests (the red block defines none); everyday suite `-LE 'determinism-sweep|gi_determinism_long' -j4`: **1365 tests, 1364 passed, 1 failed**: `Registry_IntegratorImageIdentity` (#163, F3: it flaps; re-run alone with `--repeat until-pass:8` it passed on the first try; no threshold or test changed). The pass set is Phase 0's (1363 passed, 1 skipped) plus `DeterminismHarness_FilesDiffer` and `_FilesSame`. `ctest -L architecture`: 36 passed. Expected-image-drift list: empty (no source file changed).

Linux (one chain through the remote cpujob at -j2, GCC 13 `-Werror`, JIT on; `phase1_linux_chain.sh`): the option-OFF build is clean; `DeterminismHarness_*` 10 of 10 pass (FilesDiffer, FilesSame included); `ctest -L architecture` 36 of 36; the option-OFF tree defines none of the red tests (`-N` lists 0). With the option ON, the red targets build or fail as on macOS: `test_gi_photon_store`, `test_gi_photon_runs`, `test_gi_miss`, `gi_quality` build; `test_gi_photon_claim` (`storePhotonForFrame`), `test_gi_cache_store` (`irradianceBatch.h`) and `test_gi_remote_commit` (`addPending`) fail with exactly those errors. The red tests on Linux: of 14, red = FileBytes, LookupPure, InMemoryLookup and RoundTrip (SegFault), Repeat, RepeatReuse, Threads, MissIndependence_{rslo,slo}, IndirectRayCount_rslo; green = GiKeys_IrradianceRecordsThreads, RepeatedPoint_{rslo,slo}, IndirectRayCount_slo (the same four findings as macOS). The 34 sweep entries: **21 red, 13 green** (`build-logs/040-l-p1-sweep.log`): the three photon entries that were flaky greens on macOS (`gi-motion-subdiv-translate-photon`, `gi-subdiv-facevarying-seam-photon`, `gi-subdiv-new-tags-photon`) and `gi-photon-then-indirect-reyes` are red on Linux, `gi-subdiv-loop-photon` is green there; the cached, occlusion-cache-off and indirectdiffuse-JIT entries are red as on macOS. The Linux timing and the Linux suite are not part of this checkpoint (T034 lists neither).

### Checkpoint 0 (Phase 0)

(see §0; the expected-image-drift list is empty: no code changed)

## 4. Images and sweeps

`tools/baseline-gi-images-macos.sha256` (18 float images, two identical runs). The sweep is not run in Phase 0.


### T094: the one re-render (Phase 7), recorded first; nothing is copied into `examples/rib/tests/references/` or `tests/` before PAUSE 7b

Rendered by the finished tree (`build`, macOS) at the **default thread count**, with the `numthreads 1` line removed in a scratch copy of the two occlusion scenes (`tools/phase7_render_refs.sh`: each scene twice into `build-logs/040-refs/<scene>.run{1,2}/`, every render its own cpujob). **Every one of the 8 images is byte-identical between its two runs**, and each `-slo` image is byte-identical to its `.rslo` image (`gi_quality` 0.0000). The Linux host rendered the same 8 scenes twice (`build-logs/040-refs-linux/`): stable too. Metric: the `MaxBlockAvgDiff` the visual test prints (`gi_quality`; it reproduces the drift list's figures).

| test(s) | reference (`examples/rib/tests/references/`) | reason | old reference against the starting tree's image | **new image against the old reference** | Linux new image against the macOS one |
|---|---|---|---|---|---|
| `Visual_sphere-occlusion-reyes`, `-slo` | `sphere-occlusion-reyes.tif` | M2's record set and D1-A's draws (the cached occlusion image) | 12.36 | 30.72 (macOS and Linux) | 3.13 |
| `Visual_photon-parity`, `-slo` | `photon-parity.tif` | D2-A (the photon lookup memo removed) and the canonical photon order | 0.00 macOS, 0.92 Linux | 7.06 | 0.03 |
| `Visual_photon-solar0-alone`, `-slo` | `photon-solar0-alone.tif` | as above | 0.00 macOS, 0.12 Linux | 11.09 | 0.02 |
| `Visual_photon-solar0-after-distant`, `-slo` | `photon-solar0-after-distant.tif` | as above | 0.00 macOS, 1.52 Linux | 8.77 | 3.42 |

References whose images do not change (diff to the existing reference equal before and after the spec, so none is re-recorded): `sphere-photonmap-reyes` (5.97), `sphere-indirectdiffuse-reyes` (0.00: #173), `sphere-gather-reyes`, `sphere-ray-self-hit-*` (0.00), the four `subdiv-*-photon` (0.44 to 1.88 against their raytrace twins).

Text and binary references (each recorded twice per platform into `build-logs/040-refs/` and `build-logs/040-refs-linux/`, identical): `tests/gi_split/reference/{macos,linux}/files.txt` (lines 26-27: 52016 to 96528 bytes), `loopback-cache.bin` (48828 to 33248 bytes), `tests/preview/reference/{macos,linux}/datascene.txt` (22 lines on macOS: the cache's triangle and disk counts, 5580 to 6180 and 93 to 103), `tests/shading_contract/replay/{macos,linux}.txt` (the origin of the rays of the occlusion and indirectdiffuse requests: 384 of 788 lines on macOS), and spec 039's `tests/visual/golden/occlusion-sobol_cache.tif` (macOS) and `golden/linux/occlusion-sobol_cache.tif` (Linux; per-platform bit-exact; macOS new against old 85.39 on 48 x 48).

## 5. Timing (FR-023)

Baseline: §0.5 (T010). Phase 3 (D2-A, the memo removed): see Checkpoint 3 above: the three standard scenes are within spread on both platforms; a render that reads the photon map through `photonmap()` costs +720 % on macOS (2.78 s to 22.78 s, `tools/perf-photon-lookup.rib`) and +597 % on Linux (3.24 s to 22.57 s). Phase 7 compares against the baseline. The maintainer accepted D2-A's cost as measured (2026-11-16); the follow-up, option C (precomputed irradiance at the photons, R6), is GitHub #178 (P1 enhancement).


### T097 on Linux (`tools/timing-after-linux.txt`, `tools/timing-twoframe-linux.txt`, GCC 13, the same scenes and scales, 4 threads, the photon pass at 1)

| scene | starting tree | finished tree | difference | verdict |
|---|---|---|---|---|
| `perf-occlusion-cache` | 9.14 s, spread 1.70 | 8.03 s, 0.14 | -1.11 s (-12.1 %) | within spread |
| `perf-indirect-photon` | 13.81 s, 0.21 | 13.90 s, 0.21 | +0.09 s (+0.7 %) | within spread |
| `perf-photon-pass` | 7.81 s, 0.56 | 7.90 s, 1.22 | +0.09 s (+1.2 %) | within spread (macOS's +21 % is not reproduced on Linux) |
| two-frame idiom (format x6; frame 1 alone 2.30 s to 2.03 s) | 4.72 s, 0.09 | 7.64 s, 0.09 | **+2.92 s (+61.9 %), beyond spread** | frame 2 about 2.4 s before, about 5.6 s now |

**Maintainer decision (2026-11-17): the macOS `perf-photon-pass` +21 % (+0.71 s, beyond a spread of 0.45 s) is accepted and not optimised.** Likely cause: Phase 2's canonical sort of the photons in `balance()` (a `memcmp` over 38 bytes per comparison) plus the claim inside the store's lock.

### Where the two-frame scene's frame 2 time goes (maintainer request, before PAUSE 7; temporary printed counters and timestamps in `CIrradianceCache` and `irradianceLookup`, removed again; `build-logs/040-p7-breakdown/`)

The scene is `tools/perf-occlusion-twoframe.rib` at 3840 x 2880, ShadingRate 1/3, 4 threads (the numbers are thread-summed milliseconds, the counters are exact; the atomic counters distort the wall time, so each figure is from the run that carried only its own instruments: `run1.out`, `run3.out`):

| | frame 1 (writes the cache) | frame 2 (reads it) |
|---|---|---|
| lookups (`irradianceLookup` calls) | 1,879,414 | 1,879,822 |
| misses (sampler runs, records made) | 11,824 (11,206 committed) | 434 |
| records examined per lookup (front test and weight) | 1.04 | **125** (235,391,318 in all, 217,921,802 accept calls) |
| `lookupCached` | 81 ms | **5,418 ms** |
| the sampler | 24 ms | 1 ms |
| `clampAgainstFixed`, `addPending`, batch add | 0.2 + 1.8 + 0.8 ms | 1.7 ms, 0, 0 |
| `commitPending` (dedupe + sort 1.0, clamp 3.8, place 1.0 ms for 11,206 records) | 7.3 ms | 0 |
| file write / file read / fixed-index build | 21.5 ms / 0 / 0 | 0 / 15.0 ms / 1.7 ms |

**Frame 2's time is the lookups over the fixed part, nothing else**: the commit, the file read and write, the index and the sampler together are under 70 ms of about 5.5 s. A lookup in frame 2 examines 125 records because the cache it reads is **8.4 times denser than the starting tree's**: the same scene at the same scale writes **11,206 records under M2 and 1,328 on the starting tree** (`icache_dump.py`: `build-logs/040-p7-breakdown/{run3,base}/perf-occlusion-two.icache`). That is M2's batch scope (research R2): a lookup sees the fixed part and its own `shade()` call's records only, so every one of the many small REYES grids of a fine shading rate makes its own records where the starting tree's sequential tree shared them across grids. Cost per examined record is unchanged (the front test, the weight and one accept hash; the accept call is part of the pinned contract: once per record that passes the front test). So frame 2 costs about 2 to 2.4 times frame 1 here because it examines about 8 times the records, and the starting tree's frame 1 paid its smaller share of the same work while it built its tree. **Decision (maintainer, 2026-11-17): the frame-2 cost is accepted for spec 040; the cache-density follow-up is filed as GitHub #179 (P1) with this measurement and the three directions; the two-frame idiom is not promoted (`RIB_GUIDE.md` notes the cost instead: about 2x per later frame at fine shading rates until #179), so T108 is skipped.** Not fixed here (no optimisation): the places to look are coarsening the committed set (a cull of records inside another record's radius at commit), a coarser batch (larger grids or a raytrace chunk), and a cheaper reject before the accept hash; the maintainer decides fix-in-040 or file.

### T097: the overhead of the finished tree (FR-023, SC-007), macOS

`timing_sc007.sh build-base build 5` (`tools/timing-after-macos.txt`), interleaved series against the starting tree, 4 threads (the photon pass at 1), the three scaled scenes of T010 (photon pass emit x75): the spec sets no percentage, a difference beyond the spread goes to the maintainer.

| scene | starting tree (median, spread) | finished tree | difference of medians | verdict |
|---|---|---|---|---|
| `perf-occlusion-cache` (cached occlusion, 7680 x 5760) | 3.78 s, 0.51 | 3.40 s, 0.32 | -0.38 s (-10.0 %) | within spread |
| `perf-indirect-photon` | 4.43 s, 0.55 | 4.11 s, 0.75 | -0.32 s (-7.3 %) | within spread |
| `perf-photon-pass` (one thread) | 3.36 s, 0.39 | 4.07 s, 0.45 | **+0.71 s (+21.0 %)** | **beyond the spread (slower)**: reported, not optimised away. Phase 2's canonical sort of the photons in `balance()` and the claim inside the store's lock are the cost of the photon pass; Phase 5's informal run (T077) showed +9.5 % within a spread of 0.48 s. The lookup's own cost (D2-A) is in section 5's Checkpoint 3 figures |

**The two-frame idiom (D5's alternative)** (`tools/perf-occlusion-twoframe.rib`, frame 1 writes the cache `"w"`, frame 2 reads it `"r"`; `tools/timing-twoframe-macos.txt`, both scenes at format x6 because the x12 scene writes 177 MB images and the disk is tight; the medians are under the script's 3 s floor, the differences are against the stated spreads): frame 1 alone (`perf-occlusion-cache`) 0.87 s on the starting tree and 0.83 s on the finished one (within spread); the two-frame scene 1.79 s on the starting tree and **2.80 s on the finished one (+1.01 s, +56.6 %, beyond a spread of 0.25 s)**. Frame 2's cost is therefore about 0.92 s on the starting tree and about 1.97 s on the finished one: with a full fixed part M2's second frame costs about twice frame 1 here, where the starting tree's second frame cost about as much as its first. This is the number D5 asks for; I did not investigate it (the fixed part's lookups and the kd-tree index of `clampAgainstFixed` are the places to look).

## 6. Issue evidence

(Phase 2 onward; baselines: #164 reproduced under ASan §0.7; #163 reproduced in research.md R1)

| issue | what | test (macOS) | test (Linux) | state |
|---|---|---|---|---|
| #160 | the in-memory photon map has a null lookup root | `GiDet_PhotonInMemoryLookup`, `GiDet_PhotonLookupPure` | pass (closing unit sets and `gi_determinism` 20 of 20) | fixed by the memo's removal (D2-A); the maintainer closes it at merge only if its test passed on both platforms |
| #161 | a `-P:N` client placed received records without the server's error multiplier | `GiRemote_LoopbackCommit`, `GiRemote_LoopbackLookup` (`differ == 0`), the `-P:2` cache sweep (34 of 34) | pass (`GiRemote_LoopbackCommit`, `_LoopbackLookup`, the `-P:2` sweep, 3 of 3 sweeps of 616) | subsumed by the commit; same closing rule |
| #163 | the photon map differs from run to run | `GiDet_PhotonRepeat*`, `GiDet_PhotonThreads*`, `GiDet_PhotonFileBytes`, the photon sweep entries, the 16 crash series; `Registry_IntegratorImageIdentity` 30 of 30 | pass: the closing 30-run series 0 crashes, one md5 per scene; registry 30 of 30 | fixed; same closing rule |
| #164 | the photon pass crashes with several threads | `GiDet_PhotonClaim`, 16 series x 30 runs with 0 crashes (`tools/photon-crash-after-macos.txt`) | pass: 480 runs, 0 crashes (`tools/photon-crash-after-linux.txt`) | fixed; same closing rule |
| #165 | `delete` on `new[]` | `GiDet_PhotonRoundTrip`, the claim test (heap poisoning) | pass | fixed; same closing rule |
| #158, #159, #166, #122 | | | | not touched |
| #170 | the photon pass fails under `-P:N` | | | filed; not fixed here (the photon sweep entries carry no `-P:2` and cite it) |
| #173 | the interpreter's `indirectdiffuse()` traces no ray | `GiDet_IndirectRayCount_slo` passes; `_rslo` is red (red block) | same on Linux | filed; not fixed here (diagnosis: the sampler receives a denormal `samples`) |
| #178 | the photon lookup's cost without the memo (option C) | | | filed by the maintainer; the follow-up |
| #179 | the cache is 8 times as dense; a reused cache costs about 2x per later frame | | | filed by the maintainer with the section 5 measurement |

## 7. Departures and findings

**PAUSE 0 answered (maintainer, 2026-11-15):** F1 is #173. F2 narrows H1 (only the two `photon-parity` sweep entries stay guarded until Phase 3; the five subdiv/motion photon entries leave the guard in Phase 2 with the map-bytes checks). Phase 5 compares against 258 (macOS) / 242 (Linux). F3/F6 are the known #163 and Linux baseline failure. Services 2.6.0 at T039a.

**Findings for the maintainer (nothing here was fixed or filed):**

- **F1 = GitHub #173** (filed by the maintainer after PAUSE 0, P1; out of spec; touches FR-012, T024, T058, Phase 5): `indirectdiffuse()` in the probe scenes traces no ray: `CIrradianceSampler::sample` runs once per shading point with `numSamples == 0`
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

- Phase 1: `gi_recording.h` holds the recording cache (T024 says `gi_detkit.h`), so that `gi_detkit.h` names no renderer type; `GiDet_PhotonClaim` is its own executable (`test_gi_photon_claim`, the same source with `GIDET_CLAIM`) so that the other photon-store modes stay buildable and red by assertion; `raycount` counts through orender's statistics (`Total Traced` is the oracle: it is exact for occlusion), not through a recording sampler; `repeat` compares the committed set and the image within each hider across `-t:1`/`-t:4` (the two hiders do not shade the same points); `GIDET_RECORD_HOOK` (CMake cache variable, default `insertRecord`) selects `insertRecord` or `addPending` in the recording tests and is switched by the task that lands `addPending`; the harness patch also touches the approved `tests/visual/scene_copy.h` and `check_determinism_harness.sh`; one `-fsyntax-only` of the patched harness was run outside `cpujob` by the sub-agent that wrote it (a parse, no object, no test run).

- T015 built three explicit targets (`orender file openrender_shaders`), not the default targets (the test executables are not needed to render); the ASan configure, build and runs went through `cpujob` and ran under the coordinator's scoping message (not a separate ask). `build-asan` (189 MB) is kept. No source file was edited.
- T008: `image_manifest.sh` gained the modes `gi-render` and `gi-hash` (spec 032's modes are unchanged); the float line is put on the first line of the scratch copy as the harness does.
- T014, T011, T013 and T014a run through `tools/phase0_jobs.sh <step>` (one cpujob per series or render, the same script on both platforms); `measure_gi.py` gained the `M040_SEED` variable (T014a). `phase0_asan.sh`, `phase0_asan_run.py` and `phase0_linux_chain.sh` are the ASan and Linux drivers. The first macOS runs of T011 and T013 used two equivalent scratch scripts (`build-logs/`), then were re-run with the `maxerror` variants through `phase0_jobs.sh`.
- T007: the spec's `Sampling_Occlusion*` does not exist; `OcclusionSampler_*`, `OcclusionGolden_*` and `SamplingJit*` stand for it. `gi-test-set.txt` lists regular expressions, 25 patterns, 95 tests.
- T017: the Linux first build used the default 2700 s timeout (it finished in about 11 minutes).
- `tools/base-LastTest-macos.log` and `base-LastTest-linux.log` are force-added (`*.log` is ignored), as spec 032 did.

**Closing departures (Phase 8):** (1) FR-024 and SC-009: `CLAUDE.md` and `DEVNOTES.md` are drafts (`claude-md-draft.md`, `devnotes-draft.md`) that the coordinator applies at merge preparation (M6); nothing in this spec depends on that application. `NEWS.md`, `RIB_GUIDE.md`, `BUGS.md`, `DOMAIN_BOUNDARIES.md` and four user docs were edited directly. (2) The services-version step (H2): `kRendererServicesVersion` is the literal 2.6.0, renumbered to the next free MINOR at merge if specs 033 and 057 have not taken 2.4.0 and 2.5.0. (3) The expected-drift mechanism: named GI visual tests exceeded their thresholds from Phase 3 to Phase 7 and were re-rendered once; no threshold was widened (only lowered, `phase7-thresholds.patch`). (4) Tasks whose evidence differed from the plan: T075 (the rendered `.icache` record set changes, not only the radii: 177 to 359 records), T081 (the `-P:N` client read no file: `CHANNELFLAG_READ`), T108 skipped (the two-frame idiom is not promoted), the Linux quality gate skipped (quality is algorithmic; Linux determinism is covered by the sweep), and T104 and T106 ran on Linux after the macOS closing run (host availability). (5) Approved test or reference edits, each with its PAUSE: 1a (the Phase 1 sources and patches), 2a/2b (the photon references, D7 patch), 4a, 5a (insertrecord-to-addpending, store tests, normals), 6a (loopback, lookup), 7a/7b (pins, default set, thresholds, references), plus the six reference changes of T093a. (6) The Linux closing chain ran after the macOS one (the host was unreachable, then busy): the Phase 8 commit carries both platforms. (7) Open questions that remain: D5 (answered: no prepass spec for now, revisit after #179), the cache density (#179), the photon lookup cost (#178), #173.

## 8. Validation summary (T110)

| SC | statement | tests | macOS | Linux | evidence |
|---|---|---|---|---|---|
| SC-001 | each GI scene bit-identical across `-t:1`, default, `-t:2`, `-t:4`, `-P:2`, in 3 of 3 sweep repetitions, each platform against itself | `DeterminismSweep_gi-*` (34) | 3 of 3 (Phase 7) and the full 616-entry sweep 3 of 3 (closing) | 3 of 3 (Phase 7) and the full 616-entry sweep 3 of 3 (closing) | `build-logs/040-p7-sweep{1,2,3}.log`, `040-final-sweep{1,2,3}.log`, `040-l-p7-sweep*.log` |
| SC-002 | the photon `.gpm` is byte-identical over 30 runs and across `-t:1/2/4/8` and default; `Registry_IntegratorImageIdentity` 30 of 30 | `GiDet_PhotonRepeat*`, `_PhotonThreads*`, `_PhotonRepeat30` forms, the registry repeat | pass; 16 series 0 crashes; registry 30 of 30 | pass: 16 series x 30 runs, 0 crashes, one md5 per scene, identical at the default thread count and `-t:4`; registry 30 of 30; `gi_determinism_long` 3 of 3 | `tools/photon-crash-after-{macos,linux}.txt`, `build-logs/040-final-registry.log`, `build-logs/040-linux-final/040-l-final-registry.log`, `040-final-long.log` |
| SC-003 | a probe varying only the other records (0, 10, 1000) finds the same record, `.rslo` and `.slo` | `GiDet_MissIndependence_{rslo,slo}` | pass | pass (closing `gi_determinism` 20 of 20) | `build-logs/040-p4-unit.log` and later, `040-linux-final/040-l-final-gidet.log` |
| SC-004 | 100 random insertion orders give identical lookups, placement and radii | `GiDet_CommitOrder`, `_CommitDedupe`, `_BatchClamp` | pass | pass (closing `gi_determinism` 20 of 20) | `build-logs/040-p5-unit3.log`, `040-linux-final/040-l-final-gidet.log` |
| SC-005 | a cache and a map written and read back give bit-identical lookups; a map filled in memory can be queried | `GiDet_CacheRoundTrip`, `GiDet_PhotonRoundTrip`, `GiDet_PhotonInMemoryLookup` | pass | pass | as above |
| SC-006 | the removed pins are gone, the kept ones listed, nothing widened or disabled, every re-render on the FR-018 list | `thread-pins.md`, `rerendered-references.md`, `phase7-*.patch` | done | done | those files |
| SC-007 | the overhead is measured for the three scaled scenes on both platforms, with noise, before the spec closes | `timing_sc007.sh` | section 5: -10 %, -7 %, +21 % (accepted) | section 5: -12 %, +0.7 %, +1.2 % | `tools/timing-after-{macos,linux}.txt` |
| SC-008 | `-L architecture` with no allowlist line added; `gi_split`, `shading_parity`, `texture_only`, `state`, `plugin_abi`, `link` and the everyday run pass with no new failure | the labels | all pass (Checkpoint 8) | all pass but the known `TextureTile_RenderByteIdentical` (the starting tree's Linux failure) | `build-logs/040-final-*.log`, `build-logs/040-linux-final/040-l-final-*.log` |
| SC-009 | no `[NEEDS CLARIFICATION]` remains and no requirement depends on a merge | `grep` of `spec.md`, T109 | none found; no merge-dependent text in the drafts | n/a | this section |
