// tests/unit/imageio/test_display_parity.cpp -- ImageIO_DisplayParity (spec 048, T103, SC-003, FR-019).
//
// The oiio display driver writes the samples the legacy drivers write. Two modes:
//   synthetic                          drives the legacy driver (file, openexr, rgbe) and oiio through CDisplayHost with the same synthetic
//                                      rows, then decodes both files with the fixed decoder of the format and compares every sample
//   compare <format> <legacy> <oiio>   decodes two files written by a render of the one scene with two displays (run_display_parity.cmake)
//
// Cases: TIFF 8/16/32-bit unsigned (a quantize maximum above 65535)/float, PNG 8/16 (and PNG with no quantize: the legacy PNG default of 0..255), EXR half, RGBE rgb. The synthetic rows include
// exact half ties, half denormals, 65504, 65520, overflow, -0, infinities and NaN (where the format keeps them). Display parameters (FR-019):
// quantize 8 and 16, gamma (samples only: gamma writes a gAMA chunk and changes no sample), TIFF compression lzw/zip/none (and the compression
// tag read back), EXR compression RLE/ZIPS/ZIP/PIZ/PXR24 (UPPERCASE: the legacy driver ignores any other spelling; the compression read back
// from both files), and dither with an amplitude, through the shared file_base code (its dither is a hash of the pixel and channel, so it
// needs no seed and the two drivers agree exactly). Alpha: 4 samples are written as 4 samples. Not compared: TIFF EXTRASAMPLES (the legacy
// driver writes associated alpha, the OIIO one unassociated, see the T108 report), PNG gAMA, EXR chromaticities. 16-bit PNG: the legacy driver writes
// every sample byte-swapped on a little-endian host (GitHub #181, D-PNG-WRITE16, not fixed in spec 048), so the legacy file is decoded with its bytes
// swapped back (decode(..., legacyFile = true)); the oiio file is decoded as it is.
#include "display_harness.h"

using namespace dh;

namespace {

    struct Case {
            const char *name;
            const char *format;      // tiff, png, exr, rgbe
            const char *ext;
            const char *legacyType;  // the display type the legacy module is started as
            const char *legacyEnv;   // FILE_DSPLY, OPENEXR_DSPLY, RGBE_DSPLY
            int ns;
            const char *mode;
            float q[5];
            const char *paramName;   // an extra display parameter ("" for none)
            const char *paramValue;
            bool floatSrc;           // synthetic source: float-like (specials) or quantized
    };

