# Implementation Plan: Versioned Plug-in ABIs

**Branch**: `037-plugin-abi-semver` | **Date**: 2026-10-29 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/037-plugin-abi-semver/spec.md` (specified and
clarified; constitution 1.3.0).

## Summary

The four published plug-in families (display, shadeop, dlo, implicit; plus the optional
RenderMan DynamicLoad generators) get a SemVer interface version declared in their header and
reported by a per-family, C-linkage version query symbol, which one shared host routine in
`libRIBCommon` checks before any other call (reject an unsupported MAJOR, warn on a newer MINOR,
accept unversioned modules as 1.0.0 with a warning naming 1.2.0). The headers become strict-C17
includable with default-visibility exports, and a shared plug-in convention (`orplugin.h`: version
encoding, table header with `structSize`/`abiVersion`, status codes, handle and ownership rules,
plus `orplugin.hpp`) is defined once.

Display moves to **2.0.0**: a function table with a per-display opaque context, a `const`,
host-owned parameter snapshot (RIB parameters plus copies of the renderer values), channel
descriptors, a written threading contract, `finish` exactly once, and unload only after every
`finish` (the dispatch-time unload race goes). A display host class in the renderer core owns
loading, dispatch to drivers and release; a confined v1 adapter in the same layer keeps
unversioned and 1.x drivers and `RiCustomDisplay` working from per-display copies, with a
thread-local bound only during the v1 `displayStart`. The four in-tree drivers migrate with
pixel-identical output.

A new Shader DSO Operands library (`src/shadeop/`, `libRIBShadeop`) owns scanning, the table
lookup, the version check, a hand-written reentrant declaration parser (replacing the flex/bison
`dsoParse` compiled into four libraries) and optional per-thread operand state (shadeop 1.1.0); the
renderer and the compiler both use it. A Geometric Procedures module (`src/procedures/`) owns
dlo, implicit and DynamicLoad loading with reference-counted release; the Scene Interface reaches it
through a Scene-owned hook the renderer core installs, and generator modules are released after
`root->destroy()`. Texturing reads 3D-texture channels through an interface it owns (DB-021).
`file_base.h` stops being installed, the guides are corrected and a plug-in author page joins the
Hugo site. Ten incidental defects (FR-031..FR-040) are fixed, each with a test.

## Technical Context

**Language/Version**: C++20 (host, in-tree drivers), C17 (public headers, C test plug-ins, the
guide's example driver); CMake ≥ 3.19, `-Werror`.

**Primary Dependencies**: none new. `dlopen`/`dlsym` through `common/os.h`; flex and bison remain
mandatory for the RIB, `ifexpr` and RSL grammars but are no longer used for the shadeop
declarations.

**Storage**: N/A. Memory only; test baselines are files under `tests/plugins/`.

**Testing**: CTest, new label `plugin_abi` (the pixel-baseline capture is a separate build target,
`plugin-abi-capture-baselines`, never a ctest, and refuses to overwrite existing references);
test plug-ins are `MODULE` targets with `.dsply` suffix built from `tests/plugins/`; in-process
test programs link `ribRender`; `cmake -P` scripts for the mechanical checks; an opt-in TSan tree
(`OPENRENDER_TSAN`). Existing suites (visual, shading parity, state, common, framebuffer, texture,
architecture) must stay green.

**Target Platform**: macOS 13.3+ (arm64/x86-64, Mach-O bundles for modules) and Ubuntu 24.04 (GCC
13, ELF). Existing `_WINDOWS` branches kept as they are.

**Project Type**: renderer libraries + CLI (`orender`, `oshader`) + loadable plug-ins.

**Performance Goals**: no measurable change in render time: the per-tile display path gains one
atomic load per display; the shadeop `exec` path gains one indexed slot read (no lock); module
loading is unchanged in count.

**Constraints**: pixel-identical output of every in-tree driver (FR-021, SC-004); no allowlist line
added (FR-029); no task may depend on spec 028, on the merge or on a future release (FR-007);
loader-site edits in `rendererContext.cpp` and `implicitSurface.cpp` kept small (028 overlaps them).

**Scale/Scope**: 4 public headers + 2 new public convention headers; 4 in-tree drivers; 16
includer files; 6 `osLoadModule` call sites collapsed into 3 modules; ~60 test plug-in variants from ~8
sources.

No NEEDS CLARIFICATION remains. The ten maintainer questions (Q1–Q10) were resolved on
2026-10-29, each adopting the recommendation (research.md, "Maintainer decisions"); Q7 added FR-040
to the spec and Q9 a site page.

## Constitution Check

*GATE: checked before Phase 0 and re-checked after Phase 1 (below). Constitution 1.3.0.*

| Principle | Status | Notes |
|---|---|---|
| I. Clean code | PASS | Loader code consolidated (6 `osLoadModule` sites → 3 modules); `currentDisplay`, `findParameter` statics and the four-copy parser removed. |
| II. Language standards | PASS | Public headers made strict-C17 includable (they are not today); C17 test plug-ins and example; C++20 elsewhere. |
| III. TDD (non-negotiable) | PASS (planned) | Every phase begins with failing tests: baselines and the parser characterization are captured against the unmodified tree first (research R9, R16); version tests are written against fixture plug-ins before the loaders read a version. |
| IV. CLI | PASS | No new tool; `orender`/`oshader` print the new diagnostics through the existing sink rules; exit codes unchanged. |
| V. Minimal dependencies | PASS | None added; one generated parser removed. |
| VI. Platform targeting | PASS | macOS and Linux via the same CMake; visibility macro covers GCC/Clang; Windows branches untouched. |
| VII. Documentation/site | PASS | Guides in `DEVNOTES_DETAILS/` updated (FR-028) and a new site page `docs/site/content/manual/reference/plugin-abi.md` for plug-in authors (versioning, the convention, migrating a display driver to 2.0), linked from `display-drivers.md` and `dso-shading.md` (Q9, research R19). |
| VIII. Domain separation | PASS | Gate below. |

**Principle VIII — Domain Separation and Modular Boundaries**:

- [x] **Domains touched are named**: Display Outputs (`dsply.h`, four drivers, `file_base`),
      Shader DSO Operands (new `src/shadeop/`), Geometric Procedures (new `src/procedures/`),
      Geometry (`CDLObject`, `CImplicit`), Scene Interface (`RiProcDynamicLoad`, the new hook,
      deletion of `dso.{l,y,h}`), Shading (`dsoHandle.h`, `scriptFunctions.h` DSO macros, the
      compiler's `enumerateDso`, unused include in `shading/rslo.y`), Texturing (`texture3d.cpp`,
      new `texture3dChannels.h`), Graphics State (no State source changes; only the
      `stateConversions.h` asserts in the core and the vocabulary test), renderer core (display
      host, v1 adapter, `getDSO`, DLO/implicit branches, the hook and channel-source
      implementations, `endFrame`), Infra (`orplugin.h`, `orplugin.hpp`, `pluginAbi.{h,cpp}`).
- [x] **No new cross-domain dependency outside the table, none on the renderer core from a
      domain**: every added edge is listed in [contracts/allowlist-end-state.md](contracts/allowlist-end-state.md):
      Display/ShaderDSO/Procedures → Infra (`* -> Infra`), Shading → ShaderDSO, Geometry →
      Procedures, Core → any. The Scene Interface does **not** depend on Procedures: it calls a
      Scene-owned hook (research R11). Texturing's new interface is its own; the core implements it.
- [x] **Interface changes specified before implementation**: [plugin-convention.md](contracts/plugin-convention.md),
      [display-v2.md](contracts/display-v2.md), [display-v1-adapter.md](contracts/display-v1-adapter.md),
      [shadeop.md](contracts/shadeop.md), [procedures.md](contracts/procedures.md),
      [texture3d-channels.md](contracts/texture3d-channels.md).
- [x] **No hider-specific special case**: none; display dispatch is shared by every hider.
- [x] **Changed plug-in interfaces bump their SemVer and document it**: display 1.0.0 → **2.0.0**
      (MAJOR: new entry points, `displayRawData` removed) with MAJOR 1 listed as still supported
      through the adapter (rule 4 as amended in 1.3.0); shadeop **1.1.0** (MINOR: optional
      per-thread entries); dlo, implicit and DynamicLoad **1.0.0** (first declared version; header
      changes are binary-compatible). Each header carries its history.
- [x] **Registered violations not extended; reductions noted**: DB-003, DB-014 and DB-021 resolved;
      DB-019 reduced from 3 lines to 0 (stays Open for 038/043); DB-024's display-ABI duplication
      item done. Edges removed: 3 (DB-019) + 3 (DB-021). Edges added under any entry: 0. The
      Texturing `CRenderer::` metric goes down by one reference; no metric goes up.

Edges, as the check sees them (details and line numbers in the allowlist contract):

| Change | Edges |
|---|---|
| Removed, allowlisted | `dso_generated.cpp`(compiler) → `dso.h`; `rslo_generated.cpp`(shading) → `dso.h`; `rslo.cpp` → `dso.h`; `texture3d.cpp` → `renderer.h`, → `displayChannel.h`, → `CRenderer::retrieveDisplayChannel` |
| Removed, allowed | Core → Scene `dso.h` (4 files); Scene/Geometry → Infra `osLoadModule` (`ri.cpp`, `implicitSurface.cpp`, `dlobject.cpp`) |
| Added, allowed | Display → Infra (`orplugin.h`); ShaderDSO → Infra; Procedures → Infra; Shading → ShaderDSO (`src/shadeop/*`, includes and link); Geometry → Procedures (includes and link); Core → ShaderDSO, Procedures, Display, Scene, Texturing |
| Added, disallowed | none |

**Post-design re-check (after Phase 1)**: PASS. The contracts add no edge beyond the table above;
the display host and v1 adapter are renderer-core files, so their knowledge of every display MAJOR
belongs to the composition root, not to a domain.

## Project Structure

### Documentation (this feature)

```text
specs/037-plugin-abi-semver/
├── spec.md
├── plan.md                  # this file
├── research.md              # decisions R1–R19, measured facts M1–M10, resolved questions Q1–Q10
├── data-model.md            # versions, verdicts, tables, display/shadeop/procedure entities
├── quickstart.md            # validation steps
├── contracts/
│   ├── plugin-convention.md     # orplugin.h, orplugin.hpp, pluginAbi.h
│   ├── display-v2.md            # dsply.h 2.0.0, host behaviour, driver migration
│   ├── display-v1-adapter.md    # the confined v1 adapter
│   ├── shadeop.md               # shadeop.h 1.1.0, parser, module, Shading side
│   ├── procedures.md            # dlo.h, implicit.h, DynamicLoad, Procedures module, Scene hook
│   ├── texture3d-channels.md    # Texturing's channel interface (DB-021)
│   └── allowlist-end-state.md   # edges, allowlist, domain map, metrics, registry
├── checklists/requirements.md
└── tasks.md                 # /speckit-tasks (not created here)
```

### Source Code (repository root)

```text
src/common/
├── orplugin.h               # NEW, installed: convention (C17)
├── orplugin.hpp             # NEW, installed: C++ convenience
└── pluginAbi.{h,cpp}        # NEW: the one version/table check (libRIBCommon)

src/display/
├── dsply.h                  # display 2.0.0 (v1 declarations leave it)
├── file/  file.cpp, file_base.{h,cpp}, file_tiff.*, file_png.*   # v2; file_base.h no longer installed
├── openexr/openexr.cpp      # v2
├── rgbe/file.cpp            # v2
└── framebuffer/framebuffer.cpp  # v2; delete moves from data() to finish()

src/shadeop/                 # NEW domain directory (Shader DSO Operands)
├── shadeop.h                # git mv from src/ri/render/shadeop.h; 1.1.0
├── shadeopDeclaration.{h,cpp}
├── shadeopModule.{h,cpp}
└── CMakeLists.txt           # openrendershadeop_obj -> libRIBShadeop (shared + static)

src/procedures/              # NEW domain directory (Geometric Procedures)
├── dlo.h                    # git mv from src/ri/geometry/dlo.h
├── implicit.h               # git mv from src/ri/geometry/implicit.h
├── proceduralModules.{h,cpp}
└── CMakeLists.txt           # openrenderprocedures_obj -> into ribRender

src/ri/render/               # renderer core
├── displayHost.{h,cpp}      # NEW: load / start / dispatch / finish / release
├── displayV1Adapter.{h,cpp} # NEW: the only v1-aware code
├── displayV1Abi.h           # NEW: v1 types (from the old dsply.h)
├── rendererProcedures.cpp   # NEW: DynamicLoad hook implementation
├── rendererDisplay.cpp      # channel layout stays; findParameter/currentDisplay deleted
├── rendererFiles.cpp        # getDSO -> CShadeopRegistry
├── rendererContext.cpp      # dlo/implicit branches acquire through Procedures (small diff)
├── renderer.{h,cpp}         # hooks installed; releaseFrameProceduralModules() in endFrame
├── rendererStatics.cpp      # dsos -> shadeops (null pointer only)
└── stateConversions.h       # asserts against OrDisplayParamType and ParameterTypeV1

src/ri/parse/
├── procDynamicLoadHook.h    # NEW: Scene-owned hook
├── ri.cpp                   # RiProcDynamicLoad calls the hook
└── dso.{l,y,h}              # DELETED

src/ri/geometry/  dlobject.{h,cpp}, implicitSurface.{h,cpp}   # hold Procedures module references
src/ri/texture/   texture3d.cpp, texture3dChannels.{h,cpp} (NEW)
src/libshader/shading/  dsoHandle.h, scriptFunctions.h, rslo.y (unused include removed)
src/libshader/compiler/ rslo.cpp (enumerateDso), CMakeLists.txt (no dso grammar)
src/ri/CMakeLists.txt     # no dso grammar; public header list; link shadeop + procedures

examples/dsotest/         # declares SHADEOP_DECLARE_VERSION(); built as a test module
examples/display/simpledisplay.c   # NEW: the guide's C17 v2 example

tests/plugins/            # NEW: label plugin_abi
├── CMakeLists.txt
├── display/  shadeop/  dlo/  implicit/  dynamicload/  convention/   # fixture sources + variants
├── display_baseline/     # deterministic scenes, decoded-pixel references, compare tool
├── scenes/               # dlo / implicit / DynamicLoad / shadeop scenes
├── abi_headers_c17.c, abi_headers_cxx20.cpp
├── test_*.cpp            # in-process drivers of the loaders and CDisplayHost
└── *.cmake               # nm, doc-path, install-list and no-cast checks

tests/architecture/  domain-map.txt, allowlist.txt, metrics-baseline.txt
DEVNOTES.md, DEVNOTES_DETAILS/{DISPLAY_PLUGIN_GUIDE,DOMAIN_BOUNDARIES}.md, INSTALL_ARTIFACTS.md,
NEWS.md/ChangeLog.md, new DEVNOTES_DETAILS/PLUGIN_ABI.md (convention, shadeop and procedurals guide)
CMake/openRenderConfig.cmake.in  # RIBShadeop targets in the package (Q1 approved)
docs/site/content/manual/reference/plugin-abi.md   # NEW: plug-in author page (Q9); links from display-drivers.md, dso-shading.md
```

**Structure Decision**: two new domain directories at `src/` level, beside `src/display/`, so
that the three plug-in domains each have their own directory and target, and their headers stop
living inside the renderer core and Geometry directories. Host-side plug-in machinery that must
know every MAJOR (display host, v1 adapter) stays in the renderer core.

## Phase outline for `/speckit-tasks`

Ordered so every phase starts red and ends green (`ctest -L architecture` at the end of each,
CLAUDE.md agent rules). Phases 3–6 are independent of one another after Phase 2.

1. **Test scaffolding and baselines (no behaviour change).** `tests/plugins/` CMake, the fixture
   plug-in sources and their unversioned variants, the deterministic display/AOV baseline scenes and
   decoded-pixel capture target (dither, `quantize` override, `type` selection and PNG `gAMA`
   included; research R16), the DLO/implicit/DynamicLoad/shadeop scenes, the legacy `file.dsply`
   fixture built from frozen base-tree sources, the shadeop declaration characterization test
   against the existing `dsoParse`. All green on the unmodified tree.
2. **Convention and the one check (US2, US6, FR-001..FR-005, FR-010).** `orplugin.h/.hpp`,
   `pluginAbi.{h,cpp}`, C17/C++20 header tests, convention tests with the guard-page fixture,
   versioned variants of the fixtures. Headers gain C guards and visibility; `SHADEOP` signature
   fix (FR-031). Public header list and install check scaffold.
3. **Procedures (US5, FR-022..FR-024, FR-032, FR-034).** `git mv` of `dlo.h`/`implicit.h`,
   `proceduralModules`, the Scene hook, `rendererProcedures.cpp`, DLO/implicit branch changes,
   `endFrame` release. Domain map.
4. **Shadeop (US4, FR-011..FR-013, FR-033, FR-037).** `git mv` of `shadeop.h`, the parser (the
   characterization test re-pointed), the module and library, `getDSO`/`enumerateDso`, Shading's DSO
   macros, per-thread state, deletion of `dso.{l,y,h}`; DB-019 lines deleted.
5. **Display v2 (US3, US1-5/6, FR-006, FR-014..FR-021, FR-035, FR-039, FR-040).** FR-040 first:
   `RiDisplayV` drops quad/mpoint/double parameters (research R18). `CDisplayHost`, the
   snapshot and descriptors, the v1 adapter (custom displays included), deferred unload; migrate
   `file`, `openexr`, `rgbe`, `framebuffer`; pixel identity; hostile/two-display/late-disable tests.
   The existing `tests/framebuffer` suite does not load the driver module and stays as is; it must
   still pass (never disabled). Verify first that the network-server path and the bucket `clear()`
   calls reach `dispatch` only while the display host is started (research M10).
6. **Texturing channels (US7, FR-025).** `texture3dChannels`, core implementation, DB-021 lines
   deleted.
7. **Install and docs (US8, FR-026..FR-029, FR-036, FR-038).** `file_base.h` not installed, guide
   example, doc-path and install-list checks, guides, version histories, NEWS/ChangeLog, DEVNOTES,
   the site page `plugin-abi.md` (research R19), registry entries, metrics baseline.
8. **Validation.** Full run, TSan tree, determinism sweep (`-j2`), Linux.

## Risks

| Risk | Mitigation |
|---|---|
| Pixel identity of the migrated drivers (FR-021) breaks on a lookup-precedence detail (`type`, quantize defaults, string storage) | Baselines captured first (R16); the snapshot reproduces `findParameter`'s precedence exactly (M3, R5); decoded-pixel compare, not bytes. |
| Unloading modules that used to stay mapped exposes plug-ins that keep pointers or threads alive (third-party drivers; the framebuffer's helper is a separate process) | Unload only at `endDisplays` after `finish`; convention rule 5; the v1 adapter keeps the legacy "no finish after failure". If a real driver misbehaves, the fallback is "release at `RiEnd`" (a change of the release point only). |
| macOS refuses to unload some images (bundles with TLS or ObjC), so unload-observing tests flake | Tests assert the host's own counters (`proceduralModuleStats`, display host stats) and use C fixtures without TLS; the destructor log is supplementary. |
| `libRIBShadeop` adds an installed library and package targets (self-contained bundler, rpath) | Approved (Q1); follows the `libshader_compiler` pattern; covered by the install-list check and the real-install quickstart step. |
| Merge overlap with spec 028 in `rendererContext.cpp` (DLO branch) and `implicitSurface.cpp` (constructor) | Keep 037's edits to those blocks minimal and self-contained; whichever lands second resolves the textual conflict. No task depends on 028. |
| Per-thread shadeop slots indexed by `CShadingContext::thread` meet a context with an out-of-range index (imager, network server) | Out-of-range index falls back to the process-wide data with a one-time diagnostic (R10). |
| The hand-written parser accepts a different language | Characterization test written against `dsoParse` before it is deleted; only stdout echo of unknown characters differs. |
| TSan noise from third-party libraries (libtiff, OpenEXR) | The TSan run is limited to `-L plugin_abi`; suppressions only for third-party frames, never for openRender code. |
| Display parameters of type quad/mpoint/double carry uninitialized `type`/`data` (M4) and the snapshot copy would read them | Fixed at the source as FR-040 (Q7): `RiDisplayV` drops them with a warning; the snapshot also skips unknown types; `DisplayAbi_UnsupportedParamType`. |

## Complexity Tracking

No constitution violation needs justification. Two additions are larger than the minimum and are
recorded for review:

| Addition | Why needed | Simpler alternative rejected because |
|---|---|---|
| New shared library `libRIBShadeop` | One module used by renderer, compiler and shading (FR-012/037) without an upward edge | A static archive in two shared libraries keeps two copies of the parser; compiling into `libRIBCommon` puts a domain in infrastructure |
| `CDisplayHost` class extracted from `CRenderer` statics | SC-005 (100 start orders) and SC-006 (1,000 cycles) need a unit drivable without a frame; one place to select the v1 adapter | Driving these through full renders is minutes per run and cannot randomize start order |

## Note from the checklist review (2026-10-29)

- The spec now carries FR-041..FR-045 and SC-015..SC-018 (table and header rules, per-process version record, v1 adapter rules, DynamicLoad lifetime, metrics rule, site page, v1 serialisation tests). Contracts and quickstart list the added tests.
- `tests/architecture/allowlist.txt` has a DB-019 section header that says "(4)" over 3 lines. The implementation phase that deletes the DB-019 lines removes the section and with it the miscount (allowlist-end-state contract); `allowlist.txt` is not edited at the checklist stage.
- Operand scan order is lexicographic by file name within a directory (FR-013); the shadeop contract is updated to match.
- Checklist closure: guides are `DISPLAY_PLUGIN_GUIDE.md` (updated) plus the new `DEVNOTES_DETAILS/PLUGIN_ABI.md`; new requirements FR-046..FR-049 and SC-019/SC-020 (adapter named-file rule, version encoding and single version source, install list source and layouts, zero/depth-only displays).
