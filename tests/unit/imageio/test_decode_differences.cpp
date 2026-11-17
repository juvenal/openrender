// tests/unit/imageio/test_decode_differences.cpp -- ImageIO_DecodeDifferences (spec 048, T056, FR-012, SC-009).
//
// One check per entry of differences.txt (contracts/difference-list.md). The ids in the data file and the checks
// below must be the same set. Each check pins what the two paths deliver for its fixture. The entries are
// predictions from OIIO's source: a check that fails shows the prediction was wrong and goes to the maintainer
// (PAUSE-DIFFERENCES); it is never loosened here.
#include "decode_harness.h"

#include <cmath>
#include <functional>
#include <map>
#include <set>

using namespace harness;

static const std::string dir = imageioFixtures() + "/differences/";

// ---- helpers ----------------------------------------------------------------------------------------------------
static float floatAt(const Decoded &d, size_t i) {
    float f;
    memcpy(&f, &d.bytes[i * 4], 4);
    return f;
}
static uint32_t bitsOf(float f) {
    uint32_t u;
    memcpy(&u, &f, 4);
    return u;
}
static uint16_t u16At(const Decoded &d, size_t i) {
    uint16_t v;
    memcpy(&v, &d.bytes[i * 2], 2);
    return v;
}
static float halfToFloat(uint16_t h) {
    int sign = (h >> 15) & 1, exp = (h >> 10) & 31, man = h & 1023;
    float v;
    if (exp == 0)
        v = std::ldexp((float)man, -24);
    else if (exp == 31)
        v = man ? NAN : INFINITY;
    else
        v = std::ldexp((float)(man | 1024), exp - 25);
    return sign ? -v : v;
}
// the closed-form u8 sample of tests/unit/imageio/fixtures/FIXTURES.md
static uint8_t u8At(int x, int y, int c) { return (uint8_t)((13 * x + 7 * y + 31 * c + 5) % 256); }

static Decoded need(const std::string &name, EImageBackend b, bool mustSucceed = true) {
    Decoded d = decode(dir + name, b);
    if (mustSucceed)
        EXPECT(d.ok(), "%s %s did not decode: %s", b == EImageBackend::Legacy ? "legacy" : "oiio", name.c_str(),
               d.allMessages().c_str());
    return d;
}

// ---- the checks -------------------------------------------------------------------------------------------------
static void checkRgbeExp() {
    // OIIO's literal table entries for exponent bytes 40, 223, 226, as float32 (research R5); mantissa byte 128.
    const float oiioLiteral[3] = {1.2621774e-29f, 1.547425e+26f, 1.23794e+27f};
    const int exps[3] = {40, 223, 226};
    Decoded legacy = need("rgbe_exp3.hdr", EImageBackend::Legacy);
    Decoded oiio = need("rgbe_exp3.hdr", EImageBackend::OIIO);
    if (legacy.ok() && oiio.ok()) {
        for (int i = 0; i < 3; i++) {
            float exact = 128.0f * (float)std::ldexp(1.0, exps[i] - 136);
            float pinned = 128.0f * oiioLiteral[i];
            for (int c = 0; c < 3; c++) {
                EXPECT(bitsOf(floatAt(legacy, i * 3 + c)) == bitsOf(exact), "legacy exponent %d: 0x%08x, expected the exact ldexp value 0x%08x",
                       exps[i], bitsOf(floatAt(legacy, i * 3 + c)), bitsOf(exact));
                EXPECT(bitsOf(floatAt(oiio, i * 3 + c)) == bitsOf(pinned), "oiio exponent %d: 0x%08x, expected OIIO's table value 0x%08x",
                       exps[i], bitsOf(floatAt(oiio, i * 3 + c)), bitsOf(pinned));
            }
            EXPECT(bitsOf(exact) != bitsOf(pinned), "exponent %d: no difference left to list", exps[i]);
        }
    }
    // every other exponent byte: bit-identical
    Decoded l = need("rgbe_exp256.hdr", EImageBackend::Legacy);
    Decoded o = need("rgbe_exp256.hdr", EImageBackend::OIIO);
    if (l.ok() && o.ok()) {
        int equal = 0;
        for (int x = 0; x < 256; x++) {
            bool same = true;
            for (int c = 0; c < 3; c++)
                same = same && bitsOf(floatAt(l, x * 3 + c)) == bitsOf(floatAt(o, x * 3 + c));
            bool listed = x == 40 || x == 223 || x == 226;
            if (listed)
                EXPECT(!same, "exponent byte %d should differ by one ulp", x);
            else if (same)
                equal++;
            else
                EXPECT(false, "exponent byte %d differs and is not listed", x);
        }
        EXPECT(equal == 253, "%d of 253 unlisted exponents are identical", equal);
    }
}

