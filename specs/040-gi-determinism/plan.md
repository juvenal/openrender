# Implementation Plan: GI Determinism

**Branch**: `040-gi-determinism` | **Date**: 2026-11-14 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/040-gi-determinism/spec.md` (clarified 2026-11-13)

**Project rules followed**: the `## Spec-kit agent rules` section of `CLAUDE.md`. The worktree copy and the main checkout's copy
are identical, last changed in commit `732ba64d`; the worktree copy was read.

## Summary

Make the photon map and the irradiance cache, and so every GI image and file, a function of the scene, the options and the seed,
at any thread count and (for the irradiance cache) any process count.

**Photon map**
- The map is claimed and reset inside the store's lock, so no store races a reset (FR-003, #164, #165).
- Before `balance()` the photons are sorted into a canonical order by key, with content breaking ties, so the kd-tree layout and
  every `flags` value are fixed (FR-002).
- Every byte of the file is defined, and the reset also clears `maxPower` (FR-004). Research R1 measured #163's cause: two
  defects, neither involving threads.
  - Unassigned bytes are written: item 0 and the padding.
  - A `maxPower` carries over from a map that was read before the pass.
- A photon lookup becomes a pure function of the map and the query. The recommendation is to remove the `PHOTON_LOOKUP_CACHE`
  memo (Decision D2), which also removes #160.

**Irradiance cache: mechanism M2 (research R2)**
- During a frame, a lookup sees two things, and neither depends on timing, threads or processes:
  - the **fixed part**: the records read from the file, read-only for the whole frame;
  - the **records made earlier in its own shading batch**: one REYES grid or one raytrace shading group, in lane order.
- Each miss makes its record from the point's identity alone. The sampler draws by index from a copy of the call's sub-stream
  (FR-011).
- All records made in the frame (or, on a `-P:N` client, received from the servers) are committed once at frame end, in a
  canonical order:
  - dedupe by key, then sort by key;
  - clamp from raw radii, so the result doesn't depend on order;
  - place each record by the existing rule, with its own `K`;
  - put each node's list in canonical order.
- The file layout and the 240-byte wire record are unchanged. The wire carries raw radii and, under Decision D4, each record's
  `maxError` in the 8-byte link slot.
- The accept draws become a pure hash of (call key, record identity) instead of a cursor draw (Decision D1, which changes
  gotcha 37).
- `indirectdiffuse()` with the cache off draws `samples` keyed directions, as `occlusion()` has since spec 039 (FR-012).

**Sweep and references**
- The sweep harness gains byte comparison of map and cache files, float output and a depth display.
- The 18 GI scenes join the sweep with their pins removed.
- The GI references change once (FR-018).

## Technical Context

**Language/Version**: C++20 (`-Werror`), CMake ≥ 3.19; Python 3 for the measurement tools only.

**Primary Dependencies**: none new. `libtiff` (existing) for the harness; `common/sampling.h` (Infra) for the hash.

**Storage**: `.gpm` and `.icache` files, layouts unchanged (contracts/files-and-wire.md); the content becomes canonical.

**Testing**: CTest. New labels `gi_determinism`, and `gi_determinism_long` (opt-in, the 30-run forms). Existing labels: `gi_split`,
`gi_split_threads`, `determinism`, `determinism-sweep`, `visual`, `shading_parity`, `architecture`.

**Target Platform**: macOS (Apple Silicon, the fanless M2, `-j4` at most) and Linux (Ubuntu 24.04, GCC 13 `-Werror`, remote `cpujob`
`-j2`). Each platform is compared with itself only.

**Project Type**: renderer library and CLI (`orender`).

**Performance Goals**: no pass/fail percentage (FR-023). The overhead is measured against the starting tree on three scaled scenes,
in interleaved series, with noise, on both platforms. Baselines are in research R7. On this host, a difference within about 5 % on the
occlusion and indirectdiffuse scenes is noise.

**Constraints**:
- No allowlist line and no new cross-domain edge (FR-020).
- No `CRendererServices` member changes, but 040 **does step `kRendererServicesVersion`** (FR-021; maintainer decision 2026-11-15): `rendererServices.h`'s 2.2.0 history names `IPhotonStore` and `irradianceCalls.h` as part of the versioned boundary, and 040 removes four `IPhotonStore` members and changes `lookupCached`'s signature. Rule: the next free MINOR on master at the time of the edit (033 takes 2.4.0, 057 2.5.0, so probably 2.6.0), the literal number written at the edit, a history entry saying it is breaking within 2.x, re-checked at merge preparation.
- No file format change (FR-022). The wire record keeps its size (FR-022, D4).
- Thresholds are unchanged or tightened, and no test is disabled (FR-018).
- No task depends on the merge (FR-024).

