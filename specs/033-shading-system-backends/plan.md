# Implementation Plan: Shading System Backends

**Branch**: `033-shading-system-backends` | **Date**: 2026-11-14 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/033-shading-system-backends/spec.md`, clarified 2026-11-13 (FR-005 cache
kept with #167; FR-011 one layer only; FR-014 #115 and #121 fixed neither; #134 in; 033 merges before 057; the
`pendingInstanceKey` leak moved, not fixed).

**Status of this plan**: **approved by the maintainer on 2026-11-14**: every decision P-Q1 to P-Q10 as recommended, plus the
library-name decision (below, "Decisions approved"). Tasks: [tasks.md](tasks.md). No decision of
`RENDER_ARCHITECTURE_REFERENCES.md` changes (D8, D9, D12, D22-D25, D27 and section 5.4 are applied as written; see
"Architecture decisions relied on").

## Summary

The renderer core stops choosing, loading, binding, initialising and running shaders by concrete type. Shading gets
one facade, `ShadingSystem`, over registered `IShaderBackend`s (the RSL interpreter, `.rslo`, and the RSL JIT, `.slo`),
and the core asks it for a shader by name, type and the scene's two `shaderformat` values. Nine parts:

1. **The bind-time init moves into Shading** (`src/ri/render/init.cpp` -> `src/libshader/shading/rsloInit.cpp`, the JIT
   half into the JIT backend). Its 19 coordinate-system lookups and its variable lookup go through the renderer services
   (`getMatrix` with a bind-time phase, `lookupVariable`), answered exactly as today (#134, FR-009).
2. **`CShader` splits** into the backend-neutral description (it keeps the name `CShader`, and `type`, `parameters`,
   `usedParameters`, `flags`, `data`) and a backend-private payload (the interpreter's bytecode and tables, the JIT's
   entries); `shader.h` stops including LLVM headers (FR-007).
3. **Execution dispatches by backend**: `CShadingContext::execute()` keeps its name and becomes the dispatcher; the
   bytecode loop becomes `interpret()` with no JIT code; the JIT block (nested-context save, ambient `Cl`, the "no
   bytecode" warning) moves verbatim into the JIT backend (FR-010).
4. **`ShadingSystem`** owns the loaded-shader cache (by name first, #167 kept), the format precedence, the backend order
   and the binding; a Shading-owned `IShaderFileResolver` (the core wraps `locateFileEx`) and `IShaderBindHost` (the core
   supplies the instance key and the light list) keep every step and diagnostic in today's order (FR-001 to FR-006,
   FR-008).
5. **The group-shaped API**: every binding is a `ShaderGroupBuilder` group of one layer, which is its layer's instance;
   `connect()` and a second layer report "not supported before spec 034"; `shade()` and the imager run groups through
   `groupPrepare`/`groupExecute` (FR-011, FR-012).
6. **JIT message passing through the services**: `op_lightsource_f` stops reading `getParameterShape()` and
   `shadername(type)` stops reading the attribute record; both backends' requests are recorded and compared (FR-013,
   FR-014). These and part 1 need **renderer services 2.4.0** (P-Q1).
7. **A metadata API** (`ShaderMetadata.h`, library `libshader_metadata`) that `sloinfo` uses alone (FR-016 to FR-018).
8. **The shading library is renamed `libRSLRuntime`** (maintainer, 2026-11-14; Phase 8): `libshader_shading` becomes
   `libRSLRuntime` with the old target, export and file names kept for one release; the inspection reader
   `libshader_runtime` is not merged into it ("Library decision").
9. **Checks and records**: three name scans in the `architecture` label, a precedence characterisation and a bind-time
   characterisation recorded on the unchanged tree, DB-007 resolved, DB-018 and DB-024 rewritten (FR-003, FR-022,
   FR-026).

No image changes in any phase; the Phase 1 manifests (visual suite and determinism sweep, per platform), the spec 026
and 028 replays (never re-recorded), and the Phase 1 characterisations prove it at every checkpoint. The light-shader
path is moved, not redesigned; its entry points before and after are recorded for spec 057 in
[contracts/light-shader-entry-points.md](contracts/light-shader-entry-points.md) (FR-029).

## Technical Context

**Language/Version**: C++20 (`-Werror`), C17 for the plug-in headers (untouched); CMake >= 3.19.

**Primary Dependencies**: none new. `libRIBCommon`, `libRIBShadeop`, `libRSLRuntime` (today's `libshader_shading`,
renamed in Phase 8), `libshader_runtime` (kept: the `.rslo` inspection reader, its `rslo.h` is a public installed
header), `libshader_jitmeta` (kept); LLVM only for the existing JIT. New internal static library `libshader_metadata`
(P-Q7).

**Storage**: N/A; memory only. New recorded files are test references (precedence, bind-time spaces, message requests,
`sloinfo` outputs) and the Phase 1 manifests and timings under `tools/`.

**Testing**: ctest. New label `shaderformat`; additions to `shading_contract`, `libshader`, `architecture`; a new
`tests/shading_system/` directory for the facade's unit tests (label `shading_system`). Red-first targets are gated by a
temporary option `OPENRENDER_033_RED_TESTS` (as spec 029's DEP-2) and join the normal build in the phase that makes each
green; the option is removed in Phase 9. Suites at every checkpoint: the full run (`-LE determinism-sweep`),
`architecture`, `shading_contract`, `shading_parity`, `libshader`, `^determinism$`, `shaderformat`; the sweep once per
platform at the end.

**Target Platform**: macOS (Apple Silicon, Apple clang, vcpkg, 13.3 target; trees `build` and `build-nojit`) and Linux
(Ubuntu 24.04, GCC 13, `-Werror`, `~/Projects/Development/CLI/openrender-033`, remote `cpujob -j2`).

**Project Type**: renderer library and CLI tools.

**Domains touched**:

| Domain | What changes |
|---|---|
| Shading | New: `shadingSystem.{h,cpp}`, `shaderBackend.h`, `shaderPayload.h`, `shaderGroup.{h,cpp}`, `rsloBackend.cpp`, `sloBackend.cpp`, `rsloInit.cpp` (moved from `src/ri/render/init.cpp`), `shaderBind.cpp` (`bindCoordinateSystem`); `src/libshader/include/openrender/ShaderMetadata.h`; `src/libshader/metadata/` (`libshader_metadata`: `shaderMetadata.cpp`, `rsloMetadataReader.cpp`, `sloMetadataReader.cpp`, `CMakeLists.txt`). Edited: `shader.{h,cpp}`, `execute.cpp` (`interpret`, JIT block out), `shading.{h,cpp}` (`execute` dispatcher, `interpret`, `shaderName`, run sites), `rslo.y` (`shaderCreate` builds the payload), `sloLoader.cpp`, `rslOps.{h,cpp}` (`op_lightsource_f`), `shaderRefs.h` (`lightInstanceOf`), `serviceTypes.h`, `rendererServices.h` (2.4.0), `CMakeLists.txt` |
| Renderer core | `rendererContext.{h,cpp}` (`getShader`, `IShaderBindHost`, `init` removed), `rendererFiles.cpp` (`getShader` forwarders, no loader names), `renderer.h`, `rendererStatics.cpp` (`shadingSystem` replaces `globalFiles`), `imager.cpp` (group run), `rendererShadingServices.cpp` + `rendererServicesImpl.h` (2.4.0), `searchPathResolver.h` (`CShaderSearchPathResolver`), `rendererFileSearch.cpp` (comment), `src/ri/CMakeLists.txt`; deleted: `render/init.cpp` |
| Consumers | `src/sloinfo/sloinfo.cpp`, `src/sloinfo/CMakeLists.txt` |
| Tests | new `tests/shading_system/`, `tests/shaderformat/`, cases in `tests/shading_contract/`, `src/libshader/tests/`, three checks and fixtures in `tests/architecture/`; **no approved test edited for the move** (P-Q6); **two approved test files edited for the library rename, a third proposed, each only with the maintainer's approval** (Phase 8 TEST-APPROVAL below) |
| Docs | `DEVNOTES.md`, `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md`, `DEVNOTES_DETAILS/OSHADER_UPDATES.md`, `claude-md-draft.md` (spec dir); for the rename also `INSTALL.md`, `INSTALL_ARTIFACTS.md`, `NEWS.md`, the comment block of `CMake/openRenderConfig.cmake.in`; `DEVNOTES_DETAILS/BINDINGS_GUIDE.md` is re-grepped (it names no library today: nothing to change unless the grep finds one) |
| Build and packaging (rename, Phase 8) | `src/libshader/shading/CMakeLists.txt` (target `libRSLRuntime`, `OUTPUT_NAME RSLRuntime`, `EXPORT_NAME RSLRuntime`, ALIAS `libshader_shading`), `src/ri/CMakeLists.txt` (the link line and the compat symlink list), `CMake/openRenderConfig.cmake.in` (exported alias `openRender::shader_shading`), `src/libshader/CMakeLists.txt` and `src/CMakeLists.txt` (comments) |

**Not edited**: `libshader_compiler` and its name (spec 043 renames it `libRSLCompile`: out of scope here); every Graphics State, Texturing, Geometry, Hiders (photon emission included), Scene Interface, Display,
Shader DSO and Procedures source; the JIT emitter and every `op_*` signature; the `.slo` and `.rslo` formats (A-9);
`libshader_runtime` (the inspection reader keeps its name, files and `rslo`/`rslo_static` aliases); the installed headers (`OPENRENDER_PUBLIC_HEADERS`); `RSLShading.{h,cpp}` (it keeps calling
`execute()`, A-6); the recorded replays and every image reference and manifest; `CLAUDE.md` (draft only);
`RiLightSourceV`/`RiAreaLightSourceV`/`extractLightHandle`/`takeInstanceKey` bodies.

**Performance Goals**: no measurable change (FR-024, SC-009). Added: one virtual call per shader run (the dispatcher),
one uncontended mutex per binding, one services call per `shadername()` and per bind-time named-space lookup (bind time
only).

**Constraints**: byte-identical output per platform, `.rslo` and `.slo`, every hider and thread count; no threshold,
reference, manifest or replay changed; the allowlist gains no line; no task depends on the merge; `cpujob` for every job.

**Scale/Scope**: about 12 new source files, 1 moved, 1 deleted, about 22 edited sources, 6 CMake files and the package
config template; about 9 new tests, 3 new checks; 0 approved test files edited for the move, 2 (a third proposed) for the
library rename, each subject to approval.

**Measurements at the plan stage** (2026-11-14, macOS; details and commands in [research.md](research.md) R0):

- Base build: green (`exit 0`, about 2.5 min at `-j4`). **JIT-off build: green** (`exit 0`, 137 `.rslo`, no `.slo`).
- `archcheck`: pass; 71 `CONSUMER` lines; `sloinfo` 3 (runtime `rslo.h`, `llvmJit.h`, `sloMetadata.h`); `oshader` 3,
  all compiler headers (A-5: none is this spec's); METRIC TOTAL 491/71, Shading 0/0.
- Bind-time init in the core: 19 `CRenderer::findCoordinateSystem` + 1 `CRenderer::retrieveVariable` (preprocessed).
- FR-003 scan on the starting tree: 31 identifier lines + 4 `OPENRENDER_HAVE_LLVM` lines (2 core files).
- Loading benchmark (`sloinfo` x 100, `plastic`): `.rslo` 0.401-0.407 s, `.slo` 0.415-0.436 s (one cold 1.131 s run
  discarded). It measures tool start-up, not the facade (R12).
- `kRendererServicesVersion` is **2.3.0**, not the spec's 2.1.0 (R0.2).
- Disk: 18 GiB free. Linux: not used at this stage.

## Constitution Check

*GATE: must pass before Phase 0 research; re-checked after Phase 1 design.*

| Principle | Status | Notes |
|---|---|---|
| I. Clean code | PASS | The interpreter's function loses its JIT branch; one switch by null test, two loader declarations in the core, a 426-line re-instantiation of Shading's macro bodies in the core and an unchecked downcast (`lightInstanceOf`) go. |
| II. C++20 / C17 | PASS | Abstract interfaces with defaulted virtual destructors, `final` singletons, default member initialisers for the new request fields. |
| III. TDD (non-negotiable) | PASS (by plan order) | Phase 1 records characterisations on the unchanged tree (they pass there by construction); Phase 2 writes every new test and check, each shown red, behind `OPENRENDER_033_RED_TESTS`; code follows. |
| IV. CLI | PASS | `sloinfo`/`rsloinfo` output, arguments and exit codes unchanged (SC-006). |
| V. Minimal dependencies | PASS | None added. |
| VI. Linux and macOS | PASS (gated) | Both platforms record their own baselines; Phase 3 waits for the Linux baseline. JIT-off tree checked on macOS (and Linux in Phase 9). |
| VII. Docs | PASS | Registry, DEVNOTES, OSHADER_UPDATES; `CLAUDE.md` only as a draft. |
| VIII. Domain separation | PASS | Below. |

**Principle VIII — Domain Separation and Modular Boundaries**:

- [x] **Domains touched are named**: Shading, the renderer core, one consumer (`sloinfo`), tests. Graphics State,
      Texturing, Geometry, Hiders, Transport, Scene Interface, Display Outputs, Shader DSO Operands and Geometric
      Procedures are not edited.
- [x] **No new cross-domain dependency outside the table, none on the core from a domain**:
  - **Added**: Core -> Shading (`shadingSystem.h`, `shaderBackend.h`, `shaderGroup.h`); Shading -> Graphics State
    (`attributes.h`, `xform.h`, `stateHandles.h` from the new files); Consumer -> Shading (`sloinfo` ->
    `ShaderMetadata.h`, replacing 3 internal edges). All allowed.
  - **Removed**: the hidden Shading -> Core path of #134 (20 references compiled in the core, invisible to `archcheck`);
    the core's knowledge of the two loaders and the JIT fields (DB-007).
  - **Upward capabilities** (file search, the instance key, the light list) are inverted through interfaces Shading
    owns and the core implements (`IShaderFileResolver`, `IShaderBindHost`), rule 2; none is bound at load time.
- [x] **Interface changes specified**: [contracts/shading-system.md](contracts/shading-system.md),
      [contracts/shader-backend.md](contracts/shader-backend.md), [contracts/shader-group.md](contracts/shader-group.md),
      [contracts/renderer-services-2.4.md](contracts/renderer-services-2.4.md),
      [contracts/shader-metadata.md](contracts/shader-metadata.md),
      [contracts/light-shader-entry-points.md](contracts/light-shader-entry-points.md),
      [contracts/checks-and-tests.md](contracts/checks-and-tests.md), [data-model.md](data-model.md).
- [x] **No hider-specific special case**: no hider is touched; the run helpers serve every hider through `shade()`.
- [x] **Plug-in interfaces**: none changed (shadeop, procedural, display untouched). The renderer services are not a
      plug-in ABI; their own policy applies (2.x MINOR step, P-Q1).
- [x] **Registered violations not extended; reductions noted**: DB-007 resolved; DB-018 narrowed (the JIT's last two
      direct reads); DB-024 narrowed (`sloinfo`); the allowlist gains no line.

**Post-design re-check** (after Phase 1 of planning): PASS. The design artifacts add no edge outside the table, every
new interface is specified, and the three scans enforce FR-003, SC-005 and SC-006.

## Decisions approved (2026-11-14)

The maintainer approved every plan decision as recommended. The design below is written to these answers.

| # | Decision |
|---|---|
| P-Q1 | Renderer services **2.3.0 -> 2.4.0**, additive with defaults: `MatrixRequest::phase`, `MessageRequest::destBytes`, `getShaderName()`. Spec 057 takes **2.5.0** at its sync (033 merges first). |
| P-Q2 | A bind-time named space that does not exist gives identity, with no warning, on both backends. |
| P-Q3 | The request-tuple comparison (builtin, source, object, light, name, expected type; destination descriptors excluded); `lightsource()` compared only inside an `illuminance` loop over one light; the multi-light and out-of-loop divergence recorded under #115. |
| P-Q4 | `shadername()` with no services answers `""` plus one warning. |
| P-Q5 | A one-layer group **is** its layer's instance. |
| P-Q6 | The names approved tests use are kept (Shading-internal loaders, `execute()` as dispatcher, `CRenderer::getShader` forwarders): **no approved test is edited for the move**. |
| P-Q7 | `libshader_metadata` is a separate static library with one reader per format. |
| P-Q8 | The timing threshold of R12: median of 5 runs (first discarded); pass if after <= before + max(5%, 2 x the Phase 1 spread). |
| P-Q9 | `lightInstanceOf()` returns `CShaderInstance *`. |
| P-Q10 | The corrections of R0.2 and the coordinator's list are applied to spec.md (services 2.3.0 in FR-015 and A-7; DB-007's line numbers; `OPENRENDER_DEFAULT_FORMAT` is a `#define`; DB-018's remaining scope; FR-021's "core's baseline" defined as "no class row of `metrics-baseline.txt` rises"). Done at the tasks stage. |
| L-1 | **Library names (maintainer, 2026-11-14)**: the shader load and runtime library is **libRSLRuntime**; decision and evidence in "Library decision" below. |
| C-1 | **Coordination with spec 057**: 033 merges first and takes services 2.4.0 (057: 2.5.0). 033 changes `lightInstanceOf()`'s return type and rewrites `getShader` and binding into `ShadingSystem::bind`, but leaves `RiLightSourceV`, `RiAreaLightSourceV`, `extractLightHandle`, `takeInstanceKey` and `pendingInstanceKey` exactly as they are (leak included; 057 fixes it). `contracts/light-shader-entry-points.md` stays current with the final design, library names included. |

