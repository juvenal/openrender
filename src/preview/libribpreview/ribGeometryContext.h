#pragma once

#include <vector>
#include <cmath>
#include <cstring>

#include "riInterface.h"
#include "object.h"
#include "xform.h"
#include "attributes.h"
#include "options.h"

// Lightweight RIB geometry context.
//
// Implements transform/attribute stacks, camera options, and all geometry Ri
// methods using the same geometry classes as the full renderer — but with zero
// rendering initialization (no display plugins, no shader loading, no network).
//
// Subclasses override addObject() to consume individual geometry primitives.
// emitObject() is the internal dispatcher: when inside ObjectBegin/End it
// accumulates objects in the instance record; otherwise it calls addObject().

class CRibGeometryContext : public CRiInterface {
public:
    CRibGeometryContext();
    ~CRibGeometryContext() override;

    // Subclass provides geometry consumption (pure virtual)
    virtual void addObject(CObject *) override = 0;

    // ── Camera state captured at RiWorldBegin ──────────────────────────────
    struct CameraState {
        float camToWorld[16];  // camera→world (column-major)
        float viewMatrix[16];  // world→camera (column-major)
        float projMatrix[16];  // projection matrix (row-major, Metal NDC)
        float imagePlane;      // 1/tan(fov/2) for perspective, 1 for ortho
        int   projectionType;  // 0=perspective, 1=orthographic
        float fov;
        float nearPlane, farPlane;
        float screenLeft, screenRight, screenTop, screenBottom;
        float frameAspectRatio;
    } cameraState;

    // ── Camera / frame ──────────────────────────────────────────────────────
    void RiFrameBegin(int) override {}
    void RiFrameEnd() override {}
    void RiFormat(int xres, int yres, float aspect) override;
    void RiFrameAspectRatio(float aspect) override;
    void RiScreenWindow(float left, float right, float bot, float top) override;
    void RiProjectionV(const char *name, int n, const char *tokens[], const void *params[]) override;
    void RiClipping(float hither, float yon) override;

    // ── Transforms ─────────────────────────────────────────────────────────
    void RiIdentity() override;
    void RiTransform(float m[4][4]) override;
    void RiConcatTransform(float m[4][4]) override;
    void RiPerspective(float fov) override;
    void RiTranslate(float dx, float dy, float dz) override;
    void RiRotate(float angle, float ax, float ay, float az) override;
    void RiScale(float sx, float sy, float sz) override;
    void RiSkew(float angle, float dx1, float dy1, float dz1,
                float dx2, float dy2, float dz2) override;
    void RiCoordinateSystem(const char *) override {}
    void RiCoordSysTransform(const char *) override {}
    void RiTransformBegin() override;
    void RiTransformEnd() override;

    // ── Attributes ─────────────────────────────────────────────────────────
    void RiAttributeBegin() override;
    void RiAttributeEnd() override;
    void RiColor(float *Cs) override;
    void RiOpacity(float *) override {}
    void RiSides(int nsides) override;
    void RiBasis(float ubasis[4][4], int ustep, float vbasis[4][4], int vstep) override;
    void RiOrientation(const char *orientation) override;
    void RiReverseOrientation() override;

    // ── World ───────────────────────────────────────────────────────────────
    void RiWorldBegin() override;
    void RiWorldEnd() override {}

    // ── Object instances ───────────────────────────────────────────────────
    void *RiObjectBegin() override;
    void  RiObjectEnd() override;
    void  RiObjectInstance(const void *handle) override;

    // ── Motion (blur ignored — geometry in motion blocks: first sample only)
    void RiMotionBeginV(int n, float times[]) override;
    void RiMotionEnd() override;

