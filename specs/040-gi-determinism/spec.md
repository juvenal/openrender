# Feature Specification: GI Determinism

**Feature Branch**: `040-gi-determinism`

**Created**: 2026-11-13

**Status**: Draft

**Input**: User description: "Spec 040 (GitHub #79): order-independent photon map and irradiance cache. Photon emission keyed by global photon index, the photon map built from index-sorted photons. Irradiance-cache records created order-independently (a deterministic merge or a prepass, chosen in plan), cache acceptance drawing from the sample stream. Acceptance: GI and photon scenes bit-identical at `-t:1`, `-t:N` and `-P:N` with their thread pins removed; GI thresholds unchanged or tightened; overhead measured. It must hold the spec 022 architecture allowlist: no new allowlist lines. Depends on spec 032 (#38), spec 039 (#78) and #55 (all merged or closed)."

## Context

**Why.** Spec 039 made every non-GI render bit-identical at any thread or process count by keying each random draw to the identity of the sample that makes it. It left one class of scene out on purpose: a GI scene, in which some lookup reads a structure that is filled while the frame renders (the irradiance cache behind `occlusion()` and `indirectdiffuse()` with a non-zero `maxerror`, the default 0.6, and the photon maps). Those structures are filled in the order the points happen to be shaded, or the photons happen to be stored, so the image, the written files and the stored photons depend on thread timing. Spec 032 made the two structures passive storage in Texturing and gave every record and photon a deterministic key, in memory, "for spec 040", and read nothing with it. This spec is the consumer: it makes the contents of both structures, and so every GI image and file, a function of the scene alone.

**Where this spec sits.** Issue #79, plan row 040 (layer L4b, deferred track, parallel to the critical path). Dependencies, checked with git on 2026-11-13: spec 032 (branch `032-gi-cache-split`, merge `8f45d919`) and spec 039 (branch `039-sampling-foundation`, `12d271ed`) are both ancestors of master `278b0394`, and their `specs/` directories are present; #48 (closed 2026-10-17) and #55 (closed 2026-10-26) are closed. Spec 053 (#93, Bxdf GI) follows this spec and builds on its determinism. The architecture reference lists no decision (D-number) for 040; the adjacent ones are D1 (the photon hider is a hider: visibility, not transport), D26 (GI with `Bxdf` materials is spec 053, after 040) and D28 (every hider by integrator cell renders or fails with a diagnostic). None is changed here.

**Measured starting point** (read at master `278b0394`; the plan stage re-measures).

*What the keys already give.*