## Library decision (L-1)

**Instruction** (maintainer): rename the shader loader and the shading engine to `libRSLRuntime`; if today's
`libshader_runtime` and `libshader_shading` can become one library, they do; if not, `libRSLRuntime` is the shading engine
plus loading and the plan states why the rest stays separate. The old target names stay as ALIAS targets for one release;
the installed file names change; the export set provides the old names as aliases if it can.

**Finding (measured 2026-11-14 on `build`, macOS)**: the premise needs one correction. `libshader_runtime` is **not** the
loader. Both shader loaders are already in `libshader_shading`: `src/libshader/shading/rslo.{y,l}` (the `.rslo` loader that
builds `CShader`) and `sloLoader.cpp` (the `.slo` loader). `libshader_runtime` (`src/libshader/runtime/`) is the
**inspection** reader behind the public header `rslo.h`: `rsloGet`/`rsloDelete`, 45 exported symbols, a 72,456-byte
dylib, used by `sloinfo` and (through the P-Q7 reader) by `libshader_metadata`. So `libRSLRuntime` is `libshader_shading`
renamed: shading engine, both loaders, the JIT, the backends, `ShadingSystem` and the services boundary.

**Can the two become one? Technically yes after a prefix rename; the decision is no**, on three measured grounds:

1. **Symbol clash.** `nm -gU` of the two dylibs: **37 global symbols are defined in both**: the whole flex/bison `rslo`
   prefix set (`rsloparse`, `rslolex`, `rsloerror`, `rslotext`, `rsloin`, `rslolval`, `rsloLineno`, `rslo_scan_*`, ...).
   Both parsers also use the prefix in their actions (`rslotext`, `rslolval`, `rsloLineno`), so a unique prefix is a
   source edit of the inspection grammar, not a CMake argument; and the inspection parser defines unprefixed globals
   (`parameters`, `shaderType`, `currentParameter`, `parsedShaderName`, ...) that would enter the engine's symbol
   space. This is the clash `src/libshader/CMakeLists.txt` records (and a third `rslo` parser lives in the compiler).
