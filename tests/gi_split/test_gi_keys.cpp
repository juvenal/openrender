/**
 * tests/gi_split/test_gi_keys.cpp
 *
 * Spec 032, T-E (US5, FR-017 to FR-019, contracts/gi-keys.md "Properties the key test checks"; OQ-3 and OQ-8): the
 * deterministic keys the stores carry for spec 040. Three CTest names from one source; red until Phase 7 (the keys) and,
 * for the photons, Phase 5 (ri/render/photonMap.h).
 *
 *   GiKeys_Photons            (label gi_split)           the (key, photon) set of the photon-pass map is identical across two
 *                                                         -t:1 runs, and keys are unique within the map
 *   GiKeys_IrradianceRecords  (label gi_split)           the (key, record) set of the irradiance cache is identical across two
 *                                                         -t:1 runs, and keys are unique (so the records two runs share have
 *                                                         equal keys: OQ-3)
 *   GiKeys_PhotonsThreads     (label gi_split_threads)   as GiKeys_Photons but -t:1 against -t:4: opt-in, not in the everyday
 *                                                         run (the intermittent CPhotonHider crash and the unguarded
 *                                                         `modifying` reset make it unfit for it: OQ-3)
 *
 * One in-process render per process (GitHub #2: a second RiBegin/RiEnd cycle corrupts the heap): the parent process runs
 * itself twice as a child (`photons-dump` / `records-dump`), each child renders once and writes a text dump, and the parent
 * compares the dumps. A photon is compared by P, N, flags, C, theta and phi, never by its raw bytes (they include padding:
 * OQ-8); between two thread counts `flags` is left out (it is the kd-tree splitting axis balance() assigns, a function of the
 * order the photons were stored in, which differs between one thread and four, and not part of a photon's identity). A record
 * is compared by every field.
 *
 * Where the keys are read from:
 *   photons  the child keeps the map alive across the frame with attach() and reads CPhotonMap::photonKey(i) afterwards
 *   records  the cache is created and freed by the renderer inside the frame, so the child installs a services wrapper
 *            (CRecordingServices, below, a CContractForwarder) over the renderer's own for the length of the frame. Its
 *            getCache() builds the cache itself, as a CIrradianceCache subclass whose insertRecord() copies every (key, record)
 *            before forwarding, and owns it; the renderer never sees it. This is the test's only seam: nothing in src/ is changed
 *            for it, but CIrradianceCache and insertRecord() must stay non-final (recorded in the plan and contracts/irradiance-cache.md).
 *
 * Usage:
 *   test_gi_keys photons|records|photons-threads <work dir>        the parent (the test)
 *   test_gi_keys photons-dump|records-dump <threads> <out file>    one render and its dump (the parent's child)
 * The environment is the visual tests' (SHADERS, ORENDERHOME, DISPLAYS), set by CTest.
 */

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <vector>

#include <unistd.h>

#include "ri.h"
#include "ri/render/photonMap.h"
#include "ri/render/renderer.h"
#include "ri/texture/irradianceCache.h"
#include "libshader/shading/shading.h"
#include "shading_contract/contract_forwarder.h" // the forwarding services of spec 026's tests (needs M7: it is `final` today)

static int g_fail = 0;
#define EXPECT(cond, ...)                                        \
    do {                                                         \
        if (!(cond)) {                                           \
            ++g_fail;                                            \
            fprintf(stderr, "FAIL %s:%d  ", __FILE__, __LINE__); \
            fprintf(stderr, __VA_ARGS__);                        \
            fprintf(stderr, "\n");                               \
        }                                                        \
    } while (0)

// ---------------------------------------------------------------------------------------------------------
// text helpers
// ---------------------------------------------------------------------------------------------------------
static void putFloats(std::ostringstream &o, const float *v, int n) {
    for (int i = 0; i < n; i++) {
        char b[40];
        snprintf(b, sizeof(b), " %a", (double)v[i]);
        o << b;
    }
}

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

// ---------------------------------------------------------------------------------------------------------
// the photon child
// ---------------------------------------------------------------------------------------------------------
static RtPoint floorQuad[4] = {{-3, 0, -3}, {-3, 0, 3}, {3, 0, 3}, {3, 0, -3}};

