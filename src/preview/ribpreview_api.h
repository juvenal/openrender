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
    float fov;               // vertical FOV in degrees (perspective only)
    float frameAspectRatio;  // added for spec 016's --json camera reporting
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

// ─── Data documents (spec 016) ──────────────────────────────────────────────
// A second document type alongside the RIB-scene path above: openRender's precomputed
// data-structure files (photon maps, irradiance/gather caches, point clouds, brick maps, and
// raw debug-geometry dumps), auto-detected from content.

typedef struct {
    float *verts;   // flat float3
    float *cols;    // flat float3, parallel to verts
    int    count;   // number of float3 elements (not floats)
} PrimArrayC;

typedef enum {
    RIBDATA_TYPE_PHOTONMAP = 0,
    RIBDATA_TYPE_IRRADIANCECACHE,
    RIBDATA_TYPE_GATHERCACHE,
    RIBDATA_TYPE_POINTCLOUD,
    RIBDATA_TYPE_BRICKMAP,
    RIBDATA_TYPE_DEBUGDUMP,
} RibDataType;

typedef struct {
    PrimArrayC     lines;
    PrimArrayC     points;
    PrimArrayC     triangles;      // includes CPU-expanded discs
    int            sourceDiskCount;
    int            decimatedCount;
    PreviewBoundsC bounds;
    PreviewCameraC camera;         // synthesized framing camera
    RibDataType    documentType;
    int            numChannels;
    int            currentChannel; // -1 if numChannels == 0
    int            detailLevel;    // -1 if not applicable (non-brick-map)
    int            drawMode;       // meaning is per-documentType; see data-model.md
} DataSceneC;

typedef struct RibDataDocument RibDataDocument; // opaque

// Sniff a file's type without opening it. Returns RIBDATA_TYPE_* if recognized and compatible;
// -1 if the file has no data-file magic number at all (including a valid RIB scene -- this
// function does not attempt RIB parsing); -2 if the magic number matched a known data-file type
// but the file is otherwise incompatible (version or word-size mismatch) -- callers must treat
// -2 as "this is a data file" (route to ribdata_open()/exit code 4), not fall back to RIB, or a
// corrupted/incompatible data file silently renders as an empty RIB scene.
int ribdata_sniff(const char *path);

// Open a data file. Returns NULL on failure (message written to stderr); *err receives one of
// the EDataFileType codes from src/ri/dataLoad.h so the caller can map
// DATA_BAD_VERSION/DATA_BAD_WORDSIZE to CLI exit code 4, and DATA_NOT_A_DATA_FILE to "try
// ribpreview_load() instead".
RibDataDocument *ribdata_open(const char *path, int *err);

// Produce the current visualization state. The returned pointer is owned by the document and
// is invalidated by the next call to ribdata_snapshot() or ribdata_key() on the same handle.
const DataSceneC *ribdata_snapshot(RibDataDocument *doc);

// Apply a key press (ASCII value) to the document's interactive state. Returns non-zero if
// state changed (caller should call ribdata_snapshot() again and re-upload); zero otherwise.
int ribdata_key(RibDataDocument *doc, int key);

// Channel name lookup, 0-indexed. Returns NULL if index is out of range or numChannels == 0.
const char *ribdata_channel_name(RibDataDocument *doc, int index);

// Releases the document and everything ribdata_snapshot() has returned for it. Safe to call
// with NULL (no-op).
void ribdata_close(RibDataDocument *doc);

// Camera export — C-linkage wrappers around the C++ cameraExport API
// (src/preview/libribpreview/cameraExport.h), defined in cameraExport.cpp.
// camToWorld16: row-major 4×4 camera-to-world matrix (16 floats).
// projType: 0 = perspective, 1 = orthographic.
// fovDeg: vertical FOV in degrees (perspective only).
// Returns 1 on success, 0 on failure (error written to stderr).
int ribcam_write(const float *camToWorld16, int projType, float fovDeg,
                 const char *outputPath);
int ribcam_replace(const float *camToWorld16, int projType, float fovDeg,
                    const char *existingPath);

#ifdef __cplusplus
}
#endif
