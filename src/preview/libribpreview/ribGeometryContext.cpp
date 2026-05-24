#include "ribGeometryContext.h"

#include "rendererContext.h"   // for CDelayedInstance, addInstance pattern
#include "polygons.h"
#include "patches.h"
#include "quadrics.h"
#include "curves.h"
#include "points.h"
#include "subdivisionCreator.h"
#include "delayed.h"
#include "pl.h"
#include "rib.h"
#include "renderer.h"
#include "ri.h"

#include <vector>
#include <string>
#include <sstream>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ─────────────────────────────────────────────────────────────────────────────
// Construction / destruction
// ─────────────────────────────────────────────────────────────────────────────

CRibGeometryContext::CRibGeometryContext() {
    currentXform_ = new CXform();
    currentXform_->attach();

    currentAttributes_ = new CAttributes();
    currentAttributes_->attach();

    memset(&cameraState, 0, sizeof(cameraState));
    // Identity matrices
    for (int i = 0; i < 4; i++) {
        cameraState.camToWorld[i*4+i] = 1.0f;
        cameraState.viewMatrix[i*4+i] = 1.0f;
    }
    cameraState.projectionType = 0;
    cameraState.fov        = 90.0f;
    cameraState.nearPlane  = 0.001f;
    cameraState.farPlane   = 1e5f;
    cameraState.imagePlane = 1.0f;
}

CRibGeometryContext::~CRibGeometryContext() {
    // Release instance objects (attached in RiObjectEnd)
    for (auto *inst : allocatedInstances_) {
        CObject *obj = inst->objects;
        while (obj) {
            CObject *next = obj->sibling;
            obj->detach();
            obj = next;
        }
        delete inst;
    }
    // Clean up any instance stack entries (malformed RIB)
    for (auto *inst : instanceStack_) {
        if (inst) {
            CObject *obj = inst->objects;
            while (obj) {
                CObject *next = obj->sibling;
                obj->detach();
                obj = next;
            }
            delete inst;
        }
    }
    // Release xform stack entries
    for (auto *xf : xformStack_) xf->detach();
    for (auto *at : attrStack_)  at->detach();
    // Release current
    currentXform_->detach();
    currentAttributes_->detach();
}

// ─────────────────────────────────────────────────────────────────────────────
// emitObject — routes to instance list or virtual addObject()
// ─────────────────────────────────────────────────────────────────────────────

void CRibGeometryContext::emitObject(CObject *o) {
    if (!o) return;
    if (currentInstance_) {
        o->sibling = currentInstance_->objects;
        currentInstance_->objects = o;
    } else {
        addObject(o);
    }
}

// ─────────────────────────────────────────────────────────────────────────────
// Camera / frame options
// ─────────────────────────────────────────────────────────────────────────────

void CRibGeometryContext::RiFormat(int xres, int yres, float aspect) {
    if (xres <= 0 || yres <= 0) return;
    options_.xres = xres;
    options_.yres = yres;
    if (aspect > 0) options_.pixelAR = aspect;
    // Recompute frameAR from resolution if not explicitly set
    if (!options_.frameARSet)
        options_.frameAspectRatio = (float)xres * options_.pixelAR / (float)yres;
}

void CRibGeometryContext::RiFrameAspectRatio(float aspect) {
    if (aspect == 0) return;
    options_.frameAspectRatio = aspect;
    options_.frameARSet = true;
}

void CRibGeometryContext::RiScreenWindow(float left, float right, float bot, float top) {
    options_.screenLeft      = left;
    options_.screenRight     = right;
    options_.screenBottom    = bot;
    options_.screenTop       = top;
    options_.screenWindowSet = true;
}

void CRibGeometryContext::RiProjectionV(const char *name, int n, const char *tokens[], const void *params[]) {
    if (strcmp(name, RI_PERSPECTIVE) == 0) {
        options_.projection = OPTIONS_PROJECTION_PERSPECTIVE;
        for (int i = 0; i < n; i++) {
            if (strcmp(tokens[i], RI_FOV) == 0)
                options_.fov = ((const float *)params[i])[0];
        }
    } else if (strcmp(name, RI_ORTHOGRAPHIC) == 0) {
        options_.projection = OPTIONS_PROJECTION_ORTHOGRAPHIC;
    }
}

