/**
 * tests/gi_determinism/gi_detkit.h
 *
 * Spec 040: the helpers the gi_determinism tests share. Header-only. It includes spec 032's tests/gi_split/gi_testkit.h (file bytes,
 * SHA-256, the first difference of two byte vectors) and adds:
 *
 *   EXPECT / g_fail             the failure counter of every test of this directory
 *   runCommand / RunResult      one child process through system(): exit code, terminating signal
 *   Gpm / parseGpm              the layout of a photon map file (.gpm), the classification of an offset, the padding check
 *   Icache / parseIcache        the layout of an irradiance cache file (.icache): every node and every record, field by field
 *
 * The recording cache (a CIrradianceCache that copies every record a sampler hands it, the seam of spec 032's test_gi_keys.cpp) is in
 * gi_recording.h, so that this header names no renderer type.
 */

#ifndef GI_DETKIT_H
#define GI_DETKIT_H

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <set>
#include <string>
#include <vector>

#include <sys/wait.h>
#include <unistd.h>

#include "gi_split/gi_testkit.h"

namespace gidet {

    using gitest::Bytes;

    static int g_fail = 0;

#define EXPECT(cond, ...)                                        \
    do {                                                         \
        if (!(cond)) {                                           \
            ++gidet::g_fail;                                     \
            fprintf(stderr, "FAIL %s:%d  ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__);                        \
            fprintf(stderr, "\n");                               \
        }                                                        \
    } while (0)

    // ---------------------------------------------------------------------------------------------------------
    // child processes
    // ---------------------------------------------------------------------------------------------------------
    struct RunResult {
            int exitCode = -1; // the exit status, -1 when the child was killed
            int signal = 0;    // the terminating signal, 0 when it exited
            bool ok() const { return signal == 0 && exitCode == 0; }
    };

    inline RunResult runCommand(const std::string &cmd) {
        fflush(stdout);
        fflush(stderr);
        const int status = system(cmd.c_str());
        RunResult r;
        if (status != -1 && WIFEXITED(status)) {
            r.exitCode = WEXITSTATUS(status);
        }
        else if (status != -1 && WIFSIGNALED(status)) {
            r.signal = WTERMSIG(status);
        }
        return r;
    }

    inline std::string describe(const RunResult &r) {
        char b[80];
        if (r.signal != 0) {
            snprintf(b, sizeof(b), "killed by signal %d", r.signal);
        }
        else {
            snprintf(b, sizeof(b), "exit code %d", r.exitCode);
        }
        return b;
    }

    // `cd <dir> && <orender> <opts> <rib> > <log> 2>&1`: the environment (SHADERS, DISPLAYS, ORENDERHOME) is the test's, set by CTest
    inline RunResult runOrender(const std::string &orender, const std::string &dir, const std::string &rib, const std::string &opts, const std::string &log) {
        const std::string cmd = "cd \"" + dir + "\" && \"" + orender + "\" " + opts + " \"" + rib + "\" > \"" + log + "\" 2>&1";
        return runCommand(cmd);
    }

    inline bool makeDir(const std::string &path) {
        return system(("mkdir -p \"" + path + "\"").c_str()) == 0;
    }

    inline std::string absolute(const std::string &p) {
        if (!p.empty() && p[0] == '/') {
            return p;
        }
        char cwd[4096];
        if (getcwd(cwd, sizeof(cwd)) == nullptr) {
            return p;
        }
        return std::string(cwd) + "/" + p;
    }

    // ---------------------------------------------------------------------------------------------------------
    // a small deterministic generator (the same numbers on every platform)
    // ---------------------------------------------------------------------------------------------------------
    struct Lcg {
            uint64_t s;
            explicit Lcg(uint64_t seed) : s(seed * 2862933555777941757ull + 3037000493ull) {}
            uint32_t next() {
                s = s * 6364136223846793005ull + 1442695040888963407ull;
                return (uint32_t)(s >> 33);
            }
            float unit() { return (float)(next() & 0xFFFFFF) / 16777216.0f; }
            float range(float a, float b) { return a + (b - a) * unit(); }
    };

    // ---------------------------------------------------------------------------------------------------------
    // the .gpm layout (contracts/files-and-wire.md; measure_gi.py's gpm_layout)
    //   ropen header: magic (4), version[4] (16), strlen(name) (4), the type name and its NUL; then numItems, maxItems, then
    //   numItems + 1 items of 44 bytes (item 0 first), then the bound (24), two matrices (128) and the square root of maxPower (4)
    // ---------------------------------------------------------------------------------------------------------
    static const size_t kGpmItem = 44;

    struct Gpm {
            size_t headerLen = 0;
            int numItems = 0;
            int maxItems = 0;
            size_t itemsOff = 0;   // the offset of item 0
            size_t trailerOff = 0; // the offset of the bound
    };

    inline bool parseGpm(const Bytes &d, Gpm &g) {
        if (d.size() < 32) {
            return false;
        }
        int32_t n = 0;
        memcpy(&n, &d[20], 4);
        if (n < 0 || n > 64) {
            return false;
        }
        g.headerLen = 24 + (size_t)n + 1;
        if (d.size() < g.headerLen + 8) {
            return false;
        }
        memcpy(&g.numItems, &d[g.headerLen], 4);
        memcpy(&g.maxItems, &d[g.headerLen + 4], 4);
        g.itemsOff = g.headerLen + 8;
        g.trailerOff = g.itemsOff + kGpmItem * (size_t)(g.numItems + 1);
        return g.numItems >= 0 && d.size() >= g.trailerOff + 24 + 128 + 4;
    }

    // Every byte of a map file is defined (FR-004): item 0 is zero, and so are the padding pairs (offsets 26-27 and 42-43) of every item
    inline bool gpmPaddingZero(const Bytes &d, const Gpm &g, std::string &why) {
        char b[160];
        for (size_t k = 0; k < kGpmItem; k++) {
            if (d[g.itemsOff + k] != 0) {
                snprintf(b, sizeof(b), "item 0 byte %zu is 0x%02x", k, d[g.itemsOff + k]);
                why = b;
                return false;
            }
        }
        for (int i = 1; i <= g.numItems; i++) {
            const size_t base = g.itemsOff + kGpmItem * (size_t)i;
            const size_t pads[4] = {26, 27, 42, 43};
            for (size_t p : pads) {
                if (d[base + p] != 0) {
                    snprintf(b, sizeof(b), "item %d padding byte %zu is 0x%02x", i, p, d[base + p]);
                    why = b;
                    return false;
                }
            }
        }
        return true;
    }

    // ---------------------------------------------------------------------------------------------------------
    // the .icache layout: ropen header, int maxDepth, then nodes depth-first: centre (3 floats), side, int32 count, count records of 56
    // floats (P, N, irradiance, coverage, envdir, gP[21], gR[21], dP), int32 hasChild[8], then the children of the non-zero entries
    // ---------------------------------------------------------------------------------------------------------
    struct IcacheRecord {
            float f[56];
    };

    struct IcacheNode {
            int depth = 0;
            float centre[3] = {0, 0, 0};
            float side = 0;
            std::vector<IcacheRecord> records;
    };

    struct Icache {
            int maxDepth = 0;
            std::vector<IcacheNode> nodes; // depth-first, the order of the file
            size_t records() const {
                size_t n = 0;
                for (const IcacheNode &nd : nodes) {
                    n += nd.records.size();
                }
                return n;
            }
    };

    inline bool parseIcacheNode(const Bytes &d, size_t &off, int depth, Icache &c) {
        if (off + 20 > d.size()) {
            return false;
        }
        IcacheNode nd;
        nd.depth = depth;
        memcpy(nd.centre, &d[off], 12);
        memcpy(&nd.side, &d[off + 12], 4);
        int32_t count = 0;
        memcpy(&count, &d[off + 16], 4);
        off += 20;
        if (count < 0 || off + (size_t)count * 224 + 32 > d.size()) {
            return false;
        }
        nd.records.resize((size_t)count);
        for (int32_t i = 0; i < count; i++) {
            memcpy(nd.records[(size_t)i].f, &d[off], 224);
            off += 224;
        }
        int32_t has[8];
        memcpy(has, &d[off], 32);
        off += 32;
        c.nodes.push_back(nd);
        for (int i = 0; i < 8; i++) {
            if (has[i] != 0 && !parseIcacheNode(d, off, depth + 1, c)) {
                return false;
            }
        }
        return true;
    }

    inline bool parseIcache(const Bytes &d, Icache &c) {
        if (d.size() < 32) {
            return false;
        }
        int32_t n = 0;
        memcpy(&n, &d[20], 4);
        if (n < 0 || n > 64) {
            return false;
        }
        size_t off = 24 + (size_t)n + 1;
        if (d.size() < off + 4) {
            return false;
        }
        int32_t maxDepth = 0;
        memcpy(&maxDepth, &d[off], 4);
        c.maxDepth = maxDepth;
        off += 4;
        c.nodes.clear();
        return parseIcacheNode(d, off, 0, c) && off == d.size();
    }

    // The bit pattern of a float vector as hex, for failure messages and set comparison
    inline std::string bits(const float *v, int n) {
        std::string s;
        for (int i = 0; i < n; i++) {
            uint32_t u;
            memcpy(&u, &v[i], 4);
            char b[12];
            snprintf(b, sizeof(b), "%08x", u);
            s += b;
        }
        return s;
    }

} // namespace gidet

#endif // GI_DETKIT_H
