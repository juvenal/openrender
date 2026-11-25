/*
 * Spec 035, Phase 3 (T059), label closuremode: closuremode with a bound area light (spec 057) and with the photon hider.
 *
 *   AreaLight_interp / AreaLight_jit   a receiver (cl_lit, capable as cl_litcap) lit by a bound area light (AreaLightSource + an emitter quad):
 *                            `color` and `both` equal the no-mention render (the spec 057 path, the same code) byte for byte; the `closures`
 *                            receiver draws no emitter sample and runs no light shader (the wrapped light's counter is 0), and is black;
 *                            the color receiver's pixels equal the scene without the closures receiver (the receivers are side by side and
 *                            neither is in the other's shadow or reflection rays: R-9, R19) (US5 AS2, AS3, SC-007)
 *   Photon                   a photon pass gives the same .gpm bytes in every mode (US5 AS4): the photon hider never calls shade()
 *
 * Red on the starting tree: does not compile (closureConsumer.h, the closuremode token).
 */
#include <cstdio>
#include <fstream>
#include <iterator>
#include <string>
#include <unistd.h>

#include "mode_support.h"

using namespace clmode;

namespace {

    // The emitter: a quad above the frame (y = 3) facing -y, between the receivers' plane (z = 1) and the camera
    void emitter(const char *format) {
        RtPoint quad[4] = {{-1, 3, 0.2f}, {1, 3, 0.2f}, {1, 3, 0.8f}, {-1, 3, 0.8f}};
        RtFloat intensity = 200.0f;
        RiAttributeBegin();
        const RtLightHandle light = RiAreaLightSource((std::string("cl_arealight_") + format).c_str(), "intensity", &intensity, RI_NULL);
        RiPolygon(4, RI_P, (RtPointer)quad, RI_NULL);
        RiAttributeEnd();
        RiIlluminate(light, RI_TRUE); // the light is on only inside the block that made it, unless it is switched on outside
    }

    SScene receiver(const char *format, const char *mode) {
        SScene s;
        s.hider = "raytrace";
        s.format = format;
        s.surface = "cl_litcap"; // capable: the same shader whatever the mode, so the images can be compared
        s.attributeMode = std::string(mode) == "none" ? "" : mode;
        s.consumer = true;
        s.size = 16;
        s.extra = [format]() { emitter(format); };
        return s;
    }

    void sectionAreaLight(const char *format) {
        const SRun none = render(receiver(format, "none"));
        // the reference of spec 057 is the no-mention render itself; it must be lit (the control)
        CL_CHECK(none.lightRuns > 0);
        CL_CHECK(none.image.at(8, 8, 0) > 0.0f);
        const SRun color = render(receiver(format, "color"));
        CL_CHECK(color.image == none.image);
        const SRun both = render(receiver(format, "both"));
        CL_CHECK(both.image == none.image);
        CL_CHECK(both.lightRuns == none.lightRuns);
        const SRun closures = render(receiver(format, "closures"));
        CL_CHECK(closures.lightRuns == 0); // no emitter sample, no light shader
        for (int y = 0; y < closures.image.height; ++y)
            for (int x = 0; x < closures.image.width; ++x)
                CL_CHECK(closures.image.at(x, y, 0) == 0.0f && closures.image.at(x, y, 1) == 0.0f && closures.image.at(x, y, 2) == 0.0f);

        // two receivers side by side: the left one color, the right one closures; the left one's pixels equal the scene with no right receiver
        SScene alone = receiver(format, "none");
        alone.halfFront = true;
        const SRun leftOnly = render(alone);
        SScene pair = receiver(format, "none");
        pair.halfFront = true;
        pair.extra = [format]() {
            emitter(format);
            RtPoint right[4] = {{0, -4, 1}, {4, -4, 1}, {4, 4, 1}, {0, 4, 1}};
            RiAttributeBegin();
            RtString mode = (RtString) "closures";
            RiAttribute(RI_SHADE, RI_CLOSUREMODE, &mode, RI_NULL);
            RtColor c = {0.25f, 0.5f, 0.75f};
            RiColor(c);
            RiSurface((std::string("cl_litcap_") + format).c_str(), RI_NULL);
            RiPolygon(4, RI_P, (RtPointer)right, RI_NULL);
            RiAttributeEnd();
        };
        const SRun both2 = render(pair);
        const int w = both2.image.width, h = both2.image.height;
        int different = 0;
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w / 2; ++x)
                for (int k = 0; k < 4; ++k)
                    if (both2.image.at(x, y, k) != leftOnly.image.at(x, y, k))
                        ++different;
        CL_CHECK(different == 0);
    }

    // The photon pass: one global map per mode, compared byte for byte
    std::string photonMap(const std::string &mode, const std::string &file) {
        RtInt emit = 2000, threads = 1;
        RtString globalMap = const_cast<RtString>(file.c_str());
        RtPoint receiver[4] = {{-4, -4, 1}, {4, -4, 1}, {4, 4, 1}, {-4, 4, 1}};
        RiBegin(RI_NULL);
        RiOption(RI_LIMITS, RI_NUMTHREADS, &threads, RI_NULL);
        RiHider("photon", RI_EMIT, &emit, RI_NULL);
        RiFormat(8, 8, 1);
        RiWorldBegin();
        RiAttribute(RI_PHOTON, RI_GLOBALMAP, &globalMap, RI_NULL);
        RtFloat lightIntensity = 300.0f;
        RiAttributeBegin();
        RiTranslate(0, 0, 0.5f);
        RiLightSource("pointlight", "intensity", &lightIntensity, RI_NULL);
        RiAttributeEnd();
        RiAttributeBegin();
        if (!mode.empty()) {
            RtString m = const_cast<RtString>(mode.c_str());
            RiAttribute(RI_SHADE, RI_CLOSUREMODE, &m, RI_NULL);
        }
        RiSurface("matte", RI_NULL);
        RiPolygon(4, RI_P, (RtPointer)receiver, RI_NULL);
        RiAttributeEnd();
        RiWorldEnd();
        RiEnd();
        std::ifstream f(file, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }

    void sectionPhoton() {
        const std::string none = photonMap("", "photon-none.gpm");
        CL_CHECK(!none.empty());
        for (const char *mode : {"color", "both", "closures"}) {
            const std::string bytes = photonMap(mode, std::string("photon-") + mode + ".gpm");
            CL_CHECK(bytes == none);
        }
        for (const char *mode : {"none", "color", "both", "closures"})
            unlink((std::string("photon-") + mode + ".gpm").c_str());
    }

} // namespace

int main(int argc, char **argv) {
    cldbl::registerBackend();
    const std::string which = argc > 1 ? argv[1] : "";
    if (which == "AreaLight_interp")
        sectionAreaLight("rslo");
    else if (which == "AreaLight_jit")
        sectionAreaLight("slo");
    else if (which == "Photon")
        sectionPhoton();
    else {
        std::printf("usage: test_mode_arealight <AreaLight_interp|AreaLight_jit|Photon>\n");
        cldbl::unregisterBackend();
        return 2;
    }
    cldbl::unregisterBackend();
    return CL_RESULT(("ClosureMode_" + which).c_str());
}