void CRibGeometryContext::RiClipping(float hither, float yon) {
    if (hither > 0) options_.nearPlane = hither;
    if (yon > hither) options_.farPlane = yon;
}

// ─────────────────────────────────────────────────────────────────────────────
// Transform operations
// ─────────────────────────────────────────────────────────────────────────────

void CRibGeometryContext::RiIdentity() {
    currentXform_->identity();
}

void CRibGeometryContext::RiTransform(float m[4][4]) {
    // m is row-major [row][col]; column-major element(c,r) = c*4+r
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            currentXform_->from[r + c*4] = m[r][c];
    if (invertm(currentXform_->to, currentXform_->from))
        identitym(currentXform_->to);
    currentXform_->flip = -1;  // force recalculation
}

void CRibGeometryContext::RiConcatTransform(float m[4][4]) {
    CXform tmp;
    for (int r = 0; r < 4; r++)
        for (int c = 0; c < 4; c++)
            tmp.from[r + c*4] = m[r][c];
    if (invertm(tmp.to, tmp.from))
        identitym(tmp.to);
    currentXform_->concat(&tmp);
}

void CRibGeometryContext::RiPerspective(float fov) {
    // Perspective is handled via RiProjectionV — this adds a perspective xform
    // as a concat (matches full renderer behavior for camera setup)
    float s = (float)(1.0 / tan((double)fov * 0.5 * M_PI / 180.0));
    // Build a perspective matrix that scales the X/Y by s (purely for xform concat)
    CXform tmp;
    identitym(tmp.from);
    tmp.from[element(0,0)] = s;
    tmp.from[element(1,1)] = s;
    if (invertm(tmp.to, tmp.from))
        identitym(tmp.to);
    currentXform_->concat(&tmp);
}

void CRibGeometryContext::RiTranslate(float dx, float dy, float dz) {
    currentXform_->translate(dx, dy, dz);
}

void CRibGeometryContext::RiRotate(float angle, float ax, float ay, float az) {
    currentXform_->rotate(angle, ax, ay, az);
}

void CRibGeometryContext::RiScale(float sx, float sy, float sz) {
    currentXform_->scale(sx, sy, sz);
}

void CRibGeometryContext::RiSkew(float angle,
    float dx1, float dy1, float dz1,
    float dx2, float dy2, float dz2) {
    currentXform_->skew(angle, dx1, dy1, dz1, dx2, dy2, dz2);
}

void CRibGeometryContext::RiTransformBegin() {
    xformStack_.push_back(currentXform_);
    currentXform_ = new CXform(currentXform_);
    currentXform_->attach();
}

void CRibGeometryContext::RiTransformEnd() {
    if (xformStack_.empty()) return;
    currentXform_->detach();
    currentXform_ = xformStack_.back();
    xformStack_.pop_back();
}

// ─────────────────────────────────────────────────────────────────────────────
// Attribute operations
// ─────────────────────────────────────────────────────────────────────────────

void CRibGeometryContext::RiAttributeBegin() {
    // Push xform and attributes simultaneously (RIB semantics)
    xformStack_.push_back(currentXform_);
    currentXform_ = new CXform(currentXform_);
    currentXform_->attach();

    attrStack_.push_back(currentAttributes_);
    currentAttributes_ = new CAttributes(currentAttributes_);
    currentAttributes_->attach();
}

void CRibGeometryContext::RiAttributeEnd() {
    if (!xformStack_.empty()) {
        currentXform_->detach();
        currentXform_ = xformStack_.back();
        xformStack_.pop_back();
    }
    if (!attrStack_.empty()) {
        currentAttributes_->detach();
        currentAttributes_ = attrStack_.back();
        attrStack_.pop_back();
    }
}

void CRibGeometryContext::RiColor(float *Cs) {
    if (!Cs) return;
    currentAttributes_->surfaceColor[0] = Cs[0];
    currentAttributes_->surfaceColor[1] = Cs[1];
    currentAttributes_->surfaceColor[2] = Cs[2];
}

void CRibGeometryContext::RiSides(int nsides) {
    if (nsides == 1)
        currentAttributes_->flags &= ~ATTRIBUTES_FLAGS_DOUBLE_SIDED;
    else if (nsides == 2)
        currentAttributes_->flags |= ATTRIBUTES_FLAGS_DOUBLE_SIDED;
}

