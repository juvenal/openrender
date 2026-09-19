/**
 * Project: openRender
 *
 * File: ribOut.h
 *
 * Description:
 *   This file defines the interface for ribOut.
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
//  File				:	ribOut.h
//  Classes				:	CRibOut
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef RIBOUT_H
#define RIBOUT_H

#include "common/containers.h"
#include "common/global.h" // The global header file
#include "riInterface.h"
#include "ri_config.h"

#ifdef HAVE_ZLIB
#include <zlib.h>
#endif

class CVariable;

extern const int ribOutScratchSize;

///////////////////////////////////////////////////////////////////////
// Class				:	CRibOut
// Description			:	This class implements a RIB file output
// Comments				:
class CRibOut : public CRiInterface {

        ///////////////////////////////////////////////////////////////////////
        // Class				:	CRibAttributes
        // Description			:	The attributes holder for the RIB file output
        // Comments				:
        class CRibAttributes {
            public:
                CRibAttributes();
                CRibAttributes(CRibAttributes *);
                ~CRibAttributes();

                int uStep, vStep;
                CRibAttributes *next;
        };

    public:
        CRibOut(const char *);
        CRibOut(FILE *);
        virtual ~CRibOut();

        virtual void RiDeclare(const char *, const char *) override;

        virtual void RiFrameBegin(int) override;
        virtual void RiFrameEnd(void) override;
        virtual void RiWorldBegin(void) override;
        virtual void RiWorldEnd(void) override;

        virtual void RiFormat(int xres, int yres, float aspect) override;
        virtual void RiFrameAspectRatio(float aspect) override;
        virtual void RiScreenWindow(float left, float right, float bot, float top) override;
        virtual void RiCropWindow(float xmin, float xmax, float ymin, float ymax) override;
        virtual void RiProjectionV(const char *name, int n, const char *tokens[], const void *params[]) override;
        virtual void RiClipping(float hither, float yon) override;
        virtual void RiClippingPlane(float x, float y, float z, float nx, float ny, float nz) override;
        virtual void RiDepthOfField(float fstop, float focallength, float focaldistance) override;
        virtual void RiShutter(float smin, float smax) override;

        virtual void RiPixelVariance(float variation) override;
        virtual void RiPixelSamples(float xsamples, float ysamples) override;
        virtual void RiPixelFilter(float (*function)(float, float, float, float), float xwidth, float ywidth) override;
        virtual void RiExposure(float gain, float gamma) override;
        virtual void RiImagerV(const char *name, int n, const char *tokens[], const void *params[]) override;
        virtual void RiQuantize(const char *type, int one, int qmin, int qmax, float ampl) override;
        virtual void RiDisplayV(const char *name, const char *type, const char *mode, int n, const char *tokens[], const void *params[]) override;
        virtual void RiCustomDisplayV(const char *name, RtToken mode, RtDisplayStartFunction, RtDisplayDataFunction, RtDisplayFinishFunction, RtInt n, RtToken tokens[], RtPointer params[]) override;
        virtual void RiDisplayChannelV(const char *channel, int n, const char *tokens[], const void *params[]) override;

        virtual void RiHiderV(const char *type, int n, const char *tokens[], const void *params[]) override;
        virtual void RiColorSamples(int N, float *nRGB, float *RGBn) override;
        virtual void RiRelativeDetail(float relativedetail) override;
        virtual void RiOptionV(const char *name, int n, const char *tokens[], const void *params[]) override;

        virtual void RiAttributeBegin(void) override;
        virtual void RiAttributeEnd(void) override;
        virtual void RiColor(float *Cs) override;
        virtual void RiOpacity(float *Cs) override;
        virtual void RiTextureCoordinates(float s1, float t1, float s2, float t2, float s3, float t3, float s4, float t4) override;

        virtual void *RiLightSourceV(const char *name, int n, const char *tokens[], const void *params[]) override;
        virtual void *RiAreaLightSourceV(const char *name, int n, const char *tokens[], const void *params[]) override;

        virtual void RiIlluminate(const void *light, int onoff) override;
        virtual void RiSurfaceV(const char *name, int n, const char *tokens[], const void *params[]) override;
        virtual void RiAtmosphereV(const char *name, int n, const char *tokens[], const void *params[]) override;
        virtual void RiInteriorV(const char *name, int n, const char *tokens[], const void *params[]) override;
        virtual void RiExteriorV(const char *name, int n, const char *tokens[], const void *params[]) override;
        virtual void RiShadingRate(float size) override;
        virtual void RiShadingInterpolation(const char *type) override;
        virtual void RiMatte(int onoff) override;

        virtual void RiBound(float *bound) override;
        virtual void RiDetail(float *bound) override;
        virtual void RiDetailRange(float minvis, float lowtran, float uptran, float maxvis) override;
        virtual void RiGeometricApproximation(const char *type, float value) override;
        virtual void RiGeometricRepresentation(const char *type) override;
        virtual void RiOrientation(const char *orientation) override;
        virtual void RiReverseOrientation(void) override;
        virtual void RiSides(int nsides) override;

        virtual void RiIdentity(void) override;
        virtual void RiTransform(float transform[][4]) override;
        virtual void RiConcatTransform(float transform[][4]) override;
        virtual void RiPerspective(float fov) override;
        virtual void RiTranslate(float dx, float dy, float dz) override;
        virtual void RiRotate(float angle, float dx, float dy, float dz) override;
        virtual void RiScale(float dx, float dy, float dz) override;
        virtual void RiSkew(float angle, float dx1, float dy1, float dz1, float dx2, float dy2, float dz2) override;
        virtual void RiDeformationV(const char *name, int n, const char *tokens[], const void *params[]) override;
        virtual void RiDisplacementV(const char *name, int n, const char *tokens[], const void *params[]) override;
        virtual void RiCoordinateSystem(const char *space) override;
        virtual void RiCoordSysTransform(const char *space) override;

        virtual RtPoint *RiTransformPoints(const char *fromspace, const char *tospace, int npoints, RtPoint *points) override;

        virtual void RiTransformBegin(void) override;
        virtual void RiTransformEnd(void) override;

        virtual void RiAttributeV(const char *name, int n, const char *tokens[], const void *params[]) override;

        virtual void RiPolygonV(int nvertices, int n, const char *tokens[], const void *params[]) override;
        virtual void RiGeneralPolygonV(int nloops, int *nverts, int n, const char *tokens[], const void *params[]) override;
        virtual void RiPointsPolygonsV(int npolys, int *nverts, int *verts, int n, const char *tokens[], const void *params[]) override;
        virtual void RiPointsGeneralPolygonsV(int npolys, int *nloops, int *nverts, int *verts, int n, const char *tokens[], const void *params[]) override;
        virtual void RiBasis(float ubasis[][4], int ustep, float vbasis[][4], int vstep) override;

        // CRibOut has no real CAttributes to hand back from getAttributes()
        // (it tracks only uStep/vStep, in its own lightweight CRibAttributes,
        // for its own RIB-text-generation correctness), but it does track
        // those two fields correctly -- see the CRibAttributes class above
        // and RiBasis()'s body. Answering this correctly matters even for a
        // RIB generator (RiBegin with a filename is RISpec's own term for
        // this mode): per RISpec 3.2 section 5.2, the number of patches a
        // PatchMeshV request produces -- hence the parameter-list size the
        // shared grammar (rib.y) expects before this class ever sees the
        // call -- is defined in terms of the current basis's step size, not
        // just validated against it. Without this override, a spec-
        // compliant RIB file that sets a non-default RiBasis before a
        // PatchMesh/Curves statement would have that statement's expected
        // vertex count computed against the wrong (default bezier/3) step,
        // rejecting valid geometry instead of passing it through.
        void getBasisSteps(int &uStep, int &vStep) override {
            uStep = attributes->uStep;
            vStep = attributes->vStep;
        }

        virtual void RiPatchV(const char *type, int n, const char *tokens[], const void *params[]) override;
        virtual void RiPatchMeshV(const char *type, int nu, const char *uwrap, int nv, const char *vwrap, int n, const char *tokens[], const void *params[]) override;
        virtual void RiNuPatchV(int nu, int uorder, float *uknot, float umin, float umax, int nv, int vorder, float *vknot, float vmin, float vmax, int n, const char *tokens[], const void *params[]) override;
        virtual void RiTrimCurve(int nloops, int *ncurves, int *order, float *knot, float *amin, float *amax, int *n, float *u, float *v, float *w) override;

        virtual void RiSphereV(float radius, float zmin, float zmax, float thetamax, int n, const char *tokens[], const void *params[]) override;
        virtual void RiConeV(float height, float radius, float thetamax, int n, const char *tokens[], const void *params[]) override;
        virtual void RiCylinderV(float radius, float zmin, float zmax, float thetamax, int n, const char *tokens[], const void *params[]) override;
        virtual void RiHyperboloidV(float *point1, float *point2, float thetamax, int n, const char *tokens[], const void *params[]) override;
        virtual void RiParaboloidV(float rmax, float zmin, float zmax, float thetamax, int n, const char *tokens[], const void *params[]) override;
        virtual void RiDiskV(float height, float radius, float thetamax, int n, const char *tokens[], const void *params[]) override;
        virtual void RiTorusV(float majorrad, float minorrad, float phimin, float phimax, float thetamax, int n, const char *tokens[], const void *params[]) override;
        virtual void RiProcedural(void *data, float *bound, void (*subdivfunc)(void *, float), void (*freefunc)(void *)) override;
        virtual void RiGeometryV(const char *type, int n, const char *tokens[], const void *params[]) override;

        virtual void RiCurvesV(const char *degree, int ncurves, int nverts[], const char *wrap, int n, const char *tokens[], const void *params[]) override;
        virtual void RiPointsV(int npts, int n, const char *tokens[], const void *params[]) override;
        virtual void RiSubdivisionMeshV(const char *scheme, int nfaces, int nvertices[], int vertices[], int ntags, const char *tags[], int nargs[], int intargs[], float floatargs[], int n, const char *tokens[], const void *params[]) override;
        virtual void RiHierarchicalSubdivisionMeshV(const char *scheme, int nfaces, int nvertices[], int vertices[], int ntags, const char *tags[], int nargs[], int intargs[], float floatargs[], int noverrides, int overrideFaceIndex[], int overrideLevel[], const char *overrideTags[], float overrideValues[], int n, const char *tokens[], const void *params[]) override;
        virtual void RiBlobbyV(int nleaf, int ncode, int code[], int nflt, float flt[], int nstr, const char *str[], int n, const char *tokens[], const void *params[]) override;

        virtual void RiProcDelayedReadArchive(const char *data, float detail) override;
        virtual void RiProcRunProgram(const char *data, float detail) override;
        virtual void RiProcDynamicLoad(const char *data, float detail) override;

        virtual void RiProcFree(const char *) override;

        virtual void RiSolidBegin(const char *type) override;
        virtual void RiSolidEnd(void) override;
        virtual void *RiObjectBegin(void) override;

        virtual void RiObjectEnd(void) override;
        virtual void RiObjectInstance(const void *handle) override;
        virtual void RiMotionBeginV(int N, float times[]) override;
        virtual void RiMotionEnd(void) override;

        virtual void RiMakeTextureV(const char *pic, const char *tex, const char *swrap, const char *twrap, float (*filterfunc)(float, float, float, float), float swidth, float twidth, int n, const char *tokens[], const void *params[]) override;
        virtual void RiMakeBumpV(const char *pic, const char *tex, const char *swrap, const char *twrap, float (*filterfunc)(float, float, float, float), float swidth, float twidth, int n, const char *tokens[], const void *params[]) override;
        virtual void RiMakeLatLongEnvironmentV(const char *pic, const char *tex, float (*filterfunc)(float, float, float, float), float swidth, float twidth, int n, const char *tokens[], const void *params[]) override;
        virtual void RiMakeCubeFaceEnvironmentV(const char *px, const char *nx, const char *py, const char *ny, const char *pz, const char *nz, const char *tex, float fov, float (*filterfunc)(float, float, float, float), float swidth, float twidth, int n, const char *tokens[], const void *params[]) override;
        virtual void RiMakeShadowV(const char *pic, const char *tex, int n, const char *tokens[], const void *params[]) override;
        virtual void RiMakeBrickMapV(int n, const char **src, const char *dest, int numTokens, const char *tokens[], const void *params[]) override;

        virtual void RiErrorHandler(void (*handler)(int, int, const char *)) override;

        virtual void RiArchiveRecord(const char *type, const char *format, va_list args) override;
        virtual void RiReadArchiveV(const char *filename, void (*callback)(const char *, ...), int n, const char *tokens[], const void *params[]) override;

        virtual void *RiArchiveBeginV(const char *name, int n, const char *tokens[], const void *parms[]) override;
        virtual void RiArchiveEnd(void) override;

        virtual void RiResourceV(const char *handle, const char *type, int n, const char *tokens[], const void *parms[]) override;
        virtual void RiResourceBegin(void) override;
        virtual void RiResourceEnd(void) override;

        virtual void RiIfBeginV(const char *expr, int n, const char *tokens[], const void *parms[]) override;
        virtual void RiElseIfV(const char *expr, int n, const char *tokens[], const void *parms[]) override;
        virtual void RiElse(void) override;
        virtual void RiIfEnd(void) override;

    private:
        void writePL(int, const char *[], const void *[]);
        void writePL(int numVertex, int numVarying, int numFaceVarying, int numUniform, int, const char *[], const void *[]);
        void declareVariable(const char *, const char *);
        void declareDefaultVariables();
        void completeInit();

        const char *outName;
        FILE *outFile;
        int outputCompressed;
        int outputIsPipe;
        CDictionary<const char *, CVariable *> *declaredVariables; // Declared variables
        int numLightSources;
        int numObjects;
        CRibAttributes *attributes;
        char *scratch;

        ///////////////////////////////////////////////////////////////////////
        // Class				:	CRibOut
        // Method				:	vout
        // Description			:	Write a variable argument list
        // Return Value			:	-
        // Comments				:
        void vout(const char *mes, va_list args) {
            const int l = vsnprintf(scratch, ribOutScratchSize, mes, args);

#ifdef HAVE_ZLIB
            if (outputCompressed)
                gzwrite(outFile, scratch, l);
            else
                fwrite(scratch, 1, l, outFile);
#else
            fwrite(scratch, 1, l, outFile);
#endif
        }

        ///////////////////////////////////////////////////////////////////////
        // Class				:	CRibOut
        // Method				:	out
        // Description			:	Write an argument list
        // Return Value			:	-
        // Comments				:
        void out(const char *mes, ...) {
            va_list args;

            va_start(args, mes);

            const int l = vsnprintf(scratch, ribOutScratchSize, mes, args);

#ifdef HAVE_ZLIB
            if (outputCompressed)
                gzwrite(outFile, scratch, l);
            else
                fwrite(scratch, 1, l, outFile);
#else
            fwrite(scratch, 1, l, outFile);
#endif

            va_end(args);
        }
};

#endif
