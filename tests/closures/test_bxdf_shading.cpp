/*
 * Spec 035, Phase 4 (T084), labels closures, bxdf: a Bxdf-governed object through the shade step, on the three camera hiders and under rslo and
 * slo, through the delegating cldbl backend. One program, one section per ctest (argv[1]).
 *
 *   IR026_<hider>          Closure_Interim_IR026_<hider>: no consumer: one diagnostic per Bxdf NAME per frame across 4 threads; no error; a Surface
 *                          bound before the Bxdf does not run (cldbl executes 0), no shader load, no instance key
 *   Precedence             Closure_Interim_IR026_Precedence: IR-026 wins over IR-017 and IR-018 (a Bxdf under attribute closuremode closures or
 *                          both and no consumer gives the one IR-026 diagnostic and none of the other two)
 *   ByteIdentity_<hider>   Closure_Interim_IR026_ByteIdentity (R-4): the Bxdf-governed object renders byte for byte as the same object with no
 *                          material statement in a scene with no Surface anywhere (the defaultsurface RiWorldBegin bound), with and without a
 *                          user Surface bound ahead of the Bxdf; rslo and slo
 *   PreWorld_<hider>       Closure_Interim_IR026_PreWorld (B2): a Bxdf bound BEFORE WorldBegin in a scene with no Surface renders as the no-material
 *                          object (the defaultsurface, handed over lazily at RiWorldBegin); with a pre-world Surface it renders the no-shader path
 *                          and never that Surface. The Bxdf-governed attributes' stored usedParameters equals the no-material object's (R1)
 *   Consumer_<hider>       Closure_Bxdf_Consumer: with a consumer: one tree per real point, fromBxdf set, Ci = 0 (a black image), Oi = Os (the
 *                          attribute's, and the primitive's when it gives one), the Surface slot does not run, every tree equals the hand-built
 *                          one, identical at 1 and 4 threads
 *   ConsumerHiders         the set of distinct tree hashes is the same on the three hiders (and is the hand-built tree's)
 *   MessagePassing         Closure_Bxdf_MessagePassing (E12): a Bxdf governs and a user Surface with a message parameter sits in the slot: a
 *                          displacement shader's surface("x", v) returns that Surface's parameter (RISpec 3.2 section 15.8)
 *
 * The scenes tilt the quad 45 degrees: at normal incidence the defaultsurface (Os*Cs*(0.2 + 0.8*cos^2)) and the no-shader path (Cs*|I.N|) both give
 * Cs, so only a tilted quad tells the two substitutes apart; each test checks that the two differ before it relies on the difference.
 *
 * Red on the starting tree: does not compile (bxdfInstance.h, RiBxdf).
 */
#include <cmath>
#include <cstdio>
#include <cstring>
#include <set>
#include <string>

#include "bxdf_support.h"
#include "bxdf_tree_support.h"
#include "bxdfRegistry.h"
#include "attributes.h"
#include "renderer.h"
#include "rendererContext.h"

using namespace clbx;
using openrender::shading::findBxdf;
using openrender::shading::SBxdfLookup;

namespace {

    const float kTilt = 45.0f;

    bool pixelsDiffer(const SImage &a, const SImage &b) {
        float d = 0;
        for (int k = 0; k < 3; ++k)
            d += std::fabs(a.at(3, 3, k) - b.at(3, 3, k));
        return d > 1e-3f;
    }

    // ---- IR026_<hider> -----------------------------------------------------------------------------------------------------
    void sectionIR026(const std::string &hider) {
        for (const char *format : {"rslo", "slo"}) {
            SBxdfScene s;
            s.hider = hider;
            s.format = format;
            s.noFront = true;
            s.threads = 4;
            s.size = 64; // several 16 x 16 buckets, so several threads shade
            s.world = [&]() {
                RtColor c = {0.25f, 0.5f, 0.75f};
                RiColor(c);
                RiSurface((std::string("cl_probe_") + format).c_str(), RI_NULL); // bound before the Bxdf: must not run
                RiAttributeBegin();
                bxdf("plastic");
                quad(-4, -4, 0, 4, kTilt);
                RiAttributeEnd();
                RiAttributeBegin();
                RtFloat kd = 0.25f;
                RiBxdf("plastic", RI_NULL, "float Kd", &kd, RI_NULL); // the same name again: no second diagnostic
                quad(0, 0, 4, 4, kTilt);
                RiAttributeEnd();
                RiAttributeBegin();
                bxdf("conductor");
                quad(0, -4, 4, 0, kTilt);
                RiAttributeEnd();
            };
            const SRun run = render(s);
            CL_CHECK(clmode::warningsNaming(run, "plastic") == 1);   // per name, per frame
            CL_CHECK(clmode::warningsNaming(run, "conductor") == 1);
            CL_CHECK(run.warnings == 2 && run.errors == 0);
            CL_CHECK(run.executes == 0);                              // the Surface slot did not run
            CL_CHECK(run.trees == 0);
            // the object rendered (the substitute, not nothing): every pixel is covered
            CL_CHECK(run.image.at(3, 3, 3) == 1.0f && run.image.at(60, 60, 3) == 1.0f);
        }
    }

