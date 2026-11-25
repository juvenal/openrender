/*
 * Spec 035, Phase 3 (T059), label closuremode: the lighting gate, through the delegating cldbl backend, for both formats.
 *
 *   Gate_interp / Gate_jit   cl_gate_probe.sl under `closures` (the gate closed) against the same scene under `color`, through the real
 *                            interpreter (rslo) and the real JIT (slo): ambient(), diffuse(), specular(), phong(), illuminance() with and
 *                            without a category, lightsource() in a loop, occlusion(), indirectdiffuse() and gather() read 0 under closures
 *                            (and have a non-zero positive control under color); trace(), transmission(), visibility(), environment(),
 *                            shadow() and specularbrdf() are not gated (equal to color's); a ray traced from the gated surface shades its
 *                            hit in a new state with the HIT object's own mode (the occluder runs the light); lightingSuppressed is 0 again
 *                            after the gated execution (the next object of the same frame, shaded later, is lit)
 *
 * Positive-control exception: under the interpreter indirectdiffuse() traces no ray (GitHub #173), so r_indirect has no positive control there.
 *
 * Red on the starting tree: does not compile (closureConsumer.h, the closuremode token).
 */
#include <cstdio>
#include <string>

#include "mode_support.h"

using namespace clmode;

namespace {

    void sectionGate(const char *format) {
        const bool jit = std::string(format) == "slo";
        SScene base;
        base.hider = "raytrace";
        base.format = format;
        base.surface = "cl_gatecap";
        base.light = true;
        base.ambient = true;
        base.occluder = true;
        base.occluderSurface = "cl_probe"; // lights not used by the occluder
        base.consumer = true;
        const SRun color = render(base);
        SScene g = base;
        g.attributeMode = "closures";
        const SRun closed = render(g);

        const char *gated[] = {"r_ambient", "r_diffuse", "r_specular", "r_phong", "r_illum", "r_illumcat", "r_lightsource", "r_occlusion", "r_indirect", "r_gather"};
        for (const char *name : gated) {
            const double control = color.outputMean.at(name);
            const bool indirectUnderInterpreter = std::string(name) == "r_indirect" && !jit;
            if (!indirectUnderInterpreter)
                CL_CHECK(control > 0.0); // the positive control: the builtin returns something under color, in this backend
            // Under the interpreter this zero assertion for r_indirect is NON-EVIDENTIAL until GitHub #173 is fixed: the interpreter's indirectdiffuse()
            // traces no ray, so it reads 0 under color as well and the zero proves nothing about the gate. The JIT run carries the evidence.
            CL_CHECK(closed.outputMean.at(name) == 0.0); // and 0 when the gate is closed
        }
        // r_shadow is left out of the equality: shadow("raytrace", P) called from a surface reads the light state the lights leave behind (Ps, the
        // direction of the last light), which a closed gate never sets, so it reads uninitialised memory (GitHub #236, as it does today for a
        // surface that calls it before any light ran). It is only checked to stay a fraction.
        const char *open[] = {"r_trace", "r_transmission", "r_visibility", "r_environment", "r_texture", "r_specbrdf"};
        for (const char *name : open)
            CL_CHECK(closed.outputMean.at(name) == color.outputMean.at(name)); // not gated
        CL_CHECK(closed.outputMean.at("r_shadow") >= 0.0 && closed.outputMean.at("r_shadow") <= 1.0);
        CL_CHECK(color.outputMean.at("r_trace") > 0.0);          // the traced ray hit the occluder: the not-gated builtin did work
        CL_CHECK(closed.lightRuns == 0 && color.lightRuns > 0);

        // a ray traced from a gated surface shades its hit in a new state with the hit object's own mode: an occluder that reads the lights
        SScene hit = g;
        hit.occluderSurface = "cl_gate"; // runs illuminance and the lights for its own shading (color mode)
        const SRun withHit = render(hit);
        CL_CHECK(withHit.lightRuns > 0);

        // lightingSuppressed is 0 again after the gated execution: a color object shaded later in the same frame is lit
        SScene after = base;
        after.halfFront = true;
        after.attributeMode = "closures";
        after.extra = [format]() {
            RtPoint right[4] = {{0, -4, 1}, {4, -4, 1}, {4, 4, 1}, {0, 4, 1}};
            RiAttributeBegin();
            RtColor c = {0.25f, 0.5f, 0.75f};
            RiColor(c);
            RiSurface((std::string("cl_gatecap_") + format).c_str(), RI_NULL);
            RiPolygon(4, RI_P, (RtPointer)right, RI_NULL);
            RiAttributeEnd();
        };
        const SRun mixed = render(after);
        CL_CHECK(mixed.lightRuns > 0);                          // the color half ran the light
        CL_CHECK(mixed.outputMean.at("r_illum") > 0.0);
    }

} // namespace

int main(int argc, char **argv) {
    cldbl::registerBackend();
    const std::string which = argc > 1 ? argv[1] : "";
    if (which == "Gate_interp")
        sectionGate("rslo");
    else if (which == "Gate_jit")
        sectionGate("slo");
    else {
        std::printf("usage: test_mode_gate <Gate_interp|Gate_jit>\n");
        cldbl::unregisterBackend();
        return 2;
    }
    cldbl::unregisterBackend();
    return CL_RESULT(("ClosureMode_" + which).c_str());
}