static int photonsDump(int threads, const std::string &out) {
    const std::string mapName = out + ".gpm";
    const char *map = mapName.c_str();
    RtInt numThreads = threads;
    RtInt emit = 4000;
    RtFloat intensity = 4;
    RtPoint from = {0, 2.2f, 0.5f};
    remove(map);

    RiBegin(RI_NULL);
    RiOption(RI_LIMITS, RI_NUMTHREADS, &numThreads, RI_NULL);
    RiFrameBegin(1);
    RiHider("photon", RI_EMIT, &emit, RI_NULL);
    RiFormat(16, 16, 1);
    RiTranslate(0, -1, 5);
    RiWorldBegin();
    RiLightSource("pointlight", RI_INTENSITY, &intensity, RI_FROM, from, RI_NULL);
    RiAttribute(RI_PHOTON, RI_GLOBALMAP, &map, RI_NULL);
    RiPolygon(4, RI_P, (RtPointer)floorQuad, RI_NULL);
    RiTransformBegin();
    RiTranslate(0, 0.6f, 0.5f);
    RiSphere(0.6f, -0.6f, 0.6f, 360, RI_NULL);
    RiTransformEnd();
    // keep the map alive across the end of the frame: the renderer releases its own reference there
    CPhotonMap *photons = CRenderer::getPhotonMap(map);
    photons->attach();
    RiWorldEnd();
    RiFrameEnd();

    std::vector<std::string> lines;
    for (int i = 1; i <= photons->numItems; i++) {
        const CPhoton &p = photons->items[i];
        std::ostringstream o;
        char key[24];
        snprintf(key, sizeof(key), "%016llx", (unsigned long long)photons->photonKey(i));
        o << key;
        putFloats(o, p.P, 3);
        putFloats(o, p.N, 3);
        o << " " << (int)p.flags;
        putFloats(o, p.C, 3);
        o << " " << (int)p.theta << " " << (int)p.phi;
        lines.push_back(o.str());
    }
    photons->detach();
    RiEnd();
    remove(map);
    printf("photons-dump -t:%d: %zu photons\n", threads, lines.size());
    if (lines.empty()) {
        fprintf(stderr, "the photon pass stored no photon\n");
        return 1;
    }
    return writeLines(out, lines) ? 0 : 1;
}

// ---------------------------------------------------------------------------------------------------------
// the record child
// ---------------------------------------------------------------------------------------------------------
static std::vector<std::string> g_recordLines;

// An irradiance cache that copies every record handed to insertRecord() with its key, then inserts it as usual
class CRecordingCache final : public CIrradianceCache {
    public:
        using CIrradianceCache::CIrradianceCache;
        void insertRecord(const CIrradianceRecord &r, float maxError) override {
            std::ostringstream o;
            char key[24];
            snprintf(key, sizeof(key), "%016llx", (unsigned long long)r.key);
            o << key;
            putFloats(o, r.P, 3);
            putFloats(o, r.N, 3);
            putFloats(o, r.irradiance, 3);
            putFloats(o, &r.coverage, 1);
            putFloats(o, r.envdir, 3);
            putFloats(o, r.gP, 21);
            putFloats(o, r.gR, 21);
            putFloats(o, &r.dP, 1);
            {
                std::lock_guard<std::mutex> g(linesMutex);
                g_recordLines.push_back(o.str());
            }
            CIrradianceCache::insertRecord(r, maxError);
        }

    private:
        static std::mutex linesMutex;
};
std::mutex CRecordingCache::linesMutex;

// The renderer's services with getCache() replaced: tests/shading_contract/contract_forwarder.h forwards every other member, so
// this class lists none of them and cannot drift from the interface. Needs M7 (tools/test-edit-patches/M7-contract_forwarder.diff):
// CContractForwarder is `final` on the starting tree; M7 removes that one word, which changes nothing else.
class CRecordingServices final : public CContractForwarder {
    public:
        explicit CRecordingServices(CRendererServices *i) : CContractForwarder(i) {}
        ~CRecordingServices() override { delete cache; }

        CRecordingCache *cache = nullptr;

        CTexture3d *getCache(const char *handle, const char *mode, const float *from, const float *to) override {
            if (cache == nullptr) {
                (void)mode;
                const int flags = CACHE_SAMPLE; // the lookup samples on a miss, as mode "w" and the default do
                // no file (no CACHE_WRITE): the records are what the test reads
                cache = new CRecordingCache(handle != nullptr ? handle : "", flags, nullptr, from, to, CRenderer::toNDC, CRenderer::worldBmin,
                                            CRenderer::worldBmax);
            }
            return cache;
        }
};

