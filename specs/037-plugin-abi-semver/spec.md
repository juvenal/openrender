# Feature Specification: Versioned Plug-in ABIs

**Feature Branch**: `037-plugin-abi-semver`
**Created**: 2026-10-28
**Status**: Draft
**Input**: Constitution Principle VIII rule 4 remediation, layer L7 (modernization), after spec 023; GitHub issue #26 (registry DB-003) plus the plug-in ABI safety work of DB-014, the channel-descriptor half of DB-021 and the DSO-loading half of DB-019. The four published plug-in headers (`shadeop.h`, `dlo.h`, `implicit.h`, `dsply.h`) gain a SemVer interface version, a version query symbol and C-includability, each with a host-side MAJOR check. Display plug-ins move to a v2 interface (per-display context handle, `const` parameter snapshot, channel descriptors, no writable pointers into renderer statics, no file-static current display), with a documented threading contract and deferred unload. Shader DSO operands get a per-thread init and one shadeop module that owns loading and `dsoParse`. One Geometric Procedures module owns the dlo, implicit and RiProc loaders. The spec also defines the **shared plug-in convention** (decision D7 of `DEVNOTES_DETAILS/RENDER_ARCHITECTURE_REFERENCES.md`, section 9) that every later plug-in type copies. Run-time loading of the RIS plug-in types (integrators, Bxdfs, lights, filters, patterns, hiders) is spec 051 and is out of scope here.

## Clarifications

### Session 2026-10-29

