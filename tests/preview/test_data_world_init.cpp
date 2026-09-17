#include <cassert>
#include <cstdio>
#include <cstring>

#include "common/algebra.h"
#include "ri/dataLoad.h"
#include "ri/photonMap.h"
#include "ri/renderer.h"

static int g_pass = 0, g_fail = 0;
#define CHECK(expr) do { \
    if (expr) { ++g_pass; } \
    else { ++g_fail; fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); } \
} while(0)

// A photon map's read constructor combines its own stored fromWorld/toWorld matrices with
// CRenderer::fromWorld/toWorld (photonMap.cpp:103-104) -- if those statics are left at their
// zero-initialized default instead of identity, the combined transform silently degenerates to
// the zero matrix. Writing identity as the file's own matrices means the correct combined
// result is exactly identity, bit for bit -- any deviation pins the landmine.
static void writePhotonMapFixture(const char *path) {
    // CPhotonMap::write() persists CRenderer::fromWorld/toWorld (photonMap.cpp:210-219) as the
    // file's own stored matrices -- seed them to identity here too, or the fixture itself
    // bakes in the zero-initialized default and the read-side assertions below would pass on
    // a zero * identity coincidence rather than on open()'s actual seeding.
    identitym(CRenderer::fromWorld);
    identitym(CRenderer::toWorld);

    CPhotonMap *map = new CPhotonMap(path, NULL);

    float P[3] = { 1, 2, 3 };
    float N[3] = { 0, 0, 1 };
    float I[3] = { 0, 0, 1 };
    float C[3] = { 1, 1, 1 };
    map->store(P, N, I, C);

    map->modifying = TRUE;
    map->write(NULL);
    delete map;
}

static bool isIdentity(const float *m) {
    for (int row = 0; row < 4; row++)
        for (int col = 0; col < 4; col++) {
            float expected = (row == col) ? 1.0f : 0.0f;
            if (m[row + 4 * col] != expected)
                return false;
        }
    return true;
}

int main() {
    const char *path = "test_data_world_init_scratch.ptm";
    writePhotonMapFixture(path);

    // Deliberately corrupt CRenderer's transform statics to a non-identity/degenerate state
    // after writing the fixture (whose own stored matrices are identity) but before open() --
    // this is what open() must overwrite. Without this, the file's own identity matrices
    // combined with an already-identity static would pass even if open() seeded nothing.
    initv(CRenderer::worldBmin, 0, 0, 0);
    initv(CRenderer::worldBmax, 0, 0, 0);
    memset(CRenderer::fromWorld, 0, sizeof(matrix));
    memset(CRenderer::toWorld, 0, sizeof(matrix));

    CDataDocument doc;
    EDataFileType type = doc.open(path);

    CHECK(type == DATA_PHOTONMAP);
    CHECK(doc.view() != NULL);

    CPhotonMap *pm = dynamic_cast<CPhotonMap *>(doc.view());
    CHECK(pm != NULL);
    if (pm != NULL) {
        CHECK(isIdentity(pm->to));
        CHECK(isIdentity(pm->from));
    }

    // CRenderer::worldBmin/worldBmax must be seeded to +-C_INFINITY before construction, not
    // left at their zero default (irradiance.cpp:101 reads them on the fresh-cache branch).
    CHECK(CRenderer::worldBmin[0] == C_INFINITY && CRenderer::worldBmin[1] == C_INFINITY &&
          CRenderer::worldBmin[2] == C_INFINITY);
    CHECK(CRenderer::worldBmax[0] == -C_INFINITY && CRenderer::worldBmax[1] == -C_INFINITY &&
          CRenderer::worldBmax[2] == -C_INFINITY);
    CHECK(isIdentity(CRenderer::fromWorld));
    CHECK(isIdentity(CRenderer::toWorld));

    float bmin[3], bmax[3];
    doc.view()->bound(bmin, bmax);
    for (int i = 0; i < 3; i++) {
        CHECK(bmin[i] > -C_INFINITY && bmin[i] < C_INFINITY);
        CHECK(bmax[i] > -C_INFINITY && bmax[i] < C_INFINITY);
    }

    remove(path);

    printf("test_data_world_init: %d pass, %d fail\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
