// tests/unit/imageio/test_bake_exclusions.cpp -- ImageIO_BakeExclusions (spec 048, T089, FR-017, US3-4, P-Q4).
//
// With "imageio" "oiio" selected, the (bits, channels) combinations OIIO cannot write the way the texture reader expects are
// baked by the LEGACY writer instead (data-model section 4: 16-bit with 3 or more channels, 8-bit and float with 5 or more):
//   - the file is byte-identical to the legacy bake of the same source, and carries no OIIO SOFTWARE tag;
//   - the CODE_INFO message of contracts/bake-writer.md is reported exactly once per call, BEFORE any output file exists
//     (the handler checks the output path at the moment the message arrives);
//   - no OIIO-written file of an excluded kind is ever produced.
// The allowed combinations (16-bit with 1 or 2 channels included: written with tiff:write_extrasamples 0) are written by OIIO and
// report no such message. Every kind (texture, shadow map, cylinder, cubic and spherical environment) is covered by its own cases.
#include "bake_cases.h"
#include "bake_compare.h"

using namespace sel;

struct Seen {
        std::string message;
        bool outputExisted;
};
static std::vector<Seen> seen;
static std::string currentOutput;

static void handler(RtInt, RtInt, const char *message) {
    seen.push_back({message, exists(currentOutput)});
    // also keep the shared counters of the harness meaningful for other captures
    capture(0, 0, message);
}

static int legacyWriterMessages(const char *expectedText, bool &beforeOutput) {
    int n = 0;
    beforeOutput = true;
    for (const Seen &s : seen)
        if (s.message.find("baked by the legacy writer") != std::string::npos) {
            if (s.message.find(expectedText) != std::string::npos)
                n++;
            beforeOutput = beforeOutput && !s.outputExisted;
        }
    return n;
}

int main() {
    RiBegin(RI_NULL);
    RiErrorHandler(handler);

    int excluded = 0, allowed = 0;
    for (const bakecases::BakeCase &c : bakecases::cases()) {
        const std::string legacy = scratch() + "/legacy-" + c.name + ".tex";
        const std::string oiio = scratch() + "/oiio-" + c.name + ".tex";
        currentOutput = legacy;
        bakecases::bakeCase(c, legacy, nullptr);
        seen.clear();
        currentOutput = oiio;
        bakecases::bakeCase(c, oiio, "oiio");
        CHECK(exists(oiio));
        if (!exists(oiio) || !exists(legacy))
            continue;

        char expected[200];
        if (c.bits == 16)
            snprintf(expected, sizeof(expected), "16-bit %d-channel textures are baked by the legacy writer (OpenImageIO writes PHOTOMETRIC_RGB, which the texture reader scales; GitHub #18)", c.channels);
        else
            snprintf(expected, sizeof(expected), "%d-channel textures are baked by the legacy writer (OpenImageIO writes PHOTOMETRIC_RGB; legacy leaves it unset)", c.channels);

        bool before = true;
        int messages = legacyWriterMessages(expected, before);
        if (c.excluded) {
            excluded++;
            CHECK(messages == 1);                                  // reported once per call
            CHECK(before);                                         // and before any output file existed
            CHECK(bytesOf(oiio) == bytesOf(legacy));               // the legacy writer wrote it, byte for byte
            CHECK(!bakecmp::writtenByOiio(oiio));                  // never an OIIO-written file of an excluded kind
            if (messages != 1 || bytesOf(oiio) != bytesOf(legacy))
                fprintf(stderr, "case %s: %d message(s) of the expected text, output %s the legacy bake\n", c.name, messages, bytesOf(oiio) == bytesOf(legacy) ? "equals" : "differs from");
        } else {
            allowed++;
            CHECK(messages == 0);                                  // no gate message for a combination OIIO writes
            CHECK(bakecmp::writtenByOiio(oiio));                   // written by OIIO
        }
    }
    RiEnd();
    CHECK(excluded >= 10);
    CHECK(allowed >= 25);
    printf("ImageIO_BakeExclusions: %d excluded and %d allowed cases\n", excluded, allowed);
    return finish("ImageIO_BakeExclusions");
}
