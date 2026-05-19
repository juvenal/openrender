#include "previewContext.h"
#include "ribpreview_api.h"
#include "riHooks.h"
#include "renderer.h"
#include "riInterface.h"
#include "rib.h"
#include "options.h"
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

CPreviewContext::CPreviewContext() {
    result_.sceneBounds.min = { FLT_MAX,  FLT_MAX,  FLT_MAX};
    result_.sceneBounds.max = {-FLT_MAX, -FLT_MAX, -FLT_MAX};
}

void CPreviewContext::RiDisplayV(const char * /*name*/, const char * /*type*/,
                                  const char * /*mode*/,
                                  int /*n*/, const char * /*tokens*/[],
                                  const void * /*params*/[]) {
    // Redirect all output to /dev/null — we only want geometry.
    CRendererContext::RiDisplayV("/dev/null", "file", "rgba", 0, nullptr, nullptr);
}

void CPreviewContext::RiWorldBegin() {
    // Ensure at least one display is configured if the RIB has no Display statement.
    CRendererContext::RiDisplayV("/dev/null", "file", "rgba", 0, nullptr, nullptr);
    CRendererContext::RiWorldBegin();
    extractCamera();
}

void CPreviewContext::RiWorldEnd() {
    CRendererContext::RiWorldEnd();
}

// ─── addObject ────────────────────────────────────────────────────────────────
//
// CPreviewContext has friend access to all geometry classes, so we can read
// their private fields here. We extract raw data and call free tessellator
// functions that need no friend access.
//
// refCount on obj is still 0 (we never call obj->attach()), so we use
// delete directly; ~CObject handles xform/attributes detach.

void CPreviewContext::addObject(CObject *obj) {
    if (!obj) return;
    std::vector<float3> &V = result_.vertices;
    AABB                &B = result_.sceneBounds;

    if (auto *m = dynamic_cast<CPolygonMesh *>(obj)) {
        tessPolygon(m->pl->data0, m->npoly, m->nholes, m->nvertices, m->vertices,
                    m->xform->from, V, B);

    } else if (auto *m = dynamic_cast<CPatchMesh *>(obj)) {
        tessPatch(m->pl->data0, m->uVertices, m->vVertices, m->xform->from, V, B);

    } else if (auto *m = dynamic_cast<CNURBSPatchMesh *>(obj)) {
        tessNurbs(m->pl->data0, m->uVertices, m->vVertices, m->xform->from, V, B);

    } else if (auto *s = dynamic_cast<CSphere *>(obj)) {
        tessQuadricSphere(s->r, s->umax, s->vmin, s->vmax, s->xform->from, V, B);

    } else if (auto *d = dynamic_cast<CDisk *>(obj)) {
        tessQuadricDisk(d->r, d->z, d->umax, d->xform->from, V, B);

    } else if (auto *c = dynamic_cast<CCone *>(obj)) {
        tessQuadricCone(c->r, c->height, c->umax, c->xform->from, V, B);

    } else if (auto *c = dynamic_cast<CCylinder *>(obj)) {
        tessQuadricCylinder(c->r, c->zmin, c->zmax, c->umax, c->xform->from, V, B);

    } else if (auto *p = dynamic_cast<CParaboloid *>(obj)) {
        tessQuadricParaboloid(p->r, p->zmin, p->zmax, p->umax, p->xform->from, V, B);

    } else if (auto *h = dynamic_cast<CHyperboloid *>(obj)) {
        tessQuadricHyperboloid(h->p1, h->p2, h->umax, h->xform->from, V, B);

    } else if (auto *t = dynamic_cast<CToroid *>(obj)) {
        tessQuadricToroid(t->rmax, t->rmin, t->vmin, t->vmax, t->umax, t->xform->from, V, B);

    } else if (auto *c = dynamic_cast<CCurveMesh *>(obj)) {
        tessCurve(c->pl->data0, c->numCurves, c->nverts,
                  c->wrap != 0, c->xform->from, V, B);

    } else if (auto *p = dynamic_cast<CPoints *>(obj)) {
        tessPoints(p->pl->data0, p->numPoints, p->xform->from, V, B);

    } else if (auto *s = dynamic_cast<CSubdivMesh *>(obj)) {
        tessSubdivision(s->pl->data0, s->numFaces,
                        s->numVerticesPerFace, s->vertexIndices,
                        s->xform->from, V, B);

    } else if (auto *d = dynamic_cast<CDelayedObject *>(obj)) {
        tessProc(d->bmin, d->bmax, V, B);

    } else {
        fprintf(stderr, "orender-wire: unknown primitive type, skipped\n");
    }

    delete obj;
}

// ─── Camera extraction ────────────────────────────────────────────────────────

void CPreviewContext::extractCamera() {
    PreviewCamera &cam = result_.camera;

    // Transpose CRenderer::fromWorld (column-major) into row-major viewMatrix.
    const float *fw = CRenderer::fromWorld;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            cam.viewMatrix[r*4+c] = fw[r + c*4];

    cam.nearPlane        = CRenderer::clipMin;
    cam.farPlane         = CRenderer::clipMax;
    cam.frameAspectRatio = CRenderer::frameAR;
    cam.fov              = CRenderer::fov;

    cam.projectionType = (CRenderer::projection == OPTIONS_PROJECTION_PERSPECTIVE)
                         ? ProjectionType::Perspective
                         : ProjectionType::Orthographic;

    // Build row-major projection matrix.
    float *proj = cam.projMatrix;
    for (int i = 0; i < 16; i++) proj[i] = 0.0f;

    float n = cam.nearPlane, f = cam.farPlane;
    if (cam.projectionType == ProjectionType::Perspective) {
        float fv = 1.0f / tanf(cam.fov * 3.14159265358979f / 360.0f);
        proj[0]  = (cam.frameAspectRatio > 0.0f) ? fv / cam.frameAspectRatio : fv;
        proj[5]  = fv;
        proj[10] = (f + n) / (n - f);
        proj[11] = (2.0f * f * n) / (n - f);
        proj[14] = -1.0f;
    } else {
        proj[0] = proj[5] = proj[10] = proj[15] = 1.0f;
    }
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
    RiSetContextFactory([]() -> CRendererContext * { return new CPreviewContext(); });
    RiBegin(NULL);
    ribParse(ribPath, NULL);

    // Capture scene BEFORE RiEnd deletes the context.
    CPreviewContext *ctx = static_cast<CPreviewContext *>(renderMan);
    PreviewScene scene   = std::move(ctx->result_);

    RiEnd();
    RiSetContextFactory(nullptr);

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
        for (int i = 0; i < nVerts; i++) {
            out->vertices[i*3+0] = scene.vertices[i].x;
            out->vertices[i*3+1] = scene.vertices[i].y;
            out->vertices[i*3+2] = scene.vertices[i].z;
        }
    } else {
        out->vertices = nullptr;
    }

    // Camera: transpose row-major (C++) → column-major (C ABI / Metal / GL).
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++) {
            out->camera.projMatrix[c*4+r] = cam.projMatrix[r*4+c];
            out->camera.viewMatrix[c*4+r] = cam.viewMatrix[r*4+c];
        }
    out->camera.nearPlane      = cam.nearPlane;
    out->camera.farPlane       = cam.farPlane;
    out->camera.projectionType = (cam.projectionType == ProjectionType::Perspective) ? 0 : 1;

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
    delete scene;
}
