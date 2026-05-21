#include "tessSubdivision.h"
#include "tessUtils.h"

void tessSubdivision(const float *positions, int numFaces,
                     const int *numVerticesPerFace, const int *vertexIndices,
                     const float *xfrom, const float3 &col,
                     std::vector<float3> &verts, std::vector<float3> &cols, AABB &bounds) {
    int base = 0;
    for (int f = 0; f < numFaces; f++) {
        int nv = numVerticesPerFace[f];
        for (int v = 0; v < nv; v++) {
            int i0 = vertexIndices[base + v];
            int i1 = vertexIndices[base + (v + 1) % nv];
            pushEdge(verts, cols, bounds,
                     xfPoint(xfrom, positions[i0*3], positions[i0*3+1], positions[i0*3+2]),
                     xfPoint(xfrom, positions[i1*3], positions[i1*3+1], positions[i1*3+2]), col);
        }
        base += nv;
    }
}
