#pragma once
#include "previewTypes.h"
#include <vector>

void tessSubdivision(
    const float *positions,
    int numFaces,
    const int *numVerticesPerFace,
    const int *vertexIndices,
    const float *xfrom,
    std::vector<float3> &verts, AABB &bounds
);
