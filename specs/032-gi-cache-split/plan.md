# Implementation Plan: GI Cache Split

**Branch**: `032-gi-cache-split` | **Date**: 2026-11-07 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/032-gi-cache-split/spec.md`, with the maintainer's nine answers of the
2026-11-07 clarify session (Q1 to Q9), which this plan treats as decisions.

**Status of this plan**: **approved 2026-11-07**. The maintainer accepted the recommended answer of every open question
(OQ-1 to OQ-8); see "Maintainer answers" at the end. The spec was corrected to match ("Plan-stage corrections
(2026-11-07)" in its Clarifications).

## Summary

Each GI structure is split in two along Principle VIII's table. Passive storage and lookup go to Texturing; sampling
and emission stay in Hiders and reach the storage only through Shading-owned interfaces. The renderer core composes
the pieces.

- **Irradiance cache.**
  - `CIrradianceCache` moves to `src/ri/texture/` as a passive store behind a header-only abstract
    `CIrradianceStore` (a `CTexture3d`). Its lookup takes a draw callback that it calls at the exact point and count
    of today's accept draw.
  - One Shading function, `libshader::irradianceLookup`, serves the interpreter and the JIT. It does the point-based
    branch, the store lookup and, on a miss, asks a Shading-owned `IIrradianceSampler`.
  - The `CHider` base installs that sampler; the core installs it as a process default for contexts no hider owns.
    Shading then calls the store's virtual `insertRecord` under the old lock discipline.
  - `irradianceDispatch.cpp`, its stub and `irradianceSampleAccept` are deleted (Q1 A).
- **Photon map.**
  - The storage becomes the Texturing `CPhotonStore`.
  - The hiders reach it through Shading's `photonMapAccess.h` (`IPhotonEstimator`, `IPhotonStore`,
    `CPhotonMapRefTarget`, `photonMapOf`).
  - A header-only Core `CPhotonMap` composes store, handle target, `IPhotonLookup` and the viewer's adapter, keeping
    the name and every existing test's view of it (Q4 A).
- **3D family.** `CTexture3d` loses the viewer base, the shading-context lookup (reshaped to `lookupPointBased` with
  a per-lookup options struct) and its friends; `CBrickMapGeometry` goes (Q6).
- **Data viewer.** It owns one adapter per document type.
- **Remote channels.** They use narrow public read and insert operations.
- **Keys.** Irradiance records carry a 64-bit key in memory and on the wire. Photons carry one in a side array that
  `CMap::balance` permutes. No file changes (Q3 B).

Result:
- DB-010 and DB-020 are resolved.
- DB-025 loses 11 lines; DB-017 loses its irradiance pair.
- 25 allowlist lines are deleted (344 -> 319) and none added.
- Every GI image is byte-identical on both platforms (Q5 A).

## Technical Context

**Language/Version**: C++20 (`-Werror`), CMake >= 3.19.

**Primary Dependencies**: none new. The libraries touched are `libRIBCommon` (only `core/map.h`'s hook, a header),
`libshader_shading`, `libRIBRender`, `libRIBVector` and `libribpreview` (unchanged sources).

**Storage**: no persistent change. `.pm` and irradiance-cache files are byte-identical (FR-019). New recorded test
references live under `tests/gi_split/reference/` and `tests/preview/reference/`; the timing data lives under
`specs/032-gi-cache-split/tools/`.

**Testing**:
- ctest, with a new label `gi_split` in the everyday run.
- New tests T-A to T-G and fixture F1 (`contracts/tests.md`).
- The GI test set and the determinism sweep against Phase 1 manifests.

**Target Platform**: macOS (Apple Silicon, Apple Clang, vcpkg, 13.3 target) and Linux (Ubuntu 24.04, GCC 13,
`-Werror`, x86_64 without FMA, over ssh with the remote `cpujob -j2`).

**Project Type**: renderer library and CLI.

**Domains touched**:
- Texturing: the new stores; `CTexture3d`, `pointCloud`, `brickmap` and `pointHierarchy` edited.
- Shading: `irradianceCalls`, `irradianceSampler.h`, `photonMapAccess.h`; `shading.{h,cpp}`, `giFunctions.h` and
  `rendererServices.h` edited.
- Hiders: `irradianceSampler.cpp` and `hiderIrradianceSampler.h`; `photon.{h,cpp}` and `hider.cpp` edited; `irradiance*`, `photonMap.*` deleted.
- Infra: `core/map.h`'s `permuted` hook.
- Core: `render/photonMap.h`; `rendererFiles`, `remoteChannel`, `renderer`, `rendererContext`, `rendererStatics`,
  `rendererServicesImpl.h` and the default-sampler installation edited.
- Consumer: four viewer adapters and `dataLoad.cpp`.
- Graphics State: a comment only.

**Not edited**: `geometry/surface.cpp` (spec 031's, Q2 B); `test_photon_solar0.cpp`; the bodies of
`CPointCloud::lookup`/`store` and of the point hierarchy's traversal (FR-030); `ptcapi.*`; the brick-map paging;
`domain-rules.txt`; `domain-map.txt`; the constitution.

**Performance Goals**: within the run-to-run noise of the starting tree on three scaled GI scratch scenes (SC-007,
research R14). The added costs:
- one indirect call per record that passes the front test, replacing a direct call;
- two or three virtual calls per cache lookup;
- up to four virtual calls per stored photon;
- a virtual hook per `CMap::balance`.

**Constraints**:
- Byte-identical images per platform at the same pins; no threshold, reference or replay changed.
- The draw stream exactly as today (FR-006).
- `libshader_shading` gains no undefined symbol.
- The allowlist and `ribvector-core-residual.txt` only shrink.
- Builds and tests only through `cpujob`.

**Scale/Scope**:
- New sources: about 12 (2 Texturing stores, 1 Texturing header, 3 Shading files, 1 Hiders sampler pair, 1 Core
  header, 4 viewer adapter pairs).
- Deleted:
  - 6 moved out: `irradiance.{h,cpp}`, `irradianceDispatch.cpp`, `irradianceDispatchStub.cpp`, `photonMap.{h,cpp}`;
  - 3 dead files: `radiance.{h,cpp}`, `harmonics.h` (OQ-4).
- Edited: about 30.
- New tests: 9, plus 6 approved mechanical edits.

**Measurements at the plan stage**: research M. Phase 1 re-measures on a build.

## Constitution Check

*GATE: passes before Phase 0. Re-checked after Phase 1 design: PASS.*

**I to VII** (clean code, C++20, TDD, CLI, dependencies, platforms, docs):
- No new dependency.
- No platform-specific code.
- TDD: tests T-A to T-G are written red first, where a red state exists. T-A, T-B and T-F are characterizations
  recorded green on the base tree. All of them go through the test-approval pause before any implementation.
- The documentation edits (`CLAUDE.md` gotchas 24, 25 and 28, `DEVNOTES.md`, `DOMAIN_BOUNDARIES.md`) are drafted at
  implement and shown for approval (FR-025). This stage edits none of them, and the spec-kit agent-context update was
  not run for that reason.

**Principle VIII — Domain Separation and Modular Boundaries**:

- [x] Domains touched are named (Technical Context).
- [x] No new cross-domain dependency outside the allowed table, and none on the renderer core from inside a domain.
  Every new edge is allowed:
  - Shading -> Texturing (`irradianceStore.h`, `texture3d.h`) and Shading -> Graphics State (`stateHandles.h`);
  - Hiders -> Shading (`irradianceSampler.h`, `photonMapAccess.h`) and Hiders -> Geometry (`object.h`);
  - Core -> Texturing, Shading, Hiders, Consumer (`render/photonMap.h`, the default sampler);
  - Consumer -> Texturing and Core (the adapters, `dataLoad.cpp`);
  - Texturing -> Infra only.
  - Edges removed: Hiders -> Texturing (DB-010), Hiders and Texturing -> Consumer (DB-025), and Hiders -> Core for the
    world bound and matrices (part of DB-011).
- [x] Every added or altered cross-domain dependency has its interface specified: `contracts/irradiance-cache.md`,
  `photon-map.md`, `texture3d-family.md`, `viewer-adapters.md`, `remote-channels.md`, `gi-keys.md`,
  `renderer-services-version.md`.
- [x] No hider-specific special case: one sampler for every hider (Q1), installed by the `CHider` base.
- [x] No plug-in interface (shadeop, procedural, display) changes. Shading's internal boundary steps to 2.2.0 (MINOR,
  additions only, R12).
- [x] Registered violations are not extended; the reductions are listed in `contracts/allowlist-end-state.md`. One
  edge stays deliberately: Core `CPhotonMap` -> Consumer `CPhotonMapView`, allowed by `Core -> *`, needed by an
  existing test (OQ-2).

**Post-design re-check**: PASS. Two premises of the spec could not hold as written; neither blocks the design:
- US5's thread-count invariance for irradiance records (OQ-3);
- US4 AS1's sender/receiver lookup equality, which the base placement rule may break (OQ-5).

## Project Structure

### Documentation (this feature)

```text
specs/032-gi-cache-split/
├── spec.md
├── plan.md              # this file
├── research.md          # Phase 0: M (measurements), R1-R15 (decisions)
├── data-model.md        # Phase 1
├── quickstart.md        # Phase 1: validation run guide
├── contracts/
│   ├── irradiance-cache.md          # store, shared lookup, sampler, draw order
│   ├── photon-map.md                # Texturing store, Shading access, Core bridge
│   ├── texture3d-family.md          # CTexture3d reshape, friends, read interfaces
│   ├── viewer-adapters.md           # the data viewer's adapters and document classes
│   ├── remote-channels.md           # wire layout, public operations, loopback checks
│   ├── gi-keys.md                   # record and photon keys, CMap hook
│   ├── renderer-services-version.md # 2.2.0 entry, collision rule
│   ├── allowlist-end-state.md       # allowlist, registry, metrics, residual, build lists
│   └── tests.md                     # existing-test surface, GI set, TEST-APPROVAL inventory
├── checklists/requirements.md
└── tools/                           # Phase 1 onward: manifests, recordings' scripts, perf scenes, timing script
```

### Source code touched

```text
src/ri/texture/
├── irradianceStore.h        NEW   CIrradianceStore, CIrradianceRecord, IrradianceDrawFn, irradianceStoreOf
├── irradianceCache.{h,cpp}  NEW   (from hiders/irradiance.*) passive store + read interface
├── photonStore.{h,cpp}      NEW   (from hiders/photonMap.*) CPhoton, CPhotonStore + key side array
├── texture3d.h              EDIT  no CDataView, no CShadingContext, lookupPointBased, public channel ops, no friend
├── pointCloud.{h,cpp}       EDIT  viewer members out, friend out, points()/pointData()
├── brickmap.{h,cpp}         EDIT  viewer members out, CBrickMapGeometry out, visitVoxels()
└── pointHierarchy.{h,cpp}   EDIT  Options out, lookupPointBased
src/ri/core/map.h            EDIT  CMap::permuted hook (Infra)
src/libshader/shading/
├── irradianceCalls.{h,cpp}  NEW   libshader::irradianceLookup (interpreter + JIT)
├── irradianceSampler.h      NEW   IIrradianceSampler, IrradianceSample
├── photonMapAccess.h        NEW   IPhotonEstimator, IPhotonStore, CPhotonMapRefTarget, photonMapOf, photonKey
├── giFunctions.h            EDIT  IDEXPR calls irradianceLookup
├── shading.{h,cpp}          EDIT  sampler slots, boundDraws(), JIT call site
├── rendererServices.h       EDIT  2.2.0 entry, getCache wording
└── CMakeLists.txt           EDIT  irradianceCalls.cpp
src/ri/hiders/
├── irradianceSampler.cpp, hiderIrradianceSampler.h  NEW  (from irradianceDispatch.cpp) the sampler; the header is not named irradianceSampler.h, Shading owns that basename
├── hider.cpp                EDIT  installs the sampler
├── photon.{h,cpp}           EDIT  photonMapOf/IPhotonStore, keys, writePhotons(CRenderer::world)
├── irradiance.{h,cpp}, irradianceDispatch.cpp, photonMap.{h,cpp}   DELETED
├── radiance.{h,cpp}, harmonics.h                                   DELETED (dead, compiled by no target; OQ-4)
src/ri/vector/irradianceDispatchStub.cpp                            DELETED
src/ri/render/
├── photonMap.h              NEW   Core CPhotonMap, header-only
├── rendererFiles.cpp        EDIT  constructor args, no pointHierarchyMaxSolidAngle
├── remoteChannel.{h,cpp}    EDIT  public operations
├── renderer.cpp / rendererShadingServices.cpp  EDIT  default sampler at RiBegin/RiEnd
└── rendererContext.cpp, rendererStatics.cpp, rendererServicesImpl.h, renderer.h, rendererContext.h  EDIT  includes
src/ri/dataviewer/
├── {photonMap,irradianceCache,pointCloud,brickMap}View.{h,cpp}  NEW
└── dataLoad.cpp             EDIT  document classes, constructor args
src/ri/CMakeLists.txt        EDIT  ri_sources, ribVector lists
tests/architecture/          EDIT  allowlist.txt, metrics-baseline.txt, ribvector-core-residual.txt; NEW T-G script
tests/gi_split/              NEW   T-A, T-A2, T-E, T-F, fixture F1, references
tests/preview/               NEW   T-B + reference; EDIT M3, M4
tests/unit/texture_only/     EDIT  M6; NEW T-C, T-D
tests/unit/photon_map/       EDIT  M1, M2
```

**Structure decision**: the existing layout. Every new source sits in a directory `domain-map.txt` already
classifies, so no map or rule changes.

## Design

The design is in the contracts; research R1 to R15 hold the decisions and their alternatives. In one paragraph each:

- **Irradiance cache** (`contracts/irradiance-cache.md`). The store's `lookupCached` is today's traversal, with
  `irradianceSampleAccept` replaced by a function-pointer draw callback at the same line. Shading's
  `irradianceLookup`:
  - runs the point-based branch first;
  - on a miss transforms the tangents and asks the sampler (the context's, else the process default, else it warns
    once);
  - builds the record and key and calls `insertRecord`, then transforms the envdir.

  `insertRecord` holds the mutex for allocation, `clamp()` and placement. The gradients are computed by the sampler,
  outside the lock.
- **Photon map** (`contracts/photon-map.md`). The Texturing `CPhotonStore` takes the world matrices by argument. The
  Shading interfaces have names distinct from the store's methods (the `-Woverloaded-virtual` trap). The header-only
  Core `CPhotonMap` forwards inline; `writePhotons(CRenderer::world)` writes the matrices `CRenderer::fromWorld/toWorld`
  are copied from.
- **3D family** (`contracts/texture3d-family.md`). `lookupPointBased(..., const CPointBasedLookup &)` replaces the
  context-taking virtual. The structures keep their storage-level `typeName`/`bound`; the viewer members leave.
- **Viewer** (`contracts/viewer-adapters.md`). Adapters plus document classes, shaped by the existing tests: `CPhotonMap`
  is itself drawable, and `dynamic_cast<CPointCloud *>(doc.view())` keeps working. The statics stay class-static in
  the adapters.
- **Channels** (`contracts/remote-channels.md`). The same traversal and bytes; the receiver inserts through
  `insertReceived` in arrival order.
- **Keys** (`contracts/gi-keys.md`). The record key comes from the bound sub-stream, the photon key from the point
  key and the bounce. `CMap::permuted` reorders the side array.
- **Services** (`contracts/renderer-services-version.md`). 2.2.0, additions only. `getCache` keeps its type (OQ-1); Core's `getCache` `dynamic_cast`s the entry it finds and answers `nullptr` for a name registered as another 3D kind (analyze M3).

## Phases and order of work

Every phase ends with `ctest -L architecture` (agent rules) and the phase's own checks; the implement agent pauses at
every phase boundary.

| # | Phase | Content | Gate |
|---|---|---|---|
| 1 | Setup and baselines (both platforms) | Copy `image_manifest.sh` and `linux_sync.sh` from spec 029's `tools/`. Base build; suite log; image manifest; sweep manifest; archcheck report; frozen `build-base`; perf scratch scenes and `timing_sc007.sh`, with the base series. Fixture F1. **Record T-A, T-B and T-F on the unmodified tree, before any `src/` edit, each twice** (the two recordings must agree: indeterminate bytes, OQ-8), and stage them. Re-recording stays allowed until Phase 3 begins, in case the masks change at the approval pause. Re-measure research M | the recordings staged on macOS and Linux |
| 2 | Tests | Write T-A2, T-C, T-D, T-E and T-G (red where a red state exists) and prepare M1 to M6 as exact patches (not applied: M5 and M6a land in Phase 4, M1 to M4 and M6b in Phase 5, when their targets exist). **TEST-APPROVAL pauses** 2a (new tests) and 2b (include edits); Phase 1 has its own pause 1a before T-A, T-B and T-F are recorded | maintainer approval |
| 3 | 3D family ties and the three Texturing-derived adapters (US3, US4 partial) | `CTexture3d` without `CDataView`/friend (it keeps the 6-argument `lookup` and `CPointHierarchy::Options` until Phase 4: Shading calls that virtual); `CBrickMapGeometry` out; the point-cloud channel through the public operations. **The adapters and document classes of the point cloud, the brick map and the irradiance cache**: the cache, still in `src/ri/hiders/` here, loses `draw`/`keyDown`/`drawDiscs`, which move to `CIrradianceCacheView` (Consumer -> Hiders is allowed), and gains `forEachNode`/`forEachRecord`. It gets no temporary `CDataView` base (that would be an un-allowlisted include edge). Allowlist: the DB-025 lines of `texture3d.h`, `brickmap.cpp`, `pointCloud.cpp` and `hiders/irradiance.cpp` (9 of the 11), and the header count | `-L architecture`, T-B, the preview tests, `plugin_abi`, T-A (cloud), the GI set |
| 4 | Irradiance cache split (US1, US2) | `irradianceStore.h`, `irradianceCache.*` in Texturing; `irradianceSampler.h`, `irradianceCalls.*`; `CIrradianceSampler` and its installation; the adapter repointed at the Texturing class; `CTexture3d` without `CShadingContext`, `lookupPointBased`, `CPointHierarchy::Options` out; the six historical `CRenderer` comments under `src/ri/texture/` reworded; the cache channel through the public operations; the dispatch pair, stub and `irradianceSampleAccept` deleted; the dead `radiance.{h,cpp}` and `harmonics.h` deleted (OQ-4); build lists; residual row; M5. `nm -u` of the sampler object (no Texturing symbol) and of `libshader_shading` (no new import). Allowlist: DB-010 (8, the section goes) and the DB-011 lines of `irradiance.cpp` and `irradianceDispatch.cpp` (4), and the headers | `-L architecture`, T-C (cache), T-D, T-A, T-A2, `-L link`, the shading contract, the GI set byte-identical |
| 5 | Photon map split (US1, US2, US3) | the dead `CPhotonRay` and `#include "ray.h"` removed from the Hiders `photonMap.h` first (T078a); `photonStore.*`; `photonMapAccess.h`; Core `photonMap.h`; the photon-map adapter; `photon.cpp` and the sampler through `photonMapOf`; `writePhotons`; `hiders/photonMap.*` deleted. Allowlist: DB-025 `photonMap.h`/`photonMap.cpp` (2) and DB-011 `photonMap.cpp` (2), and the headers | `-L architecture`, T-C (photons), T-F, the #64 tests, `PhotonSolar0_*`, T-B, the GI set |
| 6 | Boundary close-out (US1, US4) | Metrics lowered to the reported values; T-G green; the registry drafts (shown for approval). Every allowlist line has already gone in the phase that made it stale (archcheck fails on a STALE line, so no phase may end with one) | `-L architecture`, T-G, T-A2 (re-run) |
| 7 | Keys (US5) | `CIrradianceRecord::key`, `irradianceRecordKey`, `boundDraws()`; the `CMap::permuted` hook, `CPhotonStore::keys`, `photonKey`, `tracePhoton`'s argument; T-A's after-state comparison strips the key | T-E, T-A, T-F, the GI set |
| 8 | Guardrails, docs, Linux, perf, sweep | `CLAUDE.md`/`DEVNOTES.md`/`DOMAIN_BOUNDARIES.md` drafts for approval (FR-025); the 2.2.0 entry; the no-JIT build and the TSan build compile; SC-007 timing on both platforms; the Linux set (FR-024); **last: the determinism sweep on both platforms** | SC-001 to SC-010 |

