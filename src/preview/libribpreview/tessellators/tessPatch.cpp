#include "tessPatch.h"
#include "tessUtils.h"

static void tessGrid(const float *positions, int nu, int nv,
                     const float *xfrom,
                     std::vector<float3> &verts, AABB &bounds) {
    auto pt = [&](int u, int v) -> float3 {
        int idx = (v * nu + u) * 3;
        return xfPoint(xfrom, positions[idx], positions[idx+1], positions[idx+2]);
    };
    for (int v = 0; v < nv; v++)
        for (int u = 0; u < nu - 1; u++)
            pushEdge(verts, bounds, pt(u, v), pt(u+1, v));
    for (int u = 0; u < nu; u++)
        for (int v = 0; v < nv - 1; v++)
            pushEdge(verts, bounds, pt(u, v), pt(u, v+1));
}

void tessPatch(const float *positions, int nu, int nv,
               const float *xfrom,
               std::vector<float3> &verts, AABB &bounds) {
    tessGrid(positions, nu, nv, xfrom, verts, bounds);
}

void tessNurbs(const float *positions, int nu, int nv,
               const float *xfrom,
               std::vector<float3> &verts, AABB &bounds) {
    tessGrid(positions, nu, nv, xfrom, verts, bounds);
}
