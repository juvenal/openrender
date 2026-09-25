/**
 * tests/unit/image_input/test_image_input_rgbe.cpp
 *
 * Unit test for CRgbeImageInput (spec 018, T025): asserts CRgbeImageInput
 * decodes the tiny/large RGBE (.hdr) fixtures to their expected pixel
 * values (within RGBE's lossy shared-exponent encoding tolerance), per the
 * formulas in tests/unit/image_input/fixtures/FIXTURES.md. A correct
 * round-trip proves RGBE_ReadHeader()/RGBE_ReadPixels() (display/rgbe/
 * rgbe.h) are genuinely being invoked, not stubbed.
 */

#include "imageInputRgbe.h"
#include "ri.h"

#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "CHECK FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); abort(); } } while (0)

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static std::string fixturesDir() {
    const char *dir = getenv("IMAGE_INPUT_FIXTURES_DIR");
    return dir ? dir : "tests/unit/image_input/fixtures";
}

static float fx(int x, int w) { return w > 1 ? (float)x / (float)(w - 1) : 0.0f; }
static float fy(int y, int h) { return h > 1 ? (float)y / (float)(h - 1) : 0.0f; }

// Same formulas used to generate the OpenEXR fixtures (shared generator).
static float expectR(int x, int y, int w, int h) { return 0.1f + 0.4f * fx(x, w) + 0.1f * fy(y, h); }
static float expectG(int x, int y, int w, int) { return 0.2f + 0.3f * fx(x, w) + 2.5f * (x == 0 && y == 0); }
static float expectB(int x, int y, int w, int h) { return 0.3f - 0.2f * fx(x, w) - 0.1f * fy(y, h); }

// RGBE is a lossy shared-exponent encoding: one 8-bit exponent shared by
// all 3 channels (derived from the pixel's MAX channel), plus an 8-bit
// mantissa per channel. So precision for a given channel is relative to
// that pixel's max(|R|,|G|,|B|), not the channel's own magnitude -- a
// small channel sharing a pixel with a much larger one (like R=0.1 next to
// the deliberate G=2.7 no-clamping spike at (0,0)) is quantized coarsely.
// Tolerance: ~1/128 of the pixel's max channel (2x the raw ~1/256 mantissa
// step, for encode+read rounding), with a small absolute floor.
static bool closeEnough(float actual, float expected, float pixelMax) {
    float tolerance = pixelMax * (1.0f / 128.0f) + 0.002f;
    return fabsf(actual - expected) < tolerance;
}

static void checkRgbe(const char *fixture, int w, int h) {
    std::string path = fixturesDir() + "/" + fixture;

    CRgbeImageInput input;
    CImageInfo info;
    CHECK(input.open(path.c_str(), info));
    CHECK(info.width == w);
    CHECK(info.height == h);
    CHECK(info.numChannels == 3);
    CHECK(info.bitsPerSample == 32);
    CHECK(info.isFloatFormat);

    std::vector<float> data((size_t)w * h * 3);
    CHECK(input.readImage(data.data()));

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float *px = &data[((size_t)y * w + x) * 3];
            float er = expectR(x, y, w, h), eg = expectG(x, y, w, h), eb = expectB(x, y, w, h);
            float pixelMax = fabsf(er);
            if (fabsf(eg) > pixelMax) pixelMax = fabsf(eg);
            if (fabsf(eb) > pixelMax) pixelMax = fabsf(eb);
            CHECK(closeEnough(px[0], er, pixelMax));
            CHECK(closeEnough(px[1], eg, pixelMax));
            CHECK(closeEnough(px[2], eb, pixelMax));
        }
    }

    input.close();
    printf("checkRgbe(%s) OK\n", fixture);
}

int main() {
    // error() (called on decode failure) depends on the global renderMan
    // singleton, initialized between RiBegin()/RiEnd() -- same pattern
    // already used by the other three decoders' tests.
    RiBegin(RI_NULL);

    checkRgbe("tiny.hdr", 4, 4);
    checkRgbe("large.hdr", 512, 512);

    // Unsupported/missing file must fail cleanly, not crash.
    {
        CRgbeImageInput input;
        CImageInfo info;
        CHECK(!input.open((fixturesDir() + "/does_not_exist.hdr").c_str(), info));
    }

    RiEnd();

    printf("test_image_input_rgbe: ALL PASS\n");
    return 0;
}
