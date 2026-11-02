/**
 * Project: openRender
 *
 * File: test_self_registration.cpp
 *
 * Description:
 *   Spec 038 (Scene Interface untangle), Preview_SelfRegistration (US4; contracts/backend-registration.md).
 *   A program that links libribpreview alone (no renderer) begins a session with the back-end the preview
 *   registers itself, through the Scene Interface's named back-end hook:
 *
 *   1. RiBeginBackEnd("preview", nullptr) returns a non-null context that is the active one; RiEndBackEnd
 *      leaves no active context and the diagnostic sink that was installed before it in place.
 *   2. RiBeginBackEnd("nonexistent", nullptr) prints "Back-end \"nonexistent\" is not available in this build
 *      (none registered)", returns nullptr, and leaves the light-session slot free (a following
 *      RiBeginBackEnd("preview", nullptr) succeeds).
 *   3. FR-014, the unchanged diagnostics of RiBegin in this program: RiBegin("#x") prints the existing
 *      "Full-render mode is not available in this build" text (ri.cpp:315), because no renderer context is
 *      registered. RiBegin("file.rib") does NOT print the RIB-output text in this program: libribpreview
 *      links libRIBVector, which compiles output/ribOut.cpp and so registers the RIB writer (checked on the
 *      starting tree with nm: g_ribOutRegistrar is in libRIBVector). The case asserts that unchanged
 *      behaviour (a writer is active and wrote file.rib) and is reported to the maintainer: tasks.md T037 and
 *      FR-014 expect the "RIB-output mode is not available" text here, which only a program that links
 *      neither libRIBVector nor libRIBOut can produce.
 *
 *   DISABLED, with an EXCLUDE_FROM_ALL executable that does not compile on the starting tree (RiBeginBackEnd
 *   does not exist): that failure is its red evidence (FR-029, decision A). Phase 4 enables it.
 *   Usage: test_preview_self_registration <scratch dir>
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include <cstdio>
#include <filesystem>
#include <string>
#include <unistd.h>

#include "common/diagnostics.h"
#include "ri.h"
#include "riInterface.h"
#include "rib.h"
#include "scene_interface_testing.h"

namespace fs = std::filesystem;

namespace {

    bool saw(const sitest::CRecordingSink &sink, const char *text) {
        for (const std::string &l : sink.lines)
            if (l.find(text) != std::string::npos)
                return true;
        return false;
    }

    void preview() {
        CDiagnosticSink *before = sitest::currentSink();
        CRiInterface *activeBefore = renderMan;

        CRiInterface *context = RiBeginBackEnd("preview", nullptr);
        SI_CHECK_MSG(context != nullptr, "RiBeginBackEnd(\"preview\") returned nullptr: the preview did not register itself");
        SI_CHECK(renderMan == context);
        SI_CHECK(sitest::currentSink() != before); // diagnostics are routed to the context

        RiEndBackEnd(context);
        SI_CHECK_MSG(renderMan == activeBefore, "RiEndBackEnd left an active context");
        SI_CHECK_MSG(sitest::currentSink() == before, "RiEndBackEnd did not leave the previous sink in place");
    }

    void unknownKey() {
        sitest::CRecordingSink sink;
        CDiagnosticSink *saved = installDiagnosticSink(&sink);
        CRiInterface *context = RiBeginBackEnd("nonexistent", nullptr);
        installDiagnosticSink(saved);

        SI_CHECK(context == nullptr);
        SI_CHECK_MSG(saw(sink, "Back-end \"nonexistent\" is not available in this build (none registered)"), "the diagnostic is missing; got:\n%s",
                     sink.text().c_str());

        // the light-session slot is free
        CRiInterface *again = RiBeginBackEnd("preview", nullptr);
        SI_CHECK_MSG(again != nullptr, "a failed RiBeginBackEnd left the light-session slot occupied");
        if (again != nullptr)
            RiEndBackEnd(again);
    }

    void unchangedRiBeginDiagnostics(const fs::path &scratch) {
        {
            sitest::CRecordingSink sink;
            CDiagnosticSink *saved = installDiagnosticSink(&sink);
            RiBegin((RtToken) "#x");
            installDiagnosticSink(saved);
            SI_CHECK_MSG(saw(sink, "Full-render mode is not available in this build"), "RiBegin(\"#x\"): the diagnostic changed; got:\n%s",
                         sink.text().c_str());
            SI_CHECK(renderMan == nullptr);
        }
        {
            SI_CHECK(chdir(scratch.c_str()) == 0);
            sitest::CRecordingSink sink;
            CDiagnosticSink *saved = installDiagnosticSink(&sink);
            RiBegin((RtToken) "file.rib");
            const bool active = renderMan != nullptr;
            installDiagnosticSink(saved);
            SI_CHECK_MSG(!saw(sink, "RIB-output mode is not available in this build"), "RiBegin(\"file.rib\") reports the writer missing, but libRIBVector registers it");
            SI_CHECK_MSG(active, "RiBegin(\"file.rib\") did not make a writer active in a program that links libRIBVector");
            if (active)
                RiEnd();
            SI_CHECK_MSG(fs::exists(scratch / "file.rib"), "the writer did not write file.rib");
        }
    }

} // namespace

int main(int argc, char **argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s <scratch dir>\n", argv[0]);
        return 2;
    }
    const fs::path scratch = fs::absolute(argv[1]);
    std::error_code ec;
    fs::remove_all(scratch, ec);
    fs::create_directories(scratch);

    preview();
    unknownKey();
    unchangedRiBeginDiagnostics(scratch);

    if (sitest::failures() != 0) {
        std::fprintf(stderr, "Preview_SelfRegistration: %d failure(s)\n", sitest::failures());
        return 1;
    }
    std::printf("Preview_SelfRegistration: ok\n");
    return 0;
}
