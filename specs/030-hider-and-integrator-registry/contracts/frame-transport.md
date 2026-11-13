# Contract: the core's per-frame transport slot and the refused frame

**Status**: refused-frame design approved as written by the maintainer (Q1, 2026-11-07).

**Domain**: renderer core (`src/ri/render/rendererTransport.{h,cpp}`, `renderer.cpp`,
`rendererDisplay.cpp`, `rendererServicesImpl.h`). **Research**: R4, R5, R8, R14.
**Requirements**: FR-005, FR-007, FR-008, FR-012, FR-016, FR-022, FR-024.

## The slot

```cpp
struct SHiderTraits {
    bool rasterization = false;     // resolved hider provides "rasterization"
    bool displayOutput = false;     // resolved hider provides "display-output"
    bool displayChannels = false;   // resolved hider provides "display-channels"
    bool illuminationHook = false;  // resolved hider provides "illumination-hook"
    bool deepShadowOutput = false;  // the NAMED hider provides "deep-shadow-output" (false for an unknown name)
};

struct SFrameTransport {
    const OrHiderTable *named = nullptr;          // registration of CRenderer::hider as written; nullptr if unknown
    const OrHiderTable *hider = nullptr;          // resolved (named, or the reyes fallback)
    const OrIntegratorTable *integratorNamed = nullptr;
    const OrIntegratorTable *integrator = nullptr;// resolved (named, or classic)
    OrIntegratorInstance *integratorInstance = nullptr;
    bool prepared = false;                        // prepare returned OK: finish is due
    bool refused = false;                         // detected before displays: nothing built, no output file
    bool aborted = false;                         // detected after displays (create NULL, prepare non-OK): no thread runs
    SHiderTraits traits;                          // all false on a refused frame
    CHiderPeers peers;                            // { contexts, numThreads }: owned by the slot (never a local), set by setFramePeers() right after contexts is allocated and before the construction loop; valid until endFrameTransport()
};
// renderFrame() runs only when !refused && !aborted.

const SFrameTransport &frameTransport();          // read-only; a default value outside a frame
// Core-only (called by CRenderer):
void beginFrameTransport(const COptions *o);      // resolve, check, create + configure, traits
void setFramePeers(CHider **contexts, int numThreads);   // stores the peer group in the slot (before the construction loop)
void prepareFrameTransport();                     // prepare (the peers were stored by setFramePeers); non-OK: error, abortFrameTransport()
void abortFrameTransport();                       // a frame that fails after its displays began (create NULL, prepare non-OK): aborted, traits and peers cleared,
                                                  // the integrator instance destroyed WITHOUT finish (also called by prepareFrameTransport on a prepare failure)
void finishFrameTransport();                      // finish + destroy (start of endFrame)
void endFrameTransport();                         // clear (end of endFrame, after endDisplays)
```

Written only before render threads exist and after they are joined; read-only in between (FR-022).
Not part of Graphics State's `FrameParams` (research R14).

## `CRenderer::beginFrame()` (replacing `renderer.cpp:777-792` and `:808-840`)

```text
hiderFlags = 0                                   (unchanged, :736; only HIDER_BREAK/HIDER_DONE use it now)
...
beginFrameTransport(o)                           resolve hider: unknown -> error "Hider \"%s\" unavailable", reyes
                                                 resolve integrator: unknown -> error listing names, classic
                                                 transportCheckPair: shortfall -> error CODE_LIMIT, refused
                                                 not refused: create, configure, report unaccepted parameters
if (!refused && hider->preDisplaySetup) hider->preDisplaySetup()
beginDisplays()                                  no-display branch when refused or !traits.displayOutput
... samplingConfig, beginFrameParams()  (unchanged position, :805)
if (!refused):
    contexts = new CHider *[numThreads] (all NULL)
    setFramePeers(contexts, numThreads)         the slot owns the peers; set BEFORE the loop, so a constructor may read it,
                                                and CReyes may keep a pointer to it until endFrame
    for each thread i: contexts[i] = hiderOf(hider->create(&info{i, numThreads, &frame}))   (frame.peers = &frameTransport().peers)
                       NULL -> error CODE_BUG, aborted, stop the loop
                       dispatchJob = hider->dispatch == BUCKETS ? dispatchReyes : dispatchPhoton
                       contexts[i]->shadingContext().setSamplingConfig(...); updateState()   (unchanged)
    if (!aborted) prepareFrameTransport()       prepare; non-OK -> error, aborted
    if (aborted): destroy the hiders created so far at once, delete[] contexts, contexts = NULL,
                  traits cleared, integrator destroyed without finish, hiderFlags |= HIDER_BREAK
                  (abortFrameTransport() does the slot's part: aborted, traits and peers cleared, integrator destroyed without finish;
                   (everything created for the frame is destroyed: hiders and integrator instance; a configure failure,
                   detected earlier, destroyed the instance it had created and refused the frame)
else:
    contexts stays NULL
```

