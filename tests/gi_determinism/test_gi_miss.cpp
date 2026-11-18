/**
 * tests/gi_determinism/test_gi_miss.cpp
 *
 * Spec 040, US4 and US2 (FR-011, FR-012, FR-013; SC-003; contracts/draw-rules.md): what a cache miss makes, and how often a point is shaded.
 * Three modes from one source; every mode is a parent that runs itself as children, one render per child process (GitHub #2: a second
 * RiBegin/RiEnd cycle in one process corrupts the heap).
 *
 *   miss <scene>       (GiDet_MissIndependence_{rslo,slo})  the raw record the sampler makes at the probe surface is bit-identical with 0, 10 and
 *                      1000 other records in the fixed part. The records are written to a cache file by the test and read with CACHE_READ; each is
 *                      in the root node, in the probe's plane, and faces the other way, so a lookup visits every one of them (it draws for each
 *                      on the starting tree) and accepts none. The child proves that precondition before it renders, so a test that passes is
 *                      never vacuous. FR-011: "the sampler's draws MUST NOT shift by the number of records the lookup visited or accepted".
 *   raycount <scene>   (GiDet_IndirectRayCount_{rslo,slo})  indirectdiffuse() with the cache off traces exactly `samples` (12) rays per point:
 *                      Total Traced equals 12 x the number of sampler calls and the indirect-diffuse ray counter equals Total Traced, at -t:1 and
 *                      -t:4, and the same on both. GitHub #173: on the starting tree it traces no ray at all, so this case is red for "0 rays"
 *                      first. The counts are read from orender's statistics output ("Total Traced", "Num Samples", "Num Rays"); research T011:
 *                      Total Traced counts every ray traceRay() sends, and it is the oracle, not the indirect-diffuse counter alone.
 *   repeat <scene>     (GiDet_RepeatedPoint_{rslo,slo})  a small displaced sphere with occlusion() and a cache written to a file: a point is
 *                      shaded more than once (the displacement estimate, the lazily shaded grid). At -t:1 and -t:4, under the reyes and raytrace hiders:
 *                      (a) every (key, record bits) pair the recording cache sees for one key has the same bits, whichever shading reached the cache
 *                      first; (b) the committed records hold no two with the same P and N; (c) the committed records and the image are
 *                      bit-identical across the four configurations (FR-013, US2 scenario 5).
 *
 * Usage: test_gi_miss <miss|raycount|repeat> <scene.rib> <orender> <work dir>      the parent (the test)
 *        test_gi_miss miss-dump <scene.rib> <fixed records> <out>                  one render of `miss`, in a child
 *        test_gi_miss repeat-dump <scene.rib> <threads> <hider> <out>              one render of `repeat`, in a child
 * The environment (SHADERS, ORENDERHOME, DISPLAYS) is the visual tests', set by CTest.
 * Red reasons on the starting tree: miss, an assertion (the sampler's cursor draws shift with the number of records visited); raycount, an
 * assertion (0 rays, GitHub #173); repeat, an assertion at -t:4 (records and their order follow the order of the shadings).
 */

#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <sstream>

#include "gi_recording.h"

using namespace gidet;

static std::string g_self;

// ---------------------------------------------------------------------------------------------------------
// children
// ---------------------------------------------------------------------------------------------------------
static bool writeLines(const std::string &path, std::vector<std::string> lines) {
    std::sort(lines.begin(), lines.end());
    std::ofstream f(path, std::ios::binary);
    for (const std::string &l : lines) {
        f << l << "\n";
    }
    return (bool)f;
}

static bool readLines(const std::string &path, std::vector<std::string> &lines) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    std::string l;
    while (std::getline(f, l)) {
        lines.push_back(l);
    }
    return true;
}

