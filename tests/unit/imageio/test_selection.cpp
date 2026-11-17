// tests/unit/imageio/test_selection.cpp -- the C++ half of ImageIO_Selection (spec 048, T073, FR-013, P-Q6, P-Q15).
//
// Backend selection is visible and explicit: Option "imageio" "backend", the per-call "imageio" parameter. Precedence
// is the call's parameter, then the Option, then "legacy". It is observed through DECODING (the OIIO writer arrives in
// Phase 6): the 4x4 RGB PNG with a tRNS chunk bakes to 3 samples per pixel when the legacy decoder read it and to 4
// when OIIO did (D-PNG-TRNS), and the RLE RGBE file only decodes on OIIO (D-RGBE-RLE), so its baked texture equals the
// bake of the same pixels written flat only when OIIO decoded it.
//
// One RiBegin/RiEnd session per case; the case is the first argument: option, precedence, unknown_call.
#include "selection_harness.h"

using namespace sel;

static const char *kUnknownText = "Unknown image backend \"bogus\" (expected \"legacy\" or \"oiio\")";

// Option "imageio" "backend": parsed, and an unknown value reports and keeps the previous one
static void caseOption() {
    const std::string trns = fixture("differences/trns_rgb8_4x4.png");

    RiBegin(RI_NULL);
    RiErrorHandler(capture);

    // default: legacy decoded it (3 samples)
    CHECK(samplesPerPixel(bake(trns, "opt-default.tex", nullptr)) == 3);

    setBackend("oiio");
    CHECK(samplesPerPixel(bake(trns, "opt-oiio.tex", nullptr)) == 4);

    setBackend("legacy");
    CHECK(samplesPerPixel(bake(trns, "opt-legacy.tex", nullptr)) == 3);

    // an unknown value: one report naming it and the two valid ones, and the previous value stays
    setBackend("oiio");
    diagnostics().clear();
    setBackend("bogus");
    CHECK(countContaining(kUnknownText) == 1);
    CHECK(samplesPerPixel(bake(trns, "opt-after-bogus.tex", nullptr)) == 4);
    RiEnd();

    // an unknown value as the first: the default (legacy) stays
    RiBegin(RI_NULL);
    RiErrorHandler(capture);
    diagnostics().clear();
    setBackend("bogus");
    CHECK(countContaining(kUnknownText) == 1);
    CHECK(samplesPerPixel(bake(trns, "opt-bogus-first.tex", nullptr)) == 3);
    RiEnd();
}

// the call's "imageio" parameter, then the Option, then legacy
static void casePrecedence() {
    const std::string trns = fixture("differences/trns_rgb8_4x4.png");
    const std::string rle = fixture("differences/rle_16x4_rle.hdr");
    const std::string flat = fixture("differences/rle_16x4_flat.hdr");

    // the reference: the flat file decodes the same pixels on the legacy path
    RiBegin(RI_NULL);
    const std::string flatTex = bake(flat, "prec-flat.tex", nullptr);
    CHECK(exists(flatTex));
    const std::vector<unsigned char> flatBytes = bytesOf(flatTex);
    CHECK(!flatBytes.empty());

    // 1. no Option, no call: legacy
    CHECK(samplesPerPixel(bake(trns, "prec-1-trns.tex", nullptr)) == 3);
    {
        std::string t = bake(rle, "prec-1-rle.tex", nullptr);
        CHECK(!exists(t) || bytesOf(t) != flatBytes); // legacy cannot read the RLE file (GitHub #171)
    }
    // 2. the call alone: oiio
    CHECK(samplesPerPixel(bake(trns, "prec-2-trns.tex", "oiio")) == 4);
    CHECK(bytesOf(bake(rle, "prec-2-rle.tex", "oiio")) == flatBytes);
    RiEnd();

    // 3. the Option alone: oiio
    RiBegin(RI_NULL);
    setBackend("oiio");
    CHECK(samplesPerPixel(bake(trns, "prec-3-trns.tex", nullptr)) == 4);
    CHECK(bytesOf(bake(rle, "prec-3-rle.tex", nullptr)) == flatBytes);
    // 4. the call wins over the Option: oiio Option, legacy call
    CHECK(samplesPerPixel(bake(trns, "prec-4-trns.tex", "legacy")) == 3);
    {
        std::string t = bake(rle, "prec-4-rle.tex", "legacy");
        CHECK(!exists(t) || bytesOf(t) != flatBytes);
    }
    RiEnd();

    // 5. legacy Option, oiio call: oiio
    RiBegin(RI_NULL);
    setBackend("legacy");
    CHECK(samplesPerPixel(bake(trns, "prec-5-trns.tex", "oiio")) == 4);
    CHECK(bytesOf(bake(rle, "prec-5-rle.tex", "oiio")) == flatBytes);
    RiEnd();
}

// an unknown per-call value is reported and names the value
static void caseUnknownCall() {
    RiBegin(RI_NULL);
    RiErrorHandler(capture);
    diagnostics().clear();
    bake(fixture("differences/trns_rgb8_4x4.png"), "unk-call.tex", "bogus");
    CHECK(countContaining("bogus") >= 1);
    RiEnd();
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: test_selection <option|precedence|unknown_call>\n");
        return 2;
    }
    if (strcmp(argv[1], "option") == 0)
        caseOption();
    else if (strcmp(argv[1], "precedence") == 0)
        casePrecedence();
    else if (strcmp(argv[1], "unknown_call") == 0)
        caseUnknownCall();
    else {
        fprintf(stderr, "unknown case %s\n", argv[1]);
        return 2;
    }
    return finish(argv[1]);
}
