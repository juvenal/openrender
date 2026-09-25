/**
 * tests/unit/image_input/test_image_input_tiff.cpp
 *
 * Unit test for CTiffImageInput (spec 018, T005): asserts the CImageInput/
 * CImageInfo contract shape compiles and that CTiffImageInput decodes the
 * tiny/large TIFF fixtures to their exact expected pixel values, per the
 * formulas in tests/unit/image_input/fixtures/FIXTURES.md.
 */

#include "imageInputTiff.h"
#include "ri.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

// assert() compiles away under -DNDEBUG (Release builds), which silently
// no-ops these checks and leaves the values that fed them "unused" --
// tripping -Werror,-Wunused-variable on a Release build. Use an
// always-active check instead.
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "CHECK FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); abort(); } } while (0)

static std::string fixturesDir() {
    const char *dir = getenv("IMAGE_INPUT_FIXTURES_DIR");
    return dir ? dir : "tests/unit/image_input/fixtures";
}

// R(x,y) = (3x+10) mod 256, G(x,y) = (5y+20) mod 256, B(x,y) = (x+2y+30) mod 256
static void checkRgb(const char *fixture, int w, int h) {
    std::string path = fixturesDir() + "/" + fixture;

    CTiffImageInput input;
    CImageInfo info;
    CHECK(input.open(path.c_str(), info));
    CHECK(info.width == w);
    CHECK(info.height == h);
    CHECK(info.numChannels == 3);
    CHECK(info.bitsPerSample == 8);
    CHECK(!info.isFloatFormat);

    std::vector<uint8_t> data(w * h * 3);
    CHECK(input.readImage(data.data()));

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t *px = &data[(y * w + x) * 3];
            uint8_t expectedR = (uint8_t)((3 * x + 10) % 256);
            uint8_t expectedG = (uint8_t)((5 * y + 20) % 256);
            uint8_t expectedB = (uint8_t)((x + 2 * y + 30) % 256);
            CHECK(px[0] == expectedR);
            CHECK(px[1] == expectedG);
            CHECK(px[2] == expectedB);
        }
    }

    input.close();
    printf("checkRgb(%s) OK\n", fixture);
}

int main() {
    // error() (called on decode failure) depends on the global renderMan
    // singleton, initialized between RiBegin()/RiEnd() -- same pattern
    // already used by tests/unit/blobby's tests.
    RiBegin(RI_NULL);

    checkRgb("tiny_rgb.tif", 4, 4);
    checkRgb("large_rgb.tif", 512, 512);

    // Unsupported/missing file must fail cleanly, not crash.
    {
        CTiffImageInput input;
        CImageInfo info;
        CHECK(!input.open((fixturesDir() + "/does_not_exist.tif").c_str(), info));
    }

    RiEnd();

    printf("test_image_input_tiff: ALL PASS\n");
    return 0;
}
