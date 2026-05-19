#pragma once

struct CameraExport {
    float cameraToWorld[16]; // row-major 4×4 camera-to-world matrix
    int   projectionType;    // 0 = perspective, 1 = orthographic
    float fov;               // horizontal FOV in degrees (perspective only)
    const char *outputPath;
    bool  updateExisting;    // if true, replace camera in existing file; else write new file
};

// Write a new RIB camera snippet (Projection + Transform statements).
bool writeRibCamera(const CameraExport &cam);

// Replace the Projection and Transform in the pre-WorldBegin section of an existing RIB.
// All other content is preserved byte-for-byte.
bool replaceRibCamera(const CameraExport &cam);
