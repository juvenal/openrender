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

**Cost of D2-A (T052, §5).** `timing_sc007.sh build-base build 5`, interleaved series, macOS (`tools/timing-p3-macos.txt`): `perf-occlusion-cache` 5.09 / 5.03 s, `perf-indirect-photon` 5.87 / 5.79 s, `perf-photon-pass` 4.12 / 4.47 s: all within spread (and `perf-indirect-photon` does not read the photon map under the interpreter, GitHub #173, so it cannot show the cost). **On a render that does read it** (`tools/perf-photon-lookup.rib`: the photon pass, then a 2560 x 1920 raytrace with 2 x 2 samples and one `photonmap()` lookup per hit, 4 threads; `tools/timing-p3-lookup-macos.txt`): **2.78 s with the memo, 22.78 s without: +720 %, beyond the spread** (spread 2.2 and 4.0 s; at 640 x 480: 0.26 s against 1.52 s, +480 %). The memo made a lookup near a previous one nearly free; the pure kd-tree search does the full 24-photon search every time. This is the cost the maintainer asked to see; **it is reported, not optimised away** (accepted by the maintainer, 2026-11-16; the follow-up is GitHub #178) (R6 option C, precomputed irradiance at the photons, is the later spec; the raise of the factor to make the baseline pass the 3 s floor would only widen the ratio).

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

## 5. Timing (FR-023)

Baseline: §0.5 (T010). Phase 3 (D2-A, the memo removed): see Checkpoint 3 above: the three standard scenes are within spread on both platforms; a render that reads the photon map through `photonmap()` costs +720 % on macOS (2.78 s to 22.78 s, `tools/perf-photon-lookup.rib`). Phase 7 compares against the baseline. The maintainer accepted D2-A's cost as measured (2026-11-16); the follow-up, option C (precomputed irradiance at the photons, R6), is GitHub #178 (P1 enhancement).

## 6. Issue evidence

(Phase 2 onward; baselines: #164 reproduced under ASan §0.7; #163 reproduced in research.md R1)

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
