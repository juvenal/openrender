/**
 * tests/preview/test_data_scene_unchanged.cpp
 *
 * Spec 032, T-B Preview_DataSceneUnchanged (US3, FR-014, SC-004): what the data viewer shows for each of the
 * five document types does not change when the data structures stop deriving from CDataView.
 *
 * For each of the five document types (a photon map, an irradiance cache, a point cloud, a brick map and a
 * debug dump) it records, before and after each key of a key sequence:
 *   * through the ribdata_* C API (unchanged on both trees): documentType, the channel count and names, the
 *     current channel, the detail level, the draw mode, the bounds, the framing camera, and for every primitive
 *     array (lines, points, triangles) the count and a hash of the positions and colours, the disk counts;
 *   * through CDataDocument and a recording CPrimitiveSink: typeName(), bound(), the channel accessors and the
 *     draw() stream itself, batch by batch (kind, count, hash), which exposes the chunking the C API merges.
 * The result is compared with tests/preview/reference/<platform>/datascene.txt.
 *
 * The fixtures are generated at run time. The photon map is written by an orender photon pass (the small scene
 * of T-F, -t:1), not by constructing a CPhotonMap, so this test includes no photon-map header; the irradiance
 * cache is fixture F1's file; the point cloud and the brick map are built as test_data_keys.cpp builds them;
 * the debug dump as test_debugdump_roundtrip.cpp does.
 *
 * Usage:
 *   test_data_scene_unchanged <orender> <rib dir> <F1 cache> <work dir> (--record <file> | <reference file>)
 */

#include <cassert>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>
#include <vector>

#include "common/algebra.h"
#include "ri/dataviewer/dataLoad.h"
#include "ri/dataviewer/dataView.h"
#include "ri/dataviewer/debug.h"
#include "ri/parse/riInterface.h"
#include "ri/texture/brickmap.h"
#include "ri/texture/pointCloud.h"
#include "ribpreview_api.h"

// ---------------------------------------------------------------------------------------------------------
static std::string g_out;

__attribute__((format(printf, 1, 2))) static void put(const char *fmt, ...) {
    char buf[1024];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    g_out += buf;
    g_out += "\n";
}

// FNV-1a, 64 bit, over the raw bytes of float data
static uint64_t hashFloats(const float *p, size_t count, uint64_t h = 1469598103934665603ull) {
    const unsigned char *b = (const unsigned char *)p;
    for (size_t i = 0; i < count * sizeof(float); i++) {
        h ^= b[i];
        h *= 1099511628211ull;
    }
    return h;
}

static uint64_t hashInts(const int *p, size_t count, uint64_t h = 1469598103934665603ull) {
    const unsigned char *b = (const unsigned char *)p;
    for (size_t i = 0; i < count * sizeof(int); i++) {
        h ^= b[i];
        h *= 1099511628211ull;
    }
    return h;
}

// ---------------------------------------------------------------------------------------------------------
// The ribdata_* C API snapshot
// ---------------------------------------------------------------------------------------------------------
static void putArray(const char *name, const PrimArrayC &a) {
    put("  %s count=%d hashVerts=%016llx hashCols=%016llx", name, a.count,
        (unsigned long long)hashFloats(a.verts, (size_t)a.count * 3), (unsigned long long)hashFloats(a.cols, (size_t)a.count * 3));
}

static void putSnapshot(RibDataDocument *doc) {
    const DataSceneC *s = ribdata_snapshot(doc);
    put("  documentType=%d numChannels=%d currentChannel=%d detailLevel=%d drawMode=%d", (int)s->documentType, s->numChannels,
        s->currentChannel, s->detailLevel, s->drawMode);
    for (int i = 0; i < s->numChannels; i++) {
        const char *n = ribdata_channel_name(doc, i);
        put("  channel %d \"%s\"", i, n ? n : "(null)");
    }
    put("  bounds min (%a %a %a) max (%a %a %a)", (double)s->bounds.sceneBoundsMin[0], (double)s->bounds.sceneBoundsMin[1],
        (double)s->bounds.sceneBoundsMin[2], (double)s->bounds.sceneBoundsMax[0], (double)s->bounds.sceneBoundsMax[1],
        (double)s->bounds.sceneBoundsMax[2]);
    put("  camera projection=%d fov=%a near=%a far=%a aspect=%a", s->camera.projectionType, (double)s->camera.fov,
        (double)s->camera.nearPlane, (double)s->camera.farPlane, (double)s->camera.frameAspectRatio);
    putArray("lines", s->lines);
    putArray("points", s->points);
    putArray("triangles", s->triangles);
    put("  sourceDiskCount=%d decimatedCount=%d", s->sourceDiskCount, s->decimatedCount);
}

