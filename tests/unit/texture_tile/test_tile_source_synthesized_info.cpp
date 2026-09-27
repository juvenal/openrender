/**
 * tests/unit/texture_tile/test_tile_source_synthesized_info.cpp
 *
 * Unit test for CSynthesizedTileSource::info()/fetchTile() (spec
 * 020-runtime-tile-paging, T012): asserts CTileLevelInfo and fetched pixel
 * content are correct for a known fixture, mirroring
 * test_tile_source_tiff_info.cpp's existing pattern for the spec 019
 * CTiffTileSource backend.
 *
 * large_rgb8.png (tests/unit/image_input/fixtures/, spec 018) is already
 * 512x512 -- a power of two -- so level 0 of the synthesized pyramid is a
 * direct resample with no non-power-of-two resize to account for, letting
 * this test assert exact pixel values against the closed-form formula in
 * that directory's own FIXTURES.md: R(x,y) = (3x+10) mod 256,
 * G(x,y) = (5y+20) mod 256, B(x,y) = (x+2y+30) mod 256.
 *
 * This is a standalone C++ binary, not a RIB render -- it never goes
 * through the renderer's TEXTURES search path, so it reads the fixture
 * path from its own IMAGE_INPUT_FIXTURES_DIR environment variable,
 * matching spec 018's own established convention
 * (test_image_input_tiff.cpp's getenv("IMAGE_INPUT_FIXTURES_DIR")).
 */

#include "ri.h"
#include "tileSource.h"

#include <cstdio>
#include <cstdlib>
#include <string>

#define CHECK(cond)                                                              \
    do {                                                                         \
        if (!(cond)) {                                                          \
            fprintf(stderr, "CHECK FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); \
            abort();                                                             \
        }                                                                        \
    } while (0)

static std::string fixturesDir() {
    const char *dir = getenv("IMAGE_INPUT_FIXTURES_DIR");
    return dir ? dir : "tests/unit/image_input/fixtures";
}

static void checkTile(CTileSource *source, int tileX, int tileY, int tileWidth, int tileHeight) {
    unsigned char tile[32 * 32 * 3];
    CHECK(source->fetchTile(tileX, tileY, tile) == true);

    const int x0 = tileX * tileWidth;
    const int y0 = tileY * tileHeight;

    for (int y = 0; y < tileHeight; y++) {
        for (int x = 0; x < tileWidth; x++) {
            const unsigned char *p = &tile[(y * tileWidth + x) * 3];
            const int px = x0 + x;
            const int py = y0 + y;
            const unsigned char r = (unsigned char)((3 * px + 10) % 256);
            const unsigned char g = (unsigned char)((5 * py + 20) % 256);
            const unsigned char b = (unsigned char)((px + 2 * py + 30) % 256);

            CHECK(p[0] == r);
            CHECK(p[1] == g);
            CHECK(p[2] == b);
        }
    }
}

int main() {
    // CSynthesizedTileSource's construction path (adjustSize<T>/
    // filterScaleImage<T>/filterImage<T>, T006/T007) allocates from
    // CRenderer::globalMemory, and CImageInput::open()'s own failure
    // paths call error(), both of which depend on renderer-global state
    // (CRenderer::globalMemory, the global renderMan singleton) that
    // starts NULL/unset and is otherwise only initialized inside
    // CRenderer::beginRenderer() -- a full RIB-parse/render lifecycle
    // this standalone unit test has no reason to pull in, unlike
    // CTiffTileSource's own test (test_tile_source_tiff_info.cpp), which
    // never touches the arena or calls error() at all. RiBegin(RI_NULL)/
    // RiEnd() is the established, minimal-context way to get both
    // initialized without a real render, matching the exact pattern
    // test_image_input_png.cpp/test_image_input_tiff.cpp already use for
    // the same reason.
    RiBegin(RI_NULL);

    const std::string path = fixturesDir() + "/large_rgb8.png";

    CTileSource *source = createSynthesizedTileSource(path.c_str(), 0);
    CHECK(source != nullptr);

    CTileLevelInfo info;
    source->info(info);

    CHECK(info.width == 512);
    CHECK(info.height == 512);
    CHECK(info.tileWidth == 32);  // DEFAULT_TILE_SIZE, ri_config.h -- fixed
                                  // per level, not clamped to the level's
                                  // own dimensions (research.md SS2)
    CHECK(info.tileHeight == 32);
    CHECK(info.numChannels == 3);
    CHECK(info.bitsPerSample == 8);
    CHECK(!info.isFloatFormat);

    // Tile (0,0): pixels (0..31, 0..31).
    checkTile(source, 0, 0, info.tileWidth, info.tileHeight);

    // A tile away from the origin, to catch a tileX/tileY-to-pixel-offset
    // bug tile (0,0) alone couldn't: pixels (64..95, 96..127).
    checkTile(source, 2, 3, info.tileWidth, info.tileHeight);

    delete source;

    // Level 1: the next mip level down -- half the base level's
    // resolution (box-filter reduction of level 0, appendPyramid()'s
    // exact 2x2-block-average math replicated in memory).
    source = createSynthesizedTileSource(path.c_str(), 1);
    CHECK(source != nullptr);

    source->info(info);

    CHECK(info.width == 256);
    CHECK(info.height == 256);
    CHECK(info.tileWidth == 32);
    CHECK(info.tileHeight == 32);
    CHECK(info.numChannels == 3);
    CHECK(info.bitsPerSample == 8);
    CHECK(!info.isFloatFormat);

    delete source;

    // An out-of-range level must fail closed, not crash (contract rule 5
    // territory, applied to the factory itself).
    CHECK(createSynthesizedTileSource(path.c_str(), 999) == nullptr);

    // An unrecognized/nonexistent source must fail closed too.
    CHECK(createSynthesizedTileSource("does-not-exist.png", 0) == nullptr);

    printf("test_tile_source_synthesized_info: ALL PASS\n");

    RiEnd();

    return 0;
}