Phase 3 takes the irradiance cache's viewer half with it. Dropping `CDataView` from `CTexture3d` would otherwise break
the still-unsplit cache: its `draw()` calls the inherited `drawDisks`/`drawPoints`/`chunkSize`, and `dataLoad.cpp`
stores it as a `CDataView *`. With that, the 3D reshape is independent of the store split, which Phase 4 then does
alone.

Keys come last, so the wire change is isolated in one phase and T-A's comparison changes in exactly one place.

## Overlap with the concurrent specs (030, 031, 056)

This table is derived from the three spec.md files. Their plan.md files were unfilled templates when it was written,
so the table must be re-checked against their plans before tasks. No task of this spec depends on another spec's
merge; whichever merges second resolves the shared text below.

| Shared item | 030 | 031 | 056 | How 032 avoids a semantic collision |
|---|---|---|---|---|
| `src/ri/hiders/photon.{h,cpp}` | deletes the `hiderFlags` write (`preDisplaySetup`) | ray-side interface (DB-017 lines of `photon.cpp`) | no | 032 edits only the store path (`:95-103`, `:630-662`), `tracePhoton`'s signature and its four call sites, and `balanceList`'s type. Disjoint lines; 030 must not touch the store path (spec) |
| `src/ri/hiders/hider.{h,cpp}` | registration (the runtime stays) | the producer and forwarding entry | no | 032 adds one line to `CHider::CHider` (`setIrradianceSampler`) and an include |
| `src/libshader/shading/shading.{h,cpp}` | no | retires `shade(CSurface *)` forwarding | `shade()` tail, `gatherSample`, bundles | 032 edits the JIT occlusion/indirectdiffuse body (`:3500-3560`) and adds sampler slots and `boundDraws()` to `shading.h`. Different functions |
| `giFunctions.h` | no | no | no (`shaderFunctions.h`, `executeMisc.cpp`) | the `IDEXPR` call line only |
| `rendererServices.h` / `kRendererServicesVersion` | no step found | may step (`IShadingProducer`) | states no change | collision rule: the second to merge renumbers to the next MINOR (`renderer-services-version.md`) |
| `rendererServicesImpl.h` | `:61` (`hiderFlags` for the hook) | no | no | 032 changes only the include (`render/photonMap.h`) |
| `rendererContext.cpp`, `rendererStatics.cpp`, `renderer.cpp` | hcshader removal, hider includes | no | no | 032 changes only the photon map's include line, the default sampler install (`beginRenderer`/`endRenderer`), and nothing in the hider selection |
| `src/ri/CMakeLists.txt` `ri_sources`, `ribVector_sources` | removes `hcshader.cpp` | removes three Dispatch/Stub pairs | no | 032 removes and adds its own entries; the second to merge resolves the text |
| `tests/architecture/allowlist.txt` | DB-011 `photon.cpp`/`zbuffer.cpp`/`reyes.cpp` lines (`:127`, `:176`, `:211`, `:263`) | DB-006, DB-017, DB-004, DB-011 `reyes`/`stochastic`, **DB-011 `photonMap.cpp` `:137-138`** (its P-Q3 (a)), DB-025 `surface.cpp` (`:356`) | adds none | 032 deletes `:100-107`, `:111-112`, `:125-126`, `:137-138`, `:357-367`. **`:137-138` is deleted by both 031 and 032**: the second to merge drops its duplicate edit, and if 032 lands first, 031's `FrameParams`-based read in `photonMap.cpp` disappears with the file. Every other deletion is disjoint. Abutting hunks (`:125-127`, `:356-357`) conflict textually; the second to merge keeps every deletion and corrects the headers |
| DB-025 | no | removes `surface.cpp` | no | second to merge moves it to Resolved (Q2 B) |
| DB-017 | no | deletes 3 pairs and 78 lines, narrows to the irradiance pair | no | **same rule**: the second of 031 and 032 to merge moves DB-017 to Resolved. The spec does not say this; the plan adds it |
| DB-011 history, `metrics-baseline.txt` Hiders row and TOTAL | lowers | lowers | no | each records its own reduction; the second to merge re-measures |
| `ribvector-core-residual.txt` `rendererStatics.cpp` row | may shorten (hcshader) | Geometry names; deletes three stub rows and `rendererDeclarations.cpp` | no | 032 deletes the irradiance stub row and rewrites only the Hiders part of the cell |
| `geometry/surface.cpp` | no | edits (`debug.h` include, `DEBUG_*` blocks) | no | **032 does not edit `surface.cpp`**. 031's spec says the file is "touched by both"; for 032 that is not so after Q2 B |
| DB-020 | none left (029 removed the light friends) | no | no | 032 resolves it |
| `csgSolid.cpp` | no | edits | edits | not touched by 032 |