void CRibGeometryContext::RiBasis(float ubasis[4][4], int ustep, float vbasis[4][4], int vstep) {
    currentAttributes_->uStep = ustep;
    currentAttributes_->vStep = vstep;
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            currentAttributes_->uBasis[element(i, j)] = ubasis[j][i];
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++)
            currentAttributes_->vBasis[element(i, j)] = vbasis[j][i];
}

void CRibGeometryContext::RiOrientation(const char *orientation) {
    if (!orientation) return;
    if (strcmp(orientation, RI_OUTSIDE) == 0 || strcmp(orientation, RI_LH) == 0)
        currentAttributes_->flags &= ~ATTRIBUTES_FLAGS_INSIDE;
    else if (strcmp(orientation, RI_INSIDE) == 0 || strcmp(orientation, RI_RH) == 0)
        currentAttributes_->flags |= ATTRIBUTES_FLAGS_INSIDE;
}

void CRibGeometryContext::RiReverseOrientation() {
    currentAttributes_->flags ^= ATTRIBUTES_FLAGS_INSIDE;
}

// ─────────────────────────────────────────────────────────────────────────────
// World begin — capture camera state
// ─────────────────────────────────────────────────────────────────────────────

void CRibGeometryContext::RiWorldBegin() {
    // Apply default screen window from aspect ratio if not explicitly set
    if (!options_.screenWindowSet) {
        float ar = options_.frameAspectRatio;
        if (ar >= 1.0f) {
            options_.screenLeft   = -ar;
            options_.screenRight  =  ar;
            options_.screenTop    =  1.0f;
            options_.screenBottom = -1.0f;
        } else {
            options_.screenLeft   = -1.0f;
            options_.screenRight  =  1.0f;
            options_.screenTop    =  1.0f / ar;
            options_.screenBottom = -1.0f / ar;
        }
    }

    // Compute imagePlane (= 1/tan(fov/2) for perspective)
    float imagePlane;
    if (options_.projection == OPTIONS_PROJECTION_PERSPECTIVE)
        imagePlane = 1.0f / tanf((float)M_PI / 180.0f * options_.fov * 0.5f);
    else
        imagePlane = 1.0f;

    // currentXform_->from = camera→world, currentXform_->to = world→camera
    memcpy(cameraState.camToWorld, currentXform_->from, 16 * sizeof(float));
    memcpy(cameraState.viewMatrix,  currentXform_->to,  16 * sizeof(float));

    cameraState.imagePlane       = imagePlane;
    cameraState.projectionType   = (options_.projection == OPTIONS_PROJECTION_PERSPECTIVE) ? 0 : 1;
    cameraState.fov              = options_.fov;
    cameraState.nearPlane        = options_.nearPlane;
    cameraState.farPlane         = options_.farPlane;
    cameraState.screenLeft       = options_.screenLeft;
    cameraState.screenRight      = options_.screenRight;
    cameraState.screenTop        = options_.screenTop;
    cameraState.screenBottom     = options_.screenBottom;
    cameraState.frameAspectRatio = options_.frameAspectRatio;

    // Build projection matrix (row-major, Metal NDC: Z ∈ [0,1])
    float *proj = cameraState.projMatrix;
    memset(proj, 0, 16 * sizeof(float));
    float sw_half_x = (options_.screenRight - options_.screenLeft)   * 0.5f;
    float sw_half_y = (options_.screenTop   - options_.screenBottom) * 0.5f;
    float sw_cx     = (options_.screenLeft  + options_.screenRight)  * 0.5f;
    float sw_cy     = (options_.screenBottom + options_.screenTop)   * 0.5f;
    float n = options_.nearPlane, f = options_.farPlane;
    if (cameraState.projectionType == 0) {
        proj[0]  = imagePlane / sw_half_x;
        proj[2]  = -sw_cx / sw_half_x;
        proj[5]  = imagePlane / sw_half_y;
        proj[6]  = -sw_cy / sw_half_y;
        proj[10] = f / (f - n);
        proj[11] = (f * n) / (n - f);
        proj[14] = 1.0f;
    } else {
        proj[0]  =  1.0f / sw_half_x;
        proj[3]  = -sw_cx / sw_half_x;
        proj[5]  =  1.0f / sw_half_y;
        proj[7]  = -sw_cy / sw_half_y;
        proj[10] =  1.0f / (f - n);
        proj[11] = -n / (f - n);
        proj[15] =  1.0f;
    }

    // Reset xform to identity — geometry is in world space after WorldBegin
    currentXform_->identity();
}

