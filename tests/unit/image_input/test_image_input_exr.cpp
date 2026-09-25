/**
 * tests/unit/image_input/test_image_input_exr.cpp
 *
 * Unit test for COpenExrImageInput (spec 018, T019): asserts the
 * CImageInput/CImageInfo contract shape compiles and that
 * COpenExrImageInput decodes the tiny/large OpenEXR fixtures (RGB, RGBA,
 * luminance, HALF-precision RGB) to their exact expected pixel values, per
 * the formulas in tests/unit/image_input/fixtures/FIXTURES.md, plus
 * negative tests that multi-part files and unsupported channel layouts are
 * rejected rather than decoded (or guessed at).
 *
 * Only built when HAVE_OPENEXR (see tests/unit/image_input/CMakeLists.txt).
 */

#include "imageInputExr.h"
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

static float expectR(int x, int y, int w, int h) { return 0.1f + 0.4f * fx(x, w) + 0.1f * fy(y, h); }
static float expectG(int x, int y, int w, int) { return 0.2f + 0.3f * fx(x, w) + 2.5f * (x == 0 && y == 0); }
static float expectB(int x, int y, int w, int h) { return 0.3f - 0.2f * fx(x, w) - 0.1f * fy(y, h); }
static float expectA(int x, int, int w, int) { return 0.5f + 0.4f * fx(x, w); }
static float expectY(int x, int y, int w, int h) { return 0.4f + 0.4f * fx(x, w) + 0.2f * fy(y, h); }

static void checkRgb(const char *fixture, int w, int h, float eps) {
    std::string path = fixturesDir() + "/" + fixture;

    COpenExrImageInput input;
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
            CHECK(fabsf(px[0] - expectR(x, y, w, h)) < eps);
            CHECK(fabsf(px[1] - expectG(x, y, w, h)) < eps);
            CHECK(fabsf(px[2] - expectB(x, y, w, h)) < eps);
        }
    }

    // Prove no clamping: G(0,0) is deliberately > 1.0.
    CHECK(data[1] > 1.0f);

    input.close();
    printf("checkRgb(%s) OK\n", fixture);
}

static void checkRgba(const char *fixture, int w, int h) {
    std::string path = fixturesDir() + "/" + fixture;

    COpenExrImageInput input;
    CImageInfo info;
    CHECK(input.open(path.c_str(), info));
    CHECK(info.numChannels == 4);
    CHECK(info.bitsPerSample == 32);
    CHECK(info.isFloatFormat);

    std::vector<float> data((size_t)w * h * 4);
    CHECK(input.readImage(data.data()));

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            float *px = &data[((size_t)y * w + x) * 4];
            CHECK(fabsf(px[0] - expectR(x, y, w, h)) < 1e-5f);
            CHECK(fabsf(px[1] - expectG(x, y, w, h)) < 1e-5f);
            CHECK(fabsf(px[2] - expectB(x, y, w, h)) < 1e-5f);
            CHECK(fabsf(px[3] - expectA(x, y, w, h)) < 1e-5f);
        }
    }

    input.close();
    printf("checkRgba(%s) OK\n", fixture);
}

static void checkLuminance(const char *fixture, int w, int h) {
    std::string path = fixturesDir() + "/" + fixture;

    COpenExrImageInput input;
    CImageInfo info;
    CHECK(input.open(path.c_str(), info));
    CHECK(info.numChannels == 1);
    CHECK(info.bitsPerSample == 32);
    CHECK(info.isFloatFormat);

    std::vector<float> data((size_t)w * h);
    CHECK(input.readImage(data.data()));

    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            CHECK(fabsf(data[(size_t)y * w + x] - expectY(x, y, w, h)) < 1e-5f);

    input.close();
    printf("checkLuminance(%s) OK\n", fixture);
}

static void checkRejected(const char *fixture) {
    std::string path = fixturesDir() + "/" + fixture;

    COpenExrImageInput input;
    CImageInfo info;
    CHECK(!input.open(path.c_str(), info));

    printf("checkRejected(%s) OK\n", fixture);
}

int main() {
    // error() (called on decode failure) depends on the global renderMan
    // singleton, initialized between RiBegin()/RiEnd() -- same pattern
    // already used by test_image_input_tiff.cpp/test_image_input_png.cpp.
    RiBegin(RI_NULL);

    // FLOAT-stored channels: exact-ish (tiny float rounding from the
    // fixture generator's own arithmetic, not from our decode path).
    checkRgb("tiny_rgb.exr", 4, 4, 1e-5f);
    checkRgb("large_rgb.exr", 512, 512, 1e-5f);

    checkRgba("tiny_rgba.exr", 4, 4);
    checkRgba("large_rgba.exr", 512, 512);

    checkLuminance("tiny_luminance.exr", 4, 4);
    checkLuminance("large_luminance.exr", 512, 512);

    // HALF-stored channels: promoted to 32-bit float, but only accurate to
    // half-precision rounding (~3 significant digits) -- looser tolerance.
    checkRgb("tiny_half.exr", 4, 4, 0.01f);
    checkRgb("large_half.exr", 512, 512, 0.01f);

    // Negative tests: multi-part (unconditionally rejected, even with an
    // otherwise-valid single-part-equivalent layout) and unsupported
    // channel sets (R,G,B,Z -- 4 channels but not RGBA).
    checkRejected("tiny_multipart.exr");
    checkRejected("large_multipart.exr");
    checkRejected("tiny_unsupported_channels.exr");
    checkRejected("large_unsupported_channels.exr");

    // Unsupported/missing file must fail cleanly, not crash.
    {
        COpenExrImageInput input;
        CImageInfo info;
        CHECK(!input.open((fixturesDir() + "/does_not_exist.exr").c_str(), info));
    }

    RiEnd();

    printf("test_image_input_exr: ALL PASS\n");
    return 0;
}
