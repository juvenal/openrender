#include "tessProc.h"
#include "tessUtils.h"
#include <cstdio>

void tessProc(const float *bmin, const float *bmax,
              std::vector<float3> &verts, AABB &bounds) {
    float x0 = bmin[0], y0 = bmin[1], z0 = bmin[2];
    float x1 = bmax[0], y1 = bmax[1], z1 = bmax[2];
    float3 c[8] = {
        {x0,y0,z0}, {x1,y0,z0}, {x0,y1,z0}, {x1,y1,z0},
        {x0,y0,z1}, {x1,y0,z1}, {x0,y1,z1}, {x1,y1,z1}
    };
    fprintf(stderr, "orender-wire: warning: procedural primitive represented as bounding box\n");
    // 12 edges of the bounding box
    pushEdge(verts, bounds, c[0], c[1]); pushEdge(verts, bounds, c[1], c[3]);
    pushEdge(verts, bounds, c[3], c[2]); pushEdge(verts, bounds, c[2], c[0]);
    pushEdge(verts, bounds, c[4], c[5]); pushEdge(verts, bounds, c[5], c[7]);
    pushEdge(verts, bounds, c[7], c[6]); pushEdge(verts, bounds, c[6], c[4]);
    pushEdge(verts, bounds, c[0], c[4]); pushEdge(verts, bounds, c[1], c[5]);
    pushEdge(verts, bounds, c[2], c[6]); pushEdge(verts, bounds, c[3], c[7]);
}
