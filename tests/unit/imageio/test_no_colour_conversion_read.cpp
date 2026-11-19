// tests/unit/imageio/test_no_colour_conversion_read.cpp -- the program of ImageIO_NoColourConversion (spec 048, T059,
// P-Q10). For each fixture named on the command line it prints one line,
//     <file> legacy=<digest> oiio=<digest>
// a 64-bit FNV-1a digest of the CImageInfo and every decoded byte. run_two_ocio_envs.cmake runs it with OCIO unset
// and with OCIO pointing at a different colour configuration, and fails unless the digests agree between the two
// runs and between the two backends: the library never converts a colour, whatever configuration is loaded.
#include "decode_harness.h"

using namespace harness;

static uint64_t digest(const Decoded &d) {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](const void *p, size_t n) {
        const uint8_t *b = (const uint8_t *)p;
        for (size_t i = 0; i < n; i++) {
            h ^= b[i];
            h *= 1099511628211ull;
        }
    };
    int32_t info[5] = {d.info.width, d.info.height, d.info.numChannels, d.info.bitsPerSample, d.info.isFloatFormat ? 1 : 0};
    mix(info, sizeof(info));
    mix(d.bytes.data(), d.bytes.size());
    return h;
}

int main(int argc, char **argv) {
    installSink();
    int bad = 0;
    for (int i = 1; i < argc; i++) {
        Decoded legacy = decode(argv[i], EImageBackend::Legacy);
        Decoded oiio = decode(argv[i], EImageBackend::OIIO);
        if (!legacy.ok() || !oiio.ok()) {
            fprintf(stderr, "cannot decode %s (legacy ok=%d, oiio ok=%d): %s\n", argv[i], (int)legacy.ok(), (int)oiio.ok(),
                    oiio.allMessages().c_str());
            bad++;
            continue;
        }
        printf("%s legacy=%016llx oiio=%016llx\n", baseName(argv[i]).c_str(), (unsigned long long)digest(legacy),
               (unsigned long long)digest(oiio));
    }
    return bad == 0 ? 0 : 1;
}