**Scale/Scope**: 18 GI sweep scenes; photon maps of a few 10^6 photons in the scaled scenes (46273 photons per 20000 emitted); caches of 10^3 to 10^5 records.

## Constitution Check

*GATE: Must pass before Phase 0 research. Re-check after Phase 1 design.* Constitution 1.4.0 (`.specify/memory/constitution.md`).

| Principle | Check |
|---|---|
| I Clean code | One function per responsibility: claim, canonical sort, commit, accept hash. `insertRecord`, `clamp` and `insertReceived` are removed with their last callers. Under D2-A the photon memo goes too. |
| II Language | C++20; nothing beyond what the tree already uses. |
| III TDD (non-negotiable) | Every phase starts with failing tests: byte-level photon file, claim, commit order, miss independence, lookup purity, the batch-identity gate, and the GI sweep entries. |
| IV CLI | No new option or token. `maxerror` and the cache modes are unchanged. |
| V Minimal dependencies | None added. |
| VI Platforms | macOS and Linux validated separately. No Windows code path is removed. |
| VII Documentation | `CLAUDE.md` (gotcha 37, GI stores, Recent Changes), `DEVNOTES.md`, the registry history, `RIB_GUIDE.md` (the two-frame cache idiom), and the header of `determinism-scenes.cmake`. |

**Principle VIII — Domain Separation and Modular Boundaries**:

- [x] **Domains touched**:
  - **Texturing**: `texture/irradianceCache.{h,cpp}`, `irradianceStore.h`, `photonStore.{h,cpp}`, `core/map.h` (included only by
    the photon store).
  - **Shading**: `libshader/shading/irradianceCalls.{h,cpp}`, `photonMapAccess.h`, `shading.{h,cpp}` (the batch record list in
    `CShadingState`).
  - **Hiders**: `hiders/photon.cpp`, `hiders/irradianceSampler.cpp`.
  - **Renderer core**: `render/photonMap.h` (forwarders), `render/remoteChannel.cpp`.
  - Tests and documentation.
- [x] **No new cross-domain dependency**: Shading → Texturing through `irradianceStore.h` exists; Hiders → Shading through
  `photonMapAccess.h` exists; Texturing → Infra (`common/giHash.h`, a new header of this spec that wraps `sampling.h`'s hashing and leaves `sampling.h` unedited) is allowed. Texturing still includes no Shading, Hiders or Core
  header (`GiSplit_Boundary`); Hiders include no Texturing header.
- [x] **Every altered interface is specified**: `CIrradianceStore` in contracts/irradiance-store.md, `IPhotonStore` and `CPhotonStore`
  in contracts/photon-store.md, the wire in contracts/files-and-wire.md.
- [x] **No hider-specific special case**: batch scope is defined on a shading batch, whatever hider produced it. The photon claim is
  in the store. The batch is a Shading concept (the points one `shade()` call evaluates); a cross-hider difference in the cache comes from batch size (a REYES grid against a raytrace shading group), not from hider-specific code; the Phase 5 quality gate (T074) reports per hider and the gotcha 37 draft says so (M5).
- [x] **No plug-in interface changes** (shadeop, procedural, display): no plug-in SemVer step. The services interface steps (Constraints).
- [x] **Registered violations not extended**: no Open DB entry covers these files' edges (DB-010 and DB-020 were resolved by spec
  032). No allowlist line is added.

**Gate result: PASS** (no violation; Complexity Tracking is empty).

**Re-check after Phase 1 design: PASS.**
- The contracts add no include.
- `IrradianceAcceptFn` passes a `uint64_t` identity, so Texturing needs only Infra's `giRecordIdentity` (`common/giHash.h`).
- The batch view is a plain Texturing struct that Shading fills.
- The pending set and the commit live in Texturing.

## Design decisions taken by the plan (evidence in research.md)

1. **Irradiance mechanism M2**: the fixed part plus batch scope, with a canonical commit at frame end (R2, R4). Its main risk is image quality at batch boundaries: a lookup no longer sees neighbouring batches' records. Phase 5's
   quality gate measures it. R2's table covers
   the alternatives M1, M3, M4, M5 and M6. M1 is the fallback for any batch kind that fails the Phase 1 gate.
2. **Photon claim inside the store's lock** (R5). Rejected: a compare-and-swap on `modifying` alone, and a reset in the core.
3. **Canonical photon order**: by key, then by the bytes of the defined fields (data-model.md §1).
4. **Defined bytes**: allocations are value-initialised; `write()` rebuilds each item in a zeroed buffer; the reset clears
   `maxPower` and the bound (R1).
