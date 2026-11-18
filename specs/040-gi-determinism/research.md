# Research: GI Determinism (spec 040)

Base: master `278b0394` (this worktree, built in `build/`, Release, macOS 27 on the fanless M2). Every measurement ran through
`~/.claude/bin/cpujob` with the scripts in `tools/`; the raw logs are `tools/measure-plan-macos.txt` (job 1) and
`tools/measure-plan2-macos.txt` (job 2). The spec cites no D-number of its own; the adjacent decisions D1 (the photon hider is a
hider), D26 (Bxdf GI is spec 053) and D28 (every (hider, integrator) cell renders or fails with a diagnostic) are not changed by
anything below. Decisions the spec reserves for the maintainer are marked **Decision Dn (maintainer)** and are not taken here.

---

## R1. The cause of GitHub #163 (FR-006)

**Question.** Why does the photon hider write a different global map from the same scene at one thread?

**Method.** `tools/identity-photon.rib` is the photon frame of `Registry_IntegratorImageIdentity` (`photonFrameText(1, "",
"identity.gpm")`: 2000 photons, `-t:1` pinned). `measure_gi.py repeat` rendered it 30 times as separate processes, in a fresh
directory per run (`fresh`: no map exists when the pass starts) and in one directory (`reuse`: the previous run's map is there and
`getPhotonMap()` reads it before the pass resets it), then again with `MallocPreScribble=1`. The first two distinct files were
compared byte by byte and each differing byte classified by its place in the file: header (34 bytes), counts (8), item `k`
(44 bytes: P 0-11, N 12-23, flags 24-25, pad 26-27, C 28-39, theta 40, phi 41, pad 42-43), trailer (bound 24, matrices 128,
`maxPower` 4). Every run stored 440 photons.

**Findings.**