static int missDump(const std::string &scene, int fixed, const std::string &out) {
    const char *file = "gidet-miss-fixed.icache";
    if (!writeRejectedRecords(file, fixed)) {
        fprintf(stderr, "the fixed cache file was not written\n");
        return 1;
    }
    // the precondition: the lookup the probe makes visits every fixed record and accepts none
    bool hit = true;
    const int visited = visitedByLookup(file, hit);
    printf("miss-dump: %d fixed records, a lookup at the probe visits %d, hit %d\n", fixed, visited, hit ? 1 : 0);
    if (visited != fixed || hit) {
        fprintf(stderr, "FAIL: the precondition does not hold (visited %d of %d, hit %d): the case would prove nothing\n", visited, fixed, hit ? 1 : 0);
        return 1;
    }

    RtInt one = 1;
    RiBegin(RI_NULL);
    RiOption(RI_LIMITS, RI_NUMTHREADS, &one, RI_NULL);
    CRendererServices *original = CShadingContext::defaultServices();
    RecordingServices recording(original, CACHE_READ | CACHE_SAMPLE);
    CShadingContext::setDefaultServices(&recording);
    RiReadArchive((RtToken)scene.c_str(), NULL, NULL);
    CShadingContext::setDefaultServices(original);

    std::vector<std::string> lines;
    if (recording.cache != nullptr) {
        for (const SeenRecord &s : recording.cache->seen()) {
            lines.push_back(hexKey(s.key) + " " + s.bits);
        }
    }
    RiEnd();
    printf("miss-dump: %zu record(s) made\n", lines.size());
    if (lines.empty()) {
        fprintf(stderr, "FAIL: the render made no record: the probe never missed\n");
        return 1;
    }
    return writeLines(out, lines) ? 0 : 1;
}

static int repeatDump(const std::string &scene, int threads, const std::string &hider, const std::string &out) {
    // the scene is a template: @HIDER@
    Bytes in;
    if (!gitest::readFile(scene, in)) {
        fprintf(stderr, "cannot read %s\n", scene.c_str());
        return 2;
    }
    std::string text(in.begin(), in.end());
    for (size_t at; (at = text.find("@HIDER@")) != std::string::npos;) {
        text.replace(at, 7, hider);
    }
    const std::string local = "gidet-repeat-scene.rib";
    gitest::writeText(local, text);
    remove("repeat.tif");
    remove("gidet-repeat.icache");

    RtInt numThreads = threads;
    RiBegin(RI_NULL);
    RiOption(RI_LIMITS, RI_NUMTHREADS, &numThreads, RI_NULL);
    CRendererServices *original = CShadingContext::defaultServices();
    RecordingServices recording(original, CACHE_SAMPLE);
    CShadingContext::setDefaultServices(&recording);
    RiReadArchive((RtToken)local.c_str(), NULL, NULL);

    int bad = 0;
    std::vector<std::string> lines;
    if (recording.cache == nullptr) {
        fprintf(stderr, "FAIL: the render never asked for the cache\n");
        RiEnd();
        return 1;
    }
    // (a) every pair seen for one key carries the same bits
    std::map<uint64_t, std::string> byKey;
    size_t seenCount = 0, repeated = 0;
    for (const SeenRecord &s : recording.cache->seen()) {
        seenCount++;
        auto it = byKey.find(s.key);
        if (it == byKey.end()) {
            byKey[s.key] = s.bits;
        }
        else {
            repeated++;
            if (it->second != s.bits) {
                bad++;
                fprintf(stderr, "FAIL (a): key %s was seen with two different records\n", hexKey(s.key).c_str());
            }
        }
        lines.push_back("SEEN " + hexKey(s.key) + " " + s.bits);
    }
    // (b) the committed records hold no two with the same P and N; they are the set compared across configurations
    std::set<std::string> pn;
    size_t committed = 0;
    recording.cache->forEachRecord([&](const CIrradianceRecord &r) {
        committed++;
        const std::string k = bits(r.P, 3) + bits(r.N, 3);
        if (!pn.insert(k).second) {
            bad++;
            fprintf(stderr, "FAIL (b): two committed records share P and N\n");
        }
        lines.push_back("COMMITTED " + recordFileBits(r));
    });
    CShadingContext::setDefaultServices(original);
    RiEnd();
    Bytes img;
    if (gitest::readFile("repeat.tif", img)) {
        lines.push_back("IMAGE " + gitest::sha256Hex(img));
    }
    else {
        bad++;
        fprintf(stderr, "FAIL: no image was written\n");
    }
    printf("repeat-dump %s -t:%d: %zu records seen (%zu repeated keys), %zu committed\n", hider.c_str(), threads, seenCount, repeated, committed);
    if (seenCount == 0 || committed == 0) {
        fprintf(stderr, "FAIL: no record was made\n");
        return 1;
    }
    return (writeLines(out, lines) && bad == 0) ? 0 : 1;
}

