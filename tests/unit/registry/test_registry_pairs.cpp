/**
 * Project: openRender
 *
 * File: test_registry_pairs.cpp
 *
 * Description:
 *   Spec 030 (hider and integrator registry), US4 AS1-AS4, FR-012, FR-013, FR-016, FR-024, SC-006.
 *   contracts/frame-transport.md "The refused frame" and "Invariants checked by tests" 1-3 and 6.
 *
 *     Registry_PairRefusal_<hider>  (reyes, zbuffer, raytrace, photon) test-double integrators with the built-in
 *                                   hider: one that requires an unprovided capability and one that requires a newer
 *                                   version of a provided one are each refused with ONE error that names hider,
 *                                   integrator and capability (and, for a version shortfall, both versions); the
 *                                   world block of the refused frame holds geometry and a Declare and runs without
 *                                   a crash; no image and no photon map is written; the integrator is not even
 *                                   created; RiLastError is set (orender exits -1); a following frame renders; an
 *                                   integrator whose OPTIONAL capability the hider lacks is accepted and its
 *                                   configure sees the capability absent from the hider's provided list.
 *                                   (The Debug-build asserts of endFrame(), stats.numRasterGrids/Objects == 0, are
 *                                   exercised when this test runs in the Debug tree of Phase 9.)
 *     Registry_ClassicPairs         `classic` with each built-in hider is accepted; configure, prepare and finish
 *                                   are each called once per rendered frame (read from classic's own counters) and
 *                                   zero times for a refused frame.
 *
 *   Red by build failure until Phase 9 (the test-double integrator ABI), by assertion afterwards if the core does not
 *   refuse. The case is the first argument (`refusal <hider>` or `classic_pairs`).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include <cstring>
#include <string>

#include "common/diagnostics.h"
#include "integrator_doubles.h"
#include "registry_testing.h"
#include "ri.h"
#include "transport/classicIntegrator.h"

using namespace regtest;

namespace {

    // The world-block extras of a refused frame: a Declare and geometry the core must tolerate without a hider.
    const char *kWorldExtras = "    Declare \"refusedvar\" \"uniform float\"\n"
                               "    Translate 0 0 1\n"
                               "    Sphere 0.5 -0.5 0.5 360\n";

    bool isPhoton(const std::string &hider) {
        return hider == "photon";
    }

    // One frame of `hider` with `integrator` ("" for no statement). The output is an image, or the photon map.
    std::string oneFrame(int number, const std::string &hider, const std::string &integrator, const std::string &output) {
        const std::string statement = integrator.empty() ? std::string() : "  Integrator \"" + integrator + "\"\n";
        if (isPhoton(hider))
            return photonFrameText(number, statement, output);
        SFrame f;
        f.number = number;
        f.statements = "  Hider \"" + hider + "\"\n" + statement;
        f.image = output;
        f.world = kWorldExtras;
        return frameText(f);
    }

    int errorsWithCode(const SReports &r, int code) {
        int n = 0;
        for (const SReport &x : r.list)
            if (x.severity >= SReports::errorSeverity && x.code == code)
                ++n;
        return n;
    }

    // Registry_PairRefusal_<hider>
    void refusal(const std::string &hider) {
        resetIntegratorLogs();
        const char *ext = isPhoton(hider) ? ".gpm" : ".tif";
        const std::string out1 = "pair_" + hider + "_missing" + ext, out2 = "pair_" + hider + "_newer" + ext, out3 = "pair_" + hider + "_optional" + ext,
                          out4 = "pair_" + hider + "_classic" + ext;
        for (const std::string &o : {out1, out2, out3, out4})
            removeFile(o);

        // Frame 1: requires a capability nobody provides. Frame 2: requires rasterization 2.0.0. Frame 3: an optional
        // capability only. Frame 4: classic.
        std::string rib = oneFrame(1, hider, integratorDoubleName(DOUBLE_NEEDS_MISSING), out1) +
                          oneFrame(2, hider, integratorDoubleName(DOUBLE_NEEDS_NEWER), out2) +
                          oneFrame(3, hider, integratorDoubleName(DOUBLE_OPTIONAL), out3) + oneFrame(4, hider, "classic", out4);
        const SSession s = renderRib(rib, "pair_" + hider + ".rib");

        const std::string who = "Hider \"" + hider + "\" cannot run integrator \"";

        // Frame 1: one RIE_LIMIT error naming hider, integrator and the missing capability.
        const std::string text1 = who + integratorDoubleName(DOUBLE_NEEDS_MISSING) + "\"";
        REG_CHECK_MSG(s.frames.count(text1.c_str()) == 1, "got:\n%s", s.frames.text().c_str());
        REG_CHECK(s.frames.count("missing \"test-unprovided\"") == 1);

        // Frame 2: one error; rasterization is either provided at 1.0.0 (reyes, zbuffer: both versions in the text) or
        // missing (raytrace, photon).
        const std::string text2 = who + integratorDoubleName(DOUBLE_NEEDS_NEWER) + "\"";
        REG_CHECK_MSG(s.frames.count(text2.c_str()) == 1, "got:\n%s", s.frames.text().c_str());
        if (hider == "reyes" || hider == "zbuffer")
            REG_CHECK_MSG(s.frames.count("\"rasterization\" 1.0.0 provided, 2.0.0 required") == 1, "both versions in the text; got:\n%s",
                          s.frames.text().c_str());
        else
            REG_CHECK_MSG(s.frames.count("missing \"rasterization\" 2.0.0") == 1, "got:\n%s", s.frames.text().c_str());

        // Both refusals are RIE_LIMIT errors and the only errors of the session; "the frame is not rendered".
        REG_CHECK_MSG(errorsWithCode(s.frames, RIE_LIMIT) == 2, "got:\n%s", s.frames.text().c_str());
        REG_CHECK_MSG(s.frames.errors() == 2, "got:\n%s", s.frames.text().c_str());
        REG_CHECK(s.frames.count("the frame is not rendered") == 2);
        REG_CHECK_MSG(s.lastError != RIE_NOERROR, "orender exits -1 after a refusal");

        // Nothing was built for a refused frame, and nothing was written.
        REG_CHECK_MSG(integratorLog(DOUBLE_NEEDS_MISSING).created == 0 && integratorLog(DOUBLE_NEEDS_MISSING).configured == 0 &&
                          integratorLog(DOUBLE_NEEDS_MISSING).finished == 0,
                      "the refused integrator is never created or called");
        REG_CHECK(integratorLog(DOUBLE_NEEDS_NEWER).created == 0 && integratorLog(DOUBLE_NEEDS_NEWER).configured == 0);
        REG_CHECK_MSG(!fileExists(out1), "no output file for the first refused frame");
        REG_CHECK_MSG(!fileExists(out2), "no output file for the second refused frame");

        // Frame 3: the optional capability is absent or present, the pair is accepted, configure saw the truth.
        const SIntegratorLog &opt = integratorLog(DOUBLE_OPTIONAL);
        REG_CHECK_MSG(opt.created == 1 && opt.configured == 1 && opt.prepared == 1 && opt.finished == 1 && opt.destroyed == 1,
                      "optional-only integrator: lifecycle once (created %d configured %d prepared %d finished %d destroyed %d)", opt.created,
                      opt.configured, opt.prepared, opt.finished, opt.destroyed);
        REG_CHECK(opt.hiderName == hider);
        REG_CHECK(opt.sawProvided("illumination-hook") == isPhoton(hider));
        REG_CHECK(opt.sawProvided("rasterization") == (hider == "reyes" || hider == "zbuffer"));
        REG_CHECK_MSG(fileExists(out3), "the accepted frame wrote its output");

        // Frame 4: a following frame (classic) renders.
        REG_CHECK_MSG(fileExists(out4), "a frame after a refusal renders");
    }

    // Registry_ClassicPairs
    void classicPairs() {
        resetIntegratorLogs();
        for (const std::string hider : {"reyes", "zbuffer", "raytrace", "photon"}) {
            const char *ext = isPhoton(hider) ? ".gpm" : ".tif";
            const std::string out = "classic_" + hider + ext;
            removeFile(out);

            const SClassicLifecycleCounts before = transportClassicLifecycleCounts();
            const SSession s = renderRib(oneFrame(1, hider, "classic", out), "classic_" + hider + ".rib");
            const SClassicLifecycleCounts after = transportClassicLifecycleCounts();
            REG_CHECK_MSG(s.frames.errors() == 0 && s.lastError == RIE_NOERROR, "%s: got:\n%s", hider.c_str(), s.frames.text().c_str());
            REG_CHECK_MSG(fileExists(out), "%s: rendered", hider.c_str());
            REG_CHECK_MSG(after.configure == before.configure + 1 && after.prepare == before.prepare + 1 && after.finish == before.finish + 1,
                          "%s: configure/prepare/finish once per rendered frame", hider.c_str());

            // No statement at all is the same: classic is the default.
            const SClassicLifecycleCounts mid = transportClassicLifecycleCounts();
            const std::string out2 = "classic_default_" + hider + ext;
            removeFile(out2);
            const SSession d = renderRib(oneFrame(1, hider, "", out2), "classic_default_" + hider + ".rib");
            const SClassicLifecycleCounts end = transportClassicLifecycleCounts();
            REG_CHECK(d.frames.errors() == 0 && fileExists(out2));
            REG_CHECK_MSG(end.configure == mid.configure + 1 && end.prepare == mid.prepare + 1 && end.finish == mid.finish + 1,
                          "%s: the default integrator is classic and is called once", hider.c_str());

            // A refused frame calls classic zero times.
            const SClassicLifecycleCounts pre = transportClassicLifecycleCounts();
            const std::string out3 = "classic_refused_" + hider + ext;
            removeFile(out3);
            const SSession r = renderRib(oneFrame(1, hider, integratorDoubleName(DOUBLE_NEEDS_MISSING), out3), "classic_refused_" + hider + ".rib");
            const SClassicLifecycleCounts post = transportClassicLifecycleCounts();
            REG_CHECK(r.frames.errors() == 1 && !fileExists(out3));
            REG_CHECK_MSG(post.configure == pre.configure && post.prepare == pre.prepare && post.finish == pre.finish,
                          "%s: a refused frame calls no lifecycle slot", hider.c_str());
        }
    }

} // namespace

int main(int argc, char **argv) {
    const std::string which = argc > 1 ? argv[1] : "";
    if (which == "refusal" && argc > 2)
        refusal(argv[2]);
    else if (which == "classic_pairs")
        classicPairs();
    else {
        std::fprintf(stderr, "usage: %s refusal reyes|zbuffer|raytrace|photon | classic_pairs\n", argv[0]);
        return 2;
    }
    return finish(which.c_str());
}
