// tests/unit/imageio/test_decode_errors.cpp -- ImageIO_DecodeErrors (spec 048, T055, FR-011, FR-023, P-Q5).
//
// Both paths refuse what the legacy decoders refuse, with no partial image: indexed, interlaced and sub-8-bit PNG,
// TIFF with a bits-per-sample other than 8/16/32 and tiled TIFF, multi-part, unsupported-channel and deep EXR, and
// missing, truncated and corrupted files of each format; a PNG named .tif; and JPEG, TGA, BMP and WebP (nullptr by
// suffix, and failing when renamed to a known suffix).
//
// What is asserted of the TEXT of the error: the OIIO path always names the file (FR-011). The legacy decoders'
// own messages are not asserted: some do not name the file (libtiff's handler drops the module name, and a tiled
// TIFF fails at read), and 048 does not change them.
#include "decode_harness.h"

using namespace harness;

static bool refused(const Decoded &d) {
    return !(d.created && d.opened && d.read);
}

struct Backend {
        EImageBackend which;
        const char *name;
};

static void expectRefused(const std::string &path, const Backend &b, bool nameTheFile) {
    Decoded d = decode(path, b.which);
    EXPECT(d.created, "%s: createImageInput returned nullptr for %s", b.name, baseName(path).c_str());
    EXPECT(refused(d), "%s accepted %s", b.name, baseName(path).c_str());
    EXPECT(d.bytes.empty(), "%s left a partial image for %s", b.name, baseName(path).c_str());
    if (nameTheFile) {
        EXPECT(!d.messages.empty(), "%s: no error was reported for %s", b.name, baseName(path).c_str());
        EXPECT(d.mentions(baseName(path)), "%s: the error does not name %s: '%s'", b.name, baseName(path).c_str(),
               d.allMessages().c_str());
    }
}

int main() {
    installSink();
    const Backend backends[] = {{EImageBackend::Legacy, "legacy"}, {EImageBackend::OIIO, "oiio"}};
    const std::string legacyDir = legacyFixtures();
    const std::string refusals = imageioFixtures() + "/refusals";

    // ---- files the decoders refuse ----
    std::vector<std::string> refusedFiles = {
        legacyDir + "/tiny_indexed.png",           legacyDir + "/large_indexed.png",
        refusals + "/png_interlaced.png",          refusals + "/png_gray4.png",
        refusals + "/png_gray1.png",               refusals + "/tiff_tiled.tif",
        refusals + "/tiff_bilevel.tif",
#ifdef HAVE_OPENEXR
        legacyDir + "/tiny_multipart.exr",         legacyDir + "/tiny_unsupported_channels.exr",
        refusals + "/exr_two_channel.exr",         refusals + "/exr_deep.exr",
#endif
    };
    for (const std::string &p : refusedFiles)
        for (const Backend &b : backends)
            expectRefused(p, b, b.which == EImageBackend::OIIO);

    // ---- missing, truncated, corrupted ----
    const std::string scratch = scratchDir();
    for (const Backend &b : backends)
        expectRefused(scratch + "/does_not_exist.png", b, b.which == EImageBackend::OIIO);

    struct Source {
            const char *file;
            const char *suffix;
    };
    std::vector<Source> sources = {{"large_rgb.tif", ".tif"}, {"large_rgb8.png", ".png"}, {"tiny.hdr", ".hdr"}};
#ifdef HAVE_OPENEXR
    sources.push_back({"tiny_rgb.exr", ".exr"});
#endif
    for (const Source &s : sources) {
        std::vector<uint8_t> whole = readFile(legacyDir + "/" + s.file);
        EXPECT(whole.size() > 64, "cannot read %s", s.file);
        if (whole.size() <= 64)
            continue;
        // truncated: the header intact, the data cut in half
        std::vector<uint8_t> cut(whole.begin(), whole.begin() + (long)(whole.size() / 2));
        std::string truncated = scratch + "/truncated" + s.suffix;
        EXPECT(writeFile(truncated, cut), "cannot write %s", truncated.c_str());
        // corrupted: the first 16 bytes (the magic and the start of the header) overwritten
        std::vector<uint8_t> bad = whole;
        for (int i = 0; i < 16; i++)
            bad[i] = 0xAA;
        std::string corrupted = scratch + "/corrupted" + s.suffix;
        EXPECT(writeFile(corrupted, bad), "cannot write %s", corrupted.c_str());
        for (const Backend &b : backends) {
            expectRefused(truncated, b, b.which == EImageBackend::OIIO);
            expectRefused(corrupted, b, b.which == EImageBackend::OIIO);
        }
    }

    // ---- content and suffix disagree: a PNG named .tif, a TIFF named .png ----
    {
        EXPECT(writeFile(scratch + "/png_as.tif", readFile(legacyDir + "/large_rgb8.png")), "cannot write png_as.tif");
        EXPECT(writeFile(scratch + "/tif_as.png", readFile(legacyDir + "/large_rgb.tif")), "cannot write tif_as.png");
        for (const Backend &b : backends) {
            expectRefused(scratch + "/png_as.tif", b, b.which == EImageBackend::OIIO);
            expectRefused(scratch + "/tif_as.png", b, b.which == EImageBackend::OIIO);
        }
    }

    // ---- JPEG, TGA, BMP and WebP (FR-023): nullptr by suffix, and refused when renamed to a known suffix ----
    for (const char *fmt : {"fmt.jpg", "fmt.tga", "fmt.bmp", "fmt.webp"}) {
        std::vector<uint8_t> bytes = readFile(refusals + "/" + fmt);
        EXPECT(!bytes.empty(), "cannot read %s", fmt);
        for (const Backend &b : backends) {
            CImageInput *in = createImageInput((refusals + "/" + fmt).c_str(), b.which);
            EXPECT(in == nullptr, "%s: createImageInput accepted the suffix of %s", b.name, fmt);
            delete in;
        }
        std::vector<const char *> renamed = {".png", ".tif", ".hdr"};
#ifdef HAVE_OPENEXR
        renamed.push_back(".exr");
#endif
        for (const char *suffix : renamed) {
            std::string as = scratch + "/" + std::string(fmt) + suffix;
            EXPECT(writeFile(as, bytes), "cannot write %s", as.c_str());
            for (const Backend &b : backends)
                expectRefused(as, b, b.which == EImageBackend::OIIO);
        }
    }

    // ---- the one-shot contract ----
    for (const Backend &b : backends) {
        const std::string path = legacyDir + "/tiny_rgb8.png";
        std::vector<std::string> msgs;
        CaptureSink::capture() = &msgs;
        CImageInput *in = createImageInput(path.c_str(), b.which);
        EXPECT(in != nullptr, "%s: no input for %s", b.name, path.c_str());
        if (in != nullptr) {
            std::vector<uint8_t> buf(4 * 4 * 3);
            EXPECT(!in->readImage(buf.data()), "%s: readImage before open returned true", b.name);
            CImageInfo info;
            EXPECT(in->open(path.c_str(), info), "%s: open failed", b.name);
            EXPECT(in->readImage(buf.data()), "%s: the first readImage failed", b.name);
            if (b.which == EImageBackend::OIIO)
                EXPECT(!in->readImage(buf.data()), "oiio: a second readImage returned true (one-shot)");
            in->close();
            EXPECT(!in->readImage(buf.data()), "%s: readImage after close returned true", b.name);
            in->close(); // idempotent
            delete in;
        }
        CaptureSink::capture() = nullptr;
    }
    return finish("ImageIO_DecodeErrors");
}
