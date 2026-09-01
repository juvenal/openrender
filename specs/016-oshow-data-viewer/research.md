# Phase 0 Research: orender-wire Data-Structure Viewer (oshow Absorption)

All unknowns below were resolved during pre-spec investigation (three parallel exploration
passes plus a dedicated design pass) and verified again against source during this planning
pass. None remain open for Phase 1.

## 1. Replacing `CView`/`dlopen`/`TGl*` with a sink

**Decision**: Introduce `src/ri/dataView.h`/`.cpp` with two types: `class CPrimitiveSink`
(pure-virtual: `triangles`, `triangleMesh`, `lines`, `points`, `disks`) and `class CDataView`
(replaces `CView`). `CDataView` keeps the exact same six static drawing entry points
(`drawTriangles`, `drawTriangleMesh`, `drawLines`, `drawPoints`, `drawDisks`, `drawFile`) and the
same `chunkSize = 128 * 3` constant, forwarding to an installed `CPrimitiveSink*` instead of a
`dlopen`ed function pointer. `src/gui/` (one file, `opengl.h`, never `add_subdirectory`'d) is
deleted.

**Rationale**: The eight `draw()` bodies across `src/ri/{pointCloud,brickmap(×2),photonMap,
irradiance,radiance(×2),debug}.cpp` call `drawDisks(chunkSize, P, dP, N, C)` etc. unqualified
and use `chunkSize` as an array bound. Keeping identical names/signatures/constant means those
bodies recompile with **zero diff lines** outside the new base class — verified by reading each
call site (`brickmap.cpp:1208,1231`, `pointCloud.cpp:367,369`, `photonMap.cpp:567`,
`irradiance.cpp:1103-1133`, `radiance.cpp:1371,1393`, `debug.cpp:101`). Static forwarding
(rather than per-instance virtuals) preserves the existing process-global semantics exactly and
avoids widening `CTexture3d`/`CPhotonMap`, which are multiply-inherited and allocated on the hot
render path.

**Alternatives considered**: An instance-based `virtual` sink on `CDataView` itself — rejected,
it would require every `draw()` call site to route through `this->`, which is more diff than
the static-forwarder approach for no behavioral benefit (visualization is inherently
single-threaded; `CShow` only ever ran on thread 0). A single monolithic `emit(kind, n, ...)`
call — rejected, it loses the compile-time signature checking the six distinct entry points
give, and doesn't match the existing call sites without editing them.

**Two bugs fixed while porting the debug-dump parser** (recovered via
`git show 33506f4^:src/gui/opengl.cpp`, deleted alongside the rest of `src/gui/`'s
implementation):
- The disc basis `X = P × N` (seeded from the disc's *world position*) is the zero vector — and
  `normalizevf` produces NaN — whenever `P ∥ N`, which is always true for a sample at the
  origin. Fixed by picking a basis from a fixed world axis chosen to be least parallel to `N`,
  never from `P`.
- `pglFile`'s `while (!feof(file))` loop re-emits the last record, because `feof` only becomes
  true *after* a failed read, so the stale tag variable is processed again. Fixed to
  `while (fread(&tag, sizeof(int), 1, f) == 1)`.
- The dump format's quad tag (3) has no direct equivalent in GL 3.3 core or Metal (no
  `GL_QUADS`); the parser now splits each quad into two triangles (`0,1,2` and `0,2,3`) at
  parse time, which is the only place the quad tag is interpreted.

## 2. Loading a data file outside a render (headless)

**Decision**: Do not call `CRenderer::getPhotonMap`/`getCache`/`getTexture3d`. Add
`src/ri/dataLoad.h`/`.cpp` with `dataSniff()` (ports the magic-number/version/word-size
validation at `src/ri/show.cpp:82-99` verbatim) and `class CDataDocument` whose `open()`:
1. Calls `CRenderer::initDeclarations()` and `memoryInit(CRenderer::globalMemory)` — the same
   bracket `ribpreview_load` already uses (`previewContext.cpp:150-151`).
2. Sets `CRenderer::fromWorld`, `toWorld`, `fromWorld1`, `toWorld1`, `fromNDC`, `toNDC` to
   identity, and `worldBmin`/`worldBmax` to ±infinity, **before** constructing any reader.
3. Dispatches on the sniffed type string and constructs the reader directly with identity
   `from`/`to` matrices: `new CPhotonMap(name, in)`, `new CIrradianceCache(name,
   CACHE_READ|CACHE_RDONLY, in, from, to, NULL)`, `new CPointCloud(name, from, to, in)`,
   `new CBrickMap(in, name, from, to)`, or `new CDebugView(in, name)`.
4. Owns whichever `CDataView*` it constructs and deletes it uniformly in `~CDataDocument()`.

`dataSniff()`'s result type (referenced by `contracts/c-abi.md` as `ribdata_open`'s `*err`
output) is:

```cpp
enum EDataFileType {
    DATA_UNKNOWN = 0,          // sniff not yet attempted / no file open
    DATA_NOT_A_DATA_FILE,      // no magic number and not a parseable debug dump — try RIB instead
    DATA_PHOTONMAP,
    DATA_IRRADIANCECACHE,
    DATA_GATHERCACHE,
    DATA_POINTCLOUD,
    DATA_BRICKMAP,
    DATA_DEBUGDUMP,
    DATA_BAD_VERSION,          // magic matched; VERSION_MAJOR/MINOR mismatch (show.cpp:92-93)
    DATA_BAD_WORDSIZE,         // magic matched; sizeof(int*) mismatch (show.cpp:95-96)
};
```

No `DATA_UNSUPPORTED`/hierarchical entry exists in this enum — see the note below.

**Confirmed during `/speckit-analyze` (2026-09-01) — a variant that does *not* exist in this
enum**: pre-spec design work assumed a "recognized but unsupported" data-file variant
(`CPointHierarchy`, whose `draw()`/`bound()` are empty stubs) reachable through this same
content-based dispatch. Verified against source: `CPointHierarchy` is constructed only when a
caller passes `hierarchy = TRUE` to `CRenderer::getTexture3d` (default `FALSE`,
`src/ri/renderer.h:292`); the only such caller is the RSL shading-services bridge
(`src/ri/rendererServicesImpl.h:98` → `src/libshader/shading/shading.cpp:554`), invoked by a
shader's runtime request during rendering — never by anything present in the file. Its
constructor (`pointHierarchy.h:56`) reads the identical on-disk format as `CPointCloud`; no
magic number, type string, or header field distinguishes it. Even `show.cpp` (the code this
feature replaces) always calls `getTexture3d(fileName, FALSE, NULL, from, to)` for both point
clouds and brick maps (`show.cpp:120,122`), so the original tool never reached this branch
either. **Conclusion**: this variant cannot be selected by `dataSniff()` or `CDataDocument::
open()` as designed, under any file content. It has been removed from scope entirely (spec.md's
retired FR-010; no corresponding enumerator here) rather than modeled as an unreachable branch.

**Rationale**: `CRenderer::getPhotonMap`/`getCache`/`getTexture3d` all
`assert(frameFiles != NULL)` (`rendererFiles.cpp:490,522,650`); `frameFiles` is created inside
`CRenderer::beginFrame`, which `RiBeginLite()` (used by the existing RIB-loading path,
`previewContext.cpp:149-162`) never reaches. Reproducing all of `beginFrame` headlessly (a
`COptions`, a `CAttributes`, a `CXform`, shading-services init, memory checkpointing, a matching
`endFrame`) is far more surface than this feature needs and drags in display/network state.
Constructing readers directly also **removes an existing ownership asymmetry**: today
`CRenderer::frameFiles` owns five of the six view types and only `CDebugView` is caller-owned
(`show.cpp:132-136` is the only `delete view`); `CDataDocument` owns all six uniformly, so its
destructor is a plain `delete`.

Step 2 is necessary because `CRenderer::fromWorld`/`toWorld`/`worldBmin`/`worldBmax` are
file-scope statics documented as "initialized in beginFrame" (`renderer.cpp:196,204,206`) and
are read on the load path — `photonMap.cpp:103-104` does
`mulmm(to, fromWorld, CRenderer::toWorld)` immediately after reading a photon map's stored
matrices, and `irradiance.cpp:101` reads `CRenderer::worldBmin`/`worldBmax` on the fresh-cache
branch. Without step 2, photon maps and caches load with degenerate (zero) transforms — a
landmine that would otherwise present as a mysterious sink/rendering bug. `test_data_world_init`
(Phase 1 contract) pins this.

