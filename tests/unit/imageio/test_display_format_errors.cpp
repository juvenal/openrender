// tests/unit/imageio/test_display_format_errors.cpp -- ImageIO_DisplayFormatErrors (spec 048, T105, US4-3, P-Q7, P-Q8).
//
// How the oiio display driver chooses its format and what it refuses. The display type the renderer passes is "oiio"; the format comes from the
// display line's own `"string type"`, else from the extension of the file name (its last path component), else it is an error:
//     oiio display: cannot tell the format of "<name>"; give a known extension or "string type"
// and nothing is written. A `"string type"` wins over the extension. Known extensions: .tif .tiff .png .exr .hdr .pic. RGBE needs 3 or 4 samples
// (R, G, B; alpha dropped): 1, 2 or 5 samples is an error and nothing is written. No exception crosses start, data or finish.
//   argv: [noexr]     "noexr" is the build without OpenEXR (registered only there): type "openexr" fails naming the format
#include "display_harness.h"

using namespace dh;

namespace {

    const char *kUnknownText = "oiio display: cannot tell the format of";

    Run oiioRun(const std::string &file, int ns = 4, const char *mode = "rgba") {
        Run r;
        r.module = module("OIIO_DSPLY");
        r.type = "oiio";
        r.out = file;
        r.ns = ns;
        r.mode = mode;
        return r;
    }

    // the display fails to start: false, the message once with the exact text, no file
    void refused(const char *what, Run r, const std::string &exactName) {
        sink().messages.clear();
        const std::vector<float> src = quantizedSource(r.w, r.h, r.ns);
        bool started = true;
        try {
            started = runDisplay(r, src);
        } catch (...) {
            DFAIL("%s: an exception crossed the display interface", what);
        }
        DCHECK(!started);
        DCHECK(!exists(r.out));
        const std::string expected = std::string(kUnknownText) + " \"" + exactName + "\"; give a known extension or \"string type\"";
        if (!sink().has(expected))
            DFAIL("%s: expected the message\n  %s\ngot\n%s", what, expected.c_str(), sink().all().c_str());
    }

    void usual(const std::string &dir) {
        // no "string type": the extension decides (the renderer-provided "type" is the display type, "oiio", and must not matter)
        {
            Run r = oiioRun(dir + "/ext.tif");
            DCHECK(runDisplay(r, quantizedSource(16, 16, 4)));
            DCHECK(isTiff(r.out));
        }
        {
            Run r = oiioRun(dir + "/ext.tiff");
            DCHECK(runDisplay(r, quantizedSource(16, 16, 4)));
            DCHECK(isTiff(r.out));
        }
        {
            Run r = oiioRun(dir + "/ext.png");
            DCHECK(runDisplay(r, quantizedSource(16, 16, 4)));
            DCHECK(isPng(r.out));
        }
#ifdef HAVE_OPENEXR
        {
            Run r = oiioRun(dir + "/ext.exr");
            r.quantizer[1] = r.quantizer[3] = 0;
            DCHECK(runDisplay(r, quantizedSource(16, 16, 4)));
            DCHECK(isExr(r.out));
        }
#endif
        for (const char *ext : {"hdr", "pic"}) {
            Run r = oiioRun(dir + "/ext." + ext, 3, "rgb");
            r.quantizer[1] = r.quantizer[3] = 0;
            DCHECK(runDisplay(r, floatSource(16, 16, 3, true)));
            DCHECK(isHdr(r.out));
        }

        // "string type" wins over the extension, both ways
        {
            Run r = oiioRun(dir + "/wins.tif");
            r.params.push_back(stringParam("type", "png"));
            DCHECK(runDisplay(r, quantizedSource(16, 16, 4)));
            DCHECK(isPng(r.out));
            DCHECK(!isTiff(r.out));
        }
        {
            Run r = oiioRun(dir + "/wins.png");
            r.params.push_back(stringParam("type", "tiff"));
            DCHECK(runDisplay(r, quantizedSource(16, 16, 4)));
            DCHECK(isTiff(r.out));
        }
        // "string type" alone makes a name with no known extension writable
        {
            Run r = oiioRun(dir + "/typed.foo");
            r.params.push_back(stringParam("type", "png"));
            DCHECK(runDisplay(r, quantizedSource(16, 16, 4)));
            DCHECK(isPng(r.out));
        }
        {
            Run r = oiioRun(dir + "/typed-noext");
            r.params.push_back(stringParam("type", "tiff"));
            DCHECK(runDisplay(r, quantizedSource(16, 16, 4)));
            DCHECK(isTiff(r.out));
        }
    }

    void unknown(const std::string &dir) {
        refused("unknown extension", oiioRun(dir + "/x.foo"), dir + "/x.foo");
        refused("no extension", oiioRun(dir + "/x"), dir + "/x");
        // a dot in a directory name is not an extension
        mkdir((dir + "/dot.dir").c_str(), 0755);
        refused("a dot in the directory only", oiioRun(dir + "/dot.dir/x"), dir + "/dot.dir/x");
        // an unknown own "string type": the same refusal (the renderer's own "type" parameter, the display type, does not count)
        Run r = oiioRun(dir + "/y.tif");
        r.params.push_back(stringParam("type", "bogus"));
        refused("unknown string type", r, dir + "/y.tif");
    }

