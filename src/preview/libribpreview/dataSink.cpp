#include "dataSink.h"

#include "diskExpand.h"
#include "ri/dataLoad.h"

#include <cmath>
#include <cstring>
#include <vector>

// A fixed, deterministic cap per primitive kind (research.md §6) -- matches the existing
// MAX_POINTS idiom in tessPoints.cpp, applied uniformly to lines/points/triangles/disks
// instead of just RIB Points primitives.
static constexpr int MAX_PRIMITIVES_PER_KIND = 100000;

CDataSceneSink::CDataSceneSink(DataScene &s) : scene(s) {
}

void CDataSceneSink::triangles(int n, const float *P, const float *C) {
    for (int i = 0; i < n; i++) {
        scene.triVerts.push_back({P[i * 3 + 0], P[i * 3 + 1], P[i * 3 + 2]});
        scene.triCols.push_back({C[i * 3 + 0], C[i * 3 + 1], C[i * 3 + 2]});
    }
}

void CDataSceneSink::triangleMesh(int n, const int *indices, const float *P, const float *C) {
    // `n` triangles, three indices each, indexing into the flat P/C vertex arrays.
    for (int t = 0; t < n; t++) {
        for (int k = 0; k < 3; k++) {
            int idx = indices[t * 3 + k];
            scene.triVerts.push_back({P[idx * 3 + 0], P[idx * 3 + 1], P[idx * 3 + 2]});
            scene.triCols.push_back({C[idx * 3 + 0], C[idx * 3 + 1], C[idx * 3 + 2]});
        }
    }
}

void CDataSceneSink::lines(int n, const float *P, const float *C) {
    for (int i = 0; i < n; i++) {
        scene.lineVerts.push_back({P[i * 3 + 0], P[i * 3 + 1], P[i * 3 + 2]});
        scene.lineCols.push_back({C[i * 3 + 0], C[i * 3 + 1], C[i * 3 + 2]});
    }
}

void CDataSceneSink::points(int n, const float *P, const float *C) {
    for (int i = 0; i < n; i++) {
        scene.pointVerts.push_back({P[i * 3 + 0], P[i * 3 + 1], P[i * 3 + 2]});
        scene.pointCols.push_back({C[i * 3 + 0], C[i * 3 + 1], C[i * 3 + 2]});
    }
}

void CDataSceneSink::disks(int n, const float *P, const float *dP, const float *N, const float *C) {
    // dP is a scalar radius per disk (stride 1) -- see DiskPrimitive's comment in dataScene.h.
    // Reading it at stride 3 here would over-read past the end of an n-element array.
    for (int i = 0; i < n; i++) {
        DiskPrimitive d;
        d.P = {P[i * 3 + 0], P[i * 3 + 1], P[i * 3 + 2]};
        d.radius = dP[i];
        d.N = {N[i * 3 + 0], N[i * 3 + 1], N[i * 3 + 2]};
        d.C = {C[i * 3 + 0], C[i * 3 + 1], C[i * 3 + 2]};
        scene.disks.push_back(d);
    }
}

///////////////////////////////////////////////////////////////////////
// Drop whole primitives (groups of `groupSize` vertices) via even subsampling once the group
// count exceeds `cap`. Returns the number of primitives dropped.
//
// Indexes the OUTPUT position (0..cap-1) and maps each back to a source index via
// `i * total / cap`, rather than stepping the INPUT by a fixed `stride = total / cap`: integer
// division makes `stride` truncate to 1 for any `total` in [cap, 2*cap), silently keeping every
// single group and not reducing the count at all in that range -- e.g. 199,999 groups with
// cap=100,000 kept all 199,999 instead of capping at 100,000, doing roughly 4x the downstream
// work of a cloud comfortably over the cap. The output-indexed form always keeps exactly `cap`
// groups (when total > cap), evenly spread, regardless of the total/cap ratio.
static int decimateGrouped(std::vector<float3> &verts, std::vector<float3> &cols, int groupSize, int cap) {
    int totalGroups = (int)verts.size() / groupSize;
    if (totalGroups <= cap)
        return 0;

    std::vector<float3> newVerts, newCols;
    newVerts.reserve((size_t)cap * groupSize);
    newCols.reserve((size_t)cap * groupSize);

    for (int i = 0; i < cap; i++) {
        int g = (int)((int64_t)i * totalGroups / cap);
        for (int k = 0; k < groupSize; k++) {
            newVerts.push_back(verts[(size_t)g * groupSize + k]);
            newCols.push_back(cols[(size_t)g * groupSize + k]);
        }
    }

    verts.swap(newVerts);
    cols.swap(newCols);
    return totalGroups - cap;
}

