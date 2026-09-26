/**
 * tests/unit/texture_tile/test_tile_source_tiff_info.cpp
 *
 * Unit test for CTiffTileSource::info() (spec 019-tilesource-extraction,
 * T020): asserts CTileLevelInfo is populated correctly for a known fixture.
 *
 * info() has no caller anywhere in this spec's own production code --
 * readMadeTexture()/readTexture() keep their existing direct TIFF geometry
 * queries (research.md SS1/SS4) -- so without a direct test it would ship as
 * unverified, uncalled interface surface. CTiffTileSource itself is
 * file-local to texture.cpp (spec 019 T015 deliberately doesn't expose it
 * outside that one file, since nothing in production needs to); this test
 * reaches it only through the createTiffTileSource() factory declared in
 * tileSource.h, exercising it purely through the abstract CTileSource
 * interface.
 */

#include "tileSource.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "CHECK FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); abort(); } } while (0)

static std::string fixturesDir() {
    const char *dir = getenv("TEXTURE_TILE_FIXTURES_DIR");
    return dir ? dir : "tests/unit/texture_tile/fixtures";
}

int main() {
    const std::string path = fixturesDir() + "/concurrency_rgb.tex";

    // Directory 0: the base mip level of concurrency_rgb.tex's 9-level
    // pyramid (512x512, otexmake's default 32x32 tiles, 8-bit RGB) --
    // see fixtures/FIXTURES.md.
    CTileSource *source = createTiffTileSource(path.c_str(), 0);

    CTileLevelInfo info;
    source->info(info);

    CHECK(info.width == 512);
    CHECK(info.height == 512);
    CHECK(info.tileWidth == 32);
    CHECK(info.tileHeight == 32);
    CHECK(info.numChannels == 3);
    CHECK(info.bitsPerSample == 8);
    CHECK(!info.isFloatFormat);

    delete source;

    // Directory 1: the next mip level down -- half the base level's
    // resolution, same tile size.
    source = createTiffTileSource(path.c_str(), 1);
    source->info(info);

    CHECK(info.width == 256);
    CHECK(info.height == 256);
    CHECK(info.tileWidth == 32);
    CHECK(info.tileHeight == 32);
    CHECK(info.numChannels == 3);
    CHECK(info.bitsPerSample == 8);
    CHECK(!info.isFloatFormat);

    delete source;

    printf("test_tile_source_tiff_info: ALL PASS\n");
    return 0;
}
