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

## Enable the opt-in disk cache and verify reuse (SC-003)

```bash
# First render: cache miss, builds and writes the cache entry.
Option "texturecache" "enable" [1]
# ... rest of scene referencing an unbaked source ...
```

```bash
# Re-render the identical scene; the second run should reuse the
# already-written cache entry rather than re-decoding the source.
# Byte-identical comparison against a fresh in-memory-only render
# (cache disabled) is the actual regression bar -- see
# tests/unit/texture_tile/ for the automated version of this check.
```

## Verify stale-cache detection (SC-004)

```bash
# After the disk cache is populated for a source, touch (modify) the
# source file and re-render; the cache MUST be rebuilt, not silently
# reused with outdated data. See the dedicated automated test in
# tests/unit/texture_tile/ for the exact mechanics.
```

## Run the new concurrency tests

```bash
# Multi-threaded, single-process: the synthesized backend's own
# fetchTile() concurrency (SC-005).
ctest --test-dir build -R <TBD, tasks.md names this> --output-on-failure

# Multi-process: concurrent disk-cache writers never produce a
# corrupted/partial cache file (SC-006/FR-016).
ctest --test-dir build -R <TBD, tasks.md names this> --output-on-failure
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