// Same output-indexed exact-cap scheme as decimateGrouped() above, and for the same reason.
static int decimateDisks(std::vector<DiskPrimitive> &disks, int cap) {
    int total = (int)disks.size();
    if (total <= cap)
        return 0;

    std::vector<DiskPrimitive> kept;
    kept.reserve(cap);
    for (int i = 0; i < cap; i++) {
        int idx = (int)((int64_t)i * total / cap);
        kept.push_back(disks[idx]);
    }

    disks.swap(kept);
    return total - cap;
}

///////////////////////////////////////////////////////////////////////
// Builds a simple perspective camera (row-major, matching PreviewCamera's convention) that
// frames `bounds` from a fixed diagonal viewing direction. A degenerate/non-finite box (an
// empty debug-geometry dump; nothing else can produce one -- CPointCloud/CPhotonMap always
// contain at least their dummy item, see tasks.md T034) falls back to framing a unit box at
// the origin so the camera itself never carries NaN/Inf.
static PreviewCamera synthesizeCamera(const AABB &bounds) {
    float cx = 0.5f * (bounds.min.x + bounds.max.x);
    float cy = 0.5f * (bounds.min.y + bounds.max.y);
    float cz = 0.5f * (bounds.min.z + bounds.max.z);
    float dx = bounds.max.x - bounds.min.x;
    float dy = bounds.max.y - bounds.min.y;
    float dz = bounds.max.z - bounds.min.z;
    float radius = 0.5f * std::sqrt(dx * dx + dy * dy + dz * dz);

    bool sane = std::isfinite(cx) && std::isfinite(cy) && std::isfinite(cz) &&
                std::isfinite(radius) && dx >= 0.0f && dy >= 0.0f && dz >= 0.0f;
    if (!sane || radius <= 0.0f) {
        cx = cy = cz = 0.0f;
        radius = 1.0f;
    }

    const float fovDeg = 45.0f;
    const float fovRad = fovDeg * (float)M_PI / 180.0f;

    // Eye on a fixed diagonal direction from the center, far enough that a vertical FOV of
    // fovDeg encloses the bounding sphere with headroom.
    float dirX = 0.5f, dirY = 0.4f, dirZ = 1.0f;
    float dirLen = std::sqrt(dirX * dirX + dirY * dirY + dirZ * dirZ);
    dirX /= dirLen; dirY /= dirLen; dirZ /= dirLen;

    float distance = (radius * 1.5f) / std::sin(fovRad * 0.5f);
    float eyeX = cx + dirX * distance;
    float eyeY = cy + dirY * distance;
    float eyeZ = cz + dirZ * distance;

    // Look-at basis: z is the forward/viewing direction, from eye toward center. This matches
    // openRender's own camera convention (not OpenGL's) -- confirmed against
    // ribGeometryContext.cpp:337-352's projection matrix, whose w'=+z_view (proj[14]=1.0, not
    // -1.0) requires visible points to have *positive* view-space Z, i.e. the camera looks down
    // +Z, not -Z. Using "z points from center toward eye" (camera looks down -Z, the OpenGL
    // convention) here was the bug: paired with that projection matrix it puts the entire scene
    // behind the camera, so nothing -- not even the grid, drawn with the same matrix -- appears.
    float zx = -dirX, zy = -dirY, zz = -dirZ;
    float upX = 0.0f, upY = 1.0f, upZ = 0.0f;
    // x = normalize(up x z)
    float xx = upY * zz - upZ * zy;
    float xy = upZ * zx - upX * zz;
    float xz = upX * zy - upY * zx;
    float xlen = std::sqrt(xx * xx + xy * xy + xz * xz);
    xx /= xlen; xy /= xlen; xz /= xlen;
    // y = z x x
    float yx = zy * xz - zz * xy;
    float yy = zz * xx - zx * xz;
    float yz = zx * xy - zy * xx;

    PreviewCamera cam{};
    cam.projectionType = ProjectionType::Perspective;
    cam.fov = fovDeg;
    cam.frameAspectRatio = 1.0f;
    cam.nearPlane = distance * 0.01f;
    cam.farPlane = distance + radius * 4.0f + 10.0f;

    // PreviewCamera::viewMatrix must be the camera-to-world matrix ("from"), not
    // world-to-camera ("to") -- both ArcballCamera implementations (macOS's
    // ArcballCamera.swift, Linux's arcball.cpp) invert whatever they're given and use *that*
    // result directly as the actual render/view matrix ("from = to^{-1} = camera-to-world").
    // Confirmed independently on both platforms via identical comments; this is also why
    // camera export (cameraToWorldMatrix) round-trips correctly. Camera-to-world here is the
    // transpose of the look-at rotation (X/Y/Z basis rows above), with translation = eye.
    float view[16] = {
        xx, yx, zx, eyeX,
        xy, yy, zy, eyeY,
        xz, yz, zz, eyeZ,
        0, 0, 0, 1,
    };
    std::memcpy(cam.viewMatrix, view, sizeof(view));

    // Matches ribGeometryContext.cpp:337-352's convention exactly (Metal NDC, Z in [0,1],
    // w'=+z_view): proj[10]=far/(far-near), proj[11]=-(far*near)/(far-near), proj[14]=1 -- not
    // the OpenGL-standard Z-in-[-1,1]/w'=-z_view form this used before.
    float f = 1.0f / std::tan(fovRad * 0.5f);
    float nearP = cam.nearPlane, farP = cam.farPlane;
    float proj[16] = {
        f, 0, 0, 0,
        0, f, 0, 0,
        0, 0, farP / (farP - nearP), -(farP * nearP) / (farP - nearP),
        0, 0, 1, 0,
    };
    std::memcpy(cam.projMatrix, proj, sizeof(proj));

    return cam;
}

