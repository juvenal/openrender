#include <cassert>
#include <cstdio>

#include "ri/dataviewer/dataView.h"
#include "ri/hiders/photonMap.h"

static int g_pass = 0, g_fail = 0;
#define CHECK(expr) do { \
    if (expr) { ++g_pass; } \
    else { ++g_fail; fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); } \
} while(0)

// Records the largest vertex.x seen across every points()/disks() call. Photons below are
// stored with P.x = i+1 (i = 0..count-1), so the dummy CPhotonMap::balance() seeds at P=(0,0,0)
// never wins the max, and the max is exactly `count` iff every stored photon was drawn —
// pinning the off-by-one in CPhotonMap::draw() (T018) without needing to know its internal
// numItems (which includes that dummy entry).
class CMaxXSink : public CPrimitiveSink {
    public:
        float maxPx = -1.0f;

        void triangles(int, const float *, const float *) override {}
        void triangleMesh(int, const int *, const float *, const float *) override {}
        void lines(int, const float *, const float *) override {}

        void points(int n, const float *P, const float *) override {
            for (int i = 0; i < n; i++)
                if (P[i * 3] > maxPx) maxPx = P[i * 3];
        }

        void disks(int n, const float *P, const float *, const float *, const float *) override {
            for (int i = 0; i < n; i++)
                if (P[i * 3] > maxPx) maxPx = P[i * 3];
        }
};

static void runCase(int count) {
    CPhotonMap *map = new CPhotonMap("test_dataview_chunking_scratch", NULL);

    for (int i = 0; i < count; i++) {
        float P[3] = { (float)(i + 1), 0, 0 };
        float N[3] = { 0, 0, 1 };
        float I[3] = { 0, 0, 1 };
        float C[3] = { 1, 1, 1 };
        map->store(P, N, I, C);
    }

    CMaxXSink sink;
    CDataView::install(&sink);
    map->draw();
    CDataView::install(NULL);

    CHECK(sink.maxPx == (float)count);

    delete map;
}

int main() {
    // Tail-flush boundaries: 0, 1, chunkSize-1, chunkSize, chunkSize+1
    runCase(0);
    runCase(1);
    runCase(CDataView::chunkSize - 1);
    runCase(CDataView::chunkSize);
    runCase(CDataView::chunkSize + 1);

    printf("test_dataview_chunking: %d pass, %d fail\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
