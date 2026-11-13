/**
 * Project: openRender
 *
 * File: test_registry_hider_statements.cpp
 *
 * Description:
 *   Spec 030 (hider and integrator registry), US1 AS2-AS4, FR-002, FR-005, FR-007, FR-023.
 *
 *   THESE ARE CHARACTERIZATION TESTS. They pin what the Hider statement does on the starting tree
 *   (master e83d005d) through RI calls and the diagnostic text only, with no registry header, so they are
 *   GREEN on the starting tree and must stay green through every phase (the stated exception to "a new
 *   test that is green at its gate proves nothing", tasks.md "Tests: classification"):
 *
 *     Registry_HiderAliasDiagnostics   Hider "stochastic" warns once per statement with today's text;
 *                                      "hidden" and "reyes" are silent; all three store "reyes"; a frame
 *                                      with two render threads still warns once.
 *     Registry_UnknownHiderFallback    Hider "bogus": today's error at frame begin (once), the reyes
 *                                      image (byte-identical to Hider "reyes"), orender's exit status
 *                                      (RiLastError) set; with a TSM display the TSM error still appears.
 *
 *   The case to run is the first argument; the working directory receives the scenes and the images.
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

#include "options.h"
#include "registry_testing.h"
#include "ri.h"
#include "riInterface.h"

using namespace regtest;

namespace {

    const char *kStochasticWarning = "Deprecated Hider \"stochastic\"; use \"reyes\" instead";
    const char *kBogusError = "Hider \"bogus\" unavailable";
    const char *kTsmError = "Hider must be reyes (or hidden, or the deprecated stochastic alias) for TSM";

    // The options block of the active context, fetched again after every statement.
    COptions *options() {
        return static_cast<CRiInterface *>(const_cast<void *>(RiGetContext()))->getOptions();
    }

    void hiderStatement(const char *name) {
        RiHider(const_cast<RtToken>(name), RI_NULL);
    }

    // Registry_HiderAliasDiagnostics
    void aliasDiagnostics() {
        // 1. Through the API: one warning per "stochastic" statement, none for "hidden" or "reyes".
        {
            RiLastError = RIE_NOERROR;
            RiBegin(RI_NULL);
            sessionReports().clear();
            RiErrorHandler(sessionHandler);

            hiderStatement("stochastic");
            REG_CHECK(std::strcmp(options()->hider, "reyes") == 0);
            REG_CHECK(sessionReports().count(kStochasticWarning) == 1);
            REG_CHECK(sessionReports().warnings() == 1);
            REG_CHECK(sessionReports().errors() == 0);

            hiderStatement("stochastic");
            REG_CHECK(sessionReports().count(kStochasticWarning) == 2); // once per statement

            sessionReports().clear();
            hiderStatement("hidden");
            REG_CHECK(std::strcmp(options()->hider, "reyes") == 0);
            hiderStatement("reyes");
            REG_CHECK(std::strcmp(options()->hider, "reyes") == 0);
            REG_CHECK_MSG(sessionReports().list.empty(), "hidden/reyes are silent; got:\n%s", sessionReports().text().c_str());

            hiderStatement("zbuffer");
            REG_CHECK(std::strcmp(options()->hider, "zbuffer") == 0);
            REG_CHECK(sessionReports().list.empty());
            RiEnd();
        }

        // 2. Through a frame with two render threads: the warning is the statement's, so it appears once.
        {
            SFrame f;
            f.threads = 2;
            f.statements = "  Hider \"stochastic\"\n";
            f.image = "alias_stochastic.tif";
            removeFile(f.image);
            const SSession s = renderRib(frameText(f), "alias_stochastic.rib");
            REG_CHECK_MSG(s.frames.count(kStochasticWarning) == 1, "got:\n%s", s.frames.text().c_str());
            REG_CHECK(s.frames.errors() == 0);
            REG_CHECK_MSG(fileExists(f.image), "the frame rendered");
        }

        // 3. "hidden" in a frame: silent, and the same image as "reyes".
        {
            SFrame hidden, reyes;
            hidden.statements = "  Hider \"hidden\"\n";
            hidden.image = "alias_hidden.tif";
            reyes.statements = "  Hider \"reyes\"\n";
            reyes.image = "alias_reyes.tif";
            removeFile(hidden.image);
            removeFile(reyes.image);
            const SSession a = renderRib(frameText(hidden), "alias_hidden.rib");
            const SSession b = renderRib(frameText(reyes), "alias_reyes.rib");
            REG_CHECK(a.frames.list.empty());
            REG_CHECK(b.frames.list.empty());
            REG_CHECK(fileExists(hidden.image) && fileExists(reyes.image));
            REG_CHECK(readFile(hidden.image) == readFile(reyes.image));
        }
    }

    // Registry_UnknownHiderFallback
    void unknownFallback() {
        // 1. Hider "bogus": the error at frame begin (once), the reyes image, a non-zero exit status.
        SFrame reyes;
        reyes.statements = "  Hider \"reyes\"\n";
        reyes.image = "unknown_reyes.tif";
        removeFile(reyes.image);
        const SSession base = renderRib(frameText(reyes), "unknown_reyes.rib");
        REG_CHECK(base.frames.errors() == 0);
        REG_CHECK(base.lastError == RIE_NOERROR);
        REG_CHECK(fileExists(reyes.image));

        SFrame bogus;
        bogus.statements = "  Hider \"bogus\"\n";
        bogus.image = "unknown_bogus.tif";
        removeFile(bogus.image);
        const SSession s = renderRib(frameText(bogus), "unknown_bogus.rib");
        REG_CHECK_MSG(s.frames.count(kBogusError) == 1, "got:\n%s", s.frames.text().c_str());
        REG_CHECK(s.frames.errors() == 1);
        REG_CHECK_MSG(s.lastError != RIE_NOERROR, "orender exits -1 when RiLastError is set");
        REG_CHECK(fileExists(bogus.image));
        REG_CHECK_MSG(readFile(bogus.image) == readFile(reyes.image), "the fallback renders the reyes image");

        // 2. With a TSM display the TSM check still names the unknown hider as not reyes: both errors appear.
        std::string tsm = "FrameBegin 1\n"
                          "  Option \"limits\" \"numthreads\" [1]\n"
                          "  Option \"limits\" \"bucketsize\" [16 16]\n"
                          "  Hider \"bogus\"\n"
                          "  Display \"unknown_bogus.tsm\" \"tsm\" \"z\"\n"
                          "  Format 32 32 1\n"
                          "  PixelSamples 1 1\n"
                          "  Projection \"orthographic\"\n"
                          "  Translate 0 0 5\n"
                          "  WorldBegin\n"
                          "    Surface \"constant\"\n"
                          "    Sphere 0.8 -0.8 0.8 360\n"
                          "  WorldEnd\n"
                          "FrameEnd\n";
        const SSession t = renderRib(tsm, "unknown_bogus_tsm.rib");
        REG_CHECK_MSG(t.frames.count(kBogusError) == 1, "got:\n%s", t.frames.text().c_str());
        REG_CHECK_MSG(t.frames.count(kTsmError) == 1, "got:\n%s", t.frames.text().c_str());
        REG_CHECK(t.lastError != RIE_NOERROR);
    }

} // namespace

int main(int argc, char **argv) {
    const std::string which = argc > 1 ? argv[1] : "";
    if (which == "alias_diagnostics")
        aliasDiagnostics();
    else if (which == "unknown_fallback")
        unknownFallback();
    else {
        std::fprintf(stderr, "usage: %s alias_diagnostics|unknown_fallback\n", argv[0]);
        return 2;
    }
    return finish(which.c_str());
}
