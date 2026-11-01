/**
 * Project: openRender
 *
 * File: test_versions.cpp
 *
 * Description:
 *   Spec 037 (versioned plug-in ABIs), Phase 3: ProceduresAbi_Versions and ProceduresAbi_SameRoutine
 *   (US1, US5, FR-003, FR-004, FR-007, FR-009; SC-001, SC-002, SC-008).
 *
 *   An in-process program that links the renderer and drives the real RIB statements of the three
 *   procedural families: it opens a session, installs a diagnostic sink and a custom display that
 *   accepts every tile, and reads small archives (RiReadArchive) inside a world block:
 *       Geometry "dlo"      "string file" [<module>]
 *       Geometry "implicit" "string file" [<module>]
 *       Procedural "DynamicLoad" [<module> ""] [<bound>]
 *   The modules are the fixtures of tests/plugins/{dlo,implicit,dynamicload}/. Their call log
 *   (OR_TEST_PLUGIN_LOG, pluginLog.h) says which entry points ran.
 *
 *   argv: <plugins root (build/tests/plugins)> <mode>, mode versions or sameroutine.
 *
 *   versions
 *     good, hidden        loaded, no version diagnostic
 *     newer (1.9.0)       loaded, exactly one CODE_VERSION warning
 *     v2 (2.0.0)          rejected with exactly one CODE_VERSION error that names the file, the
 *                         interface, both versions and the remedy, and NO entry point of the module
 *                         is called (SC-001)
 *     legacy (no symbol)  loaded, exactly one warning per module per process across two instances
 *                         (SC-002)
 *     dynamicload plain and versioned load with no diagnostic at all (FR-009); v2 is rejected
 *   sameroutine
 *     the diagnostics of the three kinds (dlo, implicit, dynamicload) come out of one routine: the
 *     rejection, the newer-MINOR warning and the unversioned warning are the same text once the
 *     path and the interface name are replaced
 *
 *   Red on the starting tree by assertion: the loaders call no version check, so nothing is
 *   reported and a MAJOR 2 module is loaded and called.
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
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>

#include "common/diagnostics.h"
#include "ri/parse/ri.h"

namespace {

    struct SReport {
            EErrorCode code;
            EDiagnosticSeverity severity;
            std::string message;
    };

    class CRecordingSink : public CDiagnosticSink {
        public:
            void report(EErrorCode code, EDiagnosticSeverity severity, const char *message) override {
                reports.push_back({code, severity, message});
            }
            std::vector<const SReport *> versionReports() const {
                std::vector<const SReport *> v;
                for (const SReport &r : reports)
                    if (r.code == CODE_VERSION)
                        v.push_back(&r);
                return v;
            }
            std::vector<SReport> reports;
    };

    CRecordingSink g_sink;
    int g_failed = 0;
    std::string g_root;
    std::string g_log;
    int g_frame = 0;

    void expect(bool condition, const std::string &what) {
        if (!condition) {
            ++g_failed;
            printf("FAIL %s\n", what.c_str());
        }
    }

    bool contains(const std::string &text, const std::string &part) {
        return text.find(part) != std::string::npos;
    }

    // A display that takes every tile and writes nothing, so a frame really renders.
    void *acceptStart(const char *, int, int, int, const char *, RtDisplayParameterFunction) {
        static int handle;
        return &handle;
    }
    int acceptData(void *, int, int, int, int, float *) { return RI_TRUE; }
    void acceptFinish(void *) {}

    std::vector<std::string> readLog() {
        std::vector<std::string> lines;
        std::ifstream in(g_log);
        for (std::string line; std::getline(in, line);)
            lines.push_back(line);
        return lines;
    }

    // What one case left behind.
    struct SCase {
            std::vector<SReport> reports;
            std::vector<std::string> log;
            std::vector<SReport> versionReports;
    };

    // Runs one frame whose world block is `body`, with a fresh diagnostic list and call log.
    SCase runFrame(const std::string &body) {
        const std::string archive = g_log + ".rib";
        {
            std::ofstream out(archive);
            out << body << "\n";
        }
        unlink(g_log.c_str());
        g_sink.reports.clear();

        RiFrameBegin(++g_frame);
        RiFormat(8, 8, 1);
        RiPixelSamples(1, 1);
        float fov = 30.0f;
        RiProjection("perspective", "fov", &fov, RI_NULL);
        RiTranslate(0, 0, 4);
        RiWorldBegin();
        RiSurface("constant", RI_NULL);
        RiReadArchive((RtString)archive.c_str(), nullptr, RI_NULL);
        RiWorldEnd();
        RiFrameEnd();
        unlink(archive.c_str());

        SCase c;
        c.reports = g_sink.reports;
        c.log = readLog();
        for (const SReport &r : c.reports)
            if (r.code == CODE_VERSION)
                c.versionReports.push_back(r);
        return c;
    }

    std::string modulePath(const char *family, const char *variant) {
        return g_root + "/" + family + "/" + variant + "/" + variant + ".dsply";
    }

    std::string statement(const std::string &family, const std::string &path) {
        if (family == "dynamicload")
            return "Procedural \"DynamicLoad\" [\"" + path + "\" \"\"] [-1 1 -1 1 -1 1]";
        return "Geometry \"" + family + "\" \"string file\" [\"" + path + "\"]";
    }

    // The entry point the family's host calls first.
    std::string firstEntry(const std::string &family) {
        if (family == "dlo")
            return "dloInit";
        if (family == "implicit")
            return "implicitInit";
        return "ConvertParameters";
    }

    bool logHas(const SCase &c, const std::string &tag, const std::string &call) {
        for (const std::string &line : c.log)
            if (contains(line, tag + ":" + call))
                return true;
        return false;
    }

    bool logHasAnyEntry(const SCase &c, const std::string &tag) {
        for (const std::string &line : c.log)
            if (contains(line, " " + tag + ":") && !contains(line, tag + ":unload"))
                return true;
        return false;
    }

    // ---- versions ----
    void versionsFor(const std::string &family) {
        const std::string first = firstEntry(family);

        for (const char *variant : {"good", "hidden"}) {
            if (family == "dynamicload")
                continue;
            const SCase c = runFrame(statement(family, modulePath(family.c_str(), variant)));
            expect(c.versionReports.empty(), family + " " + variant + ": no version diagnostic");
            expect(logHas(c, variant, first), family + " " + variant + ": the module is loaded and initialized (" + first + ")");
        }

        if (family != "dynamicload") {
            const std::string newer = modulePath(family.c_str(), "newer");
            const SCase c = runFrame(statement(family, newer));
            expect(c.versionReports.size() == 1 && c.versionReports[0].severity == DIAGNOSTIC_WARNING, family + " newer: exactly one CODE_VERSION warning");
            if (!c.versionReports.empty())
                expect(contains(c.versionReports[0].message, newer) && contains(c.versionReports[0].message, family) && contains(c.versionReports[0].message, "1.9.0") && contains(c.versionReports[0].message, "1.0.0"), family + " newer: the warning names the file, the interface and both versions");
            expect(logHas(c, "newer", first), family + " newer: a newer MINOR is still loaded");

            // legacy: one warning per module per process, across two instances
            const std::string legacy = modulePath(family.c_str(), "legacy");
            const SCase two = runFrame(statement(family, legacy) + "\n" + statement(family, legacy));
            expect(two.versionReports.size() == 1 && two.versionReports[0].severity == DIAGNOSTIC_WARNING, family + " legacy: exactly one warning for two instances");
            if (!two.versionReports.empty())
                expect(contains(two.versionReports[0].message, legacy) && contains(two.versionReports[0].message, "1.0.0") && contains(two.versionReports[0].message, "1.2.0"), family + " legacy: the warning names the file, 1.0.0 and the release 1.2.0");
            expect(logHas(two, "legacy", first), family + " legacy: loaded");
            // and a third load, in another frame, prints nothing
            const SCase again = runFrame(statement(family, legacy));
            expect(again.versionReports.empty(), family + " legacy: a later load of the same module prints nothing");
        }
        else {
            for (const char *variant : {"plain", "versioned"}) {
                const SCase c = runFrame(statement(family, modulePath("dynamicload", variant)));
                expect(c.versionReports.empty(), family + " " + variant + ": loads with no version diagnostic (FR-009)");
                expect(logHas(c, variant, first), family + " " + variant + ": the generator is expanded");
            }
        }

        // v2: rejected, and no entry point of the module is called
        {
            const std::string v2 = modulePath(family.c_str(), "v2");
            const SCase c = runFrame(statement(family, v2));
            expect(c.versionReports.size() == 1 && c.versionReports[0].severity == DIAGNOSTIC_ERROR, family + " v2: exactly one CODE_VERSION error");
            if (!c.versionReports.empty()) {
                const std::string &m = c.versionReports[0].message;
                expect(contains(m, v2) && contains(m, family), family + " v2: the error names the file and the interface");
                expect(contains(m, "2.0.0") && contains(m, "1.0.0"), family + " v2: both versions");
                expect(contains(m, "rebuild against the installed header"), family + " v2: the remedy");
            }
            expect(!logHasAnyEntry(c, "v2"), family + " v2: no entry point of the rejected module was called (SC-001)");
        }
    }

    // ---- sameroutine ----
    // The message with the path and the interface name replaced, so the three families compare.
    std::string normalized(std::string message, const std::string &path, const std::string &interfaceName) {
        for (size_t at; (at = message.find(path)) != std::string::npos;)
            message.replace(at, path.size(), "<PATH>");
        for (size_t at = 0; (at = message.find(interfaceName, at)) != std::string::npos; at += 7)
            message.replace(at, interfaceName.size(), "<IFACE>");
        return message;
    }

    void sameRoutine() {
        const char *families[3] = {"dlo", "implicit", "dynamicload"};
        for (const char *kind : {"v2", "newer", "legacy"}) {
            std::vector<std::string> texts;
            for (const char *family : families) {
                if (std::string(family) == "dynamicload" && std::string(kind) != "v2")
                    continue; // dynamicload has no newer fixture and does not warn when unversioned
                const std::string path = modulePath(family, kind);
                const SCase c = runFrame(statement(family, path));
                expect(c.versionReports.size() == 1, std::string(family) + " " + kind + ": exactly one diagnostic");
                if (c.versionReports.size() == 1)
                    texts.push_back(normalized(c.versionReports[0].message, path, family));
            }
            for (size_t i = 1; i < texts.size(); ++i)
                expect(texts[i] == texts[0], std::string(kind) + ": the diagnostics of the families differ in more than the path and the interface name:\n    " + texts[0] + "\n    " + texts[i]);
        }
    }

} // namespace

int main(int argc, char **argv) {
    if (argc != 3) {
        fprintf(stderr, "usage: test_procedures_versions <plugins root> versions|sameroutine\n");
        return 2;
    }
    g_root = argv[1];
    const std::string mode = argv[2];
    g_log = "procedures_calls.log"; // in the test's working directory
    setenv("OR_TEST_PLUGIN_LOG", g_log.c_str(), 1);

    RiBegin(RI_NULL);
    installDiagnosticSink(&g_sink);
    RiCustomDisplay("procedures-versions", RI_RGBA, acceptStart, acceptData, acceptFinish, RI_NULL);

    if (mode == "versions") {
        versionsFor("dlo");
        versionsFor("implicit");
        versionsFor("dynamicload");
    }
    else if (mode == "sameroutine") {
        sameRoutine();
    }
    else {
        fprintf(stderr, "unknown mode %s\n", mode.c_str());
        return 2;
    }

    installDiagnosticSink(nullptr);
    RiEnd();
    unlink(g_log.c_str());

    if (g_failed != 0) {
        printf("%d check(s) failed\n", g_failed);
        return 1;
    }
    printf("ok\n");
    return 0;
}