    void sectionPrecedence() {
        for (const char *mode : {"closures", "both"}) {
            SBxdfScene s;
            s.front = [&]() {
                RtString m = const_cast<RtString>(mode);
                RiAttribute(RI_SHADE, RI_CLOSUREMODE, &m, RI_NULL);
                bxdf("diffuse");
            };
            const SRun run = render(s);
            CL_CHECK(run.warnings == 1 && clmode::warningsNaming(run, "diffuse") == 1); // IR-026 only
            CL_CHECK(run.errors == 0);
        }
        // the scene-wide mode too
        SBxdfScene s;
        s.preWorld = []() {
            RtString m = const_cast<RtString>("closures");
            RiOption(RI_SHADE, RI_CLOSUREMODE, &m, RI_NULL);
        };
        s.front = []() { bxdf("diffuse"); };
        const SRun run = render(s);
        CL_CHECK(run.warnings == 1);
    }

    // ---- ByteIdentity_<hider> ----------------------------------------------------------------------------------------------
    void sectionByteIdentity(const std::string &hider) {
        for (const char *format : {"rslo", "slo"}) {
            SBxdfScene control;
            control.hider = hider;
            control.format = format;
            control.tilt = kTilt;
            const SRun none = render(control); // no material statement: RiWorldBegin's defaultsurface
            SBxdfScene nullPath = control;
            nullPath.front = []() { RiSurface("null", RI_NULL); };
            const SRun noShader = render(nullPath);
            CL_CHECK(pixelsDiffer(none.image, noShader.image)); // the two substitutes can be told apart

            SBxdfScene withBxdf = control;
            withBxdf.front = []() { bxdf("plastic"); };
            const SRun a = render(withBxdf);
            CL_CHECK(a.image == none.image);
            CL_CHECK(a.warnings == 1);

            SBxdfScene withSurfaceFirst = control;
            withSurfaceFirst.front = [&]() {
                RiSurface((std::string("cl_probe_") + format).c_str(), RI_NULL);
                bxdf("conductor");
            };
            const SRun b = render(withSurfaceFirst);
            CL_CHECK(b.image == none.image); // never the user's Surface
            CL_CHECK(b.executes == 0);
        }
    }

    // ---- PreWorld_<hider> --------------------------------------------------------------------------------------------------
    unsigned int g_used = 0;
    void recordUsed() { g_used = CRenderer::context->getAttributes(FALSE)->usedParameters; }

    void sectionPreWorld(const std::string &hider) {
        for (const char *format : {"rslo", "slo"}) {
            SBxdfScene control;
            control.hider = hider;
            control.format = format;
            control.tilt = kTilt;
            control.front = []() { recordUsed(); };
            const SRun none = render(control);
            const unsigned int usedNone = g_used;
            SBxdfScene nullPath = control;
            nullPath.front = []() { RiSurface("null", RI_NULL); };
            const SRun noShader = render(nullPath);
            CL_CHECK(pixelsDiffer(none.image, noShader.image));

            // a Bxdf before WorldBegin and no Surface anywhere: the defaultsurface, handed over at RiWorldBegin
            SBxdfScene early = control;
            early.preWorld = []() { bxdf("plastic"); };
            early.front = []() { recordUsed(); };
            const SRun a = render(early);
            CL_CHECK(a.image == none.image);
            CL_CHECK(g_used == usedNone); // R1: the stored usedParameters is the no-material object's
            CL_CHECK(a.warnings == 1);

            // a Surface given before WorldBegin: RiWorldBegin binds no defaultsurface, the substitute is the no-shader path, never that Surface
            SBxdfScene surfaceEarly = control;
            surfaceEarly.preWorld = [&]() {
                RiSurface((std::string("cl_probe_") + format).c_str(), RI_NULL);
                bxdf("plastic");
            };
            surfaceEarly.front = nullptr;
            const SRun b = render(surfaceEarly);
            CL_CHECK(b.image == noShader.image);
            CL_CHECK(b.executes == 0);
            // the same with the Bxdf first and the Surface after it: the Surface governs, so it runs
            SBxdfScene surfaceAfter = control;
            surfaceAfter.preWorld = [&]() {
                bxdf("plastic");
                RiSurface((std::string("cl_probe_") + format).c_str(), RI_NULL);
            };
            surfaceAfter.front = nullptr;
            const SRun c = render(surfaceAfter);
            CL_CHECK(c.executes > 0 && c.warnings == 0);
        }
    }

