# Implementation Plan: Texture System

**Branch**: `027-texture-system` | **Date**: 2026-10-27 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/027-texture-system/spec.md`. The Clarifications of session 2026-10-27
and the maintainer's five plan decisions of the same day (recorded there) are applied as decided. Six questions
for the maintainer are at the end (Q1–Q6); the plan is written with each recommendation applied.

## Summary

Texturing gets a texture system of its own and stops depending on the renderer core, Shading and Graphics State
for its 2D, environment and shadow work. It resolves DB-026 and the Texturing part of DB-010. In order:

1. **The RGBE codec moves to libRIBCommon** (`src/common/rgbe.{h,cpp}`, `extern "C"`); the `rgbe` driver links the
   shared copy; the renderer library stops compiling the driver's source. DB-026 is Resolved.
2. **File search through a resolver**: a Texturing-owned `CTextureFileResolver`, implemented by the core over
   `CRenderer::locateFile`. `texmake`, `brickmap` and the loaders take it instead of `TSearchpath *`, which removes
   every Graphics State include from Texturing.
3. **A per-frame `CTextureSystem`** owns the texture-family cache (split out of `frameFiles`), the tile cache, the
   LRU eviction, the per-thread budgets and reference numbers, the block mutex and the disk-cache settings. The core
   only constructs it in `beginFrame()`, configures it and destroys it in `endFrame()`. Its handle lookup is locked
   by construction: GitHub #20.
4. **Lookups take an option block and per-thread data**: `TextureOpt` (by value at the call site, passed by
   reference, carrying the spec 039 sample stream whose advance the caller writes back) and `TexturePerthread`.
   No 2D/environment/shadow lookup names `CShadingContext`.
5. **Shading reaches textures only through its services**: `CRendererServices::textureSystem()` replaces
   `getTexture`/`getEnvironment`/`getTextureInfo`; one Shading unit (`textureCalls`) serves the interpreter, the JIT,
   `rslBuiltins.cpp` and the irradiance hider; with no system a builtin returns its fill value and warns once per
   kind. The `texture3d()`/`bake3d()` channel operations go through three services forwarders, which removes the
   `CTexture3d::resolve` import (7 → 6). The point hierarchy reads its solid-angle limit through an accessor the
   core gives it.
6. **Guardrails**: 25 DB-010 and 5 DB-026 allowlist lines deleted, the two `texture3d.cpp` lines re-tagged to DB-021,
   none added; Texturing's renderer-core metric 70/16 → 1/1; registry updated.

Tests are written first: a Texturing-only program (no renderer-core, hider or shading object in the link), a
substitute texture system under both backends, #20's concurrent first-load test (4 threads, 200 repeats), a
sample-stream test, and an RGBE display-driver byte test. Every phase keeps the architecture check green and every
image byte-identical, the JIT texture paths included.

## Technical Context

**Language/Version**: C++20 (`-Werror`). CMake ≥ 3.19.

**Primary Dependencies**: no new dependency. libRIBCommon (sampling types, diagnostics, `CTrie`, `CMemPage`,
`TMutex`), libtiff, libpng, OpenEXR (optional), `libshader_shading`, `libRIBRender`.

**Storage**: N/A. Memory only; the opt-in disk cache (spec 020) keeps its files and format.

**Testing**: ctest.
- `texture_tile` (run at `-j2`): `TextureSystem_ConcurrentFirstLoad`, `TextureSystem_StreamCursor` join the nine
  existing tests.
- `texture_only` (new, behind `OPENRENDER_TEXTURE_ONLY_TESTS`): `TextureOnly_Standalone`.
- `shading_parity`: `ShadingParity_TextureSubstitute` (JIT builds only).
- `image_input`: `Display_RgbeBytes`.
- `common`: `Common_StateOwnership` with the RGBE symbols.
- Also `architecture`, `visual`, `^determinism$`, the full suite every phase, and the sweep once at the end.

**Target Platform**: macOS (Apple Silicon, Clang, vcpkg) and Linux (Ubuntu 24.04, GCC 13, `-Werror`, through the
maintainer's rsync + Maestri path).

**Project Type**: renderer library and CLI.

**Domains touched**:

| Domain | What changes |
|---|---|
| Texturing | the main subject: `textureSystem.{h,cpp}`, `textureResolver.h`, `textureInternal.h` (new); `texture.{h,cpp}`, `texmake.{h,cpp}`, `brickmap.{h,cpp}`, `pointCloud.h`, `pointHierarchy.{h,cpp}`, `texture3d.h` (one include), `imageInputRgbe.cpp` |
| Shading | `rendererServices.h`, `textureCalls.{h,cpp}` (new), `shading.{h,cpp}`, `shaderFunctions.h`, `giFunctions.h`, `shaderPl.{h,cpp}`, `rslBuiltins.cpp`, `execute.cpp` (includes) |
| Hiders | `irradianceDispatch.cpp`: two environment lookups through `CShadingContext::lookupEnvironment()` |
| Display Outputs | `src/display/rgbe/`: the codec leaves; `file.cpp` includes `common/rgbe.h` |
| Infrastructure | `src/common/rgbe.{h,cpp}` (moved in) |
| Renderer core | `renderer.{h,cpp}`, `rendererStatics.cpp`, `rendererMutexes.cpp`, `rendererFiles.cpp`, `searchPathResolver.h` (new), `rendererFileSearch.cpp`, `rendererContext.cpp` (`RiMake*`), `rendererServicesImpl.h` |
| Consumers | tests; `otexmake` unchanged (it goes through `RiMake*`) |

Graphics State, Geometry and Scene Interface are not edited. `texture3d.cpp` is not edited (FR-015).

**Performance Goals**: no measurable change (SC-010): one struct built and one virtual call per builtin call; the
lookup arithmetic is unchanged.

**Constraints**:
- byte-identical renders in every phase, on macOS, including the JIT texture paths (FR-020, SC-006);
- no allowlist line added or re-keyed except the two approved re-tags (FR-019); the metric baseline only goes down;
- no threshold widened, no test disabled to get a green result; no reference re-rendered;
- no task depends on the merge (FR-021);
- `cpujob` for every build and test: one job at a time machine-wide, `nice -n 19`, at most `-j4`; `-j2` for the
  sweep and for `texture_tile`.

**Scale/Scope**: about 25 source files changed; 6 Texturing and Shading files and 1 core header added; 2 files moved;
5 test executables or scripts added with their fixtures and probe shaders; the architecture data files and the
registry updated.

**Measurements** (macOS, `build-027` at `0f76934` = `bc261b1` + spec, 2026-10-27; [validation.md](validation.md) §0):
- **Architecture check**: PASS; `DB-010 x 35`, `DB-021 x 1`, `DB-025 x 12`, `DB-026 x 3`; Texturing 70/16, TOTAL
  721/110. The 16 members are listed by `tools/core_usage.py` (research R1).
- **Shading flat-namespace imports**: 7, the spec 024 set.
- **`texture_tile`** 9/9, **`image_input`** 6/6, **texture-related visual and parity tests** 40/40.
- **Not yet measured** (the first measurement task, maintainer decision 4): the full suite, the image manifest and
  its noise, the JIT probes, the lookup values for SC-003, the RGBE driver reference, the timing; and all of Linux.

## Constitution Check

*GATE: must pass before Phase 0 research; re-checked after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. Clean code | PASS | One owner for the texture state instead of ten `CRenderer` statics mutated from `texture.cpp`; one Shading unit for every texture request instead of ~15 hand-written call sites; three duplicated getters deleted. |
| II. C++20 / C17 | PASS | Scoped enum, aggregates, `std::mutex` for the process-wide synthesis lock, `std::atomic<bool>` warn-once flags; `extern "C"` for the C codec. No extension. |
| III. TDD (non-negotiable) | PASS (by plan order) | Phase 2 writes the tests and records their red before any production change (see "Test design"). Four are red by build failure, as in 024's T-P5 and 025's T-S1; #20's test is also red by assertion at phase 5's adapter step; one test is a characterization test, green on both trees by design and labelled so. |
| IV. CLI | N/A | No CLI change. |
| V. Minimal dependencies | PASS | None added. |
| VI. Linux and macOS | PASS | Both platforms measured before and after (quickstart §0, §5); GCC 13 `-Werror`; platform-tagged allowlist lines handled per platform. |
| VII. Docs | PASS | `DOMAIN_BOUNDARIES.md`, the allowlist and metric headers, DEVNOTES, `CLAUDE.md` (Architecture and a Known gotcha on the texture system's lifetime and the services' texture part). No user-facing page changes: rendering does not change. |
| VIII. Domain separation | PASS | See below. |

**Principle VIII — Domain Separation and Modular Boundaries** (every plan):

- [x] **Domains touched are named**: Texturing, Shading, Hiders (`irradianceDispatch.cpp`), Display Outputs
  (`src/display/rgbe`), infrastructure (`src/common`), the renderer core, Consumers (`tests/`).
- [x] **No new cross-domain dependency outside the table, and none on the renderer core from inside a domain.**
  - **Added**: Shading → Texturing (`textureSystem.h` from `rendererServices.h`, `shading.h`, `textureCalls.h`,
    `shaderPl.h`), allowed; Texturing → Infra and Display Outputs → Infra (`common/rgbe.h`), allowed; Core →
    Texturing and Core → Shading (the system's construction, `CSearchPathResolver`, the services), allowed.
  - **Removed**: Texturing → renderer core (`renderer.h` and `CRenderer::` from `texture.cpp`, `texmake.{h,cpp}`,
    `brickmap.cpp`); Texturing → Shading (`shading.h` from `texture.cpp`, `pointHierarchy.cpp`);
    Texturing → Graphics State (`options.h` ×4, `attributes.h`, `xform.h`); Texturing → Display Outputs (the codec).
  - **Inversions**: the resolver and the point hierarchy's solid-angle accessor are Texturing-owned interfaces,
    implemented and installed by the core (rule 2). Neither is a load-time-bound symbol.
  - **Hiders**: no new Hiders → Texturing edge. `irradianceDispatch.cpp` calls a Shading member and gets the types
    through `shading.h`; the hider files keep their DB-010 includes at the same paths.
- [x] **Every added or altered cross-domain dependency has its interface specified**:
  [contracts/texture-system.md](contracts/texture-system.md),
  [contracts/renderer-services.md](contracts/renderer-services.md),
  [contracts/rgbe-codec.md](contracts/rgbe-codec.md), [data-model.md](data-model.md).
- [x] **No hider-specific special case**: the hider uses the same Shading helper as the builtins; Texturing names no
  hider.
- [x] **Plug-in interfaces**: none changed. `dsply.h` is untouched; the RGBE codec was never part of the display ABI
  (each driver compiled its own copy). No Semantic Versioning bump (DB-003, DB-014 untouched).
- [x] **Registered violations are not extended, and reductions are noted**
  ([contracts/allowlist-end-state.md](contracts/allowlist-end-state.md)):
  - DB-026: 5 lines → 0, Resolved.
  - DB-010: 35 → 8 (25 deleted, 2 re-tagged with the maintainer's sign-off); stays Open for the hiders' GI part (032).
  - DB-021: 1 → 3 by the re-tag only; the edges themselves exist today (spec 025's forwarder); stays Open for 037.
  - DB-001: 7 → 6 flat-namespace imports; edge count unchanged.
  - DB-025, DB-023 (no production Texturing target: the Texturing-only target is test-only, as 025's State one),
    DB-017, DB-005: not extended.

**Post-design re-check (after Phase 1)**: PASS, unchanged. The contracts add no edge beyond those listed.

**Reference-doc decisions (D1–D30)**: none changed. D22 (independently versioned renderer-services API) is left to
spec 026, which adopts this texture part; D23 (the in-tree RGBE codec is removed when OSL lands) is unaffected by
its move into libRIBCommon; D30(4) (an OIIO wrapper library beside libRIBCommon for 048) is unaffected.

## Project Structure

### Documentation (this feature)

```text
specs/027-texture-system/
├── spec.md, plan.md, research.md, data-model.md, quickstart.md, validation.md
├── contracts/texture-system.md, renderer-services.md, rgbe-codec.md, allowlist-end-state.md
├── checklists/requirements.md
├── tools/                      # measurement scripts and data
│   ├── core_usage.py, core-usage-texturing-bc261b1.txt          # plan stage
│   ├── archcheck-bc261b1-macos.txt, libshader-dynlookup-{before,bc261b1}-macos.txt
│   ├── image_manifest.sh       # phase 1: spec 025's tool, copied (025 is not merged)
│   ├── jit_texture_probes.sh   # phase 1: renders the texture/env/shadow/unbaked parity scenes as .slo
│   ├── lookup-probe.rib, lookupprobe.sl, master-lookup-values.txt     # phase 1: SC-003 values
│   ├── time_texture_scenes.sh, timing-*.tsv                           # phase 1 and 9: SC-010
│   └── baseline-images.sha256, baseline-image-noise.txt, baseline-diffs.txt
└── tasks.md                    # /speckit-tasks (not created here)
```

### Source code touched

```text
src/common/
├── rgbe.{h,cpp}                 # MOVED from src/display/rgbe/; extern "C"
└── CMakeLists.txt               # + rgbe.cpp in openrendercommon_obj
src/display/rgbe/file.cpp        # include "common/rgbe.h"
src/ri/texture/
├── textureSystem.h              # NEW: CTextureSystem, TextureHandle, ETextureKind, TextureOpt, TextureSampleStream,
│                                #      TexturePerthread (opaque), TextureInfo, CTextureSystemSettings, createTextureSystem
├── textureResolver.h            # NEW: CTextureFileResolver
├── textureInternal.h            # NEW, Texturing-private: CTextureCache, TexturePerthread's definition, loader declarations
├── textureSystem.cpp            # NEW: CTextureSystemImpl (family cache, load lock, handles, perthread, lookup dispatch)
├── texture.{h,cpp}              # lookups take (TextureOpt&, TexturePerthread*); loaders are Texturing functions;
│                                #   tile state in CTextureCache; - renderer.h, shading.h, attributes.h
├── texmake.{h,cpp}              # resolver instead of TSearchpath; - renderer.h, options.h
├── brickmap.{h,cpp}, pointCloud.h   # makeBrickMap takes the resolver; - renderer.h, options.h
├── pointHierarchy.{h,cpp}       # CPointHierarchy::Options; - options.h, shading.h
├── texture3d.h                  # - xform.h (unused)
└── imageInputRgbe.cpp           # include "common/rgbe.h"
src/ri/CMakeLists.txt            # - ../display/rgbe/rgbe.cpp; + texture/textureSystem.cpp in ri_sources
src/ri/render/
├── renderer.h                   # - the texture statics, mutexes and members; + static CTextureSystem *textureSystem
├── renderer.cpp                 # beginFrame/endFrame construct and destroy the system (where init/shutdownTextures ran)
├── rendererStatics.cpp, rendererMutexes.cpp   # - the texture statics and the two mutexes
├── rendererFiles.cpp            # - getTexture/getEnvironment/getTextureInfo; CPointHierarchy options accessor
├── searchPathResolver.h         # NEW: CSearchPathResolver (core implementation of the resolver)
├── rendererFileSearch.cpp       # + its definition, beside locateFile
├── rendererContext.cpp          # RiMake* pass a CSearchPathResolver
└── rendererServicesImpl.h       # textureSystem(), the three 3D forwarders; - the three texture getters
src/libshader/shading/
├── rendererServices.h           # the texture part (contracts/renderer-services.md)
├── textureCalls.{h,cpp}         # NEW: handle acquisition, no-system rule, warn-once
├── shading.{h,cpp}              # textureOptions/commitTextureDraws/lookupEnvironment; jit* texture bodies;
│                                #   - rendererGetTexture/Environment/TextureInfo
├── shaderFunctions.h, giFunctions.h   # the interpreter's texture, environment, shadow, textureinfo, texture3d, bake3d
├── shaderPl.{h,cpp}             # lookups hold TextureHandle *
├── rslBuiltins.cpp              # rsl_texture_c_scalar through textureCalls
├── execute.cpp                  # includes only
└── CMakeLists.txt               # + textureCalls.cpp
src/ri/hiders/irradianceDispatch.cpp   # two environment lookups through lookupEnvironment()
tests/
├── unit/texture_tile/           # + test_texture_system_concurrent_first_load.cpp, test_texture_system_stream_cursor.cpp
├── unit/texture_only/           # NEW: CMakeLists.txt (behind OPENRENDER_TEXTURE_ONLY_TESTS), test_texture_only_standalone.cpp
├── unit/image_input/            # + Display_RgbeBytes (script, rgbe-display.rib, references/rgbe-display.hdr)
├── unit/common/owned-symbols.txt  # + the six RGBE names
├── shading_parity/              # + test_texture_substitute.cpp, texture probe shaders; table_services.h, recording_services.h
├── architecture/allowlist.txt, metrics-baseline.txt
└── CMakeLists.txt               # + add_subdirectory(unit/texture_only)
DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md, DEVNOTES.md, CLAUDE.md
```

**Structure decision**: the existing layout. New Texturing code goes in `src/ri/texture/`, new Shading code in
`src/libshader/shading/`, the codec in `src/common/`; all three directories are already classified in
`domain-map.txt`, so no domain-map change. The Texturing-only library compiles `src/ri/texture` sources a second
time with a narrow include path and exists only behind `OPENRENDER_TEXTURE_ONLY_TESTS` (archcheck exits 2 on a
missing in-scope object, research R15; spec 025 measured that doubly compiled units leave every reported number
unchanged).

## Design

Details in [research.md](research.md). In brief:

1. **Option block** (R2, R3): `TextureOpt` holds what every lookup reads from the scratch today, including both
   sample counts (`samples` from `texture()`'s block, `traceSamples` and `bias` from `environment()`/`shadow()`'s
   trace block), because the shadow, deep-shadow and cubic-environment lookups read the trace block while the
   spherical and cylindrical ones read the 2D block through their side texture. The sample stream is a value in the
   block; nested lookups share it; the caller writes `stream.dim` back.
2. **Per-thread data** (R4): `numThreads` blocks, the range the contexts use (the imager runs in a pool context);
   each owns its scratch arena instead of borrowing the context's.
3. **Handles** (R5): the family cache is keyed by name as `frameFiles` is; same substitutes, same messages, the
   `textureinfo()` miss uncached. Only names shared across families change behaviour (they no longer collide).
4. **#20** (R6): the handle lookup holds a load lock across find → load → insert; from phase 6, when the services'
   wrapper is gone, the core passes `shaderMutex`, so texture loads stay serialized with the other resource loads
   exactly as the wrapper does today (passing it while the wrapper still holds it would self-deadlock).
5. **Lifetime** (R7): per frame, at `initTextures`/`shutdownTextures`'s positions; the services return the frame's
   system or `nullptr`.
6. **Resolver** (R8): Texturing-owned interface, core implementation over `locateFile`; `RiMake*` and the system
   use it; `TSearchpath` leaves Texturing.
7. **Services** (R9): `textureSystem()` replaces three getters; `getCache`/`getTexture3d`/`getPhotonMap` stay (FR-014);
   three 3D forwarders on the services, not the system, so the Texturing-only link needs no `texture3d.o`.
8. **Shading** (R10, R11): `textureCalls` plus three inline `CShadingContext` members serve every call site; each
   backend keeps the request it makes today (Q1); `environment()` traces by name; no system → fill and one warning
   per kind.
9. **Point hierarchy** (R12): `CPointHierarchy::Options` with a core-supplied solid-angle accessor; no signature of
   `CTexture3d` changes.
10. **Synthesis lock** (R13): process-wide, Texturing-owned (Q4).
11. **RGBE** (R14): moved, `extern "C"`, one definer checked by `Common_StateOwnership`; a new driver byte test.
12. **Guardrails** (R17): line by line in [contracts/allowlist-end-state.md](contracts/allowlist-end-state.md).

## Phases

Every phase ends with the checkpoint below and a pause for the maintainer's review. Lines of the allowlist are
deleted **at the start** of the phase that removes their edges, so its architecture check is red until the code
lands (spec 025's practice). A phase may change no image.

**Checkpoint (CP-a … CP-d)**, quickstart §2:
- **CP-a** build clean under `-Werror`;
- **CP-b** the full suite against phase 1's pass list plus the tests enabled so far (`texture_tile` also alone at
  `-j2`); a red test from the known-flaky set is re-run alone before it is written off;
- **CP-c** `ctest -L architecture` green; `ALLOWED`, `METRIC`, `RESULT` recorded in validation.md;
- **CP-d** `image_manifest.sh diff`: 0 differing and 0 missing images against phase 1's manifest (the suite's
  images and the JIT texture probes), every `MaxBlockAvgDiff` equal to the baseline's.

| # | Phase | Content | Lines deleted at start | Expected red until done |
|---|---|---|---|---|
| 1 | Baselines | Re-measure on a fresh macOS build (decision 4): archcheck, members, imports, full suite, manifest and noise, JIT probes, `master-lookup-values.txt`, timing. Copy `image_manifest.sh`; write `jit_texture_probes.sh`, the lookup probe, the timing script. List the Linux baseline run for the maintainer | — | — |
| 2 | Tests first | Write T-T1…T-D1 (table below); record each red; write `Display_RgbeBytes` and its reference from the starting tree | — | the red tests (build failures) |
| 3 | RGBE codec (US4) | Move the codec, `extern "C"`, CMake, `owned-symbols.txt`; DB-026 → Resolved | 5 DB-026 | 3 `VIOLATION` (macOS) |
| 4 | Resolver and State includes | `CTextureFileResolver`, `CSearchPathResolver`; `texmake`/`brickmap`/loaders/`RiMake*` use it; drop the unused `attributes.h`, `xform.h`, `options.h` includes | 13 DB-010 | 13 `VIOLATION` |
| 5 | The system owns the state | `CTextureSystem` with `CTextureCache`, `TexturePerthread`, settings, the family cache split from `frameFiles`; loaders and init/shutdown move; core constructs/destroys per frame; the services' texture getters forward to it for now; lookups still take the context and reach their per-thread state through their cache. **#20**: the find-load-insert first moves verbatim (enable `TextureSystem_ConcurrentFirstLoad`: red by assertion), then the system's own load lock lands (green). `loadMutex` stays `nullptr` in this phase: the services' getters still hold `shaderMutex` around their forward, and it is a plain, non-recursive pthread mutex (`os.cpp:419-425`) | 10 DB-010 | 10 `VIOLATION`; #20's test between the two steps |
| 6 | Option block and services | `TextureOpt`/`TexturePerthread`/handles in the lookups; `textureSystem()` in the services, three getters removed; `textureCalls`; every 2D/environment/shadow/textureinfo call site of both backends, `rslBuiltins.cpp`, the hider; no-system rule; in the step that deletes the getters and their `shaderMutex` wrapper, the core starts passing `&CRenderer::shaderMutex` as `loadMutex`; enable `TextureSystem_StreamCursor` and the 2D part of `ShadingParity_TextureSubstitute` | 1 DB-010 | 1 `VIOLATION` |
| 7 | 3D through the services, point hierarchy | The three 3D forwarders and the `texture3d()`/`bake3d()` call sites; `CPointHierarchy::Options`; imports 7 → 6; the 3D part of the substitute test | 1 DB-010 | 1 `VIOLATION` |
| 8 | Texturing stands alone | `OPENRENDER_TEXTURE_ONLY_TESTS` default ON; `TextureOnly_Standalone` green against `master-lookup-values.txt`; first archcheck run with the doubly compiled units | — | — |
| 9 | Registry, docs, Linux | Re-tag the 2 `texture3d.cpp` lines to DB-021; DB-010/DB-021/DB-001 text (DB-026 was moved to Resolved in phase 3); metric baseline 1/1 and the TOTAL row as reported; DEVNOTES, CLAUDE.md; timing against the baseline; the determinism sweep; the maintainer's Linux run (quickstart §5) | — | — |

Phase 3 can ship alone (spec US4). Phases 4–8 depend in order. The two test-only `EXCLUDE_FROM_ALL` targets are
switched into `all` in the phase that makes them build.

## Test design (red first)

| Id | Test | Red on the starting tree because | Covers |
|---|---|---|---|
| T-T1 | `TextureOnly_Standalone` (`tests/unit/texture_only`, label `texture_only;unit`, behind `OPENRENDER_TEXTURE_ONLY_TESTS`): links `texturing_only` + libRIBCommon + image libraries only; a test resolver; opens a baked and an unbaked fixture; lookups with a caller-built `TextureOpt` (`samples` 1, box filter, the probe's footprints) and `TexturePerthread`; results formatted with `%f` must equal `tools/master-lookup-values.txt` (recorded by the starting tree's renderer, research R15) | build failure in a scratch tree with the option on: `textureSystem.h` missing; `shading.h`, `renderer.h`, `options.h`, `attributes.h` not found on the narrow path | US1 AS1/AS4, FR-007, FR-018(a), SC-003 |
| T-C1 | `TextureSystem_ConcurrentFirstLoad` (`tests/unit/texture_tile`, labels `texture_tile;unit`, links `ribRender`, run at `-j2` through `cpujob`): 4 threads behind a barrier, 200 repeats, a fresh system each; a counting resolver with a 1 ms delay; asserts one handle, one locate, one `CODE_NOFILE` for a missing name. Plain build, no ThreadSanitizer | build failure (API missing); then **by assertion** at phase 5's adapter step, before the lock | FR-016, FR-018(c), Edge Cases (first-use races), #20 |
| T-K1 | `TextureSystem_StreamCursor` (`tests/unit/texture_tile`, links `ribRender`): dimensions consumed per lookup class (data-model §5), the side texture drawing from its environment's stream, the advanced `stream.dim` visible to the caller | build failure (API missing) | FR-005, FR-018(d), US2 AS4 |
| T-S1 | `ShadingParity_TextureSubstitute` (`tests/shading_parity`, JIT builds only): substitute services whose texture system records every call; one probe shader per builtin as `.rslo` and `.slo`; each backend's log equals its expected request (research R16); handle requests, `textureinfo()` and 3D requests equal across backends; canned values returned; then no system: fill values and one warning per kind | build failure (`CRendererServices::textureSystem` missing) | US3 AS1–AS4, FR-009–FR-012, FR-018(b), SC-005, SC-008 |
| T-D1 | `Display_RgbeBytes` (`tests/unit/image_input`): the `rgbe` driver's output bytes against a reference written by the starting tree's driver | **characterization**: green on both trees by design | US4 AS2, FR-017, SC-009 |
| T-A1 | `ctest -L architecture` after each phase's deletions | `VIOLATION` for each deleted line until its phase lands; any other violation means the design is wrong — stop and report | FR-019, SC-001, SC-002 |

Existing guards whose assertions do not change: the nine `texture_tile` tests (including `TileSource_TiffInfo` and
`TileSource_SynthesizedInfo`, which keep compiling against `createTiffTileSource`/`createSynthesizedTileSource`,
FR-008); `ImageInput_*`; `Parity_{texture,unbaked}-*`, `Parity_env`, `Parity_shadow`;
`Visual_sphere-{occlusion,texture3d-bake3d,textureinfo}-reyes{,-slo}`; the occlusion `ShadingParity_JitPL*`;
`ShadingServices_*` (the services doubles follow the interface); `DepthDeterminism_*`, `ColorDeterminism_*` and the
sweep's texture scenes; `Common_StateOwnership`.

## Risks

- **A changed image in a behaviour-preserving phase.** CP-d compares every image, the JIT texture probes included,
  because no `-slo` test covers the JIT texture paths (research R16). Any difference stops the phase; it is never
  absorbed by a threshold or a re-render.
- **Sample-stream drift** (a lookup consuming a different number of dimensions, or the write-back missed at one
  call site): T-K1 pins the counts; the stochastic texture scenes and the determinism tests catch the rest.
- **A new `globalArena` race** if texture loads stopped holding `shaderMutex` (research R6): avoided by the load-lock
  setting; a reviewer should check that no other path lost that lock.
- **Self-deadlock on `shaderMutex`** (plain pthread mutex): `loadMutex` is set to it only in the step that removes the
  services' wrapper (phase 6). The only `shaderMutex` holders are the services getters (`rendererServicesImpl.h`);
  none of them (`getCache`, `getTexture3d`, `getPhotonMap`) reaches a texture handle while holding it, and the
  occlusion setup acquires its cache and its environment one after the other, not nested.
- **Transitive includes** after `texture.h` drops `attributes.h` and `texture3d.h` drops `xform.h`: fixed only with
  allowed headers (State headers in Shading, Hiders or the core; infrastructure headers in Texturing). A hider file
  never gains a Texturing include. If Texturing would need a foreign header, stop and report.
- **Lifetime**: a lookup reaching a destroyed system. The contexts and their cached handles die before the system
  in `endFrame()`; the services pointer is cleared there; T-S1's no-system part checks the null path.
- **The Texturing-only library and archcheck**: behind the option until phase 8; the first run with it on is
  recorded (025's R11 result predicts no change in any reported number).
- **Linux**: `FILE`-mangled lines handled per platform; the five known Linux failures (#45, #46) are the baseline there
  and must not spread.
- **Known flaky tests** (memory `reference_ctest_flaky_tests.md`): a red one is re-run alone before it is written off;
  `ShadingParity_AmbientAccumulation` is a real heap bug.
- **Merge order with spec 025** (in implementation on its own branch): both edit `renderer.{h,cpp}`,
  `rendererServicesImpl.h`, `irradianceDispatch.cpp` and `shading.{h,cpp}`; whichever merges second rebases and
  re-runs its checkpoint. 025 changes no Texturing allowlist line and keeps the `retrieveDisplayChannel` forwarder
  out of line, which FR-015's re-tag relies on.

## Questions for the maintainer

Each is written into the plan with its recommendation applied; a different answer changes the named phase.

**Q1 — FR-010 against FR-020: the JIT does not request the same texture lookup as the interpreter today** (phase 6).
The JIT's `texture()`, `environment()` and `shadow()` take no optional parameter list (the emitter passes only the
positional operands), use the unfiltered point lookup or a zero-width footprint, and read whatever options an earlier
call left in the scratch (research R10; the #55 class of defect, not yet filed for these builtins). FR-010 and US3
AS1 require both backends to send the same request and produce the same result; FR-020 and SC-006 require `.slo`
renders to stay byte-identical. Both cannot hold.
- **Recommended (A)**: amend FR-010, US3 AS1 and SC-008 so each backend keeps today's request, the substitute test
  pins what each sends, and equality across backends is required for the handle request (name, kind),
  `textureinfo()` and the 3D lookups. The maintainer files the JIT gap as its own issue, fixed like #55 on its own
  branch (it changes `.slo` output).
- (B) Fix it in 027 as a last, output-changing phase (an emitter parameter list for the three builtins, the
  interpreter's footprint in the JIT), with the maintainer's approval of any changed image at that time, as 025 did
  for FR-025. Larger scope, and no `-slo` test covers these builtins today.

**Q2 — FR-009's "replacing the five resource getters"** (phases 6–7). With FR-014 keeping the irradiance-cache and
3D-texture entries in the core, the plan replaces `getTexture`, `getEnvironment` and `getTextureInfo` with
`textureSystem()` and keeps `getCache` and `getTexture3d` until 032, adding three services forwarders for the
`texture3d()`/`bake3d()` channel operations. Shading's two irradiance-cache lookups with its context
(`giFunctions.h:235`, `shading.cpp:3393`) are calls of `CTexture3d`'s six-argument lookup and stay as SC-005's "apart
from the 3D lookup" exception. A related reading: FR-002 says an unknown file yields a handle to a substitute; for
the `textureinfo()` kind the plan returns a null handle and caches nothing, which is today's behaviour and what US2 AS3
requires ("exactly as today, including the `textureinfo()` probe of a nonexistent file"). **Recommended**: confirm both
readings (they change no requirement's intent).

**Q3 — #20 is not reachable on master** (phase 5). `CRendererServicesImpl` already holds `CRenderer::shaderMutex`
around every texture getter, and nothing else calls them (research R6). The unlocked sequence #20 names exists in
`CRenderer::getTexture`, so the issue is real in that function but masked by its only caller. The plan's red evidence
for FR-018(c) is (1) a build failure on `bc261b1` and (2) an assertion failure at phase 5's adapter step, after the
sequence moves into the system and before the system's own lock lands; a test against the starting tree through the
services would be green. **Recommended**: accept (1)+(2) as FR-018(c)'s red, keep `Closes #20` (the system is safe
without any caller's lock, which is #20's fix), and keep texture loads under `shaderMutex` through the load-lock
setting. Note: the GI getters (`getPhotonMap`, `getCache`, `getTexture3d`) keep their unlocked sequence on
`frameFiles`, still serialized only by the services wrapper; that is 032's cache.