| Series | md5s in 30 runs | Differing bytes (first two distinct files) |
|---|---|---|
| fresh | 2 (19 + 11; one of them is #163's `54981b3f...`, the other its `7708e265...`) | item 0: all 44 bytes' fields (N 8, C 12, flags 2, pad 4, theta 1, phi 1); trailing padding (bytes 42-43) of 16 items: 32 bytes. **No byte of a real field of a stored photon, no trailer byte.** |
| reuse | 28 (two pairs and 26 singletons) | the same item-0/padding bytes **and** the 4 bytes of `maxPower` |
| fresh, `MallocPreScribble=1` | 2 (15 + 15) | as fresh |
| reuse, `MallocPreScribble=1` | 28 | between two of them: only `maxPower` |

The photon pass of `photon-parity` (3 lights, 46273 photons) at `-t:1` gave 10 different files in 10 fresh runs, differing only in
item 0 and in the trailing padding of 4206 items.

**Cause (established).** Two defects, both independent of threads:

1. **Unassigned bytes are written.** `CMap::balance()` copies the balanced items into `finalitems = new T[numItems + 1]` from index 1;
   `finalitems[0]` is never assigned, and `CMap::write()` writes `numItems + 1` items from index 0. `CPhoton` has 4 padding bytes
   that `CMap::store()` never writes (it assigns the fields of a slot of `new T[]`), and the struct copy may or may not carry them.
   The heap contents there differ from run to run (ASLR and allocator state; `MallocPreScribble` does not make them repeatable, so
   they are not only freshly allocated bytes but reused ones). This alone gives the two-way split of #163.
2. **`maxPower` survives the reset.** When the map file already exists, `getPhotonMap()` reads it, which sets `maxPower` to the
   file's value; that value is already the square root (`write()` stores `sqrtf(maxPower)`). `CPhotonStore::reset()` clears the items
   and the keys but not `maxPower`, so the pass starts from the old map's value, keeps it when it is larger than this frame's
   largest `|C|^2` (it always is: power below 1 has a square root larger than itself), and writes its square root again. Each re-run
   in the same directory therefore writes a different `maxPower`, drifting towards 1. This is a real field, read back by every
   lookup (`searchRadius` is computed from it), so a camera pass that reads a map regenerated over a stale one renders differently.
   The suite's photon fixtures delete the stale map first, which is why the suite only sees cause 1.

Linux was not measured at this stage: both causes are allocator- and file-state-dependent, not platform code, and Linux's 4 of 4
(spec 030) is what glibc's chunk reuse would give for cause 1. The Linux validation of the fix (SC-002) measures it.

**Decision.** FR-004 and a reset that clears `maxPower` and the bound fix #163 (contracts/photon-store.md). The 30-run test of US1
stays as the regression guard in both forms (fresh and reuse). No debugger was needed: the byte classification is the evidence.

**Alternatives considered.** Comparing field by field (what `GiKeys_*` do, OQ-8 of spec 032) hides cause 1 by construction and
would have hidden cause 2 as well; rejected as the test of FR-004 (the spec asks for a byte comparison).

---

## R2. The irradiance cache mechanism (FR-007 to FR-009, FR-013 to FR-015)

**Constraints, from the code.**

- **Lookups happen during the render** (FR-008) and their results are used at once by the shader; a lookup cannot wait for later
  records or be re-run.
- **`-P:N` exchanges records only at frame end** (FR-015). `CRemoteICacheChannel` is a `REMOTECHANNEL_PERFRAME` channel:
  `sendRemoteFrame()`/`recvRemoteFrame()` run once per frame, after the servers have rendered (`remoteChannel.cpp:524-578`). A server
  can never see another server's records during the frame without a new protocol. So whatever a lookup sees must be computable by
  the process that shades the point, from (a) what every process has before the frame starts and (b) what that process makes itself
  in an order that is a function of the scene.
- **Which bucket shades a REYES grid is timing.** `CReyes::shadeGrid()` shades a grid once, under the grid's lock, in whichever bucket's
  thread reaches it first (`reyes.cpp:821-832`); at `-t:1` the first bucket in raster order, at `-t:N` any of the buckets it straddles.
  Any rule keyed on buckets or on a bucket wave is therefore not a function of the scene for the points of a straddling grid.
- **What a shading batch is, is a function of the scene.** A REYES/zbuffer grid is the dicing of an object (deterministic, shaded once).
  A raytrace shading group (`trace.cpp:90-215`) is the rays of one bundle that hit one object, in ray order, chunked by `maxGridSize`;
  the *order* of the groups follows object-pointer hashes (`key = address bits`), but the composition of each group does not. Spec 039
  already keys a batch on this (`batchKey = mix(bucket, object->stableId, depth, chunkIndex)`). Within a batch both backends walk the
  real lanes in order (`IDEXPR` and `jitOcclusionBatch`, `shading.cpp:3684`), skipping tagged lanes the same way.

**Candidates.**

| | Mechanism | FR-008 | FR-015 (`-P:N`) | Cost | Verdict |
|---|---|---|---|---|---|
| M1 | **Frozen fixed part only**: a lookup sees only the records that existed at frame begin (`F`, read from the file); every miss computes a sample; the frame's records are committed canonically at frame end | yes | yes, with no protocol change beyond sending raw records | loses all in-frame reuse | correct but the slowest; kept as the fallback for any batch kind found not to be deterministic |
| M2 | **M1 plus batch scope**: a lookup also sees the records made earlier *in its own shading batch* (creation order, lane order); canonical commit at frame end | yes, given R4 | yes, same as M1 | keeps reuse inside a grid or ray group; loses reuse across batches | **chosen** |
| M3 | Deterministic prepass: a seed pass whose records are made independently of each other (every prepass point records), then the beauty pass reads them frozen | yes | needs the prepass repeated on every server, or a new broadcast of its records through the setup channel (a protocol change) | a second traversal of the frame and a record per prepass point | rejected for 040; offered as a follow-up lever (Decision D5) |
| M4 | Wavefront generations: buckets in fixed waves with a barrier and a canonical commit per wave | no for straddling grids (see above); would need visibility keyed per point, not per bucket | needs a record exchange per wave between processes (protocol change) | idle threads at every barrier | rejected |
| M5 | Per-thread caches merged after the frame | no (the spec says so: lookups happen during the render) | - | - | rejected by FR-008 |
| M6 | A global lock and a canonical order of all lookups, or speculative ordered execution | yes only if every lookup waits for all earlier ones in the order | no | serialises the render | rejected (the spec's own edge case) |

**Decision: M2.** During a frame:

- The fixed part `F` (the file's records) is read-only. Lookups read it without a lock (today's lookups also read without a lock, but
  concurrently with inserts and with `clamp()` rewriting neighbours' radii: a data race this design removes).
- A miss computes its record from the point's identity alone (R9) and appends it to the batch's own list (Shading, in the shading
  state) and to the cache's pending set (under the cache's lock: a record costs a hemisphere of rays, so the lock is not contended).
- At frame end the pending set is committed once: dedupe by key, sort by key, an order-independent clamp computed from raw radii, the
  existing placement rule with each record's own `K`, and node lists in canonical order (data-model.md §5).

How it meets FR-008: the visible set of a lookup is `F` plus the earlier records of its own batch; `F` is the file (a function of the
scene and of the previous frame, itself deterministic), and the batch and its lane order are functions of the scene (R4). No thread,
bucket order or process boundary enters. How it meets FR-015: every server holds the same `F` and shades every point it shades with
the same batch as one process would, so its pixels equal the one-process pixels; it sends its pending (raw) records, and the client's
commit equals the one-process commit because the commit is a function of the set. FR-009: the tree shape, each node's list and each
radius are functions of the committed set; the batch list is summed after the tree, in creation order. FR-013: a point shaded twice
(a grid that two servers shade, a repeated derivative batch) makes the same key with the same bits; the commit keeps one. FR-014: the
written order is the canonical list order and `readNode` appends, so a read gives the written order back.

**Cost, measured as a proxy.** The counters below (`measure_plan2.sh`, stats level 3, one thread, 640 x 480, ShadingRate 1) give the
records today's cache makes, the batches, and the cost with no reuse at all (M1's bound: `maxerror 0`, every point sampled).

| `perf-occlusion-cache` unscaled, `-t:1` | Lookups (points shaded with `occlusion()`) | Records made (samples) | Rays | Points per batch (avg) |
|---|---|---|---|---|
| cache on (`maxerror 0.5`, today) | 26372 | **1472** (one per 17.9 lookups) | 17664 | 19.43 |
| cache off (`maxerror 0`, M1's bound: every lookup samples) | 26372 | 26372 | 316464 | 19.43 |

So the scene has about 26372 / 19.43 = **~1357 shading batches** (the average includes the occluder's batches, which make no
lookup, so the GI batches are fewer). M2's floor is one record per GI batch, about today's 1472; its ceiling is M1's 26372. Today's
cache reuses a record about 18 times and a batch holds about 19 points. The two ratios are similar, but that does not show where
today's reuse happens; M2's real record count is measured in Phase 5, from the written cache, against the starting tree's
count taken the same way in Phase 0. The
render times (0.22 s with the cache, 0.11 s without) are too short to time and say only that this scene's rays are cheap (two
spheres): there the cache's own bookkeeping costs more than its rays, and M1 would not be slower. In a scene with expensive rays
the ratio of records is what matters. The `indirectdiffuse()` counters of `perf-indirect-photon` were not usable: the scene reported
575248 indirectdiffuse samples and 0 indirectdiffuse rays, identically with and without the cache (an inconsistency of the statistics
path noted for Phase 0, not investigated here), so Phase 0 measures that scene by its timing and by a record count from the written
cache instead.

M2 makes at least one record per batch that calls a cached GI function and at most the uncached cost; the implementation measures
it on the scaled scenes (FR-023) against both. If it is too slow, the levers are, in order: (a) the user's two-frame idiom that already
exists (frame 1 writes the cache, frame 2 reads it with `CACHE_READ`, so `F` is full and every batch reuses it), documented in
`RIB_GUIDE.md`; (b) M3 as a later spec (Decision D5).

**Alternatives considered.** See the table. A bucket-scoped variant of M2 was rejected because of straddling grids; an object-scoped
variant (all batches of one object) was rejected because the batches of an object run on several threads at once.

---

## R3. The photon pass under `-P:N` (FR-001's process clause, the spec's Assumption)

**Measured.** The photon pass of `photon-parity` (`perf-photon-pass.rib` without its thread pin) under `-P:2`: 3 of 3 runs failed
(exit 255, 255, SIGSEGV), no map written.
The output of one run (`measure_plan2.sh` section 2):

```
photon-pass-nopin.rib (20): Failed to download file perf-photon-pass.gpm      (twice: one per server)
photon-pass-nopin.rib (26): Invalid net command
photon-pass-nopin.rib (26): Connection broken                                   (twice)
photon-pass-nopin.rib (26): Invalid net command
exit 255
```

The servers fail to fetch the map file (the log does not say more), no photon job is distributed (nothing produces
`NETWORK_PHOTON_BUNDLE`), and the session breaks; a 2 MB map is left behind, written by a process the log does not identify.

`CJob::NETWORK_PHOTON_BUNDLE` is declared (`renderer.h:210`) and nothing produces it: photon jobs are only dispatched locally
(`CRenderer::dispatchPhoton`, `rendererJobs.cpp:150-185`).

**Decision.** The photon pass does not run under `-P:N` on the starting tree, so FR-001's process clause is vacuous (the spec's
Assumption: "Where it does not, FR-001's process-count clause is vacuous"). Spec 040 does not implement a distributed photon pass.
The photon scenes' sweep entries list their configurations without `-P:2`, with this reason on the entry (contracts/
determinism-harness.md). A camera frame that *reads* a photon map under `-P:2` (each server reads the same file) stays in scope.
**Decision D6 (maintainer):** file the `-P:N` photon-pass failure as a new Bug Tracker issue (recommended) rather than fix it in 040.