**Confirmed bug fixed on the same pass**: `map.h:168` stores items in a 1-indexed array
(`&items[++numItems]`, index 0 unused). `CPointCloud::draw` correctly iterates `numItems` times
from `items + 1` (`pointCloud.cpp:364`). `CPhotonMap::draw` iterates `numItems - 1` times from
the same starting point (`photonMap.cpp:565`) — **one short**, silently dropping the last
photon on every visualization. Fixed to match `CPointCloud`'s loop bound;
`test_dataview_chunking` pins the corrected boundary behavior.

**One dependency ruled out empirically rather than by reading**: whether
`CTexture3d::retrieveDisplayChannel` (`texture3d.cpp:119`) is reachable on the read-only
point-cloud load path (as opposed to only on a write/declare path) could not be fully resolved
by static reading alone. `test_data_pointcloud` (Phase 1 contract) exercises exactly this path
and settles it; if it is reachable, `CDataDocument::open()` gains a minimal display-channel
stub as a small follow-up within the same task.

**Alternatives considered**: Reproducing `beginFrame`/`endFrame` in full — rejected, far more
surface than needed and reintroduces display/network coupling this feature is explicitly
removing. Patching `CRenderer::get*` to tolerate a null `frameFiles` — rejected, that code path
is shared with the full renderer and any behavior change there is out of this feature's blast
radius.

## 3. Brick-map test fixture strategy

**Decision (resolved during this planning pass — prior design notes had this open)**:
Brick maps **do** have an in-tree write path: `CBrickMap(const char *name, const float *bmin,
const float *bmax, const float *from, const float *to, const float *toNDC, CChannel *channels,
int numChannels, int maxDepth)` (`brickmap.h:97`) constructs a writable map (`modifying = TRUE`,
`brickmap.cpp:261`); `void store(const float *data, const float *P, const float *N, float dP)`
(`brickmap.h:204`) inserts voxels; `~CBrickMap()` calls `flushBrickMap(TRUE)`
(`brickmap.cpp:300`), which writes every resident brick's voxel data to `file`
(`brickmap.cpp:1442-1520`). This is the same shape as `CPointCloud`'s write-then-flush pattern,
already used successfully for that fixture. The exact glue (how `file` is opened/assigned for a
fresh write instance) is a small, self-contained detail resolved at task-authoring time — it
does not require a render, a committed binary, or deferral to the manual checklist.

**Rationale**: Removes what would otherwise be the one test-coverage gap among the six data
types. All six now generate fixtures in-process, inside a test's timeout, with no renderer,
shader compilation, or `ORENDERHOME` involved.

**Alternatives considered**: A committed `<100 KB` binary fixture — rejected now that a writer
is confirmed to exist; a binary fixture would also silently go stale across future on-disk
format version bumps. A full `bake3d`-driven render to produce a real brick map — rejected as
disproportionate: it drags the full shading engine and shader compilation into what is
fundamentally a file-format test.

## 4. C ABI shape

**Decision**: Extend `src/preview/ribpreview_api.h` only — no fourth duplicate header. Add
`PrimArrayC { float *verts; float *cols; int count; }`, `DataSceneC` (four `PrimArrayC`-shaped
groups — lines, points, triangles-including-CPU-expanded-discs — plus `sourceDiskCount`,
`decimatedCount`, bounds, a synthesized framing camera, `documentType`, and the interactive
state accessors), and an **opaque handle** API: `ribdata_sniff`, `ribdata_open`,
`ribdata_snapshot`, `ribdata_key`, `ribdata_channel_name`, `ribdata_close`. `ribpreview_load`/
`ribpreview_free` (the existing RIB-path value-pair API) are untouched.

**Rationale**: The `keyDown`/re-emit interaction cycle (channel/detail/draw-mode changes) needs
the underlying `CDataView` to survive across frames so a key event can call `keyDown()` and
re-draw into a fresh sink — a value-struct API would require reconstructing the whole document
on every keystroke. An opaque handle avoids that without complicating the RIB path, which has
no equivalent interaction cycle and stays exactly as it is.