**Q4 — the pyramid-synthesis lock is process-wide, not owned by the system** (phase 5). FR-001 lists "the mutexes that
serialize … in-memory pyramid synthesis" among the system's. The lock guards `globalArena` (process-wide), and FR-008
keeps `createSynthesizedTileSource()` and its direct unit test, which run with no system. **Recommended**: a
Texturing-owned process-wide `std::mutex` that the system uses (research R13), with FR-001's wording adjusted; the
block-registry and eviction mutex does become the system's. Alternative: a `globalArena` lock in libRIBCommon.

**Q5 — `src/ri/hiders/radiance.cpp` is not compiled** (phase 6). FR-013 names its environment call, but the file is
absent from `src/ri/CMakeLists.txt`, and the call already does not match today's signature (it passes a
`CTextureLookup *` where a `CShadingContext *` is expected), so no build can verify an edit. **Recommended**: leave the
file untouched and drop it from FR-013 (recorded in research R21); deleting the dead file would be a separate cleanup.

**Q6 — FR-018(d)'s "equals master's" cursor position** (phase 6). The starting tree has no option block, and
`drawCursorDim` is a protected member of an abstract context, so no test can read master's cursor directly. T-K1
asserts the dimensions each lookup consumes against a table derived from the starting tree's code (data-model §5),
and the byte-identical stochastic renders and determinism tests prove the sequence end to end. **Recommended**:
accept that as FR-018(d)'s "equals master's".

Recorded, not questions: the within-family name collision (one name used with `texture()` and `environment()`) keeps
today's undefined behaviour; the maintainer may want an issue for it (research R5). `rsl_texture_c_scalar` has no
caller; it is routed, not deleted (research R21).

## Complexity Tracking

None. The Constitution Check has no violation to justify.
