# Feature Specification: GI Cache Split

**Feature Branch**: `032-gi-cache-split`

**Created**: 2026-11-06

**Status**: Draft

**Input**: User description: "Spec 032 (GitHub #38): the photon-map and irradiance-cache storage and lookup move to Texturing, while the sampling and emission stay in Hiders through `ITracer`; the data viewer reads the GI and 3D-texture data through adapters it owns; the remote-channel friends are removed. Keep the hook for spec 040: cache records and photons carry a deterministic key so 040 can merge them order-independently. Resolves DB-025, DB-010 (the GI part) and DB-020 (the remote friends). Success: Texturing depends on no other domain, and the GI visual tests are unchanged."

## Context

**Why.** Constitution Principle VIII says Texturing depends on nothing, and Hiders may not depend on Texturing. Spec 027 removed every Texturing dependency but the GI and 3D part; spec 029 gave the hiders their own shading context and Shading its `IPhotonLookup` and `IIlluminationHook`. What is left is the GI caches, which sit on the wrong side of the line twice over: the irradiance cache is a Hiders class built on a Texturing base class, and the photon map is a Hiders class that implements a Shading interface, derives from a Graphics State handle target and draws through the data viewer. Texturing, meanwhile, still names the shading context in its 3D lookup, derives its 3D classes from the data viewer's `CDataView` and befriends a renderer-core class. This spec splits each GI structure in two: passive storage and lookup in Texturing, sampling and emission in Hiders.

**Where this spec sits.** Issue #38. Depends on spec 027 (#33, texture system) and spec 029 (#35, hider/shading separation), both merged (master `e83d005d`). Spec 040 (#79, GI determinism) follows and consumes the keys of User Story 5. Spec 053 (#93, Bxdf GI) builds on the storage this spec produces.

