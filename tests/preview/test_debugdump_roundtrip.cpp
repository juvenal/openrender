#include <cassert>
#include <cstdio>

#include "ri/dataviewer/dataView.h"
#include "ri/dataviewer/debug.h"

static int g_pass = 0, g_fail = 0;
#define CHECK(expr)                                                         \
    do {                                                                    \
        if (expr) {                                                         \
            ++g_pass;                                                       \
        }                                                                   \
        else {                                                              \
            ++g_fail;                                                       \
            fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); \
        }                                                                   \
    } while (0)

class CCountingSink : public CPrimitiveSink {
    public:
        int pointVerts = 0, lineVerts = 0, triVerts = 0;
        int triangleCalls = 0;

        void triangles(int n, const float *, const float *) override {
            triVerts += n;
            triangleCalls++;
        }
        void triangleMesh(int, const int *, const float *, const float *) override {}
        void lines(int n, const float *, const float *) override { lineVerts += n; }
        void points(int n, const float *, const float *) override { pointVerts += n; }
        void disks(int, const float *, const float *, const float *, const float *) override {}
};

int main() {
    const char *fileName = "test_debugdump_roundtrip_scratch.dbg";

    // Write one record of each type, ending on a quad — the historical `while (!feof(file))`
    // bug re-emitted whatever the last record was, so ending on the quad makes a regression
    // show up as extra triangle vertices rather than being masked by an earlier record type.
    {
        CDebugView writer(fileName);
        float p0[3] = {1, 0, 0};
        writer.point(p0);

        float l0[3] = {2, 0, 0}, l1[3] = {3, 0, 0};
        writer.line(l0, l1);

        float t0[3] = {4, 0, 0}, t1[3] = {5, 0, 0}, t2[3] = {6, 0, 0};
        writer.triangle(t0, t1, t2);

        float q0[3] = {7, 0, 0}, q1[3] = {8, 0, 0}, q2[3] = {9, 0, 0}, q3[3] = {10, 0, 0};
        writer.quad(q0, q1, q2, q3);
    } // ~CDebugView() flushes the header and closes the file

    CCountingSink sink;
    CDataView::install(&sink);
    CDataView::drawFile(fileName);
    CDataView::install(NULL);

    CHECK(sink.pointVerts == 1);
    CHECK(sink.lineVerts == 2);
    // 1 real triangle (3 verts) + 1 quad split into 2 triangles (6 verts) = 9.
    // The `!feof` bug would re-process the quad tag once more at EOF, adding 6 more (15 total).
    CHECK(sink.triVerts == 9);

    remove(fileName);

    printf("test_debugdump_roundtrip: %d pass, %d fail\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