Spec 056 must not add a Hiders -> Texturing edge (spec). Nothing in 032 constrains 056's volume step.

## Issues near this work (not absorbed; research R15)

- **#122**: the bodies of `CPointCloud::lookup`/`store` are unchanged, but layout and vtable move. A `bake3d` crash is
  re-run in isolation; there is no crash-rate criterion, no fix and no TSan run (Q8).
- **#131**: T-A's point-cloud loopback records the base behaviour, whatever `numItems` says.
- **#104**: the GI entries stay name-keyed (recorded in `photon-map.md`).
- **#145, #146, #97**: untouched.
- **#15**: looks stale.

## TEST-APPROVAL inventory

The full inventory is in `contracts/tests.md`:
- new tests T-A, T-A2, T-B, T-C, T-D, T-E, T-F and T-G, and fixture F1;
- the mechanical edits M1 to M4 (the `photonMap.h` include path in four existing tests);
- M5 (the new T-A's one adaptation);
- M6 (the `texture_only` CMake addition).

The **masked `socketpair()` loopback test (T-A)** and the **mechanical include edits (M1 to M4)** are the approval
points the brief names. Nothing is applied before approval except the base recordings (T-A, T-B, T-F), which must
precede any `src/` edit and are approved with this plan.

## Risks

| Risk | Mitigation |
|---|---|
| The draw sequence moves (an extra or missing accept draw) and images change | The callback is called at the old line; T-D asserts the sequence; the GI set is byte-compared; the trace replay is unchanged |
| A Shading call into the store becomes non-virtual and adds an import | Only virtuals and inline accessors are called; Phase 4 runs the `nm -u` check; `-L link` and `check_shading_self_contained` |
| GCC 13 `-Woverloaded-virtual` (in `-Wall` since GCC 13) under `-Werror` | Distinct names for every new virtual (`lookupPointBased`, `lookupCached`, `insertRecord`, `estimate`, `storePhoton` ...); the Linux build runs at the end of Phases 3, 4 and 5 |
| `CRenderer::world->from` differs from `CRenderer::fromWorld` at the hider's destructor, so the `.pm` bytes change | They are copies (`renderer.cpp:300-303`); T-F compares the bytes; Phase 5 verifies the hider is destroyed before `endFrame` detaches `world` (`renderer.cpp:923`) |
| The header-only `CPhotonMap` pulls Core code into `libRIBVector` beyond `rendererStatics.cpp` | It reads only `CRenderer::fromWorld/toWorld`; `SceneInterface_RibVectorResidual` and `Preview_NoCoreReach` run in Phase 5 |
| `texture3d.cpp`/`photonStore.cpp` do not compile on the narrow `texture_only` path | The staging copy of `map.h` (M6); any further surprise is reported, never fixed by widening the path |
| A `bake3d` crash (#122) in validation | Re-run in isolation; recorded, not judged |
| Textual conflicts with 030 and 031 | The overlap table; the second to merge resolves |

## Open questions for the maintainer (all answered 2026-11-07; see "Maintainer answers")

Each had a recommendation, which the design follows; the maintainer accepted every one.

- **OQ-1 `getCache`'s return type.**
  - Recommended (R4): keep `CTexture3d *` and convert with `irradianceStoreOf()`, as `photonMapOf` does. No test
    double changes.
  - Alternative: return `CIrradianceStore *` (breaking within 2.x, 2.2.0) with eight signature-following test edits.
- **OQ-2 `CPhotonMap` derives from the viewer's `CPhotonMapView`** (Core -> Consumer, allowed by `Core -> *`).
  - Recommended: yes. `test_dataview_chunking` calls `draw()` on a `CPhotonMap` it builds, and `test_data_world_init`
    `dynamic_cast`s `doc.view()` to `CPhotonMap *`.
  - Alternative: no viewer base in Core, which requires non-mechanical edits of both tests (FR-029).
- **OQ-3 US5's Independent Test.**
  - Recommended: correct the spec to "photon `(key, photon)` sets equal across runs and thread counts; irradiance
    `(key, record)` sets equal across repeated `-t:1` runs, and the same sample gives the same key at any thread
    count". Which points miss the cache depends on insertion timing until spec 040.
  - The photon half rests on two more things:
    - the intermittent `CPhotonHider` crash (`DEVNOTES_DETAILS/BUGS.md`), which is why the GI pins exist;
    - the unguarded `modifying` check-then-`resetPhotons()`, which can drop a thread's early photons when two threads
      both reset.

    T-E's cross-thread-count photon comparison is therefore opt-in (label `gi_split_threads`, not in the everyday
    run), and the everyday T-E compares repeated `-t:1` runs only.
- **OQ-4 the dead files `src/ri/hiders/radiance.{h,cpp}` and `harmonics.h`.** No target compiles them; they include
  `texture.h` and call the photon map's 4-argument lookup. FR-002 ("no file under `src/ri/hiders/` may include a
  Texturing header") is literal about files.
  - Recommended: delete them in this spec. They do not compile (`CCache` is undefined), and git keeps the history.
  - Alternative: leave them, and scope T-G's Hiders scan and FR-002's wording to compiled sources.
- **OQ-5 US4 AS1 (receiver lookups equal the sender's).** The base receiver places a record by `dP`, without the
  sender's `/K` (`remoteChannel.cpp:582` "error adjustment!!"), so the two trees can differ in shape and the lookups
  in value. The base tree has no context-free lookup to measure it.
  - Recommended: T-A pins the receiver's full tree (its written file) against the base recording; T-A2 compares
    lookups on the finished tree and reports, not fixes, any difference.
- **OQ-6 DB-017 resolution.** Recommended: the same second-to-merge rule as DB-025 (031 narrows it to the irradiance
  pair, 032 deletes the pair). The spec states this only for DB-025.
- **OQ-7 the 2.2.0 collision** with 031 (or 030) stepping the version. Recommended: whichever merges second renumbers
  its entry to the next MINOR at rebase; no content change.
- **OQ-8 indeterminate bytes in the raw files and the wire.** Some bytes are never assigned. They come from `new T[]`
  and `memcpy`, so they are often zero but not guaranteed.
  - In `.pm` files:
    - `CMap::write` writes `items[0]`, which `balance()` never assigns;
    - `CPhoton` has 2 padding bytes after `flags` and 2 at its tail;
    - the dummy photon of an empty map has unset `theta`/`phi`.
  - On the point-cloud channel's frame: each raw `CPointCloudPoint` has 2 padding bytes after `flags`.

  So FR-019 / US2 AS4's "`.pm` bytes equal master's" is not well defined even on master, and a raw byte comparison
  can flake. Q7 fixed the loopback's masks at two items (`next`, the setup tail).
  - Recommended:
    - T-F compares `.pm` files field by field (header, counts, items 1 to n by field, bounds, matrices, `maxPower`),
      skipping item 0, the padding and the dummy photon's direction. Cache files are fully defined and keep a plain
      SHA-256.
    - T-A also masks the `CPointCloudPoint` padding.
    - Every base recording is taken twice and must agree before it is staged.
  - Alternative: keep the raw comparisons and accept that a flake is possible.

  This extends Q7's mask set, so it needs the maintainer's word.

## Maintainer answers (2026-11-07, plan approved)

Every recommendation was accepted. The design above already follows each answer.

| OQ | Answer |
|---|---|
| OQ-1 | `getCache` keeps returning `CTexture3d *`, converted with `irradianceStoreOf()`. No test double changes |
| OQ-2 | Yes. The core `CPhotonMap` derives from the viewer's `CPhotonMapView` (Core -> Consumer, allowed) |
| OQ-3 | US5's Independent Test is corrected as proposed. Photons: the same (key, photon) set across runs and thread counts. Records: the same set across repeated `-t:1` runs, and the same sample gives the same key at any thread count. The cross-thread photon comparison is opt-in (`gi_split_threads`) |
| OQ-4 | Delete `src/ri/hiders/radiance.{h,cpp}` and `harmonics.h` (confirmed: no CMake list compiles them). Phase 4 deletes them with the dispatch pair. T-G's Hiders scan then covers every file under `src/ri/hiders/` with no exception |
| OQ-5 | Yes. T-A pins the receiver's written tree against the base; T-A2 compares lookups on the finished tree and reports differences without fixing them |
| OQ-6 | DB-017: whichever of 031 and 032 merges second moves it to Resolved, as with DB-025 |
| OQ-7 | If both specs take services version 2.2.0, the second to merge renumbers |
| OQ-8 | Yes. `.pm` files are compared field by field; cache files keep SHA-256. T-A adds the `CPointCloudPoint` padding as a third mask. Every base recording is taken twice and must agree |

**TEST-APPROVAL list.** T-A to T-G, F1 and M1 to M6 are approved at plan level. Each test is still shown at the
implement stage's test pause before it is applied.

**Cross-spec note** (from the coordinator's line-by-line check of 031's approved plan):
- Spec 031, under its option P-Q3 (a), also deletes allowlist lines 137-138 (`photonMap.cpp -> CRenderer::toWorld`,
  `-> fromWorld`), which 032 deletes in Phase 5.
- Whichever spec merges second drops its duplicate edit. If 032 lands first, 031's `FrameParams`-based read in
  `photonMap.cpp` disappears with the file, since `hiders/photonMap.cpp` is deleted and the matrices are injected.
- Every other allowlist deletion of 032 is disjoint from 031's and 030's.
- 031's spec is being corrected to say that 032 does not edit `surface.cpp`.

## Spec corrections found at the plan stage (no requirement changed by this plan)

All of these were applied to spec.md on 2026-11-07 after approval. FR and SC numbering is unchanged.

- US5 Independent Test: OQ-3.
- US4 AS1: OQ-5.
- FR-002's scope against uncompiled files: OQ-4.
- FR-019 / US2 AS4's "`.pm` bytes equal master's": some bytes are indeterminate even on master (OQ-8), so the plan
  reads it as "every defined byte".
- 031's spec says `surface.cpp` is "touched by both". 032 does not touch it (Q2 B, FR-016).
- SC-002's "at most 320": the deleted dispatch file's include line goes in any case, so the target on `e83d005d` is
  exactly **319**. 318 is reachable only if the base already lacks `surface.cpp`'s line.

## Complexity Tracking

| Choice | Why needed | Simpler alternative rejected because |
|---|---|---|
| Core `CPhotonMap` derives from a Consumer adapter | Two existing tests require a drawable, `dynamic_cast`-able `CPhotonMap` (FR-029) | Composition breaks both tests (OQ-2) |
| Process-default sampler beside the `CHider`-installed one | Two contract tests run GI calls on a context no hider owns | A `CHider`-only install fails them; a services member needs the forwarder doubles edited (R3) |
| `CMap::permuted` virtual hook in Infra | FR-017 requires `balance()` to permute the photon keys, and only `balance()` knows the permutation | Recomputing the order after `balance()` is impossible without a per-item identity inside `CPhoton` (Q3 forbids it) |
| Header-only Core `CPhotonMap` | `libRIBVector` constructs it, and its residual list may not grow | A `render/photonMap.cpp` would add a Core file to the list |

## Maintainer answers to the tasks stage (2026-11-09)

tasks.md was approved with every recommendation. Where they change this plan, the text above was edited.

| # | Answer | Effect on this plan |
|---|---|---|
| D8 | Reword the six historical `CRenderer` comments under `src/ri/texture/` in Phase 4 | Phase 4 row; SC-001's literal scan holds |
| T-B | The photon-map fixture is written by an `orender` photon pass, header-free | `contracts/tests.md` T-B row; T-B is not among M1 to M4 |
| D3, D4 | A pause before the T-A, T-B and T-F recordings; approvals split as 2a (new tests) and 2b (include edits) | Phase 1 and Phase 2 rows |
| Version | `kRendererServicesVersion` 2.2.0 is stepped in Phase 8 | none (already planned) |
| No-OpenEXR | Built only if the tree supports configuring one; report either way | none |
| Analyze remediation (2026-11-09) | The Hiders sampler header is `hiderIrradianceSampler.h`; T-A2/T-C/T-D/T-E targets live in `tests/gi_split` and red evidence is taken in a scratch tree; T078a removes the dead `CPhotonRay`; Core's `getCache` answers `nullptr` on a kind mismatch (second CTest name `GiCache_KindMismatch` of T-A2); T-E is enabled by T108a | Phase 2, 4, 5 and 7 rows; `contracts/` |
| D1, D2, D5 | Departures listed in tasks.md: the 6-argument lookup reshape moves to Phase 4; M5 and M6a land in Phase 4, M1 to M4 and M6b in Phase 5; T-A2 is enabled in Phase 4 | Phase 2, 3, 4 and 6 rows; `contracts/tests.md` T-A2 row (the "Red until Phase 6" inconsistency is fixed) |
