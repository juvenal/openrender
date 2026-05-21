#include "tessPatch.h"
#include "tessUtils.h"

static void tessGrid(const float *positions, int nu, int nv,
                     const float *xfrom, const float3 &col,
                     std::vector<float3> &verts, std::vector<float3> &cols, AABB &bounds) {
    auto pt = [&](int u, int v) -> float3 {
        int idx = (v * nu + u) * 3;
        return xfPoint(xfrom, positions[idx], positions[idx+1], positions[idx+2]);
    };
    for (int v = 0; v < nv; v++)
        for (int u = 0; u < nu - 1; u++)
            pushEdge(verts, cols, bounds, pt(u, v), pt(u+1, v), col);
    for (int u = 0; u < nu; u++)
        for (int v = 0; v < nv - 1; v++)
            pushEdge(verts, cols, bounds, pt(u, v), pt(u, v+1), col);
}

void tessPatch(const float *positions, int nu, int nv,
               const float *xfrom, const float3 &col,
               std::vector<float3> &verts, std::vector<float3> &cols, AABB &bounds) {
    tessGrid(positions, nu, nv, xfrom, col, verts, cols, bounds);
}

void tessNurbs(const float *positions, int nu, int nv,
               const float *xfrom, const float3 &col,
               std::vector<float3> &verts, std::vector<float3> &cols, AABB &bounds) {
    tessGrid(positions, nu, nv, xfrom, col, verts, cols, bounds);
}
