# Implementation Plan: OIIO Image I/O Backend

**Branch**: `048-oiio-image-io` | **Date**: 2026-11-14 | **Spec**: [spec.md](spec.md)

**Input**: Feature specification from `specs/048-oiio-image-io/spec.md`, clarified 2026-11-13 (OIIO optional until 049;
explicit selection, legacy default, never an automatic fallback; only TIFF, PNG, RGBE and OpenEXR on the OIIO paths;
no OIIO tile source; the shared-vcpkg constraint). Decisions cited: D23, D24, D30 part 4
(`DEVNOTES_DETAILS/RENDER_ARCHITECTURE_REFERENCES.md`) and the "OIIO" row of the maintainer's plan decision table.
None of them is changed by this plan.

**Status of this plan**: **approved by the maintainer on 2026-11-14, every question P-Q1 to P-Q16 as recommended**
(see "Decisions approved" below). The design follows those answers. Nothing here has been built yet: no vcpkg run, no
configure, no build. Of the measurements that need OpenImageIO installed, M0, M1 and M3a are approved with conditions
(P-Q14, below) and are the first tasks of Phase 1 in [tasks.md](tasks.md); their exact commands are in
[quickstart.md](quickstart.md) section M (FR-024).

**Tasks-stage edit to spec.md (2026-11-14)**: the OpenColorIO edge case was reworded as P-Q10 decided: "no colour
conversion is ever applied" instead of "OpenColorIO must not be reached from any path". Nothing else in the spec
changed.

**Plan-stage edit to spec.md**: the requirements were renumbered so they read in order. The old FR-024 (OIIO-only
formats out of scope) is now **FR-023**, the old FR-025 (shared `VCPKG_INSTALLED_DIR`) is now **FR-024**, and the old
FR-023 (no task depends on the merge) is now **FR-025**. Every reference in spec.md and in
`checklists/requirements.md` was updated. Nothing else in the spec changed. This plan uses the new numbers.

## Summary

OpenImageIO 3 enters the build as an optional, explicitly selected, extra image backend. Nothing is removed and no
default changes. The work has six parts:

1. **One utility library holds OIIO** (FR-004, FR-005; [research R1](research.md#r1-which-shared-library-holds-oiio)):
   a new shared library `libRIBImageIO` (CMake target `openrenderimageio`, sources in `src/imageio/`). It links OIIO
   privately and publishes a small C++ interface (`src/imageio/orImageIO.h`, namespace `orimageio`) that names no
   OIIO type. It depends only on `libRIBCommon`. It is built only when OIIO is enabled. `libRIBRender`, `otexmake`
   (through `libRIBRender`) and the new `oiio` display module link it. `libRIBVector` does not: the
   `imageInput.cpp`/`texmake.cpp` it shares with `libRIBRender` compile their OIIO branch only under `HAVE_OIIO`,
   which is defined on `ribRender_obj` alone. That is the same mechanism `HAVE_OPENEXR` already uses
   (`src/ri/CMakeLists.txt:214-233`, `:385-396`). There is no static archive of the library: `ribRender_static` and
   `ribVector_static` link the shared one, as `libshader_shading` does (gotcha 22). The library exports only
   `orimageio::` symbols, so a static OIIO (vcpkg) lives in exactly one image.
2. **The constitution amendment** (FR-006; [contracts/constitution-1.5.0-draft.md](contracts/constitution-1.5.0-draft.md)):
   constitution 1.4.0 -> 1.5.0 (MINOR). Principle VIII gains a "Utility libraries" paragraph and a one-row table:
   Image I/O, used by Texturing and Display Outputs, depending on no domain. `archcheck` gains an eleventh layered
   class, `ImageIO` (`domain-rules.txt`: `ImageIO ->`, `Texturing -> ImageIO`, `Display -> ImageIO`;
   `domain-map.txt`: `prefix src/imageio/ ImageIO`). This follows exactly how spec 030 added `Transport`
   (commit `e0a7e4e3`: enum entry, name, help text, two fixtures). No allowlist line is added (FR-007).
3. **Decode backend** (FR-009 to FR-012): `COiioImageInput : CImageInput` in Texturing
   (`src/ri/texture/imageInputOiio.{h,cpp}`, an empty translation unit without `HAVE_OIIO`). It adapts
   `orimageio::Reader`. `createImageInput(filename, EImageBackend)` keeps one suffix table for both backends, so a
   suffix the legacy table does not know returns `nullptr` on both paths (FR-023). The reader opens the file **with
   the format the suffix chose**, never by probing, and refuses a file whose content is another format. It always
   passes `oiio:UnassociatedAlpha=1` and `oiio:RawColor=1`, and it refuses everything the legacy decoders refuse,
   using the same checks ([research R7](research.md#r7-decode-acceptance-policy)). The documented difference list
   holds only the cases where legacy accepts a file and then reads it differently
   ([contracts/difference-list.md](contracts/difference-list.md)). One additive virtual,
   `CImageInput::readMatrix()`, replaces `texmake.cpp`'s `dynamic_cast<CTiffImageInput *>` for the shadow-map
   matrices, so callers stop needing to know the backend (FR-009).
4. **Baking writer** (FR-015 to FR-017): `texmake.cpp`'s `appendLayer` writes through a small sink. The legacy sink
   is the current libtiff code, which stays byte-identical, pinned by `ImageInput_TiffBakeByteIdentical`. The OIIO
   sink is `orimageio::TiledTiffWriter`, which reproduces libtiff's "tags set before this directory" semantics
   directory by directory. From source evidence ([research R8](research.md#r8-baking-what-oiio-3114-can-and-cannot-write)):
   OIIO preserves the texture format string (including openRender's custom strings), the wrap modes and the two
   matrices. It writes the full-size tags only when they differ from the data size, a limit the plan works around
   with OIIO's own `full_x`/`full_y` rule (P-Q3). It **forces `PHOTOMETRIC_RGB` for 3 or 4 channels**, and the
   texture reader scales a 16-bit `PHOTOMETRIC_RGB` texture by 2 (`texture.cpp:2476-2479`). The gate is one rule:
   OIIO writes a (bits, channels) combination only where its `PHOTOMETRIC` and `EXTRASAMPLES` equal legacy's.
   So 16-bit bakes with 3 or more channels, and 8-bit or float bakes with 5 or more channels (legacy leaves
   `PHOTOMETRIC` unset there), stay on the legacy writer, with a message (FR-017, P-Q4).
5. **One display driver** (FR-019, FR-020): `oiio.dsply` (`src/display/oiio/`), display ABI 2.0.0, built on the
   existing `openrenderfilebase`, so quantization, dither and gamma are the very code the legacy drivers run. Only
   the codec step goes through `orimageio::ScanlineWriter`. The format comes from `"string type"` or from the
   extension. An unknown format is an error, never TIFF by default (P-Q7). The legacy `file`, `openexr`, `rgbe` and
   `framebuffer` drivers are untouched.
6. **Selection and build** (FR-002, FR-003, FR-013; [contracts/selection.md](contracts/selection.md),
   [contracts/dependencies-and-build.md](contracts/dependencies-and-build.md)):
   - Selection spellings: `Option "imageio" "backend" ["oiio"]` (scene), the per-call `"string imageio"` parameter
     of `MakeTexture`/`MakeShadow`/`MakeLatLongEnvironment`/`MakeCubeFaceEnvironment`, `otexmake -imageio oiio`,
     and `Display "x.exr" "oiio" "rgba"`. The default is `legacy` everywhere.
   - CMake: `-DOPENRENDER_ENABLE_OIIO=ON`, default OFF. OIIO is found version-less and checked for >= 3.0 by hand,
     because OIIO's package version file is `SameMajorVersion`, the same trap as LLVM's.
   - vcpkg: `openimageio` (no default features) is a manifest **feature** `oiio`, not a top-level dependency.
     Turning OIIO on derives a separate install directory before `project()` (`$VCPKG_INSTALLED_DIR-oiio`) and
     fails if that directory is the shared one. OIIO-on and OIIO-off trees can then never thrash one install tree,
     now or after the merge (FR-024).
   - Ubuntu 24.04 (OIIO 2.4.17, OCIO 2.1.3, both below OIIO 3.1's floors) builds without OIIO by default. A
     developer may point `OpenImageIO_ROOT` at a prefix build of OIIO 3.1.14, which builds OCIO itself.
   - No CI job enables OIIO in this spec.

**Tolerance (FR-021)**: none, anywhere. Every parity comparison is exact, with one exception: the render-equivalence
metric of US3-3 (`ImageIO_BakeRender`, the visual block-average metric at the default threshold 20, approved
2026-11-14), which also reports exact identity and is backed by the exact tag-and-sample comparison of `ImageIO_BakeParity`. The one float discrepancy found
([research R5](research.md#r5-float-parity-and-the-tolerance-question)) is in OIIO's RGBE decode table: exponent
bytes 40, 223 and 226 are one float ulp off the exact power of two (checked entry by entry against `ldexp`). It is a
listed difference, pinned to its exact values by a test, not a tolerance. EXR half conversion is
round-to-nearest-even on both paths (Imath on the legacy side, `vcvt_f16_f32`/F16C or Imath inside OIIO). RGBE
encoding is the same arithmetic: a single float division computed in double and rounded is correctly rounded. Both
are confirmed by exact tests that include ties, denormals and overflow. A test that finds a difference stops the
work and goes back to the maintainer; it is never widened.

## Technical Context

**Language/Version**: C++20 (`-Werror`), C17 for the display ABI header as today; CMake >= 3.19 (unchanged floor).

**Primary Dependencies**: OpenImageIO >= 3.0, optional, through its own CMake package. It is vendored on macOS by
vcpkg (port 3.1.14 at baseline `319504a5`, no default features; OpenColorIO comes with it as a required dependency,
in both the port and OIIO's own build, `externalpackages.cmake:141`). On Linux it comes from a documented prefix build.
No other new dependency. libtiff, libpng, OpenEXR and the in-tree RGBE codec stay as they are.

**Storage**: N/A. The baked-texture container is unchanged. The disk-cache key of spec 020 gains a backend suffix
only when OIIO is selected, so legacy entries keep their names (FR-014).

**Testing**: CTest. The new label `imageio` covers decode, bake and display parity, the difference list,
selection, single copy, and the "absent" checks. The existing labels `image_input`, `texture_tile`,
`texture_only`, `plugin_abi`, `visual` and `architecture` run unchanged, against unchanged references and thresholds
(FR-014, SC-004). OIIO-dependent tests are registered only when `HAVE_OIIO`, and the "absent" tests only when it is
off (FR-003).

**Target Platform**: macOS arm64/x86_64 (vcpkg static triplet or Homebrew) and Ubuntu 24.04 (GCC 13). Windows
unsupported.

**Project Type**: renderer: libraries, CLI tools and dlopen'ed plug-in modules.

**Performance Goals**: no change on the default path. On the OIIO path, decode and bake throughput is not a goal of
this spec and is not measured as a gate.

**Constraints**:
- No vcpkg run, and no configure that touches the shared `VCPKG_INSTALLED_DIR`, without approval (FR-024).
- Builds go through `~/.claude/bin/cpujob` at `-j4` at most, and vcpkg's own parallelism is capped with
  `VCPKG_MAX_CONCURRENCY=4`.
- OIIO's code lives in exactly one image (FR-004, gotcha 22).
- No exception crosses the library or plug-in boundary (spec edge case).
- No colour conversion is applied (FR-010).
- The texture system's load lock is never taken re-entrantly (gotcha 25): the reader never calls back into the
  texture system.

**Scale/Scope**: about 1.2k lines of new C++ (library about 600, adapter 150, sink and gate 150, driver 250), about
40 small fixtures, 22 new CTest tests (about 17 test programs and scripts) and 2 archcheck fixtures, one archcheck class, and documentation.

## Constitution Check

*GATE: must pass before Phase 0 research. Re-checked after Phase 1 design (below).*

| Principle | Status | Note |
|---|---|---|
| I Clean code | pass | One reader, one tiled writer and one scanline writer. The gate is one table, `kOiioBakeExclusions` in `texmake.cpp`. |
| II C++20/C17 | pass | No extensions. The display module keeps the C ABI. |
| III TDD | pass | Every parity test is written first against the legacy path (green), then the OIIO path is added (red, then green); see quickstart V1 to V9. |
| IV CLI | pass | `otexmake -imageio legacy\|oiio`, documented in `--help` and `man/otexmake.1`. |
| V Minimal dependencies | **justified** | OIIO is optional, version-floored and documented (FR-002). D23 makes it the backend OSL needs anyway (049), so the dependency is justified there. A build without it is today's build. |
| VI Platforms | pass | macOS and Linux paths documented; Linux falls back to the legacy build when OIIO 3 is absent (US5-4). |
| VII Docs/site | pass | FR-022's list goes in the tasks: man pages, site, INSTALL/COMPILING, DEVNOTES, a new `DEVNOTES_DETAILS/IMAGE_IO.md`. |

**Principle VIII, Domain Separation and Modular Boundaries** (every plan):

- [x] **Domains touched**: Texturing (decode adapter, bake sink and gate, settings field), Display Outputs (the `oiio`
      module), the renderer core (option to settings mapping, the `Make*` parameter injection), Scene Interface
      tokens (`imageio`, `backend`, in the three token lists), Graphics State (one `COptions` field), consumers
      (`otexmake`, tests), and the **new utility library Image I/O**.
- [x] **No forbidden edge.** The only new cross-class edges are Texturing -> ImageIO and Display -> ImageIO, made
      legal by the amendment that lands in this spec, before any code under `src/imageio/` exists (the spec 030
      precedent). ImageIO depends on Infra only. Graphics State stores its own vocabulary value (`stateVocabulary.h`)
      and never includes Texturing. The core maps it to Texturing's enum.
- [x] **Interface changes specified**: [contracts/imageio-library.md](contracts/imageio-library.md) (the library's
      published interface), [data-model.md](data-model.md) §2 (`EImageBackend`, `createImageInput` overload,
      `CImageInput::readMatrix`, `CTextureSystemSettings::imageBackend`), §4 (the bake sink).
- [x] **No hider-specific special case.** The hiders are untouched.
- [x] **Plug-in interfaces**: display ABI unchanged (2.0.0). The new module declares the version like every driver,
      and `PluginAbi_*` version tables do not change.
- [x] **Registry**: no registered violation is extended, none is added. DB-023 (domains as build targets) gains a
      separately buildable utility target; noted, not resolved.

**Post-design re-check (after Phase 1)**: unchanged; all gates pass. Open governance item: P-Q2, the amendment text,
needs the maintainer's approval before the implementation phase that applies it.

## Project Structure

### Documentation (this feature)

```text
specs/048-oiio-image-io/
├── spec.md                         # renumbered FR-023..FR-025 at plan stage
├── plan.md                         # this file
├── research.md                     # R1-R14, evidence for every decision
├── data-model.md                   # entities, enums, acceptance rules, tag sets, gate table
├── quickstart.md                   # validation scenarios V1-V10 and measurements M1-M4 (approval needed)
├── contracts/
│   ├── imageio-library.md          # libRIBImageIO's published C++ interface, export and error rules
│   ├── selection.md                # Option/parameter/flag/driver spellings and precedence
│   ├── bake-writer.md              # the bake sink, per-directory tag semantics, exclusions
│   ├── oiio-display-driver.md      # the oiio.dsply driver: formats, parameters, errors
│   ├── difference-list.md          # the one documented, tested list (FR-012), initial entries
│   ├── dependencies-and-build.md   # CMake option, vcpkg feature, install-dir guard, Linux, CI
│   └── constitution-1.5.0-draft.md # amendment text + domain-rules/domain-map/DOMAIN_BOUNDARIES edits
├── checklists/requirements.md
└── tasks.md                        # next stage, not created here
```

### Source Code (repository root)

```text
src/imageio/                        # NEW: utility library libRIBImageIO (class ImageIO)
├── CMakeLists.txt                  # shared only; OIIO PRIVATE; exported-symbols list (macOS) / --exclude-libs (Linux)
├── orImageIO.h                     # published interface, no OIIO type (contracts/imageio-library.md)
├── reader.cpp                      # orimageio::Reader
├── tiledTiffWriter.cpp             # orimageio::TiledTiffWriter (bakes)
├── scanlineWriter.cpp              # orimageio::ScanlineWriter (display)
├── oiioInit.cpp                    # one-time OIIO attributes (threads, exr_threads, plugin_searchpath)
└── exports.txt                     # macOS exported-symbols list (__ZN9orimageio*, typeinfo)
src/ri/texture/
├── imageInput.{h,cpp}              # + EImageBackend, createImageInput(filename, backend), readMatrix() (additive)
├── imageInputTiff.{h,cpp}          # + readMatrix() override (moves the libtiff matrix read out of texmake.cpp)
├── imageInputOiio.{h,cpp}          # NEW: COiioImageInput, under HAVE_OIIO
├── texmake.cpp                     # + "imageio" parameter, bake sink, kOiioBakeExclusions
├── textureSystem.h                 # + CTextureSystemSettings::imageBackend
└── texture.cpp                     # synthesized source + disk cache honour the backend
src/display/oiio/                   # NEW: oiio.dsply (Display), links openrenderfilebase + openrenderimageio
src/ri/state/{options.h,options.cpp,stateVocabulary.h,declarations.cpp}  # + COptions::imageBackend (State vocabulary); declarations (compiled into ribVector)
src/ri/render/{rendererContext.cpp,renderer.cpp}  # Option parse, mapping
src/ri/parse/ri.h, src/common/riVocabulary.h, src/common/riTokens.cpp      # RI_IMAGEIO, RI_BACKEND (all three)
src/otexmake/otexmake.cpp           # -imageio flag
CMakeLists.txt                      # OPENRENDER_ENABLE_OIIO, vcpkg feature + install-dir guard, find + version check
vcpkg.json                          # "features": { "oiio": openimageio (default-features false) }
CMake/openRenderConfig.cmake.in     # OPENRENDER_CONFIG_HAVE_OIIO; static consumers get the shared library
tests/unit/imageio/                 # NEW: parity, differences, selection, single-copy, absent
tests/architecture/                 # domain-rules.txt, domain-map.txt, archcheck class ImageIO, 2 fixtures
.specify/memory/constitution.md, .specify/templates/plan-template.md, DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md
```

**Structure decision**: one new library directory (`src/imageio/`) beside `src/common/`, one new display module
directory, and additive edits elsewhere. The library is classified by location, as every class is (`domain-map.txt`
research R4 of spec 022).

## Phases (as built into tasks.md)

[tasks.md](tasks.md) turns these into tasks. Each phase ends with a CHECKPOINT (the maintainer commits) and, from
Phase 2, `ctest -L architecture`.

1. **Setup, baselines and the approved measurements**: pre-flight, M0 (private OIIO-off base tree), the OIIO-off
   baseline build and tests on macOS and on the Linux host, M1 (vcpkg OIIO into the private tree), M3a (the Linux
   prefix build), the shared-tree checksum guard. No source change.
2. **Governance gate** (no code under `src/imageio/` yet): apply the amendment (constitution, Sync Impact Report,
   plan template, `domain-rules.txt`, `domain-map.txt`, archcheck `Class::ImageIO`, fixtures `imageio-allowed` and
   `imageio-reverse`, `DOMAIN_BOUNDARIES.md`). It must pass on the unchanged tree.
3. **Build plumbing, OFF by default**: CMake option, find and version check, vcpkg feature and install-dir guard, the
   empty library target, the "absent" tests (`ImageIO_AbsentBuild`), then the first OIIO-on configure in this
   worktree. Byte-identity check of the default build (quickstart V1).
4. **Decode (US1)**: fixtures and generator, parity test (legacy-only first, then both), `orimageio::Reader`,
   `COiioImageInput`, `readMatrix`, the difference list and its tests.
5. **Selection (US2, FR-013)**: tokens in all three lists, `COptions`, declarations (`state/declarations.cpp`), the
   core mapping, the `Make*` parameter, `otexmake -imageio`, the disk-cache key, `ImageIO_SelectWithoutOIIO`.
6. **Baking (US3)**: the sink split (byte-identity first, pinned by recorded legacy hashes), `TiledTiffWriter`, the
   gate table, the bake parity and render-equivalence tests.
7. **Display (US4)**: `ScanlineWriter`, `oiio.dsply`, display parity, load and format-error tests.
8. **Single copy, Linux with OIIO, documentation (US5)**: `ImageIO_SingleCopy` over the install tree; Linux
   validation with M3b; docs (FR-022); measurements M4 recorded (SC-007, SC-008).
9. **Closing validation on both platforms**: the full suite, the determinism sweep and the architecture check on an
   OIIO-on and an OIIO-off build of each platform; byte-identity against the Phase 1 baseline; the audit.

## Overlaps with concurrent specs (designed against master `278b0394` only)

| File or area | 048 edits | Also edited by | Expected conflict |
|---|---|---|---|
| `src/ri/CMakeLists.txt` (`ri_sources`, `ribRender_obj` definitions/links) | `texture/imageInputOiio.cpp`, `HAVE_OIIO` block | **040** (Texturing GI storage, shared source lists) | textual, line-range; the later merge reconciles |
| `src/ri/texture/` (`texmake.cpp`, `texture.cpp`, `textureSystem.h`, `imageInput.*`) | adapter, sink, settings | **040** (photon and irradiance stores in the same directory) | possible in `texture.cpp`/`textureSystem.h` only if 040 touches the settings struct |
| Root `CMakeLists.txt` option list, `vcpkg.json` | `OPENRENDER_ENABLE_OIIO`, feature `oiio` | **033** (its spec names `vcpkg.json` (048) and the root option list) | textual |
| `riVocabulary.h`, `riTokens.cpp`, `ri.h`, `options.h/.cpp`, `state/declarations.cpp`, `rendererContext.cpp` `RiOptionV` | `imageio`/`backend` tokens and option | **057** (`Option "arealight"`) | textual, adjacent lines |
| `.specify/memory/constitution.md`, `tests/architecture/{domain-rules,domain-map}.txt`, archcheck class enum | amendment 1.5.0, class `ImageIO` | none of 033/040/057 amends the constitution (checked their spec and plan drafts on 2026-11-14) | none expected; a later amendment would collide on the version header |
| `tests/architecture/allowlist.txt`, `metrics-baseline.txt` | none | **033** deletes allowlist lines, changes the metrics | none |
| `CLAUDE.md`, `DEVNOTES.md`, `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` | Recent Changes, a gotcha, Testing list, registry text | **033, 040, 057** | textual on every merge |

## Decisions approved (2026-11-14)

The maintainer approved every plan decision as recommended. The text of each question follows in "Maintainer
questions"; this table is the record of the answer and of what it changes.

| Q | Decision | Consequence recorded |
|---|---|---|
| P-Q1 | New shared `libRIBImageIO` (`src/imageio/`, class `ImageIO`), shared only; `libRIBVector` does not link it | Phase 2 (the amendment, governance) and Phase 3 (the target) |
| P-Q2 | Constitution 1.5.0 amendment text as drafted in `contracts/constitution-1.5.0-draft.md` | Phase 2 applies it, before any file exists under `src/imageio/` |
| P-Q3 | Option (a): `full_x = 1`, `full_width = W - 1` (and the `y` form) so OIIO writes the full-size tags also when equal; `ImageIO_BakeParity` guards it against version skew | Phase 6 |
| P-Q4 | Two bake exclusion rows: 16-bit with 3 or more channels, and 8-bit/float with 5 or more channels, stay on the legacy writer with a message; 16-bit 2-channel is written with `tiff:write_extrasamples` 0 | Phase 6 |
| P-Q5 | The OIIO decode path refuses exactly what legacy refuses, with the same checks; the difference list holds only "legacy accepts, result differs" | Phase 4 |
| P-Q6 | `Option "imageio" "backend" ["oiio"]` without OIIO: one error, the value stays `legacy`, the scene renders. An explicit per-call `"imageio" "oiio"` or `otexmake -imageio oiio` without OIIO fails and writes nothing | Phase 5 (T074, T079), tested by `ImageIO_SelectWithoutOIIO` |
| P-Q7 | `oiio` display driver: unknown or missing format is an error naming the file; nothing is written | Phase 7 |
| P-Q8 | RGBE output rule: R,G,B from a 3- or 4-sample display (alpha dropped, documented); 1, 2 or 5 samples is an error | Phase 7 |
| P-Q9 | The two legacy defects are filed as **GitHub #171** (RLE `.hdr` decode: `CRgbeImageInput` uses `RGBE_ReadPixels`, which cannot read RLE) and **GitHub #172** (the `rgbe` display driver passes `numSamples`-strided scanlines to `RGBE_WritePixels`, so `rgba` output is garbled). Spec 048 does not fix either | Cited by `D-RGBE-RLE`, the display contract's "Not covered" and `DEVNOTES_DETAILS/IMAGE_IO.md`; no task fixes them |
| P-Q10 | The spec's edge case says "no colour conversion is ever applied" (OIIO may load a colour configuration to describe metadata); backed by `ImageIO_NoColourConversion` (decoded samples equal with `OCIO` unset and set to another config, for a cICP PNG on read and for every PNG the driver writes) | spec.md edited now; Phase 4 (read) and Phase 7 (write) |
| P-Q11 | No CI job enables OIIO in 048 | Phase 3 (T035) asserts `release.yml` has no `OPENRENDER_ENABLE_OIIO` and keeps at least one job without OIIO |
| P-Q12 | Linux: a documented prefix build of OIIO 3.1.14 consumed with `-DOpenImageIO_ROOT=`; Ubuntu 24.04 builds without OIIO by default | Phase 8 docs; M3a/M3b. pystring pin: OIIO 3.1.14.0's local pystring build uses the git branch `master` and verifies it against commit `381829c2c1696ffec9277b339952a9588e6e67cf`; upstream `master` has moved (`1922c8f2...`), so the first configure with `-DOpenImageIO_BUILD_MISSING_DEPS=all` stops with `pystring: Tag master resolved to commit ..., but expected ...`. Do not set `OpenImageIO_DEPENDENCY_BUILD_ALLOW_UNVERIFIED_TAGS=ON`: in `<build>/deps/pystring` run `git fetch --unshallow` and `git checkout -B master 381829c2c1696ffec9277b339952a9588e6e67cf`, then configure again (the verification still runs and passes). Found at M3a, 2026-11-15 (validation.md §6) |
| P-Q13 | `OPENRENDER_ENABLE_OIIO` is OFF by default; ON without OIIO warns and builds the legacy tree | Phase 3 (T040, T041) |
| P-Q14 | Measurements M0, M1 and M3a approved, with the conditions below. M2 is analysis only; M4 runs at implementation | Phase 1 tasks of tasks.md, exactly as `quickstart.md` section M writes them |
| P-Q15 | The per-call `"imageio"` parameter on `MakeTexture`, `MakeShadow`, `MakeLatLongEnvironment` and `MakeCubeFaceEnvironment` is documented | Phase 5 and Phase 8 docs |
| P-Q16 | The three RGBE table entries (exponent bytes 40, 223, 226) are one pinned, tested listed difference (`D-RGBE-EXP`); no tolerance | Phase 4 |

**P-Q14 conditions** (the maintainer's, applied to the measurement tasks):
- **M0 and M1** checksum the shared vcpkg tree's `vcpkg/status` and `vcpkg/manifest-info.json`
  (`~/.cache/vcpkg-installed/openrender`) before and after, and **stop if either changes**.
- **M1 holds vcpkg's lock** (`~/.vcpkg/vcpkg-running.lock`): its task says to ask the coordinator for a slot at a time
  when no other worktree is configuring.
- **M3a runs on the Linux host in `~/oiio-048`** through the remote `cpujob` at `-j2`. `ssh` works over WARP, `ping`
  does not. Never touch `openrender`, `cellar`, `shrimp`, `openrender-029` or `openrender-038` there.
- **All macOS work goes through `~/.claude/bin/cpujob`**, `-j4` at most, never `-j8`, with `VCPKG_MAX_CONCURRENCY=4`.

**Analyze findings fixed (2026-11-15)**: the maintainer approved fixing every finding of the analyze stage. What
changed in the design documents:
- **FR-021 and SC-008 (H3, option a)**: parity tests run in every build that has OIIO, on macOS (vendored) and on the
  Linux validation host (prefix build); CI does not run them in 048 and no CI job is added (P-Q11 stands).
- **Declarations (M3)**: the new option token and the per-call `"imageio"` are declared in Graphics State's
  `src/ri/state/declarations.cpp` (beside `RI_TEXTURECACHEENABLE`, compiled into ribVector), not in
  `render/rendererDeclarations.cpp`.
- **Sink split (M2)**: the split of `texmake.cpp`'s `appendLayer` into `CBakeSink`/`CLegacyBakeSink` is an approved
  legacy refactor, pinned by the sha256 of the legacy bake of every bake fixture recorded on the unmodified tree
  (before T093), compared after T093 and again at the closing byte-identity task.
- **Tolerance (M7)**: the one exception is the US3-3 render-equivalence metric (threshold 20, approved).
- **FR-017, SC-002, US3-4 (M6)**: the unit of exclusion is the texture kind and the (bits, channels) combination.
- **Phase numbers and test count (M1)**: the P-Q table and the phase sketch name tasks.md's phases; the scale is 22 new
  CTest tests.
- **Tasks (H1, H2, H4, H5, M4, M8, M9 and the lows)**: applied in tasks.md; see its "Analyze-stage answers".

**Tasks approved (2026-11-14)**: the maintainer approved `tasks.md` (136 tasks, 9 phases; 138 after the analyze fixes of 2026-11-15), including the default parity
threshold 20 for the new bake render scenes (`ImageIO_BakeRender`, T092). For T012 (M1) the agent asks the coordinator
for the vcpkg slot when it reaches that task.

## Maintainer questions (answered above; text kept for the reasoning)

Each question has a recommendation; the design above assumes it, and the maintainer approved every one.

- **P-Q1, library placement.** A new shared `libRIBImageIO` (`src/imageio/`, class `ImageIO`) holds OIIO. `libRIBRender`
  links it under `HAVE_OIIO`; `libRIBVector` does not (the `HAVE_OPENEXR` mechanism). *Recommended.* Rejected
  alternatives, with reasons, are in research R1: `libRIBCommon`, `libRIBRender`, Infra classification.
- **P-Q2, constitution 1.5.0 text.** Approve [contracts/constitution-1.5.0-draft.md](contracts/constitution-1.5.0-draft.md)
  (Utility libraries paragraph, one-row table, archcheck class `ImageIO`). *Recommended as drafted.*
- **P-Q3, the full-size tags.** OIIO writes `IMAGEFULLWIDTH/LENGTH` only when the full size differs from the data
  size, as `full_width + full_x` (`tiffoutput.cpp:565-572`). Legacy writes them on the first directory of every
  pyramid, also when they are equal (every power-of-two source, including the existing 64x64 fixture).
  - *Recommended (a)*: set `full_x = 1, full_width = W - 1` (and `full_y = 1, full_height = H - 1`; for a 1-pixel
    axis, put the offset on the other axis). OIIO then writes exactly W and H. `full_x` triggers no other tag
    (`XPOSITION` comes from the data origin, `tiffoutput.cpp:912`), and the bake parity test pins it against
    version skew.
  - (b) Amend FR-016 to accept "absent when equal", which the reader treats identically (`texture.cpp:2634-2637`).
  - (c) Bake power-of-two sources on the legacy writer (FR-017).
  - (d) Patch the tags with libtiff after OIIO closes the file. Rejected: that is not writing through OIIO.
- **P-Q4, the bake exclusions.** One rule (data-model §4): 16-bit with 3 or more channels and 8-bit/float with 5 or
  more channels stay legacy; 16-bit 2-channel is written with `tiff:write_extrasamples` 0 to match legacy's missing
  EXTRASAMPLES. The 16-bit reason: OIIO forces `PHOTOMETRIC_RGB` for 3 or more channels
  (`tiffoutput.cpp:657`), with no override. `readMadeTexture<unsigned short>` doubles such textures
  (`texture.cpp:2476`, issue #18). So those combinations go to the legacy writer with a message (FR-017).
  *Recommended.* Changing the reader heuristic is issue #18's follow-up, out of scope.
- **P-Q5, decode acceptance.** The OIIO path refuses exactly what legacy refuses, with the same checks (PNG
  interlace, palette and bit depth from the IHDR bytes; TIFF bits per sample and tiles; EXR parts, channel layout and
  deep). The list then holds only "legacy accepts, the result differs" (research R7). *Recommended.* Alternative:
  accept everything OIIO decodes correctly and list each widening, which is closer to what 049 will do.
- **P-Q6, OIIO selected in a build without OIIO.** `Option "imageio" "backend" ["oiio"]` reports one error and is not
  applied, so the scene renders with `legacy`, as `Option "texturecache"` degrades. An explicit per-call
  `"imageio" "oiio"` on a `Make*` call, or `otexmake -imageio oiio`, fails that operation with the error and writes
  nothing. *Recommended.* Alternative: the Option also makes every OIIO-needing operation fail.
- **P-Q7, `oiio` driver format resolution.** `"string type"` (`tiff`, `png`, `openexr`, `rgbe`) wins; otherwise the
  extension (`.tif`/`.tiff`, `.png`, `.exr`, `.hdr`/`.pic`); otherwise an **error naming the file**, and nothing is
  written. *Recommended*: US4-3 forbids writing a format nobody asked for. This differs from `file`, which writes
  TIFF for any unknown name; documented.
- **P-Q8, RGBE output through `oiio`.** OIIO's HDR writer takes exactly 3 channels (`hdroutput.cpp:206`). The driver
  writes R,G,B from 3- or 4-sample displays (alpha dropped, documented) and refuses 1, 2 or 5 samples. Parity is
  tested on `rgb`. *Recommended.*
- **P-Q9, two legacy defects found, to file as issues and not fix here.** (1) `CRgbeImageInput` uses
  `RGBE_ReadPixels`, which the codec itself says "will not correctly handle run length encoding" (`rgbe.cpp:224`),
  so RLE `.hdr` sources, the common kind, misdecode today. (2) The `rgbe` display driver passes
  `numSamples`-strided scanlines to `RGBE_WritePixels`, which assumes 3 floats a pixel (`rgbe/file.cpp:69`), so
  `rgba` RGBE output is garbled. *Recommended: file both*; each is on the difference list or noted in the driver
  contract. **Approved; filed as GitHub #171 (RLE `.hdr` decode) and #172 (`rgbe` driver `rgba` stride). 048 fixes neither.**
- **P-Q10, spec edge-case wording on OpenColorIO.** "It must not be reached from any path" cannot hold literally.
  OIIO's PNG reader calls `ColorConfig::default_colorconfig()` when a file has a cICP chunk (`png_pvt.h:332`), and
  its PNG **writer** calls it on every file (`png_pvt.h:629`, `write_info`) to choose the gAMA/sRGB chunks. Both
  uses only describe metadata; neither converts a sample. The default config is OCIO's built-in one unless the `OCIO`
  environment variable names another file. *Recommended*: reword to "no colour conversion is ever applied; OIIO may
  load a colour configuration to describe metadata". Back it with two tests: decoded samples are identical with
  `OCIO` unset and with `OCIO` set to a different config, for a cICP PNG on read and for every PNG the driver
  writes.
- **P-Q11, CI.** No CI job enables OIIO in 048. `vcpkg.json` changes only by an unused feature, so the cache key
  changes, the `restore-keys` prefix restores the previous archives, and LLVM's ABI hash is unchanged, so it is not
  rebuilt (research R11). *Recommended.* Alternative: an OIIO-enabled macOS job now (about 9 extra ports, a few
  minutes once cached).
- **P-Q12, Linux.** No system OIIO 3 on 24.04. Document a prefix build of OIIO 3.1.14 with
  `-DOpenImageIO_BUILD_MISSING_DEPS=all` (builds OCIO 2.3+, fmt, robin-map), consumed with
  `-DOpenImageIO_ROOT=`. Linux validation of the OIIO path uses it on the validation host (M3, approval needed).
  *Recommended.* Alternative: vcpkg on Linux, which needs Linux triplets the repo does not have.
- **P-Q13, default of `OPENRENDER_ENABLE_OIIO`.** OFF. *Recommended*: this Mac has Homebrew OIIO 3.2.2 installed,
  and an AUTO default would silently link it, with its ffmpeg, libheif, libraw and jxl closure, into every Homebrew
  build. Explicit ON with OIIO missing prints a warning and builds the legacy tree (US5-4, the OpenEXR precedent).
- **P-Q14, measurements.** Approve M1 (macOS vcpkg OIIO into a private install root) and M3a (Linux prefix build),
  quickstart section M. M2 (CI) is analysed in research R11; M4 (library size) runs at implementation.
- **P-Q15, the per-call `"imageio"` parameter.** Expose and document it on the four `Make*` calls (precedence: call,
  then Option, then `legacy`). *Recommended*: it is the channel `otexmake` and the core use anyway, and it lets RIB
  tests force either path per call. Alternative: keep it internal and undocumented.
- **P-Q16, the RGBE table difference.** List it (D-RGBE-EXP) with a test that pins the exact OIIO values for
  exponent bytes 40, 223 and 226, and keep the parity fixtures exact everywhere else. *Recommended.* No tolerance.