static void checkRgbeRle() {
    Decoded flat = need("rle_16x4_flat.hdr", EImageBackend::Legacy);
    Decoded rleLegacy = decode(dir + "rle_16x4_rle.hdr", EImageBackend::Legacy); // outcome recorded, not asserted
    Decoded rleOiio = need("rle_16x4_rle.hdr", EImageBackend::OIIO);
    std::string why;
    if (flat.ok() && rleOiio.ok())
        EXPECT(sameSamples(flat, rleOiio, why), "OIIO's decode of the RLE file differs from legacy's decode of the flat one: %s", why.c_str());
    if (!rleLegacy.ok())
        printf("D-RGBE-RLE legacy RLE outcome: refused (opened=%d read=%d)\n", (int)rleLegacy.opened, (int)rleLegacy.read);
    else if (flat.ok() && sameSamples(flat, rleLegacy, why))
        printf("D-RGBE-RLE legacy RLE outcome: decoded correctly (no legacy defect)\n");
    else
        printf("D-RGBE-RLE legacy RLE outcome: decoded, but different from the flat file (%s)\n", why.c_str());
}

static void checkPngTrns() {
    Decoded legacy = need("trns_rgb8_4x4.png", EImageBackend::Legacy);
    Decoded oiio = need("trns_rgb8_4x4.png", EImageBackend::OIIO);
    if (legacy.ok())
        EXPECT(legacy.info.numChannels == 3 && legacy.info.bitsPerSample == 8, "legacy: %d channels %d bits", legacy.info.numChannels, legacy.info.bitsPerSample);
    if (oiio.ok()) {
        EXPECT(oiio.info.numChannels == 4 && oiio.info.bitsPerSample == 8, "oiio: %d channels %d bits", oiio.info.numChannels, oiio.info.bitsPerSample);
        if (legacy.ok() && oiio.info.numChannels == 4)
            for (int y = 0; y < 4; y++)
                for (int x = 0; x < 4; x++) {
                    for (int c = 0; c < 3; c++)
                        EXPECT(legacy.bytes[(y * 4 + x) * 3 + c] == oiio.bytes[(y * 4 + x) * 4 + c], "RGB (%d,%d,%d) differs", x, y, c);
                    uint8_t a = oiio.bytes[(y * 4 + x) * 4 + 3];
                    EXPECT(a == (x == y ? 0 : 255), "alpha at (%d,%d) is %d, expected %d", x, y, a, x == y ? 0 : 255);
                }
    }
}

static void checkTiffSeparate() {
    Decoded legacy = decode(dir + "tiff_separate_rgb8_4x4.tif", EImageBackend::Legacy); // outcome recorded
    Decoded oiio = need("tiff_separate_rgb8_4x4.tif", EImageBackend::OIIO);
    bool formula = true;
    if (oiio.ok()) {
        EXPECT(oiio.info.numChannels == 3 && oiio.info.bitsPerSample == 8, "oiio info");
        for (int y = 0; y < 4; y++)
            for (int x = 0; x < 4; x++)
                for (int c = 0; c < 3; c++)
                    formula = formula && oiio.bytes[(y * 4 + x) * 3 + c] == u8At(x, y, c);
        EXPECT(formula, "OIIO does not equal the closed-form formula");
    }
    bool legacyMatches = legacy.ok() && legacy.bytes.size() == 48;
    if (legacyMatches)
        for (int y = 0; y < 4; y++)
            for (int x = 0; x < 4; x++)
                for (int c = 0; c < 3; c++)
                    legacyMatches = legacyMatches && legacy.bytes[(y * 4 + x) * 3 + c] == u8At(x, y, c);
    printf("D-TIFF-SEPARATE legacy outcome: %s\n", !legacy.ok() ? "refused" : legacyMatches ? "equals the formula (no legacy defect)" : "differs from the formula (legacy defect)");
}

