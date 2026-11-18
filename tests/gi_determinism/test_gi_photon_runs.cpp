/**
 * tests/gi_determinism/test_gi_photon_runs.cpp
 *
 * Spec 040, US1 (FR-001, FR-002, FR-004, FR-006; SC-002): the photon map a render writes is the same on every run and at every thread
 * count. Each render is a separate orender process; the written .gpm files are compared byte for byte.
 *
 *   repeat <fresh|reuse> <runs>   identity-photon.rib (2000 photons, one thread) rendered <runs> times. `fresh` removes the map before each run,
 *                                 `reuse` leaves the previous run's map in place (research.md R1's second cause: the stale maxPower). All
 *                                 runs must exit 0 and write one and the same file.
 *   threads                       identity-photon.rib with its thread pin removed, at -t:1, -t:2, -t:4, -t:8 and the default: one file
 *   threads4 <runs>               perf-photon-pass.rib (three lights, 20000 emitted, 46273 photons) at -t:4, <runs> times: no crash, one
 *                                 file, numItems 46273 every time (starting tree: 9 to 11 of 30 crash, every completed map different)
 *
 * Usage: test_gi_photon_runs <mode> [args] <orender> <tools dir> <work dir>
 *   tools dir  specs/040-gi-determinism/tools (identity-photon.rib, perf-photon-pass.rib)
 * A crash of a child is a test failure, never a skipped run.
 * Red reasons on the starting tree: assertion (2 distinct maps in 30 fresh runs, 28 in 30 reuse runs, research.md R1; 21 distinct maps and 9
 * of 30 crashes at -t:4, R5).
 */

#include <map>
#include <string>
#include <vector>

#include "gi_detkit.h"

using namespace gidet;

static std::string g_orender, g_tools, g_work;

static bool substitute(const std::string &from, const std::string &to, const std::string &threads, bool dropPin) {
    Bytes in;
    if (!gitest::readFile(from, in)) {
        return false;
    }
    std::string text(in.begin(), in.end()), out;
    size_t pos = 0;
    while (pos < text.size()) {
        size_t eol = text.find('\n', pos);
        if (eol == std::string::npos) {
            eol = text.size();
        }
        std::string line = text.substr(pos, eol - pos);
        pos = eol + 1;
        if (dropPin && line.find("\"numthreads\"") != std::string::npos) {
            continue;
        }
        size_t at = line.find("@THREADS@");
        if (at != std::string::npos) {
            line.replace(at, 9, threads);
        }
        out += line + "\n";
    }
    return gitest::writeText(to, out);
}

struct Series {
        std::map<std::string, int> histogram; // sha256 -> runs
        std::vector<int> items;
        int crashes = 0;
};

// One render; returns false when the child failed or wrote no map
static bool oneRun(const std::string &rib, const std::string &dir, const std::string &map, const std::string &opts, Series &s, const char *label) {
    const RunResult r = runOrender(g_orender, dir, rib, opts, dir + "/render.log");
    if (!r.ok()) {
        s.crashes++;
        EXPECT(false, "%s: the render failed (%s)", label, describe(r).c_str());
        return false;
    }
    Bytes b;
    if (!gitest::readFile(dir + "/" + map, b)) {
        EXPECT(false, "%s: no %s was written", label, map.c_str());
        return false;
    }
    Gpm g;
    if (!parseGpm(b, g)) {
        EXPECT(false, "%s: %s does not parse", label, map.c_str());
        return false;
    }
    s.histogram[gitest::sha256Hex(b)]++;
    s.items.push_back(g.numItems);
    return true;
}

static void report(const char *label, const Series &s, int runs) {
    printf("%s: %d runs, %d failed, %zu distinct map(s)\n", label, runs, s.crashes, s.histogram.size());
}

