/**
 * Project: openRender
 *
 * File: test_registry_integrator_statements.cpp
 *
 * Description:
 *   Spec 030 (hider and integrator registry), US3 AS3 and AS5, US4 AS5, FR-020, FR-021, FR-024.
 *
 *     Registry_UnknownIntegratorFallback  Integrator "bogus": one error at frame begin that lists the registered
 *                                         names and says it uses "classic"; the classic image (byte-identical to the
 *                                         frame with no statement); orender's exit status is set; with Hider "bogus"
 *                                         as well, both fall back (two errors, the reyes + classic image); a
 *                                         parameter the integrator does not define is reported by name and ignored,
 *                                         the frame still renders.
 *     Registry_IntegratorParameters       the parameters a statement carries reach the integrator's configure in the
 *                                         stored data shape (an int as int[], a color as 3 floats, a string as
 *                                         char *[]); a parameter configure does not accept is diagnosed by name; a
 *                                         string `handle` is consumed by the core (stored, not passed, no
 *                                         diagnostic); a non-string `handle` is rejected at the statement; the frame
 *                                         renders in every case.
 *
 *   Red by build failure until Phase 9 (the test-double integrator ABI). The case is the first argument.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include <cmath>
#include <cstring>
#include <string>

#include "common/diagnostics.h"
#include "common/rendererc.h"
#include "integrator_doubles.h"
#include "options.h"
#include "registry_testing.h"
#include "ri.h"
#include "riInterface.h"

using namespace regtest;

namespace {

    COptions *options() {
        return static_cast<CRiInterface *>(const_cast<void *>(RiGetContext()))->getOptions();
    }

    std::string image(const std::string &statements, const std::string &name, SSession *out = nullptr) {
        SFrame f;
        f.statements = statements;
        f.image = name + ".tif";
        removeFile(f.image);
        const SSession s = renderRib(frameText(f), name + ".rib");
        if (out != nullptr)
            *out = s;
        return fileExists(f.image) ? readFile(f.image) : std::string();
    }

    // Registry_UnknownIntegratorFallback
    void unknownFallback() {
        SSession base;
        const std::string baseline = image("", "unk_base", &base);
        REG_CHECK(base.frames.errors() == 0 && !baseline.empty());

        // 1. Integrator "bogus": one error naming the registered integrators, classic's image, a non-zero exit status.
        SSession a;
        const std::string imageA = image("  Integrator \"bogus\"\n", "unk_integrator", &a);
        REG_CHECK_MSG(a.frames.count("Integrator \"bogus\" unavailable; registered: ") == 1, "got:\n%s", a.frames.text().c_str());
        REG_CHECK(a.frames.count("classic") >= 1 && a.frames.count("Using \"classic\"") == 1);
        REG_CHECK(a.frames.errors() == 1);
        REG_CHECK(a.lastError != RIE_NOERROR);
        REG_CHECK_MSG(imageA == baseline, "the fallback renders the classic image");

        // 2. Both unknown: two errors, both fall back, the reyes + classic image.
        SSession b;
        const std::string imageB = image("  Hider \"bogus\"\n  Integrator \"bogus\"\n", "unk_both", &b);
        REG_CHECK_MSG(b.frames.count("Hider \"bogus\" unavailable") == 1, "got:\n%s", b.frames.text().c_str());
        REG_CHECK(b.frames.count("Integrator \"bogus\" unavailable; registered: ") == 1);
        REG_CHECK_MSG(b.frames.errors() == 2, "got:\n%s", b.frames.text().c_str());
        REG_CHECK(b.lastError != RIE_NOERROR);
        REG_CHECK(imageB == baseline);

        // 3. An undefined parameter of a known integrator (US3 AS5): reported by name, ignored, the frame renders.
        SSession c;
        const std::string imageC = image("  Integrator \"classic\" \"int maxdepth\" [5]\n", "unk_param", &c);
        REG_CHECK_MSG(c.frames.count("Integrator \"classic\" does not define parameter \"maxdepth\"; ignored") == 1, "got:\n%s", c.frames.text().c_str());
        REG_CHECK(c.frames.errors() == 1); // an error, like an unknown Option: orender exits -1 (maintainer answer Q4)
        REG_CHECK(c.lastError != RIE_NOERROR);
        REG_CHECK(imageC == baseline);

        // 4. Known and clean: no diagnostic at all.
        SSession d;
        const std::string imageD = image("  Integrator \"classic\"\n", "unk_classic", &d);
        REG_CHECK(d.frames.list.empty() && d.lastError == RIE_NOERROR && imageD == baseline);
    }

    const SRecordedParameter *find(const SIntegratorLog &log, const char *name) {
        for (const SRecordedParameter &p : log.parameters)
            if (p.name == name)
                return &p;
        return nullptr;
    }

    // Registry_IntegratorParameters
    void parameters() {
        // 1. The statement as the API gets it: the string handle is consumed by the core.
        {
            RiBegin(RI_NULL);
            sessionReports().clear();
            RiErrorHandler(sessionHandler);
            RtString handle = const_cast<RtString>("beauty");
            RiIntegrator(const_cast<RtToken>("double-recorder"), const_cast<RtToken>("string handle"), &handle, RI_NULL);
            REG_CHECK_MSG(sessionReports().list.empty(), "a string handle is consumed silently; got:\n%s", sessionReports().text().c_str());
            REG_CHECK(options()->integratorHandle != nullptr && std::strcmp(options()->integratorHandle, "beauty") == 0);
            REG_CHECK_MSG(options()->numIntegratorParameters == 0, "the handle is not a parameter of the integrator");
            // a non-string handle is rejected at the statement
            RtInt notAString = 3;
            RiIntegrator(const_cast<RtToken>("double-recorder"), const_cast<RtToken>("int handle"), &notAString, RI_NULL);
            REG_CHECK_MSG(sessionReports().count("Integrator parameter \"handle\" must be a string") == 1, "got:\n%s", sessionReports().text().c_str());
            REG_CHECK(sessionReports().errors() == 1);
            RiEnd();
        }

        // 2. Through a frame: the stored data shape reaches configure, the unaccepted one is diagnosed.
        resetIntegratorLogs();
        SFrame f;
        f.statements = "  Integrator \"double-recorder\" \"int maxdepth\" [5] \"color tint\" [0.1 0.2 0.3] \"string mode\" [\"fast\"] "
                       "\"float rejected\" [2] \"string handle\" [\"beauty\"]\n";
        f.image = "params_frame.tif";
        removeFile(f.image);
        const SSession s = renderRib(frameText(f), "params_frame.rib");
        const SIntegratorLog &log = integratorLog(DOUBLE_RECORDER);
        REG_CHECK_MSG(s.frames.count("Integrator \"double-recorder\" does not define parameter \"rejected\"; ignored") == 1, "got:\n%s",
                      s.frames.text().c_str());
        REG_CHECK_MSG(s.frames.errors() == 1, "only the unaccepted parameter is an error; got:\n%s", s.frames.text().c_str());
        REG_CHECK_MSG(fileExists(f.image), "the frame renders");
        REG_CHECK(log.created == 1 && log.configured == 1 && log.prepared == 1 && log.finished == 1 && log.destroyed == 1);
        REG_CHECK(log.hiderName == "reyes");
        REG_CHECK_MSG(log.parameters.size() == 4, "the handle is not passed; got %zu parameters", log.parameters.size());
        REG_CHECK(find(log, "handle") == nullptr);

        const SRecordedParameter *depth = find(log, "maxdepth");
        REG_CHECK(depth != nullptr && depth->type == TYPE_INTEGER && depth->numItems == 1 && depth->ints.size() == 1 && depth->ints[0] == 5);
        const SRecordedParameter *tint = find(log, "tint");
        REG_CHECK(tint != nullptr && tint->type == TYPE_COLOR && tint->numItems == 1 && tint->floats.size() == 3);
        if (tint != nullptr && tint->floats.size() == 3) {
            REG_CHECK(std::fabs(tint->floats[0] - 0.1f) < 1e-6f && std::fabs(tint->floats[1] - 0.2f) < 1e-6f && std::fabs(tint->floats[2] - 0.3f) < 1e-6f);
        }
        const SRecordedParameter *mode = find(log, "mode");
        REG_CHECK(mode != nullptr && mode->type == TYPE_STRING && mode->numItems == 1 && mode->strings.size() == 1 && mode->strings[0] == "fast");
        const SRecordedParameter *rejected = find(log, "rejected");
        REG_CHECK(rejected != nullptr && rejected->type == TYPE_FLOAT && rejected->numItems == 1 && rejected->floats.size() == 1 && rejected->floats[0] == 2.0f);

        // 3. A non-string handle in a frame: rejected at the statement, not passed, the frame renders.
        resetIntegratorLogs();
        SFrame g;
        g.statements = "  Integrator \"double-recorder\" \"int handle\" [3] \"int maxdepth\" [7]\n";
        g.image = "params_badhandle.tif";
        removeFile(g.image);
        const SSession t = renderRib(frameText(g), "params_badhandle.rib");
        REG_CHECK_MSG(t.frames.count("Integrator parameter \"handle\" must be a string") == 1, "got:\n%s", t.frames.text().c_str());
        REG_CHECK(t.frames.errors() == 1);
        REG_CHECK(fileExists(g.image));
        const SIntegratorLog &log2 = integratorLog(DOUBLE_RECORDER);
        REG_CHECK(log2.parameters.size() == 1 && find(log2, "handle") == nullptr && find(log2, "maxdepth") != nullptr);
    }

} // namespace

int main(int argc, char **argv) {
    const std::string which = argc > 1 ? argv[1] : "";
    if (which == "unknown_fallback")
        unknownFallback();
    else if (which == "parameters")
        parameters();
    else {
        std::fprintf(stderr, "usage: %s unknown_fallback|parameters\n", argv[0]);
        return 2;
    }
    return finish(which.c_str());
}