**Spec 027's hand-over settles the 3D family's scope (the plan does not).** Spec 027's FR-014 left to this spec: `CTexture3d`'s lookup and constructor/destructor signatures, "the class hierarchy it shares with `CIrradianceCache`", the photon-map, irradiance-cache and 3D-texture entries of the name-keyed resource cache (`frameFiles`, with the photon-map reference counting of #64), and the services `getCache` and `getTexture3d`. Its SC-005 allows one exception to "zero lookup signatures in `src/ri/texture` name `CShadingContext`": the 3D lookup, for this spec. The plan's row for 032 does not mention the 3D family (its V7 row assigned `texture3d.*`, `brickmap.*` and `pointHierarchy.cpp` to 027, which moved the 2D families only). So the 3D family is in scope here, **limited to what ties it to other domains**: the shading-context parameter, the data-viewer base, the remote friends and the dangling friend. Point-cloud and brick-map internals (paging, hierarchy construction, the `.ptc` and brick formats, `ptcapi`) are out of scope.

**Measured starting point** (read at master `e83d005d`; the plan stage re-measures).

*Texturing's remaining ties* (`src/ri/texture/`):

- `texture3d.h:43` derives `CTexture3d` from `CFileResource` and `CDataView` (`:33` includes `dataView.h`); `:37` forward-declares `CShadingContext`; `:67` declares the pure virtual six-argument `lookup(..., CShadingContext *)`; `:100` befriends `CRemotePtCloudChannel`. `pointCloud.h:65` and `brickmap.h:206` override that lookup with `assert(FALSE)`; `pointCloud.h:149` also befriends `CRemotePtCloudChannel`; `brickmap.h:39` and `:254` forward-declare and befriend `CBrickMapGeometry`, a class defined nowhere.
- `pointHierarchy.h:58` carries `Options::maxSolidAngle(const CShadingContext *)` and `pointHierarchy.cpp:473` implements the lookup with it (spec 027 made it a callback; the pointer is opaque in Texturing).
- `brickmap.cpp` and `pointCloud.cpp` call `CDataView::drawPoints`, `drawDisks`, `drawTriangles` (DB-025 link edges). `brickmap.h` and `pointCloud.h` keep viewer state in statics (`drawDiscs`, `drawChannel`, `detail`, `drawType`) and override `keyDown`, `currentChannel`, `detailLevel`, `drawMode`.
- The allowlist holds no line for Texturing other than DB-025's six (`texture3d.h`, `brickmap.cpp`, `pointCloud.cpp`). The registry's DB-010 history still says Texturing keeps `texture3d.cpp`'s `renderer.h` and `displayChannel.h` includes; that text is stale (spec 037 gave `texture3d.cpp` its own `texture3dChannels.h`, and the Texturing metric row is 0 / 0).

*The irradiance cache* (`src/ri/hiders/`):

- `irradiance.h` derives `CIrradianceCache` from `CTexture3d` and includes `options.h` (Graphics State), `shader.h` (Shading), `depository.h` and `texture3d.h`; `:137` befriends `CRemoteICacheChannel`.
- `irradiance.cpp:100` reads `CRenderer::worldBmin`/`worldBmax` in the constructor. `:309` `lookup()` reads `context->currentShadingState->scratch.occlusionParams` (`pointbased`, `pointHierarchy`, `maxError`). `:388` calls `irradianceSampleAccept(w, smallSampleWeight, context)`, which consumes one value from the context's sample stream (`irradianceDispatch.cpp:675`, `context->drawNext()`) for **every record that passes the front test** (`:365-368` skips a record in front of the query point with `continue` **before** the accept call, and that record draws nothing), even when the weight is zero and even when `smallSampleWeight` is 0 (a cache that is not being sampled). The point-based branch (`pointbased` with a hierarchy) draws nothing. The sampler's own draws (`drawNext2`, `drawNext`, `drawBound2D`) follow on the same cursor, so the stream order is: accept draws in traversal order, then sampler draws.
- The two callers of the cache lookup are both in Shading: the interpreter macro `giFunctions.h:235` and the JIT `shading.cpp:3531` (`cache->lookup(C, ..., this)`), each after `DRAW_BIND`. Shading reaches the cache only through virtuals of `CTexture3d`: no `CTexture3d`, cache or photon-map symbol is among `libshader_shading`'s imports (`specs/029-hider-shading-separation/tools/libshader-imports-after-linux.txt`), and a new non-virtual call from Shading into Texturing code would add an undefined symbol and fail `-L link` (gotcha 28).
- On a miss, `lookup()` calls `sample()` (`irradianceDispatch.cpp:252`): it traces and shades a hemisphere through the context, reads photon maps and inserts the new record into the tree under the cache's mutex (`:589-660`). `vector/irradianceDispatchStub.cpp:48` makes `sample()` a `CODE_BUG` for `libRIBVector` (the DB-017 pair that spec 031 leaves to this spec; `ribvector-core-residual.txt` pins it, and `rendererStatics.cpp` for `irradiance.cpp` and `photonMap.cpp`).
- Files: records are written and read field by field (`writeFloat32` and friends, portable). `CCacheSample` goes over the network **as raw `sizeof` bytes** (`render/remoteChannel.cpp:514-600`).

*The photon map* (`src/ri/hiders/photonMap.{h,cpp}`):

- `CPhotonMap : CMap<CPhoton>, CFileResource, CDataView, CMapRefTarget, IPhotonLookup`. It includes `stateHandles.h` and `xform.h` (Graphics State), `photonLookup.h` (Shading), `ray.h`, and `object.h`/`raytracer.h` in the `.cpp` (Geometry, Hiders), and reads `CRenderer::toWorld`/`fromWorld` at `photonMap.cpp:102-103,212,217` (DB-011).
- Writers: `photon.cpp:630-662` (`modifying`, `reset()`, `balanceList`, `store(P, N, ray.dir, Cl)`) and `photon.cpp:97-102` (`write(CRenderer::world)` in the destructor). Readers: `radiance.cpp:315,1119` and `irradianceDispatch.cpp:477` call the four-argument `lookup(C, P, N, estimator)`; Shading calls `IPhotonLookup::lookup(C, P, maxFound)`, which has no normal and no estimator.
- `CMap::write` (`core/map.h:140`) is a raw `fwrite` of `sizeof(T)` items with no header: any field added to `CPhoton` changes the `.pm` layout.

*The core and the services*: `CRenderer::getPhotonMap`, `getCache`, `getTexture3d` (`rendererFiles.cpp:147,185` and below) construct the structures, look them up in `frameFiles` and register remote channels under a network client; `CRendererServices::getCache`/`getTexture3d` hand `CTexture3d *` to Shading and `getPhotonMap` hands an `IPhotonLookup *`. Shading includes `texture3d.h` (`shading.cpp:49`, `execute.cpp:49`), which the table allows.

*The viewer*: `src/ri/dataviewer/` (class Consumer) defines `CDataView`, `CPrimitiveSink` and `CDebugView`; `dataLoad.cpp` constructs the document types through their public constructors; `libribpreview` adapts the sink into a `DataScene` (`dataSink.*`). DB-025 is 12 allowlist lines: 11 in Texturing and Hiders and **one in Geometry** (`geometry/surface.cpp` includes `debug.h`; the use is a `CDebugView` under `DEBUG_HITS` / `DEBUG_TESSELATIONS`, off in every build).

**The dependency table shapes the design** (`tests/architecture/domain-rules.txt`): `Texturing ->` nothing; `Hiders -> GraphicsState Geometry Shading Display`; `Shading -> GraphicsState Texturing ShaderDSO`; Core and Consumers may depend on everything; everyone may depend on Infra. A Texturing class therefore cannot implement `IPhotonLookup` (Shading), derive `CMapRefTarget` (Graphics State), call back into the sampler (Hiders) or read `CRenderer` (Core); and a Hiders class cannot name a Texturing type. Whatever joins a Texturing structure to its consumers must be owned by Infra or Shading, or be bridged by Core. The plan's one-line brief does not say how; the Clarifications below settle it.

## Clarifications

### Session 2026-11-07

All nine answers are the maintainer's. Option names are those of the clarify questions.

- Q1: Where does the irradiance cache's miss path live? → A. The storage is passive: `lookup` returns hit or miss and takes a draw callback. One shared Shading function, used by both call sites (`giFunctions.h:235` and `shading.cpp:3531`), does lookup, then on a miss asks a Shading-owned sampler interface that the `CHider` base installs (one class, not one per hider), then calls a virtual `insert` on the store. The sampler stays in Hiders. The record and store interfaces live in Infra, or the store's abstract class is header-only in Texturing, so `libshader_shading` gains no undefined symbol. The insert keeps the existing lock discipline, including `clamp()`'s. `irradianceDispatch.cpp`, its stub and `irradianceSampleAccept` are deleted by this spec (every Q1 option removed that pair, B included); spec 031 leaves the pair to this spec.
- Q2: Who removes the `geometry/surface.cpp` include of `debug.h`? → B. Spec 031 deletes the include and the dead `DEBUG_HITS`/`DEBUG_TESSELATIONS` blocks (decided and recorded in 031). This spec deletes the other 11 DB-025 lines. Whichever spec merges second moves DB-025 to Resolved; no task depends on the other's merge, so this spec records a history entry and leaves the status for that merge, unless the line is already gone from its base.
- Q3: Are the 040 keys persisted or sent? → B, refined. The cache record key is in memory and on the network wire. The photon key is in memory in a parallel side array outside `CPhoton`, which `CMap::balance` and growth permute with the items. No file carries a key: `.pm` and cache files stay byte-identical and `tests/visual/test_photon_solar0.cpp` (which hard-codes `PHOTON_SIZE = 44`) is untouched. Wire compatibility is same-build only (the handshake compares only MAJOR.MINOR).
- Q4: Photon-map lifetime object and test edits? → A. A Core-level class keeps the name `CPhotonMap` and derives from the Texturing store, `CMapRefTarget` and Shading's `IPhotonLookup`, so the #64 lifetime tests stay valid. The only permitted edits to existing tests are mechanical include-path and namespace changes, listed in the plan for approval at the test-approval pause. Hiders reach the photon map through store and estimator interfaces in Infra or Shading (the plan places them); the four-argument `lookup` and the `photonMapOf()` replacement go under FR-009's version step.
- Q5: Linux standard for GI renders? → A. GI scenes are byte-identical on both platforms, each against its own master baseline, with `MaxBlockAvgDiff` unchanged (the standard of specs 029, 030 and 031). A `bake3d` crash (#122) is re-run in isolation and not judged a regression. A Linux target that fuses multiply-adds (aarch64, or x86 with `-march=native`) is not covered: the validation host is x86_64 with no FMA and the build sets no `-march`/`-ffp-contract` option.
- Q6: Delete the `CBrickMapGeometry` friend? → Yes, both lines (`brickmap.h:39,254`; defined and used nowhere in the tree). This fully resolves DB-020 here.
- Q7: Loopback test for the remote channels? → A. A new loopback test over `socketpair()`, recorded on the unmodified base tree before any `src/` edit, comparing the receiver's lookups with the sender's and the defined bytes with a stored reference. It masks the `next` pointer of each `CCacheSample` and the uninitialised tail of the 1024-byte point-cloud channel definition. It needs explicit approval at the test-approval pause. The after-state reference differs by exactly the cache record's key field.
- Q8: ThreadSanitizer run for #122? → Left out of this spec. Kept: the `lookup`/`store` bodies are unchanged apart from the signature; `bake3d` crashes are re-run in isolation; #122 is referenced; there is no crash-rate criterion and no claim of a fix. No informational pair run.
- Q9: Performance bar? → B. Within the run-to-run noise measured on the starting tree, on new scaled GI scratch copies (occlusion, indirectdiffuse with a photon map, point cloud), in interleaved series, as spec 029 did, with the scenes under `specs/032-gi-cache-split/tools/`. The "2%, the bar of specs 023 and 039" attribution was wrong: spec 039 replaced 2% with per-scene budgets, and no existing performance scene exercises GI.

### Plan-stage corrections (2026-11-07)

The maintainer approved the plan with every recommendation (plan.md, "Maintainer answers"). These corrections follow from those answers. No FR or SC was renumbered. Q7 above is kept as decided; OQ-5 and OQ-8 below refine what its loopback test checks.

- **OQ-3**: US5's Independent Test is corrected.
  - Photons give the same (key, photon) set across runs and across thread counts.
  - Irradiance records give the same (key, record) set across repeated `-t:1` runs, and a record created at a given sample has the same key at any thread count. Which points miss the cache depends on insertion timing until spec 040.
  - The cross-thread photon comparison is opt-in (label `gi_split_threads`).
- **OQ-4**: the dead files `src/ri/hiders/radiance.{h,cpp}` and `harmonics.h` are deleted (FR-002). No CMake list compiles them, and they do not compile.
- **OQ-5**: US4 AS1, its Independent Test and SC-005 are corrected. The base receiver places a record by `dP` without the sender's `/K`, so sender and receiver lookups may differ even on the base tree.
  - The loopback test pins the receiver's written tree against the base recording.
  - A finished-tree test compares receiver and sender lookups and reports any difference without fixing it.
- **OQ-8**: some bytes are never assigned: `items[0]`, the `CPhoton` padding, the dummy photon's direction, and the `CPointCloudPoint` padding.
  - FR-019 and US2 AS4 compare `.pm` files field by field over the defined bytes; cache files keep a byte comparison.
  - FR-012 and SC-005 add the `CPointCloudPoint` padding as a third loopback mask.
  - Every base recording is taken twice and must agree.
- **OQ-6**: DB-017 follows DB-025's rule: whichever of this spec and spec 031 merges second moves it to Resolved (FR-021).
- **The SC-002 target**: exactly 319 `DB-` lines. The deleted dispatch file's include line goes in any case, so 320 cannot happen; 318 only if the base already lacks the `surface.cpp` line.
- **Spec 031 overlap**: spec 031 (its option P-Q3 (a)) also deletes allowlist lines 137-138 (`photonMap.cpp -> toWorld/fromWorld`), which this spec deletes in its photon phase. Whichever spec merges second drops its duplicate edit. If this spec lands first, 031's `FrameParams`-based read in `photonMap.cpp` disappears with the file.

Corrections folded into the spec from the clarify scan: the accept draw is consumed per record that passes the front test (not per candidate); the determinism sweep contains no GI scene except the two `bake3d` scenes; the remote channels are not wholly untested (the sweep's `-P:2` runs the two `bake3d` scenes through `CRemotePtCloudChannel`; `CRemoteICacheChannel` has no coverage).

## User Scenarios & Testing *(mandatory)*

The "users" are the maintainer and contributors (domain boundaries, the GI code), shader authors and render users (whose images must not change), tool authors (the data viewer, the preview) and the author of spec 040.

### User Story 1 - Texturing depends on no other domain, and Hiders do not depend on Texturing (Priority: P1)

A maintainer builds and tests the Texturing code without any other domain. Every GI and 3D storage structure (irradiance cache, photon map, point cloud, brick map, point hierarchy) lives in `src/ri/texture/` and includes only Texturing and Infra headers. The hiders sample and emit; they reach storage only through interfaces owned by Shading or Infra.

**Why this priority**: it is the first acceptance line of #38 and the last Texturing dependency in the registry. Adapters, friends and keys follow from it.

**Independent Test**: the include scan of `src/ri/texture` lists no header of another domain; the architecture check lists no allowlist line with a Texturing origin or a Hiders-to-Texturing edge; a Texturing-only test program links `libRIBCommon` and the texture library and constructs, fills, looks up, writes and reads back an irradiance cache and a photon map.

**Acceptance Scenarios**:

1. **Given** the finished tree, **When** `src/ri/texture` is searched for `CShadingContext`, `CRenderer`, `CDataView`, `CRemote`, `CMapRefTarget`, `IPhotonLookup` and `dataView.h`, **Then** nothing is found.
2. **Given** the architecture check, **When** it runs on macOS and Linux, **Then** it passes, DB-010 has no allowlist line, DB-025 has at most the one `geometry/surface.cpp` line (spec 031 removes it), and Texturing's `CRenderer::` metric row stays 0 / 0.
3. **Given** a program that links only Texturing and Infra, **When** it builds a photon map from stored photons and an irradiance cache from inserted records, writes both and reads them back, **Then** the lookups on the reloaded data equal the lookups on the original.
4. **Given** the hiders' sources, **When** they are scanned, **Then** none includes a `src/ri/texture` header or references a Texturing symbol (include edge or link edge).

---

### User Story 2 - GI scenes render exactly as before (Priority: P1)

A render user running occlusion, indirectdiffuse, photon-map, point-cloud and brick-map scenes gets the same pixels from the same RIB, interpreter and JIT, on both platforms.

**Why this priority**: the other acceptance line of #38 and the starting point of 040. The move touches the hot lookup path and the order of draws from the sample stream.

**Independent Test**: the GI test set (SC-003) renders on macOS and on Linux to bytes identical to the same platform's master baseline at the same thread pins, with every `MaxBlockAvgDiff` unchanged.

**Acceptance Scenarios**:

1. **Given** each scene of the GI test set, **When** it is rendered on the finished tree at the same thread count and seed as on master, **Then** the output is byte-identical to that platform's master baseline on macOS and on Linux, and no threshold or reference changed. A `bake3d` scene that crashes (#122) is re-run in isolation and is not judged a regression.
2. **Given** an irradiance-cache lookup with candidate records, **When** it runs, **Then** it consumes the same number of values from the sample stream, in the same order, as before: one accept draw for every record that passes the front test, including those with zero discard weight, none for a record skipped by the front test, none on the point-based branch, then the sampler's draws (FR-006).
3. **Given** a `.slo` and a `.rslo` render of the same GI scene, **When** both run, **Then** their parity is unchanged.
4. **Given** a scene that writes a photon map and a later frame or scene that reads it, **When** both run, **Then** the `.pm` file written by a single-threaded run equals master's in every defined field, and maps written before this spec read back unchanged (no file carries a key). The comparison is field by field and skips `items[0]`, the `CPhoton` padding and an empty map's dummy direction, which are never assigned.

---

### User Story 3 - The data viewer reads GI and 3D data through adapters it owns (Priority: P2)

A tool author opens a photon map, an irradiance cache, a point cloud or a brick map in the data viewer or the preview. The viewer owns one adapter per document type; the structures know nothing about the viewer. Viewer-only state (selected channel, draw mode, detail level, key handling) lives in the adapter.

**Why this priority**: DB-025 is 12 allowlist lines and the reason Texturing and Hiders include a Consumer. It is independent of the storage move except for the read access it needs.

**Independent Test**: the preview's data tests (`test_data_keys`, `test_data_pointcloud`, `test_data_disk_expand`, `test_data_world_init`, `test_dataview_chunking`, `test_debugdump_roundtrip`) pass unchanged; a scan of `src/ri/texture` and `src/ri/hiders` finds no `dataView.h`, `debug.h` or `CDataView`.

**Acceptance Scenarios**:

1. **Given** each document type (photon map, irradiance cache, point cloud, brick map, debug dump) loaded through `CDataDocument::open`, **When** the preview extracts its scene, **Then** the `DataScene` (type name, channel count and names, current channel, detail level, draw mode, bound, primitive batches and their chunking) equals the pre-change one.
2. **Given** a key press on a loaded document (`d`, `p`, a channel or detail key), **When** the adapter handles it, **Then** the state and the redrawn primitives are what they were.
3. **Given** a brick map larger than memory, **When** the viewer draws it, **Then** the adapter reads bricks through the storage's read interface and loads them on demand as the old draw did.

---

### User Story 4 - Remote channels use the storage's own interface (Priority: P2)

A network-render maintainer reads the remote channels for the irradiance cache and the point cloud and finds they use public operations of the structures, not friendship. The bytes on the wire are unchanged.

**Why this priority**: it removes DB-020's remaining cross-domain entries. It shares its read interface with User Story 3.

**Independent Test**: a new loopback test (needing explicit approval at the test-approval pause) drives each channel's send and receive over a `socketpair()` without a network render, recorded on the unmodified base tree before any `src/` edit (twice, the two recordings agreeing) and passing after the change. It checks:

- the receiving cache's written tree against the base recording, and the receiving point cloud against the sender;
- the defined sent bytes against the stored reference, with three masks: the `next` pointer of each `CCacheSample`, the uninitialised tail of the 1024-byte point-cloud channel definition, and the padding of each raw `CPointCloudPoint`.

A finished-tree test also compares the receiver's lookups with the sender's and reports any difference.

**Acceptance Scenarios**:

1. **Given** an irradiance cache with records in several nodes, **When** its channel sends a frame and a second cache receives it, **Then** the second cache's tree (as written to its file) equals the one the unmodified tree produced, and the defined sent bytes equal the pre-change bytes except for the record key field the wire now carries. The second cache's lookups are compared with the first's on the finished tree, and any difference is reported, not fixed: the receiver places records by `dP` without the sender's `/K`, as it does today.
2. **Given** a point cloud with several channels, **When** its channel sends its setup data and frame to a second cloud, **Then** the second cloud has the same channels, points and data.
3. **Given** the tree, **When** searched for `friend` in `src/ri/texture` and `src/ri/hiders`, **Then** `CRemoteICacheChannel`, `CRemotePtCloudChannel` and `CBrickMapGeometry` are gone, and DB-020 has no cross-domain friend left.

---

### User Story 5 - Records and photons carry a deterministic key for spec 040 (Priority: P3)

The author of spec 040 merges per-thread records and photons in an order that does not depend on thread timing. Each irradiance record and each photon carries a key derived from identities the sampler already defines, readable from the structure, so 040 needs no further layout change.

**Why this priority**: it is the plan's explicit hook, cheap now and costly after the layouts change again. It changes no image.

**Independent Test**:

- **Photons**: a photon pass gives the same set of (key, photon) pairs (compared field by field; order may differ) across repeated runs and across thread counts. The cross-thread-count comparison is opt-in (label `gi_split_threads`): the intermittent `CPhotonHider` crash and the unguarded `modifying` reset make it unfit for the everyday run.
- **Irradiance records**: repeated `-t:1` runs give the same set of (key, record) pairs, and a record created at a given sample has the same key at any thread count. The record set itself depends on insertion timing until spec 040.
- No render output changes.

**Acceptance Scenarios**:

1. **Given** a photon emitted with global photon index *i*, **When** it is stored, **Then** it carries a key that is a pure function of *i*, the light, the bounce and the seed (the identity `photon.cpp:207-212` already derives), drawing no random value.
2. **Given** an irradiance record created at a shading point, **When** it is inserted, **Then** it carries a key that is a pure function of the identity of the sample that triggered it (the plan names it "bucket/record key"; the definition is a plan decision), identical across thread counts for the same scene, and sent with the record on the network wire.
3. **Given** the keys, **When** a lookup runs, **Then** its result, the insertion order, the tree shape and the sample-stream consumption equal those without keys.

---

### Edge Cases

- **A miss during a lookup that is also iterating the tree**: today the cache takes its mutex only for the insert; lookups read without a lock ("if word-writes are atomic"). The split must keep that discipline: a passive store read while another thread inserts must not change which records a concurrent lookup can see, or when.
- **Draw order**: the accept test consumes a draw per record that passes the front test (`irradiance.cpp:365-368` skips the others first), before the sampler runs; the point-based branch draws nothing. The draw callback the Shading driver passes must be called at the same point of the traversal; "the caller decides" would shift the stream and change images.
- **Lock discipline across the split**: lookups read the tree without a lock while `clamp()` lowers existing records' `dP` under the mutex (`irradiance.cpp:504-511`). The store's `insert` keeps that discipline exactly, `clamp()` included; the sampler's gradient computation moves outside the lock (timing only, results unchanged at `-t:1`).
- **Point-based occlusion** (`pointbased` with a point hierarchy): `lookup()` forwards to `CPointHierarchy::lookup`, whose solid-angle limit comes from `scratch.occlusionParams.maxSolidAngle` through spec 027's callback. The callback loses its context pointer; the value must still reach the hierarchy per lookup, not per hierarchy.
- **Photon map not found**: `CRenderer::getPhotonMap` constructs an empty map for a file that does not exist (`sphere-photonmap-reyes` depends on it). Unchanged.
- **Photon-map lifetime**: `framePhotonMaps` and the reference count that `CAttributes` handles hold (#64) keep their semantics when the map is no longer itself a `CMapRefTarget`. `photonmap_FrameCacheRelease` and `photonmap_TwoFrameSession` are the guard.
- **Network rendering has thin coverage**: `-P:N` spawns local servers over sockets, and the determinism sweep runs the two `bake3d` scenes at `-P:2` (opt-in label), which exercises `CRemotePtCloudChannel` through the image only. `CRemoteICacheChannel` has no coverage: the sweep contains no GI scene except those two `bake3d` scenes. A loopback characterization (US4) is part of this spec; the full client/server path stays untested for the cache and is recorded as a risk.
- **Wire compatibility is same-build only**: `CCacheSample` goes on the wire as raw bytes and the record key changes its size; the handshake compares only MAJOR.MINOR (`rendererNetwork.cpp:512`), so peers of different builds of one version are not detected.
- **`libRIBVector`**: the preview links it, and it carries `irradianceDispatchStub.cpp` and the CRenderer statics for `irradiance.cpp` and `photonMap.cpp`. The stub goes with the dispatch file; when storage is renderer-free the residual list shrinks or its reasons are rewritten; it may not grow. Spec 031 edits the same residual file and the same `rendererStatics.cpp` row (Geometry and Hiders names share a line), so the second to merge resolves a textual conflict; rebase order is the maintainer's.
- **The `.pm` format** is unversioned and raw (the file header carries only the project MAJOR.MINOR); no file changes in this spec, and the photon key stays outside `CPhoton` because `test_photon_solar0.cpp` pins its 44-byte layout.
- **Builds**: the no-JIT, no-OpenEXR and TSan (`OPENRENDER_TSAN`) builds keep building; the TSan tree must not become harder to use. Investigating #122 under it is not part of this spec.
- **Existing tests that name the moved classes**: `tests/unit/photon_map/test_photon_map_{frame_release,sessions}.cpp`, `tests/preview/test_dataview_chunking.cpp`, `test_data_world_init.cpp` and others include `ri/hiders/photonMap.h`; only mechanical include-path and namespace edits are allowed, listed in the plan for approval at the test-approval pause.
- **Point cloud and brick map internals stay put**: where a move forces a signature change, the bodies of `CPointCloud::lookup`/`store` and `CBrickMap`'s paging are not edited (see Overlap, #122).
- **Brick-map drawing is lazy**: drawing walks bricks that may not be loaded; the read interface must allow that without exposing the paging internals to the viewer.

## Requirements *(mandatory)*

### Functional Requirements

**Boundary**

- **FR-001**: Every source and header under `src/ri/texture/` MUST include only Texturing and Infra headers and reference no symbol of another domain's class; Texturing's `CRenderer::` metric row MUST stay 0 / 0. The name `CShadingContext` MUST NOT appear under `src/ri/texture/` (this closes spec 027's SC-005 exception).
- **FR-002**: No file under `src/ri/hiders/` MAY include a Texturing header or reference a Texturing symbol after this spec (include edge or link edge). Hiders reach the irradiance cache and the photon map only through interfaces owned by Shading or Infra (the sampler interface is Shading's; the photon store and estimator interfaces are Infra's or Shading's, placed by the plan; FR-027, FR-028). The dead files `src/ri/hiders/radiance.{h,cpp}` and `harmonics.h` are deleted: no target compiles them, and `radiance.cpp` includes `texture.h`.
- **FR-003**: A file that moves into `src/ri/texture/`, or a new one created there, MUST satisfy FR-001 when it arrives. No allowlist line MAY be added or re-keyed by this spec; a violation in a moved file cannot be allowlisted under its new path.
- **FR-004**: The storage and lookup of the irradiance cache (nodes, records, tree insertion, the weighting of a lookup, clamping, bound, file read and write) and of the photon map (the `CMap<CPhoton>` store, the lookup octree, `store`, `balance`, `reset`, file read and write, `searchRadius`, `maxPower`) MUST live in Texturing. They MUST NOT derive from `CMapRefTarget`, `IPhotonLookup` or `CDataView`; the Graphics State handle target and Shading's lookup interface are provided by the Core-level `CPhotonMap` (FR-028), outside Texturing.
- **FR-005**: Everything a storage structure reads from the renderer (`CRenderer::worldBmin`/`worldBmax` for the cache, `toWorld`/`fromWorld` for the photon map's file transform and write, the NDC matrix) MUST be passed in by its creator or by the call. The photon hider's `write(CRenderer::world)` becomes an explicit argument.
- **FR-006**: The irradiance-cache lookup MUST NOT name the shading context. The per-lookup parameters it reads today from `scratch.occlusionParams` (`pointbased`, the point hierarchy, `maxError`, `maxSolidAngle`) and the discard draw MUST come from the caller (a draw callback). The number, order and call site of draws from the sample stream MUST be preserved exactly: one accept draw for every record that passes the front test (`irradiance.cpp:365-368`, applied before the accept call), including records of zero weight and including a cache whose `smallSampleWeight` is 0; none for a record the front test skips; none on the point-based branch; the accept draws in traversal order, then the sampler's draws.
- **FR-007**: `CTexture3d`'s six-argument lookup, its overrides in `CPointCloud`/`CBrickMap`/`CPointHierarchy` and `CPointHierarchy::Options` MUST be reshaped so they name no shading type. The bodies of `CPointCloud::lookup`/`store` and of the point hierarchy's traversal MUST be unchanged apart from the signature.
- **FR-008**: The sampling of the irradiance cache (the hemisphere sample, tracing, shading of hits, photon-map reads, gradients, radius) and the emission, bouncing and storing of photons MUST stay in Hiders and reach the tracer through `ITracer`. The hand-off to the storage (lookup, miss, insert) is FR-027's. `irradianceDispatch.cpp`, `vector/irradianceDispatchStub.cpp` and `irradianceSampleAccept` MUST be deleted by this spec.
- **FR-009**: `CRendererServices::getCache`, `getTexture3d`, `getPhotonMap` and the `texture3d*` operations MUST keep working for Shading. If any signature changes, `kRendererServicesVersion` MUST step with a history entry (2.x policy: a MINOR step may break, saying so), and every implementer and the contract test double MUST follow. The four-argument photon `lookup(C, P, N, estimator)`, the `photonMapOf()` replacement and any change to `getCache`'s return type fall under this step.
- **FR-010**: The GI and 3D entries of `frameFiles` (name-keyed), the `framePhotonMaps` release at `endFrame()` (#64) and the construction of an empty map for a missing file MUST behave exactly as today.

**Friends and remote channels**

- **FR-011**: The friends `CRemoteICacheChannel` (`irradiance.h:137`), `CRemotePtCloudChannel` (`texture3d.h:100`, `pointCloud.h:149`) and `CBrickMapGeometry` (`brickmap.h:254`, and its forward declaration at `:39`; defined and used nowhere in the tree) MUST be removed. The channels MUST read and fill the structures through public operations the storage offers (read-only traversal of records, points and channels; insertion; channel definition), the same operations the viewer adapters use.
- **FR-012**: The defined bytes a channel sends for a given structure MUST equal today's, except that the irradiance record carries its key on the wire (the one recorded change; sender and receiver agree, same-build compatibility only). "Defined" excludes three things, which a test masks: the `next` pointer of each `CCacheSample`, the uninitialised tail of the 1024-byte point-cloud channel definition, and the padding of each raw `CPointCloudPoint`. A new loopback test of both channels over `socketpair()` MUST be recorded on the unmodified base tree before any `src/` edit (twice, the two recordings agreeing) and pass on the finished tree; it needs explicit approval at the test-approval pause. The after-state reference differs by exactly the key field.
- **FR-013**: The friend `makeBrickMap` of `CPointCloud` (`pointCloud.h:147`, inside Texturing) is not a cross-domain friend and MAY stay.

**Data viewer**

- **FR-014**: No class under `src/ri/texture/` or `src/ri/hiders/` MAY derive from `CDataView` or call its static drawing functions. The viewer MUST own one adapter per GI and 3D document type (photon map, irradiance cache, point cloud, brick map; the point hierarchy's draw is empty and needs none), implementing `CDataView` over a read-only accessor of the structure.
- **FR-015**: The adapters MUST own the viewer-only state that lives in statics of the structures today (`drawDiscs`, `drawChannel`, `detail`, `drawType`) and the key handling. Behaviour observable by the preview (`DataScene` fields, primitive batches, chunking) MUST be identical.
- **FR-016**: No file under `src/ri/texture/` or `src/ri/hiders/` MAY include `dataView.h` or `debug.h`. `geometry/surface.cpp` is spec 031's to clean (the include and the dead `DEBUG_*` blocks); this spec does not edit it.

**Keys for spec 040**

- **FR-017**: Each irradiance record and each photon MUST carry a deterministic key, exposed through the structure's read interface. The record key is a field of the record (in memory and on the wire). The photon key lives in a parallel side array of the map, outside `CPhoton`, which `CMap::balance` and growth MUST permute with the items. The photon key is a pure function of the identity `photon.cpp:207-212` already derives (global photon index, light, seed, frame) plus the bounce; the record's key is a pure function of the sample identity of the shading point that triggered it (definition recorded in the plan). Neither key draws a random number.
- **FR-018**: With keys present, lookups, insertion order, tree shape, balancing, stream consumption and every rendered image MUST be what they were. No rendering path reads a key in this spec.
- **FR-019**: No file carries a key. For the same scene at `-t:1`:
  - the cache file bytes MUST equal master's;
  - the `.pm` file MUST equal master's in every defined field, compared field by field, skipping the bytes that are never assigned (`items[0]`, the `CPhoton` padding, an empty map's dummy direction).

  `CPhoton` keeps its 44-byte layout, `CPhoton` keeps its 44-byte layout and `tests/visual/test_photon_solar0.cpp` is untouched. Only the irradiance record is sent with its key; wire compatibility is same-build only.

**Registry, allowlist, tests, documents**

- **FR-020**: The allowlist lines this spec retires MUST be deleted (SC-002), none added or re-keyed; `Architecture_DomainBoundaries` MUST pass on macOS and, from the Linux run, on Linux. A line whose key (a path) no longer holds the code is stale and MUST be deleted.
- **FR-021**: DB-010 and DB-020 MUST move to Resolved with this spec's number and the date. DB-025 gets a history entry for the 11 lines this spec removes; its status moves to Resolved with whichever of this spec and spec 031 merges second, and no task or registry text of either depends on the other's merge (unless the `surface.cpp` line is already gone from this spec's base, in which case this spec resolves it). DB-017 follows the same rule:
  - this spec deletes the irradiance pair and records a history entry;
  - spec 031 deletes the other three pairs;
  - whichever of the two merges second moves DB-017 to Resolved, unless the other three pairs are already gone from this spec's base.

  The history of DB-011, DB-017, DB-024 and the Texturing / Hiders `CRenderer::` metric baseline MUST be updated for what this spec changes. `ribvector-core-residual.txt` MAY only shrink.
- **FR-022**: The stale registry text (DB-010 history about `texture3d.cpp`; DB-020's tracking line, which names spec 030 for friends spec 029 already removed) MUST be corrected in the same edit.
- **FR-023**: No requirement, task, success criterion or registry entry of this spec MAY depend on the merge: no post-merge step, no merge-commit hash. Issue #38 closes through a `Closes #38` line the maintainer writes in the merge commit.
- **FR-024**: The architecture check, the GI test set, the preview data tests and the plug-in ABI texture3d characterization MUST also pass on Linux (Ubuntu 24.04, GCC 13, `-Werror`).
- **FR-025**: Documentation edits belong to the implement stage and are shown to the maintainer for approval: the gotcha 24, 25 and 28 text of `CLAUDE.md` about the 3D family, `getCache`/`getTexture3d` and `CPhotonMap`; `DEVNOTES.md`; `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`. This stage edits none of them.
- **FR-026**: The GI thread pins stay (`-t:1`, `photon-parity` and the others) with their pointer to spec 040; no test threshold is widened and no reference is regenerated by this spec.

**Hand-off, photon map, tests** (from the 2026-11-07 session)

- **FR-027**: One shared Shading function, called by both the interpreter (`giFunctions.h:235`) and the JIT (`shading.cpp:3531`), MUST do the cache lookup, the miss hand-off and the insert, so the two paths cannot diverge. The storage is passive: its `lookup` returns hit or miss and takes a draw callback and the per-lookup parameters. On a miss the function asks a Shading-owned sampler interface that the `CHider` base installs (one implementation, not one per hider; the sampler stays in Hiders), then calls a virtual `insert` on the store. The record type and the store interface are owned by Infra, or the store's abstract class is header-only in Texturing; every Shading call into the store is a virtual call, so `libshader_shading` gains no undefined symbol (`-L link` stays green). The insert and `clamp()` keep the existing lock discipline.
- **FR-028**: A Core-level class named `CPhotonMap` MUST derive from the Texturing photon store, `CMapRefTarget` and Shading's `IPhotonLookup`, so `getPhotonMap`, the #64 reference counting and the lifetime tests keep their shape. Hiders (`photon.cpp`'s store, balance, reset, `modifying` and write path; `radiance.cpp`; the sampler's photon reads) reach the map only through store and estimator interfaces in Infra or Shading, placed by the plan.
- **FR-029**: The only permitted edits to existing tests are mechanical include-path and namespace changes, each listed in the plan and approved at the test-approval pause. `test_photon_solar0.cpp` is not edited. No existing assertion, threshold or reference changes.
- **FR-030**: The `lookup` and `store` bodies of `CPointCloud` and of the point hierarchy's traversal MUST be unchanged apart from the signature; this spec adds no crash-rate criterion for #122, claims no fix, and runs no ThreadSanitizer investigation. A `bake3d` crash in a validation run is re-run in isolation before it is judged.

### Key Entities

- **Irradiance cache (storage)**: an octree of records over a world bound; a record holds position, normal, irradiance, coverage, environment direction, translational and rotational gradients, validity radius and (new) a key. Passive: it stores, looks up and iterates; it does not sample.
- **Irradiance sampler**: the Hiders-side computation of a new record at a point (hemisphere trace, shading, gradients); the source of record keys.
- **Photon map (storage)**: a balanced spatial map of photons (position, normal, power, packed direction, flags and a key), a lookup octree, the file transform, `maxPower`, `searchRadius`; passive.
- **Photon emitter**: the photon hider's emission, bounce and store logic; the source of photon keys.
- **Read interface**: the narrow, read-only traversal (and the matching insertion) a structure offers to the remote channels and the viewer adapters.
- **Bridge**: the glue in Core, Shading or Hiders that lets a Texturing structure act as a Graphics State map handle and as Shading's `IPhotonLookup`.
- **Viewer adapter**: an object in the data viewer that implements `CDataView` over a structure's read interface and owns the viewer-only state.
- **GI key**: the deterministic identity of a record or a photon (width a plan decision), for 040's merge. The record key is a record field (memory and wire); the photon key is a parallel side array of the map.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: `src/ri/texture/` contains zero cross-domain includes and zero references to `CShadingContext`, `CRenderer`, `CDataView`, `CRemote*`, `CMapRefTarget`, `IPhotonLookup`; the architecture check reports no Texturing-origin allowlist line and Texturing's metric row is 0 / 0.
- **SC-002**: Allowlist lines retired, all by deletion: DB-010: 8 (all Hiders-origin); DB-025: 11 (Texturing 6, Hiders 5; the twelfth, `geometry/surface.cpp`, is spec 031's); DB-011: 5 keyed to the files whose reads are injected (`irradiance.cpp` include, `worldBmax`, `worldBmin`; `photonMap.cpp` `toWorld`, `fromWorld`), plus `irradianceDispatch.cpp`'s `renderer.h` include if the file's replacement no longer needs it (the file itself is deleted by FR-008, so its three lines, two DB-010 and one DB-011, are stale either way). From 344 `DB-` lines (verified at `e83d005d`), the target is exactly 319: 25 lines deleted (DB-010 8, DB-011 6, DB-025 11). The deleted dispatch file's include line goes in any case, so 320 cannot happen; 318 is reached only if the `surface.cpp` line is already gone from this spec's base. `photon.cpp`'s DB-011 and DB-017 lines stay. Re-measured at plan (plan.md, research M).
- **SC-003**: The GI test set renders byte-identical to the same platform's master baseline, on macOS and on Linux, at the same pins, with every `MaxBlockAvgDiff` unchanged: `Visual_sphere-{occlusion,indirectdiffuse,photonmap,texture3d-bake3d}-reyes` and their `-slo` variants; `photon-parity` (interpreter and JIT); `sampling-occlusion-*`; the irradiance, point-cloud, brick-map and photon scenes of the visual suite and the two `bake3d` scenes of the determinism sweep (the sweep holds no other GI scene; at the sweep's pins); `photonmap_FrameCacheRelease`, `photonmap_TwoFrameSession`, `photonmap_ObjectDefinitionRelease`, `PhotonSolar0_*`; the plug-in ABI texture3d characterization. A `bake3d` crash (#122) is re-run in isolation and not judged a regression. A Linux target that fuses multiply-adds is not covered. No threshold widened, no reference regenerated.
- **SC-004**: The preview's data tests listed in User Story 3 pass unchanged; the `DataScene` of the five document types equals the pre-change recording.
- **SC-005**: The remote-channel loopback test (recorded twice on the unmodified base tree, approved at the test-approval pause) passes on the finished tree for both channels:
  - the defined bytes sent equal the recorded bytes except for the documented record key field (masked: the `next` pointer, the uninitialised channel-definition tail and the `CPointCloudPoint` padding);
  - the receiving cache's written tree equals the base recording;
  - the finished-tree lookup comparison reports any receiver/sender difference, which is recorded, not fixed.
- **SC-006**: A Texturing-only program (links Infra and the texture library) builds, fills, writes, reloads and queries an irradiance cache and a photon map with equal results.
- **SC-007**: Render time of three new scaled GI scratch copies (an occlusion scene, an indirectdiffuse scene with a photon map, a point-cloud scene), kept under `specs/032-gi-cache-split/tools/`, is within the run-to-run noise measured on the starting tree, in interleaved series on the same host and thread count, as spec 029 did (its FR-026/SC-010). No existing performance scene exercises GI, and spec 039 replaced the 2% bar with per-scene budgets; the plan names the scenes and the series count.
- **SC-008**: `grep -rn "friend " src/ri/texture src/ri/hiders` shows no cross-domain friend; DB-020 is Resolved. `irradianceDispatch.cpp`, `vector/irradianceDispatchStub.cpp` and `irradianceSampleAccept` no longer exist, `libshader_shading` imports no new symbol (`-L link` green), and `ribvector-core-residual.txt` has lost the stub's line.
- **SC-009**: `ctest -L architecture`, `-L visual`, `-L shading_parity`, `-L texture_tile`, `-L texture_only`, `-L state`, `-L plugin_abi` and `-L link` pass on macOS, and the same set on Linux, with no new failure against master (known intermittent tests are re-run in isolation before judging).
- **SC-010**: No open-question marker remains in this file (the three were answered in the 2026-11-07 session), and no FR, task or criterion depends on a merge (FR-023).

## Assumptions

- The dependency table of `tests/architecture/domain-rules.txt` is the constraint, not a suggestion; the constitution is not amended by this spec.
- The 3D family's internals (point-cloud format and I/O, brick-map paging and hierarchy, `.ptc` and brick formats, `ptcapi`) are out of scope; only the ties to other domains change.
- `CBrickMapGeometry` is dead (declared, never defined, no use anywhere in the tree, present since the initial import); the maintainer approved deleting it, which fully resolves DB-020 here.
- The GI interfaces (record, store, photon store and estimator) may live in Infra; the plan places them, subject to FR-027's rule that Shading calls the store only through virtuals.
- The validation Linux host is x86_64 with no FMA and the build sets no `-march` or `-ffp-contract` option, which is why byte-identity against Linux master is expected (spec 029 measured 913 images, 0 differing apart from the #122 crash).
- The data viewer and the preview are Consumers and may depend on Texturing and Hiders headers; the adapters therefore live in `src/ri/dataviewer/`.
- Photon emission, tracing and shading keep their present code; `CPhotonHider`'s non-thread-safe `modifying` flag and the intermittent `CPhotonHider` crash recorded in `BUGS.md` are not touched.
- The key's bit width and the record-key definition are plan decisions within FR-017's constraint (pure function of existing identities, no random draw).
- The services interface is internal C++ (2.x, unstable); a signature change is allowed with a version step.
- Spec 040 does not start before this spec merges; 053 follows 040.
- Linux validation runs over ssh with the remote `cpujob` at `-j2` in a per-spec tree, as for earlier specs; every build and test goes through `~/.claude/bin/cpujob`.

## Overlap with other work (listed, not absorbed)

**Concurrent specs.**

- **030 (hider and integrator registry)**: its brief takes DB-020's "light friends". Spec 029 already removed the `CPhotonHider`, `CSphereLight` and `CQuadLight` friends (registry, 2026-11-04), so 030 has no DB-020 friend left; 032 resolves the rest. 030 must not touch `photon.cpp`'s store path.
- **031 (Geometry hider-agnostic)**: owns the three Geometry-side `*Dispatch`/`*DispatchStub` pairs (DB-017) and `surface.cpp`. The irradiance pair (`irradianceDispatch.cpp`, `vector/irradianceDispatchStub.cpp`) is deleted by this spec (it matches 031's Open Question 2, option A: 031 leaves the pair; the two answers must stay together). 031 deletes the `surface.cpp` include of `debug.h` and the dead `DEBUG_*` blocks; this spec does not edit `surface.cpp`. Both specs edit `ribvector-core-residual.txt` and the `rendererStatics.cpp` row; the second to merge resolves the textual conflict. DB-017 and DB-025 are moved to Resolved by whichever spec merges second (FR-021). 031 (its option P-Q3 (a)) also deletes allowlist lines 137-138 (`photonMap.cpp -> toWorld/fromWorld`), which this spec deletes in its photon phase:
  - whichever spec merges second drops its duplicate edit;
  - if this spec lands first, 031's `FrameParams`-based read in `photonMap.cpp` disappears with the file.

  Every other allowlist deletion of 031 and 030 is disjoint from this spec's.
- **056 (volume shaders)**: no overlap found; it must not add a Hiders-to-Texturing edge.

**Open Bug Tracker issues touching this code** (none absorbed; this spec claims to fix none).

| Issue | What | Overlap |
|-------|------|---------|
| #122 | Intermittent SIGSEGV in `CPointCloud::lookup` in `sphere-texture3d-bake3d-reyes` and `-slo` (P1; race suspected between `bake3d()` storing and `texture3d()` looking up; backtrace in the recursion over the hierarchy; unexplained) | This spec edits `pointCloud.h/.cpp` and `texture3d.h` (viewer base, friend, lookup signature). That changes the class layout and vtable and can shift the race's timing, hiding or exposing it without fixing it. So the `lookup`/`store` bodies stay unchanged (FR-007, FR-030), no before/after crash rate is a success criterion, no fix is claimed, a `bake3d` crash in validation is re-run in isolation, and the TSan investigation is left out of this spec (a bug-fix session may take #122). |
| #102 | `bake3d()` writes the `.ptc` in thread-completion order | 040-adjacent; the key of US5 does not apply to point clouds; not fixed. |
| #130, #131 | `ptcapi.h` signature mismatch; point cloud reads back fewer points | `ptcapi.*` and `pointCloud.cpp` are near this spec's edits; not touched. The plug-in ABI texture3d reference records the current behaviour and must stay unchanged. |
| #104 | One file name used for `texture()` and `environment()`/`shadow()` returns the wrong class from the cache | This spec now owns the GI entries of `frameFiles`; the same name-keyed hazard exists across the GI kinds. Not fixed; the plan records whether the GI entries are still keyed by name only. |
| #103 | `makeBrickMap` error calls with `%s` and no argument | `brickmap.cpp` is edited; not touched. |
| #145, #146, #97 | JIT `photonmap()` ignores `estimator`; `ambient()` in an imager derefs a null object; JIT ignores optional lists of `texture3d`/`bake3d`/`photonmap` | In Shading's GI builtins; the photon lookup contract (`estimator`) is reshaped only as FR-009 and FR-028 force it (the four-argument lookup), and parity is kept as it is. |
| #15 | `assert(numChannels == 3)` does not compile | Looks stale: `irradiance.cpp:70` already calls `numChannels()`. Reported for the maintainer to close; not this spec's task. |
| #79, #93 | Specs 040 and 053 | Consumers of this spec; US5 is the hook. |

Also: the unexplained intermittent `CPhotonHider` crash (`DEVNOTES_DETAILS/BUGS.md`; `photon-parity` crashed 4 of 30 at default threads on macOS) is why GI scenes keep `-t:1`; no pin is removed or validated away by the storage move.

## Out of Scope

- Making the photon map or the irradiance cache order-independent (spec 040); using the keys.
- Photon scattering and the irradiance cache for Bxdf materials (spec 053).
- The point-cloud, brick-map and `ptcapi` internals, formats and the bugs above.
- The hider registry and integrators (030), Geometry's dicer and ray interfaces and the other Dispatch pairs (031), volume shaders (056), the shading-system backends (033).
- Changing the sampler (039), the situation table, or any image.