// ---------------------------------------------------------------------------------------------------------
// The CDataView draw stream: a sink that records each batch
// ---------------------------------------------------------------------------------------------------------
class CRecordingSink : public CPrimitiveSink {
    public:
        void triangles(int n, const float *P, const float *C) override {
            put("    triangles n=%d hashP=%016llx hashC=%016llx", n, (unsigned long long)hashFloats(P, (size_t)n * 3),
                (unsigned long long)hashFloats(C, (size_t)n * 3));
        }
        void triangleMesh(int n, const int *indices, const float *P, const float *C) override {
            int maxIndex = -1;
            for (int i = 0; i < n * 3; i++) {
                if (indices[i] > maxIndex) {
                    maxIndex = indices[i];
                }
            }
            put("    triangleMesh n=%d hashIdx=%016llx vertices=%d hashP=%016llx hashC=%016llx", n,
                (unsigned long long)hashInts(indices, (size_t)n * 3), maxIndex + 1,
                (unsigned long long)hashFloats(P, (size_t)(maxIndex + 1) * 3), (unsigned long long)hashFloats(C, (size_t)(maxIndex + 1) * 3));
        }
        void lines(int n, const float *P, const float *C) override {
            put("    lines n=%d hashP=%016llx hashC=%016llx", n, (unsigned long long)hashFloats(P, (size_t)n * 3),
                (unsigned long long)hashFloats(C, (size_t)n * 3));
        }
        void points(int n, const float *P, const float *C) override {
            put("    points n=%d hashP=%016llx hashC=%016llx", n, (unsigned long long)hashFloats(P, (size_t)n * 3),
                (unsigned long long)hashFloats(C, (size_t)n * 3));
        }
        void disks(int n, const float *P, const float *dP, const float *N, const float *C) override {
            put("    disks n=%d hashP=%016llx hashdP=%016llx hashN=%016llx hashC=%016llx", n, (unsigned long long)hashFloats(P, (size_t)n * 3),
                (unsigned long long)hashFloats(dP, (size_t)n), (unsigned long long)hashFloats(N, (size_t)n * 3),
                (unsigned long long)hashFloats(C, (size_t)n * 3));
        }
};

static void putView(CDataView *view) {
    float bmin[3], bmax[3];
    view->bound(bmin, bmax);
    put("  typeName=\"%s\" numChannels=%d currentChannel=%d detailLevel=%d drawMode=%d", view->typeName(), view->numChannels(),
        view->currentChannel(), view->detailLevel(), view->drawMode());
    for (int i = 0; i < view->numChannels(); i++) {
        const char *n = view->channelName(i);
        put("  channel %d \"%s\"", i, n ? n : "(null)");
    }
    put("  bound min (%a %a %a) max (%a %a %a)", (double)bmin[0], (double)bmin[1], (double)bmin[2], (double)bmax[0], (double)bmax[1],
        (double)bmax[2]);
    CRecordingSink sink;
    CDataView::install(&sink);
    put("  draw (chunkSize %d):", CDataView::chunkSize);
    view->draw();
    CDataView::install(NULL);
}