    void rgbeSamples(const std::string &dir) {
        // 1, 2 and 5 samples: refused, nothing written
        for (int ns : {1, 2, 5}) {
            Run r = oiioRun(dir + "/bad" + std::to_string(ns) + ".hdr", ns, "custom");
            r.quantizer[1] = r.quantizer[3] = 0;
            sink().messages.clear();
            bool started = true;
            try {
                started = runDisplay(r, floatSource(16, 16, ns, true));
            } catch (...) {
                DFAIL("RGBE with %d samples: an exception crossed the display interface", ns);
            }
            DCHECK(!started);
            DCHECK(!exists(r.out));
            DCHECK(sink().has("oiio display"));
        }
        // 4 samples: R, G, B written, alpha dropped. The file equals the one written from the same RGB with 3 samples.
        const std::vector<float> rgba = floatSource(16, 16, 4, true);
        std::vector<float> rgb((size_t)16 * 16 * 3);
        for (size_t i = 0; i < (size_t)16 * 16; i++)
            for (int c = 0; c < 3; c++)
                rgb[i * 3 + c] = rgba[i * 4 + c];
        Run four = oiioRun(dir + "/four.hdr", 4, "rgba");
        four.quantizer[1] = four.quantizer[3] = 0;
        Run three = oiioRun(dir + "/three.hdr", 3, "rgb");
        three.quantizer[1] = three.quantizer[3] = 0;
        DCHECK(runDisplay(four, rgba));
        DCHECK(runDisplay(three, rgb));
        Img a = decodeRgbe(three.out), b = decodeRgbe(four.out);
        DCHECK(a.ok() && b.ok());
        if (a.ok() && b.ok()) {
            DCHECK(b.n == 3);
            compareImages("rgbe 4 samples against 3", a, b);
        }
    }

    // hostile starts: each returns false, nothing escapes. A negative height makes the base class's allocation throw; a missing directory makes
    // the writer fail to create the file.
    void noExceptions(const std::string &dir) {
        for (const char *ext : {"tif", "png", "hdr"}) {
            Run r = oiioRun(dir + "/neg." + ext, std::string(ext) == "hdr" ? 3 : 4, std::string(ext) == "hdr" ? "rgb" : "rgba");
            r.h = -1;
            bool started = true;
            try {
                started = runDisplay(r, std::vector<float>());
            } catch (...) {
                DFAIL("height -1 (%s): an exception crossed the display interface", ext);
            }
            DCHECK(!started);
            DCHECK(!exists(r.out));

            Run m = oiioRun(dir + "/no/such/dir/out." + ext, std::string(ext) == "hdr" ? 3 : 4, std::string(ext) == "hdr" ? "rgb" : "rgba");
            sink().messages.clear();
            started = true;
            try {
                started = runDisplay(m, floatSource(16, 16, m.ns, true));
            } catch (...) {
                DFAIL("missing directory (%s): an exception crossed the display interface", ext);
            }
            DCHECK(!started);
            DCHECK(!sink().messages.empty()); // reported, not silent
        }
    }

    void noExr(const std::string &dir) {
        for (const std::string &file : {dir + "/n.exr", dir + "/n.dat"}) {
            Run r = oiioRun(file);
            r.params.push_back(stringParam("type", "openexr"));
            r.quantizer[1] = r.quantizer[3] = 0;
            sink().messages.clear();
            bool started = true;
            try {
                started = runDisplay(r, quantizedSource(16, 16, 4));
            } catch (...) {
                DFAIL("openexr without OpenEXR: an exception crossed the display interface");
            }
            DCHECK(!started);
            DCHECK(!exists(file));
            const std::string all = sink().all();
            DCHECK(all.find("OpenEXR") != std::string::npos || all.find("openexr") != std::string::npos); // names the format
        }
        // the extension alone, without a "string type", is the same refusal
        Run r = oiioRun(dir + "/m.exr");
        sink().messages.clear();
        DCHECK(!runDisplay(r, quantizedSource(16, 16, 4)));
        DCHECK(!exists(r.out));
    }

} // namespace

int main(int argc, char **argv) {
    installSink();
    if (!moduleExists("OIIO_DSPLY"))
        return 1;
    const bool noexr = argc == 2 && std::string(argv[1]) == "noexr";
    const std::string dir = workDir(noexr ? "formats_noexr" : "formats");
    if (noexr) {
        noExr(dir);
    }
    else {
        usual(dir);
        unknown(dir);
        rgbeSamples(dir);
        noExceptions(dir);
    }
    if (failures() != 0) {
        fprintf(stderr, "ImageIO_DisplayFormatErrors: FAILED, %d check(s)\n", failures());
        return 1;
    }
    printf("ImageIO_DisplayFormatErrors%s: ok\n", noexr ? " (no OpenEXR)" : "");
    return 0;
}