    // ---- Consumer_<hider> --------------------------------------------------------------------------------------------------
    // The tree the plastic table builds at the delivered point: Cs (0.25, 0.5, 0.75), N = faceforward(normalize(N), I) = (0, 0, -1) for the untilted
    // quad seen by the orthographic camera (I = (0, 0, 1)). The hash is over the flattened lobes, as the consumer double's.
    uint64_t expectedPlasticHash() {
        const SBxdfLookup l = findBxdf("plastic");
        CL_CHECK(l.table != nullptr);
        if (l.table == nullptr)
            return 0;
        std::vector<const void *> values;
        std::vector<bxtree::F3> storage((size_t)l.table->numParams);
        for (int i = 0; i < l.table->numParams; ++i) {
            const OrBxdfParam &p = l.table->params[i];
            storage[(size_t)i] = {p.defaultValue[0], p.defaultValue[1], p.defaultValue[2]};
            values.push_back(storage[(size_t)i].data());
        }
        OrBxdfData *data = l.table->create(values.data(), nullptr);
        OrBxdfPoint point = bxtree::pointWith({1, 1, 1});
        point.N[0] = 0.0f;
        point.N[1] = 0.0f;
        point.N[2] = -1.0f;
        point.Cs[0] = 0.25f;
        point.Cs[1] = 0.5f;
        point.Cs[2] = 0.75f;
        bxtree::Arena arena;
        OrClosureAllocator alloc = {&arena.page, bxtree::allocate};
        const OrClosureNode *tree = l.table->build(data, &point, &alloc);
        const uint64_t h = tree != nullptr ? clmode::hashTree(tree) : 0;
        l.table->destroy(data);
        return h;
    }

    SBxdfScene consumerScene(const std::string &hider) {
        SBxdfScene s;
        s.hider = hider;
        s.consumer = true;
        s.front = []() {
            RiSurface("cl_cap_rslo", RI_NULL); // capable, bound ahead of the Bxdf: must not run, and must deliver nothing of its own
            bxdf("plastic");
        };
        return s;
    }

    void sectionConsumer(const std::string &hider) {
        SBxdfScene s = consumerScene(hider);
        CConsumerDouble record;
        const SRun run = render(s, &record);
        CL_CHECK(run.trees > 0);
        CL_CHECK(record.points() == record.trees());       // one tree per real point
        CL_CHECK(record.fromBxdf() == record.batches());   // every batch says so
        CL_CHECK(run.executes == 0);                       // the Surface slot did not run
        CL_CHECK(run.warnings == 0 && run.errors == 0);    // no IR-026: a consumer is installed
        if (hider == "raytrace")
            CL_CHECK(run.trees == 64);                     // one primary hit per pixel of the 8 x 8 frame
        const std::set<uint64_t> distinct(run.hashes.begin(), run.hashes.end());
        CL_CHECK(distinct.size() == 1 && *distinct.begin() == expectedPlasticHash()); // equals the hand-built tree
        // Ci = 0 and Oi = Os: a black image whose alpha is the opacity
        CL_CHECK(run.image.at(3, 3, 0) == 0.0f && run.image.at(3, 3, 1) == 0.0f && run.image.at(3, 3, 2) == 0.0f);
        CL_CHECK(run.image.at(3, 3, 3) == 1.0f);

        // the attribute's opacity (the z-buffer hider writes no opacity: its alpha is coverage, as test_mode_step.cpp has it)
        SBxdfScene half = s;
        half.opacity = 0.5f;
        const SRun halfRun = render(half);
        if (hider != "zbuffer")
            CL_CHECK(std::fabs(halfRun.image.at(3, 3, 3) - 0.5f) < 1e-3f);
        CL_CHECK(halfRun.image.at(3, 3, 0) == 0.0f);
        // the primitive's own Os (a per-vertex "Os" on the polygon) wins over the attribute's
        SBxdfScene primitive = s;
        primitive.noFront = true;
        primitive.world = []() {
            RiAttributeBegin();
            RtColor c = {0.25f, 0.5f, 0.75f};
            RiColor(c);
            bxdf("plastic");
            RtPoint p[4] = {{-4, -4, 1}, {4, -4, 1}, {4, 4, 1}, {-4, 4, 1}};
            RtColor os[4] = {{0.25f, 0.25f, 0.25f}, {0.25f, 0.25f, 0.25f}, {0.25f, 0.25f, 0.25f}, {0.25f, 0.25f, 0.25f}};
            RiPolygon(4, RI_P, (RtPointer)p, "Os", (RtPointer)os, RI_NULL);
            RiAttributeEnd();
        };
        const SRun primitiveRun = render(primitive);
        if (hider != "zbuffer")
            CL_CHECK(std::fabs(primitiveRun.image.at(3, 3, 3) - 0.25f) < 1e-3f);

        // 1 and 4 threads: the same trees and the same image
        SBxdfScene threaded = s;
        threaded.size = 32;
        threaded.threads = 1;
        const SRun one = render(threaded);
        threaded.threads = 4;
        const SRun four = render(threaded);
        CL_CHECK(one.trees > 0 && one.hashes == four.hashes && one.image == four.image);
    }

