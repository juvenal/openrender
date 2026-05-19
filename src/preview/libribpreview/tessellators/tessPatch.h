#pragma once
#include "previewTypes.h"
#include <vector>

// Control-cage wireframe for bilinear or bicubic patch meshes.
// nu × nv is the total control-point grid size.
void tessPatch(
    const float *positions,
    int nu, int nv,
    const float *xfrom,
    std::vector<float3> &verts, AABB &bounds
);

// Control-cage wireframe for NURBS patch meshes (same grid layout).
void tessNurbs(
    const float *positions,
    int nu, int nv,
    const float *xfrom,
    std::vector<float3> &verts, AABB &bounds
);
