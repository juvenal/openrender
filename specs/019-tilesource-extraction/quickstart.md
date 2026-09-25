# Quickstart: Runtime Texture Tile-Fetch Abstraction

## Build

No new CMake options or dependencies.

```bash
cmake --build build --config Release
```

## Verify the regression bar (byte-identical rendered output)

```bash
# Before making any change, save a baseline orender binary and render an
# existing scene that uses an already-baked texture:
cp build/src/orender/orender /tmp/orender-before
SHADERS="$(pwd)/build/shaders" ORENDERHOME="$(pwd)" \
DISPLAYS="$(pwd)/build/src/display/file:$(pwd)/build/src/display/rgbe:$(pwd)/build/src/display/framebuffer:$(pwd)/build/src/display/openexr" \
/tmp/orender-before examples/rib/tests/<a-texture-using-scene>.rib
mv <output>.tif /tmp/before.tif

# After implementing, rebuild and re-render the same scene:
cmake --build build --target orender
SHADERS="$(pwd)/build/shaders" ORENDERHOME="$(pwd)" \
DISPLAYS="$(pwd)/build/src/display/file:$(pwd)/build/src/display/rgbe:$(pwd)/build/src/display/framebuffer:$(pwd)/build/src/display/openexr" \
build/src/orender/orender examples/rib/tests/<a-texture-using-scene>.rib

# Must be byte-identical:
cmp /tmp/before.tif <output>.tif
```

Repeat for at least one environment-map or shadow-map scene, per spec.md's
User Story 1 acceptance scenarios 2-3.

## Run the new concurrency test

```bash
ctest --test-dir build -R TileSource_Concurrency --output-on-failure
```

Should pass reliably, repeatedly — run it several times back to back to
build confidence there's no intermittent flakiness:

```bash
for i in 1 2 3 4 5; do ctest --test-dir build -R TileSource_Concurrency || break; done
```

## Run the full test suite

```bash
ctest --test-dir build -L visual --output-on-failure   # must stay 100% passing
ctest --test-dir build -L image_input                  # spec 018's suite, unaffected but part of the standard gate
```

## Sanity check: no TIFF-specific references left in the lookup path

```bash
# Should find matches only inside the new CTiffTileSource implementation
# (and the untouched, separate readMadeTexture()/environmentLoad() metadata
# queries this spec doesn't move) -- not scattered through
# CTiledTexture<T>::lookupPixel()/CBasicTexture<T>::lookupPixel() or their
# tile-access macros.
grep -n "TIFFOpen\|TIFFReadTile\|TIFFReadScanline\|TIFFClose" src/ri/texture/texture.cpp
```
