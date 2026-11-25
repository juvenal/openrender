/*
 * Spec 035, Phase 3 (T058), label closuremode: the surface step through the delegating cldbl backend and the consumer double.
 * One program, one section per ctest (argv[1]); the three hider sections of a family are the same code with another hider.
 *
 *   ColorIdentity_<hider>   an explicit `color` (attribute, then option) takes the out-of-line plan and gives the image of the no-mention
 *                           render byte for byte, with no tree delivered (R8); a frame with no Surface (E7) renders the same under every
 *                           mode with no diagnostic
 *   Both_<hider>            `both`: the image equals `color`'s byte for byte; the trees delivered equal the real shaded points of the
 *                           `color` render (SC-002, derivative-helper points excluded); a camera hit is delivered with its ray type
 *   Closures_<hider>        `closures`: the counting light runs 0 times (and the counting illuminance 0) against at least once under
 *                           color/both; the trees equal both's; the pattern counters equal color's (SC-003); Ci is 0 and Oi the
 *                           shader's own (Q-R5); camerahitmode "primitive" still overrides the opacity afterwards
 *   IR017_<hider>           a shader that is not closure capable under both or closures renders as color; one warning per (shader, mode)
 *                           per frame across 4 threads; nothing else changes
 *   IR018_<hider>           capable, closures, no consumer: gate closed, Ci = 0, own Oi, one warning per frame; with a consumer none
 *   Precedence              IR-017 before IR-018: an incapable shader under closures with no consumer reports IR-017 only
 *
 * Red on the starting tree: does not compile (closureFrame.h, closureMode.h, closureConsumer.h, the closuremode token).
 */
#include <cstdio>
#include <string>

#include "mode_support.h"

using namespace clmode;

namespace {

    SScene sceneFor(const std::string &hider, const std::string &format) {
        SScene s;
        s.hider = hider;
        s.format = format;
        s.surface = "cl_cap"; // capable
        s.consumer = true;
        return s;
    }

    // R8: the explicit color
    void sectionColorIdentity(const std::string &hider) {
        SScene base = sceneFor(hider, "rslo");
        const SRun none = render(base);
        SScene a = base;
        a.attributeMode = "color";
        const SRun attr = render(a);
        SScene o = base;
        o.optionMode = "color";
        const SRun opt = render(o);
        CL_CHECK(none.image == attr.image);
        CL_CHECK(none.image == opt.image);
        CL_CHECK(none.trees == 0 && attr.trees == 0 && opt.trees == 0); // color delivers nothing, even with a consumer installed
        CL_CHECK(none.realPoints > 0 && none.realPoints == attr.realPoints && none.realPoints == opt.realPoints);
        CL_CHECK(none.warnings == 0 && attr.warnings == 0 && opt.warnings == 0);

        // E7: no Surface (no Bxdf either): every mode renders today's no-shader path, with no diagnostic
        SScene bare = base;
        bare.surface = "";
        const SRun bareColor = render(bare);
        for (const char *mode : {"both", "closures"}) {
            SScene m = bare;
            m.attributeMode = mode;
            const SRun r = render(m);
            CL_CHECK(r.image == bareColor.image);
            CL_CHECK(r.trees == 0 && r.warnings == 0 && r.errors == 0);
        }
    }