    // ── Geometry ────────────────────────────────────────────────────────────
    void RiPolygonV(int nvertices, int n, const char *tokens[], const void *params[]) override;
    void RiGeneralPolygonV(int nloops, int *nvertices, int n, const char *tokens[], const void *params[]) override;
    void RiPointsPolygonsV(int npolys, int *nvertices, int *vertices, int n, const char *tokens[], const void *params[]) override;
    void RiPointsGeneralPolygonsV(int npolys, int *nloops, int *nvertices, int *vertices,
                                   int n, const char *tokens[], const void *params[]) override;
    void RiPatchV(const char *type, int n, const char *tokens[], const void *params[]) override;
    void RiPatchMeshV(const char *type, int nu, const char *uwrap,
                      int nv, const char *vwrap, int n, const char *tokens[], const void *params[]) override;
    void RiNuPatchV(int nu, int uorder, float *uknot, float umin, float umax,
                    int nv, int vorder, float *vknot, float vmin, float vmax,
                    int n, const char *tokens[], const void *params[]) override;
    void RiTrimCurve(int, int *, int *, float *, float *, float *,
                     int *, float *, float *, float *) override {}
    void RiSphereV(float radius, float zmin, float zmax, float tmax,
                   int n, const char *tokens[], const void *params[]) override;
    void RiConeV(float height, float radius, float tmax,
                 int n, const char *tokens[], const void *params[]) override;
    void RiCylinderV(float radius, float zmin, float zmax, float tmax,
                     int n, const char *tokens[], const void *params[]) override;
    void RiParaboloidV(float rmax, float zmin, float zmax, float tmax,
                       int n, const char *tokens[], const void *params[]) override;
    void RiDiskV(float height, float radius, float tmax,
                 int n, const char *tokens[], const void *params[]) override;
    void RiTorusV(float rmajor, float rminor, float phimin, float phimax, float tmax,
                  int n, const char *tokens[], const void *params[]) override;
    void RiHyperboloidV(float *point1, float *point2, float tmax,
                        int n, const char *tokens[], const void *params[]) override;
    void RiCurvesV(const char *type, int ncurves, int nverts[], const char *wrap,
                   int n, const char *tokens[], const void *params[]) override;
    void RiPointsV(int npoints, int n, const char *tokens[], const void *params[]) override;
    void RiSubdivisionMeshV(const char *scheme, int nfaces, int nvertices[], int vertices[],
                             int ntags, const char *tags[], int nargs[], int intargs[],
                             float floatargs[], int n, const char *tokens[], const void *params[]) override;
    void RiGeometryV(const char *type, int n, const char *tokens[], const void *params[]) override;
    void RiProcedural(void *data, float *bound,
                      void (*subdivfunc)(void *, float), void (*freefunc)(void *)) override;
    void RiReadArchiveV(const char *filename, void (*callback)(const char *, ...),
                        int n, const char *tokens[], const void *params[]) override;

protected:
    // Internal dispatcher: routes to instance list or addObject()
    void emitObject(CObject *o);

    struct InstanceRecord {
        CObject *objects = nullptr;
    };
    InstanceRecord *currentInstance_ = nullptr;

private:
    // Transform stack
    std::vector<CXform *>      xformStack_;
    CXform                    *currentXform_  = nullptr;

    // Attribute stack
    std::vector<CAttributes *> attrStack_;
    CAttributes               *currentAttributes_ = nullptr;

    // Options (camera / render settings before WorldBegin)
    struct Options {
        int   projection      = OPTIONS_PROJECTION_PERSPECTIVE;
        float fov             = 90.0f;
        float nearPlane       = 0.001f;
        float farPlane        = 1e5f;
        float screenLeft      = -1.333f;
        float screenRight     =  1.333f;
        float screenTop       =  1.0f;
        float screenBottom    = -1.0f;
        float frameAspectRatio = 4.0f / 3.0f;
        bool  screenWindowSet = false;
        bool  frameARSet      = false;
        int   xres = 640, yres = 480;
        float pixelAR = 1.0f;
    } options_;

    // Object instance storage (for ObjectBegin/End)
    std::vector<InstanceRecord *> instanceStack_;
    std::vector<InstanceRecord *> allocatedInstances_;

    // Motion block tracking (first sample only for geometry)
    int  motionBlockDepth_         = 0;
    bool motionGeomConsumed_       = false;

    void loadNamedGeometry(const char *filename, const char *objectName);
};
