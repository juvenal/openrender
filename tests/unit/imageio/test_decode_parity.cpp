// tests/unit/imageio/test_decode_parity.cpp -- ImageIO_DecodeParity (spec 048, T054, FR-009, SC-001).
//
// For every fixture the legacy decoders accept (the spec 018 set and the parity/ set generated for 048), decode with
// createImageInput(path, Legacy) and with createImageInput(path, OIIO), and compare the CImageInfo and EVERY sample
// bit for bit (a float NaN equals a NaN by class). No tolerance anywhere (FR-021). Files of the difference list are
// not here: they live in differences/ and ImageIO_DecodeDifferences owns them.
//
// First, two checks of the harness itself on the legacy path alone: decoding twice agrees, and a changed byte is
// detected. They are green on the starting tree by design; the OIIO half is red until the OIIO decode path exists.
#include "decode_harness.h"

using namespace harness;

static bool accepted018(const std::string &name) {
    // the negative fixtures of spec 018 are refusals, covered by ImageIO_DecodeErrors
    return name.find("indexed") == std::string::npos && name.find("multipart") == std::string::npos &&
           name.find("unsupported_channels") == std::string::npos;
}

int main() {
    installSink();

    std::vector<const char *> suffixes = {".tif", ".png", ".hdr"};
#ifdef HAVE_OPENEXR
    suffixes.push_back(".exr");
#endif
    std::vector<std::string> fixtures;
    for (const std::string &p : listFiles(legacyFixtures(), suffixes))
        if (accepted018(baseName(p)))
            fixtures.push_back(p);
    for (const std::string &p : listFiles(imageioFixtures() + "/parity", suffixes))
        fixtures.push_back(p);
    EXPECT(fixtures.size() >= 40, "found only %zu fixtures (is IMAGEIO_FIXTURES_DIR right?)", fixtures.size());

    // ---- the harness, on the legacy path alone ----
    {
        Decoded a = decode(fixtures.front(), EImageBackend::Legacy);
        Decoded b = decode(fixtures.front(), EImageBackend::Legacy);
        std::string why;
        EXPECT(a.ok() && b.ok(), "legacy decode of %s failed", fixtures.front().c_str());
        EXPECT(a.ok() && b.ok() && sameSamples(a, b, why), "legacy decoded twice differs: %s", why.c_str());
        if (b.ok() && !b.bytes.empty()) {
            b.bytes[b.bytes.size() / 2] ^= 0x01;
            EXPECT(!sameSamples(a, b, why), "the harness did not notice a changed byte");
        }
        printf("harness self-check (legacy against legacy): done\n");
    }

    // ---- legacy against OIIO ----
    int compared = 0, differing = 0;
    for (const std::string &path : fixtures) {
        Decoded legacy = decode(path, EImageBackend::Legacy);
        EXPECT(legacy.ok(), "legacy refused a fixture it must accept: %s: %s", path.c_str(), legacy.allMessages().c_str());
        if (!legacy.ok())
            continue;
        Decoded oiio = decode(path, EImageBackend::OIIO);
        compared++;
        if (!oiio.ok()) {
            differing++;
            EXPECT(false, "OIIO did not decode %s (created=%d opened=%d read=%d): %s", baseName(path).c_str(), (int)oiio.created,
                   (int)oiio.opened, (int)oiio.read, oiio.allMessages().c_str());
            continue;
        }
        std::string why;
        if (!sameSamples(legacy, oiio, why)) {
            differing++;
            EXPECT(false, "%s: %s (a difference must be on the list: tests/unit/imageio/differences.txt)", baseName(path).c_str(),
                   why.c_str());
        }
        // the "no half-precision tier" rule: a half source arrives as float32
        if (baseName(path).find("half") != std::string::npos && baseName(path).find(".exr") != std::string::npos) {
            EXPECT(oiio.info.bitsPerSample == 32 && oiio.info.isFloatFormat, "%s: half must arrive as float32", baseName(path).c_str());
        }
    }
    printf("ImageIO_DecodeParity: %d fixtures compared, %d differing\n", compared, differing);
    return finish("ImageIO_DecodeParity");
}
