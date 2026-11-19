// tests/unit/imageio/tools/gen_bake_fixtures.cpp -- the source images of the bake tests (spec 048, Phase 6, T087).
//
// A second generator, so the approved first one (gen_fixtures.cpp) is not edited. A test-only tool run by hand through cpujob
// (`gen_bake_fixtures <output-dir>`, normally tests/unit/imageio/fixtures/bake); its TIFFs are checked in and are not regenerated
// by the suite. Every sample follows the closed-form formulas of fixtures/FIXTURES.md (8-bit, 16-bit and float, any channel count),
// and the shadow maps carry the two Pixar matrices. The list of files is the source column of bake_cases.h.
#include <tiffio.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <vector>

#define MUST(cond) \
    do { \
        if (!(cond)) { \
            fprintf(stderr, "gen_bake_fixtures: %s failed at %s:%d\n", #cond, __FILE__, __LINE__); \
            exit(1); \
        } \
    } while (0)

static uint8_t u8At(int x, int y, int c) { return (uint8_t)((13 * x + 7 * y + 31 * c + 5) % 256); }
static uint16_t u16At(int x, int y, int c) { return (uint16_t)((1021 * x + 517 * y + 4099 * c + 7) % 65536); }
static float f32At(int x, int y, int c) { return (float)(((x * 7 + y * 13 + c * 5) % 1000)) / 997.0f - 0.25f; }

static int written = 0;

// An uncompressed scanline TIFF: MINISBLACK for 1 or 2 channels, RGB for 3 or 4, MINISBLACK with the rest unspecified extra samples
// for 5 or more; unassociated alpha for 2 and 4. `matrices` adds the Pixar world-to-camera and world-to-screen tags of a shadow map.
static void writeTiff(const std::string &path, int w, int h, int spp, int bps, bool matrices) {
    TIFF *t = TIFFOpen(path.c_str(), "w");
    MUST(t != nullptr);
    TIFFSetField(t, TIFFTAG_IMAGEWIDTH, (uint32_t)w);
    TIFFSetField(t, TIFFTAG_IMAGELENGTH, (uint32_t)h);
    TIFFSetField(t, TIFFTAG_SAMPLESPERPIXEL, (uint16_t)spp);
    TIFFSetField(t, TIFFTAG_BITSPERSAMPLE, (uint16_t)bps);
    TIFFSetField(t, TIFFTAG_SAMPLEFORMAT, (uint16_t)(bps == 32 ? SAMPLEFORMAT_IEEEFP : SAMPLEFORMAT_UINT));
    TIFFSetField(t, TIFFTAG_PHOTOMETRIC, (uint16_t)(spp == 3 || spp == 4 ? PHOTOMETRIC_RGB : PHOTOMETRIC_MINISBLACK));
    TIFFSetField(t, TIFFTAG_PLANARCONFIG, (uint16_t)PLANARCONFIG_CONTIG);
    TIFFSetField(t, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
    TIFFSetField(t, TIFFTAG_COMPRESSION, COMPRESSION_NONE);
    TIFFSetField(t, TIFFTAG_ROWSPERSTRIP, (uint32_t)4);
    if (spp == 2 || spp == 4) {
        uint16_t extra[1] = {EXTRASAMPLE_UNASSALPHA};
        TIFFSetField(t, TIFFTAG_EXTRASAMPLES, (uint16_t)1, extra);
    } else if (spp >= 5) {
        std::vector<uint16_t> extra((size_t)spp - 1, EXTRASAMPLE_UNSPECIFIED);
        TIFFSetField(t, TIFFTAG_EXTRASAMPLES, (uint16_t)(spp - 1), extra.data());
    }
    if (matrices) {
        // two distinct, non-identity matrices (a rotation about y plus a translation, and a projection-like scale)
        float w2c[16] = {0.8f, 0, -0.6f, 0, 0, 1, 0, 0, 0.6f, 0, 0.8f, 0, 1.5f, -2.25f, 3.125f, 1};
        float w2s[16] = {2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1.0625f, 1, 0.5f, -0.5f, 0, 0};
        TIFFSetField(t, TIFFTAG_PIXAR_MATRIX_WORLDTOCAMERA, w2c);
        TIFFSetField(t, TIFFTAG_PIXAR_MATRIX_WORLDTOSCREEN, w2s);
    }
    const int bytes = bps / 8;
    std::vector<uint8_t> row((size_t)w * spp * bytes);
    for (int y = 0; y < h; y++) {
        for (int x = 0; x < w; x++)
            for (int c = 0; c < spp; c++) {
                uint8_t *d = &row[((size_t)x * spp + c) * bytes];
                if (bps == 8) {
                    *d = u8At(x, y, c);
                } else if (bps == 16) {
                    uint16_t v = u16At(x, y, c);
                    memcpy(d, &v, 2);
                } else {
                    float v = f32At(x, y, c);
                    memcpy(d, &v, 4);
                }
            }
        MUST(TIFFWriteScanline(t, row.data(), (uint32_t)y, 0) >= 0);
    }
    TIFFClose(t);
    written++;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: gen_bake_fixtures <output-dir>\n");
        return 2;
    }
    const std::string dir = argv[1];
    mkdir(dir.c_str(), 0755);
    auto name = [&](const char *prefix, const char *type, int spp) { return dir + "/" + prefix + "_" + type + "_" + std::to_string(spp) + ".tif"; };

    // plain textures, 40x24
    for (int spp = 1; spp <= 4; spp++)
        writeTiff(name("tex40", "u8", spp), 40, 24, spp, 8, false);
    for (int spp = 1; spp <= 4; spp++)
        writeTiff(name("tex40", "f32", spp), 40, 24, spp, 32, false);
    for (int spp = 1; spp <= 4; spp++)
        writeTiff(name("tex40", "u16", spp), 40, 24, spp, 16, false);
    writeTiff(name("tex40", "u8", 5), 40, 24, 5, 8, false);
    writeTiff(name("tex40", "u8", 6), 40, 24, 6, 8, false);
    writeTiff(name("tex40", "f32", 5), 40, 24, 5, 32, false);
    writeTiff(name("tex40", "f32", 6), 40, 24, 6, 32, false);
    // a power-of-two source and sources with a 1-pixel axis
    writeTiff(dir + "/pot64_u8_3.tif", 64, 64, 3, 8, false);
    writeTiff(dir + "/thin_1x16_u8_3.tif", 1, 16, 3, 8, false);
    writeTiff(dir + "/thin_16x1_f32_1.tif", 16, 1, 1, 32, false);
    // environment sources, 32x32
    writeTiff(name("env", "u8", 3), 32, 32, 3, 8, false);
    writeTiff(name("env", "u16", 1), 32, 32, 1, 16, false);
    writeTiff(name("env", "f32", 3), 32, 32, 3, 32, false);
    writeTiff(name("env", "u16", 3), 32, 32, 3, 16, false);
    // shadow-map sources, 32x32, with the two matrices
    writeTiff(name("shd", "u8", 1), 32, 32, 1, 8, true);
    writeTiff(name("shd", "u16", 1), 32, 32, 1, 16, true);
    writeTiff(name("shd", "f32", 1), 32, 32, 1, 32, true);
    writeTiff(name("shd", "u16", 3), 32, 32, 3, 16, true);

    printf("gen_bake_fixtures: %d files written under %s\n", written, dir.c_str());
    return 0;
}
