#pragma once
#include "previewTypes.h"
#include <vector>

void tessPoints(
    const float *positions,
    int numPoints,
    const float *xfrom,
    std::vector<float3> &verts, AABB &bounds
);