---

## R4. Is a shading batch a function of the scene? (the premise of M2)

**Evidence.** R2's "constraints" (grid locking; raytrace group composition; lane order in both backends; spec 039's `batchKey`), and
spec 039's sweep: 404 non-GI scenes bit-identical at `-t:1`, the default, `-t:2`, `-t:4`, `-P:2`, which would not survive if a point
were shaded in a different batch with batch-dependent data (derivatives are computed over the batch).

**Risk.** A hider or a shading path where the batch is not stable: the raytracer's chunking at `maxGridSize` is by ray order inside one
bundle (stable); the photon hider never calls the cache; a lazily shaded REYES grid is shaded once; the displacement estimate and the
P-only pass (`shadeGrid(grid, TRUE)`) run displacement, not surface shaders.

**Image quality.** A lookup no longer sees records of neighbouring batches, and batches are small (about 19 points on average in
`perf-occlusion-cache`), so seams at batch boundaries are a real risk that FR-018's single re-render would otherwise accept unseen.
Gate (Phase 5, before the re-render of Phase 7): for every cached GI scene, an uncached ground truth (`maxerror 0`, raised `samples`)
is rendered; the M2 image's error against it must be no worse than the starting tree's cached image's error against the same truth,
by the visual metric (8x8 block average) and by RMSE. Both numbers go to the maintainer with the re-render list. If M2 is worse, the
options are M1 for the failing scenes' batch kind, or D5.

