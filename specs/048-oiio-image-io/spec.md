# Feature Specification: OIIO Image I/O Backend

**Feature Branch**: `048-oiio-image-io`
**Created**: 2026-11-13
**Status**: Draft
**Input**: GitHub issue #87 (new capability, layer L7, "Spec 048: OIIO image I/O as an extra decode, display and texmake backend"); the maintainer's plan, row 048 and its decision table ("OIIO" row, D30's utility-library note); `DEVNOTES_DETAILS/RENDER_ARCHITECTURE_REFERENCES.md` D23, D24, D30 and §11.3. Builds on specs 018 (`CImageInput`), 019 (`CTileSource`), 020 (runtime tile paging), 027 (texture system, merged as `486decd2`) and 037 (display ABI 2.0.0, `CDisplayHost`). Blocks 049 (the OSL backend, where the legacy codecs are removed).

## Clarifications

### Session 2026-11-13

- Q1: Is OIIO an optional or a required build dependency until spec 049? -> A: Optional. Ubuntu 24.04 ships OIIO 2.4.17, so a required OIIO 3 would force the FHS and release builds to bundle it before D24 plans. A build without OIIO behaves exactly as today (FR-003).
- Q2: How is the OIIO path selected, and what is the default? -> A: Explicit selection only: a per-scene option or attribute (decode and baking), an `otexmake` flag, and a display driver name; exact spellings are decided at plan. The legacy code stays the default everywhere, so nothing changes unless OIIO is asked for (FR-013). OIIO is not a silent fallback for formats the legacy decoders lack.
- Q3: Do formats only OIIO reads (JPEG, TGA, BMP, WebP) enter scope? -> A: No. Only the legacy formats (TIFF, PNG, RGBE, OpenEXR where built) go through OIIO; OIIO-only formats wait for spec 049 (FR-023).
- Confirmed unchanged: an OIIO-backed tile source under the runtime texture cache stays out of scope (FR-018).

### Session 2026-11-14 (plan approved)

- Q4: Does "OpenColorIO must not be reached from any path" hold literally? -> A: No. OIIO's PNG reader (cICP) and PNG writer load a colour configuration to describe metadata. The edge case now says "no colour conversion is ever applied", backed by a test with the `OCIO` environment variable unset and set to another configuration (plan P-Q10).
- Constraint from the coordinator: the worktrees share one vcpkg install directory (`VCPKG_INSTALLED_DIR`). This spec MUST NOT install into or modify the shared one; where it needs OIIO through vcpkg it uses its own install directory for this worktree, and no vcpkg run happens before the maintainer approves it through the coordinator (FR-024).

## User Scenarios & Testing *(mandatory)*

The "users" are the people who write RIB scenes and bake textures with `otexmake`, the people who choose display drivers, the maintainers who review image changes and architecture boundaries, and the author of spec 049, who needs OIIO to be a proven, parity-tested part of the build before OSL makes it integral and the old codecs go away.

Today every image format is handled by its own hand-written path: `CImageInput` has four decoders (TIFF, PNG, RGBE, OpenEXR when built), `otexmake` writes TIFF through libtiff directly, and four display drivers (`file` for TIFF/PNG, `openexr`, `rgbe`, `framebuffer`) each carry their own codec code. OpenImageIO (OIIO) is the library OSL needs anyway. This spec adds OIIO next to those paths, as an *extra* backend, and proves it produces the same pixels. It removes nothing and changes no default.

### User Story 1 - Read images through OIIO with the same samples (Priority: P1)

A scene author or baker points the renderer or `otexmake` at an image and chooses the OIIO decode path. Every format the legacy decoders read (TIFF, PNG, RGBE, and OpenEXR where it is built) comes back with the same dimensions, channel count, sample precision and sample values as the legacy decoder gives. No colour-space or gamma conversion is applied: samples arrive exactly as stored, as `CImageInput`'s contract says today.

**Why this priority**: it is the foundation of the whole spec (the other stories build on a working OIIO decode path) and the acceptance criterion of issue #87 starts with "pixel parity for every format".

**Independent Test**: for each format fixture already used by the `ImageInput_*` tests, decode with the legacy backend and with the OIIO backend and compare `CImageInfo` and every sample. The test needs no display driver and no baking.

**Acceptance Scenarios**:

