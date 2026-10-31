/**
 * Project: openRender
 *
 * File: trace_replay.h
 *
 * Description:
 *   Spec 026, Phase 1: the trace replay shared by the recorder and the checker.
 *
 *   A replay is a characterization of the starting tree (spec 028's motion replay is
 *   the precedent): it renders a fixed set of tiny raytrace scenes in process, once per
 *   shader backend, with CShadingContext::traceRequestObserver installed, and writes
 *   every request Shading hands to the tracer: kind, label, bundle depth, path depth, and
 *   per ray its from, dir, t, tmin, time, flags, da, db, key and depth, floats as hex
 *   floats. The recording is data (tests/shading_contract/replay/{macos,linux}.txt) made
 *   on the starting tree and never re-recorded; the checker compares a fresh run with it
 *   exactly.
 *
 *   One render per process: GitHub #2 (heap corruption on a second RiBegin/RiEnd cycle)
 *   is why the recorder and the checker re-execute themselves once per (scene, backend)
 *   in the hidden "section" mode, instead of looping over Ri sessions.
 *
 *   Single render thread (the scenes say so), so the order of the requests is defined.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef SHADING_CONTRACT_TRACE_REPLAY_H
#define SHADING_CONTRACT_TRACE_REPLAY_H

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ray.h"
#include "ri.h"
#include "shading.h"

extern char **environ;

namespace replay {

    // The eight builtins whose requests the replay pins. A scene and a shader are named after each.
    inline const std::vector<std::string> &builtins() {
        static const std::vector<std::string> b = {"trace", "transmission", "visibility", "gather", "environment", "shadow", "occlusion", "indirectdiffuse"};
        return b;
    }

    // The backends this build can render: .rslo always, .slo with the JIT.
    // (One initializer list, not a push_back after it: GCC 13's -Warray-bounds reads the push_back after a
    // one-element list as an overflow, and the build is -Werror.)
    inline const std::vector<std::string> &backends() {
#ifdef SHADING_CONTRACT_HAVE_JIT
        static const std::vector<std::string> b = {"rslo", "slo"};
#else
        static const std::vector<std::string> b = {"rslo"};
#endif
        return b;
    }

    inline const char *kindName(int kind) {
        switch (kind) {
        case TRACEREQUEST_TRACE: return "trace";
        case TRACEREQUEST_TRANSMISSION: return "transmission";
        case TRACEREQUEST_VISIBILITY: return "visibility";
        case TRACEREQUEST_GATHER: return "gather";
        case TRACEREQUEST_OCCLUSION: return "occlusion";
        case TRACEREQUEST_INDIRECTDIFFUSE: return "indirectdiffuse";
        }
        return "unknown";
    }

    // --- the observer ------------------------------------------------------------

    inline std::mutex &lineMutex() {
        static std::mutex m;
        return m;
    }
    inline std::vector<std::string> &lines() {
        static std::vector<std::string> l;
        return l;
    }
    inline int &requestCount() {
        static int n = 0;
        return n;
    }
    inline long &rayCount() {
        static long n = 0;
        return n;
    }

    inline std::string hex(float f) {
        char buf[48];
        snprintf(buf, sizeof(buf), "%a", (double)f);
        return buf;
    }
    inline std::string hex3(const float *v) { return hex(v[0]) + "," + hex(v[1]) + "," + hex(v[2]); }

    inline void observe(const CTraceRequestReport &r) {
        std::lock_guard<std::mutex> lock(lineMutex());
        std::string label = r.label != nullptr ? r.label : "-";
        for (char &c : label)
            if (c == ' ')
                c = '_';
        char head[256];
        snprintf(head, sizeof(head), "req kind=%s label=%s bundledepth=%d pathdepth=%d rays=%d", kindName(r.kind), label.c_str(), r.bundleDepth, r.pathDepth, r.numRays);
        lines().push_back(head);
        requestCount()++;
        for (int i = 0; i < r.numRays; i++) {
            const CRay *ray = r.rays[i];
            char key[128];
            snprintf(key, sizeof(key), "%llx:%llx:%d:%d", (unsigned long long)ray->key.full, (unsigned long long)ray->key.path, (int)ray->key.pixelSampleIndex, (int)ray->key.kind);
            std::string l = "ray from=" + hex3(ray->from) + " dir=" + hex3(ray->dir) + " t=" + hex(ray->t) + " tmin=" + hex(ray->tmin) + " time=" + hex(ray->time) + " flags=" + std::to_string(ray->flags) + " da=" + hex(ray->da) + " db=" + hex(ray->db) + " key=" + key + " depth=" + std::to_string((int)ray->depth);
            lines().push_back(l);
            rayCount()++;
        }
    }

    // A display that takes the pixels and discards them (a scene has no Display statement).
    inline void *displayStart(const char *, int, int, int, const char *, RtDisplayParameterFunction) {
        static int token;
        return &token;
    }
    inline int displayData(void *, int, int, int, int, float *) { return RI_TRUE; }
    inline void displayFinish(void *) {}

    // Render one scene on one backend, in this process, and return its section. Call it once per process.
    inline std::vector<std::string> renderSection(const std::string &builtin, const std::string &backend) {
        const char *dir = getenv("SHADING_CONTRACT_REPLAY_SCENES");
        if (dir == nullptr) {
            fprintf(stderr, "trace_replay: SHADING_CONTRACT_REPLAY_SCENES is not set\n");
            exit(2);
        }
        const std::string rib = std::string(dir) + "/replay_" + builtin + ".rib";
        lines().clear();
        requestCount() = 0;
        rayCount() = 0;
        RiBegin(RI_NULL);
        RtString format = const_cast<RtString>(backend.c_str());
        RiOption((RtToken) "shaderformat", (RtToken) "default", &format, RI_NULL);
        RiCustomDisplay("trace-replay", RI_RGBA, displayStart, displayData, displayFinish, RI_NULL);
        CShadingContext::traceRequestObserver = &observe;
        RiReadArchive((RtToken)rib.c_str(), NULL, NULL);
        CShadingContext::traceRequestObserver = nullptr;
        RiEnd();
        std::vector<std::string> out;
        out.push_back("@@ section scene=" + builtin + " backend=" + backend);
        out.insert(out.end(), lines().begin(), lines().end());
        out.push_back("@@ end requests=" + std::to_string(requestCount()) + " rays=" + std::to_string(rayCount()));
        return out;
    }

    // --- running a section in a child process -----------------------------------

    // Re-execute this program as `<self> section <builtin> <backend> <outfile>`; return its exit status.
    inline int runSectionChild(const char *self, const std::string &builtin, const std::string &backend, const std::string &outFile) {
        std::vector<std::string> args = {self, "section", builtin, backend, outFile};
        std::vector<char *> argv;
        for (std::string &a : args)
            argv.push_back(const_cast<char *>(a.c_str()));
        argv.push_back(nullptr);
        pid_t pid;
        if (posix_spawn(&pid, self, nullptr, nullptr, argv.data(), environ) != 0)
            return -1;
        int status = 0;
        if (waitpid(pid, &status, 0) < 0)
            return -1;
        return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
    }

    inline std::vector<std::string> readLines(const std::string &path, bool *ok = nullptr) {
        std::vector<std::string> out;
        std::ifstream in(path);
        if (ok != nullptr)
            *ok = in.good();
        std::string l;
        while (std::getline(in, l))
            out.push_back(l);
        return out;
    }

    inline std::string scratchFile(const std::string &builtin, const std::string &backend) {
        return "replay-section-" + builtin + "-" + backend + ".tmp";
    }

    // The section of a replay file for (builtin, backend): the lines from its "@@ section" line to its "@@ end" line.
    inline bool findSection(const std::vector<std::string> &all, const std::string &builtin, const std::string &backend, std::vector<std::string> &out) {
        const std::string head = "@@ section scene=" + builtin + " backend=" + backend;
        size_t i = 0;
        for (; i < all.size(); i++)
            if (all[i] == head)
                break;
        if (i == all.size())
            return false;
        out.clear();
        for (; i < all.size(); i++) {
            out.push_back(all[i]);
            if (all[i].rfind("@@ end", 0) == 0)
                return true;
        }
        return false;
    }

    inline std::vector<std::string> tokens(const std::string &line) {
        std::vector<std::string> t;
        std::istringstream s(line);
        std::string w;
        while (s >> w)
            t.push_back(w);
        return t;
    }

    inline std::string tokenName(const std::string &t) {
        const size_t eq = t.find('=');
        return eq == std::string::npos ? t : t.substr(0, eq);
    }

    // Compare a fresh section with a recorded one, exactly. Returns "" when equal, else a message naming the
    // builtin, the backend, the request index, the ray index and the first differing field.
    inline std::string compareSections(const std::string &builtin, const std::string &backend, const std::vector<std::string> &recorded, const std::vector<std::string> &fresh) {
        const std::string who = "TraceReplay " + builtin + " (" + backend + "): ";
        int request = -1;
        int ray = -1;
        const size_t n = std::min(recorded.size(), fresh.size());
        for (size_t i = 1; i < n; i++) {
            const std::string &a = recorded[i];
            const std::string &b = fresh[i];
            if (b.rfind("req ", 0) == 0) {
                request++;
                ray = -1;
            }
            else if (b.rfind("ray ", 0) == 0)
                ray++;
            if (a == b)
                continue;
            const std::vector<std::string> ta = tokens(a), tb = tokens(b);
            for (size_t k = 0; k < std::max(ta.size(), tb.size()); k++) {
                const std::string va = k < ta.size() ? ta[k] : "(missing)";
                const std::string vb = k < tb.size() ? tb[k] : "(missing)";
                if (va != vb) {
                    std::string m = who + "request " + std::to_string(request);
                    if (ray >= 0)
                        m += " ray " + std::to_string(ray);
                    m += " field " + tokenName(va == "(missing)" ? vb : va) + " differs: recorded " + va + ", fresh " + vb;
                    return m;
                }
            }
            return who + "request " + std::to_string(request) + " line differs: recorded \"" + a + "\", fresh \"" + b + "\"";
        }
        if (recorded.size() != fresh.size())
            return who + "the number of lines differs: recorded " + std::to_string(recorded.size()) + ", fresh " + std::to_string(fresh.size()) + " (the section ends or goes on after request " + std::to_string(request) + ")";
        return "";
    }

    inline std::string replayFilePath() {
        const char *dir = getenv("SHADING_CONTRACT_REPLAY_DIR");
#ifdef __APPLE__
        const char *name = "macos.txt";
#else
        const char *name = "linux.txt";
#endif
        return std::string(dir != nullptr ? dir : "") + "/" + name;
    }

    // The hidden mode: `<self> section <builtin> <backend> <outfile>`.
    inline int sectionMain(int argc, char **argv) {
        if (argc != 5)
            return 2;
        const std::vector<std::string> section = renderSection(argv[2], argv[3]);
        std::ofstream out(argv[4]);
        for (const std::string &l : section)
            out << l << "\n";
        return out.good() ? 0 : 1;
    }

} // namespace replay

#endif
