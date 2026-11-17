// tests/unit/imageio/bake_compare.h -- reads two baked textures with libtiff and compares them directory by directory (spec 048,
// Phase 6, contracts/bake-writer.md). Used by ImageIO_BakeParity and ImageIO_BakeExclusions. Test code reads with libtiff only; it
// never goes through the OIIO reader, so a writer is judged by a reader that is neither of them.
#pragma once

#include "selection_harness.h"

#include <tiffio.h>

#include <algorithm>
#include <cstdarg>
#include <cstring>
#include <string>
#include <vector>

namespace bakecmp {

    inline bool str(TIFF *t, ttag_t tag, std::string &out) {
        char *s = nullptr;
        if (TIFFGetField(t, tag, &s) != 1 || s == nullptr)
            return false;
        out = s;
        return true;
    }
    inline bool matrix(TIFF *t, ttag_t tag, float m[16]) {
        float *p = nullptr;
        if (TIFFGetField(t, tag, &p) != 1 || p == nullptr)
            return false;
        memcpy(m, p, 16 * sizeof(float));
        return true;
    }
    inline bool u32(TIFF *t, ttag_t tag, uint32_t &v) { return TIFFGetField(t, tag, &v) == 1; }
    inline bool u16(TIFF *t, ttag_t tag, uint16_t &v) { return TIFFGetField(t, tag, &v) == 1; }
    inline bool f32(TIFF *t, ttag_t tag, float &v) { return TIFFGetField(t, tag, &v) == 1; }

