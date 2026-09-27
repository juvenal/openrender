# Fixture provenance (T011, spec 019; T002b, spec 020)

## `concurrency_rgb.tex` (spec 019, T011)

`concurrency_rgb.tex` is `otexmake`'s plain-texture bake of spec 018's
existing `tests/unit/image_input/fixtures/large_rgb.tif` (512x512, 8-bit
RGB, no resize needed since 512 is already a power of two) — reused
rather than generating a new source image, since it already has a known,
closed-form per-pixel formula documented in
`tests/unit/image_input/fixtures/FIXTURES.md`:

- R(x,y) = (3x+10) mod 256
- G(x,y) = (5y+20) mod 256
- B(x,y) = (x+2y+30) mod 256

for x,y in `[0,511]`.

The bake produces a 9-level mip pyramid (512 → 256 → 128 → 64 → 32 → 16 →
8 → 4 → 2). `otexmake`'s default 32x32 tile size against the 512x512 base
level (mip level 0) gives a 16x16 grid of 256 distinct tiles — level 0 is
a direct resample of the source with no box-filter reduction applied, so
its pixel values are an exact match to the formula above (no averaging to
account for), giving the concurrency test (`test_tile_source_concurrency.cpp`,
T012) plenty of distinct, exactly-verifiable tiles to fault in
concurrently from multiple threads without needing to replicate
`appendPyramid()`'s box-filter reduction math for higher mip levels.

## `nonpot_rgb.png` (spec 020, T002b)

`nonpot_rgb.png` is a deliberately **non-power-of-two** (500x300) 8-bit RGB
PNG, generated for spec 020's `020-runtime-tile-paging` — closing analysis
finding C1 (a fresh `/speckit.analyze` pass found the spec's original
Foundational fixture set was entirely power-of-two-sized: `large_rgb8.png`/
`large_rgb.exr`/`large.hdr` are all 512x512). 500x300 is deliberately not a
power of two and not a multiple of `DEFAULT_TILE_SIZE` (32,
`src/ri/core/ri_config.h:31`), so referencing it directly (no bake step)
exercises `CSynthesizedTileSource`'s non-power-of-two resize path
end to end (FR-012, spec.md's own non-power-of-two Edge Case).

Reuses the exact same closed-form RGB formula as
`tests/unit/image_input/fixtures/FIXTURES.md`'s `{}_rgb.tif`/`{}_rgb8.png`
fixtures, for `x` in `[0,499]`, `y` in `[0,299]`:

- R(x,y) = (3x+10) mod 256
- G(x,y) = (5y+20) mod 256
- B(x,y) = (x+2y+30) mod 256

Generated via a small scratch script (Pillow + numpy, not checked in — the
formula above fully reproduces it):

```python
from PIL import Image
import numpy as np

W, H = 500, 300
x = np.arange(W).reshape(1, W)
y = np.arange(H).reshape(H, 1)

R = np.broadcast_to((3 * x + 10) % 256, (H, W)).astype(np.uint8)
G = np.broadcast_to((5 * y + 20) % 256, (H, W)).astype(np.uint8)
B = np.broadcast_to((x + 2 * y + 30) % 256, (H, W)).astype(np.uint8)

img = np.stack([R, G, B], axis=-1)
Image.fromarray(img, mode="RGB").save("nonpot_rgb.png")
```

Verified against the formula directly: pixel (10,20) = (40, 120, 80),
matching R=(3*10+10)%256=40, G=(5*20+20)%256=120, B=(10+2*20+30)%256=80.

## `cache_stale_v2.png` (spec 020, T020)

`cache_stale_v2.png` is a second, deliberately-different 512x512 8-bit RGB
PNG used only by `test_texture_cache_stale_detection.sh` (T020, SC-004):
that test needs two genuinely different pixel contents at the SAME logical
source path, at two different points in time, so a bug that silently served
a stale cache entry (same pixels as before the "modification") is
distinguishable from correct stale-detection (new pixels, matching this
file's own formula, plus a new cache filename). Same channel count/bit
depth/dimensions as `tests/unit/image_input/fixtures/large_rgb8.png`
deliberately, so both share `CTiledTexture<T>`'s existing (pre-existing,
out-of-scope) 3-channel `lookupPixel()` access pattern identically — only
the pixel *content* needs to differ, not the format.

For x,y in `[0,511]`:

- R(x,y) = (x+7) mod 256
- G(x,y) = (y+13) mod 256
- B(x,y) = (x+y+19) mod 256

Generated via a small scratch script (Pillow + numpy, not checked in — the
formula above fully reproduces it):

```python
from PIL import Image
import numpy as np

W, H = 512, 512
arr = np.zeros((H, W, 3), dtype=np.uint8)
for y in range(H):
    for x in range(W):
        arr[y, x, 0] = (x + 7) % 256
        arr[y, x, 1] = (y + 13) % 256
        arr[y, x, 2] = (x + y + 19) % 256

Image.fromarray(arr, mode="RGB").save("cache_stale_v2.png")
```

Verified against the formula directly: pixel (10,20) = (17, 33, 49),
matching R=(10+7)%256=17, G=(20+13)%256=33, B=(10+20+19)%256=49.
