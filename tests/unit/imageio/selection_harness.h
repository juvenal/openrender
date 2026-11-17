// tests/unit/imageio/selection_harness.h -- the shared helpers of the selection tests (spec 048, Phase 5).
//
// The programs run one RiBegin/RiEnd session per case (the case is argv[1]), bake a small source image through the RI
// calls the scene interface exposes (RiOption, RiMakeTexture) and look at the baked TIFF with libtiff. The backend that
// decoded the source is observed through the decoded content (a tRNS PNG decodes to 3 channels on the legacy path and
// to 4 on OIIO's; the RLE RGBE file only decodes on OIIO's), never through a member of the options block.
#pragma once

#include "ri.h"

#include <tiffio.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

namespace sel {

    inline int &failures() {
        static int n = 0;
        return n;
    }

#define CHECK(cond) \
    do { \
        if (!(cond)) { \
            sel::failures()++; \
            fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

    struct Diagnostic {
            int code, severity;
            std::string message;
    };
    inline std::vector<Diagnostic> &diagnostics() {
        static std::vector<Diagnostic> d;
        return d;
    }
    inline void capture(RtInt code, RtInt severity, const char *message) {
        diagnostics().push_back({code, severity, message});
    }
    // how many captured diagnostics contain text
    inline int countContaining(const char *text) {
        int n = 0;
        for (const Diagnostic &d : diagnostics())
            if (d.message.find(text) != std::string::npos)
                n++;
        return n;
    }

    inline std::string envOr(const char *name, const char *fallback) {
        const char *v = getenv(name);
        return v != nullptr ? v : fallback;
    }
    inline std::string fixture(const char *relative) { return envOr("IMAGEIO_FIXTURES_DIR", "tests/unit/imageio/fixtures") + "/" + relative; }

    // a scratch directory for this process, left to the system's temp cleaning
    inline std::string scratch() {
        static std::string dir;
        if (dir.empty()) {
            std::string t = envOr("TMPDIR", "/tmp") + "/or048_select_XXXXXX";
            std::vector<char> b(t.begin(), t.end());
            b.push_back('\0');
            if (mkdtemp(b.data()) == nullptr) {
                fprintf(stderr, "cannot create a scratch directory\n");
                exit(2);
            }
            dir = b.data();
        }
        return dir;
    }

    inline bool exists(const std::string &p) { return access(p.c_str(), F_OK) == 0; }

    inline std::vector<unsigned char> bytesOf(const std::string &p) {
        std::vector<unsigned char> b;
        FILE *f = fopen(p.c_str(), "rb");
        if (f == nullptr)
            return b;
        unsigned char buf[8192];
        size_t n;
        while ((n = fread(buf, 1, sizeof(buf), f)) > 0)
            b.insert(b.end(), buf, buf + n);
        fclose(f);
        return b;
    }

    // SAMPLESPERPIXEL of the first directory of a baked texture, 0 if it cannot be read
    inline int samplesPerPixel(const std::string &p) {
        TIFF *t = TIFFOpen(p.c_str(), "r");
        if (t == nullptr)
            return 0;
        uint16_t spp = 0;
        TIFFGetFieldDefaulted(t, TIFFTAG_SAMPLESPERPIXEL, &spp);
        TIFFClose(t);
        return spp;
    }

    // Bakes `source` into scratch()/`name` with MakeTexture (every call of a process uses its own name: nothing is overwritten). `call` is the per-call "imageio" value, or nullptr for none.
    // Returns the output path.
    inline std::string bake(const std::string &source, const char *name, const char *call) {
        std::string out = scratch() + "/" + name;
        if (call != nullptr) {
            RtString value = const_cast<RtString>(call);
            RiMakeTexture(source.c_str(), out.c_str(), (RtToken) "periodic", (RtToken) "periodic", RiBoxFilter, 1, 1,
                          (RtToken) "string imageio", &value, RI_NULL);
        } else {
            RiMakeTexture(source.c_str(), out.c_str(), (RtToken) "periodic", (RtToken) "periodic", RiBoxFilter, 1, 1, RI_NULL);
        }
        return out;
    }

    inline void setBackend(const char *name) {
        RtString value = const_cast<RtString>(name);
        RiOption((RtToken) "imageio", (RtToken) "backend", &value, RI_NULL);
    }

    inline int finish(const char *name) {
        if (failures() != 0) {
            fprintf(stderr, "%s: FAILED, %d check(s)\n", name, failures());
            return 1;
        }
        printf("%s: ALL PASS\n", name);
        return 0;
    }

} // namespace sel