2. **The static archive.** `libshader_runtime_static` is exported as `openRender::shader_runtime_static` in the Static
   component (`openRenderStaticTargets`; `INSTALL_ARTIFACTS.md`, `openRenderConfig.cmake.in`). The engine is **shared-only**
   (gotcha 22: it binds the shared `libRIBCommon`; `libshader_shading` has no static variant). A merged library could not
   offer that archive, so the Static export would lose a target.
3. **`sloinfo`'s link and start-up.** `sloinfo` (6.4 MB) loads the 72 KB `libshader_runtime`; `libshader_shading` is
   **43 MB** (LLVM is linked into it). Merging would make every `sloinfo`/`rsloinfo` run, and the loading benchmark
   FR-024 measures (it times `sloinfo` start-up), load the engine. P-Q7 (approved) already keeps the metadata readers out of the engine.

**Therefore**: `libRSLRuntime` = the renamed shading engine plus loading. `libshader_runtime` keeps its name, files,
`rslo`/`rslo_static` aliases and `shader_runtime`/`shader_runtime_static` exports. The maintainer's alias list reduces
to **one new ALIAS, `libshader_shading`**. One naming consequence is recorded for the maintainer (open question O-1 in
tasks.md): "libshader_runtime" next to "libRSLRuntime" is confusing; a later rename of the inspection reader (for example
`libRSLInspect`) is cheap and is not done here because the maintainer named only the loader and the engine.

