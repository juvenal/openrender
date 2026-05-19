#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

// Minimal stubs so we can test without linking a full renderer context.
// The real tests will use libribpreview after implementation.
// For now these tests verify the INTERFACE expected of tessPolygon.

// --- Lightweight test harness ---
static int g_pass = 0, g_fail = 0;
#define CHECK(expr) do { \
    if (expr) { ++g_pass; } \
    else { ++g_fail; fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); } \
} while(0)

// --- Geometric helper used by tessellation layer ---
struct float3 { float x, y, z; };

// Simulate a 4×4 row-major matrix–vector multiply (position transform)
static float3 transformPoint(const float m[16], float3 p) {
    return {
        m[0]*p.x + m[1]*p.y + m[2]*p.z + m[3],
        m[4]*p.x + m[5]*p.y + m[6]*p.z + m[7],
        m[8]*p.x + m[9]*p.y + m[10]*p.z + m[11]
    };
}

// --- Tessellation logic under test (pure algorithm, no ri dependency) ---
// This mirrors what tessPolygon.cpp will implement:
//   Given nvertices[] (vertex counts per face) and vertices[] (indices into P[])
//   emit one line segment per consecutive vertex pair, closing each face.
static std::vector<float3> tessPoly(
    const float3 *P, int npoly,
    const int *nvertices, const int *indices,
    const float xform[16])
{
    std::vector<float3> edges;
    int base = 0;
    for (int f = 0; f < npoly; ++f) {
        int n = nvertices[f];
        for (int i = 0; i < n; ++i) {
            int i0 = indices[base + i];
            int i1 = indices[base + (i+1) % n];
            edges.push_back(transformPoint(xform, P[i0]));
            edges.push_back(transformPoint(xform, P[i1]));
        }
        base += n;
    }
    return edges;
}

static const float identity16[16] = {
    1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1
};
static const float translate2x[16] = {
    1,0,0,2, 0,1,0,0, 0,0,1,0, 0,0,0,1
};

int main() {
    // Triangle: 3 vertices → 3 edges (6 floats)
    float3 P_tri[3] = {{0,0,0},{1,0,0},{0,1,0}};
    int nv_tri[1] = {3};
    int idx_tri[3] = {0,1,2};
    auto edges_tri = tessPoly(P_tri, 1, nv_tri, idx_tri, identity16);
    CHECK(edges_tri.size() == 6u);

    // Quad: 4 vertices → 4 edges (8 floats)
    float3 P_quad[4] = {{0,0,0},{1,0,0},{1,1,0},{0,1,0}};
    int nv_quad[1] = {4};
    int idx_quad[4] = {0,1,2,3};
    auto edges_quad = tessPoly(P_quad, 1, nv_quad, idx_quad, identity16);
    CHECK(edges_quad.size() == 8u);

    // Transform: translate x+2 applied to triangle vertex[0]
    auto edges_xf = tessPoly(P_tri, 1, nv_tri, idx_tri, translate2x);
    float3 v0 = edges_xf[0];
    CHECK(std::fabs(v0.x - 2.0f) < 1e-5f);
    CHECK(std::fabs(v0.y) < 1e-5f);

    // World-space: identity produces same coords as input
    CHECK(std::fabs(edges_tri[0].x - P_tri[0].x) < 1e-5f);
    CHECK(std::fabs(edges_tri[1].x - P_tri[1].x) < 1e-5f);

    // Two-polygon mesh: triangle + quad → 3+4 = 7 edges
    float3 P2[5] = {{0,0,0},{1,0,0},{0.5f,1,0},{2,0,0},{3,0,0}};
    int nv2[2] = {3,3};
    int idx2[6] = {0,1,2,2,3,4};
    auto edges2 = tessPoly(P2, 2, nv2, idx2, identity16);
    CHECK(edges2.size() == 12u); // 3+3 edges × 2 endpoints

    printf("test_polygon: %d pass, %d fail\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
