#include "diskExpand.h"

#include <cmath>

static constexpr int DISK_SEGMENTS = 20;

static float3 add(const float3 &a, const float3 &b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
static float3 mul(const float3 &a, float s) { return {a.x * s, a.y * s, a.z * s}; }
static float3 cross(const float3 &a, const float3 &b) {
    return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x };
}
static float length(const float3 &a) { return std::sqrt(a.x * a.x + a.y * a.y + a.z * a.z); }

static float3 normalizeSafe(const float3 &a, const float3 &fallback) {
    float len = length(a);
    return (len > 1e-9f) ? mul(a, 1.0f / len) : fallback;
}

void expandDisk(const DiskPrimitive &disk, std::vector<float3> &outVerts, std::vector<float3> &outCols) {
    float3 N = normalizeSafe(disk.N, float3{0.0f, 0.0f, 1.0f});

    // Axis-picking basis, derived from N alone -- never from disk.P (see diskExpand.h). Pick
    // whichever world axis is farthest from parallel to N as the seed for the cross product, so
    // the seed is never (anti)parallel to N regardless of N's own direction.
    float3 refAxis = (std::fabs(N.y) < 0.99f) ? float3{0.0f, 1.0f, 0.0f} : float3{1.0f, 0.0f, 0.0f};
    float3 X = normalizeSafe(cross(refAxis, N), float3{1.0f, 0.0f, 0.0f});
    float3 Y = normalizeSafe(cross(X, N), float3{0.0f, 1.0f, 0.0f});

    const float step = 6.283185307179586f / (float)DISK_SEGMENTS;
    const float radius = disk.radius;

    outVerts.reserve(outVerts.size() + DISK_SEGMENTS * 3);
    outCols.reserve(outCols.size() + DISK_SEGMENTS * 3);

    float3 rim0 = add(disk.P, mul(X, radius)); // theta = 0
    for (int i = 0; i < DISK_SEGMENTS; i++) {
        float theta1 = (float)(i + 1) * step;
        float3 rim1 = add(disk.P, add(mul(X, std::cos(theta1) * radius), mul(Y, std::sin(theta1) * radius)));

        outVerts.push_back(disk.P);
        outVerts.push_back(rim0);
        outVerts.push_back(rim1);
        outCols.push_back(disk.C);
        outCols.push_back(disk.C);
        outCols.push_back(disk.C);

        rim0 = rim1;
    }
}
