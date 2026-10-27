# Feature Specification: Texture System

**Feature Branch**: `027-texture-system`
**Created**: 2026-10-26
**Status**: Draft
**Input**: Constitution Principle VIII remediation, layer L2, after spec 023. Give Texturing its own texture system: opaque texture handles, a by-value lookup option block, per-thread data, and ownership of the cache, eviction and paging state that today lives in the renderer core. Lookups stop taking the shading context. Shading reaches textures only through its services. The Radiance (RGBE) codec moves to shared infrastructure. Resolves registry entry DB-026 and the Texturing part of DB-010 (GitHub #33).

## Clarifications

### Session 2026-10-27

- Q: Where does this spec stop and spec 032 start (GI boundary)? → A: Option A. 027 moves the 2D, environment and shadow lookups, the point hierarchy's options, the tile cache and the texture-family resource cache. `CTexture3d`'s lookup and constructor signatures, and the photon-map, irradiance-cache and 3D-texture entries of the name-keyed cache (`frameFiles`), stay for spec 032. This keeps the 8 Hiders-origin DB-010 allowlist keys valid (FR-014, FR-019).
- Q: What happens to Texturing's reach into the channel registry (DB-021)? → A: Option A, with the maintainer's explicit sign-off: leave the code as spec 025 leaves it (out-of-line `CRenderer::retrieveDisplayChannel` forwarder) and re-tag the two `texture3d.cpp` allowlist lines (the `retrieveDisplayChannel` link and the `renderer.h` include) from DB-010 to the still-Open DB-021. Net zero lines; this is an approved exception to "no line re-keyed". DB-021 stays Open for spec 037 (FR-015, FR-019).
- Q: Which of the concurrency bugs does this spec absorb? → A: #20 only (the unlocked find-load-insert sequence of the resource cache, in the cache being rewritten). #19 stays out (needs its own ThreadSanitizer investigation; eviction changes would blur byte identity). #21 stays out (process-wide globals). #45 and #46 are not absorbed. The fix keeps renders byte-identical, has its own test that fails first, and the maintainer's merge message carries `Closes #20` (FR-016, FR-018).
- Q: Who defines the texture part of the services interface? → A: This spec defines it; spec 026 adopts it (confirmed assumption).
- Q: What is the texture system's lifetime? → A: Per frame (`beginFrame`/`endFrame`), preserving cache and unload timing (confirmed assumption).
- Note: the `brickmap.cpp` format-string defect near lines 1706/1710 is filed as GitHub #103 and is out of scope for this spec.
- Maintainer decisions for the plan (2026-10-27):
  - Q: Which allowlist lines does FR-015's re-tag cover? → A: Every platform variant of the two `texture3d.cpp` lines (the `retrieveDisplayChannel` link and the `renderer.h` include), Linux-only counterparts included, is re-tagged DB-010 → DB-021 the same way.
  - Q: What does a lookup do with no texture system installed? → A: The services return a null handle and the builtin returns its fill value, with one warning per kind of lookup (FR-002, FR-012).
  - Q: Where does the RGBE codec land? → A: In `libRIBCommon`. The `rgbe` display driver links the shared copy (gotcha 22: one definer of process-wide state). The plan settles what the `Common_StateOwnership` check lists for it.
  - Q: Are the starting numbers trusted as measured? → A: No. The plan's first measurement task re-measures them on a fresh macOS build, and lists the Linux run for the maintainer's rsync + Maestri path.
  - Q: How is #20's concurrent first-load test run? → A: 4 threads, 200 repeats, plain build (no ThreadSanitizer, which stays with #19), registered like the `texture_tile` tests and run only through `cpujob` (nice 19), at `-j2` for that label.

## User Scenarios & Testing *(mandatory)*

The "users" of this feature are the people who build, maintain and extend openRender: contributors working in the Texturing and Shading domains, maintainers reviewing remediation specs, developers who embed or link the texture code (point-cloud API, `otexmake`, the data viewer), and anyone rendering scenes that use texture, environment, shadow and 3D-texture lookups.

### Measured starting point (2026-10-26, master `bc261b1`, macOS)

Method: `archcheck --format json` against the main checkout's existing macOS build tree, `PASS`, zero violations, zero stale allowlist lines. That build is dated 2026-10-24 18:21, earlier than `bc261b1`; the only later commits that touch `src/` or `tests/architecture` are two JIT fixes under `src/libshader` (`6e52c3f`, `76e89a5`), and nothing in Texturing, the renderer core or the hiders. The numbers below are therefore master's. They must be re-taken on a fresh macOS build, and once on Linux, before the plan relies on them.

- **Texturing-origin allowlist edges: 37** (macOS counts; Linux adds one extra line per platform-dependent edge):
  - **DB-010: 27** (13 include, 14 link). Includes: `renderer.h` from `brickmap.cpp`, `texmake.cpp`, `texmake.h`, `texture.cpp`, `texture3d.cpp`; `options.h` from `brickmap.h`, `pointCloud.h`, `pointHierarchy.h`, `texmake.h`; `shading.h` from `pointHierarchy.cpp` and `texture.cpp`; `attributes.h` from `texture.h`; `xform.h` from `texture3d.h`. Links: `CRenderer::locateFile` (3 files) and, all from `texture.cpp`, `numThreads`, `texturePath`, `textureMutex`, `synthesizeMutex`, `textureMaxMemory`, `textureRefNumber`, `textureUsedBlocks`, `textureUsedMemory`, `textureCacheEnabled`, `textureCacheDirectory`; plus `CRenderer::retrieveDisplayChannel` from `texture3d.cpp`.
  - **DB-021: 1** (include of the channel-registry header from `texture3d.cpp`).
  - **DB-025: 6** (1 include, 5 links to `CDataView` from `texture3d.h`, `brickmap.cpp`, `pointCloud.cpp`). Owned by spec 032.
  - **DB-026: 3 edges, 5 allowlist lines** (1 include and 2 links on macOS, and the two links again with Linux's `FILE` mangling): the RGBE decoder's use of the display driver's RGBE codec.
- **DB-010 is 35 lines in all.** The other 8 originate in the Hiders (`irradiance.cpp`, `irradiance.h`, `irradianceDispatch.cpp`: 5 includes of `texture.h`, `texture3d.h` and `pointHierarchy.h`, and 3 links naming `CTexture3d`'s constructor, destructor and type information). They are the GI part left to spec 032.
- **Renderer-core usage metric for Texturing: 70 references, 16 distinct members** (baseline file, confirmed by the same run). The members are the cache statics (`textureRefNumber`, `textureUsedBlocks`, `textureUsedMemory`, `textureMaxMemory`), `numThreads`, `locateFile`, `texturePath`, the two mutexes, the two disk-cache options, `retrieveDisplayChannel`, and the loader entry points defined in `texture.cpp`; the plan lists the 16 exactly from archcheck's own output.
- **Lookups take the shading context.** All 2D, environment, shadow and deep-shadow lookups, and the 3D texture family's base lookup, take `CShadingContext*` and read from it: the current shader's texture parameters (filter, blur, width, fill, samples), its thread index and per-thread arena, and its sample-draw cursor (`drawNext2`/`drawNext4`, spec 039). The point hierarchy also reads the occlusion solid-angle limit from it.
- **The cache and paging state is renderer-core state mutated by Texturing.** `CRenderer` holds the tile-block list, the per-thread memory counters and reference numbers, two mutexes, the texture search path, the disk-cache switch and directory, the frame resource cache (`frameFiles`), and the definitions of `textureLoad`, `environmentLoad`, `getTexture`, `getEnvironment`, `getTextureInfo`, `initTextures` and `shutdownTextures` live in `texture.cpp`. The texture system is set up and torn down once per frame (`beginFrame`/`endFrame`), while the renderer services are registered once per session.
- **Shading already fetches resources through services** (`getTexture`, `getEnvironment`, `getCache`, `getTextureInfo`, `getTexture3d` in the renderer-services interface) but then calls the returned object's lookup directly with the shading context, in the interpreter (`shaderFunctions.h`), the JIT (`op_texture*` in `shading.cpp`), `rslBuiltins.cpp` and the hiders (`irradianceDispatch.cpp:393,533`, `radiance.cpp:1140`). Twenty-six source files under `src/` mention `CTexture`, `CEnvironment` or `CTexture3d`.
- **Shading's flat-namespace imports: 7** (spec 024's end state, re-measured on master, saved in `tools/libshader-dynlookup-before-macos.txt`). One is a Texturing symbol, `CTexture3d::resolve`; the allowed-dependency table lets Shading depend on Texturing, so the architecture check never reports it.
- **The RGBE codec is defined in Display Outputs** (`src/display/rgbe/rgbe.cpp`) and is also compiled into the renderer library (`src/ri/CMakeLists.txt`) so that the texture decoder can call it.
- **Known failures that already exist on Linux** and are not this spec's: `TextureTile_RenderByteIdentical` (#45, container bytes differ) and the four `TextureTile_Cache*` tests (#46, `orender` aborts with the disk cache on). They are the baseline on that platform.

### User Story 1 - Texturing stands on its own (Priority: P1)

A developer, or a maintainer reviewing a Principle VIII remediation, builds and tests the Texturing code without the renderer. Today even opening a texture needs the renderer core (`textureLoad` is a `CRenderer` member, the tile cache is `CRenderer` state, file search is `CRenderer::locateFile`) and a shading context for every lookup. After this feature Texturing owns a texture system that is constructed from plain settings, and the architecture check shows Texturing depending on no other domain apart from the GI part left to spec 032.

**Why this priority**: It is the remediation this spec exists for. It also unblocks specs 029 (hider/shading separation), 032 (GI cache split) and 048 (OIIO backend), which all depend on Texturing having a boundary of its own.

**Independent Test**: Build a test program that links the Texturing code but not the renderer core, constructs a texture system with a test file resolver and fixed settings, opens baked and unbaked fixtures, and performs lookups with a caller-built option block and per-thread data. Separately, run the architecture check and compare against the measured starting point.

**Acceptance Scenarios**:

1. **Given** the finished tree, **When** the Texturing test program is linked, **Then** it links and runs with no renderer-core, hider or shading object in the link, and its lookups on a known fixture return the values recorded from master.
2. **Given** the finished tree, **When** the architecture check runs on macOS and Linux, **Then** it passes, the Texturing-origin DB-010 and DB-026 allowlist lines that this spec resolves are deleted, the two `texture3d.cpp` lines are re-tagged to DB-021 (FR-015, FR-019), and no other line was added or re-keyed.
3. **Given** the finished tree, **When** the renderer-core usage metric is reported, **Then** Texturing's count has fallen from 70 references and 16 members to 1 reference and 1 member (SC-002), and the baseline file records it.
4. **Given** the texture code, **When** its sources and headers are inspected, **Then** none includes the renderer-core header, a shading header or a Graphics State header (apart from the edges FR-014 and FR-015 leave registered for specs 032 and 037 (including the two re-tagged DB-021 lines), and DB-025's data-viewer edges owned by spec 032).

---

### User Story 2 - Renders do not change (Priority: P1)

A user renders scenes that use baked textures, unbaked PNG/OpenEXR/RGBE sources, environment and shadow maps, deep shadows, point clouds, brick maps and irradiance caches, with the interpreter or the JIT, under any hider and any thread count. After this feature the images are the same, the diagnostics are the same, and the tile cache, the memory budget, the opt-in disk cache and the paging behave as before.

**Why this priority**: The move touches every texture lookup in the renderer. A pure restructuring that changes a pixel, a diagnostic or a determinism guarantee is a regression.

**Independent Test**: Run the `texture_tile` suite, the `parity-texture-*` and `parity-unbaked-*` parity tests, the texture-related visual tests and the determinism tests on master and on the finished tree, and compare pass/fail sets and, on macOS, output bytes.

**Acceptance Scenarios**:

1. **Given** each texture-bearing scene (baked, unbaked, environment, shadow, deep shadow, texture3d, point cloud, brick map, irradiance, occlusion with an environment map), **When** it is rendered on the finished tree with the same thread count and seed as on master, **Then** on macOS the output file is byte-identical to master's.
2. **Given** the `texture_tile` suite (nine tests) and the unbaked-source parity suites, **When** they run on each platform, **Then** each test has the same pass/fail result as on master. On Linux the five tests of #45 and #46 are expected to keep failing; this spec neither depends on nor fixes them.
3. **Given** a texture that is missing or unreadable, **When** a shader looks it up, **Then** the same error text is reported once for that name and the lookup returns the fill value, exactly as today (including the `textureinfo()` probe of a nonexistent file).
4. **Given** the determinism tests (`DepthDeterminism_*`, `ColorDeterminism_*`, the determinism sweep's texture scenes), **When** they run, **Then** they stay bit-identical, because each lookup consumes the same sample dimensions in the same order as before (FR-005).

---

### User Story 3 - Shading reaches textures only through its services (Priority: P1)

A shader author, or a developer embedding the shading library, uses `texture()`, `environment()`, `shadow()`, `textureinfo()`, `texture3d()`, `bake3d()` and the occlusion and radiosity lookups. Today the shading library asks its services for a texture object and then calls into the Texturing class directly, passing the shading context. After this feature every such call, in the interpreter and in the JIT, goes through the services with an option block and per-thread data the shading side builds, and a substitute texture system can stand in for the real one.

**Why this priority**: It is the second half of the remediation (Shading → Texturing through an interface Shading owns) and what spec 029 and the OSL-shaped boundary of spec 026 build on.

**Independent Test**: Run the shading library against a substitute services implementation whose texture system records every call and returns canned values. Run shaders that use each builtin under the interpreter and under the JIT and compare what the substitute received.

**Acceptance Scenarios**:

1. **Given** a substitute texture system installed through the services, **When** an interpreted shader and the same shader compiled for the JIT call `texture()`, `environment()`, `shadow()` and `textureinfo()`, **Then** the substitute receives the same handle request, the same option block and the same coordinates from both backends, and the builtins return its canned values.
2. **Given** the shading library sources, **When** they are inspected, **Then** none calls a lookup method on a Texturing class directly and none passes a shading context to Texturing.
3. **Given** the shading library, **When** its flat-namespace imports are listed on macOS, **Then** there are no more than 7 and the set contains no symbol that was not in `tools/libshader-dynlookup-before-macos.txt` (FR-011).
4. **Given** a shader that sets `"blur"`, `"width"`, `"swidth"`, `"twidth"`, `"fill"`, `"samples"` or `"filter"`, **When** it calls a texture builtin, **Then** the values reach the lookup through the option block and the image matches master.

---

### User Story 4 - One RGBE codec (Priority: P2)

A maintainer who works on the display driver or on the texture decoder finds one Radiance (RGBE) codec in shared infrastructure, used by both, instead of a Display Outputs source file that the renderer library also compiles in.

**Why this priority**: It removes DB-026 and is small and independent of the texture-system work; it can ship alone.

**Independent Test**: Run the RGBE image-input test, the RGBE display output tests and the `texture-rgbe` and `unbaked-rgbe` parity tests before and after, and run the shared-state ownership check.

**Acceptance Scenarios**:

1. **Given** the finished tree, **When** the architecture check runs, **Then** the 5 DB-026 allowlist lines are deleted and DB-026 is Resolved.
2. **Given** the finished tree, **When** the image-input RGBE test and the RGBE display driver's output tests run, **Then** results are identical to master's, including the bytes the display driver writes.
3. **Given** the finished tree, **When** the process-wide-state ownership check runs on every image the build produces, **Then** it passes and the codec has exactly one definition in a process that loads both the renderer and the RGBE display driver.

---

### Edge Cases

- **Lifetime mismatch**: the texture system is set up and torn down per frame today, while the services are registered per session (spec 024). Whichever lifetime the plan chooses, a lookup issued between frames or by an imager or default shader loaded before the first frame must find a valid system or fail with the same diagnostic as today, never dereference a stale one. Repeating `RiBegin`/`RiEnd` in one process must not leave state behind in the texture system.
- **No renderer present**: `RiMake*` calls, the point-cloud C API (`ptcapi`), `otexmake` and the data viewer use texture code outside a render, with no frame and no shading context. They must keep working, and keep working without `RiBegin`.
- **Name sharing in the resource cache**: today one name-keyed cache holds textures, environments, photon maps, irradiance caches and 3D textures together, so a name used for two kinds collides. The texture-family part of the cache is separated from the other kinds (FR-014), so behaviour changes only for such a collision, which is a latent defect; the plan must say so and keep every non-colliding case unchanged.
- **Thread counts**: the per-thread memory budget is the total budget divided by the thread count. One-thread, default and oversubscribed renders keep the same budgets, eviction order and results. A `Perthread` block must exist for every thread index the renderer can use; the plan confirms the range (including any thread outside the render pool) from the code.
- **First-use races**: two threads may first-reference the same texture name at once. The handle for a name is created once and loaded once: this spec fixes #20 (FR-016).
- **Sample-stream cursor**: texture filtering draws sample dimensions through a cursor the shading side advances (`drawCursorDim++`). An option block passed by value would lose that advance. The advance must remain visible to the caller (FR-005), or every later draw at the same shading point shifts.
- **Hider callers**: `irradianceDispatch.cpp` and `radiance.cpp` call environment lookups with their own context. They change with the lookup signature, but their include edges to Texturing are registered for spec 032 and must not grow.
- **Remote and network rendering**: `getTexture3d` and `getCache` register remote channels when writing under a network client. That core behaviour is untouched here.
- **Unbaked sources**: the in-memory pyramid synthesis stays serialized by its own mutex and keeps its disk-cache behaviour (spec 020), now owned by the texture system rather than by `CRenderer`.
- **Build options**: builds without OpenEXR or without the JIT keep building and passing. The substitute-texture-system test for the JIT is registered only when the JIT is on.
- **Linux**: `FILE` mangles differently on glibc, so allowlist link lines for the codec exist per platform. The spec must delete both.

## Requirements *(mandatory)*

### Functional Requirements

**The texture system**

- **FR-001**: Texturing MUST own one texture system. It owns the name-to-resource cache for textures, environments and shadow maps, the tile cache and its eviction (LRU by reference number), the per-thread memory budgets, counters and reference numbers, the mutexes that serialize block creation, eviction and in-memory pyramid synthesis, and the opt-in disk-cache switch and directory. None of these MAY remain defined or mutated in the renderer core.
- **FR-002**: A lookup MUST name its texture through an opaque handle that the texture system returns for a name and kind (2D texture, environment or shadow map, texture-info only). The same name in the same system lifetime MUST return the same handle. An unknown or unreadable file MUST return a handle to a substitute that returns the lookup's fill value, with the same diagnostic text as today, reported once per name.
- **FR-003**: Each lookup MUST take its per-call options in one option block passed by the caller: filter, blur, widths (combined and per axis), fill value, sample count, and the sample stream (FR-005). The shading side builds it at the call site from the shader's lookup parameters. The block has no pointer to a shading context.
- **FR-004**: Each lookup MUST take a per-thread data block obtained from the texture system, holding what lookups used to read from the shading context: the thread index, scratch memory, the thread's reference counter and its memory accounting. A lookup MUST NOT include, name or receive a shading context.
- **FR-005**: Every lookup MUST consume sample dimensions from the option block's sample stream in the same number and order as today (the stratified filter samples of the 2D, shadow, deep-shadow and environment lookups), and the consumed cursor position MUST be visible to the caller afterwards. The stream is expressed in the infrastructure sampling types of `libRIBCommon` (spec 039), which Texturing may depend on. Draws made after a lookup at the same shading point MUST be unchanged.
- **FR-006**: The loaders now defined as `CRenderer` members in `texture.cpp` (`textureLoad`, `environmentLoad`, `initTextures`, `shutdownTextures`) and the texture-family part of the `rendererFiles.cpp` loaders (`getTexture`, `getEnvironment`, `getTextureInfo`) MUST move into Texturing. The renderer core MUST only construct the system, configure it (memory budget, thread count, search path, disk-cache settings, file resolver) and register it with Shading's services.
- **FR-007**: Texturing MUST locate files through a resolver supplied when the system is constructed, never by calling the renderer core. With a test resolver the system MUST work with no renderer, no frame and no `RiBegin` (the `RiMake*` calls, the point-cloud API, `otexmake` and the data viewer).
- **FR-008**: The tile-source classes (spec 019/020) MUST keep their behaviour: the baked-TIFF source, the synthesized source, the opt-in disk cache and its stale detection. The two direct unit tests that construct them through `createTiffTileSource` and `createSynthesizedTileSource` MUST keep compiling and passing without edits beyond include paths.

**Shading side**

- **FR-009**: The renderer-services interface MUST offer the texture system to Shading (replacing the five resource getters that return Texturing classes). `texture()`, `environment()`, `shadow()`, `textureinfo()`, the 3D-texture channel lookups (`texture3d()`, `bake3d()`) and the occlusion/radiosity environment lookups MUST go through it, in the interpreter, in the JIT's `op_texture*` and in `rslBuiltins.cpp`. No Shading source MAY call a lookup method of a Texturing class or pass a shading context to Texturing.
- **FR-010**: The interpreter and the JIT MUST agree: for every texture builtin, both backends hand the services the same handle request, option block and coordinates, and produce the same result.
- **FR-011**: Shading's flat-namespace imports MUST NOT increase and MUST NOT include any symbol outside the set in `tools/libshader-dynlookup-before-macos.txt`. Routing the 3D-texture channel resolution (`CTexture3d::resolve`) through the services SHOULD remove it, taking the count from 7 to 6; if the plan finds that impossible without entering spec 032's territory (FR-014), it records why. On Linux the counterpart set measured by spec 024's method MUST NOT grow.
- **FR-012**: The shading library MUST be testable against a substitute texture system (constitution rule 5): a test installs one through the services, runs each texture builtin under both backends, and checks what the substitute received (FR-018). With no texture system installed, each lookup MUST return its fill value and report a warning once per kind of lookup per process, never dereference a missing system.
- **FR-013**: The hider callers of environment lookups (`irradianceDispatch.cpp`, `radiance.cpp`) MUST move to the new lookup form. Their include edges to Texturing are not changed by this spec: no hider-to-Texturing edge may be added or re-keyed (FR-019).

**GI boundary, channel registry and concurrency (clarification items)**

- **FR-014**: The GI boundary with spec 032 is drawn as follows. This spec moves the 2D, environment and shadow lookups, the point hierarchy's options (including the occlusion solid-angle limit it reads from the shading context today), the tile cache and the texture-family part of the name-keyed resource cache (textures, environments, shadow maps). `CTexture3d`'s lookup and constructor/destructor signatures, the class hierarchy it shares with `CIrradianceCache`, and the photon-map, irradiance-cache and 3D-texture entries of `frameFiles` (with the photon-map reference counting of #64) are NOT changed here; they belong to spec 032. The headers `texture.h`, `texture3d.h` and `pointHierarchy.h` keep their paths and `CTexture3d`'s constructor, destructor and type information keep their mangled names, so the 8 Hiders-origin DB-010 lines stay valid (FR-019). The plan decides how the texture-family part is separated from the cache's other kinds and records any name-collision effect (Edge Cases).
- **FR-015**: Texturing's reach into the channel registry stays as spec 025 leaves it: `texture3d.cpp` keeps calling the out-of-line `CRenderer::retrieveDisplayChannel` forwarder and keeps including the channel-registry header. The two `texture3d.cpp` allowlist lines that name it (the `retrieveDisplayChannel` link and the `renderer.h` include) are re-tagged from DB-010 to DB-021, with the maintainer's explicit sign-off (2026-10-27); this is the only permitted re-key (FR-019). DB-021 stays Open for spec 037. Texturing's end renderer-core usage metric is therefore 1 reference and 1 member.
- **FR-016**: This spec absorbs GitHub #20: a handle lookup by name MUST be thread-safe by construction, so two threads first-referencing one name create one handle and load it once, and the cache's find-load-insert sequence is no longer unlocked. The fix MUST keep renders byte-identical. #19 (tile-block data read outside a lock after possible concurrent eviction) and #21 (process-global libtiff error handler and `RiLastError`) are out of scope, as are #45 and #46; eviction behaviour and order MUST NOT change. The maintainer's merge message carries `Closes #20`; this spec does not write it.

**Codec**

- **FR-017**: The RGBE codec (header read, pixel read and write) MUST move to shared infrastructure that every domain may depend on, with one definition. The display driver and the texture decoder MUST both use it. The renderer library MUST stop compiling the display driver's source file. The codec's behaviour and the bytes the driver writes MUST NOT change.

**Tests**

- **FR-018**: Tests MUST be written first and shown failing on the tree before the change (constitution III): (a) a Texturing-only program that links no renderer-core object (User Story 1); (b) the substitute-texture-system test under both backends (User Story 3, FR-012); (c) a concurrent first-time-load test: many threads first-referencing one name obtain one handle and trigger one load, shown failing on the tree before the change (FR-016, #20); (d) a test that the option block's cursor position after a stochastic lookup equals master's (FR-005). Existing tests MUST NOT be disabled, relaxed or have a threshold widened to pass.

**Registry and guardrails**

- **FR-019**: The architecture check MUST pass on macOS and Linux. The allowlist MUST only shrink:
  - DB-026's 5 lines are deleted and DB-026 moves to Resolved with this spec's number and date.
  - Of DB-010's 27 Texturing-origin lines, 25 are deleted and 2 (the `texture3d.cpp` `retrieveDisplayChannel` link and `renderer.h` include) are re-tagged to DB-021 (FR-015). DB-010 stays Open for its 8 Hiders-origin lines, with its title, evidence, size and direction edited to say so; it falls from 35 lines to 8.
  - DB-021 gains those 2 lines (1 to 3) and its text is edited to say so; DB-025's 6 Texturing-origin lines are unchanged.
  - **No allowlist line MAY be added or re-keyed, except the two DB-010 to DB-021 re-tags of FR-015, which the maintainer approved.** The net change is the deletion of 25 DB-010 lines and 5 DB-026 lines and nothing else. Lines are keyed by file path (include) and mangled symbol (link), so a line whose key this spec changes counts as a new line and is not allowed. Therefore the headers the hiders include (`texture.h`, `texture3d.h`, `pointHierarchy.h`) keep their paths, and `CTexture3d`'s constructor, destructor and type information keep their mangled names (FR-014).
  - The renderer-core usage-metric baseline for Texturing is lowered to the measured end value and the TOTAL row is adjusted by the same amount.
- **FR-020**: Rendering output and existing test results MUST NOT change: same pass/fail set as master on each platform, byte-identical texture renders on macOS (SC-006).
- **FR-021**: No registry text, task or success criterion MAY refer to anything that exists only after the merge. Issues close through the maintainer's merge message (`Closes #33`); this spec does not write it.

### Key Entities

- **Texture system**: the Texturing-owned object that owns the resource cache, tile cache, eviction, per-thread budgets, paging mutexes and disk-cache settings, and answers handle requests and lookups.
- **Texture handle**: an opaque reference to one loaded texture, environment or shadow map (or its substitute), valid for the system's lifetime.
- **Lookup options**: the by-value block of per-call settings (filter, blur, widths, fill, samples, sample stream).
- **Per-thread data**: the block a shading thread obtains from the system, holding thread index, scratch memory, reference counter and memory accounting.
- **Sample stream**: the position in a counter-based sub-stream (spec 039) from which a lookup draws its stratified filter samples, advanced by each draw.
- **File resolver**: the supplier of "find this file on the search path", given to the system at construction.
- **Substitute texture / texture system**: the always-valid stand-in for a missing file, and the test double used to run the shading library without Texturing.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: The architecture check passes on macOS and Linux. DB-026 is gone (5 lines), DB-010's 27 Texturing-origin lines are gone (25 deleted, 2 re-tagged to DB-021 with the maintainer's sign-off), and no other allowlist line is added or re-keyed. DB-010's total falls from 35 to 8; DB-021 holds 3 lines.
- **SC-002**: Texturing's renderer-core usage metric falls from 70 references and 16 distinct members to 1 reference and 1 member (the `retrieveDisplayChannel` forwarder, FR-015), and the baseline records it. Shading's stays at 0 and 0.
- **SC-003**: A program that links the Texturing code and no renderer-core, hider or shading object builds, opens a baked and an unbaked fixture and returns values equal to master's recorded values.
- **SC-004**: Shading's flat-namespace imports on macOS are at most 7 and a subset of the starting set (6 if `CTexture3d::resolve` is routed); the Linux counterpart does not grow.
- **SC-005**: Zero direct lookup calls into Texturing classes remain in `src/libshader`, and zero lookup signatures in `src/ri/texture` name `CShadingContext` (apart from the 3D lookup, which FR-014 leaves for spec 032).
- **SC-006**: On macOS, every texture-bearing scene renders to bytes identical to master's, including all `parity-texture-*`, `parity-unbaked-*`, `texture-*`, `env`, `shadow`, texture3d, point-cloud, brick-map, irradiance and occlusion scenes in the visual suite and the determinism sweep, at one and at the default thread count where the scene was deterministic before.
- **SC-007**: On each platform the full test suite has the same pass/fail set as master. The `texture_tile` suite (nine tests) and the unbaked-source parity suites are unchanged; on Linux the five tests of #45/#46 still fail and no other test starts failing. The determinism tests (`DepthDeterminism_*`, `ColorDeterminism_*`) stay green.
- **SC-008**: The substitute-texture-system test passes under the interpreter and the JIT with identical recorded calls, and fails on the tree before the change.
- **SC-009**: The process-wide-state ownership check passes with the RGBE codec defined once; the RGBE display tests and `ImageInput_Rgbe` give master's results.
- **SC-010**: A texture-heavy scene's render time on the finished tree is within the run-to-run noise measured on master, on the same machine and thread count. A regression beyond noise found during implementation is a defect.
- **SC-011**: `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` lists DB-026 as Resolved with spec 027 and its date, and DB-010 and the other entries carry the edited text and counts; no entry refers to the merge.

## Assumptions

- Spec 023 is merged (`libRIBCommon` provides diagnostics, vocabulary, stats, the sampling types). Spec 039 is merged, so the sample stream exists as infrastructure. Spec 025 (graphics-state handles) is in progress on its own branch and is not a dependency; this spec does not edit `src/ri/state`. Its decision that `texture3d.cpp` keeps reaching the channel registry through a thin `CRenderer::retrieveDisplayChannel` forwarder, with DB-021 staying Open for spec 037, is taken as given.
- Spec 026 (RendererServices v2) lists "texture through TextureSystem" in its scope. Confirmed by the maintainer (2026-10-27): this spec defines the texture part of the services interface (FR-009) and spec 026 adopts it; the services shape stays the plan's decision.
- Confirmed by the maintainer (2026-10-27): the texture system's lifetime stays per frame (set up in `beginFrame`, torn down in `endFrame`), because that keeps the texture cache, memory accounting and unload timing, and with them the byte-identical renders, exactly as today. Making it session-long is a later optimization.
- The brief's scope list omits files that belong to the move: `rendererMutexes.cpp` (the two texture mutexes), `renderer.cpp` (`initTextures`, `shutdownTextures`, `CBrickMap::initBrickMap`), and `texmake.h`, which includes `renderer.h` in a public header. The plan includes them. The brief's range `rendererFiles.cpp:364-655` also does not line up with the code: it contains `shutdownFiles` (DSO cleanup), `getPhotonMap` and `getCache` (Hiders classes and remote channels) but not `getTexture3d` (lines ~670-731). The texture-family loaders are `getTexture`, `getEnvironment` and `getTextureInfo` (lines ~436-660).
- The brief says the lookup option block is passed "by value" and the lookup signature is `(TextureOpt&, Perthread*)`. This spec reads both as: built by value by the caller, passed by reference, with the sample-stream cursor advance visible to the caller (FR-005).
- The point-cloud C API (`ptcapi`), `otexmake` and the data viewer are consumers: they may depend on anything. Their behaviour is unchanged. The data viewer's edges into Texturing (DB-024/DB-025) are spec 032's.
- Out of scope: the `brickmap.cpp` format-string defect near lines 1706/1710 (filed as GitHub #103), concurrency bugs #19 and #21, the libtiff-warnings-as-errors defect (#7), the `TextureTile_*` Linux failures (#45, #46), the OIIO backend (spec 048), the GI cache storage move (spec 032), hider/shading composition (spec 029), the shading contract v2 beyond the texture interface (spec 026), per-domain CMake targets (DB-023).
- Checking and carrying the option block adds no measurable render-time cost; SC-010 treats a regression as a defect rather than setting a budget.
- The RGBE codec lands in `libRIBCommon` (the shared infrastructure the architecture check classifies as Infra). Whether the display driver module links the shared copy or keeps an object copy, and what the state-ownership check's image list then expects, is a plan decision under gotcha 22 (one shared copy of process-wide state; the codec itself holds none).
- GitHub #33 (this spec) is closed by the maintainer's merge message. Specs 029, 032 and 048 depend on this one.