**Naming scheme** (one, stated): CMake target `libRSLRuntime` (the `lib`-prefixed family of `libshader_compiler`, which
spec 043 renames `libRSLCompile`), `OUTPUT_NAME RSLRuntime` (file `libRSLRuntime.1.dylib` / `libRSLRuntime.so.1`),
`EXPORT_NAME RSLRuntime` (`openRender::RSLRuntime`), `SOVERSION 1` (unchanged). Old names for one release:
- target `libshader_shading`: `add_library(libshader_shading ALIAS libRSLRuntime)` (ALIAS targets work in
  `target_link_libraries`, `$<TARGET_FILE:>`, `$<TARGET_EXISTS:>` and `get_target_property`, so every approved test that
  links or locates `libshader_shading` keeps working untouched; an ALIAS cannot be given properties or installed, which no
  caller does: checked by grep);
- export: `CMake/openRenderConfig.cmake.in` defines `openRender::shader_shading` after the targets file is included
  (`add_library(openRender::shader_shading ALIAS openRender::RSLRuntime)`; aliasing a non-global imported target needs
  CMake 3.18, the floor is 3.19, so it works; guarded by `if(NOT TARGET ...)`);
- files: a compatibility symlink `libshader_shading.{dylib,so}` -> `libRSLRuntime.{dylib,so}` next to the real file, in the
  build tree and installed, through the existing `CMake/createCompatAlias.cmake` mechanism that `libRIB*` uses
  (`src/ri/CMakeLists.txt`), shared only (there is no static engine).
Not provided: a binary built against `@rpath/libshader_shading.1.dylib` does not load (its install name changed); NEWS says so.