    const float kF[5] = {0, 0, 0, 0, 0};
    const float k8[5] = {0, 255, 0, 255, 0};
    const float k16[5] = {0, 65535, 0, 65535, 0};
    const float k32[5] = {0, 100000, 0, 100000, 0};
    const float k8d[5] = {0, 255, 0, 255, 0.5f};
    const float k16d[5] = {0, 65535, 0, 65535, 0.5f};

#define Q(a) {a[0], a[1], a[2], a[3], a[4]}
    const Case kCases[] = {
        {"tiff8", "tiff", "tif", "file", "FILE_DSPLY", 4, "rgba", Q(k8), "", "", false},
        {"tiff8_rgb", "tiff", "tif", "file", "FILE_DSPLY", 3, "rgb", Q(k8), "", "", false},
        {"tiff16", "tiff", "tif", "file", "FILE_DSPLY", 4, "rgba", Q(k16), "", "", false},
        {"tiff_uint32", "tiff", "tif", "file", "FILE_DSPLY", 4, "rgba", Q(k32), "", "", false}, // a quantize maximum above 65535: the legacy TIFF driver writes 32-bit unsigned
        {"tiff_float", "tiff", "tif", "file", "FILE_DSPLY", 4, "rgba", Q(kF), "", "", true},
        {"png8", "png", "png", "file", "FILE_DSPLY", 4, "rgba", Q(k8), "", "", false},
        {"png8_rgb", "png", "png", "file", "FILE_DSPLY", 3, "rgb", Q(k8), "", "", false},
        {"png16", "png", "png", "file", "FILE_DSPLY", 4, "rgba", Q(k16), "", "", false},
        {"png_default_quantize", "png", "png", "file", "FILE_DSPLY", 4, "rgba", Q(kF), "", "", false}, // the legacy PNG forces 0..255 when quantize is unset
        {"exr_half", "exr", "exr", "openexr", "OPENEXR_DSPLY", 4, "rgba", Q(kF), "", "", true},
        {"exr_half_rgb", "exr", "exr", "openexr", "OPENEXR_DSPLY", 3, "rgb", Q(kF), "", "", true},
        {"rgbe_rgb", "rgbe", "hdr", "rgbe", "RGBE_DSPLY", 3, "rgb", Q(kF), "", "", true},
        // parameters
        {"tiff8_dither", "tiff", "tif", "file", "FILE_DSPLY", 4, "rgba", Q(k8d), "", "", false},
        {"tiff16_dither", "tiff", "tif", "file", "FILE_DSPLY", 4, "rgba", Q(k16d), "", "", false},
        {"png8_dither", "png", "png", "file", "FILE_DSPLY", 4, "rgba", Q(k8d), "", "", false},
        {"png16_dither", "png", "png", "file", "FILE_DSPLY", 4, "rgba", Q(k16d), "", "", false},
        {"tiff8_lzw", "tiff", "tif", "file", "FILE_DSPLY", 4, "rgba", Q(k8), "compression", "lzw", false},
        {"tiff8_zip", "tiff", "tif", "file", "FILE_DSPLY", 4, "rgba", Q(k8), "compression", "zip", false},
        {"tiff8_deflate", "tiff", "tif", "file", "FILE_DSPLY", 4, "rgba", Q(k8), "compression", "deflate", false},
        {"tiff8_none", "tiff", "tif", "file", "FILE_DSPLY", 4, "rgba", Q(k8), "compression", "none", false},
        {"tiff_float_lzw", "tiff", "tif", "file", "FILE_DSPLY", 4, "rgba", Q(kF), "compression", "lzw", true},
        {"tiff_float_zip", "tiff", "tif", "file", "FILE_DSPLY", 4, "rgba", Q(kF), "compression", "zip", true},
        {"tiff_float_none", "tiff", "tif", "file", "FILE_DSPLY", 4, "rgba", Q(kF), "compression", "none", true},
        {"exr_RLE", "exr", "exr", "openexr", "OPENEXR_DSPLY", 4, "rgba", Q(kF), "compression", "RLE", true},
        {"exr_ZIPS", "exr", "exr", "openexr", "OPENEXR_DSPLY", 4, "rgba", Q(kF), "compression", "ZIPS", true},
        {"exr_ZIP", "exr", "exr", "openexr", "OPENEXR_DSPLY", 4, "rgba", Q(kF), "compression", "ZIP", true},
        {"exr_PIZ", "exr", "exr", "openexr", "OPENEXR_DSPLY", 4, "rgba", Q(kF), "compression", "PIZ", true},
        {"exr_PXR24", "exr", "exr", "openexr", "OPENEXR_DSPLY", 4, "rgba", Q(kF), "compression", "PXR24", true},
    };
#undef Q

    Run makeRun(const Case &c, const std::string &module, const std::string &type, const std::string &out) {
        Run r;
        r.module = module;
        r.type = type;
        r.out = out;
        r.mode = c.mode;
        r.ns = c.ns;
        memcpy(r.quantizer, c.q, sizeof(r.quantizer));
        if (c.paramName[0] != '\0')
            r.params.push_back(stringParam(c.paramName, c.paramValue));
        return r;
    }

    // gamma is a display parameter the legacy drivers store for PNG gAMA only: the samples must not move
    void addGamma(Run &r) {
        SDisplayParameterSpec g;
        g.name = "gamma";
        g.type = OR_DISPLAY_PARAM_FLOAT;
        g.numItems = 1;
        g.floats.push_back(2.2f);
        r.params.insert(r.params.begin(), g);
    }

