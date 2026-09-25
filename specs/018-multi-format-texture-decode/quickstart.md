# Quickstart: Multi-Format Texture Source Decoding

## Build

No new CMake options — OpenEXR support follows the existing optional-
dependency detection already in the root `CMakeLists.txt`.

```bash
cmake --build build --config Release
```

## Verify the regression bar (byte-identical TIFF bakes)

```bash
# Before making any change, capture a reference bake from an existing
# TIFF-sourced test texture (pick any source already used by tests/):
build/src/otexmake/otexmake <existing-tiff-source>.tif reference.tex

# After implementing, rebuild and rebake the same source:
build/src/otexmake/otexmake <existing-tiff-source>.tif rebuilt.tex

# Must be byte-identical:
cmp reference.tex rebuilt.tex
```

## Try a new source format

```bash
# PNG source, no new flags needed — format is auto-detected:
build/src/otexmake/otexmake artist_albedo.png albedo.tex

# OpenEXR source (requires a build with HAVE_OPENEXR):
build/src/otexmake/otexmake hdri_probe.exr probe.tex

# RGBE / Radiance HDR source:
build/src/otexmake/otexmake studio.hdr studio.tex
```

Each of the above should produce a texture file usable exactly like a
TIFF-sourced one — e.g. referenced from a `Texture` shader parameter and
rendered. Use the **build tree** directly (no `cmake --install` needed —
the gitignored `openrender/` deploy tree is a separate, install-only
concern, per this repo's documented deploy-tree gotcha):

```bash
SHADERS="$(pwd)/build/shaders" \
ORENDERHOME="$(pwd)" \
DISPLAYS="$(pwd)/build/src/display/file:$(pwd)/build/src/display/rgbe:$(pwd)/build/src/display/framebuffer:$(pwd)/build/src/display/openexr" \
build/src/orender/orender examples/rib/<a-texture-using-scene>.rib
```

**Known caveat when trying this yourself**: a pre-existing, unrelated bug
(`appendLayer()`'s `TIFFTAG_PHOTOMETRIC` gap, GitHub issue #18 — predates
this feature, applies to plain TIFF-sourced bakes too) makes `orender`
print libtiff warnings and exit non-zero when rendering with any bake from
an exactly-3-channel-no-alpha source (image content still renders
correctly). RGBA (4-channel) or grayscale/luminance (1-channel) sources
sidestep it cleanly — `probe.tex` above, baked from a single-channel EXR
source, is unaffected; a 3-channel RGB EXR/PNG/TIFF source would hit it.

## Run the tests

```bash
# Full visual regression suite (includes the new PNG/EXR/RGBE bake+render
# scenes once added, and the existing suite this feature must not regress):
ctest --test-dir build -L visual --output-on-failure

# Skip the slow motion-blur test while iterating:
ctest --test-dir build -L visual -E slow
```

## Negative-case sanity checks (should each fail with a clear error, not a crash)

```bash
build/src/otexmake/otexmake indexed.png out.tex      # indexed PNG — rejected
build/src/otexmake/otexmake multipart.exr out.tex     # multi-part EXR — rejected, even if single-usable-part
build/src/otexmake/otexmake deep_aov.exr out.tex      # unsupported EXR channel set — rejected
build/src/otexmake/otexmake not_really.exr out.tex    # corrupted/mislabeled file — rejected
build/src/otexmake/otexmake source.bmp out.tex        # unsupported extension — rejected
```