    // FR-018, SC-002
    void sectionBoth(const std::string &hider) {
        for (const char *format : {"rslo", "slo"}) {
            SScene base = sceneFor(hider, format);
            const SRun color = render(base);
            SScene b = base;
            b.attributeMode = "both";
            const SRun both = render(b);
            CL_CHECK(both.image == color.image);                // Ci and Oi equal color's byte for byte
            CL_CHECK(color.realPoints > 0);
            CL_CHECK(both.trees == color.realPoints);           // one tree per real shaded point, the derivative helpers excluded
            CL_CHECK(both.realPoints == color.realPoints);
            CL_CHECK(both.warnings == 0 && color.warnings == 0);
            // the batch carries its ray type (Q11): a camera ray's hit for the raytrace hider, None for a rasterised grid (shaderGlobals.h; the
            // interim rule of contracts/consumer-and-backend.md, replaced by spec 058)
            CL_CHECK(both.rayTypes.count((int)(std::string(hider) == "raytrace" ? RayType::Camera : RayType::None)) == 1);
            CL_CHECK(both.modes.count((int)ClosureMode::Both) == 1 && both.modes.count((int)ClosureMode::Color) == 0); // Color never reaches a consumer
            // a surface that uses a derivative: the raytrace hider shades derivative-offset vertices next to the real ones, and none is a point
            // (Q11, SC-002): one tree per real point of the color render, however many vertices the batch has
            if (std::string(hider) == "raytrace") {
                SScene d = base;
                d.surface = "cl_dercap";
                const SRun dColor = render(d);
                d.attributeMode = "both";
                const SRun dBoth = render(d);
                CL_CHECK(dColor.realPoints > 0 && cldbl::counters().allVertices > cldbl::counters().realPoints); // the scene has derivative helpers
                CL_CHECK(dBoth.trees == dColor.realPoints);
                CL_CHECK(dBoth.image == dColor.image);
            }
            // the scene-wide spelling reaches the same plan
            SScene o = base;
            o.optionMode = "both";
            const SRun viaOption = render(o);
            CL_CHECK(viaOption.image == color.image && viaOption.trees == both.trees && viaOption.hashes == both.hashes);
        }
    }

    // FR-019, SC-003, Q-R5
    void sectionClosures(const std::string &hider) {
        for (const char *format : {"rslo", "slo"}) {
            SScene base = sceneFor(hider, format);
            base.surface = "cl_gatecap"; // the probe whose illuminance and lightsource loops need the counting light
            base.light = true;
            const SRun color = render(base);
            CL_CHECK(color.lightRuns > 0);
            CL_CHECK(color.outputMean.at("r_illum") > 0 && color.outputMean.at("r_lightsource") > 0);
            SScene b = base;
            b.attributeMode = "both";
            const SRun both = render(b);
            CL_CHECK(both.image == color.image && both.lightRuns == color.lightRuns && both.outputMean == color.outputMean);
            SScene c = base;
            c.attributeMode = "closures";
            const SRun closures = render(c);
            CL_CHECK(closures.lightRuns == 0);                      // no light shader runs for the object
            CL_CHECK(closures.outputMean.at("r_illum") == 0);       // the counting illuminance: its loop body never ran
            CL_CHECK(closures.outputMean.at("r_lightsource") == 0);
            CL_CHECK(closures.trees == both.trees && closures.hashes == both.hashes);
            CL_CHECK(closures.realPoints == color.realPoints);      // the pattern counter equals color's (the shader ran for every point)
            CL_CHECK(closures.warnings == 0);
            for (int y = 0; y < closures.image.height; ++y)
                for (int x = 0; x < closures.image.width; ++x)
                    for (int k = 0; k < 3; ++k)
                        CL_CHECK(closures.image.at(x, y, k) == 0.0f); // Ci is 0

            // Oi is the shader's own, not the object's Os: cl_cutout writes 0.25 whatever Os is. The z-buffer hider writes no opacity (alpha is 1
            // for it under color too), so the opacity is checked for the hiders that have one.
            if (hider != "zbuffer") {
                SScene cut = sceneFor(hider, format);
                cut.surface = "cl_cutcap";
                cut.opacity = 0.5f; // the object's Os differs from the shader's Oi
                SScene cutClosures = cut;
                cutClosures.attributeMode = "closures";
                const SRun cutRun = render(cutClosures);
                CL_CHECK_NEAR(cutRun.image.at(3, 3, 3), 0.25, 1e-6); // alpha: the cut-out survives
                // camerahitmode "primitive" still overrides the opacity afterwards: the primitive's Os (0.5)
                SScene prim = cutClosures;
                prim.cameraHitMode = "primitive";
                const SRun primRun = render(prim);
                CL_CHECK_NEAR(primRun.image.at(3, 3, 3), 0.5, 1e-6);
            }
        }
    }