// ─────────────────────────────────────────────────────────────────────────────
// Object instances
// ─────────────────────────────────────────────────────────────────────────────

void *CRibGeometryContext::RiObjectBegin() {
    // Push xform to identity (objects defined in local space)
    xformStack_.push_back(currentXform_);
    currentXform_ = new CXform();
    currentXform_->attach();

    // Push and create new instance record
    instanceStack_.push_back(currentInstance_);
    currentInstance_ = new InstanceRecord;
    return currentInstance_;
}

void CRibGeometryContext::RiObjectEnd() {
    if (!currentInstance_) return;

    // Attach all accumulated objects so they survive the scope
    CObject *obj = currentInstance_->objects;
    while (obj) {
        obj->attach();
        obj = obj->sibling;
    }

    allocatedInstances_.push_back(currentInstance_);

    // Restore previous instance and xform
    if (!instanceStack_.empty()) {
        currentInstance_ = instanceStack_.back();
        instanceStack_.pop_back();
    } else {
        currentInstance_ = nullptr;
    }

    if (!xformStack_.empty()) {
        currentXform_->detach();
        currentXform_ = xformStack_.back();
        xformStack_.pop_back();
    }
}

void CRibGeometryContext::RiObjectInstance(const void *handle) {
    if (!handle) return;
    const InstanceRecord *rec = static_cast<const InstanceRecord *>(handle);
    if (!rec->objects) return;
    emitObject(new CDelayedInstance(currentAttributes_, currentXform_, rec->objects));
}

// ─────────────────────────────────────────────────────────────────────────────
// Motion blocks (blur ignored; track depth only)
// ─────────────────────────────────────────────────────────────────────────────

void CRibGeometryContext::RiMotionBeginV(int /*n*/, float * /*times*/) {
    motionBlockDepth_++;
    motionGeomConsumed_ = false;
}

void CRibGeometryContext::RiMotionEnd() {
    if (motionBlockDepth_ > 0) motionBlockDepth_--;
    motionGeomConsumed_ = false;
}

// Helper: return false and skip if we're in a motion block and already have a sample
static inline bool shouldProcess(int depth, bool &consumed) {
    if (depth == 0) return true;
    if (consumed) return false;
    consumed = true;
    return true;
}

// ─────────────────────────────────────────────────────────────────────────────
// Geometry implementations
// ─────────────────────────────────────────────────────────────────────────────

void CRibGeometryContext::RiPolygonV(int nvertices, int n, const char *tokens[], const void *params[]) {
    if (!shouldProcess(motionBlockDepth_, motionGeomConsumed_)) return;
    CPl *pl = parseParameterList(1, nvertices, 0, nvertices, n, tokens, params,
                                  RI_P, PL_VARYING_TO_VERTEX, currentAttributes_);
    if (!pl) return;
    // Build sequential vertex indices [0, 1, ..., nvertices-1]
    std::vector<int> verts(nvertices);
    for (int i = 0; i < nvertices; i++) verts[i] = i;
    int one = 1;
    emitObject(new CPolygonMesh(currentAttributes_, currentXform_, pl, 1, &one, &nvertices, verts.data()));
}

void CRibGeometryContext::RiGeneralPolygonV(int nloops, int *nverts, int n, const char *tokens[], const void *params[]) {
    if (!shouldProcess(motionBlockDepth_, motionGeomConsumed_)) return;
    int nvertices = 0;
    for (int i = 0; i < nloops; i++) nvertices += nverts[i];
    CPl *pl = parseParameterList(1, nvertices, 0, nvertices, n, tokens, params,
                                  RI_P, PL_VARYING_TO_VERTEX, currentAttributes_);
    if (!pl) return;
    std::vector<int> verts(nvertices);
    for (int i = 0; i < nvertices; i++) verts[i] = i;
    emitObject(new CPolygonMesh(currentAttributes_, currentXform_, pl, 1, &nloops, nverts, verts.data()));
}

