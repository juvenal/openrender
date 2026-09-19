#include <cassert>
#include <cstdio>
#include <cstring>

#include "common/algebra.h"
#include "ri/dataLoad.h"
#include "ri/texture/pointCloud.h"

static int g_pass = 0, g_fail = 0;
#define CHECK(expr) do { \
    if (expr) { ++g_pass; } \
    else { ++g_fail; fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); } \
} while(0)

static void writePointCloudFixture(const char *path, int count) {
    matrix from, to;
    identitym(from);
    identitym(to);

    char *names[1] = { (char *)"_radiosity" };
    char *types[1] = { (char *)"float" };

    CPointCloud *cloud = new CPointCloud(path, from, to, NULL, 1, names, types, TRUE);

    for (int i = 0; i < count; i++) {
        float P[3] = { (float)(i + 1), 0, 0 };
        float N[3] = { 0, 0, 1 };
        float C[1] = { 0.5f };
        cloud->store(C, P, N, 0.1f);
    }

    delete cloud; // flush == TRUE from the write-mode ctor, so this writes the file
}

// Round-trips a point cloud with `count` points through CDataDocument::open() and checks
// type/count/bounds/channels all match what was written.
static void runCase(int count) {
    char path[64];
    snprintf(path, sizeof(path), "test_data_pointcloud_scratch_%d.ptc", count);

    writePointCloudFixture(path, count);

    CDataDocument doc;
    EDataFileType type = doc.open(path);

    CHECK(type == DATA_POINTCLOUD);
    CHECK(doc.view() != NULL);

    CPointCloud *cloud = dynamic_cast<CPointCloud *>(doc.view());
    CHECK(cloud != NULL);
    if (cloud != NULL) {
        CHECK(strcmp(cloud->typeName(), "Point Cloud") == 0);
        CHECK(cloud->numChannels() == 1);
        CHECK(cloud->channelName(0) != NULL && strcmp(cloud->channelName(0), "_radiosity") == 0);

        float bmin[3], bmax[3];
        cloud->bound(bmin, bmax);
        if (count > 0) {
            CHECK(cloud->numItems == count);
            // Points were stored at x = 1..count, y = z = 0.
            CHECK(bmin[0] == 1.0f);
            CHECK(bmax[0] == (float)count);
        } else {
            // CPointCloud::balance() unconditionally inserts one dummy point at the origin
            // with zeroed data before writing an otherwise-empty map ("to avoid an if
            // statement during lookup", pointCloud.cpp) -- a zero-point round trip is valid
            // (FR-005) but never produces a literal zero-item document for this type; it
            // always reopens as exactly one degenerate point at the origin.
            CHECK(cloud->numItems == 1);
            CHECK(bmin[0] == 0.0f && bmin[1] == 0.0f && bmin[2] == 0.0f);
            CHECK(bmax[0] == 0.0f && bmax[1] == 0.0f && bmax[2] == 0.0f);
        }
    }

    remove(path);
}

int main() {
    runCase(0);
    runCase(1);
    runCase(5);

    printf("test_data_pointcloud: %d pass, %d fail\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