    inline void bakeFail(const char *name, int dir, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
    inline void bakeFail(const char *name, int dir, const char *fmt, ...) {
        sel::failures()++;
        fprintf(stderr, "%s dir %d: ", name, dir);
        va_list ap;
        va_start(ap, fmt);
        vfprintf(stderr, fmt, ap);
        va_end(ap);
        fprintf(stderr, "\n");
    }

    // The SOFTWARE tag of the first directory: OIIO writes "OpenImageIO ..."; the legacy writer writes none.
    inline bool writtenByOiio(const std::string &path) {
        TIFF *t = TIFFOpen(path.c_str(), "r");
        if (t == nullptr)
            return false;
        std::string s;
        bool oiio = str(t, TIFFTAG_SOFTWARE, s) && s.rfind("OpenImageIO", 0) == 0;
        TIFFClose(t);
        return oiio;
    }

    inline bool hasSoftware(const std::string &path) {
        TIFF *t = TIFFOpen(path.c_str(), "r");
        if (t == nullptr)
            return false;
        std::string s;
        bool has = str(t, TIFFTAG_SOFTWARE, s);
        TIFFClose(t);
        return has;
    }

    // Compares the tags of every directory per the table of contracts/bake-writer.md and every in-image sample of every tile.
    // Failures are counted through CHECK with the case name; returns the number of directories compared.
    inline int compare(const std::string &legacyPath, const std::string &oiioPath, const char *name) {
        TIFF *a = TIFFOpen(legacyPath.c_str(), "r");
        TIFF *b = TIFFOpen(oiioPath.c_str(), "r");
        if (a == nullptr || b == nullptr) {
            sel::failures()++;
            fprintf(stderr, "%s: cannot open %s%s\n", name, a == nullptr ? "the legacy bake " : "", b == nullptr ? "the OIIO bake" : "");
            if (a != nullptr)
                TIFFClose(a);
            if (b != nullptr)
                TIFFClose(b);
            return 0;
        }
        int dirsA = TIFFNumberOfDirectories(a), dirsB = TIFFNumberOfDirectories(b);
        if (dirsA != dirsB) {
            sel::failures()++;
            fprintf(stderr, "%s: directory count %d (legacy) against %d (oiio)\n", name, dirsA, dirsB);
        }
        const int dirs = std::min(dirsA, dirsB);
        // every comparison names the case and the directory
#define BAKE_FAIL(...) bakeFail(name, d, __VA_ARGS__)
        for (int d = 0; d < dirs; d++) {
            TIFFSetDirectory(a, (tdir_t)d);
            TIFFSetDirectory(b, (tdir_t)d);

            std::string sa, sb;
            // Pixar strings: present exactly where the legacy writer writes them, the same text
            for (ttag_t tag : {(ttag_t)TIFFTAG_PIXAR_TEXTUREFORMAT, (ttag_t)TIFFTAG_PIXAR_WRAPMODES}) {
                bool ha = str(a, tag, sa), hb = str(b, tag, sb);
                if (ha != hb || (ha && sa != sb))
                    BAKE_FAIL("Pixar tag %d: legacy %s '%s', oiio %s '%s'", (int)tag, ha ? "has" : "lacks", sa.c_str(), hb ? "has" : "lacks", sb.c_str());
            }
            // matrices: the same 16 floats
            for (ttag_t tag : {(ttag_t)TIFFTAG_PIXAR_MATRIX_WORLDTOCAMERA, (ttag_t)TIFFTAG_PIXAR_MATRIX_WORLDTOSCREEN}) {
                float ma[16], mb[16];
                bool ha = matrix(a, tag, ma), hb = matrix(b, tag, mb);
                if (ha != hb || (ha && memcmp(ma, mb, sizeof(ma)) != 0))
                    BAKE_FAIL("matrix tag %d: legacy %s, oiio %s, or the 16 floats differ", (int)tag, ha ? "has it" : "lacks it", hb ? "has it" : "lacks it");
            }
            // full size (also when equal to the data size: the P-Q3a rule) and the field of view
            for (ttag_t tag : {(ttag_t)TIFFTAG_PIXAR_IMAGEFULLWIDTH, (ttag_t)TIFFTAG_PIXAR_IMAGEFULLLENGTH}) {
                uint32_t va = 0, vb = 0;
                bool ha = u32(a, tag, va), hb = u32(b, tag, vb);
                if (ha != hb || (ha && va != vb))
                    BAKE_FAIL("full-size tag %d: legacy %s %u, oiio %s %u", (int)tag, ha ? "has" : "lacks", va, hb ? "has" : "lacks", vb);
            }
            {
                float fa = 0, fb = 0;
                bool ha = f32(a, (ttag_t)TIFFTAG_PIXAR_FOVCOT, fa), hb = f32(b, (ttag_t)TIFFTAG_PIXAR_FOVCOT, fb);
                if (ha != hb || (ha && fa != fb))
                    BAKE_FAIL("FOVCOT: legacy %s %g, oiio %s %g", ha ? "has" : "lacks", fa, hb ? "has" : "lacks", fb);
            }
            // geometry, tiling and sample layout
            uint32_t w[2] = {0, 0}, h[2] = {0, 0}, tw[2] = {0, 0}, th[2] = {0, 0};
            uint16_t spp[2] = {0, 0}, bps[2] = {0, 0}, fmt[2] = {0, 0}, photo[2] = {0, 0}, planar[2] = {0, 0}, orient[2] = {0, 0}, runit[2] = {0, 0};
            uint16_t comp[2] = {0, 0};
            float xres[2] = {0, 0}, yres[2] = {0, 0};
            TIFF *t[2] = {a, b};
            for (int i = 0; i < 2; i++) {
                TIFFGetFieldDefaulted(t[i], TIFFTAG_IMAGEWIDTH, &w[i]);
                TIFFGetFieldDefaulted(t[i], TIFFTAG_IMAGELENGTH, &h[i]);
                TIFFGetFieldDefaulted(t[i], TIFFTAG_TILEWIDTH, &tw[i]);
                TIFFGetFieldDefaulted(t[i], TIFFTAG_TILELENGTH, &th[i]);
                TIFFGetFieldDefaulted(t[i], TIFFTAG_SAMPLESPERPIXEL, &spp[i]);
                TIFFGetFieldDefaulted(t[i], TIFFTAG_BITSPERSAMPLE, &bps[i]);
                TIFFGetFieldDefaulted(t[i], TIFFTAG_SAMPLEFORMAT, &fmt[i]);
                TIFFGetFieldDefaulted(t[i], TIFFTAG_PHOTOMETRIC, &photo[i]);
                TIFFGetFieldDefaulted(t[i], TIFFTAG_PLANARCONFIG, &planar[i]);
                TIFFGetFieldDefaulted(t[i], TIFFTAG_ORIENTATION, &orient[i]);
                TIFFGetFieldDefaulted(t[i], TIFFTAG_RESOLUTIONUNIT, &runit[i]);
                TIFFGetFieldDefaulted(t[i], TIFFTAG_XRESOLUTION, &xres[i]);
                TIFFGetFieldDefaulted(t[i], TIFFTAG_YRESOLUTION, &yres[i]);
                TIFFGetFieldDefaulted(t[i], TIFFTAG_COMPRESSION, &comp[i]);
            }
#define BAKE_EQ(what, arr) \
    do { \
        if (arr[0] != arr[1]) \
            BAKE_FAIL("%s: legacy %g, oiio %g", what, (double)arr[0], (double)arr[1]); \
    } while (0)
            BAKE_EQ("IMAGEWIDTH", w);
            BAKE_EQ("IMAGELENGTH", h);
            BAKE_EQ("TILEWIDTH", tw);
            BAKE_EQ("TILELENGTH", th);
            BAKE_EQ("SAMPLESPERPIXEL", spp);
            BAKE_EQ("BITSPERSAMPLE", bps);
            BAKE_EQ("SAMPLEFORMAT", fmt);
            BAKE_EQ("PHOTOMETRIC", photo);
            BAKE_EQ("PLANARCONFIG", planar);
            BAKE_EQ("ORIENTATION", orient);
            BAKE_EQ("RESOLUTIONUNIT", runit);
            BAKE_EQ("XRESOLUTION", xres);
            BAKE_EQ("YRESOLUTION", yres);
            BAKE_EQ("COMPRESSION", comp);
            if (comp[1] != COMPRESSION_LZW)
                BAKE_FAIL("COMPRESSION is %d, expected LZW", (int)comp[1]);
            // PREDICTOR absent or 1 on both
            for (int i = 0; i < 2; i++) {
                uint16_t pred = 1;
                TIFFGetFieldDefaulted(t[i], TIFFTAG_PREDICTOR, &pred);
                if (pred != 1)
                    BAKE_FAIL("PREDICTOR %d on the %s bake, expected none", (int)pred, i == 0 ? "legacy" : "oiio");
            }
            // EXTRASAMPLES: the same count and values
            {
                uint16_t ca = 0, cb = 0;
                uint16_t *va = nullptr, *vb = nullptr;
                TIFFGetField(a, TIFFTAG_EXTRASAMPLES, &ca, &va);
                TIFFGetField(b, TIFFTAG_EXTRASAMPLES, &cb, &vb);
                if (ca != cb || (ca > 0 && memcmp(va, vb, ca * sizeof(uint16_t)) != 0))
                    BAKE_FAIL("EXTRASAMPLES: legacy count %d, oiio count %d, or the values differ", (int)ca, (int)cb);
            }
            // other tags: only SOFTWARE and DATETIME may appear on the OIIO bake alone, never a Pixar tag (checked above by presence)
            for (ttag_t tag : {(ttag_t)TIFFTAG_ARTIST, (ttag_t)TIFFTAG_DOCUMENTNAME, (ttag_t)TIFFTAG_IMAGEDESCRIPTION, (ttag_t)TIFFTAG_MAKE,
                               (ttag_t)TIFFTAG_MODEL, (ttag_t)TIFFTAG_HOSTCOMPUTER, (ttag_t)TIFFTAG_PAGENAME, (ttag_t)TIFFTAG_COPYRIGHT}) {
                std::string x, y;
                if (str(b, tag, y) && !str(a, tag, x))
                    BAKE_FAIL("the OIIO bake has tag %d ('%s') the legacy one lacks, and it is not SOFTWARE or DATETIME", (int)tag, y.c_str());
            }

            // every in-image sample of every tile (padding excluded)
            if (tw[0] == tw[1] && th[0] == th[1] && tw[0] > 0 && w[0] == w[1] && h[0] == h[1] && spp[0] == spp[1] && bps[0] == bps[1]) {
                const size_t pixel = (size_t)spp[0] * (bps[0] / 8);
                std::vector<unsigned char> ta((size_t)TIFFTileSize(a)), tb((size_t)TIFFTileSize(b));
                int bad = 0;
                for (uint32_t y = 0; y < h[0] && !bad; y += th[0])
                    for (uint32_t x = 0; x < w[0] && !bad; x += tw[0]) {
                        bool okA = TIFFReadTile(a, ta.data(), x, y, 0, 0) >= 0, okB = TIFFReadTile(b, tb.data(), x, y, 0, 0) >= 0;
                        if (!okA || !okB) {
                            BAKE_FAIL("tile (%u,%u) unreadable (legacy %d, oiio %d)", x, y, (int)okA, (int)okB);
                            bad = 1;
                            break;
                        }
                        const uint32_t validW = std::min(tw[0], w[0] - x), validH = std::min(th[0], h[0] - y);
                        for (uint32_t r = 0; r < validH && !bad; r++)
                            if (memcmp(&ta[(size_t)r * tw[0] * pixel], &tb[(size_t)r * tw[0] * pixel], validW * pixel) != 0) {
                                BAKE_FAIL("tile (%u,%u) row %u: samples differ", x, y, r);
                                bad = 1;
                            }
                    }
            }
        }
#undef BAKE_EQ
#undef BAKE_FAIL
        TIFFClose(a);
        TIFFClose(b);
        return dirs;
    }

} // namespace bakecmp