**Decision.** Phase 1 adds the gate before any cache code changes: `GiKeys_IrradianceRecordsThreads` (an extension of spec 032's
`GiKeys_IrradianceRecords`, which records every `(key, record bits)` pair a render makes through `CRecordingCache`) run under M2 at
`-t:1` and `-t:4` must give the same multiset (an in-process recording double cannot observe a `-P:2` server; `-P:2` is covered by
the GI sweep entries and by the extended `GiRemote_*` tests); and the GI sweep entries. If a batch kind fails it, that kind falls back to M1
(no batch scope), which stays deterministic.

---

## R5. The photon reset race and the crash (FR-003, #164, #165)

**Measured.** The photon pass of `photon-parity` at 4 threads, 30 separate runs: **9 crashed** (SIGSEGV x4, SIGTRAP x4, SIGABRT x1:
30%, against "3 of 30" in spec 012 and "1 of 3" in spec 032); the 21 that finished all stored 46273 photons (the same as at one thread)
and gave 21 different files, differing in every field (store order, hence kd-tree layout and `flags`). At one thread, 10 of 10 finished.

**Analysis (an inference: it fits the code and the crash rate, but no backtrace was taken).** `photon.cpp:633-651` runs `isModifying()`/`setModifying()`/`resetPhotons()` outside the store's lock. A second thread
can pass the test before the first sets the flag and call `CMap::reset()`, which frees `items` (with `delete`, not `delete[]`: #165)
while a third thread is inside `storePhoton()` writing into it under the lock: a use-after-free and, when it does not crash, lost
photons. The 21 survivors lost none, so the race window is the very start of the pass, as expected.

**Decision.** `storePhotonForFrame()` claims and resets inside the store's critical section (contracts/photon-store.md); the photon
hider pushes the map on its balance list when the call returns true. A compare-and-swap on `modifying` alone was rejected: the winner
could still reset after a loser stored. A reset in the core before the threads start was rejected: maps are bound lazily per object
(`CRenderer::render`), and the core would need to know which maps a photon frame writes. `delete[]` in `CMap::reset()` (#165). The 30-run
`-t:4` test is US1's, and FR-017's condition for removing the `photon-parity` pins. Phase 0 adds one AddressSanitizer run of the
`-t:4` photon pass (in a separate build directory; ThreadSanitizer as the alternative, `OPENRENDER_TSAN=ON`) to confirm or correct
the inference before Phase 2 relies on it.

---

## R6. `PHOTON_LOOKUP_CACHE` (FR-005) — **Decision D2 (maintainer)**

**Facts.** `CPhotonStore::lookup` first asks `probe()` for earlier estimates stored near the query (radius `0.2 x` the search radius)
and returns their weighted mean; on a miss it searches the kd-tree and `insert()`s the result under the store's lock. The root of that
octree exists only for a map read from a file (`photonStore.cpp:104-130`); a map filled in memory has none, and `insert()` then
dereferences `NULL` (#160). A lookup's result therefore depends on which nearby points were looked up before, by which thread.
`searchRadius` is a member every lookup writes.

**Options.**

- **A. Remove the memo** (recommended): `lookup` = kd-tree search, sum, normalisation; `searchRadius` local. Every photon-reading
  image changes once (estimates become exact per point instead of interpolated from memo entries); the cost is one kd-tree search per
  lookup instead of an octree probe for most of them. #160 cannot be reached. Until it is removed, the photon scenes' frame-2 images depend on lookup order (research of 2026-11-15, T014a measures it on frame-2-only copies at `-t:4`), so no image comparison of a photon scene across thread counts can pass before Phase 3. To measure in the implementation: `perf-indirect-photon`
  (one photon lookup per indirectdiffuse ray hit) and `sphere-photonmap-reyes`, before and after, interleaved.
- **B. A deterministic memo**: the memo would need M2's whole apparatus (frozen part, batch scope, canonical commit) for a structure
  whose entries are cheap to recompute; not worth it.
- **C. Precomputed irradiance at the photons** (Christensen 1999): after `balance()`, compute the estimate at a subset of photons,
  deterministic and parallel, and answer lookups from the nearest one. Faster than A, but a new estimator that changes results
  differently; a later spec if A's cost is too high.

---

## R7. Overhead baselines (FR-023, SC-007)

The three scaled scenes are `tools/perf-occlusion-cache.rib` (spec 032's, occlusion through a written cache), `tools/perf-indirect-photon.rib`
(spec 032's, a photon pass then indirectdiffuse reading the map) and `tools/perf-photon-pass.rib` (new: the photon pass of photon-parity
alone), timed by `tools/timing_sc007.sh` (spec 032's script, copied; interleaved series; the photon pass on one thread because of R5).
Starting tree against itself, 5 series, 4 threads, macOS, the fanless M2:

| Scene (scale) | Median (series A) | Median (series B) | Spread A / B | A - B |
|---|---|---|---|---|
| `perf-occlusion-cache` (Format x12, ShadingRate / 3) | 3.40 s | 3.39 s | 0.11 / 0.17 s | -0.6 %, within spread |
| `perf-indirect-photon` (Format x6, ShadingRate / 3, emit x10) | 4.00 s | 3.91 s | 0.19 / 0.36 s | -2.1 %, within spread |
| `perf-photon-pass` (emit x50, one thread) | 2.21 s | 2.24 s | 0.49 / 0.37 s | +1.4 %, within spread; **under the script's 3 s floor**: the real comparison raises it to emit x75 |

(`tools/measure-plan2-macos.txt`, section 3.) So a difference of about 0.2 s (5 %) on the first two scenes and 0.5 s on the photon
pass is noise on this host at this time; Phase 0 repeats the series right before the comparison, because the fanless host's wall time
follows its thermal state.

The finished tree is compared against these in the same way (series interleaved with `build-base`), on macOS and on Linux; the spec sets
no pass/fail percentage, and a difference beyond the spread is reported to the maintainer with the numbers.

---

## R8. The accept draws (FR-010) — **Decision D1 (maintainer)**

See contracts/draw-rules.md. **A** (recommended): the accept value is a pure hash of the call's key copy and the record's identity; the
cursor is not consumed. It meets FR-010 as written and changes gotcha 37's "the draw order is a rule". **B**: keep the cursor draw in
traversal order; deterministic under M2 but contrary to FR-010's wording. Both need no new include edge (Texturing passes the record
identity through the callback; Shading hashes with `common/sampling.h`).

## R9. The sampler's draws (FR-011, FR-012)

`drawBound2D(pair, sample)` reads `drawCursor` (the sub-stream) and ignores the cursor's dimension (`shading.h:574`), so indexed draws
are independent of how many draws came before. The cached path keeps the `nt x np` grid (the gradients need it) with cell `k`'s
offsets from pair 0 sample `k` and its origin jitter from pair 1 sample `k`; `indirectdiffuse()` with the cache off takes `samples`
directions as `occlusion()` has since spec 039 (T076c). The sampler takes a copy of the bound sub-stream at its start, so a nested
`bindDraws` during an environment lookup cannot change its draws. Both backends reach the one sampler through `irradianceLookup`.
Effect: every cached GI image and every uncached `indirectdiffuse()` image changes once (FR-018).

## R10. Pins and references (FR-017, FR-018)

From spec 039's `thread-pins.md` and the scenes: `numthreads 1` in `sphere-occlusion-reyes`, `sphere-indirectdiffuse-reyes`,
`sphere-photonmap-reyes` (each with `-slo`) are removed (FR-017 mandates it; the edits are to test inputs the spec names). The
`photon-parity`/`photon-parity-slo` `-t:1` arguments and `test_photon_solar0`'s `-t:1` are removed only after 30 runs of each at the
default thread count finish without a crash (R5 shows 30% today). `sampling-*.rib.in` keep theirs. The references that change
(every cached GI image, every photon-reading image under D2-A, the photon maps' consumers) are re-rendered once, in one step, listed with
their reasons in `rerendered-references.md` (spec 039's FR-034 format); no threshold widens, any threshold whose measured margin allows
is tightened and listed.

## R11. Defects in the same functions (decisions for the maintainer: D3)

| Issue | Code 040 edits | Recommendation |
|---|---|---|
| #165 `delete` in `CMap::reset()` | yes (FR-003 names it) | **fix** (one line) |
| #164 photon SIGSEGV with threads | yes (the claim/reset) | **fix**, verified by the 30-run `-t:4` test; `Closes #164` only if that test passes on both platforms |
| #163 | yes | **fix** (R1) |
| #160 null lookup-cache root | yes under D2-A (the memo goes) | **fixed as a consequence**; the in-memory lookup test guards it |
| #161 client placement without `K` | yes (`insertReceived` replaced) | **subsumed** by the canonical commit (FR-015) |
| #158 JIT occlusion leaves output unwritten with no cache | `jitOcclusionBatch` (`shading.cpp:3640-3720`) is **not** edited by this design (the batch list lives in the shading state; the lookup goes through `irradianceLookup`) | **leave** (also keeps 040 out of the function spec 033 rewrites) |
| #159 getTexture3d kind | no | leave |
| #166 Catmull-Rom DOF noise | no | leave |
| #122 bake3d SIGSEGV | no | must not be touched (spec) |

## R12. Concurrent specs (overlaps recorded; design is against master only)

- **033 (shading-system backends)**: `libshader/shading/shading.{h,cpp}` (040 adds the batch record list to `CShadingState` and
  resets it in `shade()`; it does not edit `jitOcclusionBatch`), `irradianceCalls.cpp` (040 rewrites the cache step's body),
  `rendererServices.h` (040 changes no `CRendererServices` member but steps `kRendererServicesVersion`, maintainer decision 2026-11-15: the history names `IPhotonStore` and `irradianceCalls.h` as part of the versioned boundary; the next free MINOR on master at the time of the edit, probably 2.6.0 after 033's 2.4.0 and 057's 2.5.0, with a history entry saying it is breaking within 2.x). Merge order
  matters only for textual conflicts in `shading.{h,cpp}` and `irradianceCalls.cpp`.
- **057 (area-light geometry)**: changes how lights run and are enumerated; the photon identity includes the light index (`photon.cpp`
  `cellKey(mix(0xF07011, i), ...)`), so 057 can change which photons exist. 040's canonical order is by key, whatever the keys are; the
  second spec to merge re-runs the photon sweeps and re-renders the photon references it changes.
- **048 (OIIO)**: `tests/visual/CMakeLists.txt`, `determinism-scenes.cmake`, `CLAUDE.md`, `DEVNOTES.md`, the registry: textual only.

## R13. File and wire formats (FR-022) — **Decision D4 (maintainer)**

No file format changes (contracts/files-and-wire.md). The wire record keeps its 240 bytes; its `dP` becomes the raw radius, the
server sends only its pending records, and the 8-byte link slot (a pointer value today, meaningless to the receiver) carries the
record's `maxError`. Recommended: the slot. Alternatives: a 248-byte record, or a per-group `maxError` in the framing.

## R14. Determinism harness (FR-016)

`test_depth_determinism` decodes every output as a TIFF, so `.gpm`/`.icache` cannot be listed today; it gains `--files`, `--float` and
`--depth` (contracts/determinism-harness.md), and removes the listed files between configurations so no configuration reads a stale map
(R1's second cause would otherwise hide in the sweep).