// ---------------------------------------------------------------------------------------------------------
// One document: the C API sequence, then the CDataView sequence. One document is open at a time (FR-019).
// ---------------------------------------------------------------------------------------------------------
static bool recordDocument(const char *label, const std::string &path, const char *keys, int expectedType) {
    put("[%s]", label);

    int err = 0;
    RibDataDocument *doc = ribdata_open(path.c_str(), &err);
    if (doc == NULL) {
        fprintf(stderr, "FAIL: ribdata_open(%s) failed (%d)\n", path.c_str(), err);
        return false;
    }
    put("ribdata_sniff %d", ribdata_sniff(path.c_str()));
    put("-- initial");
    putSnapshot(doc);
    for (const char *k = keys; *k; k++) {
        const int changed = ribdata_key(doc, (int)(unsigned char)*k);
        put("-- key '%c' changed=%d", *k, changed != 0);
        putSnapshot(doc);
    }
    ribdata_close(doc);

    CDataDocument document;
    const EDataFileType type = document.open(path.c_str());
    if (document.view() == NULL || (int)type != expectedType) {
        fprintf(stderr, "FAIL: CDataDocument::open(%s) answered %d, expected %d\n", path.c_str(), (int)type, expectedType);
        return false;
    }
    put("CDataDocument type %d", (int)type);
    put("-- initial");
    putView(document.view());
    for (const char *k = keys; *k; k++) {
        const int changed = document.view()->keyDown((int)(unsigned char)*k);
        put("-- key '%c' changed=%d", *k, changed != 0);
        putView(document.view());
    }
    return true;
}

// ---------------------------------------------------------------------------------------------------------
// Fixtures
// ---------------------------------------------------------------------------------------------------------
static void writePointCloudFixture(const char *path) {
    matrix from, to;
    identitym(from);
    identitym(to);
    char *names[2] = {(char *)"_radiosity", (char *)"_extra"};
    char *types[2] = {(char *)"float", (char *)"float"};
    CPointCloud *cloud = new CPointCloud(path, from, to, NULL, 2, names, types, TRUE);
    for (int i = 0; i < 8; i++) {
        float P[3] = {(float)i, 0, 0};
        float N[3] = {0, 0, 1};
        float C[2] = {0.5f, 0.25f};
        cloud->store(C, P, N, 0.1f);
    }
    delete cloud;
}

static void writeBrickMapFixture(const char *ptcPath, const char *brkPath) {
    CRiInterface *testInterface = new CRiInterface();
    riInstallContext(testInterface);
    const char *src[1] = {ptcPath};
    makeBrickMap(1, src, brkPath, NULL, 0, NULL, NULL);
    delete testInterface; // uninstalls it
}

static void writeDebugDumpFixture(const char *path) {
    CDebugView writer(path);
    float p0[3] = {1, 0, 0};
    writer.point(p0);
    float l0[3] = {2, 0, 0}, l1[3] = {3, 0, 0};
    writer.line(l0, l1);
    float t0[3] = {4, 0, 0}, t1[3] = {5, 0, 0}, t2[3] = {6, 0, 0};
    writer.triangle(t0, t1, t2);
    float q0[3] = {7, 0, 0}, q1[3] = {8, 0, 0}, q2[3] = {9, 0, 0}, q3[3] = {10, 0, 0};
    writer.quad(q0, q1, q2, q3);
}

static bool readFile(const std::string &path, std::string &out) {
    FILE *f = fopen(path.c_str(), "rb");
    if (f == NULL) {
        return false;
    }
    char buf[65536];
    size_t n;
    out.clear();
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        out.append(buf, n);
    }
    fclose(f);
    return true;
}