void buildDataScene(CDataView *view, RibDataType documentType, DataScene &scene) {
    scene.lineVerts.clear();
    scene.lineCols.clear();
    scene.pointVerts.clear();
    scene.pointCols.clear();
    scene.triVerts.clear();
    scene.triCols.clear();
    scene.disks.clear();

    CDataSceneSink sink(scene);
    CDataView::install(&sink);
    view->draw();
    CDataView::install(NULL);

    float bmin[3], bmax[3];
    view->bound(bmin, bmax);
    scene.bounds.min = {bmin[0], bmin[1], bmin[2]};
    scene.bounds.max = {bmax[0], bmax[1], bmax[2]};

    scene.documentType = documentType;
    scene.numChannels = view->numChannels();
    scene.currentChannel = view->currentChannel();
    scene.detailLevel = view->detailLevel();
    scene.drawMode = view->drawMode();

    scene.sourceDiskCount = (int)scene.disks.size();

    scene.decimatedCount = 0;
    scene.decimatedCount += decimateGrouped(scene.pointVerts, scene.pointCols, 1, MAX_PRIMITIVES_PER_KIND);
    scene.decimatedCount += decimateGrouped(scene.lineVerts, scene.lineCols, 2, MAX_PRIMITIVES_PER_KIND);
    scene.decimatedCount += decimateGrouped(scene.triVerts, scene.triCols, 3, MAX_PRIMITIVES_PER_KIND);
    scene.decimatedCount += decimateDisks(scene.disks, MAX_PRIMITIVES_PER_KIND);

    // Discs are capped by disc count above (scene.disks, pre-expansion) rather than by the
    // resulting vertex count -- expand only the surviving discs into the triangle buffer.
    //
    // Reserving the exact final size ONCE here, before the loop, is not just an optimization:
    // expandDisk() deliberately never reserves internally (see its own comment) specifically
    // because per-call reserve() calls with no headroom turned this into O(N^2) reallocation
    // traffic -- confirmed as a 9.5-minute load for a 500K-point cloud (100,000 surviving
    // discs). Reserving here once makes every subsequent push_back inside the loop a no-op
    // allocation-wise.
    scene.triVerts.reserve(scene.triVerts.size() + scene.disks.size() * kVertsPerDisk);
    scene.triCols.reserve(scene.triCols.size() + scene.disks.size() * kVertsPerDisk);
    for (const DiskPrimitive &d : scene.disks)
        expandDisk(d, scene.triVerts, scene.triCols);

    scene.camera = synthesizeCamera(scene.bounds);
}

///////////////////////////////////////////////////////////////////////
// C ABI (ribpreview_api.h) -- the opaque RibDataDocument handle and its six entry points.

static RibDataType mapDataFileType(EDataFileType t) {
    switch (t) {
        case DATA_PHOTONMAP: return RIBDATA_TYPE_PHOTONMAP;
        case DATA_IRRADIANCECACHE: return RIBDATA_TYPE_IRRADIANCECACHE;
        case DATA_GATHERCACHE: return RIBDATA_TYPE_GATHERCACHE;
        case DATA_POINTCLOUD: return RIBDATA_TYPE_POINTCLOUD;
        case DATA_BRICKMAP: return RIBDATA_TYPE_BRICKMAP;
        default: return RIBDATA_TYPE_DEBUGDUMP;
    }
}