5. **Sampler on indexed draws** from a copy of the bound sub-stream (R9). The grid stays where the gradients need it.
6. **Harness**: `--files`, `--float` and `--depth`, plus removal of stale files between configurations (R14).
7. **The photon pass under `-P:N` is not made to work here**: it fails on the starting tree (R3), so FR-001's process clause is
   vacuous (the spec's Assumption). The photon scenes' sweep entries drop `-P:2` and give the reason on the entry.

## Decisions put to the maintainer (all approved as recommended on 2026-11-14; see "Decisions approved" below)

| # | Decision | Options | Recommendation |
|---|---|---|---|
| D1 | Accept draws vs gotcha 37's "the draw order is a rule" (FR-010) | **A**: the accept value is a pure hash of (the call's key copy, the record identity); the cursor is untouched. **B**: keep the cursor draw in traversal order. | **A**. It meets FR-010 as written. B is deterministic under M2 but contradicts FR-010's wording (contracts/draw-rules.md). |
| D2 | The `PHOTON_LOOKUP_CACHE` memo (FR-005) | **A**: remove it. **B**: make it deterministic. **C**: precomputed irradiance at the photons, in a later spec. | **A**. Every photon-reading image changes once, and the cost is measured in Phase 3 against the starting tree. C stays available if A is too slow (R6). |
| D3 | Open defects in the functions 040 edits | #165, #164, #163: fix. #160: fixed as a consequence of D2-A. #161: subsumed by the commit. #158, #159, #166: leave alone. #122: not touched. | As listed (R11). 040 does not edit `jitOcclusionBatch`, where #158 lives and which spec 033 rewrites. The maintainer writes each `Closes #NN` in the merge commit, and only for an issue whose test passed on both platforms. |
| D4 | Wire semantics (FR-022) | **A**: the record's `maxError` goes in the 8-byte link slot, `dP` is raw, and the server sends only its pending records; the size stays 240. **B**: a 248-byte record. **C**: a per-group `maxError` in the framing. | **A**. No file format changes under any option. |
| D5 | What to do if M2's overhead is too high | Document the existing two-frame idiom (frame 2 reads frame 1's cache with `CACHE_READ`); a deterministic prepass (M3) in a later spec. | Document the idiom in 040. Decide on M3 after seeing Phase 7's numbers. |
| D6 | The photon pass fails under `-P:N` (R3) | File a Bug Tracker issue, or fix it in 040. | **File it.** A distributed photon pass is a feature, not determinism. |
| D7 | `GiKeys_PhotonsThreads` (approved 2026-11-11) leaves `flags` out | **A**: tighten it to compare `flags` once the order is canonical. **B**: leave it and add `GiDet_PhotonThreads` (byte-level). | **A**, with approval. B is the fallback if the test must stay as approved. |

## Decisions approved (maintainer, 2026-11-14)

The maintainer approved every decision above as recommended. tasks.md is written to these; nothing below is open.

| # | Approved | What the tasks do with it |
|---|---|---|
| D1 | **A**: an accept draw is a pure hash of (the call's key copy, the record's identity). Gotcha 37's "the draw order is a rule" text changes accordingly. | The new text is written in `claude-md-draft.md`, not in `CLAUDE.md` (Phase 8). `test_irradiance_draw_order.cpp` pins the old rule and is rewritten only after its diff is approved (Phase 4). |
| D2 | **A**: remove the `PHOTON_LOOKUP_CACHE` memo. Every photon-reading image changes once. | Phase 3 removes it and measures the cost against the starting tree. |
| D3 | Fix #163, #164 and #165. #160 is fixed by removing the memo. #161 is subsumed by the canonical commit. Leave #158, #159, #166 and #122 untouched. | Each fixed issue has a named test (Phase 8 tabulates them). The maintainer closes an issue at merge only if its test passed on both platforms. **No task writes a `Closes` line.** |
| D4 | **A**: each record's `maxError` travels in the 8-byte link slot of the 240-byte record; `dP` on the wire is the raw radius; the server sends only its pending records. No file format change. | Phase 6. |
| D5 | If M2's overhead is too high, document the two-frame idiom and decide on a prepass after Phase 7's numbers. | A decision point at Phase 7's pause, not a task that decides it. The `RIB_GUIDE.md` task is conditional on that decision. |
| D6 | The photon-pass failure under `-P:N` is filed as GitHub #170. Spec 040 does not fix it. | The photon sweep entries drop `-P:2` and cite #170 on the entry (Phase 1, contracts/determinism-harness.md). |
| D7 | Tighten `GiKeys_PhotonsThreads` to compare `flags` (approved). | The exact diff is shown at Phase 2's test-approval pause before it is applied. |

