# Contract: C ABI extension (`ribpreview_api.h`)

This contract governs the sole boundary between `libribpreview` (C++20) and both platform
frontends (Swift, C++). It extends the existing header in place — see research.md §4 for why no
second header is introduced.

## Existing API (unchanged)

```c
typedef struct { float projMatrix[16]; float viewMatrix[16];
                 float nearPlane; float farPlane; int projectionType; } PreviewCameraC;
typedef struct { float sceneBoundsMin[3]; float sceneBoundsMax[3]; } PreviewBoundsC;
typedef struct { float *vertices; float *colors; int vertexCount;
                 PreviewCameraC camera; PreviewBoundsC bounds; } PreviewSceneC;

PreviewSceneC *ribpreview_load(const char *ribPath);
void           ribpreview_free(PreviewSceneC *scene);
```

This value-pair API continues to serve the RIB-scene path exactly as today. It is not modified
and does not participate in the new handle-based lifecycle below.

## New API — data documents

```c
typedef struct {
    float *verts;   // flat float3
    float *cols;    // flat float3, parallel to verts
    int    count;   // number of float3 elements (not floats)
} PrimArrayC;

typedef enum {
    RIBDATA_TYPE_PHOTONMAP = 0,
    RIBDATA_TYPE_IRRADIANCECACHE,
    RIBDATA_TYPE_GATHERCACHE,
    RIBDATA_TYPE_POINTCLOUD,
    RIBDATA_TYPE_BRICKMAP,
    RIBDATA_TYPE_DEBUGDUMP,
    // No RIBDATA_TYPE_UNSUPPORTED: a prior draft of this contract included one for a
    // "recognized but no visualization" case (the CPointHierarchy variant). Removed after
    // /speckit-analyze verification (research.md §2) showed that variant is a shading-time
    // rendering strategy a shader requests at render time, never a distinguishable on-disk
    // file format — it cannot be produced by ribdata_sniff()/ribdata_open() under any file
    // content, so every value in this enum always corresponds to a real, visualizable document.
} RibDataType;

typedef struct {
    PrimArrayC     lines;
    PrimArrayC     points;
    PrimArrayC     triangles;      // includes CPU-expanded discs, see research.md §5
    int            sourceDiskCount;
    int            decimatedCount;
    PreviewBoundsC bounds;
    PreviewCameraC camera;         // synthesized framing camera
    RibDataType    documentType;
    int            numChannels;
    int            currentChannel; // -1 if numChannels == 0
    int            detailLevel;    // -1 if not applicable (non-brick-map)
    int            drawMode;       // meaning is per-documentType; see data-model.md
} DataSceneC;

typedef struct RibDataDocument RibDataDocument;   // opaque

// Sniff a file's type without opening it. Returns RIBDATA_TYPE_* or -1 if not a data file
// (including a valid RIB scene — this function does not attempt RIB parsing).
int ribdata_sniff(const char *path);

// Open a data file. Returns NULL on failure (message written to stderr); *err receives one of
// the EDataFileType codes from dataSniff() (see research.md §2 for the concrete enum) so the
// caller can map DATA_BAD_VERSION/DATA_BAD_WORDSIZE to CLI exit code 4, and DATA_NOT_A_DATA_FILE
// to "try ribpreview_load() instead" (per FR-001's content-based auto-detection).
RibDataDocument *ribdata_open(const char *path, int *err);

// Produce the current visualization state. The returned pointer is owned by the document and
// is invalidated by the next call to ribdata_snapshot() or ribdata_key() on the same handle —
// callers must finish using one snapshot (e.g., upload to GPU, or serialize to JSON) before
// requesting the next.
const DataSceneC *ribdata_snapshot(RibDataDocument *doc);

// Apply a key press (ASCII value) to the document's interactive state. Returns non-zero if
// state changed (caller should call ribdata_snapshot() again and re-upload); zero if the key
// had no effect (e.g., a channel key on a document with no channels).
int ribdata_key(RibDataDocument *doc, int key);

// Channel name lookup, 0-indexed. Returns NULL if index is out of range or numChannels == 0.
const char *ribdata_channel_name(RibDataDocument *doc, int index);

// Releases the document and everything ribdata_snapshot() has returned for it. Safe to call
// with NULL (no-op).
void ribdata_close(RibDataDocument *doc);
```

## Lifecycle contract

```
ribdata_sniff(path)                 -- optional pre-check, side-effect-free
  → RibDataDocument *doc = ribdata_open(path, &err)
  → const DataSceneC *scene = ribdata_snapshot(doc)    -- upload scene to GPU / serialize
  → ... user presses a key ...
  → if (ribdata_key(doc, key)) {
        scene = ribdata_snapshot(doc);                  -- re-upload; previous `scene` pointer is now invalid
    }
  → ribdata_close(doc);                                 -- also invalidates the last `scene` pointer
```

- **Why a handle, not a value struct** (research.md §4): the `keyDown`/re-emit interaction cycle
  needs the underlying `CDataView` to persist across frames; reconstructing the whole document
  on every keystroke would be wasteful and would lose in-progress LOD/channel state that isn't
  itself part of the file.
- A `RibDataDocument` is opened by exactly one caller at a time; this feature does not require
  the ABI to support concurrent access to the same handle from multiple threads.
- `ribdata_open` on a file that is a valid RIB scene, not a data file, returns NULL with
  `*err` indicating "not a data file" — callers are expected to call `ribdata_sniff` or attempt
  `ribpreview_load` first, per the existing content-based auto-detection (FR-001).

## Header consolidation

**Corrected during implementation (I0)**: the original draft of this contract had
`CRibPreview.h` include both `ribpreview_api.h` and `cameraExport.h`. On inspection,
`cameraExport.h` declares only the C++ overload-friendly `writeRibCamera`/`replaceRibCamera`
(taking a `CameraExport&` struct, returning `bool`) — not Swift-importable, and not what
`WireframeRenderer.swift` actually calls. The C-linkage `ribcam_write`/`ribcam_replace`
wrappers it *does* call are `extern "C"` functions defined in `cameraExport.cpp` but were,
before this correction, declared **only** in the hand-maintained `CRibPreview.h` duplicate —
exactly the drift this consolidation exists to close, just not fully closed in the first draft.

Fix: `ribcam_write`/`ribcam_replace` are now declared in `ribpreview_api.h` itself, alongside
`ribpreview_load`/`ribpreview_free`, since they are genuinely part of the public C ABI. `Linux`
continues to call `writeRibCamera`/`replaceRibCamera` directly via `cameraExport.h` (unchanged —
that is C++-to-C++, no Swift involved). `orender-wire-macos/CRibPreview/include/CRibPreview.h`
is reduced to a single include:

```c
#include "ribpreview_api.h"
```

with `Package.swift`'s `CRibPreview` target given **one** `headerSearchPath` entry reaching
`../../` (for `ribpreview_api.h` alone — `cameraExport.h` is never needed on the Swift side).
This is a smaller, lower-risk change than the original two-header plan. If SPM rejects
cross-target header search paths, the fallback (a CMake `configure_file`/`copy_if_different`
staging step, gitignored) preserves the same "one header, one include" contract — the fallback
changes *how* the file reaches the target, never *what* Swift code includes.

## Linux consumption

`orender-wire-linux/main.cpp` includes `ribpreview_api.h` directly (as it already does today) —
no separate header exists on the Linux side, so there is nothing to consolidate there.