This also **collapses an existing duplication**: `orender-wire-macos/CRibPreview/include/
CRibPreview.h` is currently a hand-maintained duplicate of `ribpreview_api.h` that additionally
declares `ribcam_write`/`ribcam_replace` (absent from `ribpreview_api.h`), while the Linux side
includes `cameraExport.h` directly. `CRibPreview.h` reduces to two `#include`s
(`ribpreview_api.h`, `cameraExport.h`) so every platform consumes the same declarations.

**SPM cross-target header risk**: adding `headerSearchPath` entries to the `CRibPreview` SPM
target so it can see headers outside its own `publicHeadersPath` must be proven in the first
implementation increment (I0), before the C ABI itself is extended in I3. **Fallback if SPM
rejects it**: a CMake `configure_file`/`copy_if_different` step in
`orender-wire-macos/CMakeLists.txt` that stages `ribpreview_api.h` and `cameraExport.h` into
`CRibPreview/include/` at build time, with the staged copies gitignored — same single-source-of-
truth guarantee, enforced mechanically instead of by SPM search paths.

**Alternatives considered**: A value-returning snapshot API with no handle — rejected per the
re-emit-cycle argument above. Keeping three header copies and just adding a fourth for the new
struct — rejected, it compounds a duplication already flagged as a drift risk (spec 006's
bridging-header plan never matched what was actually built).

## 5. Disc expansion

**Decision**: CPU-side, in `libribpreview`, 20 segments / 60 vertices per disc, matching the
deleted `pglDisks` geometry exactly except for the NaN-safe basis fix from §1. Add
`src/preview/libribpreview/diskExpand.h`/`.cpp`; expanded triangles are appended into
`DataScene`/`DataSceneC`'s triangle arrays, while the pre-expansion `{P, N, dP, C}` disc list is
kept separately for JSON output and tests.

**Rationale**: One vertex format (`float3` position + `float3` color) flows into both
renderers, matching the existing scene-buffer layout in both `WireframeRenderer.swift` and
`orender-wire-linux/main.cpp` — no new vertex layout or geometry-shader/instancing path needed
on either platform. It also makes disc correctness assertable headlessly: exactly 60 vertices
per disc, every rim vertex within `dP + ε` of `P`, every triangle's plane normal parallel to
`N`, and — the regression this fixes — no NaN when `P` is at the origin or `P ∥ N`.

**Alternatives considered**: GPU instancing/geometry-shader expansion — rejected, it would add
a second vertex format per platform and move disc correctness out of reach of a headless test
(it could only be verified by rendering and reading back pixels).

## 6. Large-file detail reduction

**Decision**: A fixed, deterministic maximum primitive count with even-stride sampling (per
`spec.md` clarification), applied in `CDataSceneSink` at scene-build time. When a document
exceeds the cap, `decimatedCount` in `DataSceneC` (and the `--json` output) reports how many
primitives were dropped.

**Rationale**: Deterministic behavior means the same file always reduces identically regardless
of machine, which is both what was decided during spec clarification and directly testable
(`SC-006`). It mirrors an idiom already in the codebase: `tessPoints.cpp`'s existing
`MAX_POINTS = 100000` cap with stride subsampling for RIB `Points` primitives — this feature
applies the same idiom to the data-document path instead of inventing a new one.

**Alternatives considered**: An adaptive, frame-time-tuned target — rejected during
clarification precisely because it is not reproducible across machines and is harder to test.

## 7. Renderer changes (both platforms)

**Decision**: Add three more pipelines (points, triangles, discs — discs arrive pre-expanded to
triangles from §5) alongside the existing line pipeline, in both `WireframeRenderer.swift`'s
inline Metal shader source (`buildPipelines`) and `orender-wire-linux/main.cpp`'s GLSL 330 raw
strings. Points require `[[point_size]]` (Metal) / `gl_PointSize` + `glEnable
(GL_PROGRAM_POINT_SIZE)` (GL) since point size is otherwise undefined. Culling is explicitly
disabled on both (`setCullMode(.none)` / `glDisable(GL_CULL_FACE)`) because discs are
single-sided triangle fans. Draw order: triangles → lines → points, on top of the existing
grid/axis pass.