void CRibGeometryContext::RiPointsPolygonsV(int npolys, int *nverts, int *verts,
                                             int n, const char *tokens[], const void *params[]) {
    if (!shouldProcess(motionBlockDepth_, motionGeomConsumed_)) return;
    int nvertices = 0, mvertex = 0;
    for (int i = 0; i < npolys; i++) nvertices += nverts[i];
    for (int i = 0; i < nvertices; i++) if (verts[i] > mvertex) mvertex = verts[i];
    CPl *pl = parseParameterList(npolys, mvertex + 1, 0, nvertices, n, tokens, params,
                                  RI_P, PL_VARYING_TO_VERTEX, currentAttributes_);
    if (!pl) return;
    std::vector<int> nloops(npolys, 1);
    emitObject(new CPolygonMesh(currentAttributes_, currentXform_, pl, npolys, nloops.data(), nverts, verts));
}

void CRibGeometryContext::RiPointsGeneralPolygonsV(int npolys, int *nloops, int *nverts, int *verts,
                                                    int n, const char *tokens[], const void *params[]) {
    if (!shouldProcess(motionBlockDepth_, motionGeomConsumed_)) return;
    // Count total loops, verts, and max vertex index
    int sloops = 0, sverts = 0, numVertices = 0;
    for (int i = 0; i < npolys; i++) sloops += nloops[i];
    for (int i = 0; i < sloops; i++) sverts += nverts[i];
    for (int i = 0; i < sverts; i++) if (verts[i] > numVertices) numVertices = verts[i];
    numVertices++;
    CPl *pl = parseParameterList(npolys, numVertices, 0, sverts, n, tokens, params,
                                  RI_P, PL_VARYING_TO_VERTEX, currentAttributes_);
    if (!pl) return;
    emitObject(new CPolygonMesh(currentAttributes_, currentXform_, pl, npolys, nloops, nverts, verts));
}

void CRibGeometryContext::RiPatchV(const char *type, int n, const char *tokens[], const void *params[]) {
    if (!shouldProcess(motionBlockDepth_, motionGeomConsumed_)) return;
    int uver, vver, degree;
    if (strcmp(type, RI_BILINEAR) == 0) { degree = 1; uver = 2; vver = 2; }
    else if (strcmp(type, RI_BICUBIC) == 0) { degree = 3; uver = 4; vver = 4; }
    else return;
    CPl *pl = parseParameterList(1, uver * vver, 4, 0, n, tokens, params, RI_P, 0, currentAttributes_);
    if (!pl) return;
    emitObject(new CPatchMesh(currentAttributes_, currentXform_, pl, degree, uver, vver, FALSE, FALSE));
}

void CRibGeometryContext::RiPatchMeshV(const char *type, int nu, const char *uwrap,
                                        int nv, const char *vwrap,
                                        int n, const char *tokens[], const void *params[]) {
    if (!shouldProcess(motionBlockDepth_, motionGeomConsumed_)) return;
    int uw = (strcmp(uwrap, RI_PERIODIC) == 0) ? TRUE : FALSE;
    int vw = (strcmp(vwrap, RI_PERIODIC) == 0) ? TRUE : FALSE;
    int upatches, vpatches, numVertex, numVarying, numUniform, degree;

    if (strcmp(type, RI_BICUBIC) == 0) {
        degree = 3;
        upatches = uw ? (nu / currentAttributes_->uStep)
                      : ((nu - 4) / currentAttributes_->uStep + 1);
        vpatches = vw ? (nv / currentAttributes_->vStep)
                      : ((nv - 4) / currentAttributes_->vStep + 1);
        numVertex  = nu * nv;
        numVarying = (upatches + 1 - uw) * (vpatches + 1 - vw);
        numUniform = upatches * vpatches;
    } else if (strcmp(type, RI_BILINEAR) == 0) {
        degree = 1;
        upatches = uw ? nu : nu - 1;
        vpatches = vw ? nv : nv - 1;
        numVertex  = nu * nv;
        numVarying = nu * nv;
        numUniform = upatches * vpatches;
    } else return;

    CPl *pl = parseParameterList(numUniform, numVertex, numVarying, 0,
                                  n, tokens, params, RI_P, 0, currentAttributes_);
    if (!pl) return;
    emitObject(new CPatchMesh(currentAttributes_, currentXform_, pl, degree, nu, nv, uw, vw));
}

