# Contract: `CSynthesizedTileSource` conformance to `CTileSource`

This spec's only new concrete `CTileSource` implementation. Reuses the
existing interface unchanged (`src/ri/texture/tileSource.h`, spec 019) —
this document states how `CSynthesizedTileSource` specifically satisfies
each of that interface's contract rules, not a new interface.

## Conformance

1. **One instance per mip level.** Satisfied by construction: one
   `CSynthesizedTileSource` per pyramid level, each holding a
   `std::shared_ptr<CSynthesizedPyramid>` plus its own level index (see
   `data-model.md`) — never one instance serving all levels.
2. **Full-region fetches only.** Satisfied identically to
   `CTiffTileSource`: `fetchTile(tileX, tileY, dest)` always fills one
   complete, tile-aligned `DEFAULT_TILE_SIZE`-square tile (or, if this
   spec's synthesized backend is ever used for a flat/un-made texture, the
   whole level as tile (0,0)) — no sub-region parameter, matching the
   interface exactly.
3. **No colorspace/gamma conversion, no resampling** (beyond the
   already-established non-power-of-two resize + mip-reduction that
   happens once, at pyramid-construction time, per `research.md` §3 —
   not per `fetchTile()` call). Samples delivered exactly as they exist in
   the already-built pyramid level, in the native precision `info()`
   reports.
4. **Concurrency.** A single instance's `fetchTile()` is safe to call from
   any thread once construction (single-threaded, at texture-load time)
   has completed — the pyramid it reads from is immutable thereafter, and
   `std::shared_ptr`'s refcounting is independently thread-safe. Safe when
   a *different* instance's `fetchTile()` is called concurrently, from
   any level or texture. Not required to (and does not need to) guard
   against two threads calling `fetchTile()` on the *same* instance for
   the *same* tile concurrently — the caller's per-`CTextureBlock` lock
   (unchanged, `textureLoadBlock()`) already guarantees that never
   happens, identically to `CTiffTileSource`.
5. **Fail closed.** `fetchTile()` returns `false` only if the requested
   tile coordinates are entirely outside the level's valid bounds (a
   caller bug, since every real caller already clamps coordinates to
   valid ranges) — normal partial-trailing-tile reads (research.md §2)
   are not a failure case, they simply copy less than a full tile's worth
   of real data.

## Construction

Exposed via a factory in `tileSource.h`, mirroring
`createTiffTileSource()`'s existing pattern:

```cpp
// src/ri/texture/tileSource.h

// Decodes filename via CImageInput, builds the full in-memory mip
// pyramid (resizing to a power of two first if needed), and returns one
// CTileSource instance for level "level" of that pyramid. Returns
// nullptr if filename cannot be decoded by any supported CImageInput
// backend. Ownership of the returned CTileSource transfers to the
// caller; the underlying pyramid is shared (refcounted) across every
// CTileSource this function returns for the same decode.
CTileSource *createSynthesizedTileSource(const char *filename, int level);
```

Unlike `createTiffTileSource()` (cheap — just stores a filename+directory,
no I/O until `fetchTile()`), a caller is expected to call this factory
once per level needed (typically via a small internal helper that decodes
once and constructs all `numLevels()` instances together, since decoding
happens on the *first* call for a given source and every subsequent call
for the same source within the same texture-load reuses the already-built
pyramid) — the exact internal helper shape (e.g. a `decodeAndBuildPyramid()`
free function called once by `CRenderer::textureLoad()`'s new fallback
branch, which then calls this factory `numLevels()` times) is a
`tasks.md`-level implementation detail, not part of this contract.

## Non-goals

- Does not implement disk-cache reading — a disk cache entry is an
  ordinary baked TIFF, read back through the unmodified
  `CTiffTileSource`, not through this class.
- Does not implement disk-cache *writing* either — that is a separate
  call to the existing `makeTexture()` (research.md §8), invoked
  independently of constructing a `CSynthesizedTileSource`.