**Rationale**: `Sources/Shaders.metal` is dead code — `Package.swift` already `exclude`s it in
favor of a runtime-compiled inline string, and the two have already drifted (the excluded file
lacks the color attribute the live string has) — so it is deleted rather than maintained as a
second copy. Reusing the existing line/triangle vertex layout for all three new pipelines keeps
one buffer format throughout, consistent with the disc-expansion decision in §5.

**GL depth-range fix, done now rather than deferred**:
`ribGeometryContext.cpp:337` builds `projMatrix` row-major for Metal NDC (`z ∈ [0,1]`) and ships
it unmodified to OpenGL, which expects `z ∈ [-1,1]` — so GL has been using half its depth range
since spec 006 shipped. This was cosmetic with lines only; with triangles and discs over brick
maps it becomes visible z-fighting, so it is fixed in this feature rather than carried forward
as separate debt. GL 3.3 core has no `glClipControl` (that's GL 4.5 / `ARB_clip_control`), so
the correction (`clip.z = 2·z_metal − w`) is applied on the CPU in the Linux projection-upload
path, in the same change that touches `arcball.cpp`'s `updateAspect` — which is also reconciled
with `ArcballCamera.swift`'s orthographic handling in the same pass, since both already diverge
and the file is already open.

**Alternatives considered**: Leaving lines-only depth precision as-is and shipping discs anyway
— rejected once triangles/discs make the precision loss visually obvious; better to fix the
root cause than add a workaround per new primitive type.

## 8. macOS shell: AppKit → SwiftUI

**Decision**: `main.swift` keeps every line of its current logic — argument parsing, the
`ORENDER_WIRE_GUI` re-exec terminal-detach block, and exit codes 0–3 — and gains exactly one
trailing `OrenderWireApp.main()` call. New files: `OrenderWireApp.swift` (a `SwiftUI.App` with
`Commands` providing the menu bar), `DocumentView.swift` (`NSViewRepresentable` wrapping the
existing `WireframeRenderer` `MTKView`, itself unchanged in its rendering internals),
`ViewerModel.swift` (an `ObservableObject` bridging menu/keyboard actions to the `ribdata_*`/
`ribpreview_*` C ABI). `AppDelegate.swift` and the dead `Sources/Shaders.metal` are deleted.

**Rationale**: SPM forbids `@main` inside a file literally named `main.swift`, and forbids
top-level statements in any other file — which is exactly what makes appending
`OrenderWireApp.main()` to the *end* of the existing `main.swift` safe: every existing behavior
runs first, unmodified, and only then hands off to the SwiftUI app lifecycle. The `--json`
headless branch must execute and `exit()` **before** the `ORENDER_WIRE_GUI` re-exec check —
otherwise headless mode would detach into a background GUI process and a test harness would see
exit 0 with no output.

**Known regression risk, with an explicit mitigation carried into the contracts**:
`WireframeRenderer` currently receives keyboard events only because `AppDelegate` makes it the
window's `contentView` (`AppDelegate.swift:96-101`). A SwiftUI-hosted `NSViewRepresentable` is
frequently *not* first responder, so every keyboard shortcut could silently stop firing while
the app still renders correctly and passes a casual smoke test. Mitigated two ways: (1)
`updateNSView` calls `makeFirstResponder` whenever the view isn't already first responder; (2)
every shortcut is additionally routed through `Commands` with `.keyboardShortcut`, so menu
items work regardless of view focus. `.onKeyPress` is explicitly avoided — it requires macOS 14
and would silently raise the current `.macOS(.v12)` deployment floor.

**Alternatives considered**: Keeping `AppDelegate`/`NSMenu` and only using SwiftUI/
`NSHostingView` for auxiliary panels — rejected, it does not produce a real `Commands`-driven
menu bar and keeps the app on the AppKit lifecycle the spec asks to leave behind. Bumping the
deployment target to use `.onKeyPress`/`@Observable`/`Window` — rejected as an unrequested scope
increase; deferred to a future spec if a macOS-version bump is ever independently justified.

## 9. Linux shell: populating the header bar

**Decision**: Fill the currently empty `AdwHeaderBar` (`adw_header_bar_new()`,
`main.cpp:451-484`) with a `GtkMenuButton` bound to a `GMenuModel` whose entries are
`GSimpleAction`s shared with the existing `on_key` handler (one action implementation, two entry
points: menu and keyboard), plus an `AdwWindowTitle` subtitle showing document type / channel /
detail level / draw mode — replacing the `printf`s this feature removes. **No `AdwViewStack`**:
both document types share the same `GtkGLArea`, camera, and rendering pipelines, so a stack
would add structure around a single page of content; instead, header-bar controls are shown or
hidden based on which document type is open (channel/detail controls hidden for a RIB document;
Save Camera hidden for a data document).

**Rationale**: Sharing the action implementation between the menu and the keyboard handler is
what keeps the two entry points from drifting apart, and matches the "same effect either way"
requirement from the spec's acceptance scenarios.

**Also decided while this file is open**: soften the four hard `REQUIRED` CMake probes
(`OpenGL`, `gtk4>=4.20`, `libadwaita-1>=1.4`, `epoxy`) to warn-and-`return()`, mirroring the
existing `swift not found` pattern on the macOS side (`orender-wire-macos/CMakeLists.txt:11-14`).
A hard configure failure on a machine without GTK 4.20 is an existing violation of the
constitution's graceful-degradation clause for optional dependencies (Principle V); this feature
fixes it rather than adding a matching new violation to the file it is already touching.

**Alternatives considered**: A `GtkPopoverMenu` triggered from a plain button instead of a
`GMenuModel` — rejected, `GMenuModel` is the standard libadwaita/GNOME HIG pattern and composes
directly with `GSimpleAction`, avoiding a second, ad hoc dispatch mechanism.

## 10. CLI grammar and exit codes

**Decision**: `--help`, `--version`, and `--json` (headless statistics; no window opened; no
`ORENDERHOME`/`SHADERS`/`DISPLAYS` required, matching the existing RIB-loading path). Exit codes
0–3 keep their current meanings (0 success; 1 usage error; 2 file not found/unreadable; 3 RIB
parse failed). New: 4 = data file rejected (bad type string, version mismatch, or word-size
mismatch — the three checks at `show.cpp:88-95`); 5 = graphics initialization failed (GUI paths
only, never reachable under `--json`).

**Rationale**: This is what closes an existing Constitution Principle IV gap — `orender-wire`
currently has zero command-line options beyond a single positional file argument, and per its
own CLI contract "no other flags are accepted in v1." Extending exit codes rather than replacing
them preserves every script or test that already depends on 0–3.

**Alternatives considered**: A single generic non-zero exit code for all failure modes —
rejected, it would make `test_wire_cli`'s per-failure-mode assertions (and any future scripting
against this tool) unable to distinguish "bad arguments" from "corrupt data file" from
"renderer unavailable."