static int recordsDump(int threads, const std::string &out) {
    RtInt numThreads = threads;
    RtInt one = 1;
    RtFloat maxError = 0.6f;
    const char *handle = "gi-keys-records.icache";
    const char *mode = "w";
    const char *format = "rslo";

    RiBegin(RI_NULL);
    RiOption(RI_LIMITS, RI_NUMTHREADS, &numThreads, RI_NULL);
    RiOption("shaderformat", "default", &format, RI_NULL);
    // the renderer registered its services at RiBegin; a substitute installed now survives RiWorldBegin (CLAUDE.md gotcha 23)
    CRendererServices *original = CShadingContext::defaultServices();
    CRecordingServices recording(original);
    CShadingContext::setDefaultServices(&recording);

    RiHider("reyes", RI_NULL);
    RiDisplay("gi-keys-records.tif", "file", RI_RGBA, RI_NULL);
    RiFormat(24, 18, 1);
    RiPixelSamples(1, 1);
    RiProjection("perspective", RI_NULL);
    RiClipping(0.1f, 100);
    RiTranslate(0, 0, 5);
    RiWorldBegin();
    RiAttribute(RI_IRRADIANCE, RI_HANDLE, &handle, RI_FILEMODE, &mode, RI_MAXERROR, &maxError, RI_NULL);
    RiSurface("occlusion_probe", RI_NULL);
    RiSphere(1, -1, 1, 360, RI_NULL);
    RiAttributeBegin();
    RiAttribute("visibility", "int diffuse", &one, RI_NULL);
    RiSurface("matte", RI_NULL);
    RiTranslate(1.2f, 0, -0.5f);
    RiSphere(0.9f, -0.9f, 0.9f, 360, RI_NULL);
    RiAttributeEnd();
    RiWorldEnd();

    CShadingContext::setDefaultServices(original);
    RiEnd();
    printf("records-dump -t:%d: %zu records\n", threads, g_recordLines.size());
    if (g_recordLines.empty()) {
        fprintf(stderr, "the render inserted no record into the recording cache\n");
        return 1;
    }
    return writeLines(out, g_recordLines) ? 0 : 1;
}

// ---------------------------------------------------------------------------------------------------------
// the parent
// ---------------------------------------------------------------------------------------------------------
static bool runChild(const std::string &self, const char *what, int threads, const std::string &out) {
    const std::string cmd = "\"" + self + "\" " + what + " " + std::to_string(threads) + " \"" + out + "\"";
    printf("Running: %s\n", cmd.c_str());
    fflush(stdout);
    return system(cmd.c_str()) == 0;
}

// the keys (first token of each line) are unique
static void checkUnique(const char *label, const std::vector<std::string> &lines) {
    std::set<std::string> keys;
    for (const std::string &l : lines) {
        keys.insert(l.substr(0, l.find(' ')));
    }
    printf("%s: %zu entries, %zu distinct keys\n", label, lines.size(), keys.size());
    EXPECT(keys.size() == lines.size(), "%s: keys are not unique (%zu entries, %zu keys)", label, lines.size(), keys.size());
    EXPECT(keys.find(std::string(16, '0')) == keys.end(), "%s: an entry has the key 0 (no key was set)", label);
}

// The 8th field of a photon line (key, P x3, N x3, flags, ...) is the kd-tree's splitting axis: dropped when the thread counts differ
static void dropFlags(std::vector<std::string> &lines) {
    for (std::string &l : lines) {
        std::istringstream in(l);
        std::string field, out;
        for (int i = 1; in >> field; i++) {
            if (i != 8) {
                out += (out.empty() ? "" : " ") + field;
            }
        }
        l = out;
    }
}

static int compare(const std::string &self, const char *dumpCase, int threadsA, int threadsB, const std::string &work) {
    const std::string a = work + "/keys-a.txt", b = work + "/keys-b.txt";
    remove(a.c_str());
    remove(b.c_str());
    if (!runChild(self, dumpCase, threadsA, a) || !runChild(self, dumpCase, threadsB, b)) {
        fprintf(stderr, "FAIL: a child render failed\n");
        return 1;
    }
    std::vector<std::string> la, lb;
    EXPECT(readLines(a, la) && readLines(b, lb), "a dump is missing");
    checkUnique("run A", la);
    checkUnique("run B", lb);
    if (threadsA != threadsB && std::string(dumpCase) == "photons-dump") {
        dropFlags(la);
        dropFlags(lb);
    }
    EXPECT(la == lb, "the (key, entry) sets differ between -t:%d and -t:%d (%zu against %zu entries)", threadsA, threadsB, la.size(), lb.size());
    if (la == lb) {
        printf("the two sets are identical\n");
    }
    return g_fail == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc != 4) {
        fprintf(stderr, "usage: %s photons|records|photons-threads <work dir>\n       %s photons-dump|records-dump <threads> <out>\n", argv[0], argv[0]);
        return 2;
    }
    const std::string which = argv[1];
    if (which == "photons-dump") {
        return photonsDump(atoi(argv[2]), argv[3]);
    }
    if (which == "records-dump") {
        return recordsDump(atoi(argv[2]), argv[3]);
    }
    std::string self = argv[0];
    char cwd[4096];
    if (!self.empty() && self[0] != '/' && getcwd(cwd, sizeof(cwd)) != NULL) {
        self = std::string(cwd) + "/" + self;
    }
    if (chdir(argv[2]) != 0) {
        fprintf(stderr, "cannot enter %s\n", argv[2]);
        return 2;
    }
    if (which == "photons") {
        return compare(self, "photons-dump", 1, 1, argv[2]);
    }
    if (which == "photons-threads") {
        return compare(self, "photons-dump", 1, 4, argv[2]);
    }
    if (which == "records") {
        return compare(self, "records-dump", 1, 1, argv[2]);
    }
    fprintf(stderr, "unknown case %s\n", which.c_str());
    return 2;
}
