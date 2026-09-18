#include "previewContext.h"
#include "ribpreview_api.h"
#include "renderer.h"
#include "memory.h"
#include "rib.h"
#include "tessellators/tessPolygon.h"
#include "tessellators/tessPatch.h"
#include "tessellators/tessQuadric.h"
#include "tessellators/tessCurve.h"
#include "tessellators/tessPoints.h"
#include "tessellators/tessSubdivision.h"
#include "tessellators/tessProc.h"
#include "polygons.h"
#include "patches.h"
#include "quadrics.h"
#include "curves.h"
#include "points.h"
#include "subdivisionCreator.h"
#include "delayed.h"
#include <cmath>
#include <cstdio>
#include <cfloat>
#include <cstring>

CPreviewContext::CPreviewContext() {
    result_.sceneBounds.min = { FLT_MAX,  FLT_MAX,  FLT_MAX};
    result_.sceneBounds.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
}

void CPreviewContext::RiWorldBegin() {
    CRibGeometryContext::RiWorldBegin();
    extractCamera();
}

// ─── addObject ────────────────────────────────────────────────────────────────
//
// Called by emitObject() only for geometry NOT inside ObjectBegin/End.
// obj->xform->from = object-to-world (CRibGeometryContext resets xform to
// identity at RiWorldBegin, so camera is not baked in).

void CPreviewContext::addObject(CObject *obj) {
    if (!obj) return;

    std::vector<float3> &V = result_.vertices;
    std::vector<float3> &C = result_.colors;
    AABB                &B = result_.sceneBounds;

    float3 col = { obj->attributes->surfaceColor[0],
                   obj->attributes->surfaceColor[1],
                   obj->attributes->surfaceColor[2] };

    const float *wf = obj->xform->from;

    if (auto *m = dynamic_cast<CPolygonMesh *>(obj)) {
        const float *positions; int npoly; const int *nholes, *nvertices, *vertices;
        m->wireData(positions, npoly, nholes, nvertices, vertices);
        tessPolygon(positions, npoly, nholes, nvertices, vertices, wf, col, V, C, B);

    } else if (auto *m = dynamic_cast<CPatchMesh *>(obj)) {
        const float *positions; int nu, nv;
        m->wireData(positions, nu, nv);
        tessPatch(positions, nu, nv, wf, col, V, C, B);

    } else if (auto *m = dynamic_cast<CNURBSPatchMesh *>(obj)) {
        const float *positions; int nu, nv;
        m->wireData(positions, nu, nv);
        tessNurbs(positions, nu, nv, wf, col, V, C, B);

    } else if (auto *s = dynamic_cast<CSphere *>(obj)) {
        float r, umax, vmin, vmax;
        s->wireData(r, umax, vmin, vmax);
        tessQuadricSphere(r, umax, vmin, vmax, wf, col, V, C, B);

    } else if (auto *d = dynamic_cast<CDisk *>(obj)) {
        float r, z, umax;
        d->wireData(r, z, umax);
        tessQuadricDisk(r, z, umax, wf, col, V, C, B);

    } else if (auto *c = dynamic_cast<CCone *>(obj)) {
        float r, height, umax;
        c->wireData(r, height, umax);
        tessQuadricCone(r, height, umax, wf, col, V, C, B);

    } else if (auto *c = dynamic_cast<CCylinder *>(obj)) {
        float r, zmin, zmax, umax;
        c->wireData(r, zmin, zmax, umax);
        tessQuadricCylinder(r, zmin, zmax, umax, wf, col, V, C, B);

    } else if (auto *p = dynamic_cast<CParaboloid *>(obj)) {
        float r, zmin, zmax, umax;
        p->wireData(r, zmin, zmax, umax);
        tessQuadricParaboloid(r, zmin, zmax, umax, wf, col, V, C, B);

    } else if (auto *h = dynamic_cast<CHyperboloid *>(obj)) {
        const float *p1, *p2; float umax;
        h->wireData(p1, p2, umax);
        tessQuadricHyperboloid(p1, p2, umax, wf, col, V, C, B);

    } else if (auto *t = dynamic_cast<CToroid *>(obj)) {
        float rmax, rmin, vmin, vmax, umax;
        t->wireData(rmax, rmin, vmin, vmax, umax);
        tessQuadricToroid(rmax, rmin, vmin, vmax, umax, wf, col, V, C, B);

    } else if (auto *c = dynamic_cast<CCurveMesh *>(obj)) {
        const float *positions; int numCurves, wrap; const int *nverts;
        c->wireData(positions, numCurves, nverts, wrap);
        tessCurve(positions, numCurves, nverts, wrap != 0, wf, col, V, C, B);

    } else if (auto *p = dynamic_cast<CPoints *>(obj)) {
        const float *positions; int numPoints;
        p->wireData(positions, numPoints);
        tessPoints(positions, numPoints, wf, col, V, C, B);

    } else if (auto *s = dynamic_cast<CSubdivMesh *>(obj)) {
        const float *positions; int numFaces; const int *numVerticesPerFace, *vertexIndices;
        s->wireData(positions, numFaces, numVerticesPerFace, vertexIndices);
        tessSubdivision(positions, numFaces, numVerticesPerFace, vertexIndices,
                        wf, col, V, C, B);

    } else if (auto *d = dynamic_cast<CDelayedObject *>(obj)) {
        // CDelayedObject ctor calls xform->transformBound(bmin, bmax), so bounds
        // are already in world space. Pass identity to tessProc.
        static const float id[16] = {1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1};
        tessProc(d->bmin, d->bmax, id, col, V, C, B);

    } else if (auto *di = dynamic_cast<CDelayedInstance *>(obj)) {
        for (CObject *child = di->instance; child != nullptr; child = child->sibling)
            child->instantiate(di->attributes, di->xform, this);

    } else {
        fprintf(stderr, "orender-wire: unknown primitive type, skipped\n");
    }

    delete obj;
}

