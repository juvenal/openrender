/**
 * Project: openRender
 *
 * File: rendererContext.h
 *
 * Description:
 *   This file defines the interface for rendererContext.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	renderer.h
//  Classes				:	CRendererContext
//  Description			:	This file defines the main renderer interface
//
////////////////////////////////////////////////////////////////////////
#ifndef RENDERER_H
#define RENDERER_H

#include "common/algebra.h"
#include "common/global.h"
#include "object.h"
#include "options.h"
#include "rendererc.h" // Renderer constants
#include "resource.h"
#include "riInterface.h"
#include "shadeop.h"
#include "shader.h"
#include "texture.h"
#include "variable.h"

class CShadingContext;
class CIrradianceCache;
class CPhotonMap;
class CParticipatingMedium;
class CDelayedObject;
class CDelayedInstance;
class CNetFileMapping;
class CSGTreeNode;
class CSolidObject;

///////////////////////////////////////////////////////////////////////
// Class				:	CRendererContext
// Description			:	Holds the global rendering context
// Comments				:
class CRendererContext : public CRiInterface {
    public:
        CRendererContext(const char *ribName = NULL, const char *netString = NULL);
        ~CRendererContext();

        ///////////////////////////////////////////////////////////////////////
        // Renderer interface
        ///////////////////////////////////////////////////////////////////////
        void RiDeclare(const char *, const char *) override;

        void RiFrameBegin(int) override;
        void RiFrameEnd(void) override;
        void RiWorldBegin(void) override;
        void RiWorldEnd(void) override;

        void RiFormat(int xres, int yres, float aspect) override;
        void RiFrameAspectRatio(float aspect) override;
        void RiScreenWindow(float left, float right, float bot, float top) override;
        void RiCropWindow(float xmin, float xmax, float ymin, float ymax) override;
        void RiProjectionV(const char *name, int n, const char *tokens[], const void *params[]) override;
        void RiClipping(float hither, float yon) override;
        void RiClippingPlane(float x, float y, float z, float nx, float ny, float nz) override;
        void RiDepthOfField(float fstop, float focallength, float focaldistance) override;
        void RiShutter(float smin, float smax) override;

        void RiPixelVariance(float variation) override;
        void RiPixelSamples(float xsamples, float ysamples) override;
        void RiPixelFilter(float (*function)(float, float, float, float), float xwidth, float ywidth) override;
        void RiExposure(float gain, float gamma) override;
        void RiImagerV(const char *name, int n, const char *tokens[], const void *params[]) override;
        void RiQuantize(const char *type, int one, int qmin, int qmax, float ampl) override;
        void RiDisplayV(const char *name, const char *type, const char *mode, int n, const char *tokens[], const void *params[]) override;
        void RiCustomDisplayV(const char *name, RtToken mode, RtDisplayStartFunction, RtDisplayDataFunction, RtDisplayFinishFunction, RtInt n, RtToken tokens[], RtPointer params[]) override;
        void RiDisplayChannelV(const char *channel, int n, const char *tokens[], const void *params[]) override;

        void RiHiderV(const char *type, int n, const char *tokens[], const void *params[]) override;
        void RiColorSamples(int N, float *nRGB, float *RGBn) override;
        void RiRelativeDetail(float relativedetail) override;
        void RiOptionV(const char *name, int n, const char *tokens[], const void *params[]) override;

        void RiAttributeBegin(void) override;
        void RiAttributeEnd(void) override;
        void RiColor(float *Cs) override;
        void RiOpacity(float *Cs) override;
        void RiTextureCoordinates(float s1, float t1, float s2, float t2, float s3, float t3, float s4, float t4) override;

        void *RiLightSourceV(const char *name, int n, const char *tokens[], const void *params[]) override;
        void *RiAreaLightSourceV(const char *name, int n, const char *tokens[], const void *params[]) override;

        void RiIlluminate(const void *light, int onoff) override;
        void RiSurfaceV(const char *name, int n, const char *tokens[], const void *params[]) override;
        void RiAtmosphereV(const char *name, int n, const char *tokens[], const void *params[]) override;
        void RiInteriorV(const char *name, int n, const char *tokens[], const void *params[]) override;
        void RiExteriorV(const char *name, int n, const char *tokens[], const void *params[]) override;
        void RiShadingRate(float size) override;
        void RiShadingInterpolation(const char *type) override;
        void RiMatte(int onoff) override;

        void RiBound(float *bound) override;
        void RiDetail(float *bound) override;
        void RiDetailRange(float minvis, float lowtran, float uptran, float maxvis) override;
        void RiGeometricApproximation(const char *type, float value) override;
        void RiGeometricRepresentation(const char *type) override;
        void RiOrientation(const char *orientation) override;
        void RiReverseOrientation(void) override;
        void RiSides(int nsides) override;

        void RiIdentity(void) override;
        void RiTransform(float transform[][4]) override;
        void RiConcatTransform(float transform[][4]) override;
        void RiPerspective(float fov) override;
        void RiTranslate(float dx, float dy, float dz) override;
        void RiRotate(float angle, float dx, float dy, float dz) override;
        void RiScale(float dx, float dy, float dz) override;
        void RiSkew(float angle, float dx1, float dy1, float dz1, float dx2, float dy2, float dz2) override;
        void RiDeformationV(const char *name, int n, const char *tokens[], const void *params[]) override;
        void RiDisplacementV(const char *name, int n, const char *tokens[], const void *params[]) override;
        void RiCoordinateSystem(const char *space) override;
        void RiCoordSysTransform(const char *space) override;

        RtPoint *RiTransformPoints(const char *fromspace, const char *tospace, int npoints, RtPoint *points) override;

        void RiTransformBegin(void) override;
        void RiTransformEnd(void) override;

        void RiAttributeV(const char *name, int n, const char *tokens[], const void *params[]) override;

        void RiPolygonV(int nvertices, int n, const char *tokens[], const void *params[]) override;
        void RiGeneralPolygonV(int nloops, int *nverts, int n, const char *tokens[], const void *params[]) override;
        void RiPointsPolygonsV(int npolys, int *nverts, int *verts, int n, const char *tokens[], const void *params[]) override;
        void RiPointsGeneralPolygonsV(int npolys, int *nloops, int *nverts, int *verts, int n, const char *tokens[], const void *params[]) override;
        void RiBasis(float ubasis[][4], int ustep, float vbasis[][4], int vstep) override;
        void RiPatchV(const char *type, int n, const char *tokens[], const void *params[]) override;
        void RiPatchMeshV(const char *type, int nu, const char *uwrap, int nv, const char *vwrap, int n, const char *tokens[], const void *params[]) override;
        void RiNuPatchV(int nu, int uorder, float *uknot, float umin, float umax, int nv, int vorder, float *vknot, float vmin, float vmax, int n, const char *tokens[], const void *params[]) override;
        void RiTrimCurve(int nloops, int *ncurves, int *order, float *knot, float *amin, float *amax, int *n, float *u, float *v, float *w) override;

        void RiSphereV(float radius, float zmin, float zmax, float thetamax, int n, const char *tokens[], const void *params[]) override;
        void RiConeV(float height, float radius, float thetamax, int n, const char *tokens[], const void *params[]) override;
        void RiCylinderV(float radius, float zmin, float zmax, float thetamax, int n, const char *tokens[], const void *params[]) override;
        void RiHyperboloidV(float *point1, float *point2, float thetamax, int n, const char *tokens[], const void *params[]) override;
        void RiParaboloidV(float rmax, float zmin, float zmax, float thetamax, int n, const char *tokens[], const void *params[]) override;
        void RiDiskV(float height, float radius, float thetamax, int n, const char *tokens[], const void *params[]) override;
        void RiTorusV(float majorrad, float minorrad, float phimin, float phimax, float thetamax, int n, const char *tokens[], const void *params[]) override;
        void RiProcedural(void *data, float *bound, void (*subdivfunc)(void *, float), void (*freefunc)(void *)) override;
        void RiGeometryV(const char *type, int n, const char *tokens[], const void *params[]) override;
        void loadAndExecuteNamedGeometry(const char *filename, const char *objectName);

        void RiCurvesV(const char *degree, int ncurves, int nverts[], const char *wrap, int n, const char *tokens[], const void *params[]) override;
        void RiPointsV(int npts, int n, const char *tokens[], const void *params[]) override;
        void RiSubdivisionMeshV(const char *scheme, int nfaces, int nvertices[], int vertices[], int ntags, const char *tags[], int nargs[], int intargs[], float floatargs[], int n, const char *tokens[], const void *params[]) override;
        void RiHierarchicalSubdivisionMeshV(const char *scheme, int nfaces, int nvertices[], int vertices[], int ntags, const char *tags[], int nargs[], int intargs[], float floatargs[], int noverrides, int overrideFaceIndex[], int overrideLevel[], const char *overrideTags[], float overrideValues[], int n, const char *tokens[], const void *params[]) override;
        void RiBlobbyV(int nleaf, int ncode, int code[], int nflt, float flt[], int nstr, const char *str[], int n, const char *tokens[], const void *params[]) override;

        void RiSolidBegin(const char *type) override;
        void RiSolidEnd(void) override;
        void *RiObjectBegin(void) override;

        void RiObjectEnd(void) override;
        void RiObjectInstance(const void *handle) override;
        void RiMotionBeginV(int N, float times[]) override;
        void RiMotionEnd(void) override;

        void RiMakeTextureV(const char *pic, const char *tex, const char *swrap, const char *twrap, float (*filterfunc)(float, float, float, float), float swidth, float twidth, int n, const char *tokens[], const void *params[]) override;
        void RiMakeBumpV(const char *pic, const char *tex, const char *swrap, const char *twrap, float (*filterfunc)(float, float, float, float), float swidth, float twidth, int n, const char *tokens[], const void *params[]) override;
        void RiMakeLatLongEnvironmentV(const char *pic, const char *tex, float (*filterfunc)(float, float, float, float), float swidth, float twidth, int n, const char *tokens[], const void *params[]) override;
        void RiMakeCubeFaceEnvironmentV(const char *px, const char *nx, const char *py, const char *ny, const char *pz, const char *nz, const char *tex, float fov, float (*filterfunc)(float, float, float, float), float swidth, float twidth, int n, const char *tokens[], const void *params[]) override;
        void RiMakeShadowV(const char *pic, const char *tex, int n, const char *tokens[], const void *params[]) override;
        void RiMakeBrickMapV(int nb, const char **src, const char *dest, int n, const char *tokens[], const void *params[]) override;

        void RiArchiveRecord(const char *type, const char *format, va_list args) override;
        void RiReadArchiveV(const char *filename, void (*callback)(const char *, ...), int n, const char *tokens[], const void *params[]) override;

        void *RiArchiveBeginV(const char *name, int n, const char *tokens[], const void *parms[]) override;
        void RiArchiveEnd(void) override;

        void RiResourceV(const char *handle, const char *type, int n, const char *tokens[], const void *parms[]) override;
        void RiResourceBegin(void) override;
        void RiResourceEnd(void) override;

        void RiIfBeginV(const char *expr, int n, const char *tokens[], const void *parms[]) override;
        void RiElseIfV(const char *expr, int n, const char *tokens[], const void *parms[]) override;
        void RiElse(void) override;
        void RiIfEnd(void) override;

        void RiError(int, int, const char *) override;

        // The following functions provide access to the graphics state
        CXform *getXform(int);                                                            // Get the active XForm
        CAttributes *getAttributes(int) override;                                         // Get the active Attributes
        COptions *getOptions() override;                                                  // Get the active Options
        void getBasisSteps(int &uStep, int &vStep) override;                              // Get the active basis step sizes
        CShaderInstance *getShader(const char *, int, int, const char **, const void **); // Load a shader

        // Delayed object junk
        void processDelayedObject(CShadingContext *context, CDelayedObject *, void (*subdivisionFunction)(void *, float), void *, const float *, const float *);
        void processDelayedInstance(CShadingContext *context, CDelayedInstance *instance);
        void processDelayedSolid(CShadingContext *context, CSolidObject *solid);

        virtual void addObject(CObject *) override; // Add an object into the scene
        void addInstance(const void *);    // Add an instance into the scene
        void rendererThread(const void *);

    private:
        ///////////////////////////////////////////////////////////////////////
        // Class				:	CInstance
        // Description			:	This class is allocated at objectBegin only to hold
        //							a list of objects
        // Comments				:
        class CInstance {
            public:
                CObject *objects;
        };

        CArray<CXform *> *savedXforms;           // Used to save/restore the graphics state
        CArray<CAttributes *> *savedAttributes;  // Saved attributes
        CArray<COptions *> *savedOptions;        // Saved options
        CArray<CResource *> *savedResources;     // Saved resources
        CInstance *instance;                     // The current instance object
        CObject *delayed;                        // The current delayed object
        CArray<CInstance *> *instanceStack;      // The stack of object lists
        CArray<CInstance *> *allocatedInstances; // The list of allocated object instances
        CSGTreeNode *currentSolid;               // The current CSG tree node (SolidBegin/SolidEnd capture)
        CArray<CSGTreeNode *> *savedSolids;      // The stack of enclosing CSG tree nodes
        CXform *currentXform;                    // The current graphics state
        CAttributes *currentAttributes;
        COptions *currentOptions;
        CResource *currentResource;
        // Some RenderMan Interface related variables
        bool inWorld{false};     // True between RiWorldBegin and RiWorldEnd
        int numExpectedMotions;  // The number of expected motions in a motion block
        int numMotions;          // The number of motions so far
        float *keyTimes;         // The key times
        float *motionParameters; // The arrays of motion parameters
        int maxMotionParameters; // The maximum number of motion parameters that can be stored
        const char *lastCommand; // The text of the last motion command
        int riExecTag;           // The exec tag count for the RI interface

        void init(CProgrammableShaderInstance *); // Execute the init code of a shader
        int ifParse(const char *expr);            // Evaluate a condition
        int addMotion(float *parameters, int parameterSize, const char *name, float *&p0, float *&p1);
};

#endif