    void runOne(const Case &c, const std::string &dir, bool withGamma) {
        std::string name = std::string(c.name) + (withGamma ? "+gamma" : "");
        const std::string legacyFile = dir + "/" + name + "-legacy." + c.ext;
        const std::string oiioFile = dir + "/" + name + "-oiio." + c.ext;
        const bool rgbe = std::string(c.format) == "rgbe";
        const std::vector<float> src = c.floatSrc ? floatSource(16, 16, c.ns, rgbe) : quantizedSource(16, 16, c.ns);

        Run legacy = makeRun(c, module(c.legacyEnv), c.legacyType, legacyFile);
        Run oiio = makeRun(c, module("OIIO_DSPLY"), "oiio", oiioFile);
        if (withGamma) {
            addGamma(legacy);
            addGamma(oiio);
        }
        if (!runDisplay(legacy, src))
            DFAIL("%s: the legacy display did not start", name.c_str());
        if (!runDisplay(oiio, src))
            DFAIL("%s: the oiio display did not start", name.c_str());

        Img a = decode(c.format, legacyFile, true), b = decode(c.format, oiioFile); // legacy 16-bit PNG: byte-swapped on disk, GitHub #181
        compareImages(name.c_str(), a, b);
        if (a.ok() && b.ok()) {
            // the file keeps what the display wrote: the number of channels and, where the format has them, their names and compression
            if (a.n != c.ns)
                DFAIL("%s: the legacy file has %d channels, expected %d", name.c_str(), a.n, c.ns);
            if (a.names != b.names)
                DFAIL("%s: the channel names differ between the two files", name.c_str());
            if (a.tiffCompression != b.tiffCompression)
                DFAIL("%s: TIFF compression %d (legacy) against %d (oiio)", name.c_str(), a.tiffCompression, b.tiffCompression);
            if (a.exrCompression != b.exrCompression)
                DFAIL("%s: EXR compression %s (legacy) against %s (oiio)", name.c_str(), a.exrCompression.c_str(), b.exrCompression.c_str());
        }
    }

    // The harness checks itself first, with no oiio module: the legacy driver against itself, every case, must decode to identical samples (so a
    // later difference is the oiio driver's and not the decoders' or the tile order's).
    void selfCheck(const std::string &dir) {
        int cases = 0;
        for (const Case &c : kCases) {
#ifndef HAVE_OPENEXR
            if (std::string(c.format) == "exr")
                continue;
#endif
            const bool rgbe = std::string(c.format) == "rgbe";
            const std::vector<float> src = c.floatSrc ? floatSource(16, 16, c.ns, rgbe) : quantizedSource(16, 16, c.ns);
            Run a = makeRun(c, module(c.legacyEnv), c.legacyType, dir + "/self-" + c.name + "-a." + c.ext);
            Run b = makeRun(c, module(c.legacyEnv), c.legacyType, dir + "/self-" + c.name + "-b." + c.ext);
            if (!runDisplay(a, src) || !runDisplay(b, src)) {
                DFAIL("self-check %s: the legacy display did not start", c.name);
                continue;
            }
            compareImages((std::string("self-check ") + c.name).c_str(), decode(c.format, a.out), decode(c.format, b.out));
            cases++;
        }
        printf("harness self-check (legacy against legacy): %d cases\n", cases);
    }

    int synthetic() {
        installSink();
        if (moduleExists("FILE_DSPLY") && moduleExists("RGBE_DSPLY"))
            selfCheck(workDir("parity_self"));
        bool all = moduleExists("OIIO_DSPLY");
        all = moduleExists("FILE_DSPLY") && all;
        all = moduleExists("RGBE_DSPLY") && all;
#ifdef HAVE_OPENEXR
        all = moduleExists("OPENEXR_DSPLY") && all;
#endif
        if (!all)
            return 1;
        const std::string dir = workDir("parity");
        int compared = 0;
        for (const Case &c : kCases) {
#ifndef HAVE_OPENEXR
            if (std::string(c.format) == "exr")
                continue;
#endif
            runOne(c, dir, false);
            compared++;
        }
        // gamma changes no sample
        for (const Case &c : kCases)
            if (std::string(c.name) == "png16" || std::string(c.name) == "tiff8") {
                runOne(c, dir, true);
                compared++;
            }
        printf("ImageIO_DisplayParity: %d cases compared\n", compared);
        DCHECK(compared >= 20);
        return failures() == 0 ? 0 : 1;
    }

    int compareFiles(const char *format, const char *legacy, const char *oiio) {
        Img a = decode(format, legacy, true), b = decode(format, oiio); // legacy 16-bit PNG: byte-swapped on disk, GitHub #181
        compareImages(legacy, a, b);
        if (a.ok() && b.ok())
            printf("%s: %dx%dx%d, %zu samples, 0 differing\n", legacy, a.w, a.h, a.n, a.v.size());
        return failures() == 0 ? 0 : 1;
    }

} // namespace

int main(int argc, char **argv) {
    if (argc == 2 && strcmp(argv[1], "synthetic") == 0)
        return synthetic();
    if (argc == 5 && strcmp(argv[1], "compare") == 0)
        return compareFiles(argv[2], argv[3], argv[4]);
    fprintf(stderr, "usage: test_display_parity synthetic | compare <tiff|png|exr|rgbe> <legacy-file> <oiio-file>\n");
    return 2;
}
