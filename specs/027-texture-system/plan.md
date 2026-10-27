# Implementation Plan: Texture System

**Branch**: `027-texture-system` | **Date**: 2026-10-27 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/027-texture-system/spec.md`. The Clarifications of session 2026-10-27,
the maintainer's five plan decisions and the answers to the six plan questions of the same day (all recorded there)
are applied as decided. Revised 2026-10-27 for the answers: the JIT parity fix (FR-022) is phase 9. Q7 (the ray-traced
forms under the JIT) is answered, option A (spec.md Clarifications), and the plan applies it; its text is kept at the end.

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
6. **JIT parity of the texture builtins** (FR-022, answer to Q1): under the JIT, `texture()`, `environment()` and
   `shadow()` honour their optional parameter lists and make the interpreter's request, through #55's
   `jitParameterList<T>()`/`jitParameterListReady()` and `emitParamList()`. It is the last code phase and the only
   one allowed to change output, `.slo` only. Measured today: 16 of 18 texture/environment parity scenes differ
   under `.slo` (MaxBlockAvgDiff 34.55–252.52); no existing test or reference renders these builtins through the
   JIT, so none is expected to change.
7. **Guardrails**: 25 DB-010 and 5 DB-026 allowlist lines deleted, the two `texture3d.cpp` lines re-tagged to DB-021,
   none added; Texturing's renderer-core metric 70/16 → 1/1; registry updated.

Tests are written first: a Texturing-only program (no renderer-core, hider or shading object in the link), a
substitute texture system under both backends, #20's concurrent first-load test (4 threads, 200 repeats), a
sample-stream test, and an RGBE display-driver byte test. Every phase keeps the architecture check green and every
image byte-identical on macOS, except phase 9, which may change `.slo` renders of shaders that call `texture()`,
`environment()` or `shadow()` and nothing else (the JIT texture paths are byte-identical in every other phase).

## Technical Context

**Language/Version**: C++20 (`-Werror`). CMake ≥ 3.19.

**Primary Dependencies**: no new dependency. libRIBCommon (sampling types, diagnostics, `CTrie`, `CMemPage`,
`TMutex`), libtiff, libpng, OpenEXR (optional), `libshader_shading`, `libRIBRender`.

**Storage**: N/A. Memory only; the opt-in disk cache (spec 020) keeps its files and format.

**Testing**: ctest.
- `texture_tile` (run at `-j2`): `TextureSystem_ConcurrentFirstLoad`, `TextureSystem_ConcurrentFirstLoadExternalMutex`,
  `TextureSystem_StreamCursor`, `TextureSystem_InfoMiss` and `TextureSystem_NullResolver` join the nine existing tests.
- `texture_only` (new, behind `OPENRENDER_TEXTURE_ONLY_TESTS`): `TextureOnly_Standalone`.
- `shading_parity`: `ShadingParity_TextureSubstitute` (registered always; its interpreter half runs with the JIT off, the `.slo` half and the cross-backend checks only with it; 2D, `textureinfo()` and no-system parts, green
  in phase 6), `ShadingParity_TextureSubstitute3D` (the `texture3d()`/`bake3d()` forwarders, green in phase 7; a separate
  executable because the forwarders exist in the interface only from phase 7), `ShadingParity_JitPLDiagnosticsTexture`
  (new, T-J3; JIT builds only).
- `visual` (JIT builds only; registered in phase 2, red until phase 9): `ShadingParity_JitPL_{texture,envshadow}_<case>` and
  `ShadingParity_JitPLEngaged_{texture,envshadow}_<case>_{rslo,slo}`, extending #55's table, with the ray-traced
  cases `ShadingParity_JitPL_envshadow_raytrace_*` (Q7).
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
| Shading | phase 9: `llvmEmitter.cpp` (compiler), `rslOps.{h,cpp}` (the `_v3` entry points); `rendererServices.h`, `textureCalls.{h,cpp}` (new), `shading.{h,cpp}`, `shaderFunctions.h`, `giFunctions.h`, `shaderPl.{h,cpp}`, `rslBuiltins.cpp`, `execute.cpp` (includes) |
| Hiders | `irradianceDispatch.cpp`: two environment lookups through `CShadingContext::lookupEnvironment()` |
| Display Outputs | `src/display/rgbe/`: the codec leaves; `file.cpp` includes `common/rgbe.h` |
| Infrastructure | `src/common/rgbe.{h,cpp}` (moved in) |
| Renderer core | `renderer.{h,cpp}`, `rendererStatics.cpp`, `rendererMutexes.cpp`, `rendererFiles.cpp`, `searchPathResolver.h` (new), `rendererFileSearch.cpp`, `rendererContext.cpp` (`RiMake*`), `rendererServicesImpl.h` |
| Consumers | tests; `otexmake` unchanged (it goes through `RiMake*`) |

Graphics State, Geometry and Scene Interface are not edited. `texture3d.cpp` is not edited (FR-015).

**Performance Goals**: no measurable change (SC-010): one struct built and one virtual call per builtin call; the
lookup arithmetic is unchanged.

**Constraints**:
- byte-identical renders in every phase but phase 9, on macOS, including the JIT texture paths (FR-020, SC-006);
  phase 9 may change `.slo` renders of `texture()`/`environment()`/`shadow()` only, and any change to an existing
  test result or reference stops it for the maintainer (FR-022);
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
| III. TDD (non-negotiable) | PASS (by plan order) | Phase 2 writes the tests and records their red before any production change (see "Test design"). Four are red by build failure, as in 024's T-P5 and 025's T-S1; #20's test is also red by assertion at phase 5's adapter step; the T-J tests are red by image difference until phase 9 (measured, R22); one test is a characterization test, green on both trees by design and labelled so. |
| IV. CLI | N/A | No CLI change. |
| V. Minimal dependencies | PASS | None added. |
| VI. Linux and macOS | PASS | Both platforms measured before and after (quickstart §0, §5); GCC 13 `-Werror`; platform-tagged allowlist lines handled per platform. |
| VII. Docs | PASS | `DOMAIN_BOUNDARIES.md`, the allowlist and metric headers, DEVNOTES, `CLAUDE.md` (Architecture, a Known gotcha on the texture system's lifetime, the services' texture part and the core-lock coupling, and the shipped shader counts, which grow by the two T-J probe shaders as 024's did; **pending the maintainer's explicit authorization at implementation time, never touching "Spec-kit agent rules"**), NEWS.md and ChangeLog.md (one line for phase 9's visible JIT change). No user-facing page changes: rendering does not change. |
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
│   ├── archcheck-bc261b1-macos.txt, libshader-dynlookup-{before,bc261b1}-macos.txt   # plan stage, kept as history
│   ├── archcheck-base-macos.txt, libshader-dynlookup-base-macos.txt, core-usage-texturing-base.txt, timing-base-macos.tsv   # phase 1, on BASE
│   ├── image_manifest.sh       # phase 1: spec 025's tool, copied into this spec's tools/
│   ├── jit_texture_probes.sh   # phase 1: renders the texture/env/shadow/unbaked parity scenes as .slo
│   ├── lookup-probe.rib, lookupprobe.sl, master-lookup-values.txt     # phase 1: SC-003 values
│   ├── time_texture_scenes.sh, timing-*.tsv                           # phase 1 and 10: SC-010
│   ├── jit_texture_divergence.sh, jit-texture-divergence-*.tsv        # plan stage, phase 1 and 9: the JIT/interpreter divergence
│   ├── characterization_scenes.sh, scenes/                            # phase 2: occlusion/indirectdiffuse with "environmentmap", point hierarchy
│   ├── baseline-images.sha256, baseline-image-noise.txt, baseline-diffs.txt   # re-recorded after phase 2 (the *-p1 files are phase 1's)
│   ├── baseline-images-p1.sha256, baseline-diffs-p1.txt, baseline-image-noise-p1.txt
│   ├── baseline-images-p9.sha256, baseline-diffs-p9.txt               # phase 9: phase 10's CP-d reference
│   └── 027-linux-manifest.sha256                                      # phase 10: the Linux sync manifest
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
src/libshader/compiler/llvmEmitter.cpp # phase 9: texture/environment/shadow forms by prototype, emitParamList()
src/libshader/shading/rslOps.{h,cpp}   # phase 9: op_texture_{f,c}_v3, op_texture_full_{f,c}_v3, op_environment_{f,c}_v3, op_shadow_{f,c}_v3; - the _v2 names
shaders/jit_pl_texture_probe.sl, shaders/jit_pl_envshadow_probe.sl   # phase 2 (test shaders, built with the others; CLAUDE.md shader counts)
examples/rib/tests/jit-texture-probe.rib.in   # phase 2: template of the T-J scenes
src/ri/hiders/irradianceDispatch.cpp   # two environment lookups through lookupEnvironment()
tests/
├── unit/texture_tile/           # + test_texture_system_concurrent_first_load.cpp, test_texture_system_stream_cursor.cpp
├── unit/texture_only/           # NEW: CMakeLists.txt (behind OPENRENDER_TEXTURE_ONLY_TESTS), test_texture_only_standalone.cpp
├── unit/image_input/            # + Display_RgbeBytes (script, rgbe-display.rib, references/rgbe-display.hdr)
├── unit/common/owned-symbols.txt  # + the six RGBE names
├── visual/CMakeLists.txt        # + the T-J cases in #55's table
├── shading_parity/              # + test_texture_substitute.cpp, test_texture_substitute_3d.cpp, test_jit_paramlist_diagnostics_texture.cpp (T-J3), texture probe shaders; table_services.h, recording_services.h
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
   exactly as the wrapper does today (passing it while the wrapper still holds it would self-deadlock). The coupling is
   documented (registry note, CLAUDE.md gotcha): `loadMutex` is the core's non-recursive `shaderMutex`, so nothing that holds it
   may call `handle()`; `handle()` carries a debug re-entry assertion and `TextureSystem_ConcurrentFirstLoadExternalMutex` runs
   the production configuration.
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
12. **JIT parity** (R22): form from the prototype, #55's parameter-list machinery, one footprint helper for both
    backends, new `_v3` entry points; tests in #55's style; the only output-changing phase.
13. **Guardrails** (R17): line by line in [contracts/allowlist-end-state.md](contracts/allowlist-end-state.md).

## Phases

Every phase ends with the checkpoint below and a pause for the maintainer's review. Lines of the allowlist are
deleted **at the start** of the phase that removes their edges, so its architecture check is red until the code
lands (spec 025's practice). A phase may change no image.

**Checkpoint (CP-a … CP-d)**, quickstart §2:
- **CP-a** build clean under `-Werror`;
- **CP-b** the full suite against phase 1's pass list plus the tests enabled so far (`texture_tile` also alone at
  `-j2`). The T-J parity cases, the `_slo` engaged controls and `ShadingParity_JitPLDiagnosticsTexture` are the known-red
  tests from phase 2 to phase 8 (as spec 025's `State_LightOrder` was); any other red test is a regression. A red test from the known-flaky set is re-run
  alone before it is written off;
- **CP-c** `ctest -L architecture` green; `ALLOWED`, `METRIC`, `RESULT` recorded in validation.md;
- **CP-d** `image_manifest.sh diff`: 0 differing and 0 missing images against the manifest **re-recorded after phase 2**
  (task T035a: the suite's images, the JIT texture probes, the T-J scene outputs and the characterization scenes; `diff`
  lists unrecorded images only as information, so nothing can be compared unless it was recorded), every
  `MaxBlockAvgDiff` equal to the baseline's. Phase 10 compares with phase 9's re-recorded manifest.

| # | Phase | Content | Lines deleted at start | Expected red until done |
|---|---|---|---|---|
| 1 | Baselines | **First bring in spec 025** (merged first by decision; T005a: master merged into this branch with `--no-ff --no-commit`, never a rebase), then re-measure on a fresh macOS build (decision 4): archcheck, members, imports, full suite, manifest and noise, JIT probes, `master-lookup-values.txt`, timing. Copy `image_manifest.sh`; write `jit_texture_probes.sh`, the lookup probe, the timing script. List the Linux baseline run for the maintainer | — | — |
| 2 | Tests first | Write T-T1…T-D1 and the characterization scenes (table below); record each red; write `Display_RgbeBytes` and its reference from the starting tree; **re-record the image baseline** so it contains the T-J scenes and the characterization scenes (T035a) | — | the red tests (build failures) |
| 3 | RGBE codec (US4) | Move the codec, `extern "C"`, CMake, `owned-symbols.txt`; DB-026 → Resolved | 5 DB-026 | 3 `VIOLATION` (macOS) |
| 4 | Resolver and State includes | `CTextureFileResolver`, `CSearchPathResolver`; `texmake`/`brickmap`/loaders/`RiMake*` use it; drop the unused `attributes.h`, `xform.h`, `options.h` includes | 13 DB-010 | 13 `VIOLATION` |
| 5 | The system owns the state | `CTextureSystem` with `CTextureCache`, `TexturePerthread`, settings, the family cache split from `frameFiles`; loaders and init/shutdown move; core constructs/destroys per frame; the services' texture getters forward to it for now; lookups still take the context and reach their per-thread state through their cache. **#20**: the find-load-insert first moves verbatim (enable `TextureSystem_ConcurrentFirstLoad`: red by assertion), then the system's own load lock lands (green). `loadMutex` stays `nullptr` in this phase: the services' getters still hold `shaderMutex` around their forward, and it is a plain, non-recursive pthread mutex (`os.cpp:419-425`) | 10 DB-010 | 10 `VIOLATION`; #20's test between the two steps |
| 6 | Option block and services | `TextureOpt`/`TexturePerthread`/handles in the lookups; `textureSystem()` in the services, three getters removed; `textureCalls`; every 2D/environment/shadow/textureinfo call site of both backends, `rslBuiltins.cpp`, the hider; no-system rule; in the step that deletes the getters and their `shaderMutex` wrapper, the core starts passing `&CRenderer::shaderMutex` as `loadMutex`; enable `TextureSystem_StreamCursor` and `ShadingParity_TextureSubstitute` (2D, info, no-system) | 1 DB-010 | 1 `VIOLATION` |
| 7 | 3D through the services, point hierarchy | The three 3D forwarders and the `texture3d()`/`bake3d()` call sites; `CPointHierarchy::Options`; imports 7 → 6; enable `ShadingParity_TextureSubstitute3D` | 1 DB-010 | 1 `VIOLATION` |
| 8 | Texturing stands alone | `OPENRENDER_TEXTURE_ONLY_TESTS` default ON; `TextureOnly_Standalone` green against `master-lookup-values.txt`; first archcheck run with the doubly compiled units | — | — |
| 9 | JIT parity of `texture()`/`environment()`/`shadow()` (FR-022) — **the only output-changing phase, `.slo` only** | Emitter: form from the prototype, positional operands per prototype, the rest through `emitParamList()`; `op_texture_{f,c}_v3`, `op_texture_full_{f,c}_v3`, `op_environment_{f,c}_v3`, `op_shadow_{f,c}_v3` (the `_v2` names deleted, #55's rule); run time through `jitParameterList<CTextureLookup>()`/`<CEnvironmentLookup>()` + `jitParameterListReady()` and one footprint helper shared with the interpreter; the traced names per Q7. The T-J tests turn green; the substitute test's JIT expectations switch to "equal to the interpreter's" | — | the T-J parity and `_slo` engaged tests (red since phase 2) Also (maintainer, 2026-10-27): the seven `.slo` raytrace texture/unbaked probe renders, unstable run to run on the starting tree, must be bit-stable over four runs and equal to `.rslo` (T107a); they leave the noise list and the post-Phase-9 manifest byte-checks them. |
| 10 | Registry, docs, Linux | Re-tag the 2 `texture3d.cpp` lines to DB-021; DB-010/DB-021/DB-001 text (DB-026 was moved to Resolved in phase 3); metric baseline 1/1 and the TOTAL row as reported; DEVNOTES, CLAUDE.md; timing against the baseline; the determinism sweep; the maintainer's Linux run (quickstart §5) | — | — |

Phase 3 can ship alone (spec US4). Phases 4–8 depend in order; phase 9 needs phase 6's `textureCalls` sequence. Phase 10 edits no rendering code (allowlist data, registry, docs, measurements and the Linux run), so phase 9 is the last phase that changes code and the only one that changes output; phase 10's checkpoint is CP-d against phase 9's images.

**Phase 9's checkpoint** replaces CP-d with CP-d9:
- every `.rslo` image and every image of a scene that does not call these builtins is bit-identical to the baseline
  (expected: every image of the suite, R22);
- the JIT texture probes change and are now bit-identical to their `.rslo` renders;
- every `ShadingParity_JitPL_*` case added here passes, every new `_slo` engaged control passes, every existing
  `MaxBlockAvgDiff` equals the baseline's.
- **STOP task**: if any existing test result, `MaxBlockAvgDiff` or reference changes, list the scene, the old and new
  value and the reason, and stop for the maintainer. No re-render without the maintainer's explicit approval at that
  time; no threshold is widened.

## Test design (red first)

| Id | Test | Red on the starting tree because | Covers |
|---|---|---|---|
| T-T1 | `TextureOnly_Standalone` (`tests/unit/texture_only`, label `texture_only;unit`, behind `OPENRENDER_TEXTURE_ONLY_TESTS`): links `texturing_only` + libRIBCommon + image libraries only; a test resolver; opens a baked and an unbaked fixture; eight-coordinate `texture()` lookups only (environment and shadow take one direction/point and Shading derives their footprint; T-K1 and the renders cover them) with a caller-built `TextureOpt` (`samples` 1, box filter, the probe's footprints) and `TexturePerthread`; results formatted with `%f` must equal `tools/master-lookup-values.txt` (recorded by the starting tree's renderer, research R15) | build failure in a scratch tree with the option on: `textureSystem.h` missing; `shading.h`, `renderer.h`, `options.h`, `attributes.h` not found on the narrow path | US1 AS1/AS4, FR-007, FR-018(a), SC-003 |
| T-C1 | `TextureSystem_ConcurrentFirstLoad` (`tests/unit/texture_tile`, labels `texture_tile;unit`, links `ribRender`, run at `-j2` through `cpujob`): 4 threads behind a barrier, 200 repeats, a fresh system each; a counting resolver with a 1 ms delay; asserts one handle, one locate, one `CODE_NOFILE` for a missing name. Plain build, no ThreadSanitizer. A second entry, `TextureSystem_ConcurrentFirstLoadExternalMutex`, passes a caller-owned non-recursive mutex as `loadMutex` (the production `&shaderMutex` configuration) and checks the mutex is free afterwards; `handle()` carries a debug re-entry assertion | build failure (API missing); then **by assertion** at phase 5's adapter step, before the lock | FR-016, FR-018(c), Edge Cases (first-use races), #20 |
| T-I1 | `TextureSystem_InfoMiss` and `TextureSystem_NullResolver` (`tests/unit/texture_tile`, one executable): an `Info` request for a missing file returns a null handle, caches nothing (a second request locates again: locate count 2) and reports nothing; the make*/loader functions tolerate a null resolver (`tests/preview/test_data_keys.cpp:54` passes a NULL search path) | build failure (API missing) | FR-002, plan Q2, US2 AS3 |
| T-X1 | characterization scenes (`tools/scenes/`, hashed by the manifest, not ctest entries): `occlusion()`/`indirectdiffuse()` with `"environmentmap"` under reyes and raytrace, `-slo` variants, and a point-hierarchy occlusion with a non-default `maxsolidangle`; each gets a scratch mutation at its green point | green on both trees by design (characterization; the hash of the starting tree is the reference) | SC-006, FR-013, FR-014 |
| T-K1 | `TextureSystem_StreamCursor` (`tests/unit/texture_tile`, links `ribRender`): dimensions consumed per lookup class (data-model §5), the side texture drawing from its environment's stream, the advanced `stream.dim` visible to the caller | build failure (API missing) | FR-005, FR-018(d), US2 AS4 |
| T-S1 | `ShadingParity_TextureSubstitute` and `ShadingParity_TextureSubstitute3D` (`tests/shading_parity`, JIT builds only; two executables, the second needs the phase 7 forwarders): substitute services whose texture system records every call; one probe shader per builtin as `.rslo` and `.slo`, none using a `"raytrace"` name; each backend's log equals its expected request (research R16); handle requests, `textureinfo()` and 3D requests equal across backends; canned values returned; then no system: fill values and one warning per kind | build failure (`CRendererServices::textureSystem` missing) | US3 AS1–AS4, FR-009–FR-012, FR-018(b), SC-005, SC-008 |
| T-J1 | `ShadingParity_JitPL_texture_<case>`, `ShadingParity_JitPL_envshadow_<case>` (`tests/visual`, #55's table, JIT builds only; probes `jit_pl_texture_probe.sl`, `jit_pl_envshadow_probe.sl`; scenes from `jit-texture-probe.rib.in`): each case under `.rslo` and `.slo`, bit-identical. Cases per research R22 (`base`, `blur`, `width`, `swidth_twidth`, `fill`, `samples`, `filter`; and `bias` for shadow), plus the ray-traced cases of Q7 (`raytrace_base`, `raytrace_samples_cone`, `raytrace_bias`, `raytrace_maxdist`, `raytrace_blur`, registered as `ShadingParity_JitPL_envshadow_raytrace_*`) | **by image difference**: the JIT drops the list, picks the float form for a color result and makes a point or zero-width request (measured, R22); written and registered in phase 2 and red until phase 9: the known red tests of every checkpoint in between | FR-010, FR-022, FR-018(e), SC-012 |
| T-J2 | `ShadingParity_JitPLEngaged_{texture,envshadow}_<case>_{rslo,slo}`: base against case, same format, on the channels the case affects, must differ | the `_slo` controls fail (the JIT ignores the parameter); the `_rslo` ones pass on both trees | FR-022, FR-018(e) (a parity test cannot pass for the wrong reason) |
| T-J3 | `ShadingParity_JitPLDiagnosticsTexture`, a **new** test (not an extension of `ShadingParity_JitPLDiagnostics`, which is green today and must stay so): an unknown parameter of `texture()`, `environment()` and `shadow()` is reported once under both backends | the JIT reports nothing (the list is dropped) | FR-022 |
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
- **Merge order with spec 025** (decided by the maintainer 2026-10-27: **025 merges first**): both edit `renderer.{h,cpp}`,
  `rendererServicesImpl.h`, `irradianceDispatch.cpp` and `shading.{h,cpp}`. Task T005a waits for 025 on master, then merges
  master into this branch with `--no-ff --no-commit` for the maintainer to commit (never a rebase), and re-measures the
  starting numbers (the allowlist without the 7 DB-008 lines, the metric, the touched files) before any baseline is taken;
  the end figures (`652 / 98`) become "the re-measured TOTAL minus 69 / 12". 025 changes no Texturing allowlist line and keeps the `retrieveDisplayChannel` forwarder
  out of line, which FR-015's re-tag relies on.

## Maintainer answers (2026-10-27), applied

Q1 fix in 027, as phase 9 (FR-022); Q2 three getters replaced, `getCache`/`getTexture3d` stay for 032, the two
irradiance-cache lookups under SC-005's 3D exception, `textureinfo()` on a missing file returns a null handle; Q3
#20's red is the build failure plus the phase-5 adapter assertion, `Closes #20` kept; Q4 the synthesis lock is a
process-wide Texturing mutex (FR-001 reworded); Q5 `radiance.cpp` untouched, dropped from FR-013; Q6 FR-018(d)
checks the dimension table, the renders prove the sequence. The same-name `texture()`/`environment()` behaviour is
GitHub #104 (out of scope). GitHub #97 is narrowed to `texture3d`/`bake3d`/`photonmap`/`filterstep`, so the merge
message cannot carry `Closes #97`; it carries `Closes #20` and `Closes #33`, both written by the maintainer.

