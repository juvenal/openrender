/**
 * Project: openRender
 *
 * File: registry_testing.h
 *
 * Description:
 *   Spec 030 (hider and integrator registry): the helpers the tests of tests/unit/registry share. A
 *   check macro that counts, a recording diagnostic sink (for the reports raised before any RI context
 *   exists: registration, local registries), an RI error handler that records and forwards to
 *   RiErrorPrint (for the reports of a frame: it sets RiLastError the way orender's exit code reads it),
 *   a scene writer and an in-process render of a RIB text.
 *
 *   Header-only, used by the registry tests only; it names no hider and no integrator class.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef REGISTRY_TESTING_H
#define REGISTRY_TESTING_H

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

#include "common/diagnostics.h"
#include "ri.h"

namespace regtest {

    inline int &failures() {
        static int n = 0;
        return n;
    }

#define REG_CHECK(cond)                                                                   \
    do {                                                                                  \
        if (!(cond)) {                                                                    \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s\n", __FILE__, __LINE__, #cond); \
            ++regtest::failures();                                                        \
        }                                                                                 \
    } while (0)

#define REG_CHECK_MSG(cond, ...)                                                            \
    do {                                                                                    \
        if (!(cond)) {                                                                      \
            std::fprintf(stderr, "%s:%d: CHECK failed: %s -- ", __FILE__, __LINE__, #cond); \
            std::fprintf(stderr, __VA_ARGS__);                                              \
            std::fprintf(stderr, "\n");                                                     \
            ++regtest::failures();                                                          \
        }                                                                                   \
    } while (0)

    // One report: the error code (EErrorCode / RIE_*), the severity (DIAGNOSTIC_* or RIE_*, by the recorder
    // that took it) and the text.
    struct SReport {
            int code;
            int severity;
            std::string text;
    };

    // Counting helpers over a list of reports.
    struct SReports {
            std::vector<SReport> list;

            int count(const char *needle) const {
                int n = 0;
                for (const SReport &r : list)
                    if (r.text.find(needle) != std::string::npos)
                        ++n;
                return n;
            }
            bool has(const char *needle) const { return count(needle) > 0; }
            // reports of severity ERROR or worse (RIE_ERROR == 2 and DIAGNOSTIC_ERROR are both "error")
            int errors() const {
                int n = 0;
                for (const SReport &r : list)
                    if (r.severity >= errorSeverity)
                        ++n;
                return n;
            }
            int warnings() const {
                int n = 0;
                for (const SReport &r : list)
                    if (r.severity == warningSeverity)
                        ++n;
                return n;
            }
            std::string text() const {
                std::string s;
                for (const SReport &r : list)
                    s += "  [code " + std::to_string(r.code) + " severity " + std::to_string(r.severity) + "] " + r.text;
                return s;
            }
            void clear() { list.clear(); }

            // Both recorders below store the severity as DIAGNOSTIC_* so one threshold serves both.
            static constexpr int errorSeverity = DIAGNOSTIC_ERROR;
            static constexpr int warningSeverity = DIAGNOSTIC_WARNING;
    };

    // The diagnostic sink: what a program sees from code that has no RI context yet (registration, a local
    // registry). Install with installDiagnosticSink() and put the previous one back.
    class CRecordingSink : public CDiagnosticSink {
        public:
            SReports reports;
            void report(EErrorCode code, EDiagnosticSeverity severity, const char *message) override {
                reports.list.push_back({(int)code, (int)severity, message});
            }
    };

    // The RI error handler of a session: records the report (severity converted to DIAGNOSTIC_*) and forwards it
    // to RiErrorPrint, which sets RiLastError for errors, the value orender's exit code reads.
    inline SReports &sessionReports() {
        static SReports r;
        return r;
    }
    inline void sessionHandler(RtInt code, RtInt severity, const char *message) {
        int s = DIAGNOSTIC_INFO;
        if (severity == RIE_WARNING)
            s = DIAGNOSTIC_WARNING;
        else if (severity == RIE_ERROR)
            s = DIAGNOSTIC_ERROR;
        else if (severity == RIE_SEVERE)
            s = DIAGNOSTIC_SEVERE;
        sessionReports().list.push_back({(int)code, s, message});
        RiErrorPrint(code, severity, message);
    }

    // ---- files ---------------------------------------------------------------------------------------------

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
    inline void removeFile(const std::string &path) {
        unlink(path.c_str());
    }

    // ---- scenes --------------------------------------------------------------------------------------------

    // The header of a frame: threads, size, buckets, then the caller's statements, then the world block with one
    // light and one sphere. `statements` is placed inside the frame, before WorldBegin (an option block), and
    // `world` inside the world block after the sphere.
    struct SFrame {
            int number = 1;
            std::string statements; // e.g. "Hider \"zbuffer\"\nIntegrator \"classic\"\n"
            std::string image;      // Display file name; empty: no Display statement
            std::string world;      // extra world-block statements
            int threads = 1;
    };

    inline std::string frameText(const SFrame &f) {
        std::ostringstream s;
        s << "FrameBegin " << f.number << "\n"
          << "  Option \"limits\" \"numthreads\" [" << f.threads << "]\n"
          << "  Option \"limits\" \"bucketsize\" [16 16]\n"
          << "  Format 32 32 1\n"
          << "  PixelSamples 1 1\n"
          << "  Projection \"perspective\" \"fov\" 30\n"
          << "  Translate 0 0 4\n";
        if (!f.image.empty())
            s << "  Display \"" << f.image << "\" \"file\" \"rgba\"\n";
        s << f.statements << "  WorldBegin\n"
          << "    LightSource \"distantlight\" 1\n"
          << "    Surface \"matte\"\n"
          << "    Sphere 1 -1 1 360\n"
          << f.world << "  WorldEnd\n"
          << "FrameEnd\n";
        return s.str();
    }

    // The photon frame: a photon pass writing `map` (needs a light and receivers; no Display).
    inline std::string photonFrameText(int number, const std::string &statements, const std::string &map, int emit = 2000) {
        std::ostringstream s;
        s << "FrameBegin " << number << "\n"
          << "  Option \"limits\" \"numthreads\" [1]\n"
          << "  Hider \"photon\" \"int emit\" [" << emit << "]\n"
          << statements << "  Projection \"perspective\" \"fov\" 45\n"
          << "  Format 32 24 1\n"
          << "  Translate 0 -1 5\n"
          << "  WorldBegin\n"
          << "    LightSource \"pointlight\" 1 \"intensity\" [4] \"from\" [0 2.2 0.5]\n"
          << "    Attribute \"photon\" \"string globalmap\" [\"" << map << "\"]\n"
          << "    Surface \"matte\"\n"
          << "    Polygon \"P\" [-3 0 -3  -3 0 3  3 0 3  3 0 -3]\n"
          << "    Sphere 0.6 -0.6 0.6 360\n"
          << "  WorldEnd\n"
          << "FrameEnd\n";
        return s.str();
    }

    // What one in-process session left behind.
    struct SSession {
            SReports registration; // reports raised before the RI context existed (the sink installed before RiBegin)
            SReports frames;       // everything the RI error handler saw
            int lastError;         // RiLastError at the end of the session (orender exits -1 when it is not RIE_NOERROR)
    };

    // Writes `rib` to `path`, runs one RiBegin / RiReadArchive / RiEnd session over it and returns the reports.
    // The environment (SHADERS, ORENDERHOME) is the test's; a Display in the RIB is written to the working directory.
    inline SSession renderRib(const std::string &rib, const std::string &path) {
        SSession out;
        writeFile(path, rib);
        CRecordingSink sink;
        CDiagnosticSink *previous = installDiagnosticSink(&sink);
        sessionReports().clear();
        RiLastError = RIE_NOERROR;
        RiBegin(RI_NULL);
        RiErrorHandler(sessionHandler);
        RiReadArchive((RtToken)path.c_str(), NULL, NULL);
        RiEnd();
        installDiagnosticSink(previous);
        out.registration = sink.reports;
        out.frames = sessionReports();
        out.lastError = RiLastError;
        return out;
    }

    // Exit helper: 0 when every check held.
    inline int finish(const char *what) {
        if (failures() != 0) {
            std::fprintf(stderr, "%s: FAILED, %d check(s)\n", what, failures());
            return 1;
        }
        std::fprintf(stderr, "%s: ok\n", what);
        return 0;
    }

} // namespace regtest

#endif