void CRibGeometryContext::RiNuPatchV(int nu, int uorder, float *uknot,
                                      float /*umin*/, float /*umax*/,
                                      int nv, int vorder, float *vknot,
                                      float /*vmin*/, float /*vmax*/,
                                      int n, const char *tokens[], const void *params[]) {
    if (!shouldProcess(motionBlockDepth_, motionGeomConsumed_)) return;
    int upatches = nu - uorder + 1;
    int vpatches = nv - vorder + 1;
    CPl *pl = parseParameterList(upatches * vpatches, nu * nv,
                                  (upatches + 1) * (vpatches + 1), 0,
                                  n, tokens, params, RI_PW, 0, currentAttributes_);
    if (!pl) return;
    emitObject(new CNURBSPatchMesh(currentAttributes_, currentXform_, pl, nu, nv, uorder, vorder, uknot, vknot));
}

// Quadrics — no pl needed for wireframe tessellators

void CRibGeometryContext::RiSphereV(float radius, float zmin, float zmax, float tmax,
                                     int /*n*/, const char * /*tokens*/[], const void * /*params*/[]) {
    if (!shouldProcess(motionBlockDepth_, motionGeomConsumed_)) return;
    float r = fabsf(radius);
    if (r == 0 || zmin >= zmax) return;
    float vm = (zmin < -r) ? -r : zmin;
    float vx = (zmax >  r) ?  r : zmax;
    if (vm >= vx) return;
    emitObject(new CSphere(currentAttributes_, currentXform_, nullptr, 0, radius, vm, vx, tmax));
}

void CRibGeometryContext::RiConeV(float height, float radius, float tmax,
                                   int /*n*/, const char * /*tokens*/[], const void * /*params*/[]) {
    if (!shouldProcess(motionBlockDepth_, motionGeomConsumed_)) return;
    emitObject(new CCone(currentAttributes_, currentXform_, nullptr, 0, radius, height, tmax));
}

void CRibGeometryContext::RiCylinderV(float radius, float zmin, float zmax, float tmax,
                                       int /*n*/, const char * /*tokens*/[], const void * /*params*/[]) {
    if (!shouldProcess(motionBlockDepth_, motionGeomConsumed_)) return;
    emitObject(new CCylinder(currentAttributes_, currentXform_, nullptr, 0, radius, zmin, zmax, tmax));
}

void CRibGeometryContext::RiParaboloidV(float rmax, float zmin, float zmax, float tmax,
                                         int /*n*/, const char * /*tokens*/[], const void * /*params*/[]) {
    if (!shouldProcess(motionBlockDepth_, motionGeomConsumed_)) return;
    emitObject(new CParaboloid(currentAttributes_, currentXform_, nullptr, 0, rmax, zmin, zmax, tmax));
}

void CRibGeometryContext::RiDiskV(float height, float radius, float tmax,
                                   int /*n*/, const char * /*tokens*/[], const void * /*params*/[]) {
    if (!shouldProcess(motionBlockDepth_, motionGeomConsumed_)) return;
    emitObject(new CDisk(currentAttributes_, currentXform_, nullptr, 0, radius, height, tmax));
}

void CRibGeometryContext::RiTorusV(float rmajor, float rminor, float phimin, float phimax, float tmax,
                                    int /*n*/, const char * /*tokens*/[], const void * /*params*/[]) {
    if (!shouldProcess(motionBlockDepth_, motionGeomConsumed_)) return;
    emitObject(new CToroid(currentAttributes_, currentXform_, nullptr, 0, rmajor, rminor, phimin, phimax, tmax));
}

void CRibGeometryContext::RiHyperboloidV(float *point1, float *point2, float tmax,
                                          int /*n*/, const char * /*tokens*/[], const void * /*params*/[]) {
    if (!shouldProcess(motionBlockDepth_, motionGeomConsumed_)) return;
    emitObject(new CHyperboloid(currentAttributes_, currentXform_, nullptr, 0, point1, point2, tmax));
}

