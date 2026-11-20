// tests/unit/imageio/test_display_load.cpp -- ImageIO_DisplayLoad (spec 048, T104, FR-008, US4-2).
//
// oiio.dsply is an ordinary display module: CDisplayHost loads it, version-checks it (checkPluginVersion, display ABI 2.0.0) and starts it;
// the module exports the version query orDisplayAbiVersion and the entry orDisplayPlugin. For a TIFF display the renderer-provided NP and Nl
// matrices become the Pixar worldtoscreen and worldtocamera matrices, read back with libtiff and equal to the legacy driver's. The include
// boundary of the module's source is a separate script test (check_display_includes.cmake, in the same test registration).
//   argv: nothing (the modules come from the environment, see display_harness.h)
#include "display_harness.h"

using namespace dh;

namespace {

    void exportsTheVersionQuery(const std::string &moduleFile) {
        void *handle = dlopen(moduleFile.c_str(), RTLD_NOW | RTLD_LOCAL);
        if (handle == nullptr) {
            DFAIL("dlopen(%s) failed: %s", moduleFile.c_str(), dlerror());
            return;
        }
        typedef uint32_t (*VersionQuery)(void);
        VersionQuery query = (VersionQuery)dlsym(handle, "orDisplayAbiVersion");
        DCHECK(query != nullptr);
        if (query != nullptr)
            DCHECK(query() == OR_DISPLAY_ABI_VERSION); // 2.0.0, no version step
        DCHECK(dlsym(handle, "orDisplayPlugin") != nullptr);
        dlclose(handle);
    }

    bool pixarMatrices(const std::string &path, float toScreen[16], float toCamera[16]) {
        TIFF *t = TIFFOpen(path.c_str(), "r");
        if (t == nullptr)
            return false;
        float *p = nullptr;
        bool ok = TIFFGetField(t, TIFFTAG_PIXAR_MATRIX_WORLDTOSCREEN, &p) == 1 && p != nullptr;
        if (ok)
            memcpy(toScreen, p, 16 * sizeof(float));
        p = nullptr;
        ok = ok && TIFFGetField(t, TIFFTAG_PIXAR_MATRIX_WORLDTOCAMERA, &p) == 1 && p != nullptr;
        if (ok)
            memcpy(toCamera, p, 16 * sizeof(float));
        TIFFClose(t);
        return ok;
    }

} // namespace

int main() {
    installSink();
    if (!moduleExists("OIIO_DSPLY") || !moduleExists("FILE_DSPLY"))
        return 1;
    const std::string dir = workDir("load");

    // loads through the host, version-checked, and writes a file
    Run oiio;
    oiio.module = module("OIIO_DSPLY");
    oiio.type = "oiio";
    oiio.out = dir + "/load.tif";
    oiio.ns = 4;
    for (int i = 0; i < 16; i++) {
        oiio.nl[i] = 100.0f + (float)i; // distinct per matrix and per element
        oiio.np[i] = 200.0f + (float)i;
    }
    const std::vector<float> src = quantizedSource(16, 16, 4);
    DCHECK(runDisplay(oiio, src)); // CDisplayHost::startDisplay: dlopen, checkPluginVersion at 2.0.0, bind, start
    DCHECK(isTiff(oiio.out));
    DCHECK(!sink().has("version")); // no version warning: the module declares its version

    exportsTheVersionQuery(oiio.module);

    // NP -> worldtoscreen, Nl -> worldtocamera, equal to the legacy driver's
    Run legacy = oiio;
    legacy.module = module("FILE_DSPLY");
    legacy.type = "file";
    legacy.out = dir + "/load-legacy.tif";
    DCHECK(runDisplay(legacy, src));
    float oS[16], oC[16], lS[16], lC[16];
    const bool haveO = pixarMatrices(oiio.out, oS, oC), haveL = pixarMatrices(legacy.out, lS, lC);
    DCHECK(haveL);
    DCHECK(haveO);
    if (haveO && haveL) {
        DCHECK(memcmp(oS, lS, sizeof(oS)) == 0);
        DCHECK(memcmp(oC, lC, sizeof(oC)) == 0);
        DCHECK(memcmp(oS, oiio.np, sizeof(oS)) == 0);
        DCHECK(memcmp(oC, oiio.nl, sizeof(oC)) == 0);
    }

    if (failures() != 0) {
        fprintf(stderr, "ImageIO_DisplayLoad: FAILED, %d check(s)\n%s", failures(), sink().all().c_str());
        return 1;
    }
    printf("ImageIO_DisplayLoad: ok\n");
    return 0;
}