static void checkTiffPalette() {
    Decoded legacy = need("tiff_palette8_4x4.tif", EImageBackend::Legacy);
    Decoded oiio = need("tiff_palette8_4x4.tif", EImageBackend::OIIO);
    if (legacy.ok()) {
        EXPECT(legacy.info.numChannels == 1 && legacy.info.bitsPerSample == 8, "legacy info");
        for (int y = 0; y < 4; y++)
            for (int x = 0; x < 4; x++)
                EXPECT(legacy.bytes[y * 4 + x] == (uint8_t)(x * 16 + y * 5 + 1), "legacy raw index at (%d,%d)", x, y);
    }
    if (oiio.ok()) {
        EXPECT(oiio.info.numChannels == 3, "oiio: %d channels, expected the palette expanded to 3", oiio.info.numChannels);
        EXPECT(oiio.info.bitsPerSample == 8 || oiio.info.bitsPerSample == 16, "oiio: %d bits", oiio.info.bitsPerSample);
        if (oiio.info.numChannels == 3)
            for (int y = 0; y < 4; y++)
                for (int x = 0; x < 4; x++) {
                    int i = x * 16 + y * 5 + 1;
                    int want[3] = {i, 255 - i, (3 * i) % 256}; // the colour map entries / 257
                    for (int c = 0; c < 3; c++) {
                        int got = oiio.info.bitsPerSample == 8 ? oiio.bytes[(y * 4 + x) * 3 + c] : u16At(oiio, (size_t)(y * 4 + x) * 3 + c) / 257;
                        EXPECT(got == want[c], "oiio palette (%d,%d,%d): %d, expected %d", x, y, c, got, want[c]);
                    }
                }
    }
}

static void checkTiffMiniswhite() {
    Decoded legacy = need("tiff_miniswhite8_4x4.tif", EImageBackend::Legacy);
    Decoded oiio = need("tiff_miniswhite8_4x4.tif", EImageBackend::OIIO);
    for (int y = 0; y < 4; y++)
        for (int x = 0; x < 4; x++) {
            uint8_t v = u8At(x, y, 0);
            if (legacy.ok())
                EXPECT(legacy.bytes[y * 4 + x] == v, "legacy raw at (%d,%d)", x, y);
            if (oiio.ok())
                EXPECT(oiio.bytes[y * 4 + x] == (uint8_t)(255 - v), "oiio at (%d,%d): %d, expected %d (inverted)", x, y, oiio.bytes[y * 4 + x], 255 - v);
        }
}

static void checkTiffUint32() {
    Decoded legacy = need("tiff_uint32_4x4.tif", EImageBackend::Legacy);
    if (legacy.ok())
        EXPECT(legacy.info.bitsPerSample == 32 && legacy.info.isFloatFormat && legacy.info.numChannels == 1, "legacy info %d bits float=%d", legacy.info.bitsPerSample, (int)legacy.info.isFloatFormat);
    Decoded oiio = decode(dir + "tiff_uint32_4x4.tif", EImageBackend::OIIO);
    EXPECT(oiio.created && !(oiio.opened && oiio.read), "OIIO must create an input that refuses the 32-bit unsigned TIFF (created=%d opened=%d read=%d)", (int)oiio.created, (int)oiio.opened, (int)oiio.read);
    EXPECT(oiio.mentions("tiff_uint32_4x4.tif"), "the OIIO error does not name the file: '%s'", oiio.allMessages().c_str());
    std::string all = oiio.allMessages();
    // the file name contains the type's name: search the message without it, so the type itself must be named
    for (size_t at; (at = all.find("tiff_uint32_4x4.tif")) != std::string::npos;)
        all.erase(at, strlen("tiff_uint32_4x4.tif"));
    std::transform(all.begin(), all.end(), all.begin(), ::tolower);
    EXPECT(all.find("uint32") != std::string::npos, "the OIIO error does not name the type uint32: '%s'", oiio.allMessages().c_str());
}

static void checkTiffInt16() {
    Decoded legacy = need("tiff_int16_4x4.tif", EImageBackend::Legacy);
    if (legacy.ok())
        EXPECT(legacy.info.bitsPerSample == 16 && !legacy.info.isFloatFormat, "legacy info %d bits float=%d", legacy.info.bitsPerSample, (int)legacy.info.isFloatFormat);
    Decoded oiio = decode(dir + "tiff_int16_4x4.tif", EImageBackend::OIIO);
    EXPECT(oiio.created && !(oiio.opened && oiio.read), "OIIO must create an input that refuses the signed 16-bit TIFF (created=%d opened=%d read=%d)", (int)oiio.created, (int)oiio.opened, (int)oiio.read);
    EXPECT(oiio.mentions("tiff_int16_4x4.tif"), "the OIIO error does not name the file: '%s'", oiio.allMessages().c_str());
    std::string all = oiio.allMessages();
    // the file name contains the type's name: search the message without it, so the type itself must be named
    for (size_t at; (at = all.find("tiff_int16_4x4.tif")) != std::string::npos;)
        all.erase(at, strlen("tiff_int16_4x4.tif"));
    std::transform(all.begin(), all.end(), all.begin(), ::tolower);
    EXPECT(all.find("int16") != std::string::npos, "the OIIO error does not name the type int16: '%s'", oiio.allMessages().c_str());
}

