#include "tessProc.h"
#include "tessUtils.h"
#include <cstdio>

void tessProc(const float *bmin, const float *bmax, const float *xfrom,
              const float3 &col,
              std::vector<float3> &verts, std::vector<float3> &cols, AABB &bounds) {
    float x0 = bmin[0], y0 = bmin[1], z0 = bmin[2];
    float x1 = bmax[0], y1 = bmax[1], z1 = bmax[2];
    float3 c[8] = {
        xfPoint(xfrom, x0,y0,z0), xfPoint(xfrom, x1,y0,z0),
        xfPoint(xfrom, x0,y1,z0), xfPoint(xfrom, x1,y1,z0),
        xfPoint(xfrom, x0,y0,z1), xfPoint(xfrom, x1,y0,z1),
        xfPoint(xfrom, x0,y1,z1), xfPoint(xfrom, x1,y1,z1)
    };
    fprintf(stderr, "orender-wire: warning: procedural primitive represented as bounding box\n");
    pushEdge(verts, cols, bounds, c[0], c[1], col); pushEdge(verts, cols, bounds, c[1], c[3], col);
    pushEdge(verts, cols, bounds, c[3], c[2], col); pushEdge(verts, cols, bounds, c[2], c[0], col);
    pushEdge(verts, cols, bounds, c[4], c[5], col); pushEdge(verts, cols, bounds, c[5], c[7], col);
    pushEdge(verts, cols, bounds, c[7], c[6], col); pushEdge(verts, cols, bounds, c[6], c[4], col);
    pushEdge(verts, cols, bounds, c[0], c[4], col); pushEdge(verts, cols, bounds, c[1], c[5], col);
    pushEdge(verts, cols, bounds, c[2], c[6], col); pushEdge(verts, cols, bounds, c[3], c[7], col);
}
