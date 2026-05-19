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

// De Boor evaluation: evaluate NURBS basis at parameter t given knots[nk], order k
// Returns the value of the i-th B-spline basis function using Cox-de Boor recursion
static float deBoorBasis(const float *knots, int i, int k, float t) {
    if (k == 1) {
        return (knots[i] <= t && t < knots[i+1]) ? 1.0f : 0.0f;
    }
    float c1 = 0, c2 = 0;
    float d1 = knots[i+k-1] - knots[i];
    float d2 = knots[i+k] - knots[i+1];
    if (d1 > 1e-8f) c1 = (t - knots[i]) / d1 * deBoorBasis(knots, i, k-1, t);
    if (d2 > 1e-8f) c2 = (knots[i+k] - t) / d2 * deBoorBasis(knots, i+1, k-1, t);
    return c1 + c2;
}

// Simple NURBS patch De Boor evaluation on an NxN grid
// nu, nv = control point count in u, v
// uorder, vorder = spline order
static std::vector<float3> tessNURBS(
    int nu, int nv, int uorder, int vorder,
    const float *uknots, const float *vknots,
    float umin, float umax, float vmin, float vmax,
    const float3 *ctrl)
{
    const int N = 12;
    auto evalU = [&](float u, float v) -> float3 {
        float3 p{0,0,0};
        for (int j=0;j<nv;++j) {
            float Bv = deBoorBasis(vknots, j, vorder, v);
            if (Bv < 1e-8f) continue;
            for (int i=0;i<nu;++i) {
                float Bu = deBoorBasis(uknots, i, uorder, u);
                if (Bu < 1e-8f) continue;
                const float3 &c = ctrl[j*nu+i];
                p.x += Bu*Bv*c.x;
                p.y += Bu*Bv*c.y;
                p.z += Bu*Bv*c.z;
            }
        }
        return p;
    };

    // Clamp t so it doesn't reach the last closed knot (open B-spline convention)
    float ueps = (umax-umin)*1e-5f, veps = (vmax-vmin)*1e-5f;

    std::vector<float3> edges;
    // Horizontal: N rows × N columns = N×N edge pairs
    for (int jj=0;jj<N;++jj) {
        float v = vmin + (float)jj/N*(vmax-vmin);
        for (int ii=0;ii<N;++ii) {
            float u0 = umin+(float)ii/N*(umax-umin);
            float u1 = umin+(float)(ii+1)/N*(umax-umin)-(ii+1==N?ueps:0);
            edges.push_back(evalU(u0,v));
            edges.push_back(evalU(u1,v));
        }
    }
    // Vertical: N columns × N rows = N×N edge pairs
    for (int ii=0;ii<N;++ii) {
        float u = umin+(float)ii/N*(umax-umin);
        for (int jj=0;jj<N;++jj) {
            float v0 = vmin+(float)jj/N*(vmax-vmin);
            float v1 = vmin+(float)(jj+1)/N*(vmax-vmin)-(jj+1==N?veps:0);
            edges.push_back(evalU(u,v0));
            edges.push_back(evalU(u,v1));
        }
    }
    return edges;
}

int main() {
    // Bicubic NURBS on a 4×4 grid (order 4) — open uniform knots
    // Defines a flat patch in [0,1]×[0,1], z varies from 0 to 1
    const int nu=4, nv=4, uord=4, vord=4;
    float uknots[8] = {0,0,0,0,1,1,1,1};
    float vknots[8] = {0,0,0,0,1,1,1,1};
    float3 ctrl[16];
    for (int j=0;j<4;++j) for(int i=0;i<4;++i) {
        ctrl[j*4+i] = {(float)i/3, (float)j/3, (float)(i*j)/9};
    }
    auto edges = tessNURBS(nu,nv,uord,vord,uknots,vknots,0,1,0,1,ctrl);

    // 12×12 horizontal + 12×12 vertical = 288 segments = 576 vertices
    CHECK(edges.size() == 576u);

    bool hasNaN = false;
    for (auto &v : edges) if (std::isnan(v.x)||std::isnan(v.y)||std::isnan(v.z)) { hasNaN=true; break; }
    CHECK(!hasNaN);

    // All vertices should be in [0,1] range in x and y
    bool inBounds = true;
    for (auto &v : edges) {
        if (v.x < -0.01f || v.x > 1.01f || v.y < -0.01f || v.y > 1.01f) { inBounds=false; break; }
    }
    CHECK(inBounds);

    printf("test_nurbs: %d pass, %d fail\n", g_pass, g_fail);
    return g_fail > 0 ? 1 : 0;
}
