// tests/unit/imageio/display_harness.h -- the shared helpers of the oiio display driver tests (spec 048, Phase 7).
//
// A display driver is driven the way the renderer drives it: through CDisplayHost (src/ri/render/displayHost.h) with an SDisplaySpec and
// synthetic tiles, or by orender with a scene. The file a driver writes is read back by ONE fixed decoder that is neither writer: libtiff,
// libpng and OpenEXR in test code; the OIIO Reader only for RGBE, because the OIIO driver writes RLE and the legacy decoder cannot read it
// (GitHub #171). Samples are compared exactly (bit for bit; NaN by class).
//
// The modules are named by environment variables (set by the test registration): FILE_DSPLY, RGBE_DSPLY, OPENEXR_DSPLY and OIIO_DSPLY, each
// the path of a .dsply file. A module that does not exist is a test failure that says so ("module not found").
#pragma once

#include "common/diagnostics.h"
#include "dsply.h"
#include "orImageIO.h"
#include "ri/render/displayHost.h"

#include <png.h>
#include <tiffio.h>

#ifdef HAVE_OPENEXR
#include <ImfChannelList.h>
#include <ImfCompression.h>
#include <ImfFrameBuffer.h>
#include <ImfHeader.h>
#include <ImfInputFile.h>
#endif

#include <dlfcn.h>
#include <exception>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace dh {

    inline int &failures() {
        static int n = 0;
        return n;
    }
