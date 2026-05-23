#pragma once
#include "previewTypes.h"
#include <vector>

// CPreviewContext extracts private fields and passes raw data here.
void tessPolygon(
    const float *positions,   // [x0,y0,z0, x1,y1,z1, ...]
    int npoly,
    const int *nholes,        // total loop count per face (null → 1 loop); matches CPolygonMesh::nholes
    const int *nvertices,     // vertex count per loop (indexed by loop, not by face)
    const int *indices,       // vertex indices
    const float *xfrom,       // column-major 4×4 object→world matrix
    const float3 &col,
    std::vector<float3> &verts, std::vector<float3> &cols, AABB &bounds
);
