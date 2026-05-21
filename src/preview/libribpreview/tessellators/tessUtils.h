#pragma once
#include "previewTypes.h"
#include <cmath>
#include <vector>

// Column-major matrix transform: result = M * (x,y,z,1)
static inline float3 xfPoint(const float *m, float x, float y, float z) {
    float ox = m[0]*x + m[4]*y + m[ 8]*z + m[12];
    float oy = m[1]*x + m[5]*y + m[ 9]*z + m[13];
    float oz = m[2]*x + m[6]*y + m[10]*z + m[14];
    float ow = m[3]*x + m[7]*y + m[11]*z + m[15];
    if (ow != 1.0f && ow != 0.0f) { float inv = 1.0f/ow; ox*=inv; oy*=inv; oz*=inv; }
    return {ox, oy, oz};
}

static inline void expandAABB(AABB &bb, const float3 &p) {
    if (p.x < bb.min.x) bb.min.x = p.x;
    if (p.y < bb.min.y) bb.min.y = p.y;
    if (p.z < bb.min.z) bb.min.z = p.z;
    if (p.x > bb.max.x) bb.max.x = p.x;
    if (p.y > bb.max.y) bb.max.y = p.y;
    if (p.z > bb.max.z) bb.max.z = p.z;
}

static inline void pushEdge(std::vector<float3> &verts, std::vector<float3> &cols,
                             AABB &bb,
                             const float3 &a, const float3 &b, const float3 &col) {
    verts.push_back(a);  verts.push_back(b);
    cols.push_back(col); cols.push_back(col);
    expandAABB(bb, a);
    expandAABB(bb, b);
}
