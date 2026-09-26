# Fixture provenance (T011)

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
