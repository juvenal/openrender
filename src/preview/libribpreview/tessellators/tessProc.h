#pragma once
#include "previewTypes.h"
#include <vector>

// bmin/bmax are world-space bounds from CObject::bmin/bmax (vector[3])
void tessProc(
    const float *bmin,
    const float *bmax,
    std::vector<float3> &verts, AABB &bounds
);
