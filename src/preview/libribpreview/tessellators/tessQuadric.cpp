#include "tessQuadric.h"
#include "tessUtils.h"
#include <cmath>
#include <algorithm>

static constexpr int NU = 24;
static constexpr int NV = 12;

void tessQuadricSphere(float r, float umax, float vmin, float vmax,
                       const float *m,
                       std::vector<float3> &verts, AABB &bounds) {
    auto pt = [&](int ui, int vi) -> float3 {
        float theta = umax * ui / NU;
        float z     = vmin + (vmax - vmin) * vi / NV;
        float rz    = sqrtf(std::max(0.0f, r*r - z*z));
        return xfPoint(m, rz*cosf(theta), rz*sinf(theta), z);
    };
    for (int v = 0; v <= NV; v++)
        for (int u = 0; u < NU; u++)
            pushEdge(verts, bounds, pt(u, v), pt(u+1, v));
    for (int u = 0; u <= NU; u++)
        for (int v = 0; v < NV; v++)
            pushEdge(verts, bounds, pt(u, v), pt(u, v+1));
}

void tessQuadricDisk(float r, float z, float umax,
                     const float *m,
                     std::vector<float3> &verts, AABB &bounds) {
    for (int u = 0; u < NU; u++) {
        float t0 = umax * u / NU, t1 = umax * (u+1) / NU;
        pushEdge(verts, bounds,
                 xfPoint(m, r*cosf(t0), r*sinf(t0), z),
                 xfPoint(m, r*cosf(t1), r*sinf(t1), z));
    }
    float3 ctr = xfPoint(m, 0, 0, z);
    pushEdge(verts, bounds, ctr, xfPoint(m, r, 0, z));
    pushEdge(verts, bounds, ctr, xfPoint(m, r*cosf(umax), r*sinf(umax), z));
}

void tessQuadricCone(float r, float height, float umax,
                     const float *m,
                     std::vector<float3> &verts, AABB &bounds) {
    float3 apex = xfPoint(m, 0, 0, height);
    for (int u = 0; u < NU; u++) {
        float t0 = umax * u / NU, t1 = umax * (u+1) / NU;
        float3 a = xfPoint(m, r*cosf(t0), r*sinf(t0), 0);
        float3 b = xfPoint(m, r*cosf(t1), r*sinf(t1), 0);
        pushEdge(verts, bounds, a, b);
        pushEdge(verts, bounds, a, apex);
    }
}

void tessQuadricCylinder(float r, float zmin, float zmax, float umax,
                         const float *m,
                         std::vector<float3> &verts, AABB &bounds) {
    for (int u = 0; u < NU; u++) {
        float t0 = umax * u / NU, t1 = umax * (u+1) / NU;
        float3 lo0 = xfPoint(m, r*cosf(t0), r*sinf(t0), zmin);
        float3 lo1 = xfPoint(m, r*cosf(t1), r*sinf(t1), zmin);
        float3 hi0 = xfPoint(m, r*cosf(t0), r*sinf(t0), zmax);
        float3 hi1 = xfPoint(m, r*cosf(t1), r*sinf(t1), zmax);
        pushEdge(verts, bounds, lo0, lo1);
        pushEdge(verts, bounds, hi0, hi1);
        pushEdge(verts, bounds, lo0, hi0);
    }
}

void tessQuadricParaboloid(float rmax, float zmin, float zmax, float umax,
                           const float *m,
                           std::vector<float3> &verts, AABB &bounds) {
    if (zmax <= 0.0f) return;
    auto pt = [&](int ui, int vi) -> float3 {
        float theta = umax * ui / NU;
        float z     = zmin + (zmax - zmin) * vi / NV;
        float rz    = rmax * sqrtf(std::max(0.0f, z / zmax));
        return xfPoint(m, rz*cosf(theta), rz*sinf(theta), z);
    };
    for (int v = 0; v <= NV; v++)
        for (int u = 0; u < NU; u++)
            pushEdge(verts, bounds, pt(u, v), pt(u+1, v));
    for (int u = 0; u <= NU; u++)
        for (int v = 0; v < NV; v++)
            pushEdge(verts, bounds, pt(u, v), pt(u, v+1));
}

void tessQuadricHyperboloid(const float p1[3], const float p2[3], float umax,
                            const float *m,
                            std::vector<float3> &verts, AABB &bounds) {
    auto pt = [&](int ui, int vi) -> float3 {
        float theta = umax * ui / NU;
        float t     = (float)vi / NV;
        float px = p1[0]*(1-t) + p2[0]*t;
        float py = p1[1]*(1-t) + p2[1]*t;
        float pz = p1[2]*(1-t) + p2[2]*t;
        float rx = px*cosf(theta) - py*sinf(theta);
        float ry = px*sinf(theta) + py*cosf(theta);
        return xfPoint(m, rx, ry, pz);
    };
    for (int v = 0; v <= NV; v++)
        for (int u = 0; u < NU; u++)
            pushEdge(verts, bounds, pt(u, v), pt(u+1, v));
    for (int u = 0; u <= NU; u++)
        for (int v = 0; v < NV; v++)
            pushEdge(verts, bounds, pt(u, v), pt(u, v+1));
}

void tessQuadricToroid(float rmax, float rmin, float phimin, float phimax, float umax,
                       const float *m,
                       std::vector<float3> &verts, AABB &bounds) {
    auto pt = [&](int ui, int vi) -> float3 {
        float theta = umax * ui / NU;
        float phi   = phimin + (phimax - phimin) * vi / NV;
        float rx = (rmax + rmin*cosf(phi))*cosf(theta);
        float ry = (rmax + rmin*cosf(phi))*sinf(theta);
        float rz = rmin*sinf(phi);
        return xfPoint(m, rx, ry, rz);
    };
    for (int v = 0; v <= NV; v++)
        for (int u = 0; u < NU; u++)
            pushEdge(verts, bounds, pt(u, v), pt(u+1, v));
    for (int u = 0; u <= NU; u++)
        for (int v = 0; v < NV; v++)
            pushEdge(verts, bounds, pt(u, v), pt(u, v+1));
}