    // IR-017
    void sectionIR017(const std::string &hider) {
        for (const char *mode : {"both", "closures"}) {
            SScene base = sceneFor(hider, "rslo");
            base.surface = "cl_probe"; // not closure capable
            base.threads = 4;
            base.size = 16;
            const SRun color = render(base);
            SScene s = base;
            s.attributeMode = mode;
            const SRun r = render(s);
            CL_CHECK(r.image == color.image);                       // renders as color
            CL_CHECK(r.trees == 0);
            CL_CHECK(r.realPoints == color.realPoints);
            CL_CHECK(warningsNaming(r, "cl_probe", mode) == 1);     // one warning per (shader name, mode) per frame across the 4 threads, naming both
            CL_CHECK(r.warnings == 1);
            CL_CHECK(color.warnings == 0);
        }
    }

    // IR-018
    void sectionIR018(const std::string &hider) {
        SScene base = sceneFor(hider, "rslo");
        base.surface = "cl_cutcap";
        base.attributeMode = "closures";
        base.opacity = 0.5f; // the shader's own Oi (0.25) differs from the object's Os
        SScene noConsumer = base;
        noConsumer.consumer = false;
        noConsumer.threads = 4;
        noConsumer.size = 16;
        const SRun r = render(noConsumer);
        CL_CHECK(r.trees == 0);
        CL_CHECK(r.warnings == 1);                                  // once per frame, across the threads
        for (int y = 0; y < r.image.height; ++y)
            for (int x = 0; x < r.image.width; ++x) {
                CL_CHECK(r.image.at(x, y, 0) == 0.0f && r.image.at(x, y, 1) == 0.0f && r.image.at(x, y, 2) == 0.0f); // gate closed, Ci = 0
                if (hider != "zbuffer") // the z-buffer hider writes no opacity
                    CL_CHECK_NEAR(r.image.at(x, y, 3), 0.25, 1e-6);                                               // own Oi
            }
        SScene withConsumer = base;
        withConsumer.threads = 4;
        withConsumer.size = 16;
        const SRun c = render(withConsumer);
        CL_CHECK(c.warnings == 0);                                  // with a consumer installed there is no warning
        CL_CHECK(c.trees > 0);
    }

    // R17: IR-017 before IR-018
    void sectionPrecedence() {
        SScene s = sceneFor("raytrace", "rslo");
        s.surface = "cl_probe"; // not capable
        s.attributeMode = "closures";
        s.consumer = false;
        const SRun r = render(s);
        CL_CHECK(r.warnings == 1);                                  // at most one rule per object
        CL_CHECK(warningsNaming(r, "cl_probe", "closures") == 1);
    }

} // namespace

int main(int argc, char **argv) {
    cldbl::registerBackend();
    const std::string which = argc > 1 ? argv[1] : "";
    std::string hider;
    for (const char *h : {"raytrace", "zbuffer", "reyes"})
        if (which.size() > strlen(h) && which.compare(which.size() - strlen(h), strlen(h), h) == 0)
            hider = h;
    if (which.rfind("ColorIdentity_", 0) == 0)
        sectionColorIdentity(hider);
    else if (which.rfind("Both_", 0) == 0)
        sectionBoth(hider);
    else if (which.rfind("Closures_", 0) == 0)
        sectionClosures(hider);
    else if (which.rfind("IR017_", 0) == 0)
        sectionIR017(hider);
    else if (which.rfind("IR018_", 0) == 0)
        sectionIR018(hider);
    else if (which == "Precedence")
        sectionPrecedence();
    else {
        std::printf("usage: test_mode_step <ColorIdentity|Both|Closures|IR017|IR018>_<raytrace|zbuffer|reyes> | Precedence\n");
        cldbl::unregisterBackend();
        return 2;
    }
    cldbl::unregisterBackend();
    return CL_RESULT(("ClosureMode_" + which).c_str());
}
