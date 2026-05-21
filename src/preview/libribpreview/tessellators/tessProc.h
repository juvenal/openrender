#pragma once
#include "previewTypes.h"
#include <vector>

// bmin/bmax are the camera-space AABB corners from CDelayedObject.
// xfrom (camera-to-world, 4×4 column-major) is applied to all 8 corners.
void tessProc(
    const float *bmin,
    const float *bmax,
    const float *xfrom,
    const float3 &col,
    std::vector<float3> &verts, std::vector<float3> &cols, AABB &bounds
);
