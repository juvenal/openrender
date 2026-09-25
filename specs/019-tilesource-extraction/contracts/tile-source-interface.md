# Contract: `CTileSource` tile-fetch interface

The internal C++ interface this feature introduces at the render-time
seam `textureLoadBlock()` used to occupy inline. This is the counterpart,
on the runtime-read side, to spec 018's `CImageInput` on the bake-input
side — both are small, compiled-in, format-agnostic interfaces meant to
outlive this one spec: `CImageInput` is already consumed by 4 concrete
decoders; `CTileSource` is expected to gain further backends under spec
020, a separate, later effort.

## Interface

```cpp
// src/ri/texture/tileSource.h

struct CTileLevelInfo {
    int width = 0;
    int height = 0;
    int tileWidth = 0;
    int tileHeight = 0;
    int numChannels = 0;
    int bitsPerSample = 0; // 8, 16, or 32
    bool isFloatFormat = false;
};

class CTileSource {
public:
    virtual ~CTileSource() {}

    // Populates info with this instance's single level's geometry.
    virtual void info(CTileLevelInfo &info) = 0;

    // Fetches one full, tile-aligned tile into dest, a caller-allocated
    // buffer sized as tileWidth * tileHeight * numChannels *
    // (bitsPerSample / 8) bytes (per the info() this instance reports).
    // For a non-tiled (flat/un-made) source, tileX/tileY are always
    // (0, 0) and the "tile" is the entire image. Returns false on any
    // I/O or decode failure.
    virtual bool fetchTile(int tileX, int tileY, void *dest) = 0;
};
```

## Contract rules

1. **One instance per mip level.** A `CTileSource` implementation
   represents exactly one already-known level of one texture — there is
   no level-selection parameter anywhere in this interface. A texture
   with multiple mip levels is represented by multiple `CTileSource`
   instances, one per level (owned one-each by the corresponding
   `CTiledTexture` layer object) — not one instance serving all levels.
2. **Full-region fetches only.** Every `fetchTile()` call requests the
   complete tile (or, for a non-tiled source, the complete image) — there
   is no sub-region parameter. Implementations backing a format that can
   only decode a whole image at once (a plausible future non-TIFF
   backend) are never asked for anything less than that.
3. **No colorspace/gamma conversion, no resampling.** `fetchTile()`
   delivers samples exactly as stored, in the native precision `info()`
   reports — mirrors `CImageInput`'s equivalent rule from spec 018.
4. **Concurrency.** A single instance's `fetchTile()` must be safe to
   call from a different thread than the one that constructed it, and
   must be safe when a *different* `CTileSource` instance's `fetchTile()`
   is called concurrently from another thread. It is NOT required to
   guard against two threads calling `fetchTile()` on the *same* instance
   for the *same* tile concurrently — the caller (the per-`CTextureBlock`
   cache-slot lock, unchanged by this spec) already guarantees that
   never happens.
5. **Fail closed.** Any I/O or decode failure returns `false` — never a
   partial or garbage-filled buffer, and never a crash.

## Conformance

`CTiffTileSource` (this spec's only concrete implementation) is validated
against this contract by:
- The byte-identical rendered-output regression test (`research.md` §5):
  proves `fetchTile()`'s actual pixel output, end to end through
  rendering, is unchanged from `textureLoadBlock`'s pre-refactor
  behavior.
- The new multi-threaded concurrency test (`research.md` §6): proves rule
  4 above holds for concurrent, different-instance/different-tile access.
- Its `fetchTile()` implementation is required to still contain the
  currently-dead partial-sub-region-read and `PLANARCONFIG_SEPARATE`
  branches from `textureLoadBlock`, verbatim — a code-review-level check
  (not automatically testable, since nothing exercises those branches
  today by design; see `plan.md`'s Constraints and spec.md's FR-005).
