// Swift-visible C module for libribpreview. Content mirrors ribpreview_api.h.
#pragma once

typedef struct {
    float projMatrix[16];   // column-major 4×4 projection matrix
    float viewMatrix[16];   // column-major 4×4 view matrix
    float nearPlane;
    float farPlane;
    int   projectionType;   // 0 = perspective, 1 = orthographic
} PreviewCameraC;

typedef struct {
    float sceneBoundsMin[3];  // world-space AABB minimum
    float sceneBoundsMax[3];  // world-space AABB maximum
} PreviewBoundsC;

typedef struct {
    float        *vertices;     // flat line-list: [x0,y0,z0, x1,y1,z1, ...]
    int           vertexCount;  // number of float3 vertices (pairs → line segments)
    PreviewCameraC camera;
    PreviewBoundsC bounds;
} PreviewSceneC;

// Load a RIB file. Returns NULL on fatal error (message written to stderr).
PreviewSceneC *ribpreview_load(const char *ribPath);

// Free all resources allocated by ribpreview_load.
void ribpreview_free(PreviewSceneC *scene);

// Camera export — C wrappers around the C++ cameraExport API.
// camToWorld16: row-major 4×4 camera-to-world matrix (16 floats).
// projType: 0 = perspective, 1 = orthographic.
// fovDeg: vertical FOV in degrees (perspective only).
// Returns 1 on success, 0 on failure (error written to stderr).
int ribcam_write(const float *camToWorld16, int projType, float fovDeg,
                 const char *outputPath);
int ribcam_replace(const float *camToWorld16, int projType, float fovDeg,
                   const char *existingPath);