void CRibGeometryContext::RiCurvesV(const char *d, int ncurves, int nverts[], const char *w,
                                     int n, const char *tokens[], const void *params[]) {
    if (!shouldProcess(motionBlockDepth_, motionGeomConsumed_)) return;
    int numVertices = 0, numVaryings = 0;
    int degree, wrap;

    if (strcmp(d, RI_LINEAR) == 0) {
        degree = 1;
        wrap = (strcmp(w, RI_PERIODIC) == 0) ? TRUE : FALSE;
        for (int i = 0; i < ncurves; i++) {
            numVertices += nverts[i];
            numVaryings += nverts[i];
        }
    } else if (strcmp(d, RI_CUBIC) == 0) {
        degree = 3;
        wrap = (strcmp(w, RI_PERIODIC) == 0) ? TRUE : FALSE;
        for (int i = 0; i < ncurves; i++) {
            numVertices += nverts[i];
            if (wrap) {
                numVaryings += (nverts[i] - 4) / currentAttributes_->vStep + 1;
            } else {
                numVaryings += (nverts[i] - 4) / currentAttributes_->vStep + 2;
            }
        }
    } else return;

    CPl *pl = parseParameterList(ncurves, numVertices, numVaryings, 0,
                                  n, tokens, params, RI_P, 0, currentAttributes_);
    if (!pl) return;
    emitObject(new CCurveMesh(currentAttributes_, currentXform_, pl, degree, numVertices, ncurves, nverts, wrap));
}

void CRibGeometryContext::RiPointsV(int npoints, int n, const char *tokens[], const void *params[]) {
    if (!shouldProcess(motionBlockDepth_, motionGeomConsumed_)) return;
    CPl *pl = parseParameterList(1, npoints, 0, 0, n, tokens, params,
                                  RI_P, PL_VARYING_TO_VERTEX, currentAttributes_);
    if (!pl) return;
    emitObject(new CPoints(currentAttributes_, currentXform_, pl, npoints));
}

void CRibGeometryContext::RiSubdivisionMeshV(const char *scheme, int nfaces,
    int nvertices[], int vertices[], int ntags, const char *tags[],
    int nargs[], int intargs[], float floatargs[],
    int n, const char *tokens[], const void *params[]) {
    if (!shouldProcess(motionBlockDepth_, motionGeomConsumed_)) return;
    if (strcmp(scheme, RI_CATMULLCLARK) != 0) return;

    int j = 0;
    for (int i = 0; i < nfaces; j += nvertices[i], i++) {}
    int numVertices = -1;
    for (int i = 0; i < j; i++) if (vertices[i] > numVertices) numVertices = vertices[i];
    numVertices++;

    CPl *pl = parseParameterList(nfaces, numVertices, numVertices, j,
                                  n, tokens, params, RI_P, PL_VARYING_TO_VERTEX, currentAttributes_);
    if (!pl) return;
    emitObject(new CSubdivMesh(currentAttributes_, currentXform_, pl, nfaces, nvertices, vertices,
                                ntags, tags, nargs, intargs, floatargs));
}

// ─────────────────────────────────────────────────────────────────────────────
// Procedurals (represented as bounding boxes)
// ─────────────────────────────────────────────────────────────────────────────

void CRibGeometryContext::RiProcedural(void *data, float *bound,
    void (*subdivfunc)(void *, float), void (*freefunc)(void *)) {
    if (!shouldProcess(motionBlockDepth_, motionGeomConsumed_)) return;
    if (!bound) return;
    float bmin[3] = { bound[0], bound[2], bound[4] };
    float bmax[3] = { bound[1], bound[3], bound[5] };
    emitObject(new CDelayedObject(currentAttributes_, currentXform_,
                                   bmin, bmax, subdivfunc, freefunc, data));
}

// ─────────────────────────────────────────────────────────────────────────────
// Named geometry from GEOMETRIES search path
// ─────────────────────────────────────────────────────────────────────────────

