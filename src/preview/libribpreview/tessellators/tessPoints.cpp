#include "tessPoints.h"
#include "tessUtils.h"
#include <cstdio>

static constexpr int   MAX_POINTS  = 100000;
static constexpr float CROSS_DELTA = 0.02f;

void tessPoints(const float *positions, int numPoints,
                const float *xfrom,
                std::vector<float3> &verts, AABB &bounds) {
    int stride = 1;
    if (numPoints > MAX_POINTS) {
        stride = numPoints / MAX_POINTS;
        fprintf(stderr,
                "orender-wire: warning: point cloud has %d points; displaying %d (subsampled)\n",
                numPoints, MAX_POINTS);
    }

    for (int i = 0; i < numPoints; i += stride) {
        float x = positions[i*3], y = positions[i*3+1], z = positions[i*3+2];
        float d = CROSS_DELTA;
        pushEdge(verts, bounds,
                 xfPoint(xfrom, x-d, y,   z), xfPoint(xfrom, x+d, y,   z));
        pushEdge(verts, bounds,
                 xfPoint(xfrom, x,   y-d, z), xfPoint(xfrom, x,   y+d, z));
        pushEdge(verts, bounds,
                 xfPoint(xfrom, x,   y,   z-d), xfPoint(xfrom, x,   y,   z+d));
    }
}
