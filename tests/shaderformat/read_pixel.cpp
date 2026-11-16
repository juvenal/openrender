/**
 * tests/shaderformat/read_pixel.cpp
 *
 * Spec 033 (ShaderFormat_Precedence): prints one pixel of a 32-bit float TIFF as "r g b a" with six
 * decimals, or "none" when the file does not exist or cannot be read. The driver records the text.
 *
 * Usage: shaderformat_read_pixel <tif> <x> <y>
 */
#include <cstdio>
#include <cstdlib>

#include "../visual/depth_tiff.h"

int main(int argc, char *argv[]) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s <tif> <x> <y>\n", argv[0]);
        return 2;
    }
    FILE *f = fopen(argv[1], "rb");
    if (f == nullptr) {
        printf("none\n");
        return 0;
    }
    fclose(f);
    DepthImage img;
    if (!readFloatTiff(argv[1], img)) {
        printf("unreadable\n");
        return 0;
    }
    const uint32_t x = (uint32_t)atoi(argv[2]), y = (uint32_t)atoi(argv[3]);
    if (x >= img.width || y >= img.height) {
        printf("outside\n");
        return 0;
    }
    const float *p = &img.z[((size_t)y * img.width + x) * img.channels];
    for (uint16_t c = 0; c < img.channels && c < 4; ++c)
        printf("%s%.6f", c ? " " : "", (double)p[c]);
    printf("\n");
    return 0;
}