// ---------------------------------------------------------------------------------------------------------
// the parent
// ---------------------------------------------------------------------------------------------------------
static bool child(const std::string &args) {
    const RunResult r = runCommand("\"" + g_self + "\" " + args);
    if (!r.ok()) {
        fprintf(stderr, "child [%s]: %s\n", args.c_str(), describe(r).c_str());
    }
    return r.ok();
}

static int parentMiss(const std::string &scene) {
    std::vector<std::vector<std::string>> sets;
    const int counts[3] = {0, 10, 1000};
    for (int c : counts) {
        const std::string out = "miss-" + std::to_string(c) + ".txt";
        remove(out.c_str());
        EXPECT(child("miss-dump \"" + scene + "\" " + std::to_string(c) + " " + out), "the render with %d fixed records failed", c);
        std::vector<std::string> l;
        EXPECT(readLines(out, l), "no dump for %d fixed records", c);
        sets.push_back(l);
    }
    if (g_fail == 0) {
        printf("miss: %zu record(s) made at the probe\n", sets[0].size());
        EXPECT(sets[0] == sets[1], "FR-011: the record the miss makes differs with 10 other records in the fixed part (%zu against %zu records)", sets[1].size(), sets[0].size());
        EXPECT(sets[0] == sets[2], "FR-011: the record the miss makes differs with 1000 other records in the fixed part (%zu against %zu records)", sets[2].size(), sets[0].size());
    }
    return g_fail == 0 ? 0 : 1;
}

static bool statLine(const std::string &text, const std::string &label, long &first, long &second) {
    size_t at = text.find(label);
    if (at == std::string::npos) {
        return false;
    }
    first = strtol(text.c_str() + at + label.size(), nullptr, 10);
    // "Num Samples: 26372 (indirectdiffuse), 0 (occlusion)": the second figure follows "), "
    const size_t eol = text.find('\n', at);
    const size_t comma = text.find("), ", at);
    second = (comma != std::string::npos && comma < eol) ? strtol(text.c_str() + comma + 3, nullptr, 10) : 0;
    return true;
}

struct RayCounts {
        long traced = -1, samples = -1, rays = -1;
};

static bool oneRayCount(const std::string &orender, const std::string &scene, const std::string &threads, RayCounts &out) {
    Bytes in;
    if (!gitest::readFile(scene, in)) {
        return false;
    }
    std::string text(in.begin(), in.end());
    const size_t eol = text.find('\n');
    text.insert(eol == std::string::npos ? 0 : eol + 1, "Option \"statistics\" \"endofframe\" [3]\n"); // as measure_gi.py's `stats` mode does
    const std::string rib = "raycount-" + threads + ".rib", log = "raycount-" + threads + ".log";
    gitest::writeText(rib, text);
    const RunResult r = runCommand("\"" + orender + "\" -x 3 -t:" + threads + " \"" + rib + "\" > \"" + log + "\" 2>&1");
    EXPECT(r.ok(), "the render at -t:%s failed (%s)", threads.c_str(), describe(r).c_str());
    Bytes b;
    gitest::readFile(log, b);
    const std::string o(b.begin(), b.end());
    long x, y;
    bool ok = statLine(o, "Total Traced:", x, y);
    out.traced = ok ? x : -1;
    ok = statLine(o, "Num Samples:", x, y) && ok;
    out.samples = x;
    ok = statLine(o, "Num Rays:", x, y) && ok;
    out.rays = x;
    EXPECT(ok, "the statistics of the render at -t:%s were not found in %s", threads.c_str(), log.c_str());
    return ok;
}

