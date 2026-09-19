#include <cassert>
#include <cstdio>
#include <cstring>
#include <unistd.h>

#include "common/algebra.h"
#include "ri/texture/brickmap.h"
#include "ri/texture/pointCloud.h"
#include "ri/parse/riInterface.h"
#include "ribpreview_api.h"

static int g_pass = 0, g_fail = 0;
#define CHECK(expr) do { \
    if (expr) { ++g_pass; } \
    else { ++g_fail; fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); } \
} while(0)

static void writePointCloudFixture(const char *path) {
    matrix from, to;
    identitym(from);
    identitym(to);

    char *names[2] = { (char *)"_radiosity", (char *)"_extra" };
    char *types[2] = { (char *)"float", (char *)"float" };

    CPointCloud *cloud = new CPointCloud(path, from, to, NULL, 2, names, types, TRUE);
    for (int i = 0; i < 8; i++) {
        float P[3] = { (float)i, 0, 0 };
        float N[3] = { 0, 0, 1 };
        float C[2] = { 0.5f, 0.25f };
        cloud->store(C, P, N, 0.1f);
    }
    delete cloud;
}

// Converts a point cloud into a brick map via the existing makeBrickMap() pipeline --
// CBrickMap's own write constructor needs a CChannel array, a protected nested type only
// makeBrickMap() (a declared friend of CPointCloud) can construct.
static void writeBrickMapFixture(const char *ptcPath, const char *brkPath) {
    // makeBrickMap() (and CBrickMap construction generally) calls info()/error() for
    // legitimate diagnostics, not just failures; both unconditionally dereference the global
    // `renderMan` (normally set up by RiBegin()/RiBeginLite()'s caller). A plain CRiInterface
    // is a safe, silent sink (its default RiError() only forwards to a registered handler).
    CRiInterface *saved = renderMan;
    renderMan = new CRiInterface();

    const char *src[1] = { ptcPath };
    makeBrickMap(1, src, brkPath, NULL, 0, NULL, NULL);

    delete renderMan;
    renderMan = saved;
}

// Runs the key sequence with stdout redirected to a scratch file, and returns its byte count
// (must be 0 -- FR-016 requires interactive state to be UI-visible, never printed).
static long runKeysCapturingStdout(void (*run)()) {
    const char *capturePath = "test_data_keys_stdout_capture.txt";
    fflush(stdout);
    int savedFd = dup(STDOUT_FILENO);
    freopen(capturePath, "w", stdout);

    run();

    fflush(stdout);
    dup2(savedFd, STDOUT_FILENO);
    close(savedFd);
    clearerr(stdout);

    FILE *f = fopen(capturePath, "rb");
    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fclose(f);
    remove(capturePath);
    return size;
}

static RibDataDocument *g_pc = nullptr;
static RibDataDocument *g_bm = nullptr;

static void runPointCloudKeys() {
    const DataSceneC *snap = ribdata_snapshot(g_pc);
    CHECK(snap->numChannels == 2);

    CHECK(ribdata_key(g_pc, 'p') != 0);
    snap = ribdata_snapshot(g_pc);
    CHECK(snap->drawMode == 0); // points

    CHECK(ribdata_key(g_pc, 'd') != 0);
    snap = ribdata_snapshot(g_pc);
    CHECK(snap->drawMode == 1); // discs

    int ch0 = snap->currentChannel;
    CHECK(ribdata_key(g_pc, 'w') != 0);
    snap = ribdata_snapshot(g_pc);
    CHECK(snap->currentChannel == ch0 + 1);

    CHECK(ribdata_key(g_pc, 'q') != 0);
    snap = ribdata_snapshot(g_pc);
    CHECK(snap->currentChannel == ch0);
}

static void runBrickMapKeys() {
    const DataSceneC *snap = ribdata_snapshot(g_bm);
    int detail0 = snap->detailLevel;

    CHECK(ribdata_key(g_bm, 'm') != 0);
    snap = ribdata_snapshot(g_bm);
    CHECK(snap->detailLevel == detail0 + 1);

    CHECK(ribdata_key(g_bm, 'l') != 0);
    snap = ribdata_snapshot(g_bm);
    CHECK(snap->detailLevel == detail0);

    CHECK(ribdata_key(g_bm, 'b') != 0);
    snap = ribdata_snapshot(g_bm);
    CHECK(snap->drawMode == 0); // boxes

    CHECK(ribdata_key(g_bm, 'd') != 0);
    snap = ribdata_snapshot(g_bm);
    CHECK(snap->drawMode == 1); // discs

    CHECK(ribdata_key(g_bm, 'p') != 0);
    snap = ribdata_snapshot(g_bm);
    CHECK(snap->drawMode == 2); // points

    int ch0 = snap->currentChannel;
    CHECK(ribdata_key(g_bm, 'w') != 0);
    snap = ribdata_snapshot(g_bm);
    CHECK(snap->currentChannel == ch0 + 1);

    CHECK(ribdata_key(g_bm, 'q') != 0);
    snap = ribdata_snapshot(g_bm);
    CHECK(snap->currentChannel == ch0);
}

int main() {
    const char *ptcPath = "test_data_keys_scratch.ptc";
    const char *brkPath = "test_data_keys_scratch.brk";

    writePointCloudFixture(ptcPath);
    writeBrickMapFixture(ptcPath, brkPath);

    int err = 0;

    // CDataDocument manipulates global CRenderer state (declarations table, memory pool,
    // renderMan) -- exactly one may be open at a time (FR-019), so these run sequentially,
    // never with both g_pc and g_bm alive simultaneously.
    g_pc = ribdata_open(ptcPath, &err);
    CHECK(g_pc != NULL);
    if (g_pc != NULL) {
        long bytes = runKeysCapturingStdout(runPointCloudKeys);
        CHECK(bytes == 0);
    }
    ribdata_close(g_pc);

    g_bm = ribdata_open(brkPath, &err);
    CHECK(g_bm != NULL);
    if (g_bm != NULL) {
        long bytes = runKeysCapturingStdout(runBrickMapKeys);
        CHECK(bytes == 0);
    }
    ribdata_close(g_bm);
    remove(ptcPath);
    remove(brkPath);

    printf("test_data_keys: %d pass, %d fail\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