// Mirrors CRendererContext::loadAndExecuteNamedGeometry but uses mkstemp()
// instead of CRenderer::temporaryPath.
static bool lineStartsWithToken(const char *line, const char *token) {
    while (*line == ' ' || *line == '\t') line++;
    size_t tlen = strlen(token);
    if (strncmp(line, token, tlen) != 0) return false;
    return (line[tlen] == ' ' || line[tlen] == '\t' || line[tlen] == '\r' || line[tlen] == '\n' || line[tlen] == '\0');
}

static bool lineIsObjectBeginWithName(const char *line, const char *objectName) {
    while (*line == ' ' || *line == '\t') line++;
    if (strncmp(line, "ObjectBegin", 11) != 0) return false;
    const char *p = line + 11;
    while (*p == ' ' || *p == '\t') p++;
    if (*p == '"') p++;
    size_t nlen = strlen(objectName);
    if (strncmp(p, objectName, nlen) != 0) return false;
    p += nlen;
    return (*p == '"' || *p == '\r' || *p == '\n' || *p == '\0');
}

void CRibGeometryContext::loadNamedGeometry(const char *filename, const char *objectName) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        fprintf(stderr, "orender-wire: could not open geometry file: %s\n", filename);
        return;
    }

    // Create a temp file for the extracted geometry block
    char tmpPath[] = "/tmp/orender-wire-geom-XXXXXX";
    int fd = mkstemp(tmpPath);
    if (fd < 0) {
        fclose(file);
        fprintf(stderr, "orender-wire: could not create temp file for geometry\n");
        return;
    }
    FILE *out = fdopen(fd, "w");
    if (!out) {
        close(fd);
        fclose(file);
        return;
    }

    char line[4096];
    int depth = 0, found = 0, collecting = 0;
    while (fgets(line, (int)sizeof(line), file)) {
        if (!collecting) {
            if (lineIsObjectBeginWithName(line, objectName)) {
                found = 1;
                collecting = 1;
                depth = 1;
            }
            continue;
        }
        if (lineStartsWithToken(line, "ObjectBegin")) { depth++; fprintf(out, "%s", line); continue; }
        if (lineStartsWithToken(line, "ObjectEnd")) {
            depth--;
            if (depth == 0) {
                fclose(out);
                fclose(file);
                ribParse(tmpPath, nullptr);
                remove(tmpPath);
                return;
            }
            fprintf(out, "%s", line);
            continue;
        }
        fprintf(out, "%s", line);
    }
    fclose(out);
    fclose(file);
    remove(tmpPath);
    if (!found)
        fprintf(stderr, "orender-wire: geometry '%s' not found in '%s'\n", objectName, filename);
}

void CRibGeometryContext::RiGeometryV(const char *type, int /*n*/,
                                       const char * /*tokens*/[], const void * /*params*/[]) {
    if (!shouldProcess(motionBlockDepth_, motionGeomConsumed_)) return;

    // Only RIB-based named geometry is supported (not "implicit" or "dlo" subtypes)
    if (strcmp(type, "implicit") == 0 || strcmp(type, "dlo") == 0) {
        fprintf(stderr, "orender-wire: geometry type '%s' not supported (requires ray-tracing), skipped\n", type);
        return;
    }

    const char *geomEnv = getenv("GEOMETRIES");
    if (!geomEnv) {
        fprintf(stderr, "orender-wire: GEOMETRIES not set, geometry '%s' skipped\n", type);
        return;
    }

    // Walk colon-separated search path (prepend "." so current dir is tried first)
    std::string searchStr = std::string(".") + ":" + geomEnv;
    std::istringstream ss(searchStr);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        if (dir.empty()) continue;
        char path[4096];
        snprintf(path, sizeof(path), "%s/%s.rib", dir.c_str(), type);
        FILE *f = fopen(path, "r");
        if (!f) continue;
        fclose(f);
        loadNamedGeometry(path, type);
        return;
    }
    fprintf(stderr, "orender-wire: geometry '%s' not found in GEOMETRIES path, skipped\n", type);
}

// ─────────────────────────────────────────────────────────────────────────────
// Archive inclusion
// ─────────────────────────────────────────────────────────────────────────────

void CRibGeometryContext::RiReadArchiveV(const char *filename,
                                          void (* /*callback*/)(const char *, ...),
                                          int /*n*/, const char * /*tokens*/[], const void * /*params*/[]) {
    if (filename && *filename)
        ribParse(filename, nullptr);
}