static int parentRayCount(const std::string &orender, const std::string &scene) {
    RayCounts a, b;
    oneRayCount(orender, scene, "1", a);
    oneRayCount(orender, scene, "4", b);
    if (g_fail != 0) {
        return 1;
    }
    printf("raycount: -t:1: %ld sampler calls, Total Traced %ld, indirectdiffuse rays %ld; -t:4: %ld, %ld, %ld\n", a.samples, a.traced, a.rays, b.samples, b.traced, b.rays);
    EXPECT(a.samples > 0, "no sampler call was made (cache off, so every shading point makes one)");
    const long samplesPerPoint = 12; // indirectdiffuse_probe's literal
    EXPECT(a.traced == samplesPerPoint * a.samples, "FR-012 / GitHub #173: Total Traced is %ld, %d x %ld expected (indirectdiffuse() traced %s)", a.traced, (int)samplesPerPoint, a.samples,
           a.traced == 0 ? "no ray at all" : "another number of rays");
    EXPECT(a.rays == a.traced, "the indirect-diffuse ray counter (%ld) differs from Total Traced (%ld)", a.rays, a.traced);
    EXPECT(a.samples == b.samples && a.traced == b.traced && a.rays == b.rays, "the counts differ between -t:1 and -t:4");
    return g_fail == 0 ? 0 : 1;
}

static int parentRepeat(const std::string &scene) {
    struct Cfg {
            const char *hider;
            int threads;
    };
    const Cfg cfgs[4] = {{"reyes", 1}, {"reyes", 4}, {"raytrace", 1}, {"raytrace", 4}};
    std::vector<std::vector<std::string>> sets;
    for (const Cfg &c : cfgs) {
        const std::string out = std::string("repeat-") + c.hider + "-" + std::to_string(c.threads) + ".txt";
        remove(out.c_str());
        const bool ok = child("repeat-dump \"" + scene + "\" " + std::to_string(c.threads) + " " + c.hider + " " + out);
        EXPECT(ok, "the %s render at -t:%d failed or broke (a) or (b)", c.hider, c.threads);
        std::vector<std::string> all, kept;
        readLines(out, all);
        for (const std::string &l : all) {
            if (l.rfind("COMMITTED", 0) == 0 || l.rfind("IMAGE", 0) == 0) {
                kept.push_back(l); // the SEEN lines may repeat and differ in count: only the committed set and the image are compared
            }
        }
        sets.push_back(kept);
    }
    if (g_fail == 0) {
        // the image of the two hiders is not the same image: compare each hider's two thread counts, and the committed set within each
        EXPECT(sets[0] == sets[1], "FR-013 / SC-001: reyes -t:1 and -t:4 differ (committed set or image)");
        EXPECT(sets[2] == sets[3], "FR-013 / SC-001: raytrace -t:1 and -t:4 differ (committed set or image)");
    }
    return g_fail == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc < 5) {
        fprintf(stderr, "usage: %s miss|raycount|repeat <scene.rib> <orender> <work dir>\n", argv[0]);
        return 2;
    }
    const std::string mode = argv[1];
    g_self = absolute(argv[0]);
    if (mode == "miss-dump" || mode == "repeat-dump") {
        // children run in the parent's work directory
        const std::string scene = absolute(argv[2]);
        if (mode == "miss-dump") {
            return missDump(scene, atoi(argv[3]), argv[4]);
        }
        if (argc < 6) {
            return 2;
        }
        return repeatDump(scene, atoi(argv[3]), argv[4], argv[5]);
    }
    const std::string scene = absolute(argv[2]), orender = absolute(argv[3]);
    if (chdir(argv[4]) != 0) {
        fprintf(stderr, "cannot enter %s\n", argv[4]);
        return 2;
    }
    if (mode == "miss")
        return parentMiss(scene);
    if (mode == "raycount")
        return parentRayCount(orender, scene);
    if (mode == "repeat")
        return parentRepeat(scene);
    fprintf(stderr, "unknown mode %s\n", mode.c_str());
    return 2;
}
