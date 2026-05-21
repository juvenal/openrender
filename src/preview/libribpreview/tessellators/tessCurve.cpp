#include "tessCurve.h"
#include "tessUtils.h"

void tessCurve(const float *positions, int numCurves, const int *nverts,
               bool periodic, const float *xfrom, const float3 &col,
               std::vector<float3> &verts, std::vector<float3> &cols, AABB &bounds) {
    int base = 0;
    for (int c = 0; c < numCurves; c++) {
        int nv = nverts[c];
        for (int v = 0; v < nv - 1; v++) {
            int i0 = base + v, i1 = base + v + 1;
            pushEdge(verts, cols, bounds,
                     xfPoint(xfrom, positions[i0*3], positions[i0*3+1], positions[i0*3+2]),
                     xfPoint(xfrom, positions[i1*3], positions[i1*3+1], positions[i1*3+2]), col);
        }
        if (periodic && nv > 1) {
            int i0 = base + nv - 1;
            pushEdge(verts, cols, bounds,
                     xfPoint(xfrom, positions[i0*3], positions[i0*3+1], positions[i0*3+2]),
                     xfPoint(xfrom, positions[base*3], positions[base*3+1], positions[base*3+2]), col);
        }
        base += nv;
    }
}
