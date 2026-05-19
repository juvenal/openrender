#pragma once
#include "previewTypes.h"
#include <vector>

// Each function receives the primitive-specific parameters extracted by CPreviewContext.

void tessQuadricSphere(float r, float umax, float vmin, float vmax,
                       const float *xfrom,
                       std::vector<float3> &verts, AABB &bounds);

void tessQuadricDisk(float r, float z, float umax,
                     const float *xfrom,
                     std::vector<float3> &verts, AABB &bounds);

void tessQuadricCone(float r, float height, float umax,
                     const float *xfrom,
                     std::vector<float3> &verts, AABB &bounds);

void tessQuadricCylinder(float r, float zmin, float zmax, float umax,
                         const float *xfrom,
                         std::vector<float3> &verts, AABB &bounds);

void tessQuadricParaboloid(float rmax, float zmin, float zmax, float umax,
                           const float *xfrom,
                           std::vector<float3> &verts, AABB &bounds);

void tessQuadricHyperboloid(const float p1[3], const float p2[3], float umax,
                            const float *xfrom,
                            std::vector<float3> &verts, AABB &bounds);

void tessQuadricToroid(float rmax, float rmin, float phimin, float phimax, float umax,
                       const float *xfrom,
                       std::vector<float3> &verts, AABB &bounds);
