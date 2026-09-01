#pragma once
#include "dataScene.h"

#include <vector>

// Expands one disc primitive into a 20-segment triangle fan (60 non-indexed vertices: 20
// triangles x 3 verts), matching the deleted pre-016 pglDisks geometry -- but with the tangent
// basis derived from N alone, never from the disc's position P. The deleted implementation used
// X = P x N, which is NaN when P == (0,0,0) or P is parallel to N; that dependency is gone here
// entirely, so both cases are structurally impossible to hit.
void expandDisk(const DiskPrimitive &disk, std::vector<float3> &outVerts, std::vector<float3> &outCols);
