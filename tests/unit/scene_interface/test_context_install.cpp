/**
 * Project: openRender
 *
 * File: test_context_install.cpp
 *
 * Description:
 *   Spec 038 (Scene Interface untangle), US3, contracts/active-context.md: constructing a context does not make
 *   it the active one; installing and uninstalling are explicit Scene Interface calls that also route the
 *   diagnostics. One source, built twice (tests/unit/scene_interface/CMakeLists.txt):
 *
 *   SceneInterface_ContextInstall      rule 1 only. It uses today's API, so it is red at run time on the
 *                                      starting tree: the constructor assigns the active context and attaches
 *                                      the diagnostics. (CHECK failures are printed, never a crash: the
 *                                      globals are put back before the test ends.)
 *   SceneInterface_ContextInstallRules rules 2 to 6, with riInstallContext / riUninstallContext /
 *                                      riPreviousContext, which do not exist yet: DISABLED, and its
 *                                      EXCLUDE_FROM_ALL executable does not compile on the starting tree
 *                                      (that failure is its red evidence, FR-029 decision A).
 *
 *   Rules: 1 construct, do not install: the active context and the diagnostic sink are unchanged. 2 install:
 *   the context is the active one, and error() reaches its handler. 3 uninstall: both are what they were
 *   before the install. 4 delete an installed, active context: as 3; delete an uninstalled one: neither
 *   changes. 5 nest (a renderer, then an archive writer inside it): RiArchiveEnd makes the renderer active
 *   again with its diagnostics re-attached, and the writer's output file is complete. 6 attach twice / detach
 *   with none: harmless (spec 023 behaviour, unchanged).
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
#include <string>
#include <vector>

#include "common/diagnostics.h"
#include "ri.h"
#include "riDiagnostics.h"
#include "riInterface.h"
#include "scene_interface_testing.h"

#ifdef SI_INSTALL_RULES
#include "renderer.h"
#endif

namespace {

#ifdef SI_INSTALL_RULES
    std::vector<std::string> g_handled;

    void recordingHandler(int, int, const char *message) { g_handled.push_back(message != nullptr ? message : ""); }

    bool handled(const char *text) {
        for (const std::string &m : g_handled)
            if (m.find(text) != std::string::npos)
                return true;
        return false;
    }
#endif

    // Rule 1: a constructed context is not installed.
    void rule1ConstructDoesNotInstall() {
        sitest::CRecordingSink before;
        CDiagnosticSink *saved = installDiagnosticSink(&before); // "the sink installed before the construction"
        CRiInterface *activeBefore = renderMan;

        CRiInterface *context = new CRiInterface();
        SI_CHECK_MSG(renderMan == activeBefore, "constructing a context made it the active one (renderMan changed)");
        SI_CHECK_MSG(sitest::currentSink() == &before, "constructing a context attached the diagnostics (the sink changed)");

        error(CODE_SYSTEM, "rule1-probe\n");
        SI_CHECK_MSG(before.lines.size() == 1 && before.lines[0].find("rule1-probe") != std::string::npos,
                     "an error() after the construction did not reach the sink installed before it (%zu line(s))", before.lines.size());

        // Put the globals back whatever the constructor did, then delete: the destructor un-assigns a context that is active
        delete context;
        installDiagnosticSink(saved);
        SI_CHECK_MSG(renderMan == activeBefore, "deleting the context left renderMan changed");
    }

#ifdef SI_INSTALL_RULES

    // Rule 2: install
    void rule2Install() {
        sitest::CRecordingSink before;
        CDiagnosticSink *saved = installDiagnosticSink(&before);
        CRiInterface *activeBefore = renderMan;
        g_handled.clear();

        CRiInterface *context = new CRiInterface();
        context->RiErrorHandler(recordingHandler);
        riInstallContext(context);

        SI_CHECK(renderMan == context);
        SI_CHECK(riPreviousContext(context) == activeBefore);
        SI_CHECK(sitest::currentSink() != &before);
        error(CODE_SYSTEM, "rule2-probe\n");
        SI_CHECK_MSG(handled("rule2-probe"), "an error() after riInstallContext did not reach the context's handler");
        SI_CHECK_MSG(before.lines.empty(), "the previous sink still received a diagnostic after the install");

        // Installing an installed context is a bug (CODE_BUG) and is ignored: nothing changes
        riInstallContext(context);
        SI_CHECK(renderMan == context);
        SI_CHECK(riPreviousContext(context) == activeBefore);

        // Rule 3: uninstall
        riUninstallContext(context);
        SI_CHECK(renderMan == activeBefore);
        SI_CHECK_MSG(sitest::currentSink() == &before, "uninstalling did not restore the sink that was there before the install");
        SI_CHECK(riPreviousContext(context) == nullptr);
        error(CODE_SYSTEM, "rule3-probe\n");
        SI_CHECK_MSG(before.lines.size() == 1, "after the uninstall an error() did not reach the restored sink");
        // Uninstalling a context that is not installed does nothing
        riUninstallContext(context);
        SI_CHECK(renderMan == activeBefore);

        delete context;
        installDiagnosticSink(saved);
    }

    // Rule 4: delete
    void rule4Delete() {
        sitest::CRecordingSink before;
        CDiagnosticSink *saved = installDiagnosticSink(&before);
        CRiInterface *activeBefore = renderMan;

        // deleting an installed, active context behaves as uninstalling it
        CRiInterface *installed = new CRiInterface();
        riInstallContext(installed);
        SI_CHECK(renderMan == installed);
        delete installed;
        SI_CHECK(renderMan == activeBefore);
        SI_CHECK_MSG(sitest::currentSink() == &before, "deleting the installed context did not restore the sink");

        // deleting an uninstalled one changes neither
        CRiInterface *active = new CRiInterface();
        riInstallContext(active);
        CDiagnosticSink *attached = sitest::currentSink();
        CRiInterface *idle = new CRiInterface();
        delete idle;
        SI_CHECK(renderMan == active);
        SI_CHECK(sitest::currentSink() == attached);

        delete active;
        SI_CHECK(renderMan == activeBefore);
        installDiagnosticSink(saved);
    }

    // Rule 4, the C API's RiContext(): an installed context that another handle was activated over. Deleting it
    // clears its mark and leaves the active context and the diagnostics alone.
    void rule4DeleteInstalledButInactive() {
        RiBegin(RI_NULL);
        CRiInterface *renderer = renderMan;
        CRiInterface *plain = new CRiInterface();
        riInstallContext(plain); // active: plain, previous: the renderer
        SI_CHECK(renderMan == plain);
        SI_CHECK(riPreviousContext(plain) == renderer);
        RiContext((RtContextHandle)renderer);
        SI_CHECK(renderMan == renderer);
        CDiagnosticSink *attached = sitest::currentSink();
        delete plain;
        SI_CHECK_MSG(renderMan == renderer, "deleting an installed but inactive context changed the active one");
        SI_CHECK_MSG(sitest::currentSink() == attached, "deleting an installed but inactive context changed the sink");
        RiEnd();
    }

    // Rule 5: nest (a renderer, then an archive writer inside it)
    void rule5Nest() {
        g_handled.clear();
        RiBegin(RI_NULL);
        CRiInterface *renderer = renderMan;
        SI_CHECK(renderer != nullptr);
        RiErrorHandler(recordingHandler);

        RtArchiveHandle writer = RiArchiveBegin((RtToken) "nestwriter.rib", RI_NULL);
        (void)writer;
        SI_CHECK_MSG(renderMan != renderer, "RiArchiveBegin did not make the archive writer the active context");
        RiSphere(1, -1, 1, 360, RI_NULL);
        RiArchiveEnd();

        SI_CHECK_MSG(renderMan == renderer, "RiArchiveEnd did not make the renderer active again");
        error(CODE_SYSTEM, "rule5-probe\n");
        SI_CHECK_MSG(handled("rule5-probe"), "after RiArchiveEnd diagnostics did not reach the renderer's handler (not re-attached)");

        // The writer's file is complete: its header, the Sphere, and it was closed (the delete flushed it)
        const std::string path = std::string(CRenderer::temporaryPath) + "nestwriter.rib";
        const std::string text = sitest::readFile(path);
        SI_CHECK_MSG(text.find("##RenderMan RIB-Structure") == 0, "the archive file %s has no header", path.c_str());
        SI_CHECK_MSG(text.find("Sphere 1 -1 1 360") != std::string::npos, "the archive file %s lacks the Sphere", path.c_str());
        RiEnd();
    }

    // Rule 6: attach twice, detach with none
    void rule6AttachDetach() {
        sitest::CRecordingSink before;
        CDiagnosticSink *saved = installDiagnosticSink(&before);

        riDiagnosticsDetach(); // none attached: harmless
        SI_CHECK(sitest::currentSink() == &before);
        riDiagnosticsAttach();
        CDiagnosticSink *attached = sitest::currentSink();
        SI_CHECK(attached != &before);
        riDiagnosticsAttach(); // idempotent: the sink saved the first time is not overwritten
        SI_CHECK(sitest::currentSink() == attached);
        riDiagnosticsDetach();
        SI_CHECK(sitest::currentSink() == &before);
        riDiagnosticsDetach();
        SI_CHECK(sitest::currentSink() == &before);

        installDiagnosticSink(saved);
    }

#endif // SI_INSTALL_RULES

} // namespace

int main() {
    rule1ConstructDoesNotInstall();
#ifdef SI_INSTALL_RULES
    rule2Install();
    rule4Delete();
    rule6AttachDetach();
    rule4DeleteInstalledButInactive();
    rule5Nest();
#endif
    if (sitest::failures() != 0) {
#ifdef SI_INSTALL_RULES
        std::fprintf(stderr, "SceneInterface_ContextInstallRules: %d failure(s)\n", sitest::failures());
#else
        std::fprintf(stderr, "SceneInterface_ContextInstall: %d failure(s)\n", sitest::failures());
#endif
        return 1;
    }
    std::printf("SceneInterface_ContextInstall: ok\n");
    return 0;
}
