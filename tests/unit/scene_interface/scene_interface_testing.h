/**
 * Project: openRender
 *
 * File: scene_interface_testing.h
 *
 * Description:
 *   Helpers shared by the spec 038 (Scene Interface untangle) tests of this directory: a failure counter, file
 *   helpers, a recording diagnostic sink, the reference compare / capture routine, and the one-line text of a
 *   declared variable that the declared-set reference is written in.
 *
 *   A characterization test takes `--capture` (and `--recapture`) on its command line. `--capture` writes each
 *   reference that does not exist yet and checks nothing else; it never replaces an existing reference unless
 *   `--recapture` is also given. The build target scene-interface-capture-baselines (tests/CMakeLists.txt) is
 *   the only caller; ctest never passes the flag, so a run of the test suite cannot turn a changed output into
 *   its own reference.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef SCENE_INTERFACE_TESTING_H
#define SCENE_INTERFACE_TESTING_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include "common/diagnostics.h"
#include "common/variable.h"

namespace sitest {

    inline int &failures() {
        static int n = 0;
        return n;
    }

#define SI_CHECK_MSG(cond, ...)                                                             \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s -- ", __FILE__, __LINE__, #cond); \
            std::fprintf(stderr, __VA_ARGS__);                                              \
            std::fprintf(stderr, "\n");                                                     \
            ++sitest::failures();                                                           \
        }                                                                                   \
    } while (0)

#define SI_CHECK(cond)                                                                    \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++sitest::failures();                                                         \
        }                                                                                 \
    } while (0)

    struct Options {
            bool capture = false;
            bool recapture = false;
            bool onlyDifferences = false; // --capture-differences: write a platform override only where the result differs
    };

    // Strips --capture / --recapture / --capture-differences from argv (compacting it) and returns them.
    inline Options takeOptions(int &argc, char **argv) {
        Options o;
        int out = 1;
        for (int i = 1; i < argc; ++i) {
            if (std::strcmp(argv[i], "--capture") == 0)
                o.capture = true;
            else if (std::strcmp(argv[i], "--recapture") == 0)
                o.recapture = true;
            else if (std::strcmp(argv[i], "--capture-differences") == 0)
                o.onlyDifferences = true;
            else
                argv[out++] = argv[i];
        }
        argc = out;
        return o;
    }

    inline bool fileExists(const std::string &path) {
        std::ifstream f(path, std::ios::binary);
        return f.good();
    }

    inline std::string readFile(const std::string &path) {
        std::ifstream in(path, std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }

    inline bool writeFile(const std::string &path, const std::string &text) {
        std::ofstream out(path, std::ios::binary);
        out << text;
        return out.good();
    }

    // "line N: expected [..] actual [..]" for the first line where the two texts differ
    inline std::string firstDifference(const std::string &expected, const std::string &actual) {
        std::istringstream a(expected), b(actual);
        std::string la, lb;
        int line = 0;
        for (;;) {
            const bool ga = static_cast<bool>(std::getline(a, la));
            const bool gb = static_cast<bool>(std::getline(b, lb));
            ++line;
            if (!ga && !gb)
                return "same lines, different ends";
            if (ga != gb || la != lb)
                return "line " + std::to_string(line) + ": expected [" + (ga ? la : std::string("<end>")) + "] actual [" +
                       (gb ? lb : std::string("<end>")) + "]";
        }
    }

    // Where the references of one test live. The primary directory holds the macOS references (or the only
    // ones, for a test whose output does not depend on the platform); on Linux a file of the same name under
    // <root>/linux wins where one exists: it is captured only where the Linux result differs from the primary
    // one (the Linux gate of spec 038, T048), and every difference is explained in validation.md.
    struct RefLayout {
            std::string root;
            std::string primarySub; // "" for <root>/<name>, "macos" for <root>/macos/<name>

            std::string primary(const std::string &rel) const { return root + "/" + (primarySub.empty() ? "" : primarySub + "/") + rel; }
            std::string platformOverride(const std::string &rel) const { return root + "/linux/" + rel; }
            std::string lookup(const std::string &rel) const {
#ifdef __linux__
                if (fileExists(platformOverride(rel)))
                    return platformOverride(rel);
#endif
                return primary(rel);
            }
    };

    // Compares actual with the reference, or (capture modes) writes it:
    //   --capture              writes the primary reference if it does not exist yet; never replaces one
    //                          (--recapture also replaces)
    //   --capture-differences  writes <root>/linux/<rel> only if the lookup result differs from actual and
    //                          no override exists yet (--recapture also replaces)
    inline void compareOrCapture(const RefLayout &layout, const std::string &rel, const std::string &actual, const Options &opt, const std::string &what) {
        if (opt.onlyDifferences) {
            const std::string over = layout.platformOverride(rel);
            const std::string current = layout.lookup(rel);
            if (fileExists(over) && !opt.recapture) {
                std::printf("kept     %s\n", over.c_str());
                return;
            }
            if (fileExists(layout.primary(rel)) && readFile(layout.primary(rel)) == actual) {
                std::printf("same     %s\n", rel.c_str());
                if (fileExists(over))
                    std::printf("stale override %s (it equals the primary reference now)\n", over.c_str());
                return;
            }
            (void)current;
            std::string dir = over.substr(0, over.find_last_of('/'));
            std::string mk = "mkdir -p '" + dir + "'";
            if (std::system(mk.c_str()) != 0 || !writeFile(over, actual)) {
                std::fprintf(stderr, "cannot write %s\n", over.c_str());
                ++failures();
                return;
            }
            std::printf("DIFFERS  captured %s\n", over.c_str());
            return;
        }
        if (opt.capture) {
            const std::string path = layout.primary(rel);
            if (fileExists(path) && !opt.recapture) {
                std::printf("kept     %s (exists; configure with -DOR038_RECAPTURE=ON to replace)\n", path.c_str());
                return;
            }
            std::string dir = path.substr(0, path.find_last_of('/'));
            std::string mk = "mkdir -p '" + dir + "'";
            if (std::system(mk.c_str()) != 0 || !writeFile(path, actual)) {
                std::fprintf(stderr, "cannot write %s\n", path.c_str());
                ++failures();
                return;
            }
            std::printf("captured %s\n", path.c_str());
            return;
        }
        const std::string reference = layout.lookup(rel);
        if (!fileExists(reference)) {
            std::fprintf(stderr, "%s: missing reference %s (build the target scene-interface-capture-baselines)\n", what.c_str(), reference.c_str());
            ++failures();
            return;
        }
        const std::string expected = readFile(reference);
        if (expected != actual) {
            std::fprintf(stderr, "%s: differs from %s: %s\n", what.c_str(), reference.c_str(), firstDifference(expected, actual).c_str());
            ++failures();
        }
    }

    // Records every diagnostic as "code=<n> severity=<n> <message with \n escaped>".
    class CRecordingSink : public CDiagnosticSink {
        public:
            std::vector<std::string> lines;
            void report(EErrorCode code, EDiagnosticSeverity severity, const char *message) override {
                std::string text = message != nullptr ? message : "";
                std::string escaped;
                for (char c : text) {
                    if (c == '\n')
                        escaped += "\\n";
                    else
                        escaped += c;
                }
                lines.push_back("code=" + std::to_string((int)code) + " severity=" + std::to_string((int)severity) + " " + escaped);
            }
            std::string text() const {
                std::string out;
                for (const std::string &l : lines)
                    out += l + "\n";
                return out;
            }
    };

    inline CDiagnosticSink *currentSink() {
        CDiagnosticSink *s = installDiagnosticSink(nullptr);
        installDiagnosticSink(s);
        return s;
    }

    // The text the declared-set reference holds for one variable: every field the declaration table owns. `entry` is
    // meaningful only for a global variable (CVariable says "-1 if not global", but the declaration code leaves it
    // uninitialized: it read 1 on macOS and 117620528 on Linux), so it is written for globals only.
    inline std::string describeVariable(const CVariable &v) {
        char buffer[256];
        const std::string entry = v.storage == STORAGE_GLOBAL ? std::to_string(v.entry) : std::string("-");
        std::snprintf(buffer, sizeof(buffer), "%s type=%d container=%d storage=%d entry=%s numFloats=%d numItems=%d marker=0x%x", v.name, (int)v.type,
                      (int)v.container, (int)v.storage, entry.c_str(), v.numFloats, v.numItems, (unsigned)v.usageMarker);
        return buffer;
    }

    // The declared-set reference text (see capture_declared_set.cpp for the format). head is the first variable of
    // the variable list (the last one declared; the list is walked through CVariable::next), global(i) the i-th
    // entry of the global array.
    inline std::string declaredSetText(const CVariable *head, int numGlobals, const std::function<const CVariable *(int)> &global) {
        std::string text = "# spec 038: the table CRenderer::initDeclarations() builds, captured from the unmodified tree\n";
        int count = 0;
        for (const CVariable *v = head; v != nullptr; v = v->next)
            ++count;
        text += "variables " + std::to_string(count) + "\n";
        int index = 0;
        for (const CVariable *v = head; v != nullptr; v = v->next, ++index)
            text += "v " + std::to_string(index) + " " + describeVariable(*v) + "\n";
        text += "globals " + std::to_string(numGlobals) + "\n";
        for (int i = 0; i < numGlobals; ++i) {
            const CVariable *g = global(i);
            text += "g " + std::to_string(i) + " " + g->name + " entry=" + std::to_string(g->entry) + "\n";
        }
        return text;
    }

    // The name of the first variable of a declared-set reference (the head of the variable list), "" if none
    inline std::string declaredSetHead(const std::string &reference) {
        std::istringstream in(reference);
        std::string line;
        while (std::getline(in, line)) {
            if (line.rfind("v 0 ", 0) == 0) {
                std::istringstream fields(line.substr(4));
                std::string name;
                fields >> name;
                return name;
            }
        }
        return "";
    }

} // namespace sitest

#endif
