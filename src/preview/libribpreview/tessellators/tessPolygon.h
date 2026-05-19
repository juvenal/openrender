#pragma once
#include "previewTypes.h"
#include <vector>

// CPreviewContext extracts private fields and passes raw data here.
void tessPolygon(
    const float *positions,   // [x0,y0,z0, x1,y1,z1, ...]
    int npoly,
    const int *nholes,        // extra-loop count per face (may be null → 0 holes)
    const int *nvertices,     // vertex count per loop (indexed by loop, not by face)
    const int *indices,       // vertex indices
    const float *xfrom,       // column-major 4×4 object→world matrix
    std::vector<float3> &verts, AABB &bounds
);
