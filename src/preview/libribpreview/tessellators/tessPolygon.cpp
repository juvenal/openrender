#include "tessPolygon.h"
#include "tessUtils.h"

void tessPolygon(
    const float *positions, int npoly, const int *nholes,
    const int *nvertices, const int *indices,
    const float *xfrom, const float3 &col,
    std::vector<float3> &verts, std::vector<float3> &cols, AABB &bounds)
{
    int loopBase = 0;
    for (int f = 0; f < npoly; f++) {
        int nloops = nholes ? nholes[f] : 1;
        for (int l = 0; l < nloops; l++) {
            int nv = nvertices[loopBase];
            int vbase = 0;
            for (int ll = 0; ll < loopBase; ll++) vbase += nvertices[ll];

            for (int v = 0; v < nv; v++) {
                int i0 = indices[vbase + v];
                int i1 = indices[vbase + (v + 1) % nv];
                float3 a = xfPoint(xfrom, positions[i0*3], positions[i0*3+1], positions[i0*3+2]);
                float3 b = xfPoint(xfrom, positions[i1*3], positions[i1*3+1], positions[i1*3+2]);
                pushEdge(verts, cols, bounds, a, b, col);
            }
            loopBase++;
        }
    }
}
