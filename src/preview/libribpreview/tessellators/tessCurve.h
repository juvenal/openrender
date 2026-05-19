#pragma once
#include "previewTypes.h"
#include <vector>

void tessCurve(
    const float *positions, // xyz triples for all curve vertices, packed
    int numCurves,
    const int *nverts,      // vertex count per curve
    bool periodic,
    const float *xfrom,
    std::vector<float3> &verts, AABB &bounds
);