    void sectionConsumerHiders() {
        std::set<uint64_t> sets[3];
        int i = 0;
        for (const char *hider : {"reyes", "zbuffer", "raytrace"}) {
            const SRun run = render(consumerScene(hider));
            CL_CHECK(run.trees > 0);
            sets[i++] = std::set<uint64_t>(run.hashes.begin(), run.hashes.end());
        }
        CL_CHECK(sets[0] == sets[1] && sets[1] == sets[2]);
        CL_CHECK(sets[0].size() == 1 && *sets[0].begin() == expectedPlasticHash());
    }

    // ---- MessagePassing ----------------------------------------------------------------------------------------------------
    void sectionMessagePassing() {
        for (const char *hider : {"raytrace", "zbuffer", "reyes"}) {
            for (const char *format : {"rslo", "slo"}) {
                SBxdfScene s;
                s.hider = hider;
                s.format = format;
                s.front = [&]() {
                    RtFloat bound = 0.1f;
                    RtString space = const_cast<RtString>("object");
                    RiAttribute("displacementbound", "float sphere", &bound, "string coordinatesystem", &space, RI_NULL);
                    RiDisplacement((std::string("cl_msgd_") + format).c_str(), RI_NULL);
                    RtFloat x = 0.375f;
                    RiSurface("cl_msg_surface", "float x", &x, RI_NULL); // in the slot, not running: the Bxdf governs
                    bxdf("diffuse");
                };
                const SRun run = render(s);
                CL_CHECK(run.executes > 0);                                    // the displacement shader ran (through the cldbl wrapper)
                CL_CHECK(std::fabs(cldbl::outputMean("r_msg") - 0.375) < 1e-6); // and read the slot Surface's parameter
                CL_CHECK(run.errors == 0);
            }
        }
    }

} // namespace

int main(int argc, char **argv) {
    cldbl::registerBackend();
    const std::string which = argc > 1 ? argv[1] : "";
    auto starts = [&](const char *prefix) { return which.rfind(prefix, 0) == 0; };
    auto suffix = [&](const char *prefix) { return which.substr(std::strlen(prefix)); };
    if (starts("IR026_"))
        sectionIR026(suffix("IR026_"));
    else if (which == "Precedence")
        sectionPrecedence();
    else if (starts("ByteIdentity_"))
        sectionByteIdentity(suffix("ByteIdentity_"));
    else if (starts("PreWorld_"))
        sectionPreWorld(suffix("PreWorld_"));
    else if (starts("Consumer_"))
        sectionConsumer(suffix("Consumer_"));
    else if (which == "ConsumerHiders")
        sectionConsumerHiders();
    else if (which == "MessagePassing")
        sectionMessagePassing();
    else {
        std::printf("usage: test_bxdf_shading <IR026_h|Precedence|ByteIdentity_h|PreWorld_h|Consumer_h|ConsumerHiders|MessagePassing>\n");
        cldbl::unregisterBackend();
        return 2;
    }
    cldbl::unregisterBackend();
    return CL_RESULT(("Bxdf_Shading_" + which).c_str());
}