- Q: Display v1 support (FR-006)? -> A: Option B. Display v2 is the new MAJOR; a confined v1 adapter keeps legacy and explicit-1.x display drivers working, so the host supports each display MAJOR it still lists (1 and 2). Constitution rule 4 is read as "the host rejects a MAJOR it does not support". The constitution is NOT edited by this spec; an amendment is a separate decision (see Assumptions).
- Q: Legacy unversioned plug-ins (FR-007)? -> A: Option B. Unversioned plug-ins load as 1.0.0 with a warning that names the release in which they will be rejected. The rejection happens in that later release; nothing in this spec depends on it. The release is 1.2.0 (037 is expected to ship in 1.1.0).
- Q: Sunset release? -> A: 1.2.0. 037 is expected to ship in 1.1.0; the rejection happens in 1.2.0, not in 037, and no 037 task depends on it.
- Q: v1 adapter and renderer pointers? -> A: Copies only. A v1 driver never gets a writable alias into renderer state; a third-party driver that wrote through those pointers loses that effect, and that is intended.
- Q: Warning for an explicit display 1.x? -> A: None. Only unversioned plug-ins get the FR-007 warning.
- Q: Dynamic-load generator module lifetime? -> A: Released at frame end, after the geometry it generated is destroyed (FR-024).
- Q: `file_base.h` (FR-026)? -> A: Stop installing it; `CFileOutputBase` stays an in-tree helper for the in-tree file drivers. Install lists and docs (US8) follow.
- Q: Is US7 (Texturing channel interface, DB-021's 3 edges) in 037? -> A: Yes, it stays.
- Q: What happens to the incidental defects found while specifying? -> A: They are fixed in 037 as the work reaches each one, tracked by FR-031 to FR-039 with acceptance coverage.

## User Scenarios & Testing *(mandatory)*

The "users" of this feature are the people who write and ship plug-ins (display drivers, shader DSO operands, dynamic-load procedurals and implicit surfaces, in-tree and third-party), the maintainers who review them against constitution Principle VIII, and the authors of the later specs that add plug-in types (035, 041, 043, 051) and need one convention to copy.

### Measured starting point (2026-10-28, master `486decd`)

- **No version anywhere.** None of `src/ri/render/shadeop.h`, `src/ri/geometry/dlo.h`, `src/ri/geometry/implicit.h` or `src/display/dsply.h` declares a version, and no loader reads one. All four are installed to `<prefix>/include` (with `ptcapi.h`, `ri.h`), so third-party plug-ins compile against them today with no compatibility signal. 16 files (sources, tests, the example DSO) include these headers.
- **Not C-includable.** `dsply.h`, `dlo.h` and `implicit.h` open `extern "C" {` with no `__cplusplus` guard; the `SHADEOP_*` macros in `shadeop.h` expand to `extern "C"`. A C17 plug-in cannot include any of them (constitution Principle II sets C17 for C code). `shadeop.h` also disagrees with itself: the `dsoExecFunction` typedef is `void (*)(void *, int, void *[])` while the `SHADEOP` macro defines `void f(void *, int, char *[])`.
- **Display drivers receive writable renderer state.** The `TDisplayParameterFunction` callback (`findParameter`, `src/ri/render/rendererDisplay.cpp`) returns non-`const` `void *` into nine renderer-core statics (`clipMin`, `clipMax`, `fromWorld`, `worldToNDC`, `toScreen`, `gamma`, `gain`, and the `depthQuantizer`/`colorQuantizer` arrays), into the display's own quantizer array and into its parameter data. It reads a file-static `currentDisplay` that the host sets just before each `displayStart`; a driver that calls `findParameter` after another display has started reads the wrong display's parameters, and `quantize`/`dither` dereference `currentDisplay` without a null check. In-tree drivers call it only inside `displayStart` (their constructors), but nothing in the ABI says so.
- **Channel information is lossy.** `displayStart` receives the channel *string* and a total sample count; the host's own comment says "above sample names are now quite incorrect" for AOV displays. A driver cannot learn each channel's name, sample count, value type or offset.
- **Unload race.** When `displayData` returns false, `CRenderer::dispatch` unloads the driver module under a mutex, but other render threads test `datas[i].module` outside it and may be inside, or about to call, the unloaded module. The normal path (`endDisplays`) calls `displayFinish` and never unloads at all. Reentrancy is undocumented. `displayRawData` is exported as a no-op stub by three drivers and never called by the host.
- **Two display ABI definitions.** `ri.h` declares `RtDisplay*Function` typedefs (an `int` parameter type) and the host casts the `RiCustomDisplay` callbacks into the `dsply.h` types.
- **Shadeop loading is process-global and duplicated.** `CRenderer::getDSO` (`rendererFiles.cpp`) scans the procedural search path, loads every module, and calls the shadeop's `init(0, NULL)` once; the returned pointer is passed to every `exec` call from every shading thread. A second, independent scan lives in the compiler (`libshader/compiler/rslo.cpp`). `dsoParse` (flex/bison, `src/ri/parse/dso.{l,y}`) is compiled into both `libRIBRender` and `libshader_compiler`, keeps its result in file-statics, and is the source of the three DB-019 include edges. The scan keeps iterating past a table entry whose `exec` symbol did not resolve (the check tests the output pointer, which is never null).
- **Three scattered procedural loaders.** DLO loading is in `RiGeometryV` handling in `rendererContext.cpp` (it locates the file and then loads the unresolved `name`, not the located path); implicit surfaces load in the `CImplicit` constructor (`geometry/implicitSurface.cpp`); `RiProcDynamicLoad` is in `parse/ri.cpp` and never unloads its module (the unload call is commented out). Each is a separate, unversioned `osLoadModule`/`osResolve` sequence.
- **`file_base.h` is installed but unusable.** It is installed next to `dsply.h`, but it includes `common/os.h` (not installed), declares a C++ class with virtuals whose implementation lives in the static `openrenderfilebase` library (not installed), and so cannot be used by anyone outside the tree. `DISPLAY_PLUGIN_GUIDE.md` and `INSTALL_ARTIFACTS.md` describe it as a third-party facility and carry stale paths (`ri/dsply.h`, `src/file/`).
- **Registry and architecture check.** DB-003 (no version), DB-014 (unsafe beyond versioning) and the 037 halves of DB-019 (3 include edges to `dso.h`) and DB-021 (3 edges: 2 include, 1 link, in `texture3d.cpp`) are Open; DB-024 lists the duplicate display ABI as evidence. No test in the tree builds a plug-in module; `examples/dsotest` is not part of the build.

### User Story 1 - The host rejects an incompatible plug-in with a clear diagnostic (Priority: P1)

A plug-in author, or a user who installed a plug-in built for another release, loads a display driver, shadeop DSO, dynamic-load object or implicit-surface module. If the plug-in was built against an incompatible MAJOR version of the interface, the renderer refuses it and says so in plain words, naming the file, the interface, the plug-in's version and the host's version. A plug-in built against an older MINOR loads; one built against a newer MINOR loads with a warning; one with no version at all (every plug-in built before this spec) loads as 1.0.0 with a warning that names the release in which unversioned plug-ins will be rejected. For display plug-ins the host supports MAJOR 1 (through a confined adapter) and MAJOR 2; any other MAJOR is rejected.

**Why this priority**: It is constitution Principle VIII rule 4 and the acceptance of issue #26. Everything else in this spec is built on a version being declared and checked.

**Independent Test**: Build one test plug-in per interface at the matching version, at a different MAJOR, at a newer MINOR, and with no version symbol (a legacy plug-in); load each through the real loader and assert the outcome and the diagnostic text.

**Acceptance Scenarios**:

1. **Given** a test plug-in whose declared MAJOR is not one the host supports, **When** the renderer loads it, **Then** the load fails, the plug-in is not called, the diagnostic names the file, the interface, both versions and the remedy ("rebuild against the installed header"), and the render either continues without that plug-in or fails exactly as it does today for a plug-in that cannot be found.
2. **Given** a plug-in with a MAJOR the host supports and the same or an older MINOR, **When** it loads, **Then** it works and no version diagnostic is printed.
3. **Given** a plug-in with a MAJOR the host supports and a newer MINOR, **When** it loads, **Then** it loads and one warning names both versions.
4. **Given** a plug-in that exports no version query symbol, **When** it loads, **Then** it is treated as 1.0.0 and one warning per module per run says it is unversioned and names the release in which it will be rejected.
5. **Given** the four in-tree display driver modules (file for TIFF and PNG, OpenEXR, RGBE, framebuffer), **When** the tree is built, **Then** each exports the current display interface version (2.0.0), loads without a diagnostic and renders the same image as before.
6. **Given** a legacy display driver (no version symbol, or an explicit 1.x) built before this spec, **When** the renderer loads it, **Then** it loads through the v1 adapter with the legacy warning (unversioned only), renders the same image as before, and cannot reach or write renderer-core state.

---

### User Story 2 - The headers can be included from C and carry their version (Priority: P1)

A plug-in author writing in C (or in C++) includes `shadeop.h`, `dlo.h`, `implicit.h` or `dsply.h`, in any combination and in either language, and gets the interface's version macros and the macro that exports the version query symbol, so declaring the version costs one line.

**Why this priority**: A C function-table boundary that C cannot include is not a C boundary. The headers are also where the version history is documented.

**Independent Test**: Compile a translation unit that includes all four headers as C17 with strict warnings as errors, and the same set as C++20; compile a plug-in built with hidden-by-default symbol visibility and confirm its entry points and version symbol are still exported.

**Acceptance Scenarios**:

1. **Given** each of the four headers, **When** included alone and together by a strict C17 translation unit, **Then** it compiles without warnings.
2. **Given** the same headers included by a C++20 translation unit, **When** compiled, **Then** every existing in-tree plug-in and the example DSO still compile unchanged apart from the one-line version declaration.
3. **Given** a plug-in built with hidden-by-default visibility, **When** it is loaded, **Then** the entry points and the version query symbol resolve.
4. **Given** the `shadeop.h` exec typedef and the `SHADEOP` macro, **When** both are compiled together, **Then** they describe one signature (the `void *[]` versus `char *[]` mismatch is gone; tracked by FR-031).

---

### User Story 3 - Display plug-ins get a safe, documented v2 interface (Priority: P2)

A display driver author receives, for each display, a per-display context handle, a `const` snapshot of that display's parameters (the RIB `Display` parameters plus the renderer-provided ones: near, far, the camera and projection matrices, gamma, gain, quantization, dither, software string, display type) and a list of channel descriptors (name, sample count, value type, offset in the data the driver is sent). Nothing the driver can reach is writable renderer state, and nothing depends on which display the host started last. The threading contract is written down: when `start`, `data` and `finish` may be called, whether `data` may run on several threads at once, and when the module can be unloaded. A driver that disables itself (its `data` call reports failure) is never unloaded while another render thread may still be inside it.

**Why this priority**: These are the real safety defects (DB-014) and the reason display plug-ins are the first family to move to a new MAJOR. It is P2 only because versioning (US1, US2) must exist for a v2 to be distinguishable from v1.

**Independent Test**: Render with test drivers, including a hostile one that overwrites everything it can reach, and two displays with different parameters started in the same frame; compare against the pre-change images for the in-tree drivers.

**Acceptance Scenarios**:

1. **Given** two displays in one frame with different `quantize` and `dither` parameters, **When** both start, **Then** each driver sees exactly its own values, whatever order they start and whenever the driver reads them during `start`.
2. **Given** a hostile test driver that casts away `const` and overwrites every byte it can reach through its context, snapshot and descriptors, **When** the frame renders, **Then** the image from the in-tree drivers is unchanged and the renderer's own state (clip planes, matrices, gamma, gain, quantizers) is identical afterwards.
3. **Given** an AOV display with several channels, **When** the driver starts, **Then** the descriptors give each channel's name, sample count, type and offset, and the offsets and counts add up to the sample count the driver is sent in every tile.
4. **Given** a driver whose `data` call fails on the first tile, **When** many threads keep dispatching tiles, **Then** no thread calls into an unloaded module (repeated under a thread sanitizer) and the module is unloaded only after `finish` for that display has returned.
5. **Given** the header and the guide, **When** a driver author reads them, **Then** the reentrancy rules, the lifetime of the snapshot and descriptors, and the unload rule are stated, and every in-tree driver is written to them.
6a. **Given** a display for which a parameter lookup arrives with no current display (a legacy v1 driver calling the callback outside `start`, or `quantize`/`dither` with no display), **When** the lookup runs, **Then** it returns a documented not-found result and never dereferences a null display (FR-020, FR-039).
6b. **Given** a frame in which every display finishes normally, **When** `endDisplays` runs, **Then** each driver module is released after its `finish` has returned (FR-018, FR-035).
6. **Given** the in-tree driver modules (file for TIFF and PNG, OpenEXR, RGBE, framebuffer), **When** migrated, **Then** the decoded pixels of the display and AOV scenes are identical to the pre-change output.

---

### User Story 4 - One shadeop module, per-thread init, a stated threading contract (Priority: P2)

A shadeop author can ask for per-thread state: the renderer calls an optional per-thread init for each shading thread that uses the operand, passes that thread's data to its `exec` calls, and calls the matching per-thread cleanup. A shadeop that does not provide it behaves exactly as today (one init, one shared data pointer), and its documentation says `exec` may run on several threads at once. The renderer and the shader compiler both read shadeop declarations through the same shadeop module, which owns module scanning, the table lookup, the version check and the declaration parser (`dsoParse`).

**Why this priority**: It removes a data race that a shadeop can only avoid by luck, and it is the step V16 needs so the standalone compiler (043) can read prototypes through the versioned header alone.

**Independent Test**: Two threads call a test operand at once; a per-thread-aware operand sees distinct data per thread and an unaware one still gets the shared pointer. A shader compile and a render resolve the same operand through the same module.

**Acceptance Scenarios**:

1. **Given** a test shadeop that exports the per-thread entry points, **When** shaded on N threads, **Then** `init` runs once per participating thread, each `exec` receives its own thread's data, and the matching cleanup runs for every init before the module can be unloaded.
2. **Given** a legacy shadeop with only the process-wide init, **When** shaded on N threads, **Then** it receives the same single data pointer as today and its output is unchanged.
3. **Given** the shader compiler and the renderer, **When** each resolves a shadeop prototype, **Then** both go through the shadeop module and neither includes the Scene Interface's `dso.h` nor carries its own copy of the declaration parser, and the parser keeps no result in file-static state shared between callers (FR-012, FR-037).
4. **Given** a module table entry whose `exec` symbol does not resolve, **When** the module is scanned, **Then** it is not selected for that prototype, a diagnostic names the missing symbol, and the scan continues to a later module that has the operand; the test is on the resolved `exec[0]`, not on the output array's address (FR-013, FR-033).

---

### User Story 5 - One Geometric Procedures module owns the procedural loaders (Priority: P2)

The code that loads a DLO (`Geometry "dso"` style primitive), an implicit-surface module and a `RiProcDynamicLoad` generator lives in one module that performs the lookup, the version check, the symbol resolution and the load/unload lifetime. The renderer core, Geometry and the RI entry point call it and do not each repeat an `osLoadModule` sequence. The DLO loader loads the file the search located. A dynamic-load generator's module is released at a defined point instead of leaking.

**Why this priority**: The version check of US1 should exist once per family, not three times, and DB-014 names the scatter explicitly.

**Independent Test**: Load valid, MAJOR-mismatched, legacy and symbol-missing test modules of each of the three kinds through their normal RIB statements and assert the same version outcomes as US1; render the existing DLO, implicit and `RiProcDynamicLoad` scenes and compare.

**Acceptance Scenarios**:

1. **Given** the three kinds of procedural module, **When** each is loaded through its RIB statement, **Then** the version outcomes of US1 hold identically, produced by the same code.
2. **Given** a DLO named by a bare file name that the procedural search path locates, **When** it loads, **Then** the located file is the one loaded, not the unresolved name (FR-023, FR-032).
3. **Given** a `RiProcDynamicLoad` generator that has been expanded, **When** the frame ends, **Then** its module is released (the previously commented-out unload now happens at the defined point, FR-024, FR-034) and it is not released while its generated geometry can still call it.
4. **Given** the architecture check, **When** it runs, **Then** no allowlist line is added for the new module and the Scene Interface reaches the loader only through a route the allowed-dependency table permits.

---

### User Story 6 - The shared plug-in convention is defined once (Priority: P2)

An author adding a new plug-in type (a Bxdf, an integrator, a light, a filter, a pattern, a hider) copies one documented convention instead of inventing a boundary: a C function table that starts with its own `structSize` and a SemVer `abiVersion`, opaque handles for everything the plug-in or the host creates, defined ownership of every pointer that crosses, a status/error code set, a version query and registration entry point, and a C++ convenience header that wraps the table. The four existing families follow the convention where their legacy shape allows, and say where they cannot.

**Why this priority**: Specs 035, 041, 042, 043 and 051 build interfaces on it. It does not block the safety fixes.

**Independent Test**: A conformance test builds a minimal plug-in of a throwaway test type purely from the convention header and the C++ convenience header, with an older `structSize` and a newer one, and the host's common checking code accepts and rejects it correctly.

**Acceptance Scenarios**:

1. **Given** the convention document and header, **When** an author defines a table for a new plug-in type, **Then** the version encoding (`MAJOR<<16 | MINOR<<8 | PATCH`), the size field rule (the host accepts a table at least as large as the fields it knows and ignores the extra, and rejects a smaller one), the ownership rules and the error codes are all stated once.
2. **Given** a table whose `structSize` is smaller than the host's minimum, **When** registered, **Then** it is rejected with a diagnostic, not read past its end.
3. **Given** the C++ convenience header, **When** a plug-in is written with it, **Then** it compiles as C++20 and produces the same table a hand-written C plug-in would.

---

### User Story 7 - Texturing no longer reads the renderer's channel registry (Priority: P3)

The 3D-texture channel-list parser resolves channel names through an interface Texturing owns and the renderer core fills, instead of including the renderer core and the Graphics State channel type. It still works with no `RiBegin` session (point-cloud and brick-map tools use it that way).

**Why this priority**: It retires DB-021 and the rest of V18, but it is independent of the plug-in boundary and the smallest user-visible win.

**Independent Test**: The architecture check shows the three DB-021 edges gone; the point-cloud and 3D-texture tests, including the one that defines channels without a session, still pass.

**Acceptance Scenarios**:

1. **Given** the finished tree, **When** the architecture check runs, **Then** the three DB-021 allowlist lines are gone and none is added.
2. **Given** a 3D texture created with a channel list, **When** the channels are defined, **Then** the names, sample counts, types and fill values are what they were, with and without an open session, and an unknown name reports the same error.

---

### User Story 8 - `file_base.h` is no longer promised and the install documentation is correct (Priority: P3)

A third-party display author reading the install list and the guide is not told to use a helper that cannot link. `file_base.h` is no longer installed (`CFileOutputBase` stays an in-tree helper), and the installed headers, the install manifest, `INSTALL_ARTIFACTS.md` and `DISPLAY_PLUGIN_GUIDE.md` agree with one another and with the tree, with no stale paths.

**Why this priority**: Documentation and install hygiene; no runtime effect.

**Independent Test**: Install into a scratch prefix and compile the guide's example display against the installed headers only.

**Acceptance Scenarios**:

1. **Given** a scratch install, **When** the guide's example plug-in is built against the installed headers alone, **Then** it builds, loads and renders.
2. **Given** the install list and guides, **When** compared with the installed files, **Then** every header they name is installed and every installed header is named, `file_base.h` is in neither, and the guides' include paths and source paths exist in the tree (FR-026, FR-036, FR-038).

---

### Edge Cases

- **A plug-in advertises MAJOR 0.** Treated as an ordinary MAJOR: rejected unless the host's MAJOR is also 0 (SemVer's "anything may change" for 0.x is not modelled).
- **A plug-in exports the version symbol but it returns garbage** (an unset field, a MAJOR above any released): rejected as a MAJOR mismatch; the diagnostic prints the raw value.
- **A module is both a display driver and a shadeop** (exports symbols for several families). Each family's loader checks only its own version symbol.
- **One shadeop module holds many tables.** There is one version per module, not per table.
- **The same module is loaded twice** (two displays with the same driver, many DLO instances). The version is checked once per module per run, and the legacy warning is printed once per module per run.
- **A display driver is a legacy unversioned one** (FR-006): it loads as 1.0.0 through the confined v1 adapter, with the legacy warning.
- **A display driver declares an explicit 1.x**: same adapter, no legacy warning; a newer 1.x MINOR warns as for any family.
- **`findParameter`'s callers outside `displayStart`.** A legacy driver that calls the parameter callback after `start` returns: the outcome is defined (the lookup returns not-found and the adapter logs once), not a read of whichever display started last.
- **Zero-channel and depth-only displays.** The descriptor list is empty-safe and a `z`-only display gets one descriptor.
- **All displays disable themselves mid-frame.** Rendering stops as it does today; no module is unloaded until every in-flight call has returned.
- **Module unload at exit.** A module still referenced by live geometry (a DLO instance, an expanded procedural) is unloaded only after that geometry is destroyed.
- **Third-party plug-ins built with a compiler whose C++ ABI differs.** The boundary is C only; no C++ type crosses it.
- **`RiProcDynamicLoad` generators follow the RenderMan Interface's fixed entry points.** They carry no mandatory version symbol (see FR-009), and an absent symbol is normal, not a warning.
- **Windows.** Existing `_WINDOWS` branches in the headers are kept as they are; no new Windows support is promised or removed.