1. **Given** an 8-bit, a 16-bit and a float image in each legacy format, **When** each is read by both backends, **Then** the dimensions, channel count, bits per sample, float flag and every sample are identical.
2. **Given** a file the legacy backend refuses (for example an indexed PNG or a multi-part EXR), **When** it is read through the OIIO path, **Then** the outcome is either identical rejection with an error naming the file and the reason, or a documented, tested difference listed in one place; it is never a silent wrong pixel.
3. **Given** a missing, truncated or corrupted file, **When** it is read through the OIIO path, **Then** the call fails with an error naming the file and the reason, and no partial image is delivered as if it were valid.
4. **Given** a format the legacy code does not read but OIIO does (JPEG, TGA, BMP, WebP), **When** it is read through the OIIO path, **Then** it is refused with an error naming the file and the unsupported format, as the legacy path refuses it; such formats wait for spec 049 (FR-023).

---

### User Story 2 - Nothing changes unless OIIO is asked for (Priority: P1)

A user who never selects OIIO sees no difference at all: every existing scene renders the same bytes, every existing baked texture is the same file, every display driver writes the same output, and every existing test keeps its threshold.

**Why this priority**: the issue says "nothing removed"; 049 is where the old paths go. Until then the legacy paths are the reference the OIIO paths are measured against, so they must stay as they are.

**Independent Test**: build with OIIO present and run the existing suite (`image_input`, `texture_tile`, `texture_only`, `plugin_abi`, `visual`, `architecture`) with no selection made. All existing results, references and thresholds are unchanged.

**Acceptance Scenarios**:

1. **Given** a build that includes OIIO and no backend selected, **When** the existing suite runs, **Then** every test passes against its existing reference with no reference re-captured and no threshold changed.
2. **Given** the same scenes rendered by a build with OIIO and a build without it, **When** their images are compared, **Then** they are byte-identical.

---

### User Story 3 - Bake textures through OIIO and keep the texture layout (Priority: P2)

