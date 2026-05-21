#pragma once
#include <string>
#include <vector>

struct float3 {
    float x, y, z;
};

enum class ProjectionType { Perspective, Orthographic };

struct PreviewCamera {
    float projMatrix[16];    // row-major, camera → clip
    float viewMatrix[16];    // row-major, world → camera
    float nearPlane;
    float farPlane;
    ProjectionType projectionType;
    float fov;               // vertical FOV in degrees (perspective only; orender fov = minor-axis angle)
    float frameAspectRatio;
};

struct AABB {
    float3 min;
    float3 max;
};

struct PreviewScene {
    std::vector<float3>      vertices;    // flat line-list, always even count
    std::vector<float3>      colors;      // parallel to vertices, per-vertex RGB
    PreviewCamera            camera;
    AABB                     sceneBounds;
    std::vector<std::string> warnings;
};