static int modeRepeat(bool reuse, int runs) {
    const std::string rib = g_work + "/identity-photon.rib";
    if (!substitute(g_tools + "/identity-photon.rib", rib, "1", false)) {
        fprintf(stderr, "cannot read %s/identity-photon.rib\n", g_tools.c_str());
        return 2;
    }
    const std::string dir = g_work + (reuse ? "/reuse" : "/fresh");
    makeDir(dir);
    Series s;
    for (int i = 0; i < runs; i++) {
        if (!reuse) {
            remove((dir + "/identity.gpm").c_str());
        }
        oneRun(rib, dir, "identity.gpm", "-t:1", s, reuse ? "repeat (reuse)" : "repeat (fresh)");
    }
    report(reuse ? "repeat (reuse)" : "repeat (fresh)", s, runs);
    EXPECT(s.histogram.size() == 1, "SC-002: %zu distinct maps in %d runs (one expected)", s.histogram.size(), runs);
    return g_fail == 0 ? 0 : 1;
}

static int modeThreads() {
    const std::string rib = g_work + "/identity-photon-free.rib";
    if (!substitute(g_tools + "/identity-photon.rib", rib, "1", true)) {
        fprintf(stderr, "cannot read %s/identity-photon.rib\n", g_tools.c_str());
        return 2;
    }
    const char *configs[5] = {"-t:1", "-t:2", "-t:4", "-t:8", ""};
    Series s;
    for (const char *c : configs) {
        const std::string dir = g_work + "/t" + (c[0] ? std::string(c + 3) : std::string("default"));
        makeDir(dir);
        remove((dir + "/identity.gpm").c_str());
        oneRun(rib, dir, "identity.gpm", c, s, c[0] ? c : "default threads");
    }
    report("threads", s, 5);
    EXPECT(s.histogram.size() == 1, "FR-001: %zu distinct maps across -t:1, -t:2, -t:4, -t:8 and the default (one expected)", s.histogram.size());
    return g_fail == 0 ? 0 : 1;
}

static int modeThreads4(int runs) {
    const std::string rib = g_work + "/perf-photon-pass.rib";
    if (!substitute(g_tools + "/perf-photon-pass.rib", rib, "4", false)) {
        fprintf(stderr, "cannot read %s/perf-photon-pass.rib\n", g_tools.c_str());
        return 2;
    }
    makeDir(g_work + "/p4");
    Series s;
    for (int i = 0; i < runs; i++) {
        remove((g_work + "/p4/perf-photon-pass.gpm").c_str());
        oneRun(rib, g_work + "/p4", "perf-photon-pass.gpm", "-t:4", s, "threads4");
    }
    report("threads4", s, runs);
    EXPECT(s.crashes == 0, "FR-017: %d of %d runs failed", s.crashes, runs);
    EXPECT(s.histogram.size() == 1, "SC-002: %zu distinct maps in %d runs at -t:4 (one expected)", s.histogram.size(), runs);
    for (int n : s.items) {
        EXPECT(n == 46273, "a map holds %d photons (46273 expected)", n);
    }
    return g_fail == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    // test_gi_photon_runs repeat <fresh|reuse> <runs> <orender> <tools> <work> | threads <orender> <tools> <work> | threads4 <runs> <orender> <tools> <work>
    if (argc < 5) {
        fprintf(stderr, "usage: %s repeat <fresh|reuse> <runs> | threads | threads4 <runs>  <orender> <tools dir> <work dir>\n", argv[0]);
        return 2;
    }
    const std::string mode = argv[1];
    int rest = 2, runs = 0;
    bool reuse = false;
    if (mode == "repeat") {
        reuse = std::string(argv[2]) == "reuse";
        runs = atoi(argv[3]);
        rest = 4;
    }
    else if (mode == "threads4") {
        runs = atoi(argv[2]);
        rest = 3;
    }
    if (argc != rest + 3) {
        fprintf(stderr, "wrong argument count\n");
        return 2;
    }
    g_orender = absolute(argv[rest]);
    g_tools = absolute(argv[rest + 1]);
    g_work = absolute(argv[rest + 2]);
    makeDir(g_work);
    if (mode == "repeat")
        return modeRepeat(reuse, runs);
    if (mode == "threads")
        return modeThreads();
    if (mode == "threads4")
        return modeThreads4(runs);
    fprintf(stderr, "unknown mode %s\n", mode.c_str());
    return 2;
}
