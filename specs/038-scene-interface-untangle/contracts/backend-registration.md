# Contract: back-end registration and the preview session (FR-006, FR-007, FR-012-FR-014)

Research R5, R8.

## `riHooks.h` moves to `src/ri/parse/riHooks.h`

`git mv src/ri/render/riHooks.h src/ri/parse/riHooks.h`. Unchanged: `RiSetContextFactory`,
`RiRegisterDefaultContextFactory`, `RiRegisterRibOutFactory` (names, signatures, behaviour, the
"must be called before RiBegin, not thread-safe" note). The header comment keeps saying that the
registrations let a consumer link one back-end without the others, and names its new location.

Added:

```cpp
// A named Scene Interface back-end (spec 038), registered once by a static initializer in the
// back-end's own translation unit. The argument is passed through from RiBeginBackEnd unchanged.
using TRiBackEndFactory = CRiInterface *(*)(void *argument);
void RiRegisterBackEnd(const char *key, TRiBackEndFactory factory);
```

Includers after the move: `parse/ri.cpp`, `output/ribOut.cpp`, `render/rendererContext.cpp`,
`preview/libribpreview/previewContext.cpp` (new), and the five tests (`csgTestUtils.h`,
`test_surface_params.cpp`, `test_frame_params_equal_core.cpp`, `scene_probe.h`, `replay_capture.h`),
which include it by bare name.

## The light session (`src/ri/parse/rib.h`)

```cpp
// Begin a session with the back-end registered under key, without RiBegin's full start (no
// .orenderrc, no network, no frame range): a CRiStandardSession (scene-services.md), RiInit, the
// factory's context, riInstallContext. Returns the context, or nullptr after
// error(CODE_INCAPABLE, "Back-end \"%s\" is not available in this build (none registered)\n", key).
// One light session at a time; RiBegin's "Already started" rule applies (an active context refuses).
CRiInterface *RiBeginBackEnd(const char *key, void *argument);

// End it: delete the context (which uninstalls it), end the standard session. Like today's preview,
// it does not run RiTini.
void RiEndBackEnd(CRiInterface *context);
```

`RiBeginLite()` (`ri.cpp:255-257`, `rib.h:32`), whose only caller is the preview, is replaced by
`RiBeginBackEnd` and deleted. `ri.cpp` names no back-end; the key is the preview's string.

## The preview

`previewContext.cpp` (the translation unit of `ribpreview_load`, so a program that calls
`ribpreview_load` links the registration even from the static `libribpreview`):

```cpp
namespace {
    CRiInterface *makePreviewContext(void *) { return new CPreviewContext(); }
    struct CPreviewRegistrar { CPreviewRegistrar() { RiRegisterBackEnd("preview", &makePreviewContext); } };
    const CPreviewRegistrar g_previewRegistrar;
}

PreviewSceneC *ribpreview_load(const char *ribPath) {
    globalArena.init();
    auto *ctx = static_cast<CPreviewContext *>(RiBeginBackEnd("preview", nullptr));
    ribParse(ribPath, nullptr);
    PreviewScene scene = std::move(ctx->result_);
    RiEndBackEnd(ctx);
    globalArena.tini();
    ...  // unchanged packing
}
```

The order of `globalArena.init()` against the declarations changes (today declarations first, then
the arena); neither touches the other. `previewContext.cpp` and `ribGeometryContext.cpp` lose their
`renderer.h`/`rendererContext.h` includes and `riDiagnostics.h`; `libribpreview` names no `CRenderer`
or `CRendererContext` symbol (checked by `nm` on its objects).

## Checks

- `Preview_SelfRegistration`: a program linking `libribpreview` alone gets a non-null context from
  `RiBeginBackEnd("preview", nullptr)` that is the active one; `RiEndBackEnd` leaves no active
  context and the previous diagnostic sink in place; `RiBeginBackEnd("nonexistent", nullptr)` prints
  the diagnostic and returns `nullptr`. Registered `DISABLED` until the API exists.
- `Preview_NoCoreReach` (`cmake -P`): no `libribpreview` source includes a Core header and no
  `libribpreview` object has an undefined `CRenderer`/`CRendererContext` symbol. Red on the starting
  tree.
- The preview suite (`tests/preview`) and a characterization of `ribpreview_load` over the preview
  scenes (vertices, colors, camera, bounds) captured before the change: identical after (SC-006).
- FR-014: with no back-end linked for a mode, `RiBegin` prints the existing
  "Full-render mode is not available in this build ..." / "RIB-output mode is not available in this
  build ..." texts (`ri.cpp:315,324`), unchanged.