- An irradiance record carries `CIrradianceRecord::key = irradianceRecordKey(boundDraws())`, copied at the top of `libshader::irradianceLookup` (`irradianceCalls.cpp`); a photon's key is `photonKey(pointKey, bounce)` in a side array of `CPhotonStore` that `balance()` permutes with the items. Neither key is in any file, and nothing reads one (gotcha 37).
- Photon emission already draws from a sub-stream keyed by (light, index in the frame's sequence, frame, seed) (`photon.cpp`, spec 039 FR-023), so a given photon is the same photon on whatever thread traces it. What is not deterministic is what happens to it afterwards.

*Where the photon map depends on order* (`photon.cpp`, `photonStore.cpp`, `core/map.h`).

- Photons enter a map in the order threads store them (`CPhotonStore::store`, under the store's lock). `CMap::balance()` builds the kd-tree from that order, breaks ties between equal coordinates by it, and writes the splitting axis into each item's `flags`; the written file lists the items in tree order. `GiKeys_PhotonsThreads` already has to leave `flags` out of its comparison for this reason.
- A map is reset on the first photon that hits it, outside the lock: `if (map->isModifying() == FALSE) { setModifying(TRUE); resetPhotons(); balanceList.push(map); }` (`photon.cpp:633-639` and `:647-651`, with the comment "this isn't strictly thread safe"). With more than one thread, a second thread can reset a map that holds photons a third already stored. Photons are lost, and which ones depends on timing. `CMap::reset()` also frees its array with `delete` instead of `delete[]` (`core/map.h:156`, GitHub #165).
- `CPhoton` is 44 bytes of which 4 are padding, and `new T[]` leaves padding, `items[0]` and an empty map's dummy direction unassigned (spec 032, OQ-8). They are written to the `.pm` file as they are.
- A map lookup is **not** a function of the map and the query. `PHOTON_LOOKUP_CACHE` is defined (`photonStore.h:40`): `lookup(Cl, P, N, maxFound)` first asks `probe()` for an earlier estimate stored near the query and only then searches the photons, and stores its result (`insert(...)`, `photonStore.cpp:337-390`, `:417-455`). So the answer for a point depends on which nearby points were queried before it, by which thread. `searchRadius` is also a member that every lookup writes.

*Where the irradiance cache depends on order* (`irradianceCache.cpp`, `irradianceCalls.cpp`, `irradianceSampler.cpp`).

- **Which records exist.** A lookup sees the records inserted so far. A record is inserted when a lookup misses, so the set a given lookup sees is the set the earlier lookups of the render made, in the order they ran.
- **What a record is.** `insertRecord` runs `clamp()`, which shortens the radius `dP` of the new record and of its neighbours against each other, and places the record in the octree by the clamped radius (`irradianceCache.cpp:478-545`, `:551-600`). The radius a lookup reads and the node a record sits in therefore depend on the insertion order, even when the final set does not.
- **How a hit is summed.** Records of one node are a linked list with the newest first; the weighted sum visits them in that order and in tree order (`lookupCached`, `:350-470`), and floating-point addition is not associative.
- **Which draws a record gets.** The accept test of `lookupCached` calls the draw callback once per record that passes the front test, in traversal order, on the same cursor the sampler then uses on a miss (`irradianceCalls.cpp`, `drawNextOf`; gotcha 37: "the draw order is a rule"). So the *i*-th accept draw lands on a different record when the tree has a different shape, and the sampler's own draws (the hemisphere directions of the new record) shift by the number of records that passed the front test. The same point can therefore produce different records.
- **The 039 carry-over.** Spec 039 moved `occlusion()` onto the sampler only for `maxerror == 0` (`irradianceSampler.cpp:239-241`, T076c). `indirectdiffuse()` and cached `occlusion()` keep the `nt x np` jittered grid, because the position and rotation gradients assume it. Spec 039's `validation.md` (T076c and T076d, "Known gap (decision 4)") records that gap as 040's.
- **Files.** A cache read from a file lists each node's records in the reverse of the order they were written, so two reads of one file sum a lookup in another order (gotcha 37). A written cache lists records in tree order, which is insertion order.
- **Processes.** Under `-P:N` each server fills its own cache, and `CRemoteICacheChannel` sends the records to the client at the end of the frame (`remoteChannel.cpp:524-610`); the client places them by their raw radius, without the error multiplier the server applies (`irradianceCache.cpp:613`, "error adjustment!!"; GitHub #161). The records are not the problem; the receiving cache sees them late and in another tree.

*Open bugs in this code.*

- **#163 (P1, open)**: the photon hider writes a different global map from the same scene on different runs, **one thread included** (20 to 26 runs on two trees: 16 and 25 gave one result, 4 and 1 the other; 4 of 4 on Linux). Cause not investigated. With one thread, order cannot be the reason, which points at bytes that are not deterministic (the unassigned padding above is a candidate, not a finding) or at a nondeterminism the plan has to find. `Registry_IntegratorImageIdentity`'s photon rows (`none1 == none2`, `classic == none1`) fail intermittently because of it (passes 1 in 6 on macOS standalone).
- **#122 (P1, open)**: intermittent SIGSEGV in `CPointCloud::lookup` in the `bake3d` scene, Linux, under load; cause not established (a race between `bake3d()` storing and `texture3d()` looking up is an inference).
- **#160, #161, #164, #165 (open; #158, #159 and #166 too)**: all seven were closed between 06:30:40 and 06:30:43 UTC on 2026-11-13, each with a close event attributed to the spec 032 merge commit `8f45d919`. That was an error, not a fix: a GitHub closing keyword in the merge message matched a line written "not fixed: #N" as "fixed: #N". The maintainer reopened them on 2026-11-13, and they are open defects again. **The code at master `278b0394` still has #160, #161, #164 and #165**: `CPhotonStore::insert` starts from `root` with no null check (`photonStore.cpp:337`; only `probe()` checks, `:264`); `insertReceived` places by `dP` with no `/K` (`irradianceCache.cpp:613`); the unguarded reset in `photon.cpp:633-651`; `delete` in `CMap::reset` (`map.h:156`). The plan stage must still check the tree for a fix before it assumes one (see Overlap).

## User Scenarios & Testing *(mandatory)*

The "users" are render users and test authors (who need a GI scene to render the same image, and write the same files, every time), the maintainer (who needs the thread pins and the exceptions gone), shader authors (whose `occlusion()`, `indirectdiffuse()` and `photonmap()` calls must keep meaning what they mean), and the author of spec 053.

### User Story 1 - A photon map is the same on every run (Priority: P1)

A render user runs a photon scene twice, with one thread and with many, and gets the same photons in the same order in the same map files. The photon rows of `Registry_IntegratorImageIdentity` stop failing.

**Why this priority**: it is GitHub #163 (P1) and the smallest part of the acceptance line. It needs no change to the sampler and it unblocks the registry test.

**Independent Test**: render the photon scenes (`photon-parity`, `-slo`, `subdiv-*-photon`, `motion-subdiv-translate-photon`, `sphere-photonmap-*`) at `-t:1` twice, at the default thread count and at `-t:4` and `-t:8`, each 30 times in separate processes, and compare the written map files byte for byte. All are identical within a platform.

**Acceptance Scenarios**:

1. **Given** a scene with `Hider "photon"` and a global map, **When** it renders 30 times at `-t:1` as separate processes, **Then** every `.gpm` (and caustic map) file is byte-identical to the first. (#163's reproduction, which gave two different results in 20 to 26 runs.)
2. **Given** the same scene at `-t:1`, `-t:2`, `-t:4`, `-t:8` and the default, **When** each map file is compared, **Then** the files are byte-identical across the thread counts: the same photons, in the same order, with the same balanced layout, including each photon's `flags`.
3. **Given** a map that more than one thread stores into, **When** the frame ends, **Then** it holds exactly the photons the scene emits to it, none lost to a reset that another thread ran.
4. **Given** `Registry_IntegratorImageIdentity`, **When** it runs 30 times standalone on each platform, **Then** its photon rows pass every time.

---

### User Story 2 - A cached GI scene renders the same image at any thread or process count (Priority: P1)

A render user renders a scene with `occlusion()` or `indirectdiffuse()` and the irradiance cache on, at `-t:1`, `-t:N` and `-P:N`, interpreter and JIT, under each camera hider, and gets the same image, bit for bit. The `numthreads 1` pins on these scenes are gone.

**Why this priority**: it is the headline acceptance line of #79 and the reason spec 039 left GI out of its guarantee.

**Independent Test**: the GI scenes of the determinism sweep (see Requirements) render under the sweep's configurations (`-t:1`, the default, `-t:2`, `-t:4`, `-P:2`), compared in unquantized form (colour, alpha, depth, every AOV) plus the written cache and map files; all are identical within a platform.

**Acceptance Scenarios**:

1. **Given** `sphere-occlusion-reyes` and `sphere-indirectdiffuse-reyes` and their `-slo` variants with the `numthreads 1` option lines removed, **When** each renders at `-t:1`, `-t:2`, `-t:4`, the default and `-P:2`, **Then** the float output of every configuration is bit-identical.
2. **Given** the same scenes under the zbuffer and raytrace hiders (copies written for this spec if no scene exists), **When** they render under the same configurations, **Then** the same holds.
3. **Given** a record created at a shading point, **When** the frame renders, **Then** its content (irradiance, coverage, environment direction, gradients, radius after clamping) and its place in the cache do not depend on which points were shaded before it, by which thread or process.
4. **Given** a cache whose lookups are repeated in a different insertion order of the same record set, **When** a query is evaluated, **Then** the result is bit-identical (the sum does not depend on the order the records were inserted).
5. **Given** a render where a point is shaded more than once (a lazily shaded grid, a displacement estimate, a derivative batch that repeats a point), **When** it reaches the cache, **Then** it creates one record, not two, and the result does not depend on which shading came first.

---

### User Story 3 - A written GI file is reproducible and reads back as it was built (Priority: P2)

A pipeline that writes a photon map or an irradiance cache in one frame and reads it in the next, or that diffs the files between runs, gets the same file every time, and a lookup on the reloaded data equals the lookup on the data in memory.

**Why this priority**: the files are the GI output that survives the frame, and #163 is a file comparison. It is independent of the cache-visibility scheme.

**Independent Test**: each GI test scene that writes a map or cache is rendered at several thread counts; the files are byte-identical; the file is read back and a fixed set of queries is run on the in-memory structure and on the reloaded one with equal results.

**Acceptance Scenarios**:

1. **Given** a photon map file, **When** it is written, **Then** every byte is defined (padding, the unused first item and an empty map's dummy photon included) and identical across runs.
2. **Given** an irradiance cache file, **When** it is written, **Then** its records appear in a canonical order that is a function of the scene alone, and reading it back gives lookups bit-identical to those of the cache before writing (the "reverse of the written order" difference of gotcha 37 is gone).
3. **Given** a photon map filled in memory and never read from a file, **When** it is queried, **Then** the lookup works and gives the same answer as after a write and reload (the null lookup-cache root of #160 cannot be reached).
4. **Given** files written by the tree before this spec, **When** they are read, **Then** they load and answer lookups (the file formats are unchanged unless the plan shows one must change; a change is a decision for the maintainer, not a side effect).
5. **Given** a frame that reads a cache and extends it (`CACHE_READ` with `CACHE_WRITE` and `CACHE_SAMPLE`), **When** it renders at any thread count, **Then** records read from the file (which carry no key) and records created in the frame both land in a deterministic order.

---

### User Story 4 - A cache miss makes the same record whatever the cache held (Priority: P2)

A shader author who calls `occlusion()` or `indirectdiffuse()` gets the same sample directions for a given shading point whether the cache was empty, half full or full, on the interpreter and on the JIT, and `indirectdiffuse()` draws its samples from the keyed sampler as `occlusion()` has since spec 039.

**Why this priority**: without it the record a point creates depends on the cache's contents, so no scheme for which records exist can be deterministic. It also closes spec 039's recorded gap.

**Independent Test**: a probe renders the same point with a cache holding 0, 10 and 1000 other records and compares the record it creates (the bit pattern of every field); a second probe counts the rays `indirectdiffuse()` traces for a given `samples` and compares both backends.

**Acceptance Scenarios**:

1. **Given** a point that misses the cache, **When** the sampler runs, **Then** its draws are a function of the point's identity alone: the number of records the lookup visited or accepted changes nothing.
2. **Given** a record's accept decision for a query (the weighted-discard test of the lookup), **When** the same query meets the same record on any thread, in any traversal order, **Then** the decision is the same, and the decisions of other records do not change it.
3. **Given** `indirectdiffuse()` with the cache off, and `occlusion()` and `indirectdiffuse()` with it on, **When** they sample the hemisphere, **Then** every direction and origin jitter comes from the keyed sampler on both backends; where the gradients need a stratified grid, the grid stays and its jitter is keyed. `.slo` and `.rslo` renders of the scene agree as they do today.
4. **Given** a point-based lookup (a point hierarchy in place of the cache), **When** it runs, **Then** it draws nothing, as today.

---

### User Story 5 - The maintainer can see what determinism costs and that nothing else moved (Priority: P3)

The maintainer sees the render-time overhead of the deterministic GI on scaled scenes, the list of GI references that changed and why, the thread pins that went, the ones that stayed and why, and the allowlist unchanged.

**Why this priority**: the issue asks for the overhead to be measured, and a deterministic scheme can cost real time (a serialised prepass, a record merge). The rest is the record that keeps the spec honest.

**Independent Test**: the scaled GI scenes render in interleaved series against the starting tree; the spec directory holds the numbers, their noise, the changed-references list, the pin table and the architecture report.

**Acceptance Scenarios**:

1. **Given** the scaled GI scenes (occlusion, indirectdiffuse with a photon map, a photon pass), **When** they render on the finished tree and on the starting tree in interleaved series, **Then** the median times and their noise are reported per scene, on macOS and on Linux, thread count and host stated.
2. **Given** the finished tree, **When** `ctest -L architecture` runs, **Then** it passes, the allowlist has no line added, and no Hiders-to-Texturing edge exists; Texturing still includes only Texturing and Infra headers.
3. **Given** the pins of spec 039's `thread-pins.md`, **When** the spec ends, **Then** each is removed or kept with a stated reason; none is removed by widening a tolerance.

---

### Edge Cases

- **An empty map, a map of one photon, a photon stored in both a global and a caustic map, two maps of one name, a map read in a second frame** all keep working and keep the properties above.
- **The first photon of a frame** is the one that resets a map today; the reset must not depend on which thread reaches the map first.
- **Equal keys or coincident records**: two photons with equal coordinates, two records at the same position, or a key collision need a total order that does not depend on arrival; the keys are hashes, and 032's key test checked uniqueness on one scene only.
- **A cache with `CACHE_RDONLY`** inserts nothing; a cache with no `CACHE_SAMPLE` draws nothing on a miss. Both stay as they are, and the second still consumes no draws.
- **Records read from a file have no key.** Whatever order and tie rule the plan picks must treat them as the earlier, fixed part of the cache.
- **One bucket, more threads than buckets, a thread that gets no work, servers with different thread counts**: the results are the same.
- **A scheme that serialises the render** (a single-threaded prepass over the whole frame, a global lock on every lookup) would satisfy determinism and defeat the purpose of the threads; it must be measured (User Story 5), not hidden.
- **The sample-stream rule** (spec 039): adding or removing a draw at one call site must not change a value drawn at another. The accept draws must not shift the sampler's draws, and the sampler's must not shift the accept draws.
- **Per-platform determinism.** The guarantee is identical output across configurations on one build and platform. A build that contracts multiply-adds differently per configuration is not a case; macOS and Linux are compared to themselves, never to each other.
- **Mixed hiders.** The photon hider and the camera hiders use the same maps and cache in one session (photon pass, then render); the property holds across the two passes.

## Requirements *(mandatory)*

### Functional Requirements

**Photon map**

- **FR-001**: The contents of a photon map after a photon pass (which photons, in which order, with which balanced layout and `flags`) MUST be a function of the scene, the options and the seed alone, identical across repeated runs, thread counts and, where the photon pass runs under `-P:N`, process counts.
- **FR-002**: A photon's place in a map MUST NOT depend on the order threads stored it. The photons are ordered by an identity that is a function of the photon (light, index in the frame's sequence, bounce) before the map is balanced, and ties are broken by a rule that does not depend on arrival. The identity is unique within a map, or a documented tie-breaker makes the order total.
- **FR-003**: A map MUST be reset, and its ownership by the frame established, exactly once and without a race: no photon stored by one thread may be discarded by another thread's reset, and which thread reaches the map first MUST NOT matter. The unguarded `isModifying()`/`resetPhotons()` sequence and `CMap::reset()`'s `delete` (GitHub #165) MUST NOT survive in a path this spec relies on.
- **FR-004**: Every byte a map file contains MUST be defined and a function of the scene, including padding, the unused first item and an empty map's dummy photon. A test compares the files byte for byte, not field by field.
- **FR-005**: A photon map lookup MUST be a function of the map and the query: independent of the lookups that ran before it, of the thread, and of any estimate memoised by an earlier query. Whether the existing lookup memo (`PHOTON_LOOKUP_CACHE`) is removed or replaced by something deterministic is a plan decision, whose effect on every photon-reading image and on cost is recorded. A lookup MUST NOT write state the other lookups read (`searchRadius`).
- **FR-006**: The cause of #163 MUST be established with evidence in the plan or the implementation record (what differs between two runs, byte by byte, at one thread), and the 30-run test of User Story 1 MUST pass on both platforms. If the cause is found and fixed on master before this spec merges, the test stays as this spec's regression guard.

**Irradiance cache**

- **FR-007**: The set of records in a cache at the end of a frame, and each record's content, including its radius after neighbour clamping, MUST be a function of the scene alone, identical across repeated runs, thread counts and process counts.
- **FR-008**: The set of records a lookup can see, and their clamped radii at that moment, MUST be a function of the scene and the query, not of timing: not of which other points were shaded before it, on which thread or process. A scheme that only merges per-thread record sets after the frame does not satisfy this, because lookups happen during the render; the plan chooses the mechanism (a deterministic prepass, a canonical visibility rule, or another) and states how it satisfies this requirement and the one below.
- **FR-009**: A lookup's result MUST be bit-identical for the same record set whatever order the records were inserted, including the order of the floating-point sum, and a record's placement and clamped radius MUST NOT depend on insertion order.
- **FR-010**: A record's accept decision for a query MUST depend only on the query's identity and the record's identity (its key), not on traversal order, and MUST NOT shift any other draw. Recording that the accept draws now have a sub-stream of their own changes gotcha 37's rule ("the draw order is a rule") and is a documented, maintainer-approved change, not a side effect.
- **FR-011**: The record a miss creates MUST depend only on the point's identity and the scene, not on the cache's contents at the time: the sampler's draws MUST NOT shift by the number of records the lookup visited or accepted.
- **FR-012**: `indirectdiffuse()` with the cache off, and `occlusion()` and `indirectdiffuse()` with the cache on, MUST draw their directions and origin jitter from the keyed sampler on the interpreter and the JIT (spec 039's recorded gap, T076c decision 4). Where the gradient computation requires a stratified grid, the grid MAY stay with keyed jitter. The point-based branch keeps drawing nothing.
- **FR-013**: A point shaded more than once MUST create one record, and the record MUST NOT depend on which of its shadings reached the cache first (User Story 2, scenario 5).
- **FR-014**: A written cache MUST list its records in a canonical order that is a function of the scene alone, and a cache read back MUST give lookups bit-identical to those of the cache that was written (the reverse-order difference of gotcha 37 removed). Records read from a file carry no key and are the fixed part of a cache that is extended.
- **FR-015**: A cache that is sent between processes (`-P:N`) MUST give the same image and files as the same render in one process. The receiving cache MUST place a received record where the order-independent rule places it, so that the receiver's lookups equal the sender's (this subsumes the placement difference of #161 for the cache this spec builds; see Overlap). `-P:N` for the cached irradiance scenes is in scope (Clarifications, 2026-11-13): the mechanism of FR-008 MUST NOT assume one address space.

**Determinism harness, pins and references**

- **FR-016**: The 18 GI scenes listed as comments in `tests/visual/determinism-scenes.cmake` ("GI scenes: not swept (spec 040)") MUST be added to the determinism sweep, with the `numthreads 1` option lines of the occlusion, indirectdiffuse and photonmap scenes (and their `-slo` variants) removed. The comparison MUST be in unquantized form (colour, alpha, depth, every AOV) and MUST include the map and cache files the scenes write. The cache's per-pixel interpolation magnifies small differences, so an 8-bit comparison is not enough.
- **FR-017**: Every pin kept or removed MUST be recorded with its reason (spec 039's `thread-pins.md` is the starting inventory). The pins that exist because the cache fills in shading order (`sphere-occlusion-reyes`, `sphere-indirectdiffuse-reyes`, each with `-slo`; spec 039's `thread-pins.md` says so) MUST be removed, and so MUST the `sphere-photonmap-reyes` pair's pins unless the plan finds they isolate something else, which it then records. The pins that exist because the photon pass once crashed under threads (`photon-parity`, `photon-parity-slo`, `test_photon_solar0`'s `-t:1`) MUST be removed only if the crash is shown to be gone by a repeated multithreaded run of the same scenes, and otherwise stay with their reason. The spec 039 probes that are single-threaded by design (`sampling-*.rib.in`) keep their pins.
- **FR-018**: A GI reference image or test baseline that changes because the cache or the map is now deterministic MUST be re-rendered once, in one documented step, with the list and the reason for each recorded in the spec directory (the pattern of spec 039, FR-034). Cached GI images at `-t:1` are NOT required to stay byte-identical to master (Clarifications, 2026-11-13): the references change once, in that step. No `MaxBlockAvgDiff` or other test threshold MAY be widened; thresholds are unchanged or tightened where the measured margin allows. No test is disabled.
- **FR-019**: `Registry_IntegratorImageIdentity`'s photon rows MUST pass reliably (User Story 1, scenario 4).

**Boundaries, documentation and cost**

- **FR-020**: This spec MUST add no allowlist line and no cross-domain include or link edge; Texturing MUST still include only Texturing and Infra headers; Hiders MUST NOT include a Texturing header; the cache step stays Shading's single function reached by both backends (gotcha 37). Anything that joins Texturing storage to the sampler MUST be an interface owned by Infra or Shading. `ctest -L architecture` MUST pass on macOS and Linux.
- **FR-021**: The services version (`kRendererServicesVersion`) MUST step with a history entry if a services member changes (a MINOR step may break within 2.x, saying so).
- **FR-022**: No file format changes unless the plan shows it is needed and the maintainer approves; files written by the tree before this spec MUST still read (User Story 3, scenario 4). The wire record keeps its size unless the plan shows otherwise.
- **FR-023**: The render-time overhead of the deterministic GI MUST be measured against the starting tree on scaled GI scenes (an occlusion scene, an indirectdiffuse scene with a photon map, a photon pass), in interleaved series on one host and thread count, with the noise recorded, on macOS and on Linux, and reported. This spec sets no pass/fail percentage (see the clarification at the end).
- **FR-024**: `CLAUDE.md` (gotcha 37 and the GI stores section, and a Recent Changes entry), `DEVNOTES.md` and the registry history MUST record the change. The GI part of `determinism-scenes.cmake`'s header ("Spec 040 makes them deterministic") MUST be updated. Every item is done inside this spec's branch (clarified 2026-11-15: `CLAUDE.md` and `DEVNOTES.md` are written as drafts in this spec's directory that the coordinator applies at merge preparation, as for every spec; the other items are edited in the branch); no task depends on the merge, and issues close through a `Closes #NN` line the maintainer writes in the merge commit.

### Key Entities

- **GI key**: the deterministic identity of an irradiance record or a photon, from spec 032 (`irradianceRecordKey`, `photonKey`). In memory only until this spec decides otherwise (FR-022).
- **Photon identity**: (light, index in the frame's photon sequence, bounce), from which the photon's key derives; what orders a map (FR-002).
- **Irradiance record**: a position, a normal, irradiance, coverage, environment direction, translational and rotational gradients, a validity radius and a key.
- **Record visibility**: the property of FR-008, which records a lookup can see and in what state.
- **Accept decision**: the weighted-discard test that decides whether a candidate record joins a lookup's sum (FR-010).
- **GI scene**: spec 039's FR-031 definition: a scene in which a lookup reads a structure filled during the render (the irradiance cache with `maxerror` other than 0, a photon map). The sweep scenes of FR-016 are the GI scenes this spec covers.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: Each GI scene of FR-016, rendered under the sweep configurations (`-t:1`, the default, `-t:2`, `-t:4`, `-P:2`), produces a bit-identical result in every unquantized channel and in every map and cache file it writes, on macOS and on Linux, each platform against itself, in 3 of 3 repetitions of the sweep.
- **SC-002**: Photon map files of the photon scenes are byte-identical across 30 of 30 separate-process runs at `-t:1` and across `-t:1`, `-t:2`, `-t:4`, `-t:8` and the default, on both platforms; `Registry_IntegratorImageIdentity` passes 30 of 30 standalone on both.
- **SC-003**: A probe that varies only the number of other records in the cache (0, 10, 1000) finds the record created at a fixed point bit-identical in every case, on `.rslo` and `.slo`.
- **SC-004**: A record set inserted in 100 random orders gives a bit-identical lookup for each of a fixed set of queries, and bit-identical placement and clamped radii.
- **SC-005**: A cache and a map each written and read back give lookups bit-identical to the in-memory ones; a map filled in memory and never read can be queried.
- **SC-006**: Every thread pin of FR-017 that this spec removes is gone; the ones kept are listed with reasons; no tolerance, threshold or reference is widened or disabled, and every re-rendered reference is on the list of FR-018.
- **SC-007**: The overhead of FR-023 is measured and reported for the three scaled scenes on both platforms, with noise, and the maintainer has the numbers before the spec is closed.
- **SC-008**: `ctest -L architecture` passes on both platforms with no allowlist line added and `ctest -L gi_split`, `-L shading_parity`, `-L texture_only`, `-L state`, `-L plugin_abi`, `-L link` and the everyday full run (`-LE determinism-sweep`, which since the plan of 2026-11-14 reads `-LE 'determinism-sweep|gi_determinism_long'`: the 30-run tests carry their own label and run explicitly) pass with no new failure against master, known intermittent tests (#122 and #127's and #133's families) re-run in isolation before they are judged.
- **SC-009**: No `[NEEDS CLARIFICATION]` marker remains in this file when the plan starts, and no requirement, task or criterion depends on a merge (FR-024).

## Assumptions

- Spec 039's sample identity and sub-streams are the source of every random value; this spec adds no random source and does not change the sampler. The pre-existing per-platform rule holds: bit-identical means across configurations on one build and platform.
- The plan chooses the mechanism by which the irradiance cache is made order-independent (issue #79: "deterministic merge or prepass, chosen in plan") and the way the photon map is canonically ordered, subject to FR-008 and FR-010. The spec states properties, not methods.
- `maxerror` stays an option the user sets; this spec adds no new RIB option or token unless the plan shows one is unavoidable, in which case all four option layers apply (CLAUDE.md, Attributes/Options).
- GI under the camera hiders is covered: reyes, zbuffer and raytrace, where their shading reaches `occlusion()`/`indirectdiffuse()`. The photon hider has no comparable image, so parity (spec 039 and the hider-parity rule) never uses it; its output here is the map file.
- Whether the photon pass runs under `-P:N` today is not established; the plan measures it. Where it does not, FR-001's process-count clause is vacuous.
- The validation host for Linux is the Ubuntu 24.04, GCC 13 host used by earlier specs, reached over ssh with the remote `cpujob` at `-j2` in a per-spec tree. Every build and test runs through `~/.claude/bin/cpujob`; the determinism sweep runs at `-j2`.
- Scene comparison for this spec is in unquantized form (floating-point colour, alpha, depth and AOVs), as the plan brief requires.
- The point-cloud, brick-map and `ptcapi` internals are not changed (see the third clarification and Out of Scope).

## Overlap with other work (listed, not absorbed)

**Bug Tracker issues in this code.**

| Issue | What | This spec |
|-------|------|-----------|
| #163 | The photon map differs from run to run, one thread included | **Meant to be fixed by this spec** as a consequence of FR-001, FR-004 and FR-006. **Risk:** another session runs a bug-fixing agent against master, so #163 (and #122) may be fixed there first; the plan re-reads the issue and the tree, and if a fix has landed, this spec keeps its test and drops its duplicate edit. |
| #122 | Intermittent SIGSEGV in `CPointCloud::lookup` (`bake3d`, Linux) | **Must not be touched**: not a GI cache, no claim of a fix, no ThreadSanitizer work here. The two `bake3d` scenes stay in the sweep as they are; a crash in them is re-run in isolation and not judged a regression. Same overlap risk with the bug agent. |
| #160 | A photon map filled in memory has a null lookup-cache root | **Open (wrongly closed by the 032 merge message, reopened 2026-11-13), still in the code** (see Context). FR-005 and User Story 3 (scenario 3) make the in-memory query work as a consequence of the lookup being redefined; a separate fix on master would conflict in `photonStore.cpp`. The plan decides whether this spec carries the one-line guard. |
| #161 | A client places received records without the server's error multiplier | **Open (reopened 2026-11-13), still in the code.** `-P:N` bit-identity (FR-015) cannot hold while the receiver places by another rule, so this spec's placement rule for received records replaces it, for the cache this spec builds. A separate fix of `insertReceived` conflicts. |
| #164, #165 | Photon hider SIGSEGV with threads; `CMap::reset()` `delete`/`delete[]` | **Open (reopened 2026-11-13), still in the code.** FR-003 depends on a race-free reset and store; `278b0394` has no fix for either. A bug-fixing agent may fix them on master before this spec merges; this spec edits the same functions, so a separate fix on master would conflict, and the plan re-reads the tree before it relies on either state.
| #158, #159, #166 | Reopened together with the above (same mistaken close) | Not examined by this spec; listed so that nobody reads them as fixed. Not touched here. |
| #102 | `bake3d()` writes its `.ptc` in thread-completion order | Adjacent; **out of scope** (maintainer, 2026-11-13). Tracked in #102. |
| #127, #133 | A data race in REYES grid shading (TSan); `ColorDeterminism_transparent` intermittent | Not this spec's, but each can break a bit-identity comparison of a GI scene under reyes at `-t:N` for a reason that is not this spec's. A red GI sweep entry is investigated for them first and re-run in isolation; it is never answered by a looser comparison. |
| #145, #146, #97 | JIT `photonmap()` ignores `estimator`; `ambient()` in an imager derefs a null object; JIT ignores optional lists of `texture3d`/`bake3d`/`photonmap` | Must not be touched. The `.slo` and `.rslo` photonmap renders differ for #145's reason and are compared as they are. |
| #131 | A point cloud reads back fewer points than written | Must not be touched. |

**Concurrent specs** (all unmerged; this spec does not design around their code, only names the files that can collide).

- **033 (shading-system backends)** rewrites `CShader` and the shading engine's shader loading, group API and JIT message passing. Likely collisions: `libshader/shading/shading.{h,cpp}` (this spec adds a member to `CShadingState` and does not edit `jitOcclusionBatch`: that function calls `bindDraws` and `irradianceLookup`, and the design needs no edit there), `irradianceCalls.cpp`, and `rendererServices.h` (both specs step `kRendererServicesVersion` and add a history entry; 040 takes the next free MINOR at the time of its edit). Merge order matters for the version number and for the textual conflicts in `shading.{h,cpp}` and `irradianceCalls.cpp`.
- **048 (OIIO image I/O)** is image decode and display work; code collisions are unlikely. Shared files that collide textually: `tests/visual/CMakeLists.txt`, `tests/visual/determinism-scenes.cmake`, `CLAUDE.md`, `DEVNOTES.md`, the architecture registry. Low risk.
- **057 (area-light geometry)** changes how a bound area light runs and adds an emitter registry. The photon hider emits by running each light's `illuminate` hook, and the photon identity includes the light index (`photon.cpp`); a change to how lights are enumerated or run would change the key's inputs. Photon-emission scenes with area lights and the `quadlight`/`spherelight` shaders are the overlap. Medium risk; whichever merges second re-checks the other's sweeps.
- **Already merged, no overlap**: 030 (registry), 031 (Geometry interfaces), 056 (volume shaders); 031's `surface.cpp` and the `photonMap.cpp` allowlist edits are already in the base.

## Out of Scope

- Photon scattering and the irradiance cache for `Bxdf` materials (spec 053, D26).
- A new sampler, a new sampling option or a change of the sampler's backends (spec 039).
- Point-cloud, brick-map and `ptcapi` internals, formats and bugs (#102, #122, #130, #131); the `bake3d()` `.ptc` byte order (#102) is explicitly out of scope (Clarifications, 2026-11-13).
- Fixing the unrelated bugs of the Overlap table, and the crash recorded in `DEVNOTES_DETAILS/BUGS.md` beyond what FR-003 and FR-006 require.
- Making a GI image equal to the starting tree's: it will change, once, under FR-018.
- Any change to the hiders' camera sampling, the integrators, the light service or the shading system.

## Clarifications

### Session 2026-11-13

- Q: Must the cached GI images at `-t:1` stay byte-identical to master, or may they change once? -> A: They may change once. Cached GI images at `-t:1` are not required to stay byte-identical to master; the GI references are re-rendered once, in one documented step with the list and the reason for each recorded in the spec directory (spec 039's FR-034 pattern). No threshold is widened and no test is disabled. Encoded in FR-018 and Out of Scope. The scheme is therefore free to choose its own canonical order and need not reproduce today's `-t:1` record or photon order.
- Q: Is `-P:N` for the cached irradiance scenes in scope? -> A: Yes. FR-015 and SC-001 stand as written, and the mechanism of FR-008 must not assume one address space (`CRemoteICacheChannel`, and the receiving cache's placement of records, are part of the work).
- Q: Is the `.ptc` byte order of `bake3d()` (#102) part of this spec? -> A: No. Out of scope; tracked in #102. The two `bake3d` scenes stay in the sweep as they are (Overlap, #122).
- Correction to the record (not a question): #158, #159, #160, #161, #164, #165 and #166 were closed on 2026-11-13 by a GitHub closing keyword in the 032 merge message ("not fixed: #N" read as "fixed: #N"), not by a fix, and are reopened. The Context and Overlap table say so; the defects are still in the code.

No `[NEEDS CLARIFICATION]` marker remains.

### Decisions the plan must present for approval (not questions for clarify)

These are consequences of the requirements that change documented behaviour. The plan lists each with options and a recommendation.

- Moving the accept draws to a sub-stream of their own changes gotcha 37's "the draw order is a rule" (FR-010).
- Whether the `PHOTON_LOOKUP_CACHE` lookup memo is removed or made deterministic (FR-005), with its effect on every photon-reading image and on cost.
- Ordering photons before `balance()` makes `flags` deterministic, so `GiKeys_PhotonsThreads` could stop leaving it out. That tightens a test the maintainer approved on 2026-11-11, and needs approval.
- Any file or wire format change (FR-022).