## 11. TDD strategy for a GUI-adjacent feature (Constitution Principle III)

**Decision**: Follow the spec 006 precedent — unit-test the platform-neutral core exhaustively
(`tests/preview/`, `LABELS "preview"`), and validate the GUI phases (I0, I5, I6) through a
manual `quickstart.md` checklist rather than new automated GUI tests. What is new relative to
006: the headless `--json` mode (§10) gives this feature a machine-checkable proof of the
*entire data path* (sniffing, loading, primitive generation, interactive state) before either
platform's GUI code is touched at all (I3/I4 precede I5/I6), which is strictly more coverage
than 006 had for its equivalent core.

**Rationale**: The constitution's TDD gate is non-negotiable for testable logic; GPU rendering
and native window chrome are not economically unit-testable in this codebase's existing test
infrastructure (no GUI test harness exists for either platform), so the established, already-
approved pattern is reused rather than inventing a new one for this feature alone.

**Alternatives considered**: Building a screenshot-diff or GPU-readback test harness for the
renderer phases — rejected as disproportionate scope for this feature; it would be a
significant new testing capability for the whole `orender-wire` app, not something this feature
specifically needs, and nothing in spec 006 built one either.

## 12. Removal verification gate

**Decision**: After the removal phase (I2), `grep -rn "CShow|oshow|BUILD_SHOW|FLTK"` from the
repository root must return zero hits outside `specs/` and `ChangeLog.md`.

**Rationale**: Gives the removal phase — which touches over twenty files across build config,
source, tests, and documentation — a single, mechanically checkable completion signal, matching
FR-026's requirement that no trace of the legacy tool or its build dependency remains.