struct RibDataDocument {
    CDataDocument doc;
    RibDataType type;
    DataScene scene;
    DataSceneC sceneC{};
};

static void packSnapshot(RibDataDocument *d) {
    DataSceneC &c = d->sceneC;
    c.lines = {(float *)d->scene.lineVerts.data(), (float *)d->scene.lineCols.data(), (int)d->scene.lineVerts.size()};
    c.points = {(float *)d->scene.pointVerts.data(), (float *)d->scene.pointCols.data(), (int)d->scene.pointVerts.size()};
    c.triangles = {(float *)d->scene.triVerts.data(), (float *)d->scene.triCols.data(), (int)d->scene.triVerts.size()};
    c.sourceDiskCount = d->scene.sourceDiskCount;
    c.decimatedCount = d->scene.decimatedCount;
    c.bounds.sceneBoundsMin[0] = d->scene.bounds.min.x;
    c.bounds.sceneBoundsMin[1] = d->scene.bounds.min.y;
    c.bounds.sceneBoundsMin[2] = d->scene.bounds.min.z;
    c.bounds.sceneBoundsMax[0] = d->scene.bounds.max.x;
    c.bounds.sceneBoundsMax[1] = d->scene.bounds.max.y;
    c.bounds.sceneBoundsMax[2] = d->scene.bounds.max.z;
    for (int r = 0; r < 4; r++)
        for (int col = 0; col < 4; col++) {
            c.camera.projMatrix[col * 4 + r] = d->scene.camera.projMatrix[r * 4 + col];
            c.camera.viewMatrix[col * 4 + r] = d->scene.camera.viewMatrix[r * 4 + col];
        }
    c.camera.nearPlane = d->scene.camera.nearPlane;
    c.camera.farPlane = d->scene.camera.farPlane;
    c.camera.projectionType = (d->scene.camera.projectionType == ProjectionType::Orthographic) ? 1 : 0;
    c.camera.fov = d->scene.camera.fov;
    c.camera.frameAspectRatio = d->scene.camera.frameAspectRatio;
    c.documentType = d->scene.documentType;
    c.numChannels = d->scene.numChannels;
    c.currentChannel = d->scene.currentChannel;
    c.detailLevel = d->scene.detailLevel;
    c.drawMode = d->scene.drawMode;
}

int ribdata_sniff(const char *path) {
    EDataFileType t = dataSniff(path);
    switch (t) {
        case DATA_PHOTONMAP:
        case DATA_IRRADIANCECACHE:
        case DATA_GATHERCACHE:
        case DATA_POINTCLOUD:
        case DATA_BRICKMAP:
        case DATA_DEBUGDUMP:
            return (int)mapDataFileType(t);
        case DATA_BAD_VERSION:
        case DATA_BAD_WORDSIZE:
            return -2;
        default:
            return -1;
    }
}

RibDataDocument *ribdata_open(const char *path, int *err) {
    RibDataDocument *d = new RibDataDocument();
    EDataFileType t = d->doc.open(path);

    switch (t) {
        case DATA_PHOTONMAP:
        case DATA_IRRADIANCECACHE:
        case DATA_GATHERCACHE:
        case DATA_POINTCLOUD:
        case DATA_BRICKMAP:
        case DATA_DEBUGDUMP:
            d->type = mapDataFileType(t);
            buildDataScene(d->doc.view(), d->type, d->scene);
            if (err != NULL)
                *err = (int)t;
            return d;
        default:
            if (err != NULL)
                *err = (int)t;
            delete d;
            return NULL;
    }
}

const DataSceneC *ribdata_snapshot(RibDataDocument *doc) {
    if (doc == NULL)
        return NULL;
    packSnapshot(doc);
    return &doc->sceneC;
}

int ribdata_key(RibDataDocument *doc, int key) {
    if (doc == NULL || doc->doc.view() == NULL)
        return 0;

    int changed = doc->doc.view()->keyDown(key);
    if (changed)
        buildDataScene(doc->doc.view(), doc->type, doc->scene);

    return changed;
}

const char *ribdata_channel_name(RibDataDocument *doc, int index) {
    if (doc == NULL || doc->doc.view() == NULL)
        return NULL;
    return doc->doc.view()->channelName(index);
}

void ribdata_close(RibDataDocument *doc) {
    delete doc;
}