A baker runs `otexmake` with OIIO writing selected. The baked texture, shadow map and environment map keep everything the renderer reads: the Pixar texture tags (the texture-format string including openRender's custom strings for cubic, spherical and cylinder environments, the full image width and length, the wrap modes, and the world-to-camera and world-to-screen matrices of shadow maps), the tile layout and the sample format. A renderer reading an OIIO-baked file renders the same image as one reading the legacy-baked file.

**Why this priority**: it is the riskiest of the three backends: the private tags and environment layouts are what the texture system reads, and OIIO may not preserve them. The issue gates writing on that.

**Independent Test**: bake the checked-in `ImageInput_TiffBakeByteIdentical` fixtures plus a plain, a cubic-environment, a spherical-environment, a cylinder-environment and a shadow-map case with both writers, then compare the tag sets and the decoded samples.

**Acceptance Scenarios**:

1. **Given** a source image, **When** it is baked by the legacy writer and by the OIIO writer, **Then** every Pixar texture tag and the tile layout read back identical and the decoded samples are identical.
2. **Given** a cubic, a spherical and a cylinder environment source, **When** each is baked through OIIO, **Then** the environment layout and its format string match the legacy bake.
3. **Given** a scene using a texture baked through OIIO, **When** it renders, **Then** the image matches the same scene with the legacy-baked texture within the scene's existing threshold, unchanged.
4. **Given** that OIIO cannot preserve a required tag or layout for some texture kind, **When** that kind is baked, **Then** the writer for that kind stays on the legacy path, the exclusion is documented with the reason, and no OIIO-written file of that kind is ever produced.

---

### User Story 4 - One OIIO display driver (Priority: P2)

A scene author writes `Display` statements that use one new driver, backed by OIIO, instead of the separate `file`, `openexr` and `rgbe` drivers, and obtains the same pixels for the formats those drivers write today (TIFF, PNG, OpenEXR, RGBE). The driver follows display ABI 2.0.0 and loads through the existing display host like any other driver.

**Why this priority**: it is the user-visible output half of the spec, but the legacy drivers keep working, so it can land after the decode path.

**Independent Test**: render one fixed scene to each format with the legacy driver and with the OIIO driver, decode both outputs with one independent decoder and compare every sample. The framebuffer driver is not part of this test.

**Acceptance Scenarios**:

1. **Given** a scene and a quantization, channel set and format, **When** it is written by the legacy driver and by the OIIO driver, **Then** the decoded samples of the two files are identical.
2. **Given** the OIIO driver, **When** it is loaded, **Then** it declares a display interface version and passes the same load checks every driver passes (`checkPluginVersion`, display ABI 2.0.0).
3. **Given** a format the driver cannot write in this build (for example OpenEXR when absent), **When** a `Display` asks for it, **Then** the renderer reports a clear error naming the format; it does not write a different format.

---

### User Story 5 - OIIO enters the build without breaking the architecture or the platforms (Priority: P1)

A maintainer builds on macOS (vcpkg-vendored), on Ubuntu 24.04 and in CI. OIIO 3.0 or later is obtained the way the project's other dependencies are, it is linked into exactly one shared library, the constitution is amended so the two domains that need it (Texturing and Display Outputs) may depend on the utility library that wraps it, and the architecture check passes with no new allowlist line.

**Why this priority**: without it no other story ships; it is also where a mistake costs most (binary size, the second-copy trap of gotcha 22, the dependency rules of Principle VIII).

**Independent Test**: configure and build each supported platform; run `ctest -L architecture`; inspect the installed tree for which images reference OIIO.

**Acceptance Scenarios**:

1. **Given** the installed tree, **When** the binaries and libraries are inspected, **Then** exactly one shared library contains OIIO's code, and every other binary, module and static archive reaches it only through that library.
2. **Given** the change, **When** `ctest -L architecture` runs, **Then** it passes and `tests/architecture/allowlist.txt` has gained no line.
3. **Given** the amendment, **When** the constitution, `tests/architecture/domain-rules.txt` and `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` are read, **Then** they agree: the utility library, who may depend on it, and the rule that it depends on no domain.
4. **Given** a platform where OIIO 3.0 is not available from the system package manager (Ubuntu 24.04 ships 2.4.17), **When** the project is configured, **Then** OIIO is treated as absent: configure prints a clear message and the build is the working legacy build, behaving exactly as today (FR-003).

---

### Edge Cases

- A file whose extension and content disagree (a PNG named `.tif`): the OIIO path chooses by content or by extension, and the rule is the same as the legacy dispatch or a documented difference.
- 16-bit and half-precision sources: the legacy decoder has "no half-precision tier"; the OIIO path must deliver the same sample type the legacy path delivers for each case (half is widened to float), not a new one.
- Alpha and extra channels, premultiplication and channel naming: the OIIO path delivers the channels in the stored order with no reordering and no premultiplication change.
- Tiled and scanline sources, any planar configuration, very large images, and images with an odd tile size.
- OIIO reading a format by plug-in that is not compiled into the vendored build: the error names the file and the missing format reader, as the legacy path names a missing OpenEXR.
- OIIO's own optional colour management (the vendored OIIO 3 build pulls in OpenColorIO): no colour conversion is ever applied to a sample, on any path here. OIIO may load a colour configuration to describe metadata (its PNG reader on a cICP chunk, its PNG writer for the gAMA/sRGB chunks); that is not a conversion, and a test shows the samples are the same with the `OCIO` environment variable unset and set to another configuration.
- A process that holds two copies of OIIO (one in the utility library and one statically in a module): this is the failure gotcha 22 describes for `libRIBCommon`; the single-shared-library requirement exists to rule it out, and the display driver module, which is `dlopen`ed (gotcha 27), must not carry its own copy.
- Threads: the texture system decodes from several threads under its load lock (gotcha 25); the OIIO decode path must not take that lock re-entrantly and must be safe when several `CImageInput`s are open at once.
- An OIIO error or exception must become the project's `error()` report, never an escaping C++ exception across a plug-in boundary.

## Requirements *(mandatory)*

### Functional Requirements

**Dependency and architecture**

- **FR-001**: The system MUST use OIIO 3.0 or later as an additional image I/O backend, and MUST NOT remove, disable, rename or change any existing codec path, decoder, display driver, baker, build option or test in this spec (removals belong to spec 049).
- **FR-002**: OIIO MUST be obtained as a vendored dependency on macOS through the project's existing vcpkg mechanism, and MUST be found by the system package manager or documented installation steps on Linux; the minimum version (3.0) and every build option that selects it MUST be documented where the other dependency requirements are documented.
- **FR-003**: OIIO MUST be an optional build dependency until spec 049. A build configured without OIIO (not found, or switched off) MUST succeed and behave exactly as today: no OIIO code, no test failing for lack of OIIO (OIIO-dependent tests are not registered), and selecting an OIIO path in such a build reports a clear error naming the missing dependency. CI MUST keep at least one job without OIIO, and the FHS and release builds MUST NOT be required to bundle OIIO in this spec.
- **FR-004**: OIIO MUST be linked into exactly one shared library of the project. Every consumer (the renderer library, the vector library, `otexmake` and the other tools, the OIIO display driver module, the static packages) MUST reach OIIO only through that library's published interface and MUST NOT carry OIIO's code. A test MUST fail if a second image in the install tree contains OIIO symbols.
- **FR-005**: The OIIO wrapper MUST be a utility library that sits beside `libRIBCommon` (decision D30, part 4) and depends on no domain. Texturing and Display Outputs MUST reach it through its published interface only, and neither domain MUST depend on the other.
- **FR-006**: The constitution amendment that allows Texturing and Display Outputs to depend on the utility library MUST land in this spec: the constitution text, its version and Sync Impact Report, `tests/architecture/domain-rules.txt` and `tests/architecture/domain-map.txt`, the plan template's Principle VIII gate if it lists classes, and `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` MUST be changed together and agree.
- **FR-007**: `ctest -L architecture` MUST pass with no line added to `tests/architecture/allowlist.txt`; no new include or link edge outside the allowed-dependency table MAY be introduced.
- **FR-008**: The display driver module MUST follow display ABI 2.0.0 (spec 037): C linkage, an exported version query, loaded and version-checked by the existing host; it MUST NOT include renderer-core headers.

**Decode backend**

- **FR-009**: An OIIO-backed `CImageInput` MUST exist behind the existing interface (`open`/`readImage`/`close`, one-shot contract), so every current caller (`otexmake`, the synthesized texture source of spec 020) can use it without code that knows which backend it has.
- **FR-010**: For every format and sample precision the legacy decoders accept, the OIIO decode path MUST return identical `CImageInfo` and identical samples in the same native sample type and layout (row-major, tightly packed, channel order as stored), with no colour or gamma conversion.
- **FR-011**: Failure handling MUST match the interface contract: a failed `open` or `readImage` returns false and reports through the project's error mechanism an error naming the file and the reason; an OIIO exception or error string MUST NOT escape unreported.
- **FR-012**: Every difference between the OIIO and legacy decode paths (a file one accepts and the other refuses, a channel-count or precision case that differs) MUST be recorded in one documented, tested list; a difference that is not on the list is a parity failure.

**Backend selection**

- **FR-013**: The OIIO paths MUST be selected explicitly: decode and baking by a per-scene option or attribute and by an `otexmake` flag, display by a driver name (exact spellings decided at plan). The legacy path MUST be the default everywhere, a selection MUST be visible (an option, a flag, a driver name) and never inferred from the environment alone, and OIIO MUST NOT act as an automatic fallback for a format the legacy decoders do not read. Tests MUST be able to force either path.
- **FR-014**: With no OIIO path selected, every existing render, baked texture, display output and test MUST be byte-identical to the tree before this spec, with no reference re-captured and no threshold widened.

**Baking**

- **FR-015**: `otexmake` MUST be able to bake through OIIO writing, for the texture, shadow-map and environment modes it supports today.
- **FR-016**: An OIIO-written baked file MUST carry, with the same values as the legacy writer: the Pixar texture-format tag including openRender's custom strings for cubic, spherical and cylinder environments, the full image width and length tags, the wrap-mode tag, and for shadow maps the world-to-camera and world-to-screen matrices; and the same tile size, sample format and sample count. Writing through OIIO for a texture kind is allowed only if this holds for that kind (the gate from the issue).
- **FR-017**: If OIIO cannot preserve a tag or layout of some texture kind, that kind MUST be baked by the legacy writer even when OIIO writing is selected, with a message saying so, and the limit MUST be documented. The spec's success then covers the kinds that pass.
- **FR-018**: Baked files read by the texture system (`CTextureSystem` and the tile sources of specs 019, 020 and 027) MUST be read by the existing path; the runtime texture cache, its budgets and its load lock stay as they are. An OIIO-backed tile source is out of scope (see Assumptions).

**Display**

- **FR-019**: One OIIO display driver MUST write TIFF, PNG, RGBE and (where OpenEXR is available) OpenEXR output, and MUST accept the display parameters the legacy `file`, `openexr` and `rgbe` drivers accept (quantization, channel selection, and the others they document), producing the same samples.
- **FR-020**: The legacy `file`, `openexr`, `rgbe` and `framebuffer` drivers MUST remain installed, loadable and unchanged; legacy driver names MUST NOT become aliases of the OIIO driver in this spec (049 does that).

**Quality and documentation**

- **FR-021**: Parity (FR-010, FR-016, FR-019) MUST be asserted with tests that run in the default suite on every supported platform, using fixtures small enough not to slow it; no parity test MAY be skipped or given a tolerance to pass. Where float output cannot be bit-identical, the difference MUST be explained and the tolerance decided at the planning stage with evidence, never widened to turn a test green.
- **FR-022**: The user-facing documentation (the `otexmake` and display-driver manual pages, the site pages, `INSTALL.md` / `COMPILING.txt`, `DEVNOTES.md`, `DEVNOTES_DETAILS`, and the plug-in ABI document if the version-history table changes) MUST describe the OIIO path, its selection, its minimum version, its known differences and where it is installed.
- **FR-023**: Input formats that only OIIO reads (for example JPEG, TGA, BMP, WebP) are out of scope: the OIIO decode and baking paths MUST accept only the formats the legacy decoders read (TIFF, PNG, RGBE, and OpenEXR where built) and MUST refuse any other with an error naming the file and the format.
- **FR-024**: This spec MUST NOT install into or modify the shared `VCPKG_INSTALLED_DIR` used by the other worktrees. Where OIIO is needed through vcpkg, this worktree MUST use its own install directory, and no vcpkg run MAY happen without the maintainer's approval (asked through the coordinator). The effect of a changed `vcpkg.json` on the CI cache key (it could re-trigger the LLVM build) is measured at the plan stage.
- **FR-025**: No task MAY depend on this spec's merge: registry entries move to Resolved with the spec number and date; issue closing happens through the maintainer's merge commit.

### Key Entities

- **OIIO decode backend**: an implementation of `CImageInput` that reads through OIIO. Attributes: the file, its `CImageInfo`, the native sample type.
- **OIIO baking writer**: the `otexmake` writer that emits a tiled baked texture through OIIO. Attributes: the texture kind (plain, shadow, cubic, spherical or cylinder environment), the Pixar tag set it must carry.
- **OIIO display driver**: one display module (ABI 2.0.0) writing several formats; attributes: format, quantization, channel set.
- **Image I/O utility library**: the one shared library that contains OIIO and publishes the small interface Texturing and Display Outputs use. It depends on no domain.
- **Parity fixture**: a small image (or scene) per format and precision, with the legacy backend's output as the reference.
- **Difference list**: the documented, tested list of cases where the two paths are allowed to differ.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For every format fixture the legacy decoders accept (each precision they support), the OIIO decode path yields identical dimensions, channels, precision and samples: 0 differing samples, 100% of fixtures.
- **SC-002**: For every texture kind that OIIO baking is enabled for, the baked file's Pixar tag set, tile layout and sample format are identical to the legacy bake: 0 differing tags; for each kind it is not enabled for, the exclusion and its reason are documented.
- **SC-003**: For TIFF, PNG, RGBE and (where built) OpenEXR output, a fixed scene written by the legacy driver and by the OIIO driver decodes to 0 differing samples.
- **SC-004**: With no OIIO path selected, 100% of the existing tests pass against unchanged references and unchanged thresholds, and a render with OIIO present is byte-identical to a render without it.
- **SC-005**: Exactly one shared library in the install tree contains OIIO symbols; no other binary, module or archive does.
- **SC-006**: `ctest -L architecture` passes and `tests/architecture/allowlist.txt` is unchanged (0 lines added); the constitution, domain rules and `DOMAIN_BOUNDARIES.md` agree on the utility-library amendment.
- **SC-007**: The added install size (the OIIO library and its dependencies, and the per-binary cost) is measured and recorded on macOS arm64 and Linux; the earlier measurement for comparison is about +8.3 MB per statically linking binary on macOS (`RENDER_ARCHITECTURE_REFERENCES.md` §11.3), which the single-library rule is meant to avoid.
- **SC-008**: The build and test steps of the supported platforms (macOS vendored, Ubuntu 24.04, CI) succeed, with the first-build time and disk cost of adding OIIO recorded.
- **SC-009**: Every difference between the two decode paths is on the documented list, and each listed difference has a test.

## Assumptions

- Spec 027 is merged into master (verified: `486decd2`), and this worktree starts from master `278b0394`; specs 018, 019, 020 and 037 are merged and are the code being extended.
- The legacy decoders, the legacy `otexmake` writer and the `file`, `openexr`, `rgbe` and `framebuffer` drivers stay the reference for parity and are not edited except for the additive hooks the selection needs. The framebuffer driver has no format to write and is out of scope.
- OIIO is used for pixel I/O only. OpenColorIO, which the vcpkg OIIO 3.x port requires, is linked because it comes with OIIO, but no colour conversion is exposed or applied.
- Out of scope: an OIIO-backed tile source under the runtime texture cache (the plan brief says one "may" sit there; it is not in issue #87's scope list and can be a follow-up); removing any legacy codec, `HAVE_OPENEXR`, the JIT-off build or the CMake 3.19 floor (spec 049); making legacy display-driver names aliases of the OIIO driver (049); OSL itself; deep or multi-part images; GPU paths.
- Success criteria use parity with the legacy paths as the oracle, so no new image reference is needed; if one is, it is captured on the pre-change tree only, never re-captured to make a test pass.
- Windows is not supported (constitution Principle VI); platforms are macOS (vcpkg and Homebrew) and Linux (Ubuntu 24.04 baseline).
- The FHS package keeps linking system packages (CLAUDE.md, vcpkg section). Ubuntu 24.04's OIIO is 2.4.17, below the 3.0 floor; D24 plans for the FHS package to bundle OIIO 3 only when OSL lands, so this spec does not make the FHS build depend on OIIO 3 (Q1 was answered "optional").
- Decisions cited: D23 (OIIO 3.0 or later, vendored, extra backend until OSL, one shared library), D24 (OSL later makes OIIO integral), D30 part 4 (the OIIO wrapper is a utility library; the amendment lands here). This spec changes none of them.

### Risks and merge-order notes (for the planning stage)

- **Build and disk cost on a fanless M2.** OIIO 3 through vcpkg pulls about 23 ports (OpenColorIO is mandatory in the 3.1.14 port) and measured 3.8 minutes to build and 57 MB installed on macOS arm64 (2026-10-15 measurement, static triplet); the per-package binary cache should keep that a one-time cost. The larger risks are: (a) adding OIIO to `vcpkg.json` changes the manifest of the shared `VCPKG_INSTALLED_DIR`, which every worktree uses, so another worktree's configure may uninstall or reinstall packages in the shared tree while this one works (settled by FR-024: this worktree uses its own install directory); (b) CI cache keys include `hashFiles('vcpkg.json')`, so a changed manifest may re-trigger the roughly 110-minute LLVM build unless the restore-key prefix and the per-package ABI hashes restore it; (c) the OIIO footprint on Linux is unmeasured. The plan must measure (a) to (c) on the target machines, through `cpujob`, and say where vcpkg is run.
- **Concurrent specs (033 shading-system backends, 040 GI determinism, 057 area-light geometry).** Their worktrees were clean at the time of writing, so no code overlap is known; the briefs suggest the following:
  - *All three, textually*: `CLAUDE.md` (Recent Changes, numbered gotchas, the Testing list), `DEVNOTES.md` and `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` will each be edited by every spec, so each merge conflicts on those lines; the later merge resolves them. 048 also edits the constitution and `tests/architecture/{domain-rules,domain-map}.txt`, whose version header a second amendment would collide with.
  - *033*: moves shader loading and the backend split in `src/libshader` and will delete allowlist lines and change `tests/architecture/metrics-baseline.txt`; 048 adds none, so only line-range conflicts are possible. Both may touch the root `CMakeLists.txt` option list.
  - *040*: works on the Texturing-owned GI storage in `src/ri/texture/` (photon and irradiance stores), the same directory as the decode and bake code here; shared source lists in `src/ri/CMakeLists.txt` are the likely conflict.
  - *057*: Geometry and lights; no overlap expected beyond the documentation files.
  - Merge order is not constrained by dependencies; whichever of 048/040 lands second reconciles `src/ri/CMakeLists.txt`.

## Open Questions

None. Q1 to Q3 were answered on 2026-11-13 (see Clarifications).

Plan-stage decisions deliberately left out of this spec: which shared library holds OIIO (the brief says "decided in plan"), the exact selection spellings, the tolerance question of FR-021, the vcpkg feature set (OpenColorIO cannot be dropped from the 3.1.14 port today), whether the Linux build finds a system OIIO or fetches one, and the CI cache-key effect of the `vcpkg.json` change.
