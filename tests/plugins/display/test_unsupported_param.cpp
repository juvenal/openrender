/**
 * Project: openRender
 *
 * File: test_unsupported_param.cpp
 *
 * Description:
 *   Spec 037 (versioned plug-in ABIs), FR-040: RiDisplay must not store a display
 *   parameter whose type no display driver can receive.
 *
 *   The RIB parameter types double and mpoint (and quad, where the declaration grammar
 *   accepts it) reach CRendererContext::RiDisplayV's switch with no case that sets the
 *   entry's type or data. The entry is still counted (j is incremented), with its name and
 *   numItems set and its type and data uninitialized. The display copy constructor, the
 *   destructor and the FR-015 snapshot all switch on that type.
 *
 *   The check asserts only the stored parameter count and the number of CODE_BADTOKEN
 *   diagnostics. It never reads the type or data of an entry it has not proven initialized:
 *   when the count is wrong, the program reports the defect, flushes and leaves with
 *   _exit(), before the clone below and before RiEnd would free the display (both read the
 *   uninitialized type).
 *
 *   Expected: one stored parameter (the float) and exactly one diagnostic per dropped
 *   parameter. A COptions clone of the display (CDisplay's copy constructor, which is what
 *   the frame's option copy uses) has the same count.
 *
 *   Red on the starting tree: the double is counted (count 2) and nothing is reported.
 *
 *   `quad` is not a token of the declaration grammar (variable.l has float, double,
 *   mpoint and the others, no quad), so the check has no quad case.
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
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

#include "common/diagnostics.h"
#include "ri/parse/ri.h"
#include "ri/render/renderer.h"
#include "ri/render/rendererContext.h"
#include "ri/state/options.h"

namespace {

    // Counts the BADTOKEN diagnostics that name a given parameter.
    class CCountingSink : public CDiagnosticSink {
        public:
            void report(EErrorCode code, EDiagnosticSeverity, const char *message) override {
                if (code == CODE_BADTOKEN)
                    messages.push_back(message);
            }

            int countNaming(const char *name) const {
                int n = 0;
                for (const std::string &m : messages)
                    if (m.find(name) != std::string::npos)
                        ++n;
                return n;
            }

            std::vector<std::string> messages;
    };

    CCountingSink g_sink;
    int g_failed = 0;

    // The defect is present: say so and leave without touching the display again.
    [[noreturn]] void defectPresent(const char *what, int stored, int warnings) {
        fprintf(stdout, "DEFECT FR-040 (%s): stored parameter count %d (expected 1), warnings naming the dropped parameter %d (expected 1)\n", what, stored, warnings);
        for (const std::string &m : g_sink.messages)
            fprintf(stdout, "  diagnostic: %s", m.c_str());
        fflush(stdout);
        _exit(1);
    }

    void expectTrue(const char *label, bool condition) {
        if (!condition) {
            ++g_failed;
            fprintf(stdout, "FAIL %s\n", label);
        }
    }

    const COptions::CDisplay *storedDisplay() {
        return CRenderer::context->getOptions()->displays;
    }

    void checkCase(const char *what, const char *droppedToken, const char *droppedName) {
        float one = 1.0f;
        float dummy[4] = {0, 0, 0, 0};
        g_sink.messages.clear();

        // Display "case.tif" "file" "rgba" "<unsupported>" [..] "float ff" [1]
        RiDisplay("case.tif", "file", "rgba", droppedToken, (RtPointer)dummy, "float ff", &one, RI_NULL);

        const COptions::CDisplay *display = storedDisplay();
        const int stored = display != nullptr ? display->numParameters : -1;
        const int warnings = g_sink.countNaming(droppedName);

        // Count first. Nothing below runs when the count is wrong.
        if (stored != 1 || warnings != 1)
            defectPresent(what, stored, warnings);

        // Entry 0 is initialized now (the count covers initialized entries only).
        expectTrue("the stored parameter is the float", display->parameters[0].numItems == 1 && std::strcmp(display->parameters[0].name, "ff") == 0);

        // A clone (what a COptions copy makes) holds the same single parameter.
        COptions::CDisplay *clone = new COptions::CDisplay(display);
        expectTrue("a clone has the same count", clone->numParameters == 1);
        expectTrue("a clone keeps the float", clone->numParameters == 1 && std::strcmp(clone->parameters[0].name, "ff") == 0);
        delete clone;
    }

} // namespace

int main() {
    fprintf(stdout, "DisplayAbi_UnsupportedParamType (FR-040)\n");

    RiBegin(RI_NULL);
    CDiagnosticSink *previous = installDiagnosticSink(&g_sink);

    checkCase("double", "double dd", "dd");
    checkCase("mpoint", "mpoint mp", "mp");

    installDiagnosticSink(previous);
    RiEnd();

    if (g_failed != 0) {
        fprintf(stdout, "%d check(s) failed\n", g_failed);
        return 1;
    }
    fprintf(stdout, "ok\n");
    return 0;
}
