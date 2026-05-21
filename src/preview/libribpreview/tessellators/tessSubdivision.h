#pragma once
#include "previewTypes.h"
#include <vector>

void tessSubdivision(
    const float *positions,
    int numFaces,
    const int *numVerticesPerFace,
    const int *vertexIndices,
    const float *xfrom,
    const float3 &col,
    std::vector<float3> &verts, std::vector<float3> &cols, AABB &bounds
);
