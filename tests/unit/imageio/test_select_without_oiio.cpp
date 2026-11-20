// tests/unit/imageio/test_select_without_oiio.cpp -- the C++ half of ImageIO_SelectWithoutOIIO (spec 048, T074, FR-003, P-Q6).
//
// Registered only in a build WITHOUT OpenImageIO. Asking for the OIIO backend degrades as P-Q6 says:
//   case option: Option "imageio" "backend" ["oiio"] reports one error ("OpenImageIO support is not built into this
//                binary" and that the value stays "legacy"); the next bake is decoded by the legacy decoder.
//   case call:   MakeTexture ... "string imageio" ["oiio"] fails with the error and writes no file.
#include "selection_harness.h"

using namespace sel;

static const char *kNotBuilt = "OpenImageIO support is not built into this binary";

static void caseOption() {
    const std::string trns = fixture("differences/trns_rgb8_4x4.png");
    RiBegin(RI_NULL);
    RiErrorHandler(capture);
    diagnostics().clear();
    setBackend("oiio");
    CHECK(countContaining(kNotBuilt) == 1);
    CHECK(countContaining("stays \"legacy\"") == 1);
    // the value stayed legacy: the legacy decoder read the tRNS PNG (3 samples per pixel)
    CHECK(samplesPerPixel(bake(trns, "without-option.tex", nullptr)) == 3);
    RiEnd();
}

static void caseCall() {
    const std::string trns = fixture("differences/trns_rgb8_4x4.png");
    RiBegin(RI_NULL);
    RiErrorHandler(capture);
    diagnostics().clear();
    const std::string out = bake(trns, "without-call.tex", "oiio");
    CHECK(countContaining(kNotBuilt) >= 1);
    CHECK(!exists(out)); // the call failed and wrote nothing
    RiEnd();
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: test_select_without_oiio <option|call>\n");
        return 2;
    }
    if (strcmp(argv[1], "option") == 0)
        caseOption();
    else if (strcmp(argv[1], "call") == 0)
        caseCall();
    else {
        fprintf(stderr, "unknown case %s\n", argv[1]);
        return 2;
    }
    return finish(argv[1]);
}