Late failures (`create` returning `NULL`, `prepare` returning non-OK) happen after `beginDisplays()`, so they
**abort** rather than refuse: the frame is then in the refused frame's state (`contexts == NULL`, traits
false, so every null-safe path below applies and no render thread runs), except that the displays already
started are finished by `endDisplays()` as in today's "no active display" abort (a driver may write its
file). No built-in hider or integrator reaches either path.

## The read sites

| Site | Before | After |
|---|---|---|
| `rendererDisplay.cpp:201` | `!(hiderFlags & HIDER_NODISPLAY)` | `frameTransport().traits.displayOutput` (false on a refused frame) |
| `rendererDisplay.cpp:627, 643` | `hiderFlags & HIDER_RGBAZ_ONLY` | `!frameTransport().traits.displayChannels`; same messages |
| `rendererDisplay.cpp:770` | `strcmp(hider, "reyes") != 0` | `!frameTransport().traits.deepShadowOutput`; same message |
| `rendererServicesImpl.h:61` | `hiderFlags & HIDER_ILLUMINATIONHOOK` | `frameTransport().traits.illuminationHook` |
| `renderer.cpp:1022` | `dynamic_cast<CReyes *>(contexts[0])->drawObject` | `if (frameTransport().traits.rasterization) contexts[0]->drawObject(cObject)` |
| `rendererContext.cpp:1012` (debug log) | prints `hiderFlags` | prints `hiderFlags` (unchanged; only BREAK/DONE remain) |

`illuminationHook` is a `bool` read per illuminate/solar begin and end; no lookup on that path. Tests that exercise the hook outside a frame (the four `tests/shading_parity` tests of amendment F1, 2026-11-09) switch it through a core-only, test-only setter on the slot (tasks.md T109a; its name and signature are shown at that pause); the setter is not part of any public header.

## The refused frame

| Step | Behaviour |
|---|---|
| `RiWorldBegin` → `beginFrame()` | one `CODE_LIMIT` error; texture system, shadeops, `FrameParams` set up; no-display branch; `contexts == NULL`; no integrator instance |
| world block | statements run; `render()` attaches raytraced objects to `root` and submits nothing (`rasterization` false); unraytraced objects are released by `addObject`'s `detach()`; a `Declare` reaches `notifyGlobalVariable`, which already guards `contexts != NULL` |
| `RiWorldEnd` → `renderFrame()` | returns at once if `frameTransport().refused` or `.aborted` (before `root->setChildren(&contexts[0]->...)`) |
| `RiWorldEnd` → `endFrame()` | no `finish`; the destroy loop (each hider through the table's `destroy`, in index order) is skipped when `contexts == NULL` (and skips `NULL` entries after an aborted construction); `endFrameParams`, `root->destroy()`, procedural modules, `endDisplays()` (nothing started), remote channels, photon maps as usual; `endFrameTransport()` last |
| output | no image file, no photon map; `RiLastError` set (exit -1 for `orender`); the process continues; the next frame is checked on its own |

Not changed: `rendererDeclarationsDispatch.cpp` (null-safe; spec 031 deletes the file), `imager.cpp:133`
(null-safe), `rendererDispatchThread`/`activeContext` (never run on a refused frame).

## Invariants checked by tests

1. `classic` with each built-in hider: accepted; images byte-identical to the base (US4 AS1).
2. A test-double integrator requiring an unprovided capability, and one requiring a newer version: refused
   with each built-in hider; the error names hider, integrator and capability (versions for the shortfall);
   no output file; Debug build passes `endFrame()`'s raster asserts; a following frame renders (US4 AS2, AS3).
3. An optional capability absent: accepted; `configure` sees it absent in `hiderProvides` (US4 AS4).
4. `Hider "bogus"` + `Integrator "bogus"`: two errors, frame renders with reyes + classic (US4 AS5).
5. `Hider "bogus"` with a TSM display: the TSM error still appears (edge case).
6. `classic`'s `configure`/`prepare`/`finish` each counted once per rendered frame, zero on a refused frame.