The original questions are kept below for the record.

## Question 7 (answered 2026-10-27: option A, recorded in spec.md Clarifications)

**Q7 — `environment("raytrace")` and `shadow("raytrace")` under the JIT** (phase 9). The interpreter ray-traces when
the map name is `"raytrace"` (or the builtin's own name, `"reflection"`/`"shadow"`), using `samples`, `bias`,
`samplecone`, `maxdist` and `blur` from the same parameter list. The JIT looks the name up as a file, gets the
substitute and prints `"Failed open environment \"raytrace\""`. FR-022, FR-018(e) and SC-012 now name it (clarification 2026-10-27, Q7), and FR-010's "same result" and the parameter
lists phase 9 adds reach it.
- **Chosen (A), maintainer 2026-10-27**: include it in phase 9. The traced branch builds the interpreter's `CTraceLocation` batch from the
  same bound `CEnvironmentLookup` and reuses the trace machinery `jitTraceBatch` already drives for #55; add
  `JitPL_envshadow_raytrace_*` cases. No existing test renders it, so no output changes beyond the probes.
- (B, not chosen) Leave it out of 027 and file it as its own issue; T-J1 then has no traced case and FR-010 gets a named exception.

## Questions for the maintainer (answered above)

**Q1 — FR-010 against FR-020: the JIT does not request the same texture lookup as the interpreter today** (phase 6). **Answered 2026-10-27: option B, fix it in 027 as phase 9 (FR-022).** The text below is the question as asked, kept for the record.
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
(`giFunctions.h:235`, `shading.cpp:3394`) are calls of `CTexture3d`'s six-argument lookup and stay as SC-005's "apart
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

Recorded, not questions (historical; resolved since): the within-family name collision (one name used with `texture()` and
`environment()`) keeps today's undefined behaviour and is filed as GitHub #104, out of scope (research R5). `rsl_texture_c_scalar` has no
caller; it is routed, not deleted (research R21).

## Complexity Tracking

None. The Constitution Check has no violation to justify.
