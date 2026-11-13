/**
 * Project: openRender
 *
 * File: test_registry_frame_abort.cpp
 *
 * Description:
 *   Spec 030 (hider and integrator registry), FR-024 and contracts/frame-transport.md "Late failures"
 *   (new test, maintainer answer Q-B, 2026-11-07). Registry_FrameAbort.
 *
 *   Three failures a built-in never has, produced by test doubles, each followed by a frame that must render:
 *     (a) a hider whose create returns NULL: one error (CODE_BUG, "could not create its thread 0 context"); the
 *         frame is aborted before any render thread starts (the hider double took no bucket job), nothing leaks;
 *     (b) an integrator whose configure returns non-OK: one error ("could not be configured"); the frame is
 *         REFUSED before the displays start (no output file); prepare and finish are never called; the instance
 *         that was created is destroyed;
 *     (c) an integrator whose prepare returns non-OK: one error ("could not prepare the frame"); the frame is
 *         aborted: the hiders that were built are destroyed at once, no render thread runs, finish is not called,
 *         the instance is destroyed.
 *   No crash in any of them (a Debug build keeps endFrame()'s asserts: this test also runs in the Debug tree of
 *   Phase 9), orender's exit status is set, and the following frame (reyes, classic) renders.
 *
 *   Red by assertion once it builds: until Phase 9 the abort and configure-refusal branches do not exist.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include <string>

#include "common/diagnostics.h"
#include "hider_double.h"
#include "integrator_doubles.h"
#include "registry_testing.h"
#include "ri.h"

using namespace regtest;

namespace {

    // A session: the failing frame, then a plain reyes frame that must render.
    SSession abortThenRender(const std::string &hider, const std::string &integrator, const std::string &failedImage, const std::string &laterImage,
                             const char *rib) {
        SFrame failing;
        failing.number = 1;
        failing.statements = "  Hider \"" + hider + "\"\n" + (integrator.empty() ? std::string() : "  Integrator \"" + integrator + "\"\n");
        failing.image = failedImage;
        failing.world = "    Declare \"abortvar\" \"uniform float\"\n";
        SFrame after;
        after.number = 2;
        after.statements = "  Hider \"reyes\"\n  Integrator \"classic\"\n";
        after.image = laterImage;
        removeFile(failedImage);
        removeFile(laterImage);
        return renderRib(frameText(failing) + frameText(after), rib);
    }

} // namespace

int main() {
    SHiderDoubleCounts &hiders = hiderDoubleCounts();

    // (a) create returns NULL
    {
        hiders.reset();
        resetIntegratorLogs();
        const SSession s = abortThenRender(HIDER_DOUBLE_NULLCREATE_NAME, "", "abort_nullcreate.tif", "abort_nullcreate_after.tif", "abort_nullcreate.rib");
        REG_CHECK_MSG(s.frames.count("Hider \"" HIDER_DOUBLE_NULLCREATE_NAME "\" could not create its thread 0 context") == 1, "got:\n%s",
                      s.frames.text().c_str());
        bool bug = false;
        for (const SReport &r : s.frames.list)
            if (r.code == CODE_BUG && r.text.find("could not create its thread") != std::string::npos)
                bug = true;
        REG_CHECK_MSG(bug, "the error carries CODE_BUG");
        REG_CHECK_MSG(s.frames.errors() == 1, "exactly one error; got:\n%s", s.frames.text().c_str());
        REG_CHECK(s.lastError != RIE_NOERROR);
        REG_CHECK_MSG(hiders.nullCreates == 1, "create called once (one thread), got %d", hiders.nullCreates.load());
        REG_CHECK_MSG(hiders.buckets == 0 && hiders.created == 0, "no render thread ran");
        REG_CHECK_MSG(integratorLog(DOUBLE_CONFIGURE_FAILS).created == 0, "no stray integrator");
        REG_CHECK_MSG(fileExists("abort_nullcreate_after.tif"), "the following frame renders");
    }

    // (b) configure returns non-OK: refused before the displays
    {
        hiders.reset();
        resetIntegratorLogs();
        const SSession s = abortThenRender("reyes", integratorDoubleName(DOUBLE_CONFIGURE_FAILS), "abort_configure.tif", "abort_configure_after.tif",
                                           "abort_configure.rib");
        const SIntegratorLog &log = integratorLog(DOUBLE_CONFIGURE_FAILS);
        REG_CHECK_MSG(s.frames.count("Integrator \"double-configure-fails\" could not be configured") == 1, "got:\n%s", s.frames.text().c_str());
        REG_CHECK_MSG(s.frames.errors() == 1, "exactly one error; got:\n%s", s.frames.text().c_str());
        REG_CHECK(s.lastError != RIE_NOERROR);
        REG_CHECK_MSG(log.created == 1 && log.configured == 1, "created %d configured %d", log.created, log.configured);
        REG_CHECK_MSG(log.prepared == 0 && log.finished == 0, "prepare and finish are never called (prepared %d finished %d)", log.prepared, log.finished);
        REG_CHECK_MSG(log.destroyed == log.created, "the created instance is destroyed (created %d destroyed %d)", log.created, log.destroyed);
        REG_CHECK_MSG(!fileExists("abort_configure.tif"), "refused before the displays: no output file");
        REG_CHECK_MSG(fileExists("abort_configure_after.tif"), "the following frame renders");
    }

    // (c) prepare returns non-OK: aborted after the displays and the hiders
    {
        hiders.reset();
        resetIntegratorLogs();
        const SSession s = abortThenRender(HIDER_DOUBLE_NAME, integratorDoubleName(DOUBLE_PREPARE_FAILS), "abort_prepare.tif", "abort_prepare_after.tif",
                                           "abort_prepare.rib");
        const SIntegratorLog &log = integratorLog(DOUBLE_PREPARE_FAILS);
        REG_CHECK_MSG(s.frames.count("Integrator \"double-prepare-fails\" could not prepare the frame") == 1, "got:\n%s", s.frames.text().c_str());
        REG_CHECK_MSG(s.frames.errors() == 1, "exactly one error; got:\n%s", s.frames.text().c_str());
        REG_CHECK(s.lastError != RIE_NOERROR);
        REG_CHECK_MSG(log.created == 1 && log.configured == 1 && log.prepared == 1, "created %d configured %d prepared %d", log.created, log.configured,
                      log.prepared);
        REG_CHECK_MSG(log.finished == 0, "finish is not called for a frame whose prepare failed (finished %d)", log.finished);
        REG_CHECK_MSG(log.destroyed == log.created, "the instance is destroyed (created %d destroyed %d)", log.created, log.destroyed);
        REG_CHECK_MSG(hiders.created == 1, "the hider of the frame was built (one thread), got %d", hiders.created.load());
        REG_CHECK_MSG(hiders.destroyed == hiders.created, "the hiders built so far are destroyed at once (created %d destroyed %d)", hiders.created.load(),
                      hiders.destroyed.load());
        REG_CHECK_MSG(hiders.buckets == 0, "no render thread ran (buckets %d)", hiders.buckets.load());
        // a driver may have written the file of the aborted frame (contracts/frame-transport.md): not asserted either way
        REG_CHECK_MSG(fileExists("abort_prepare_after.tif"), "the following frame renders");
    }

    return finish("Registry_FrameAbort");
}
