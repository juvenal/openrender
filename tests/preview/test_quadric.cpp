#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

static int g_pass = 0, g_fail = 0;
#define CHECK(expr) do { \
    if (expr) { ++g_pass; } \
    else { ++g_fail; fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); } \
} while(0)

static const float M_2PI = 6.283185307f;
static const float M_PI_F = 3.141592654f;

struct float3 { float x, y, z; };

// Sphere tessellation: 24 longitude × 12 latitude steps
static std::vector<float3> tessSphere(float r, float umax, float vmin, float vmax) {
    const int NU = 24, NV = 12;
    std::vector<float3> edges;
    auto P = [&](int iu, int iv) -> float3 {
        float u = (float)iu/NU * umax;
        float v = vmin + (float)iv/NV * (vmax - vmin);
        return { r*std::cos(u)*std::cos(v),
                 r*std::sin(u)*std::cos(v),
                 r*std::sin(v) };
    };
    // Horizontal rings
    for (int iv = 0; iv <= NV; ++iv)
        for (int iu = 0; iu < NU; ++iu) {
            edges.push_back(P(iu, iv));
            edges.push_back(P(iu+1, iv));
        }
    // Vertical meridians
    for (int iu = 0; iu <= NU; ++iu)
        for (int iv = 0; iv < NV; ++iv) {
            edges.push_back(P(iu, iv));
            edges.push_back(P(iu, iv+1));
        }
    return edges;
}

int main() {
    const float PI = M_PI_F;

    // Full sphere (r=1, full sweep)
    auto edges = tessSphere(1.0f, M_2PI, -PI/2, PI/2);

    // Vertex count: (24+1)*12*2 + 24*(12+1)*2 = 600 + 624 = 1224
    // rows: (NV+1) rings × NU segments = 13×24=312 segments = 624 vertices
    // cols: (NU+1) meridians × NV steps = 25×12=300 segments = 600 vertices
    // total = 1224 vertices
    CHECK(edges.size() == 1224u);

    // No NaN
    bool hasNaN = false;
    for (auto &v : edges) if (std::isnan(v.x)||std::isnan(v.y)||std::isnan(v.z)) { hasNaN=true; break; }
    CHECK(!hasNaN);

    // No degenerate (zero-length) edges
    int degenerateCount = 0;
    for (size_t i = 0; i < edges.size(); i+=2) {
        float dx = edges[i+1].x - edges[i].x;
        float dy = edges[i+1].y - edges[i].y;
        float dz = edges[i+1].z - edges[i].z;
        float len2 = dx*dx + dy*dy + dz*dz;
        // At poles consecutive meridians can collapse — allow very small edges
        // but count truly zero ones
        if (len2 < 1e-10f) ++degenerateCount;
    }
    // At the two poles all horizontal ring edges collapse; allow up to 10% degenerate.
    // (Full sphere: 2 poles × 24 ring segments = 48 degenerate out of 612 total ≈ 7.8%)
    int total_segments = (int)(edges.size()/2);
    CHECK((float)degenerateCount / total_segments < 0.10f);

    // All vertices within bounding sphere r=1
    bool inBounds = true;
    for (auto &v : edges) {
        float r2 = v.x*v.x + v.y*v.y + v.z*v.z;
        if (r2 > 1.01f) { inBounds = false; break; }
    }
    CHECK(inBounds);

    printf("test_quadric: %d pass, %d fail\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