Approved with tasks.md (maintainer, 2026-11-14):

1. **Staged image drift is accepted.** Named GI visual tests may stay over threshold from Phase 2 to Phase 6, each listed with its measured diff at every checkpoint, until the one re-render in Phase 7. No threshold moves.
2. **Services version.** If tasks T039/T056 find that 040's changes count as a change of the versioned services interface, 040 takes the next free MINOR **after the versions already on master when 040 merges** (spec 033 takes 2.4.0 and spec 057 2.5.0, so no number is written now). The stop-and-report before any edit at T039/T056 stays.
3. **Red tests toggle `OPENRENDER_040_RED_TESTS` in the main `build` tree**; no scratch tree (spec 032's `build-red032` is not used).
4. **The 30-run long tests have their own ctest label**, `gi_determinism_long`, and are excluded from the everyday run the way the sweep is: the everyday expression is `ctest -LE 'determinism-sweep|gi_determinism_long'`. They run explicitly (`-L '^gi_determinism_long$'`) at the checkpoints that need them. `-L` is a regular expression, so the short set is selected with `-L '^gi_determinism$'`.

Analyze findings fixed (maintainer, 2026-11-15):

- **H1**: the photon scenes' sweep entries compare frame-2 images, which read the map through the `PHOTON_LOOKUP_CACHE` memo until Phase 3 removes it. They stay guarded until Phase 3; Phase 2 verifies the map with `GiDet_PhotonThreads`, `GiDet_PhotonRepeat*` and the `.gpm` byte comparison only. Phase 0 measures the frame-2 image at `-t:4` on the starting tree as evidence (T014a).
- **H2**: 040 steps `kRendererServicesVersion` (Constraints above; T002a, T039a, T062a, T079).
- **H3**: the Phase 7 re-render is recorded into `build-logs/` and copied into `examples/rib/tests/references/` only after a REFERENCE-APPROVAL pause (T094, T094a).
- **M1** `GiDet_PhotonRoundTrip` (Phase 3); **M2** `common/giHash.h`; **M5** the batch is a Shading concept; **M6** `CLAUDE.md` and `DEVNOTES.md` are drafts the coordinator applies at merge preparation; **M7** the whole `determinism-scenes.cmake` unguarding schedule is approved at PAUSE 1a; **M9** `GiDet_RepeatedPoint_*`.

Added by the maintainer with the approval: the `indirectdiffuse` counters of `perf-indirect-photon` reported samples but 0 rays
(research R2). A Phase 0 task explains that before any `indirectdiffuse` count is relied on.

## Phases foreseen (tasks.md details them; each ends with a pause for the maintainer)

0. **Baselines (no code)**:
   - `build-base` at `278b0394`.
   - The float images of the 18 GI scenes, and the GI visual references, at `-t:1` (the FR-018 change list starts here).
   - Repeat the R7 timing.
   - Count records from the written cache for `perf-indirect-photon` (R2: that scene's counters were not usable).
   - 30-run `-t:N` crash counts for `photon-parity`, `photon-parity-slo` and `test_photon_solar0`.
   - One AddressSanitizer run of the `-t:4` photon pass in a separate build directory, to confirm or correct R5's inferred cause of
     #164.
1. **Harness and red tests**:
   - `test_depth_determinism` gains `--files`, `--float` and `--depth`, with self-tests.
   - Add the GI sweep entries (red on the starting tree, recorded as expected red until their phase).
   - `GiKeys_IrradianceRecordsThreads`, the batch-identity gate (R4).
   - The red tests of the `gi_determinism` label.
2. **US1, photon map**:
   - The claim and reset (#164, #165); the reset also clears `maxPower` and the bound.
   - The canonical sort and the defined bytes (#163).
   - The 30-run tests.
   - `GiKeys_PhotonsThreads` per D7.
3. **Photon lookup (D2)**: purity, `searchRadius` as a local, #160; before-and-after timing of the photon-reading scenes.
4. **US4, sampler and accept**: indexed draws, `indirectdiffuse()` on the keyed sampler, the accept rule per D1, and the new gotcha 37
   text.
5. **US2 and US3, the irradiance cache**: the fixed part read-only, the batch list in the shading state, the pending set, the
   canonical commit, `readNode` appending, dedupe.
   - **Image-quality gate (R4)**: each cached GI scene's M2 image against an uncached ground truth must be no worse than the
     starting tree's cached image against the same truth (block average and RMSE). Both numbers go to the maintainer before Phase 7's
     re-render.
6. **`-P:N`**: the server sends its pending raw records, the client receives and commits (#161 subsumed), the wire per D4, and the
   `GiRemote_*` tests extended.
7. **Pins and references**:
   - Remove the pins FR-017 lists. Remove `photon-parity`'s only if its 30-run `-t:N` test is clean.
   - Re-render the references once and list them in `rerendered-references.md`.
   - Tighten thresholds where the margin allows.
   - Measure the overhead (FR-023) on macOS.
8. **Documentation and validation**:
   - `CLAUDE.md`, `DEVNOTES.md`, the registry history, `RIB_GUIDE.md` and the `determinism-scenes.cmake` header.
   - macOS: the full suite and the sweep ×3.
   - Linux: the build, the suite, the sweep ×3 at `-j2`, the overhead, and `Registry_IntegratorImageIdentity` 30 of 30.
   - `-L architecture` on both platforms.

## Project Structure

### Documentation (this feature)

```text
specs/040-gi-determinism/
├── spec.md              # the specification (clarified 2026-11-13)
├── plan.md              # this file
├── research.md          # Phase 0: R1-R14 (the #163 cause, the mechanism, measurements, decisions)
├── data-model.md        # Phase 1: photon, record, fixed part, batch scope, commit, accept, draws, files
├── quickstart.md        # Phase 1: validation scenarios
├── contracts/
│   ├── irradiance-store.md
│   ├── photon-store.md
│   ├── files-and-wire.md
│   ├── draw-rules.md
│   └── determinism-harness.md
├── tools/               # measurement scripts, scratch scenes, plan-stage logs (measure-plan-macos.txt, measure-plan2-macos.txt)
├── checklists/requirements.md
└── tasks.md             # Phase 2 (speckit-tasks; not created here)
```

### Source Code (repository root)

```text
src/ri/core/map.h                              # reset delete[], value-initialised allocations
src/ri/texture/photonStore.{h,cpp}             # claim/reset (maxPower, bound), canonical sort, zeroed write, lookup without memo (D2)
src/ri/texture/irradianceStore.h               # lookupCached(accept, batch view), clampAgainstFixed, addPending
src/ri/texture/irradianceCache.{h,cpp}         # read-only fixed part, pending set, commitPending, receivePending, readNode append
src/libshader/shading/photonMapAccess.h        # IPhotonStore::storePhotonForFrame
src/libshader/shading/irradianceCalls.{h,cpp}  # batch scope, accept hash (D1), addPending
src/libshader/shading/shading.{h,cpp}          # CShadingState's batch record list (reset per shade()); jitOcclusionBatch not edited
src/ri/hiders/photon.cpp                       # storePhotonForFrame
src/ri/hiders/irradianceSampler.cpp            # indexed draws; indirectdiffuse on the keyed sampler
src/ri/render/photonMap.h                      # forwarders
src/ri/render/remoteChannel.cpp                # send pending raw records; receivePending (D4)
tests/visual/test_depth_determinism.cpp, tests/visual/CMakeLists.txt, tests/visual/determinism-scenes.cmake
tests/gi_split/                                # GiKeys_*, GiRemote_* extensions
tests/gi_determinism/                          # new: unit, byte and probe tests (label gi_determinism)
examples/rib/tests/sphere-{occlusion,indirectdiffuse,photonmap}-reyes{,-slo}.rib   # pins removed (FR-017)
```

**Structure Decision**: the existing single-project layout. New tests go in `tests/gi_determinism/`, beside spec 032's
`tests/gi_split/`, with their own `CMakeLists.txt` and the `gi_determinism` label.

## Overlap with concurrent work (recorded; the design is against master `278b0394` only)

- **033 (shading system)**: textual collisions in `shading.{h,cpp}` (040 adds a member to `CShadingState` and does not edit
  `jitOcclusionBatch`) and in `irradianceCalls.cpp` (040 rewrites the body of the cache step). The services version: 040 steps it (see Constraints); 033 and 057 take 2.4.0 and 2.5.0, so 040 takes the next free MINOR at the time of its edit and the coordinator re-checks the number at merge preparation.
- **057 (area lights)**: changes the inputs of the photon key (how lights are enumerated and run). 040's order is by key, whatever the
  keys are. Whichever spec merges second re-runs the photon sweeps and re-renders the photon references it changes.
- **048 (OIIO)**: test lists and documentation only.
- **The bug-fix agent on master**: #163, #164 and #165 may be fixed there first. Before each phase that touches them, re-read master
  and the issues; if a fix has landed, keep this spec's tests and drop the duplicate edit.

## Complexity Tracking

No Constitution Check violation; nothing to justify.
