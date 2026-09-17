# Data Model: orender-wire Data-Structure Viewer (oshow Absorption)

Entities below extend the spec's Key Entities section with the concrete shape decided in
`research.md`. Two layers exist: a C++ layer internal to `libribpreview`/`libri`, and a flat C
ABI layer (`ribpreview_api.h`) consumed by both platform frontends. Field names in the C++ layer
are illustrative of intent; exact naming is finalized at task-authoring time.

## Scene Document (unchanged)

The existing RIB-scene representation. Not modified by this feature — included here only to
show where it sits relative to the new Data Document type.

| Field | Type | Notes |
|---|---|---|
| `vertices`, `colors` | `float3[]` | flat line-list, as today |
| `camera` | `PreviewCamera` | as today |
| `sceneBounds` | `AABB` | as today |

## Data Document

The loaded, displayable representation of one precomputed data-structure file. Owns exactly one
`CDataView` (C++ layer) for its lifetime.

**C++ layer** — `CDataDocument` (`src/ri/dataLoad.h`):

| Field | Type | Notes |
|---|---|---|
| `type` | `EDataFileType` (research.md §2) | one of: `DATA_PHOTONMAP`, `DATA_IRRADIANCECACHE`, `DATA_GATHERCACHE`, `DATA_POINTCLOUD`, `DATA_BRICKMAP`, `DATA_DEBUGDUMP`; `DATA_NOT_A_DATA_FILE`/`DATA_BAD_VERSION`/`DATA_BAD_WORDSIZE` never reach a constructed `CDataDocument` (open() returns null instead) |
| `view` | `CDataView *` | owned; the five reader classes or `CDebugView` |
| — | — | no separate bounds/camera fields; these are queried from `view->bound()` on demand |

**C ABI layer** — `DataSceneC` (`ribpreview_api.h`), produced by `ribdata_snapshot()`:

| Field | Type | Notes |
|---|---|---|
| `lines`, `points`, `triangles` | `PrimArrayC { float *verts; float *cols; int count; }` | `triangles` includes CPU-expanded discs |
| `sourceDiskCount` | `int` | number of discs before expansion (for `--json` and tests) |
| `decimatedCount` | `int` | primitives dropped by the deterministic cap; 0 if none |
| `bounds` | `PreviewBoundsC` | reused from the existing RIB-path struct |
| `camera` | `PreviewCameraC` | synthesized framing camera (not read from a RIB) |
| `documentType` | `RibDataType` (contracts/c-abi.md) | one of the six file-content-detectable types; never a rejection code — rejections are reported via `ribdata_open`'s `*err` output instead, so no rejected file ever produces a `DataSceneC` |
| `numChannels`, `currentChannel` | `int` | 0/-1 when not applicable (e.g., debug dump) |
| `detailLevel` | `int` | brick map only; -1 when not applicable |
| `drawMode` | `int` (enum) | meaning is per-`documentType` (see Draw Mode below) |

**Validation rules** (from FR-001 through FR-006):
- `type` is determined solely from file content (magic number + type string), never from
  filename or extension.
- `CDataDocument::open()` returns null — never a constructed document — when: the file is
  truncated/corrupt, the magic number doesn't match any known type and the file also fails to
  parse as a debug-geometry dump (`DATA_NOT_A_DATA_FILE`), or the version/word-size check fails
  (`DATA_BAD_VERSION`/`DATA_BAD_WORDSIZE`, reported distinctly — exit code 4, not a generic
  parse failure).
- Every file that sniffs as one of the six detectable types always produces a successfully
  visualized document — there is no "recognized but no visualization" outcome in this feature's
  scope (see research.md §2's confirmation that the one candidate for such a case, a
  shading-time-only rendering strategy, cannot be selected by file content at all).
- A document with zero primitives is valid (not an error) and reports `decimatedCount == 0` and
  all counts `== 0` (FR-005).

## Display Channel

One named, selectable data quantity a Data Document may expose.

| Field | Type | Notes |
|---|---|---|
| `name` | `const char *` | e.g., a recorded value name in a point cloud |
| `index` | `int` | position among the document's channels; `currentChannel` in `DataSceneC` |

**Rules**: at most one channel active at a time (FR-013); a document type with no distinct
channels (debug-geometry dump) reports `numChannels == 0` and offers no channel control
(FR-017).

## Detail Level

A brick map's level-of-detail setting.

| Field | Type | Notes |
|---|---|---|
| value | `int` | adjustable up/down (FR-011); range and default are internal to `CBrickMap` |

**Rules**: only meaningful for brick-map documents; `detailLevel == -1` in `DataSceneC` for all
other document types.

## Draw Mode

How a Data Document's primitives are currently rendered. The set of valid values depends on
`documentType`:

| Document Type | Valid Draw Modes |
|---|---|
| Brick map | boxes, discs, points |
| Point cloud | points, discs |
| Photon map, irradiance/gather cache, debug-geometry dump | fixed (points, or the dump's own per-record shape); no user-selectable draw mode |

**Rules**: switching draw mode never changes `documentType` or channel selection (FR-012,
FR-014); a document type with a fixed rendering (photon map, caches, debug dump) does not offer
a draw-mode control.

## State transitions

```
(no document) --open file--> Data Document | Scene Document
Data Document --open another file--> (previous document discarded) --> Data Document | Scene Document
Data Document --key/menu: channel next/prev--> Data Document (currentChannel updated, re-emitted)
Data Document --key/menu: detail +/-  --> Data Document (detailLevel updated, re-emitted)      [brick map only]
Data Document --key/menu: draw mode   --> Data Document (drawMode updated, re-emitted)          [brick map, point cloud]
Scene Document --orbit/pan/zoom/reset/save--> Scene Document (camera updated only; unchanged by this feature)
```

Every transition above that "re-emits" corresponds to `ribdata_key()` returning non-zero,
triggering a fresh `ribdata_snapshot()` and a GPU buffer rebuild — this mirrors the legacy
`keyDown()` → `draw()` cycle exactly (research.md §1), just routed through the new sink instead
of a `dlopen`ed function pointer.

Exactly one document (Scene or Data) exists at a time (FR-019); opening a new file always
transitions from "whatever is open" directly to the new document, with no intermediate empty
state exposed to the user.
