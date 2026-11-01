/**
 * Project: openRender
 *
 * File: pixel_compare.cpp
 *
 * Description:
 *   Spec 037 (versioned plug-in ABIs): plugin_abi_pixel_compare, the decoded-pixel
 *   reference tool of the display and procedure baselines (FR-021, SC-004, SC-013).
 *
 *   Commands:
 *     decode <image> <out.pix>   decode an image and write the decoded samples
 *     compare <image> <ref.pix>  decode an image and require the samples to be exactly those of
 *                                the reference: zero differing samples (exit 0), otherwise print
 *                                the number of differing samples and the largest difference
 *     png-gamma <png>            print the PNG gAMA chunk value (integer, gamma * 100000), or "none"
 *
 *   Decoded pixels, not bytes: TIFF and EXR headers carry dates and software strings. The decoder
 *   is spec 018's CImageInput (TIFF, PNG, OpenEXR, RGBE), picked by the *content* of the file, not
 *   its extension (a display line may write TIFF data into a name ending in .png). Samples are
 *   compared exactly as the decoder delivers them: 8-bit and 16-bit integers, 32-bit floats.
 *
 *   The reference format ("pix") is a text header line
 *       ORPIX1 <width> <height> <channels> <bitsPerSample> <isFloat>
 *   followed by width * height * channels samples in the decoder's native type and the host's
 *   byte order (little-endian on every supported platform).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

#include "imageInput.h"

namespace {

    struct Image {
            CImageInfo info;
            std::vector<uint8_t> bytes;
    };

    // The extension createImageInput() needs, chosen from the first bytes of the file.
    const char *extensionForContent(const std::string &path) {
        FILE *f = fopen(path.c_str(), "rb");
        if (f == nullptr)
            return nullptr;
        unsigned char head[16] = {0};
        const size_t n = fread(head, 1, sizeof(head), f);
        fclose(f);
        if (n >= 4 && ((head[0] == 'I' && head[1] == 'I' && head[2] == 42 && head[3] == 0) || (head[0] == 'M' && head[1] == 'M' && head[2] == 0 && head[3] == 42)))
            return ".tif";
        if (n >= 8 && head[0] == 0x89 && head[1] == 'P' && head[2] == 'N' && head[3] == 'G')
            return ".png";
        if (n >= 4 && head[0] == 0x76 && head[1] == 0x2f && head[2] == 0x31 && head[3] == 0x01)
            return ".exr";
        if (n >= 2 && head[0] == '#' && head[1] == '?')
            return ".hdr";
        return nullptr;
    }

    bool decode(const std::string &path, Image &out) {
        const char *ext = extensionForContent(path);
        if (ext == nullptr) {
            fprintf(stderr, "plugin_abi_pixel_compare: %s: not a TIFF, PNG, OpenEXR or RGBE file (or missing)\n", path.c_str());
            return false;
        }
        std::unique_ptr<CImageInput> input(createImageInput((std::string("content") + ext).c_str()));
        if (input == nullptr) {
            fprintf(stderr, "plugin_abi_pixel_compare: no decoder for %s content in this build\n", ext);
            return false;
        }
        if (!input->open(path.c_str(), out.info)) {
            fprintf(stderr, "plugin_abi_pixel_compare: %s: cannot open\n", path.c_str());
            return false;
        }
        const size_t bytes = (size_t)out.info.width * out.info.height * out.info.numChannels * (out.info.bitsPerSample / 8);
        out.bytes.assign(bytes, 0);
        if (!input->readImage(out.bytes.data())) {
            fprintf(stderr, "plugin_abi_pixel_compare: %s: cannot read\n", path.c_str());
            return false;
        }
        input->close();
        return true;
    }

    bool writePix(const std::string &path, const Image &image) {
        FILE *f = fopen(path.c_str(), "wb");
        if (f == nullptr) {
            fprintf(stderr, "plugin_abi_pixel_compare: cannot write %s\n", path.c_str());
            return false;
        }
        fprintf(f, "ORPIX1 %d %d %d %d %d\n", image.info.width, image.info.height, image.info.numChannels, image.info.bitsPerSample, image.info.isFloatFormat ? 1 : 0);
        const bool ok = fwrite(image.bytes.data(), 1, image.bytes.size(), f) == image.bytes.size();
        fclose(f);
        return ok;
    }

    bool readPix(const std::string &path, Image &image) {
        FILE *f = fopen(path.c_str(), "rb");
        if (f == nullptr) {
            fprintf(stderr, "plugin_abi_pixel_compare: cannot read reference %s\n", path.c_str());
            return false;
        }
        int isFloat = 0;
        if (fscanf(f, "ORPIX1 %d %d %d %d %d", &image.info.width, &image.info.height, &image.info.numChannels, &image.info.bitsPerSample, &isFloat) != 5 || fgetc(f) != '\n') {
            fprintf(stderr, "plugin_abi_pixel_compare: %s: bad header\n", path.c_str());
            fclose(f);
            return false;
        }
        image.info.isFloatFormat = isFloat != 0;
        const size_t bytes = (size_t)image.info.width * image.info.height * image.info.numChannels * (image.info.bitsPerSample / 8);
        image.bytes.assign(bytes, 0);
        const bool ok = fread(image.bytes.data(), 1, bytes, f) == bytes;
        fclose(f);
        if (!ok)
            fprintf(stderr, "plugin_abi_pixel_compare: %s: truncated\n", path.c_str());
        return ok;
    }

    double sampleAt(const Image &image, size_t index) {
        const uint8_t *p = image.bytes.data();
        if (image.info.isFloatFormat && image.info.bitsPerSample == 32) {
            float v;
            memcpy(&v, p + index * 4, 4);
            return v;
        }
        if (image.info.bitsPerSample == 16) {
            uint16_t v;
            memcpy(&v, p + index * 2, 2);
            return v;
        }
        if (image.info.bitsPerSample == 32) {
            uint32_t v;
            memcpy(&v, p + index * 4, 4);
            return v;
        }
        return p[index];
    }

    int compare(const std::string &imagePath, const std::string &referencePath) {
        Image actual, reference;
        if (!decode(imagePath, actual) || !readPix(referencePath, reference))
            return 2;

        if (actual.info.width != reference.info.width || actual.info.height != reference.info.height || actual.info.numChannels != reference.info.numChannels || actual.info.bitsPerSample != reference.info.bitsPerSample || actual.info.isFloatFormat != reference.info.isFloatFormat) {
            printf("DIFFER: layout %dx%dx%d %d-bit%s, reference %dx%dx%d %d-bit%s\n", actual.info.width, actual.info.height, actual.info.numChannels, actual.info.bitsPerSample, actual.info.isFloatFormat ? " float" : "", reference.info.width, reference.info.height, reference.info.numChannels, reference.info.bitsPerSample, reference.info.isFloatFormat ? " float" : "");
            return 1;
        }

        const size_t samples = (size_t)actual.info.width * actual.info.height * actual.info.numChannels;
        size_t differing = 0;
        double largest = 0;
        for (size_t i = 0; i < samples; ++i) {
            const size_t size = (size_t)actual.info.bitsPerSample / 8;
            if (memcmp(actual.bytes.data() + i * size, reference.bytes.data() + i * size, size) != 0) {
                ++differing;
                const double d = std::fabs(sampleAt(actual, i) - sampleAt(reference, i));
                if (d > largest || std::isnan(d))
                    largest = d;
            }
        }
        if (differing != 0) {
            printf("DIFFER: %zu of %zu samples differ (largest absolute difference %g)\n", differing, samples, largest);
            return 1;
        }
        printf("identical: %zu samples\n", samples);
        return 0;
    }

    int pngGamma(const std::string &path) {
        FILE *f = fopen(path.c_str(), "rb");
        if (f == nullptr) {
            fprintf(stderr, "plugin_abi_pixel_compare: cannot read %s\n", path.c_str());
            return 2;
        }
        unsigned char signature[8];
        if (fread(signature, 1, 8, f) != 8 || signature[1] != 'P' || signature[2] != 'N' || signature[3] != 'G') {
            fprintf(stderr, "plugin_abi_pixel_compare: %s: not a PNG\n", path.c_str());
            fclose(f);
            return 2;
        }
        for (;;) {
            unsigned char header[8];
            if (fread(header, 1, 8, f) != 8)
                break;
            const uint32_t length = ((uint32_t)header[0] << 24) | ((uint32_t)header[1] << 16) | ((uint32_t)header[2] << 8) | header[3];
            if (memcmp(header + 4, "gAMA", 4) == 0 && length == 4) {
                unsigned char data[4];
                if (fread(data, 1, 4, f) != 4)
                    break;
                const uint32_t value = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) | ((uint32_t)data[2] << 8) | data[3];
                printf("%u\n", value);
                fclose(f);
                return 0;
            }
            if (memcmp(header + 4, "IDAT", 4) == 0 || memcmp(header + 4, "IEND", 4) == 0)
                break;
            if (fseek(f, (long)length + 4, SEEK_CUR) != 0)
                break;
        }
        fclose(f);
        printf("none\n");
        return 0;
    }

} // namespace

int main(int argc, char **argv) {
    if (argc == 4 && strcmp(argv[1], "decode") == 0) {
        Image image;
        if (!decode(argv[2], image) || !writePix(argv[3], image))
            return 2;
        return 0;
    }
    if (argc == 4 && strcmp(argv[1], "compare") == 0)
        return compare(argv[2], argv[3]);
    if (argc == 3 && strcmp(argv[1], "png-gamma") == 0)
        return pngGamma(argv[2]);
    fprintf(stderr, "usage: plugin_abi_pixel_compare decode <image> <out.pix> | compare <image> <ref.pix> | png-gamma <png>\n");
    return 2;
}
