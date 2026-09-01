#pragma once
#include "previewTypes.h"
#include "ribpreview_api.h"

#include <vector>

// A disc primitive as read from the source data document, before CPU-side expansion into
// triangles (research.md §5). Kept separately from DataScene's triangle array so JSON output
// and tests can assert against the disc count/shape directly, without depending on the 20-
// segment tessellation an integration test would otherwise have to reverse-engineer.
//
// `radius` is a scalar, not a vector: CPrimitiveSink::disks()'s `dP` argument (src/ri/dataView.h)
// is walked with a stride-1 `cdP++` at every real call site (pointCloud.cpp, brickmap.cpp,
// irradiance.cpp) and in the deleted pre-016 pglDisks implementation alike -- one float per
// disk, unlike P/N/C which are stride-3 float3 arrays.
struct DiskPrimitive {
    float3 P;      // center
    float3 N;      // orientation normal
    float  radius; // disk radius (CPrimitiveSink::disks()'s dP argument -- scalar, not a vector)
    float3 C;      // color
};

// The platform-neutral, expanded-to-render-ready form of one open data document. Four flat
// primitive arrays (matching PrimArrayC/DataSceneC's shape 1:1) plus the pre-expansion disk
// list. Populated by CDataSceneSink (dataSink.h) from a CPrimitiveSink callback stream.
struct DataScene {
    std::vector<float3> lineVerts, lineCols;
    std::vector<float3> pointVerts, pointCols;
    std::vector<float3> triVerts, triCols; // includes CPU-expanded discs
    std::vector<DiskPrimitive> disks;      // pre-expansion; source of truth for sourceDiskCount

    int sourceDiskCount = 0;
    int decimatedCount = 0;
    AABB bounds{};
    PreviewCamera camera{};
    RibDataType documentType = RIBDATA_TYPE_PHOTONMAP;
    int numChannels = 0;
    int currentChannel = -1;
    int detailLevel = -1;
    int drawMode = 0;
};
