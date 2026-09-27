# Phase 1 Data Model: Runtime Bake-on-Load for Non-TIFF Texture Sources

This feature's persistent state is limited to the opt-in disk cache
(itself just an ordinary baked TIFF file — no new file format). Its
in-memory data model is the shape of a decoded-and-prepared source image
and how per-level `CTileSource` views share it.

## Synthesized Pyramid

The full, decoded-once, immutable mip pyramid built in memory from an
unbaked source image. Conceptually the in-memory counterpart to what a
baked TIFF's own on-disk directory structure already provides.

**Lifecycle**: Built exactly once, single-threaded, at texture-load time
(inside `CRenderer::textureLoad()`'s new fallback branch), after a
successful `CImageInput::open()`+`readImage()` — resized to a power of two
first if needed (research.md §3), then reduced level-by-level using the
same 2x2-block-average math `otexmake`'s own bake pipeline uses. Never
mutated after construction. Freed once every `CSynthesizedTileSource`
view referencing it has been destroyed (research.md §1).

**Ownership**: Shared — held via `std::shared_ptr` by every per-level
`CSynthesizedTileSource` view constructed from it (one per
`CTiledTexture<T>` layer, matching `CMadeTexture`'s existing `layers[]`
array, unchanged). No single view "owns" it exclusively.

| Field | Type | Description |
|---|---|---|
| `levels` | vector of per-level buffers | One entry per mip level, index 0 = full (post-resize) resolution |
| *(per level)* `width`, `height` | int | This level's pixel dimensions |
| *(per level)* `numChannels`, `bitsPerSample`, `isFloatFormat` | int, int, bool | Carried from the original `CImageInfo` the source decoded to — unchanged across all levels |
| *(per level)* `data` | owned buffer | Tightly-packed, row-major pixel data for this level |

## Synthesized Tile Source (the new `CTileSource` backend)

A `CTileSource` view over one level of a shared `CSynthesizedPyramid`.
One instance per level — same per-level-instance shape spec 019's
`CTiffTileSource` already uses (`tileSource.h`'s existing contract,
unchanged).

**Lifecycle**: Constructed once per level, alongside its owning
`CTiledTexture<T>` layer (mirrors `CTiffTileSource`'s own construction
timing, `research.md` from spec 019 §4). `fetchTile()` may be called any
number of times (once per cache-miss fault-in for that level's tiles —
most lookups never reach it, per the unchanged `CTextureBlock` cache
above this seam). Destroyed when the owning layer is destroyed;
destruction only releases this view's share of the pyramid (research.md
§1).

**Ownership**: Exactly one owner — the `CTiledTexture<T>` layer
constructed with it (same one-owner, freed-in-destructor discipline as
`CTiffTileSource`). Internally holds a `std::shared_ptr<CSynthesizedPyramid>`
(shared with sibling level-views of the same texture) plus its own level
index — not exclusive ownership of the pyramid data itself.

**Concurrency**: Safe for concurrent `fetchTile()` calls across different
instances (different levels, different textures) with no additional
locking — the pyramid is immutable once built and `std::shared_ptr`'s own
refcounting is already thread-safe. Matches the same concurrency contract
`CTileSource` already documents (`tileSource.h`); this spec's new
multi-threaded test (research.md §11) verifies it empirically for this
specific backend, not merely by inspection.

| Field | Type | Description |
|---|---|---|
| `pyramid` | `std::shared_ptr<CSynthesizedPyramid>` | Shared with sibling level-views of the same texture |
| `level` | int | Which pyramid level this instance represents |
| `tileWidth`, `tileHeight` | int | `DEFAULT_TILE_SIZE` (research.md §2) |

## Disk Cache Entry

An opt-in, persisted copy of a Synthesized Pyramid, stored on disk as an
ordinary baked TIFF (written via the existing `makeTexture()`, read back
via the existing, unmodified `CTiffTileSource` — no new on-disk format).

**Identity/key**: `<hash-of-absolute-source-path>-<source-mtime-epoch-seconds>.tex`
(research.md §5) under the configured (or default temp/cache) cache root.
A source file's current mtime deterministically selects which cache
filename is checked — there is no separate "is this entry stale"
comparison step; a changed source simply resolves to a different,
not-yet-existing filename, so the old entry is orphaned rather than
actively invalidated.

**Lifecycle**: Written once per distinct (source path, source mtime) pair,
the first time it's needed and the disk cache is enabled; read on every
subsequent load that resolves to the same filename, for as long as the
source file's mtime doesn't change. Never mutated once visible to readers
— a rewrite always targets a fresh temporary path first, then replaces
the (deterministically identical, since the key is a pure function of
path+mtime) final path via an atomic rename (research.md §6). Never
actively deleted by this feature (no cache-eviction requirement in
spec.md).

**Concurrency**: Multiple writers may race to produce the same cache
entry (the render-farm case, User Story 2 Acceptance Scenario 4); each
writes its own complete temporary file and atomically renames it into the
same final path — the last rename to complete wins, and every writer
produced an equally valid entry for that same key, so no reader-visible
corruption is possible (FR-016/SC-006).

## `COptions` additions (new RIB `Option` settings)

Two new fields on the existing `COptions` struct (`src/ri/state/options.h`),
alongside existing fields like `texturePath`, populated by the new
`Option "texturecache"` dispatch branch (research.md §7):

| Field | Type | Default | Description |
|---|---|---|---|
| `textureCacheEnabled` | bool | `false` (opt-in) | Whether the disk cache is consulted/written for unbaked sources |
| `textureCacheDirectory` | owned string (nullable) | unset → system temp/cache dir resolved at first use | Configured cache root override |
