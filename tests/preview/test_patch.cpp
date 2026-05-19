#include <cassert>
#include <cmath>
#include <cstdio>
#include <vector>

static int g_pass = 0, g_fail = 0;
#define CHECK(expr) do { \
    if (expr) { ++g_pass; } \
    else { ++g_fail; fprintf(stderr, "FAIL %s:%d  %s\n", __FILE__, __LINE__, #expr); } \
} while(0)

struct float3 { float x, y, z; };

// Bilinear patch: 4 control points at corners. Boundary = 4 edges.
static std::vector<float3> tessBilinear(const float3 ctrl[4]) {
    // Corner positions: (0,0), (1,0), (1,1), (0,1) in param space
    // ctrl layout: [0]=(0,0) [1]=(1,0) [2]=(0,1) [3]=(1,1)
    // Bilinear interp: P(u,v) = (1-u)(1-v)*C0 + u(1-v)*C1 + (1-u)v*C2 + u*v*C3
    auto bilerp = [&](float u, float v) -> float3 {
        float w00=(1-u)*(1-v), w10=u*(1-v), w01=(1-u)*v, w11=u*v;
        return {
            w00*ctrl[0].x + w10*ctrl[1].x + w01*ctrl[2].x + w11*ctrl[3].x,
            w00*ctrl[0].y + w10*ctrl[1].y + w01*ctrl[2].y + w11*ctrl[3].y,
            w00*ctrl[0].z + w10*ctrl[1].z + w01*ctrl[2].z + w11*ctrl[3].z,
        };
    };
    std::vector<float3> edges;
    // 4 boundary edges
    float3 corners[4] = { bilerp(0,0), bilerp(1,0), bilerp(1,1), bilerp(0,1) };
    for (int i = 0; i < 4; ++i) {
        edges.push_back(corners[i]);
        edges.push_back(corners[(i+1)%4]);
    }
    return edges;
}

// Bicubic patch: 8×8 grid → 9 horizontal strips + 9 vertical strips of 8 segments each
// = 9*8 + 8*9 = 72+72 = 144 line segments = 288 vertices
static std::vector<float3> tessBicubic(const float ctrl[16][3]) {
    // Bernstein basis B_{i,3}(t)
    auto B = [](int i, float t) -> float {
        float s = 1-t;
        switch(i) {
            case 0: return s*s*s;
            case 1: return 3*t*s*s;
            case 2: return 3*t*t*s;
            case 3: return t*t*t;
        }
        return 0;
    };
    auto eval = [&](float u, float v) -> float3 {
        float3 p{0,0,0};
        for (int j=0;j<4;++j) for(int i=0;i<4;++i) {
            float w = B(i,u)*B(j,v);
            p.x += w*ctrl[j*4+i][0];
            p.y += w*ctrl[j*4+i][1];
            p.z += w*ctrl[j*4+i][2];
        }
        return p;
    };
    const int N = 8;
    std::vector<float3> edges;
    // Horizontal lines (constant v rows, varying u)
    for (int jj = 0; jj <= N; ++jj) {
        float v = (float)jj/N;
        for (int ii = 0; ii < N; ++ii) {
            float u0=(float)ii/N, u1=(float)(ii+1)/N;
            edges.push_back(eval(u0,v));
            edges.push_back(eval(u1,v));
        }
    }
    // Vertical lines (constant u columns, varying v)
    for (int ii = 0; ii <= N; ++ii) {
        float u = (float)ii/N;
        for (int jj = 0; jj < N; ++jj) {
            float v0=(float)jj/N, v1=(float)(jj+1)/N;
            edges.push_back(eval(u,v0));
            edges.push_back(eval(u,v1));
        }
    }
    return edges;
}

int main() {
    // --- Bilinear patch: unit square ---
    float3 biCtrl[4] = {{0,0,0},{1,0,0},{0,1,0},{1,1,0}};
    auto biEdges = tessBilinear(biCtrl);
    // Exactly 4 boundary edges = 8 vertices
    CHECK(biEdges.size() == 8u);

    // --- Bicubic patch: flat 4×4 control grid on z=0 plane ---
    float ctrl[16][3];
    for (int j=0;j<4;++j) for(int i=0;i<4;++i) {
        ctrl[j*4+i][0] = (float)i/3.0f;
        ctrl[j*4+i][1] = (float)j/3.0f;
        ctrl[j*4+i][2] = 0;
    }
    auto bicEdges = tessBicubic(ctrl);
    // 9 rows × 8 segments + 9 cols × 8 segments = 144 segments = 288 vertices
    CHECK(bicEdges.size() == 288u);

    // All bicubic vertices should have z ≈ 0
    bool allZ = true;
    for (auto &v : bicEdges) if (std::fabs(v.z) > 1e-5f) { allZ=false; break; }
    CHECK(allZ);

    printf("test_patch: %d pass, %d fail\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
