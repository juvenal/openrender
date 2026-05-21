#pragma once

#ifdef __cplusplus
extern "C" {
#endif

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
    float        *vertices;     // flat line-list: [x0,y0,z0, x1,y1,z1, …]
    float        *colors;       // per-vertex RGB: [r0,g0,b0, r1,g1,b1, …], count == vertexCount
    int           vertexCount;  // number of float3 vertices (pairs → line segments)
    PreviewCameraC camera;
    PreviewBoundsC bounds;
} PreviewSceneC;

// Load a RIB file. Returns NULL on fatal error (message written to stderr).
PreviewSceneC *ribpreview_load(const char *ribPath);

// Free all resources allocated by ribpreview_load.
void ribpreview_free(PreviewSceneC *scene);

#ifdef __cplusplus
}
#endif