**Out of scope**: `libshader_compiler` is not renamed; spec 043 renames it `libRSLCompile` when it makes the compiler a
versioned library. `libshader_jitmeta`, `libshader_metadata` and `libshader_runtime` keep their names.

## Decisions asked of the maintainer (all approved as recommended on 2026-11-14)

Each question had a recommended answer; the plan is written to it.

| # | Question | Options | Recommended |
|---|---|---|---|
| **P-Q1** | Renderer services step (FR-015). FR-009 (bind-time lookups through `getMatrix`, bit-identical) and FR-013 (`op_lightsource_f` without `getParameterShape()`, `shadername(type)` without the record) cannot be met by the 2.3.0 interface (R5, R6). | (a) 2.3.0 -> **2.4.0**, additive with defaults: `MatrixRequest::phase`, `MessageRequest::destBytes`, `getShaderName()`; one history entry ([contract](contracts/renderer-services-2.4.md)). (b) No step: Shading special-cases `"current"`/`"object"` at bind time and the implementer tolerates a null light handle (changes a run-time crashing path); the shape check reads Shading's own instance; `shadername(type)` keeps its Graphics State read, and FR-013/SC-005 are amended. | **(a)** |
| **P-Q2** | A bind-time named space that does not exist (`point "nosuch" (...)` as a parameter default). Today the interpreter leaves `from`/`to` uninitialised (undefined behaviour); the JIT copies the point unchanged with no warning (R6). | (a) Both backends take the JIT's present answer: identity, no warning. (b) Identity plus the run-time "Unknown coordinate system" warning (`CODE_BUG`). | **(a)** (the JIT is unchanged; the interpreter gets a defined value) |
| **P-Q3** | FR-014's comparison with #115 unfixed: the two backends cannot make identical `lightsource()` requests (iterated light vs last light that ran; `wantVariable` true vs false) (R7). | (a) Compare the tuple (builtin, source, object, light, name, expected type), excluding the destination descriptors (`dest`, `wantVariable`, `destBytes`, #121); compare `lightsource()` only inside an `illuminance` loop over one light; record the multi-light and out-of-loop divergence under #115. (b) Exclude `lightsource()` from the comparison. | **(a)** |
| **P-Q4** | `shadername(type)` with **no** renderer services (test doubles only): today it reads the record anyway; after the reroute it answers `""` with one "renderer services do not provide shadername()" warning, like the other message builtins (US5 AS2). No approved test exercises it. | (a) Accept. (b) Keep a no-services fallback that reads the record (contradicts FR-013). | **(a)** |
| **P-Q5** | What a one-layer group is (FR-011). `RiIlluminate` casts the light handle back, the services `static_cast` a `ShaderHandle`, `shaderInstanceOf()` casts the slot target: all rely on the slot pointing at the instance (R11). | (a) The group of one layer **is** its instance; spec 034 adds a group object behind `shaderInstanceOf`/`groupExecute`/`ShaderGroupBuilder`. (b) A `CShaderGroup` wrapper now. | **(a)** |
| **P-Q6** | Approved tests (FR-025, for the move; the library rename's approvals are listed separately in TEST-APPROVAL). Tests call `parseShader`/`parseSloShader`, `new CProgrammableShaderInstance(...)`, `context->execute(instance, locals)`, `prog->parent->type/usedParameters/parameters` and `CRenderer::getShader(name, path, fmt)` (R14). | (a) Keep those names (Shading-internal, or a core forwarder that names no backend) so **no approved test is edited**; FR-025 is met as "compile and pass unchanged". (b) Rename and edit the tests. | **(a)** |
| **P-Q7** | Where the metadata halves live (FR-016, FR-017). The inspection parser (`libshader_runtime`) and the loader's parser share the flex/bison prefix `rslo` and cannot share an image; byte-identical `sloinfo` output needs the inspection parser's data (R15). | (a) A separate static `libshader_metadata` with one reader per format (`.rslo` wraps `rsloGet`, `.slo` wraps `libshader_jitmeta`); not members of `IShaderBackend`. (b) `IShaderBackend::describe()` inside `libshader_shading`, with the `.rslo` half rewritten over the loader's parser. | **(a)** |
| **P-Q8** | FR-024's threshold ("no measurable regression") and method (R12). | (a) Median of 5 runs (first discarded) of `benchmark_loading.sh` over all 137 x 2 shipped objects, and of an in-process load of the same set through the loader entry points (before) and `ShadingSystem` (after); pass if after <= before + max(5%, 2 x the Phase 1 run-to-run spread). (b) Another figure. | **(a)** |
| **P-Q9** | `lightInstanceOf()` returns `CShaderInstance *` instead of an unchecked downcast to `CProgrammableShaderInstance *` (a test backend's light, or OSL's later, is not one). Changes the type of locals in `iterateLights` and the ambient loops, which spec 057 also touches. | (a) Change it (every member used is on the base). (b) Keep the downcast; forbid non-RSL lights until 049. | **(a)** |
| **P-Q10** | Corrections to apply to spec.md at approval (R0.2): services version 2.3.0 (FR-015, A-7), the measured DB-007/DB-018/DB-024 evidence, FR-021's "core's baseline" read as "every class row at or below its baseline", `CRendererContext::getShader` ending at :350. | (a) Apply them to spec.md (Clarifications, plan session). (b) Leave spec.md as is; plan and research carry them. | **(a)** |

**Plan-level choices** (no approval needed; a reviewer may overrule): the facade's lifetime and owner (R1); static
self-registration of backends with an "unavailable" entry for a JIT-less `slo` (R2); keeping `CShader` and the loader
names (R3); `execute()` as the dispatcher and `interpret()` as the loop (R4); the per-request resolver over
`locateFileEx` (R8); the bind host and the exact bind order (R9); one cache mutex, never `shaderMutex` (R10); the scans'
scope and identifier lists (R16); the Phase 1 characterisations (R17).

## Architecture decisions relied on (RENDER_ARCHITECTURE_REFERENCES.md, none changed)

- **§5.4**: `IShadingService` is this `ShadingSystem`; 033 builds the RSL side only. Its `shade(ShadingPoint)` /
  closure-material shape is spec 035's; nothing here names a closure.
- **D25**: the facade and the backend interface stay internal C++; spec 052 wraps them in a public runtime library.
- **D8**: backends are a built-in registry (static registration), no run-time loading.
- **D9**: `Surface` stays RSL-only; the `shaderformat` precedence picks between `.slo` and `.rslo`, unchanged; a third
  backend joins the order only after the two RSL ones (R2).
- **D12, D23, D24**: the JIT-off build and the CMake 3.19 floor stay (FR-020); the `.slo`/`.rslo` formats are untouched.
- **D22**: the renderer-services version moves on its own (P-Q1); no RSL-language or closure-ABI version is touched.
- **D27**: native JIT lowering (055) is later; the JIT backend's lowering can change behind `IShaderBackend`.
- **§11.1** (OSL adapter steps): step 2 ("create a ShadingSystem; build, connect and cache shader groups") maps onto
  this facade and spec 034's groups.

## Project Structure

### Documentation (this feature)

```text
specs/033-shading-system-backends/
├── spec.md
├── plan.md                 # this file
├── research.md             # Phase 0: decisions R0-R17
├── data-model.md           # Phase 1
├── quickstart.md           # Phase 1: validation guide
├── contracts/
│   ├── shading-system.md
│   ├── shader-backend.md
│   ├── shader-group.md
│   ├── renderer-services-2.4.md
│   ├── shader-metadata.md
│   ├── light-shader-entry-points.md    # FR-029, for spec 057
│   └── checks-and-tests.md
├── checklists/requirements.md
└── tasks.md                # /speckit-tasks (not created here)
```

### Source code

```text
src/libshader/shading/
├── shadingSystem.{h,cpp}     NEW  facade: cache, precedence, order, bind
├── shaderBackend.h           NEW  IShaderBackend, registry, ShaderRequest, IShaderFileResolver, IShaderBindHost
├── shaderPayload.h           NEW  CShaderPayload, CRslPayload, CRsloPayload, CSloPayload
├── shaderGroup.{h,cpp}       NEW  ShaderGroupBuilder, groupPrepare/groupExecute
├── rsloBackend.cpp           NEW  interpreter backend
├── rsloInit.cpp              MOVED from src/ri/render/init.cpp (git mv), interpreter bind-time init
├── sloBackend.cpp            NEW  JIT backend (or the "unavailable slo" registration)
├── shaderBind.cpp            NEW  libshader::bindCoordinateSystem
├── shader.{h,cpp}, execute.cpp, shading.{h,cpp}, rslo.y, sloLoader.cpp, rslOps.{h,cpp},
│   shaderRefs.h, serviceTypes.h, rendererServices.h, CMakeLists.txt          EDITED
src/libshader/include/openrender/ShaderMetadata.h                            NEW
src/libshader/metadata/{CMakeLists.txt, shaderMetadata.cpp, rsloMetadataReader.cpp, sloMetadataReader.cpp}   NEW
src/ri/render/
├── init.cpp                  DELETED (moved)
├── rendererContext.{h,cpp}, rendererFiles.cpp, renderer.h, rendererStatics.cpp, imager.cpp,
│   rendererShadingServices.cpp, rendererServicesImpl.h, searchPathResolver.h, rendererFileSearch.cpp   EDITED
src/ri/CMakeLists.txt, src/libshader/CMakeLists.txt, src/sloinfo/{sloinfo.cpp, CMakeLists.txt}        EDITED
tests/shading_system/  tests/shaderformat/  tests/shading_contract/(new cases)  tests/architecture/(3 checks + fixtures)  NEW
```

**Structure Decision**: everything new lives in the Shading domain's existing directory (`src/libshader/shading/`), the
metadata readers in a sibling directory of their own library (`src/libshader/metadata/`, classified Shading by the
existing `src/libshader/` prefix of `domain-map.txt`, so no data-file line is added).

## Phases

Every phase ends with `ctest -L architecture`, the full run (`-LE determinism-sweep -j4`), the manifests compared with
Phase 1's, and a stop for the maintainer's review (commit by the maintainer). The services version is set to 2.4.0 once,
in the first phase that adds a member (Phase 3), with the history entry completed in Phase 7.

| Phase | Content | Gate (beyond the everyday ones) |
|---|---|---|
| **1. Baselines and characterisation** (no `src/` change) | Manifests of the visual suite and the sweep on macOS and Linux; the spec 026/028 replays green; `ShaderFormat_Precedence` recorded in `build` and `build-nojit`; `BindSpaces_Characterise`; `ShadingContract_MessageRequests`; `Sloinfo_Output137`; R12 timings. | Every record taken; Linux baseline recorded before Phase 3 |
| **2. Red tests** (no `src/` change) | The new tests and the three checks of [checks-and-tests](contracts/checks-and-tests.md), the `"tst"` fixture backend, behind `OPENRENDER_033_RED_TESTS`; each shown red for the right reason. | Red list recorded |
| **3. Bind-time init into Shading** (FR-009, #134) | `git mv init.cpp rsloInit.cpp`; JIT init path into Shading; `bindCoordinateSystem` + `MatrixRequest::phase` (2.4.0 starts); `lookupVariable`; the core calls one Shading entry for the init. | `BindSpaces_Characterise` identical; 0 `CRenderer::` in the Shading init; no `OPENRENDER_HAVE_LLVM` left from the init in the core |
| **4. Split and dispatch** (FR-007, FR-010, FR-002 part) | Payload types; loaders build payloads and set the backend; the two backend singletons (load, instantiate, initialise, execute); `execute()` dispatcher, `interpret()`, JIT block into `sloBackend.cpp`; `shaderDescription()`; `shader.h` without `llvmJit.h`; `analyse()` as a load helper; `lightInstanceOf` (P-Q9). | Contract and parity suites green; `-slo` scenes identical |
| **5. The facade** (FR-001, FR-004 to FR-006, FR-008, SC-003, SC-004) | `ShadingSystem`, registry order, unavailable `slo`, cache + mutex, resolver, bind host; `CRendererContext::getShader` shrinks; `CRenderer::getShader` forwarders; `globalFiles` and the loader names leave the core. | `ShaderFormat_Precedence` identical in both trees; `ShadingSystem_*` green (third backend included); `Shading_CoreNoBackendNames` green |
| **6. Groups** (FR-011, FR-012) | `ShaderGroupBuilder`; the core binds through it; `groupPrepare`/`groupExecute` at the `shade()`, volume-step and imager sites. | `ShadingSystem_GroupOneLayer` green |
| **7. Message passing** (FR-013, FR-014) | `destBytes` and `getShaderName` (2.4.0 entry completed); `op_lightsource_f`; `shaderName(type)`; implementer. | `ShadingContract_MessageRequestsBoth` and `Shading_JitMessagesThroughServices` green; `ShadingContract_LightsourceOversizedJit` unchanged |
| **8. Metadata API and the library rename** (FR-016 to FR-018, FR-031) | `ShaderMetadata.h`, `libshader_metadata`, the two readers; `sloinfo` through the API only. Then, after its own TEST-APPROVAL (the diffs of two or three approved files): `libshader_shading` -> `libRSLRuntime` (target, `OUTPUT_NAME`, `EXPORT_NAME`, ALIAS, exported alias, compat symlink), the three test edits, the install docs. | `SloinfoGoldenOutput`, `Sloinfo_Output137`, `ShaderMetadata_Shipped`, `Sloinfo_MetadataApiOnly` green; the rename: `ShadingSystem_*`, `Link_*`, `PluginAbi_InstallLayouts` green, S1a/S1e shown failing on a planted include, opt-in `-L '^link-package$'` green |
| **9. Close** (FR-020 to FR-022, FR-024, FR-027, FR-029) | Red-test option removed; registry (DB-007 Resolved with 033 and the date; DB-018, DB-024 rewritten); DEVNOTES (status, #167 known limitation); OSHADER_UPDATES; `claude-md-draft.md` (gotchas 3, 23, 25, 27, 28, 33, 35, "Shading pipeline", "Build"); the light contract checked against the final code; JIT-off build and its tests on macOS and Linux; Linux full run and sweep; macOS sweep; R12 timings; the opt-in `link-package` run on both platforms. The coordinator then runs `speckit-converge` (after the last phase, before merge preparation; no task here prepares the merge). | SC-001 to SC-011 |

## Files and interfaces 057 will meet

Spec 057 (area-light geometry) merges master after 033 lands. 033 changes these files and functions that 057 is known
to touch (from spec 033's "Concurrent work" section; 057's branch was not read):

| File | What 033 changes there | What 033 leaves alone |
|---|---|---|
| `src/ri/render/rendererContext.cpp` | `CRendererContext::getShader()`: body becomes the `"null"` test + `ShaderRequest` (with `CShaderSearchPathResolver`) + `ShaderGroupBuilder`; the instance creation, `takeInstanceKey()` call, `createCategories()`, `allLights` push and `init()` move into `ShadingSystem::bind()` (Shading) and the new `CRendererContext::registerLight()` / `IShaderBindHost::takeInstanceKey()` implementations | `RiLightSourceV`, `RiAreaLightSourceV`, `RiIlluminate`, `extractLightHandle`, `takeInstanceKey`, `pendingInstanceKey` (leak kept) |
| `src/ri/render/rendererContext.h` | `init(CProgrammableShaderInstance *)` removed; the class implements `IShaderBindHost` (`registerLight`, `takeInstanceKey` already exists) | everything else |
| `src/libshader/shading/shading.cpp` | `iterateLights()` and the two ambient loops: `lightInstanceOf()` now returns `CShaderInstance *` (local variable types; P-Q9); `shaderName(type)` asks `getShaderName()`; `shade()`'s surface/displacement/atmosphere runs and the volume step call `groupPrepare`/`groupExecute`; `execute()` dispatcher and `interpret()` declared in `shading.h` | `iterateLights()`' logic, `lightSideN`, `enterLight`/`exitLight`, `prepareAmbient`'s logic, the light's globals, the `jit*Parameter` family, `lightParameter` |
| `src/libshader/shading/execute.cpp` | `CShadingContext::execute` renamed `interpret`; the JIT block (`:502-528`) removed (moved to `sloBackend.cpp`); payload accessors for `codeArea`/`constantEntries`/`codeEntryPoint` | the macros (`runLightsTemplate`, `saveLighting`, ...), `execEnd`'s ambient accumulation |
| `src/libshader/shading/shader.{h,cpp}` | `CShader` split (payload, backend); `CProgrammableShaderInstance` without `jitEntry`/`jitInitEntry`; `shaderDescription()`; `analyse()` becomes a load helper | `createCategories()`, `getParameter*`, `setParameters`, `prepare()`'s logic, `illuminate()`/`execute()` bodies |
| `src/libshader/shading/shaderRefs.h` | `lightInstanceOf()` return type | `shaderInstanceOf()` |
| `src/libshader/shading/rslOps.cpp` | `op_lightsource_f` body (one request with `destBytes`, no `getParameterShape()`) | every `op_*` signature, `jitSetInitXform`, `jitSetInitCoordinateResolver`, `getFromMatrix`/`getToMatrix` |
| `src/libshader/shading/sloBackend.cpp` (new) | the JIT execute (nested-context save, light ambient `Cl`) and the JIT bind-time init | — |
| `src/libshader/shading/rsloInit.cpp` (moved) | the interpreter bind-time init, hooks through the services | — |
| `src/ri/render/init.cpp` | deleted | — |
| `src/libshader/shading/serviceTypes.h`, `rendererServices.h`, `src/ri/render/rendererShadingServices.cpp`, `rendererServicesImpl.h` | services 2.4.0 (P-Q1) | the existing members' behaviour for 2.3.0 requests |
| `src/ri/render/rendererFiles.cpp`, `renderer.h`, `rendererStatics.cpp` | `CRenderer::getShader` becomes a forwarder; `globalFiles` -> `shadingSystem` | `allLights` |
| `src/ri/hiders/photon.cpp` | nothing | photon emission over `CRenderer::allLights` |

New interfaces 057 adapts to: `ShadingSystem::bind()` (the bind order), `IShaderBindHost::registerLight()`,
`IShaderBackend::initialise()`/`execute()`, `lightInstanceOf() -> CShaderInstance *`, `libshader::bindCoordinateSystem()`
for bind-time spaces. Entry points (1) to (6) with their before/after locations:
[contracts/light-shader-entry-points.md](contracts/light-shader-entry-points.md).

## Concurrent specs 040 and 048 (textual overlaps only)

Neither touches shader loading. Expected textual conflicts, resolved by whichever merges second: `DEVNOTES.md`,
`DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` (different entries), `tests/architecture/CMakeLists.txt` (new checks),
`metrics-baseline.txt` if a row moves, each spec's `claude-md-draft.md` against `CLAUDE.md`, `src/libshader/shading/CMakeLists.txt`
and `src/ri/CMakeLists.txt` source lists, `src/libshader/shading/rendererServices.h` if either also steps the services
version (the second renumbers its entry), and possibly `shading.cpp` (040's GI builtins live there; different functions).
048 touches `vcpkg.json` and image I/O, which 033 does not.

## Seams for later specs (FR-030; none implemented)

| Spec | Seam |
|---|---|
| 034 shader networks | `ShaderGroupBuilder::connect()` and multi-layer groups behind `shaderInstanceOf`/`groupPrepare`/`groupExecute`; message lookups across layers would add a services source |
| 035 material interface | `IShaderBackend::execute()` can later produce a closure tree beside colour; no closure type is named |
| 043 compiler library | `ShaderMetadata` is engine-free and can be published at compile time |
| 049 OSL backend | a third `IShaderBackend` (`.oso`, after `.slo`/`.rslo`, D9) and a third `IShaderMetadataReader`; its payload sits behind `CShaderPayload` |
| 052 public runtime library | wraps `ShadingSystem` and the services in a C API (D25) |
| 055 native JIT lowering | the JIT backend's lowering changes behind `IShaderBackend` without a facade change (D27) |

## Departures

- **DEP-1**: FR-025 asks that tests depending on the moved code be "adapted to the new interface"; with P-Q6 (a) they
  compile and pass unchanged instead, because the names they use are kept.
- **DEP-3**: the library rename (L-1) edits approved tests that pin a library name (TEST-APPROVAL); the plan's earlier
  "0 approved test edited" holds for the move only.
- **DEP-2**: the temporary CMake option `OPENRENDER_033_RED_TESTS` (Phase 2 to Phase 9), as spec 029's DEP-2.

## TEST-APPROVAL

**The move (Phases 3 to 7)**: no approved test is edited ([contracts/checks-and-tests.md](contracts/checks-and-tests.md)
lists the approved tests checked against the design). Any edit implementation finds necessary is listed for the
maintainer before it is made.

**The library rename (Phase 8, opening approval; the diff is shown, nothing is edited before the maintainer answers)**.
Measured on this tree (2026-11-14); every other approved test links or locates the engine through the ALIAS and is not touched:

| File | Why it needs an edit | Proposed edit |
|---|---|---|
| `tests/link/check_scope.cmake:86` | It reads `CMakeFiles/libshader_shading.dir/flags.make` behind `if(EXISTS)`. After the rename the real target's directory is `libRSLRuntime.dir`, the file is missing, the include-dir list comes back empty and sub-checks S1a/S1e **pass without checking anything** (nothing turns red). | the path becomes `CMakeFiles/libRSLRuntime.dir/flags.make`; the `if(EXISTS)` guard becomes a `FATAL_ERROR` when the file is absent (so it can never pass vacuously); a planted-include run shows S1a and S1e failing |
| `tests/shading_contract/check_link_line.cmake:56` | `ITEMS "shader_shading" "RIBCommon"`: the link line of the standalone test names the real library file, now `RSLRuntime`; the check goes red. | `"shader_shading"` -> `"RSLRuntime"` |
| `tests/plugins/checks/install_consumer/CMakeLists.txt:16` (proposed, optional) | Iterates `openRender::shader_shading shader_runtime ...`. It stays green **unedited** through the exported alias, which is the point: it proves the old export name works. To prove the new one, add `RSLRuntime` to the list. | add `RSLRuntime` to the foreach |

Checked and **not** edited: `tests/unit/common/CMakeLists.txt:24` (`$<TARGET_EXISTS:libshader_shading>` and
`$<TARGET_FILE:...>` resolve through an ALIAS; verified at the Phase 8 test gate), `tests/unit/sampling`,
`tests/link/CMakeLists.txt`, `tests/plugins/checks/CMakeLists.txt:158` (`$<TARGET_FILE:libshader_shading>`), every
`target_link_libraries(... libshader_shading ...)`; comments in approved tests that say `libshader_shading` stay.

## Complexity Tracking

No constitution violation to justify.