// ─── Camera extraction ────────────────────────────────────────────────────────

void CPreviewContext::extractCamera() {
    PreviewCamera &cam = result_.camera;

    // cameraState.viewMatrix = currentXform_->to (column-major, orender convention).
    // Transpose to row-major for PreviewCamera.
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            cam.viewMatrix[r*4+c] = cameraState.viewMatrix[r + c*4];

    // projMatrix is already row-major (built in CRibGeometryContext::RiWorldBegin).
    memcpy(cam.projMatrix, cameraState.projMatrix, 16 * sizeof(float));

    cam.nearPlane        = cameraState.nearPlane;
    cam.farPlane         = cameraState.farPlane;
    cam.frameAspectRatio = cameraState.frameAspectRatio;
    cam.fov              = cameraState.fov;
    cam.projectionType   = (cameraState.projectionType == 0)
                           ? ProjectionType::Perspective
                           : ProjectionType::Orthographic;
}

void CPreviewContext::updateBounds(const float3 &p) {
    AABB &bb = result_.sceneBounds;
    if (p.x < bb.min.x) bb.min.x = p.x;
    if (p.y < bb.min.y) bb.min.y = p.y;
    if (p.z < bb.min.z) bb.min.z = p.z;
    if (p.x > bb.max.x) bb.max.x = p.x;
    if (p.y > bb.max.y) bb.max.y = p.y;
    if (p.z > bb.max.z) bb.max.z = p.z;
}

// ─── C API ────────────────────────────────────────────────────────────────────

PreviewSceneC *ribpreview_load(const char *ribPath) {
    CRenderer::initDeclarations();
    memoryInit(CRenderer::globalMemory);
    RiBeginLite();

    CPreviewContext ctx;
    // CRiInterface's own constructor already does this (renderMan = this), so
    // this line is technically redundant -- kept for clarity at the call site.
    // The reset below is NOT redundant: ctx stays alive well past ribParse(),
    // and renderMan must stop pointing at it before anything else runs.
    renderMan = &ctx;
    ribParse(ribPath, nullptr);
    renderMan = nullptr;

    PreviewScene scene = std::move(ctx.result_);

    memoryTini(CRenderer::globalMemory);
    CRenderer::shutdownDeclarations();

    // Synthesize valid clipping if no camera was set.
    PreviewCamera &cam = scene.camera;
    if (cam.nearPlane <= 0.0f) cam.nearPlane = 0.1f;
    if (cam.farPlane <= cam.nearPlane) {
        AABB &bb = scene.sceneBounds;
        float dx = bb.max.x - bb.min.x, dy = bb.max.y - bb.min.y, dz = bb.max.z - bb.min.z;
        float diag = sqrtf(dx*dx + dy*dy + dz*dz);
        cam.farPlane = cam.nearPlane + diag + 10.0f;
    }

    // Pack into the C struct.
    PreviewSceneC *out = new PreviewSceneC{};
    int nVerts = (int)scene.vertices.size();
    out->vertexCount = nVerts;

    if (nVerts > 0) {
        out->vertices = new float[nVerts * 3];
        out->colors   = new float[nVerts * 3];
        for (int i = 0; i < nVerts; i++) {
            out->vertices[i*3+0] = scene.vertices[i].x;
            out->vertices[i*3+1] = scene.vertices[i].y;
            out->vertices[i*3+2] = scene.vertices[i].z;
            out->colors[i*3+0]   = scene.colors[i].x;
            out->colors[i*3+1]   = scene.colors[i].y;
            out->colors[i*3+2]   = scene.colors[i].z;
        }
    } else {
        out->vertices = nullptr;
        out->colors   = nullptr;
    }

    // Camera: transpose row-major (C++) → column-major (C ABI / Metal / GL).
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) {
            out->camera.projMatrix[c*4+r] = cam.projMatrix[r*4+c];
            out->camera.viewMatrix[c*4+r] = cam.viewMatrix[r*4+c];
        }
    out->camera.nearPlane        = cam.nearPlane;
    out->camera.farPlane         = cam.farPlane;
    out->camera.projectionType   = (cam.projectionType == ProjectionType::Perspective) ? 0 : 1;
    out->camera.fov              = cam.fov;
    out->camera.frameAspectRatio = cam.frameAspectRatio;

    // Scene bounds.
    AABB &bb = scene.sceneBounds;
    out->bounds.sceneBoundsMin[0] = bb.min.x;
    out->bounds.sceneBoundsMin[1] = bb.min.y;
    out->bounds.sceneBoundsMin[2] = bb.min.z;
    out->bounds.sceneBoundsMax[0] = bb.max.x;
    out->bounds.sceneBoundsMax[1] = bb.max.y;
    out->bounds.sceneBoundsMax[2] = bb.max.z;

    return out;
}

void ribpreview_free(PreviewSceneC *scene) {
    if (!scene) return;
    delete[] scene->vertices;
    delete[] scene->colors;
    delete scene;
}
