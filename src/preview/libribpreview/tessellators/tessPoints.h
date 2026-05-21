#pragma once
#include "previewTypes.h"
#include <vector>

void tessPoints(
    const float *positions,
    int numPoints,
    const float *xfrom,
    const float3 &col,
    std::vector<float3> &verts, std::vector<float3> &cols, AABB &bounds
);