static void checkTiffHalf() {
    Decoded legacy = need("tiff_half_4x4.tif", EImageBackend::Legacy);
    Decoded oiio = need("tiff_half_4x4.tif", EImageBackend::OIIO);
    if (legacy.ok())
        EXPECT(legacy.info.bitsPerSample == 16 && !legacy.info.isFloatFormat, "legacy info %d/%d", legacy.info.bitsPerSample, (int)legacy.info.isFloatFormat);
    if (oiio.ok()) {
        EXPECT(oiio.info.bitsPerSample == 32 && oiio.info.isFloatFormat, "oiio info %d/%d", oiio.info.bitsPerSample, (int)oiio.info.isFloatFormat);
        for (int i = 0; i < 16 && oiio.info.isFloatFormat; i++) {
            uint16_t half = (uint16_t)(0x3c00 + i * 0x0040);
            EXPECT(bitsOf(floatAt(oiio, i)) == bitsOf(halfToFloat(half)), "half 0x%04x widened to 0x%08x, expected 0x%08x", half, bitsOf(floatAt(oiio, i)), bitsOf(halfToFloat(half)));
            if (legacy.ok())
                EXPECT(u16At(legacy, i) == half, "legacy raw half bits at %d", i);
        }
    }
}

// ---- the data file ----------------------------------------------------------------------------------------------
// D-PNG-WRITE16 is a difference of the DISPLAY drivers (the legacy `file` driver writes 16-bit PNG samples byte-swapped, GitHub #181), not of the decoders:
// it has no fixture here. Its behaviour is pinned by ImageIO_DisplayParity and ImageIO_NoColourConversionWrite; this check only requires the entry to say
// so (the issue and the two tests), so the list cannot lose the record.
static void checkPngWrite16(const std::string &dataFile) {
    FILE *f = fopen(dataFile.c_str(), "r");
    EXPECT(f != nullptr, "cannot open %s", dataFile.c_str());
    if (f == nullptr)
        return;
    char line[4096];
    bool found = false;
    while (fgets(line, sizeof(line), f)) {
        std::string s = line;
        if (s.compare(0, 14, "D-PNG-WRITE16 ") == 0) {
            found = true;
            EXPECT(s.find("#181") != std::string::npos, "D-PNG-WRITE16 does not cite GitHub #181");
            EXPECT(s.find("ImageIO_DisplayParity") != std::string::npos, "D-PNG-WRITE16 does not name ImageIO_DisplayParity");
            EXPECT(s.find("ImageIO_NoColourConversionWrite") != std::string::npos, "D-PNG-WRITE16 does not name ImageIO_NoColourConversionWrite");
        }
    }
    fclose(f);
    EXPECT(found, "D-PNG-WRITE16 is not in %s", dataFile.c_str());
}

static std::set<std::string> idsInDataFile(const std::string &path) {
    std::set<std::string> ids;
    FILE *f = fopen(path.c_str(), "r");
    EXPECT(f != nullptr, "cannot open %s", path.c_str());
    if (f == nullptr)
        return ids;
    char line[4096];
    while (fgets(line, sizeof(line), f)) {
        std::string s = line;
        if (s.empty() || s[0] == '#' || s[0] == '\n')
            continue;
        size_t bar = s.find(" | ");
        if (bar != std::string::npos)
            ids.insert(s.substr(0, bar));
    }
    fclose(f);
    return ids;
}

int main(int argc, char **argv) {
    installSink();
    std::map<std::string, std::function<void()>> checks = {
        {"D-RGBE-EXP", checkRgbeExp},       {"D-RGBE-RLE", checkRgbeRle},         {"D-PNG-TRNS", checkPngTrns},
        {"D-TIFF-SEPARATE", checkTiffSeparate}, {"D-TIFF-PALETTE", checkTiffPalette}, {"D-TIFF-MINISWHITE", checkTiffMiniswhite},
        {"D-TIFF-UINT32", checkTiffUint32}, {"D-TIFF-INT16", checkTiffInt16},     {"D-TIFF-HALF", checkTiffHalf},
    };
    std::string data = argc > 1 ? argv[1] : "tests/unit/imageio/differences.txt";
    checks["D-PNG-WRITE16"] = [data] { checkPngWrite16(data); };
    std::set<std::string> ids = idsInDataFile(data);
    for (const std::string &id : ids)
        EXPECT(checks.count(id) != 0, "%s is in differences.txt but has no check", id.c_str());
    for (auto &c : checks)
        EXPECT(ids.count(c.first) != 0, "%s has a check but is not in differences.txt", c.first.c_str());
    for (auto &c : checks) {
        int before = failures();
        c.second();
        printf("%s: %s\n", c.first.c_str(), failures() == before ? "confirmed" : "NOT CONFIRMED");
    }
    return finish("ImageIO_DecodeDifferences");
}
