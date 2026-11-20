// tests/unit/imageio/test_no_colour_conversion_write.cpp -- the program of ImageIO_NoColourConversionWrite (spec 048, T106, P-Q10).
//
// For each case named on the command line (png8, png8a, png16, png16a: 8 and 16-bit PNG, without and with alpha) it writes the same synthetic
// rows through the legacy `file` driver and through the oiio driver and prints one line
//     <case> legacy=<digest> oiio=<digest>
// a 64-bit FNV-1a digest of the samples libpng decodes. run_two_ocio_envs.cmake (the T059 wrapper, unchanged) runs it with OCIO unset and with
// OCIO pointing at a different colour configuration and fails unless the digests agree between the two runs and between the two drivers: the
// writer never converts a colour, whatever configuration is loaded. Without the oiio module the program prints nothing and exits 1.
#include "display_harness.h"

using namespace dh;

static uint64_t digest(const Img &d) {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](const void *p, size_t n) {
        const uint8_t *b = (const uint8_t *)p;
        for (size_t i = 0; i < n; i++) {
            h ^= b[i];
            h *= 1099511628211ull;
        }
    };
    int32_t info[3] = {d.w, d.h, d.n};
    mix(info, sizeof(info));
    mix(d.v.data(), d.v.size() * sizeof(float));
    return h;
}

int main(int argc, char **argv) {
    installSink();
    if (argc < 2 || !moduleExists("OIIO_DSPLY") || !moduleExists("FILE_DSPLY"))
        return 1;
    const std::string dir = workDir("ocio_write");
    int bad = 0;
    for (int i = 1; i < argc; i++) {
        const std::string name = argv[i];
        const bool sixteen = name.find("16") != std::string::npos;
        const bool alpha = name.back() == 'a';
        const int ns = alpha ? 4 : 3;
        Run legacy;
        legacy.module = module("FILE_DSPLY");
        legacy.type = "file";
        legacy.out = dir + "/" + name + "-legacy.png";
        legacy.ns = ns;
        legacy.mode = alpha ? "rgba" : "rgb";
        legacy.quantizer[1] = legacy.quantizer[3] = sixteen ? 65535.0f : 255.0f;
        Run oiio = legacy;
        oiio.module = module("OIIO_DSPLY");
        oiio.type = "oiio";
        oiio.out = dir + "/" + name + "-oiio.png";
        const std::vector<float> src = quantizedSource(16, 16, ns);
        if (!runDisplay(legacy, src) || !runDisplay(oiio, src)) {
            fprintf(stderr, "%s: a display did not start\n%s", name.c_str(), sink().all().c_str());
            bad++;
            continue;
        }
        Img a = decodePng(legacy.out, true), b = decodePng(oiio.out); // legacy 16-bit PNG is byte-swapped on disk (GitHub #181, D-PNG-WRITE16)
        if (!a.ok() || !b.ok() || a.n != ns || b.n != ns) {
            fprintf(stderr, "%s: cannot decode a PNG (legacy: %s, oiio: %s)\n", name.c_str(), a.err.c_str(), b.err.c_str());
            bad++;
            continue;
        }
        printf("%s legacy=%016llx oiio=%016llx\n", name.c_str(), (unsigned long long)digest(a), (unsigned long long)digest(b));
    }
    return bad == 0 ? 0 : 1;
}