#define DCHECK(cond) \
    do { \
        if (!(cond)) { \
            dh::failures()++; \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)
#define DFAIL(...) \
    do { \
        dh::failures()++; \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    } while (0)

    // ---- diagnostics: every report a driver makes is kept ---------------------------------------------------------------------
    class CCaptureSink : public CDiagnosticSink {
        public:
            void report(EErrorCode, EDiagnosticSeverity, const char *message) override { messages.push_back(message); }
            std::vector<std::string> messages;
            bool has(const std::string &text) const {
                for (const std::string &m : messages)
                    if (m.find(text) != std::string::npos)
                        return true;
                return false;
            }
            std::string all() const {
                std::string s;
                for (const std::string &m : messages)
                    s += m;
                return s;
            }
    };
    inline CCaptureSink &sink() {
        static CCaptureSink s;
        return s;
    }
    inline void installSink() { installDiagnosticSink(&sink()); }

    // ---- modules and files ----------------------------------------------------------------------------------------------------
    inline std::string module(const char *envName) {
        const char *v = getenv(envName);
        return v != nullptr ? v : "";
    }
    inline bool exists(const std::string &p) { return access(p.c_str(), F_OK) == 0; }
    inline bool moduleExists(const char *envName) {
        const std::string m = module(envName);
        if (m.empty() || !exists(m)) {
            DFAIL("module not found: $%s = \"%s\" (the oiio display driver does not exist yet)", envName, m.c_str());
            return false;
        }
        return true;
    }
    inline std::string workDir(const char *leaf) {
        const char *base = getenv("TMPDIR");
        std::string d = std::string(base != nullptr ? base : "/tmp") + "/or048_display_" + leaf + "_" + std::to_string((long)getpid());
        mkdir(d.c_str(), 0755);
        return d;
    }
    inline bool hasPrefix(const std::string &path, const unsigned char *magic, size_t n) {
        FILE *f = fopen(path.c_str(), "rb");
        if (f == nullptr)
            return false;
        unsigned char b[8] = {};
        size_t got = fread(b, 1, n, f);
        fclose(f);
        return got == n && memcmp(b, magic, n) == 0;
    }
    inline bool isPng(const std::string &p) {
        static const unsigned char m[8] = {0x89, 'P', 'N', 'G', 0x0d, 0x0a, 0x1a, 0x0a};
        return hasPrefix(p, m, 8);
    }
    inline bool isTiff(const std::string &p) {
        static const unsigned char le[4] = {'I', 'I', 42, 0}, be[4] = {'M', 'M', 0, 42};
        return hasPrefix(p, le, 4) || hasPrefix(p, be, 4);
    }
    inline bool isExr(const std::string &p) {
        static const unsigned char m[4] = {0x76, 0x2f, 0x31, 0x01};
        return hasPrefix(p, m, 4);
    }
    inline bool isHdr(const std::string &p) {
        static const unsigned char m[2] = {'#', '?'};
        return hasPrefix(p, m, 2);
    }

    // ---- the decoded image ----------------------------------------------------------------------------------------------------
    struct Img {
            int w = 0, h = 0, n = 0;
            std::vector<float> v;             // h * w * n, row-major, channels interleaved (stored order)
            std::vector<std::string> names;   // EXR and RGBE only
            int tiffCompression = -1;         // TIFF only
            std::string exrCompression;       // EXR only
            std::string err;
            bool ok() const { return err.empty() && n > 0; }
    };

    inline Img decodeTiff(const std::string &path) {
        Img r;
        TIFF *t = TIFFOpen(path.c_str(), "r");
        if (t == nullptr) {
            r.err = "cannot open " + path + " as a TIFF";
            return r;
        }
        uint32_t w = 0, h = 0;
        uint16_t spp = 0, bps = 0, fmt = SAMPLEFORMAT_UINT, comp = 0;
        TIFFGetField(t, TIFFTAG_IMAGEWIDTH, &w);
        TIFFGetField(t, TIFFTAG_IMAGELENGTH, &h);
        TIFFGetFieldDefaulted(t, TIFFTAG_SAMPLESPERPIXEL, &spp);
        TIFFGetFieldDefaulted(t, TIFFTAG_BITSPERSAMPLE, &bps);
        TIFFGetFieldDefaulted(t, TIFFTAG_SAMPLEFORMAT, &fmt);
        TIFFGetFieldDefaulted(t, TIFFTAG_COMPRESSION, &comp);
        r.w = (int)w;
        r.h = (int)h;
        r.n = spp;
        r.tiffCompression = comp;
        r.v.resize((size_t)w * h * spp);
        std::vector<unsigned char> line((size_t)TIFFScanlineSize(t));
        for (uint32_t y = 0; y < h; y++) {
            if (TIFFReadScanline(t, line.data(), y, 0) != 1) {
                r.err = "TIFFReadScanline failed";
                break;
            }
            for (uint32_t i = 0; i < w * spp; i++) {
                float f;
                if (bps == 8)
                    f = line[i];
                else if (bps == 16)
                    f = ((const uint16_t *)line.data())[i];
                else if (bps == 32 && fmt == SAMPLEFORMAT_IEEEFP)
                    f = ((const float *)line.data())[i];
                else
                    f = (float)((const uint32_t *)line.data())[i];
                r.v[(size_t)y * w * spp + i] = f;
            }
        }
        TIFFClose(t);
        return r;
    }

    // libpng, no conversion of any kind: the stored samples, 16-bit big-endian swapped to host order.
    // legacyByteSwapped is for a file the LEGACY file driver wrote: that driver hands libpng host-order 16-bit samples without png_set_swap,
    // so on a little-endian host every 16-bit sample is stored byte-swapped (GitHub #181, D-PNG-WRITE16; not fixed in spec 048). The flag
    // swaps the bytes of each decoded 16-bit sample back, so the samples the legacy driver MEANT are compared. It is applied to the legacy
    // side only, and only to 16-bit PNG; the oiio driver's files are decoded as they are.
    inline Img decodePng(const std::string &path, bool legacyByteSwapped = false) {
        Img r;
        FILE *f = fopen(path.c_str(), "rb");
        if (f == nullptr) {
            r.err = "cannot open " + path;
            return r;
        }
        png_structp png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
        png_infop info = png_create_info_struct(png);
        if (setjmp(png_jmpbuf(png))) {
            png_destroy_read_struct(&png, &info, nullptr);
            fclose(f);
            r.err = "libpng could not read " + path;
            return r;
        }
        png_init_io(png, f);
        png_read_info(png, info);
        const int bits = png_get_bit_depth(png, info);
        if (bits == 16)
            png_set_swap(png);
        r.w = (int)png_get_image_width(png, info);
        r.h = (int)png_get_image_height(png, info);
        r.n = (int)png_get_channels(png, info);
        std::vector<unsigned char> row(png_get_rowbytes(png, info));
        r.v.resize((size_t)r.w * r.h * r.n);
        for (int y = 0; y < r.h; y++) {
            png_read_row(png, row.data(), nullptr);
            for (int i = 0; i < r.w * r.n; i++)
            {
                uint16_t v16 = bits == 16 ? ((const uint16_t *)row.data())[i] : 0;
                if (bits == 16 && legacyByteSwapped)
                    v16 = (uint16_t)((v16 >> 8) | (v16 << 8));
                r.v[(size_t)y * r.w * r.n + i] = bits == 16 ? v16 : row[i];
            }
        }
        png_destroy_read_struct(&png, &info, nullptr);
        fclose(f);
        return r;
    }

#ifdef HAVE_OPENEXR
    // the channels R,G,B,A,Z (by name) read as FLOAT, which widens a half exactly; names lists the file's channels in OpenEXR's order
    inline Img decodeExr(const std::string &path) {
        Img r;
        try {
            Imf::InputFile in(path.c_str());
            const Imath::Box2i dw = in.header().dataWindow();
            r.w = dw.max.x - dw.min.x + 1;
            r.h = dw.max.y - dw.min.y + 1;
            for (auto it = in.header().channels().begin(); it != in.header().channels().end(); ++it)
                r.names.push_back(it.name());
            static const char *order[5] = {"R", "G", "B", "A", "Z"};
            std::vector<const char *> present;
            for (const char *c : order)
                if (in.header().channels().findChannel(c) != nullptr)
                    present.push_back(c);
            r.n = (int)present.size();
            r.v.assign((size_t)r.w * r.h * r.n, 0.0f);
            Imf::FrameBuffer fb;
            std::vector<std::vector<float>> planes(present.size(), std::vector<float>((size_t)r.w * r.h));
            for (size_t c = 0; c < present.size(); c++)
                fb.insert(present[c], Imf::Slice(Imf::FLOAT, (char *)(planes[c].data() - (size_t)dw.min.y * r.w - dw.min.x), sizeof(float), sizeof(float) * (size_t)r.w));
            in.setFrameBuffer(fb);
            in.readPixels(dw.min.y, dw.max.y);
            for (size_t c = 0; c < present.size(); c++)
                for (size_t i = 0; i < planes[c].size(); i++)
                    r.v[i * present.size() + c] = planes[c][i];
            switch (in.header().compression()) {
            case Imf::NO_COMPRESSION: r.exrCompression = "none"; break;
            case Imf::RLE_COMPRESSION: r.exrCompression = "rle"; break;
            case Imf::ZIPS_COMPRESSION: r.exrCompression = "zips"; break;
            case Imf::ZIP_COMPRESSION: r.exrCompression = "zip"; break;
            case Imf::PIZ_COMPRESSION: r.exrCompression = "piz"; break;
            case Imf::PXR24_COMPRESSION: r.exrCompression = "pxr24"; break;
            default: r.exrCompression = "other"; break;
            }
        } catch (const std::exception &e) {
            r.err = std::string("OpenEXR could not read ") + path + ": " + e.what();
            r.n = 0;
        }
        return r;
    }
#endif

    // the OIIO Reader, FLOAT delivered: the one decoder that reads the RLE files of the OIIO RGBE writer
    inline Img decodeRgbe(const std::string &path) {
        Img r;
        orimageio::Reader reader;
        orimageio::SourceTraits t;
        if (!reader.open(path.c_str(), orimageio::Format::Rgbe, t)) {
            r.err = "the OIIO Reader cannot open " + path;
            return r;
        }
        r.w = t.width;
        r.h = t.height;
        r.n = t.channels;
        r.names = t.channelNames;
        r.v.resize((size_t)r.w * r.h * r.n);
        if (!reader.read(r.v.data(), orimageio::SampleType::Float)) {
            r.err = "the OIIO Reader cannot read " + path;
            r.n = 0;
        }
        return r;
    }

    // legacyFile: the file was written by a legacy display driver (only 16-bit PNG decodes differently, see decodePng)
    inline Img decode(const std::string &format, const std::string &path, bool legacyFile = false) {
        if (!exists(path)) {
            Img r;
            r.err = "no such file: " + path;
            return r;
        }
        if (format == "tiff")
            return decodeTiff(path);
        if (format == "png")
            return decodePng(path, legacyFile);
        if (format == "rgbe")
            return decodeRgbe(path);
#ifdef HAVE_OPENEXR
        if (format == "exr")
            return decodeExr(path);
#endif
        Img r;
        r.err = "no decoder for format " + format;
        return r;
    }

    // Exact comparison: every sample bit for bit, NaN by class. Reports through DFAIL, returns the number of differing samples.
    inline long compareImages(const char *name, const Img &a, const Img &b) {
        if (!a.ok() || !b.ok()) {
            DFAIL("%s: cannot compare (legacy: %s; oiio: %s)", name, a.err.empty() ? "ok" : a.err.c_str(), b.err.empty() ? "ok" : b.err.c_str());
            return -1;
        }
        if (a.w != b.w || a.h != b.h || a.n != b.n) {
            DFAIL("%s: size %dx%dx%d (legacy) against %dx%dx%d (oiio)", name, a.w, a.h, a.n, b.w, b.h, b.n);
            return -1;
        }
        long differing = 0;
        for (size_t i = 0; i < a.v.size(); i++) {
            const float x = a.v[i], y = b.v[i];
            const bool bothNaN = std::isnan(x) && std::isnan(y);
            if (!bothNaN && memcmp(&x, &y, sizeof(float)) != 0) {
                if (differing < 4)
                    fprintf(stderr, "%s: sample %zu (pixel %zu, channel %zu): legacy %.9g, oiio %.9g\n", name, i, i / (size_t)a.n, i % (size_t)a.n, x, y);
                differing++;
            }
        }
        if (differing != 0)
            DFAIL("%s: %ld differing sample(s) of %zu", name, differing, a.v.size());
        return differing;
    }

    // ---- driving a display through CDisplayHost -------------------------------------------------------------------------------
    struct Run {
            std::string module;               // the .dsply file
            std::string out;                  // Display name
            std::string type = "file";        // the display type as written in the RIB
            std::string mode = "rgba";
            int w = 16, h = 16, ns = 4;
            float quantizer[5] = {0, 255, 0, 255, 0}; // the display's own Quantize; {0,0,0,0,0} = float output
            std::vector<SDisplayParameterSpec> params; // the display line's own parameters
            std::string software = "openRender-test";
            float nl[16] = {}, np[16] = {};            // the Nl and NP the renderer provides (distinct per run, see tiffMatrices)
    };

    inline SDisplayParameterSpec stringParam(const char *name, const char *value) {
        SDisplayParameterSpec p;
        p.name = name;
        p.type = OR_DISPLAY_PARAM_STRING;
        p.numItems = 1;
        p.strings.push_back(value);
        return p;
    }

    // Starts the display, dispatches `pixels` (w*h*ns floats) as 4x4 tiles in a fixed shuffled order (each tile a copy: a driver quantizes
    // in place), finishes it. False when the display did not start; `started` says so.
    inline bool runDisplay(const Run &r, const std::vector<float> &pixels) {
        CDisplayHost host;
        SDisplaySpec s;
        s.name = r.out;
        s.type = r.type;
        s.mode = r.mode;
        s.module = r.module;
        s.width = r.w;
        s.height = r.h;
        s.channels.push_back({r.mode, r.ns, OR_DISPLAY_VALUE_FLOAT, false, {0, 0, 0, 0, 0}});
        s.parameters = r.params;
        memcpy(s.quantizer, r.quantizer, sizeof(s.quantizer));
        memcpy(s.colorQuantizer, r.quantizer, sizeof(s.colorQuantizer));
        memcpy(s.depthQuantizer, r.quantizer, sizeof(s.depthQuantizer));
        memcpy(s.fromWorld, r.nl, sizeof(s.fromWorld));
        memcpy(s.worldToNDC, r.np, sizeof(s.worldToNDC));
        s.software = r.software;
        const int index = host.add(s);
        if (!host.startDisplay(index))
            return false;
        const int tile = 4;
        const int tilesX = (r.w + tile - 1) / tile, tilesY = (r.h + tile - 1) / tile, tiles = tilesX * tilesY;
        for (int k = 0; k < tiles; k++) {
            const int t = (k * 7 + 3) % tiles; // 7 is coprime with 16
            const int x0 = (t % tilesX) * tile, y0 = (t / tilesX) * tile;
            const int tw = std::min(tile, r.w - x0), th = std::min(tile, r.h - y0);
            std::vector<float> copy((size_t)tw * th * r.ns);
            for (int y = 0; y < th; y++)
                memcpy(&copy[(size_t)y * tw * r.ns], &pixels[((size_t)(y0 + y) * r.w + x0) * r.ns], (size_t)tw * r.ns * sizeof(float));
            host.dispatch(index, x0, y0, tw, th, copy.data());
        }
        host.finishAll();
        return true;
    }

    // ---- the synthetic sources ------------------------------------------------------------------------------------------------
    // a deterministic value in [0, 1) per (x, y, channel)
    inline float unit(int x, int y, int c) { return (float)((x * 7 + y * 13 + c * 5 + (x * y) % 11) % 97) / 97.0f; }

    // for the quantized outputs: values from below the lowest to above the highest quantized code, so both clamps are used
    inline std::vector<float> quantizedSource(int w, int h, int ns) {
        std::vector<float> v((size_t)w * h * ns);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                for (int c = 0; c < ns; c++)
                    v[((size_t)y * w + x) * ns + c] = unit(x, y, c) * 1.5f - 0.25f;
        return v;
    }

    // for the float and half outputs: the values where half conversion is delicate, at the head of the image (channel 0 of the first
    // pixels), then the ordinary values. `finiteOnly` drops NaN and the infinities (RGBE has no such codes).
    inline std::vector<float> floatSource(int w, int h, int ns, bool finiteOnly) {
        std::vector<float> v((size_t)w * h * ns);
        for (int y = 0; y < h; y++)
            for (int x = 0; x < w; x++)
                for (int c = 0; c < ns; c++)
                    v[((size_t)y * w + x) * ns + c] = unit(x, y, c) * 4.0f;
        std::vector<float> special = {
            1.0f + std::ldexp(1.0f, -11),        // an exact half tie, rounds to even (1.0)
            1.0f + 3.0f * std::ldexp(1.0f, -11), // an exact half tie, rounds to even (1 + 2^-9)
            std::ldexp(1.0f, -24),               // the smallest half denormal
            std::ldexp(1.0f, -25),               // half of it: a tie, rounds to 0
            3.0f * std::ldexp(1.0f, -25),        // a tie between two denormals
            std::ldexp(1.0f, -14),               // the smallest half normal
            6.0e-5f,                             // a half denormal
            65504.0f,                            // the largest half
            65519.0f,                            // rounds down to 65504
            65520.0f,                            // a tie that rounds up to infinity
            1.0e10f,                             // overflows half
            -1.0e10f,
            -0.0f,
            1.0f / 3.0f,
        };
        if (!finiteOnly) {
            special.push_back(INFINITY);
            special.push_back(-INFINITY);
            special.push_back(NAN);
        }
        for (size_t i = 0; i < special.size() && i < (size_t)w * h; i++)
            v[i * ns] = special[i];
        return v;
    }

} // namespace dh
