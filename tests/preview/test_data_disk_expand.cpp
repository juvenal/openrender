#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

#include "diskExpand.h"

static int g_pass = 0, g_fail = 0;
#define CHECK(expr) do { \
    if (expr) { ++g_pass; } \
    else { ++g_fail; fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); } \
} while(0)

static const float EPS = 1e-4f;

static float dist(const float3 &a, const float3 &b) {
    float dx = a.x - b.x, dy = a.y - b.y, dz = a.z - b.z;
    return std::sqrt(dx * dx + dy * dy + dz * dz);
}

static float3 sub(const float3 &a, const float3 &b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
static float3 crossf(const float3 &a, const float3 &b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
static float lenf(const float3 &a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }

static bool allFinite(const std::vector<float3> &v) {
    for (const float3 &p : v)
        if (!std::isfinite(p.x) || !std::isfinite(p.y) || !std::isfinite(p.z))
            return false;
    return true;
}

// Runs the standard checks (vertex count, rim distance, normal parallel to N, no NaN) for one
// disk and returns pass/fail via CHECK.
static void checkDisk(const DiskPrimitive &d, const char *label) {
    std::vector<float3> verts, cols;
    expandDisk(d, verts, cols);

    CHECK(verts.size() == 60);
    CHECK(cols.size() == 60);
    CHECK(allFinite(verts));

    float3 N = d.N;
    float nLen = lenf(N);
    bool haveN = nLen > 1e-6f;
    float3 Nn = haveN ? float3{N.x / nLen, N.y / nLen, N.z / nLen} : float3{0, 0, 1};

    for (size_t t = 0; t < verts.size() / 3; t++) {
        const float3 &v0 = verts[t * 3 + 0];
        const float3 &v1 = verts[t * 3 + 1];
        const float3 &v2 = verts[t * 3 + 2];

        // v0 is the disk center for every triangle in the fan.
        CHECK(dist(v0, d.P) < EPS);

        // Rim vertices sit exactly `radius` from the center.
        CHECK(std::fabs(dist(v1, d.P) - d.radius) < 1e-3f);
        CHECK(std::fabs(dist(v2, d.P) - d.radius) < 1e-3f);

        // The triangle's plane normal is parallel (or anti-parallel) to N.
        float3 e1 = sub(v1, v0), e2 = sub(v2, v0);
        float3 tn = crossf(e1, e2);
        float tnLen = lenf(tn);
        if (haveN && tnLen > 1e-9f) {
            float3 tnN = {tn.x / tnLen, tn.y / tnLen, tn.z / tnLen};
            float d3 = std::fabs(tnN.x * Nn.x + tnN.y * Nn.y + tnN.z * Nn.z);
            CHECK(d3 > 0.999f);
        }
    }

    if (g_fail > 0)
        fprintf(stderr, "  (failures above are from disk case: %s)\n", label);
}

int main() {
    // Ordinary disk, away from the origin.
    checkDisk(DiskPrimitive{ {5, 2, -3}, {0, 0, 1}, 1.5f, {1, 0, 0} }, "ordinary");

    // P at the exact origin -- the deleted pre-016 pglDisks basis (X = P x N) is NaN here since
    // P x N == 0 x N == 0. This implementation's basis is derived from N alone, so it must not
    // reproduce that bug.
    checkDisk(DiskPrimitive{ {0, 0, 0}, {0, 1, 0}, 2.0f, {0, 1, 0} }, "P at origin");

    // P parallel to N (same direction) -- also NaN in the deleted basis (P x N == 0 whenever P
    // is parallel to N, regardless of magnitude).
    checkDisk(DiskPrimitive{ {3, 0, 0}, {1, 0, 0}, 0.5f, {0, 0, 1} }, "P parallel to N");

    // N along a non-axis-aligned direction, to exercise the axis-picking fallback branch.
    checkDisk(DiskPrimitive{ {1, 1, 1}, {0, 1, 0}, 1.0f, {1, 1, 1} }, "N along +Y (ref-axis switch)");

    printf("test_data_disk_expand: %d pass, %d fail\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
