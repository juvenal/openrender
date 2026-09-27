# Quickstart: Runtime Bake-on-Load for Non-TIFF Texture Sources

## Build

No new CMake options or dependencies.

```bash
cmake --build build --config Release
```

## Reference a plain image directly, no bake step

```bash
# A scene whose Surface shader references a raw PNG/EXR/RGBE file that
# has never been through otexmake:
SHADERS="$(pwd)/build/shaders" ORENDERHOME="$(pwd)" \
DISPLAYS="$(pwd)/build/src/display/file:$(pwd)/build/src/display/rgbe:$(pwd)/build/src/display/framebuffer:$(pwd)/build/src/display/openexr" \
TEXTURES="<directory containing the unbaked source image>" \
build/src/orender/orender <a-scene-referencing-an-unbaked-texture>.rib
```

Should render with the image correctly mapped — no `error(CODE_NOFILE,
"Failed open texture ...")` message, no black/inert `CDummyTexture`
substitution.

## Verify the existing baked-TIFF path is completely unaffected (SC-002)

```bash
ctest --test-dir build -R TextureTile_RenderByteIdentical --output-on-failure
ctest --test-dir build -L visual --output-on-failure   # must stay 100% passing
```

## Verify a non-power-of-two unbaked source works (FR-012)

```bash
# examples/rib/tests/parity/unbaked-nonpot-reyes.rib references a
# deliberately non-power-of-two fixture (tests/unit/texture_tile/fixtures/
# nonpot_rgb.png, 500x300) -- confirm it renders correctly, exercising the
# same non-power-of-two resize path otexmake's own bake pipeline uses.
ctest --test-dir build -R Parity_unbaked-nonpot --output-on-failure
```

## Verify baked vs. unbaked renders are visually equivalent (User Story 1 Acceptance Scenario 2)

```bash
# Bake large_rgb8.png via otexmake, then compare its baked render against
# examples/rib/tests/parity/unbaked-png-reyes.rib's direct (unbaked)
# reference of the same source -- see tests/unit/texture_tile/ for the
# automated version of this check.
```

## Verify a genuinely undecodable file still degrades gracefully (User Story 1 Acceptance Scenario 3)

```bash
# Reference tests/unit/image_input/fixtures/tiny_indexed.png (palette-
# indexed, must be rejected by CImageInput) as a texturename; confirm the
# render still succeeds with CDummyTexture substitution and the expected
# error(CODE_NOFILE, "Failed open texture ...") message -- no crash.
```

## Enable the opt-in disk cache and verify reuse (SC-003)

```bash
# First render: cache miss, builds and writes the cache entry.
Option "texturecache" "enable" [1]
# ... rest of scene referencing an unbaked source ...
```

```bash
# Re-render the identical scene; the second run should reuse the
# already-written cache entry rather than re-decoding the source.
ctest --test-dir build -R TextureTile_CacheByteIdentical --output-on-failure
```

## Verify the default cache directory works with no override configured (FR-015)

```bash
# Option "texturecache" "enable" [1] alone -- no "directory" token --
# must still write to and read back from a real, working default
# location.
ctest --test-dir build -R TextureTile_CacheDefaultDirectory --output-on-failure
```

## Verify stale-cache detection (SC-004)

```bash
# After the disk cache is populated for a source, touch (modify) the
# source file and re-render; the cache MUST be rebuilt, not silently
# reused with outdated data.
ctest --test-dir build -R TextureTile_CacheStaleDetection --output-on-failure
```

## Verify concurrent disk-cache writers never corrupt a cache entry (SC-006/FR-016)

```bash
# Multiple orender processes racing to build the same missing cache
# entry (the render-farm case) -- none may ever observe a corrupted or
# partially-written file.
ctest --test-dir build -R TextureTile_CacheConcurrentWrite --output-on-failure
```

## Verify the decode+pyramid-construction pipeline directly

```bash
# Direct unit test: CSynthesizedTileSource::info()/fetchTile() against
# large_rgb8.png's own closed-form pixel formula, independent of any
# full render.
ctest --test-dir build -R TileSource_SynthesizedInfo --output-on-failure
```

## Run the new concurrency test

```bash
# Multi-threaded, single-process: the synthesized backend's own
# fetchTile() concurrency (SC-005).
ctest --test-dir build -R TileSource_SynthesizedConcurrency --output-on-failure
```

## Run the full test suite

```bash
ctest --test-dir build -L visual --output-on-failure
ctest --test-dir build -L texture_tile --output-on-failure
ctest --test-dir build -L image_input --output-on-failure   # spec 018's suite, unaffected but part of the standard gate
```

## Sanity check: the existing baked-TIFF fast path took priority

```bash
# For any scene using an already-baked TIFF texture, confirm the new
# fallback branch was never reached -- e.g. via a temporary log line or
# debugger breakpoint inside CRenderer::textureLoad()'s new branch,
# never firing for a baked-TIFF reference.
```