## Requirements *(mandatory)*

### Functional Requirements

**Versioning and checks (US1, US2)**

- **FR-001**: Each of `shadeop.h`, `dlo.h`, `implicit.h` and `dsply.h` MUST declare its interface version as SemVer 2.0.0 `MAJOR.MINOR.PATCH` macros, and MUST document the meaning of each part and a version history (every version change documented, per constitution rule 4). The current dlo, implicit and shadeop interfaces start at 1.0.0, except where FR-011 adds an optional entry point (MINOR). Display is 2.0.0; the display version history MUST also record that 1.x is the legacy interface and that the host still supports it through the v1 adapter (FR-006).
- **FR-002**: Each header MUST provide a macro that defines a C-linkage version query symbol returning the version the plug-in was compiled against, so a plug-in declares its version by including the header and invoking one macro. There is one version per module per family.
- **FR-003**: The host MUST read that symbol in every loader of the four families before calling any other entry point of the module. It MUST reject a MAJOR that it does not support (each family lists the MAJORs the host supports; for dlo, implicit and shadeop that is the host's own, for display it is 1 and 2) with a diagnostic that names the file, the interface, the plug-in's version and the host's version, and MUST NOT call the plug-in. It MUST load an equal-or-older MINOR silently and a newer MINOR with one warning naming both versions.
- **FR-004**: The check and the diagnostic MUST be produced by one shared routine used by every loader, not copied per loader.
- **FR-005**: The four headers MUST be includable from C17 and from C++20 in any combination, with all C++-only constructs guarded, and MUST declare the entry points so that a plug-in compiled with hidden-by-default symbol visibility still exports them. The `SHADEOP` macro and the exec function typedef MUST describe the same signature.
- **FR-006**: A plug-in that exports no version symbol MUST be loaded as 1.0.0 with one warning per module per run (the text of FR-007). Display v2 is the new display MAJOR; the host MUST keep a confined v1 adapter so display drivers at MAJOR 1 (unversioned or explicit 1.x) keep working, and so the host supports each display MAJOR it still lists (1 and 2). The adapter MUST be confined to one module and one place in the loader: v2 code paths know nothing of v1. Through the adapter a v1 driver MUST NOT be given pointers that alias renderer-core state (the adapter serves lookups from per-display, host-owned copies only; a third-party v1 driver that wrote through those pointers loses that effect, and this is intended) and MUST NOT depend on a host-wide "current display" (FR-014): the adapter binds the display for the duration of that driver's `displayStart` call only, and a lookup outside it returns not-found (FR-020). The host MUST reject a display MAJOR other than 1 or 2. The wording of constitution rule 4 that this relies on ("the host rejects a MAJOR it does not support") is recorded in the Assumptions; this spec does not edit the constitution.
- **FR-007**: An unversioned plug-in MUST be accepted as 1.0.0 with a warning, once per module per run, whose text names the release in which unversioned plug-ins will be rejected: **1.2.0** (this spec is expected to ship in 1.1.0). The same release name MUST be stated in `DISPLAY_PLUGIN_GUIDE.md`, the shadeop and procedurals guides and `NEWS.md`/`ChangeLog.md`. The rejection itself is implemented in that later release, not here: no requirement, task or test of this spec may depend on it, on a future release, or on the merge. The warning text takes the release name from one constant so the later change is a one-line edit.
- **FR-008**: Every in-tree plug-in MUST declare its version: the four display driver modules, and a test plug-in for each other family. An example DSO (`examples/dsotest`) MUST be updated to declare it.
- **FR-009**: `RiProcDynamicLoad` generators implement the RenderMan Interface's fixed entry points (`ConvertParameters`, `Subdivide`, `Free`), which this spec does not rename or version-gate. A generator MAY export the same kind of version query symbol; if present it is checked as in FR-003, and if absent it is accepted without a warning.

**Shared convention (US6)**

- **FR-010**: The spec MUST deliver one documented plug-in convention and a header implementing it: a C function table whose first fields are `structSize` and a SemVer `abiVersion` encoded `MAJOR<<16 | MINOR<<8 | PATCH`; the rule for a table larger or smaller than the host expects; opaque handles for objects either side creates; ownership and lifetime rules for every pointer that crosses (who allocates, who frees, how long it is valid); a status code set and the meaning of failure; a registration/version query entry point; and a C++ convenience header. The version check of FR-003 MUST be implemented on this convention's encoding. The convention covers *how* a table is shaped; the contents of the later plug-in types (Bxdf, integrator, light and the rest) are out of scope. No earlier copy of the convention exists in the tree at the base commit.

**Shadeop (US4)**

- **FR-011**: A shadeop module MAY export an optional per-thread init and its matching cleanup. When present, the host MUST call init once for each shading thread that executes the operand, pass that thread's data to `exec`, and call cleanup for every init before the module is unloaded; adding these optional entry points is a MINOR change. When absent, the operand MUST behave exactly as today (one process-wide init, one shared data pointer). The header and guide MUST state that `exec` may be called concurrently from several threads.
- **FR-012**: One shadeop module MUST own module scanning, the `<name>_shadeops` table lookup, the version check and the declaration parser (`dsoParse` and its grammar), and MUST expose declarations to the renderer and to the shader compiler. The compiler and the renderer MUST NOT include the Scene Interface's `dso.h`, and the declaration parser MUST NOT be compiled into more than one library. The parser's results MUST NOT live in file-static state shared between callers.
- **FR-013**: A table entry whose `exec` symbol cannot be resolved MUST NOT be selected for a prototype, MUST be diagnosed by symbol name, and MUST NOT stop the scan for a later module that has the operand.

**Display v2 (US3)**

- **FR-014**: The display interface MUST give every display a per-display context handle that the host passes to every entry point, and that identifies the display without any host-side "current display" variable. The host MUST NOT hold a file-static or other global "current display".
- **FR-015**: Each display MUST receive a `const` snapshot containing its name, resolution, total sample count and parameters: the RIB `Display` parameters (typed: float, vector, matrix, string, integer) and the renderer-provided values (near, far, the two matrices drivers use, the screen matrix, gamma, gain, quantization and dither with the depth/colour defaults resolved, the software string, the display type). Every value in the snapshot is a copy owned by the host for the display's lifetime; no pointer in the API gives write access to, or aliases, any renderer-core state.
- **FR-016**: Each display MUST receive channel descriptors: for every channel the display writes, its name, sample count, value type and offset within the per-pixel data it is sent, plus per-channel quantization when it differs from the display default. The descriptors MUST be consistent with the data actually dispatched, and replace the lossy channel string.
- **FR-017**: The header and guide MUST state the threading contract: `start` and `finish` are never concurrent with any other call for the same display and are called exactly once each; `data` may be called concurrently from several threads for disjoint tiles of the same display; no call follows `finish`; a false return from `data` disables the display. They MUST state the lifetime of the snapshot and descriptors (valid from `start` until `finish` returns).
- **FR-018**: A display module MUST be unloaded only after `finish` for every display using it has returned and no host thread is inside any of its entry points. A display disabled mid-frame keeps its module mapped until then. A module that is never disabled MUST also be released at a defined point rather than leaked.
- **FR-019**: The unused `displayRawData` entry point MUST be removed from display v2 (V21). The duplicate display typedefs in `ri.h` that exist only for the host-side cast MUST be replaced by one definition per interface; the RI-specified `RiCustomDisplay` callback types remain as the RenderMan Interface defines them, and the host converts between them and display v2 explicitly rather than by cast.
- **FR-020**: The host MUST NOT dereference an absent display when a parameter lookup arrives with none (the existing null dereference for `quantize`/`dither` disappears with FR-014).
- **FR-021**: The four in-tree driver modules MUST be migrated to display v2. Their output files MUST decode to pixels identical to the pre-change output for the display and AOV scenes, measured by exact comparison on deterministic scenes, not by the visual suite's tolerance metric.

**Geometric Procedures (US5)**

- **FR-022**: One Geometric Procedures module MUST own locating-and-loading, version checking, symbol resolution and release for dlo, implicit-surface and `RiProcDynamicLoad` modules, with the renderer core, Geometry and the RI entry point calling it. The existing per-site `osLoadModule`/`osResolve` sequences MUST be removed. The Scene Interface MUST reach the loader only by a route the constitution's allowed-dependency table permits (a registered hook or Geometry), not by a new direct dependency.
- **FR-023**: The DLO loader MUST load the file the search located. Error messages for a missing entry point or failed init keep naming the module as they do today.
- **FR-024**: A module's release MUST wait for every object that can still call it: a DLO or implicit instance holds its module until the instance is destroyed; a `RiProcDynamicLoad` generator's module is held until the geometry it generated is destroyed, and is released at frame end, after that.

**Texturing and install (US7, US8)**

- **FR-025**: The 3D-texture channel-list parsing MUST obtain channel descriptions through an interface Texturing owns, filled by the renderer core from the Graphics State channel registry, and MUST NOT include the renderer core or the Graphics State channel type. It MUST keep working with no active session.
- **FR-026**: `file_base.h` MUST no longer be installed. `CFileOutputBase` and the `openrenderfilebase` library stay an in-tree helper for the in-tree file drivers, and third-party authors are taught the C display interface by an example that does not need it. The installed headers, the install manifest, `INSTALL_ARTIFACTS.md` and `DISPLAY_PLUGIN_GUIDE.md` MUST agree: none names `file_base.h` as a third-party facility. This also resolves `file_base.h` including the uninstalled `common/os.h` (FR-038).

**Tests, documentation, registry**

- **FR-027**: A plug-in test set MUST exist for each of the four families, built from the tree's own build files: a matching-version plug-in, a different-MAJOR plug-in, a newer-MINOR plug-in, a legacy (no symbol) plug-in and, for the other conformance points, the hostile display driver, the two-display-parameter case, the per-thread shadeop pair and the late-disabling display. The tests MUST run through the project's test runner, under a label of their own.
- **FR-028**: Documentation MUST be updated: a version history per interface, the convention document, `DISPLAY_PLUGIN_GUIDE.md` (display v2, correct include paths), a shadeop and a procedurals guide section (or new guides), `INSTALL_ARTIFACTS.md` and `DEVNOTES.md`.
- **FR-029**: The registry MUST move DB-003 and DB-014 to Resolved with this spec's number and the date, delete the three DB-021 and the three DB-019 `dso.h` allowlist lines, update DB-019 (its JIT and standalone-compiler items remain for specs 038 and 043) and DB-024's display-ABI evidence, and add no allowlist line. New directories are classified in the architecture check's domain map.
- **FR-030**: No file outside the plug-in boundary changes behaviour: with no plug-in loaded, every existing scene renders as before; the visual and shading-parity suites, the framebuffer tests and the architecture check pass.

**Incidental defects found while specifying (fixed in this spec as the work reaches each one)**

- **FR-031**: The shadeop exec typedef and the `SHADEOP` macro MUST describe one signature (today `void *[]` versus `char *[]`). Covered by FR-005, US2 scenario 4.
- **FR-032**: The DLO loader MUST load the path the search located, not the unresolved name. Covered by FR-023, US5 scenario 2.
- **FR-033**: The shadeop table scan MUST test the resolved `exec[0]` entry, not whether the output array pointer is non-null. Covered by FR-013, US4 scenario 4.
- **FR-034**: `RiProcDynamicLoad` MUST release its module at the defined point (the commented-out unload is replaced by the lifetime rule of FR-024). Covered by US5 scenario 3.
- **FR-035**: `endDisplays` MUST release every working display module after its `finish` returns (today it never unloads). Covered by FR-018, US3 scenario 6b.
- **FR-036**: The stale paths in `DISPLAY_PLUGIN_GUIDE.md` (`ri/dsply.h`) and `INSTALL_ARTIFACTS.md` (`src/file/`) MUST be corrected to paths that exist. Covered by FR-028, US8 scenario 2.
- **FR-037**: `dsoParse` MUST be compiled into exactly one library and keep no result in file-static state shared between callers. Covered by FR-012, US4 scenario 3.
- **FR-038**: `file_base.h` MUST NOT include a header that is not installed; resolved by not installing `file_base.h` (FR-026).
- **FR-039**: `findParameter` MUST NOT dereference a possibly null current display for `quantize` and `dither`. Covered by FR-020, US3 scenario 6a.

Each of FR-031 to FR-039 MUST have a test or a mechanical check in the plug-in test set (FR-027) or the install check (SC-011); the tasks stage lists them.

### Key Entities

- **Interface version**: a `MAJOR.MINOR.PATCH` triple per plug-in family (and later per plug-in type), encoded in one 32-bit value, declared in the family's header and reported by a plug-in's version query symbol.
- **Version query symbol**: the C-linkage function every versioned module exports, defined by a header macro, read by the loader before any other call.
- **Function table (convention)**: a C struct that begins with its own size and version, holds the plug-in's entry points, and is the only thing that crosses the boundary for new plug-in types.
- **Display context**: the host's per-display handle passed to every display entry point; owns the snapshot and descriptors.
- **Parameter snapshot**: the immutable, host-owned copy of one display's parameters.
- **Channel descriptor**: name, sample count, value type, offset (and quantization when it differs) for one channel of one display.
- **Shadeop module**: the single owner of DSO scanning, declaration parsing and per-thread operand state.
- **Procedures module**: the single owner of dlo, implicit and `RiProcDynamicLoad` module loading, checking and release.

## Success Criteria *(mandatory)*

### Measurable Outcomes

- **SC-001**: For each of the four families, a test plug-in built with a MAJOR the host does not support is rejected at load with a diagnostic that contains the file name, the interface name and both version numbers, and the plug-in's code is never entered (verified by the test plug-in recording any call).
- **SC-002**: For each family, plug-ins at an equal or older MINOR load with no version message, at a newer MINOR load with exactly one warning, and with no version symbol load with exactly one legacy warning per module per run, across at least two loads of the same module.
- **SC-003**: All four headers compile without warnings as strict C17 and as C++20, individually and together, and a hidden-visibility plug-in of each family loads.
- **SC-004**: The decoded pixels of every output of the four in-tree display driver modules on the display and AOV scenes are identical to the pre-change outputs (zero differing pixels on deterministic scenes), and the framebuffer tests pass.
- **SC-005**: Two displays with different parameters started in one frame each see their own values in 100% of 100 randomized start orders, and a hostile display driver leaves renderer state and every other display's output unchanged.
- **SC-006**: A display that disables itself on its first tile, with eight or more threads dispatching, completes 1,000 start-dispatch-finish cycles without a crash, hang or call into unloaded code, and a thread-sanitizer run reports no race in the display, shadeop or procedural loading paths.
- **SC-007**: A per-thread-aware shadeop run on N threads receives N distinct data pointers and N matching cleanups; a legacy shadeop receives one pointer, unchanged output.
- **SC-008**: There is exactly one copy of the version-check code, one copy of the declaration parser and one module per family doing its loading; a count of `osLoadModule` call sites outside those modules and the display host loader is zero.
- **SC-009**: The architecture check passes with the three DB-019 `dso.h` edges and the three DB-021 edges gone, no line added, DB-003 and DB-014 under Resolved with this spec's number and the date, and `DEVNOTES_DETAILS/DOMAIN_BOUNDARIES.md` and the guides consistent with the tree.
- **SC-010**: A throwaway plug-in type written only from the convention header and the C++ convenience header is accepted when its table is current or larger and rejected, without reading past the table, when it is smaller or of a different MAJOR.
- **SC-011**: A display plug-in written from the guide's example builds against the headers installed in a scratch prefix and renders, the install list names exactly the headers installed, and `file_base.h` is neither installed nor named.
- **SC-013**: A legacy display driver (unversioned and explicit 1.x) built before this spec loads through the v1 adapter, renders pixels identical to the pre-change output, and cannot change renderer-core state; a display MAJOR other than 1 or 2 is rejected.
- **SC-014**: Each of the nine incidental defects (FR-031 to FR-039) has a passing test or mechanical check.
- **SC-012**: The full test suite (visual, shading parity, state, common, framebuffer, architecture) passes on macOS and Linux with no threshold widened and no test disabled.

## Assumptions

- **Constitution amendment needed (not made here)**: rule 4 of Principle VIII says the host "MUST reject a plug-in whose MAJOR version differs from its own". With the v1 display adapter the host supports two display MAJORs, so the rule as written would forbid it. This spec implements the reading "the host rejects a MAJOR it does not support" and records that the constitution should say so. Proposed wording: "The host MUST reject a plug-in whose MAJOR version is not one the host supports, with a clear diagnostic. A host MAY support more than one MAJOR of an interface, for example through a confined adapter for an older MAJOR; each supported MAJOR MUST be listed in that interface's version history, and dropping support for a MAJOR is a documented version change." Amending the constitution is the maintainer's separate decision (speckit-constitution); no requirement or task of this spec depends on it.
- **Legacy sunset**: the release named in FR-007 is a documented intention; the rejection is implemented in that release, not here.
- **Scope of "plug-in"**: this spec covers the four published families and defines the convention; run-time loading of the new RIS plug-in types, their registries and the constitution amendment that extends rule 4 to them are spec 051. The built-in registries of specs 030, 035 and 041 are not touched here.
- **Version numbers**: shadeop, dlo and implicit are 1.0.0 for the current ABI; the shadeop per-thread entry points are an optional addition (1.1.0). Display is a MAJOR change (2.0.0), and the host keeps supporting display 1.x through the v1 adapter. The exact constants are settled in the plan.
- **Encoding**: versions are encoded `MAJOR<<16 | MINOR<<8 | PATCH` (decision D7 and section 9 of the architecture reference), so MINOR and PATCH each have 8 bits.
- **Domains**: the shadeop module is classified as the Shader DSO Operands domain and the procedures module as the Geometric Procedures domain, each depending only on its own ABI header and the infrastructure floor; the display host loader stays in the renderer core (the composition root). Shading and Geometry may depend on them as the allowed-dependency table says; the Scene Interface may not depend on Procedures directly. Plan classifies new files in `tests/architecture/domain-map.txt`.
- **Display parameter lifetime**: the in-tree drivers read parameters only inside `start`; the snapshot nevertheless stays valid until `finish` returns so a driver may keep reading it.
- **Texturing (US7)** is kept in this spec because the plan (V18) assigns the descriptor half of DB-021 to 037. If the maintainer prefers it split off, US7 and FR-025 are independent and can move without touching the rest.
- **Test modules** are built by the tree's own build files as ordinary module targets next to the in-tree drivers; no build or test is run during the specify stage.
- **Merge-ordering information, not a dependency**: spec 028 (camera and motion layer) is in progress in a sibling worktree. It edits `src/ri/geometry/dlobject.cpp`, `implicitSurface.cpp`, `quadrics.cpp`, `patches.cpp`, `polygons.cpp` and `surface.cpp`, and (as of this writing) also `src/ri/render/rendererContext.cpp`. Spec 037 moves the loaders out of `implicitSurface.cpp` (constructor), `rendererContext.cpp` (DLO) and `parse/ri.cpp`, and so overlaps 028 in the first two files; 028 deliberately leaves `dlo.h` and `implicit.h` unchanged, so the headers do not overlap. Nothing in this spec requires 028; whichever lands second resolves a textual conflict in those loader sites.
- **Dependencies**: spec 023 (merged: the shared infrastructure floor, diagnostics). Specs 022, 024, 025 and 027 are merged and unaffected. This spec must land before 043 (the shader compiler library reads shadeop prototypes through the versioned `shadeop.h`) and is read by 038 and 051.
- **Out of scope**: the JIT's DSO call path (038), the standalone compiler and its builtin tables (043), run-time RIS plug-in loading (051), Windows support, and any change to the RenderMan Interface's own `RiCustomDisplay` and `RiProcDynamicLoad` contracts.