int main(int argc, char **argv) {
    if (argc != 7 && argc != 6) {
        fprintf(stderr, "usage: %s <orender> <rib dir> <F1 cache> <work dir> (--record <file> | <reference file>)\n", argv[0]);
        return 2;
    }
    const std::string orender = argv[1];
    std::string ribDir = argv[2], f1 = argv[3], work = argv[4];
    const bool record = std::string(argv[5]) == "--record";
    if ((record && argc != 7) || (!record && argc != 6)) {
        fprintf(stderr, "usage: %s <orender> <rib dir> <F1 cache> <work dir> (--record <file> | <reference file>)\n", argv[0]);
        return 2;
    }
    char cwd[4096];
    auto absolute = [&](const std::string &p) { return (!p.empty() && p[0] != '/' && getcwd(cwd, sizeof(cwd))) ? std::string(cwd) + "/" + p : p; };
    ribDir = absolute(ribDir);
    f1 = absolute(f1);
    const std::string target = absolute(argv[6 - (record ? 0 : 1)]);
    if (chdir(absolute(work).c_str()) != 0) {
        fprintf(stderr, "cannot enter %s\n", work.c_str());
        return 2;
    }

    // the photon map: an orender photon pass at -t:1 (frame 1 of photon-parity.rib writes photon-parity.gpm)
    remove("photon-parity.gpm");
    const std::string cmd = "\"" + orender + "\" -t:1 \"" + ribDir + "/photon-parity.rib\"";
    printf("Running: %s\n", cmd.c_str());
    fflush(stdout);
    if (system(cmd.c_str()) != 0) {
        fprintf(stderr, "FAIL: orender failed on photon-parity.rib\n");
        return 1;
    }

    writePointCloudFixture("gi_scene_scratch.ptc");
    writeBrickMapFixture("gi_scene_scratch.ptc", "gi_scene_scratch.brk");
    writeDebugDumpFixture("gi_scene_scratch.dbg");

    put("# spec 032 T-B Preview_DataSceneUnchanged: the data viewer's scene for the five document types, before and after each key");
    bool ok = true;
    // the key sequences of test_data_keys.cpp for the two it covers; the cache's d and p; keys a type ignores
    ok = recordDocument("photonmap", "photon-parity.gpm", "pdwq", (int)DATA_PHOTONMAP) && ok;
    ok = recordDocument("irradiancecache", f1, "dpd", (int)DATA_IRRADIANCECACHE) && ok;
    ok = recordDocument("pointcloud", "gi_scene_scratch.ptc", "pdwq", (int)DATA_POINTCLOUD) && ok;
    ok = recordDocument("brickmap", "gi_scene_scratch.brk", "mlbdpwqmmll", (int)DATA_BRICKMAP) && ok;
    ok = recordDocument("debugdump", "gi_scene_scratch.dbg", "p", (int)DATA_DEBUGDUMP) && ok;

    remove("gi_scene_scratch.ptc");
    remove("gi_scene_scratch.brk");
    remove("gi_scene_scratch.dbg");
    if (!ok) {
        return 1;
    }

    if (record) {
        FILE *f = fopen(target.c_str(), "wb");
        if (f == NULL || fwrite(g_out.data(), 1, g_out.size(), f) != g_out.size()) {
            fprintf(stderr, "cannot write %s\n", target.c_str());
            return 1;
        }
        fclose(f);
        printf("recorded %s (%zu bytes)\n", target.c_str(), g_out.size());
        return 0;
    }

    std::string ref;
    if (!readFile(target, ref)) {
        fprintf(stderr, "FAIL: no reference %s\n", target.c_str());
        return 1;
    }
    if (ref == g_out) {
        printf("Preview_DataSceneUnchanged: the five documents' scenes equal the reference\n");
        return 0;
    }
    size_t a = 0, b = 0;
    int line = 1;
    while (a < ref.size() && b < g_out.size()) {
        const size_t ea = ref.find('\n', a), eb = g_out.find('\n', b);
        const std::string la = ref.substr(a, ea == std::string::npos ? std::string::npos : ea - a);
        const std::string lb = g_out.substr(b, eb == std::string::npos ? std::string::npos : eb - b);
        if (la != lb) {
            fprintf(stderr, "FAIL: line %d differs\n  reference: %s\n  now:       %s\n", line, la.c_str(), lb.c_str());
            return 1;
        }
        if (ea == std::string::npos || eb == std::string::npos) {
            break;
        }
        a = ea + 1;
        b = eb + 1;
        line++;
    }
    fprintf(stderr, "FAIL: the descriptions differ in length (%zu against %zu bytes)\n", ref.size(), g_out.size());
    return 1;
}
