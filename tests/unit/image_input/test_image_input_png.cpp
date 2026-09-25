/**
 * tests/unit/image_input/test_image_input_png.cpp
 *
 * Unit test for CPngImageInput (spec 018, T013): asserts the CImageInput/
 * CImageInfo contract shape compiles and that CPngImageInput decodes the
 * tiny/large PNG fixtures (RGB, RGBA, 16-bit grayscale, grayscale,
 * grayscale+alpha) to their exact expected pixel values, per the formulas
 * in tests/unit/image_input/fixtures/FIXTURES.md, plus a negative test that
 * indexed/palette PNGs are rejected rather than decoded.
 */

#include "imageInputPng.h"
#include "ri.h"

#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "CHECK FAILED: %s at %s:%d\n", #cond, __FILE__, __LINE__); abort(); } } while (0)

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

static std::string fixturesDir() {
    const char *dir = getenv("IMAGE_INPUT_FIXTURES_DIR");
    return dir ? dir : "tests/unit/image_input/fixtures";
}

static uint8_t expectR(int x, int) { return (uint8_t)((3 * x + 10) % 256); }
static uint8_t expectG(int, int y) { return (uint8_t)((5 * y + 20) % 256); }
static uint8_t expectB(int x, int y) { return (uint8_t)((x + 2 * y + 30) % 256); }
static uint8_t expectA(int x, int y) { return (uint8_t)((7 * x + 11 * y + 40) % 256); }
static uint8_t expectGray(int x, int y) { return (uint8_t)((x + y) % 256); }
static uint16_t expectGray16(int x, int y) { return (uint16_t)((97 * x + 131 * y) % 65536); }

static void checkRgb(const char *fixture, int w, int h) {
    std::string path = fixturesDir() + "/" + fixture;

    CPngImageInput input;
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
            CHECK(px[0] == expectR(x, y));
            CHECK(px[1] == expectG(x, y));
            CHECK(px[2] == expectB(x, y));
        }
    }

    input.close();
    printf("checkRgb(%s) OK\n", fixture);
}

static void checkRgba(const char *fixture, int w, int h) {
    std::string path = fixturesDir() + "/" + fixture;

    CPngImageInput input;
    CImageInfo info;
    CHECK(input.open(path.c_str(), info));
    CHECK(info.numChannels == 4);
    CHECK(info.bitsPerSample == 8);

    std::vector<uint8_t> data(w * h * 4);
    CHECK(input.readImage(data.data()));

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t *px = &data[(y * w + x) * 4];
            CHECK(px[0] == expectR(x, y));
            CHECK(px[1] == expectG(x, y));
            CHECK(px[2] == expectB(x, y));
            CHECK(px[3] == expectA(x, y));
        }
    }

    input.close();
    printf("checkRgba(%s) OK\n", fixture);
}

static void checkGray(const char *fixture, int w, int h) {
    std::string path = fixturesDir() + "/" + fixture;

    CPngImageInput input;
    CImageInfo info;
    CHECK(input.open(path.c_str(), info));
    CHECK(info.numChannels == 1);
    CHECK(info.bitsPerSample == 8);

    std::vector<uint8_t> data(w * h);
    CHECK(input.readImage(data.data()));

    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            CHECK(data[y * w + x] == expectGray(x, y));

    input.close();
    printf("checkGray(%s) OK\n", fixture);
}

static void checkGrayAlpha(const char *fixture, int w, int h) {
    std::string path = fixturesDir() + "/" + fixture;

    CPngImageInput input;
    CImageInfo info;
    CHECK(input.open(path.c_str(), info));
    CHECK(info.numChannels == 2);
    CHECK(info.bitsPerSample == 8);

    std::vector<uint8_t> data(w * h * 2);
    CHECK(input.readImage(data.data()));

    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++) {
            uint8_t *px = &data[(y * w + x) * 2];
            CHECK(px[0] == expectGray(x, y));
            CHECK(px[1] == expectA(x, y));
        }
    }

    input.close();
    printf("checkGrayAlpha(%s) OK\n", fixture);
}

static void checkGray16(const char *fixture, int w, int h) {
    std::string path = fixturesDir() + "/" + fixture;

    CPngImageInput input;
    CImageInfo info;
    CHECK(input.open(path.c_str(), info));
    CHECK(info.numChannels == 1);
    CHECK(info.bitsPerSample == 16);

    std::vector<uint16_t> data(w * h);
    CHECK(input.readImage(data.data()));

    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            CHECK(data[y * w + x] == expectGray16(x, y));

    input.close();
    printf("checkGray16(%s) OK\n", fixture);
}

static void checkIndexedRejected(const char *fixture) {
    std::string path = fixturesDir() + "/" + fixture;

    CPngImageInput input;
    CImageInfo info;
    CHECK(!input.open(path.c_str(), info));

    printf("checkIndexedRejected(%s) OK\n", fixture);
}

int main() {
    // error() (called on decode failure) depends on the global renderMan
    // singleton, initialized between RiBegin()/RiEnd() -- same pattern
    // already used by test_image_input_tiff.cpp.
    RiBegin(RI_NULL);

    checkRgb("tiny_rgb8.png", 4, 4);
    checkRgb("large_rgb8.png", 512, 512);

    checkRgba("tiny_rgba8.png", 4, 4);
    checkRgba("large_rgba8.png", 512, 512);

    checkGray("tiny_gray8.png", 4, 4);
    checkGray("large_gray8.png", 512, 512);

    checkGrayAlpha("tiny_gray_alpha8.png", 4, 4);
    checkGrayAlpha("large_gray_alpha8.png", 512, 512);

    checkGray16("tiny_gray16.png", 4, 4);
    checkGray16("large_gray16.png", 512, 512);

    checkIndexedRejected("tiny_indexed.png");
    checkIndexedRejected("large_indexed.png");

    // Unsupported/missing file must fail cleanly, not crash.
    {
        CPngImageInput input;
        CImageInfo info;
        CHECK(!input.open((fixturesDir() + "/does_not_exist.png").c_str(), info));
    }

    RiEnd();

    printf("test_image_input_png: ALL PASS\n");
    return 0;
}
