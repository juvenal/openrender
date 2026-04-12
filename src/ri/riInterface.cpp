/**
 * Project: openRender
 *
 * File: riInterface.cpp
 *
 * Description:
 *   This file implements the functionality for riInterface.
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
//  File				:	riInterface.cpp
//  Classes				:	CRiInterface
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#include <stdarg.h>

#include "common/os.h"
#include "ri.h"
#include "riInterface.h"
#include "ri_config.h"

CRiInterface::CRiInterface() {
    errorHandler = RiErrorPrint;
    renderMan = this;
}

CRiInterface::~CRiInterface() {
}

void CRiInterface::RiDeclare(const char *, const char *) {
}

void CRiInterface::RiFrameBegin(int) {
}

void CRiInterface::RiFrameEnd(void) {
}

void CRiInterface::RiWorldBegin(void) {
}

void CRiInterface::RiWorldEnd(void) {
}

void CRiInterface::RiFormat(int, int, float) {
}

void CRiInterface::RiFrameAspectRatio(float) {
}

void CRiInterface::RiScreenWindow(float, float, float, float) {
}

void CRiInterface::RiCropWindow(float, float, float, float) {
}

void CRiInterface::RiProjectionV(const char *, int, const char *[], const void *[]) {
}

void CRiInterface::RiClipping(float, float) {
}

void CRiInterface::RiClippingPlane(float, float, float, float, float, float) {
}

void CRiInterface::RiDepthOfField(float, float, float) {
}

void CRiInterface::RiShutter(float, float) {
}

void CRiInterface::RiPixelVariance(float) {
}

void CRiInterface::RiPixelSamples(float, float) {
}

void CRiInterface::RiPixelFilter(float (*)(float, float, float, float), float, float) {
}

void CRiInterface::RiExposure(float, float) {
}

void CRiInterface::RiImagerV(const char *, int, const char *[], const void *[]) {
}

void CRiInterface::RiQuantize(const char *, int, int, int, float) {
}

void CRiInterface::RiDisplayV(const char *, const char *, const char *, int, const char *[], const void *[]) {
}

void CRiInterface::RiCustomDisplayV(const char *, RtToken, RtDisplayStartFunction, RtDisplayDataFunction, RtDisplayFinishFunction, RtInt, RtToken[], RtPointer[]) {
}

void CRiInterface::RiDisplayChannelV(const char *, int, const char *[], const void *[]) {
}

void CRiInterface::RiHiderV(const char *, int, const char *[], const void *[]) {
}

void CRiInterface::RiColorSamples(int, float *, float *) {
}

void CRiInterface::RiRelativeDetail(float) {
}

void CRiInterface::RiOptionV(const char *, int, const char *[], const void *[]) {
}

void CRiInterface::RiAttributeBegin(void) {
}

void CRiInterface::RiAttributeEnd(void) {
}

void CRiInterface::RiColor(float *) {
}

void CRiInterface::RiOpacity(float *) {
}

void CRiInterface::RiTextureCoordinates(float, float, float, float, float, float, float, float) {
}

void *CRiInterface::RiLightSourceV(const char *, int, const char *[], const void *[]) {
    return NULL;
}

void *CRiInterface::RiAreaLightSourceV(const char *, int, const char *[], const void *[]) {
    return NULL;
}

void CRiInterface::RiIlluminate(const void *, int) {
}

void CRiInterface::RiSurfaceV(const char *, int, const char *[], const void *[]) {
}

void CRiInterface::RiAtmosphereV(const char *, int, const char *[], const void *[]) {
}

void CRiInterface::RiInteriorV(const char *, int, const char *[], const void *[]) {
}

void CRiInterface::RiExteriorV(const char *, int, const char *[], const void *[]) {
}

void CRiInterface::RiShadingRate(float) {
}

void CRiInterface::RiShadingInterpolation(const char *) {
}

void CRiInterface::RiMatte(int) {
}

void CRiInterface::RiBound(float *) {
}

void CRiInterface::RiDetail(float *) {
}

void CRiInterface::RiDetailRange(float, float, float, float) {
}

void CRiInterface::RiGeometricApproximation(const char *, float) {
}

void CRiInterface::RiGeometricRepresentation(const char *) {
}

void CRiInterface::RiOrientation(const char *) {
}

void CRiInterface::RiReverseOrientation(void) {
}

void CRiInterface::RiSides(int) {
}

void CRiInterface::RiIdentity(void) {
}

void CRiInterface::RiTransform(float [][4]) {
}

void CRiInterface::RiConcatTransform(float [][4]) {
}

void CRiInterface::RiPerspective(float) {
}

void CRiInterface::RiTranslate(float, float, float) {
}

void CRiInterface::RiRotate(float, float, float, float) {
}

void CRiInterface::RiScale(float, float, float) {
}

void CRiInterface::RiSkew(float, float, float, float, float, float, float) {
}

void CRiInterface::RiDeformationV(const char *, int, const char *[], const void *[]) {
}

void CRiInterface::RiDisplacementV(const char *, int, const char *[], const void *[]) {
}

void CRiInterface::RiCoordinateSystem(const char *) {
}

void CRiInterface::RiCoordSysTransform(const char *) {
}

RtPoint *CRiInterface::RiTransformPoints(const char *, const char *, int, RtPoint *) {
    return NULL;
}

void CRiInterface::RiTransformBegin(void) {
}

void CRiInterface::RiTransformEnd(void) {
}

void CRiInterface::RiAttributeV(const char *, int, const char *[], const void *[]) {
}

void CRiInterface::RiPolygonV(int, int, const char *[], const void *[]) {
}

void CRiInterface::RiGeneralPolygonV(int, int *, int, const char *[], const void *[]) {
}

void CRiInterface::RiPointsPolygonsV(int, int *, int *, int, const char *[], const void *[]) {
}

void CRiInterface::RiPointsGeneralPolygonsV(int, int *, int *, int *, int, const char *[], const void *[]) {
}

void CRiInterface::RiBasis(float [][4], int, float [][4], int) {
}

void CRiInterface::RiPatchV(const char *, int, const char *[], const void *[]) {
}

void CRiInterface::RiPatchMeshV(const char *, int, const char *, int, const char *, int, const char *[], const void *[]) {
}

void CRiInterface::RiNuPatchV(int, int, float *, float, float, int, int, float *, float, float, int, const char *[], const void *[]) {
}

void CRiInterface::RiTrimCurve(int, int *, int *, float *, float *, float *, int *, float *, float *, float *) {
}

void CRiInterface::RiSphereV(float, float, float, float, int, const char *[], const void *[]) {
}

void CRiInterface::RiConeV(float, float, float, int, const char *[], const void *[]) {
}

void CRiInterface::RiCylinderV(float, float, float, float, int, const char *[], const void *[]) {
}

void CRiInterface::RiHyperboloidV(float *, float *, float, int, const char *[], const void *[]) {
}

void CRiInterface::RiParaboloidV(float, float, float, float, int, const char *[], const void *[]) {
}

void CRiInterface::RiDiskV(float, float, float, int, const char *[], const void *[]) {
}

void CRiInterface::RiTorusV(float, float, float, float, float, int, const char *[], const void *[]) {
}

void CRiInterface::RiProcedural(void *, float *, void (*)(void *, float), void (*)(void *)) {
}

void CRiInterface::RiGeometryV(const char *, int, const char *[], const void *[]) {
}

void CRiInterface::RiCurvesV(const char *, int, int[], const char *, int, const char *[], const void *[]) {
}

void CRiInterface::RiPointsV(int, int, const char *[], const void *[]) {
}

void CRiInterface::RiSubdivisionMeshV(const char *, int, int[], int[], int, const char *[], int[], int[], float[], int, const char *[], const void *[]) {
}

void CRiInterface::RiBlobbyV(int, int, int[], int, float[], int, const char *[], int, const char *[], const void *[]) {
}

void CRiInterface::RiProcDelayedReadArchive(const char *, float) {
}

void CRiInterface::RiProcRunProgram(const char *, float) {
}

void CRiInterface::RiProcDynamicLoad(const char *, float) {
}

void CRiInterface::RiProcFree(const char *) {
}

void CRiInterface::RiSolidBegin(const char *) {
}

void CRiInterface::RiSolidEnd(void) {
}

void *CRiInterface::RiObjectBegin(void) {
    return NULL;
}

void CRiInterface::RiObjectEnd(void) {
}

void CRiInterface::RiObjectInstance(const void *) {
}

void CRiInterface::RiMotionBeginV(int, float[]) {
}

void CRiInterface::RiMotionEnd(void) {
}

void CRiInterface::RiMakeTextureV(const char *, const char *, const char *, const char *, float (*)(float, float, float, float), float, float, int, const char *[], const void *[]) {
}

void CRiInterface::RiMakeBumpV(const char *, const char *, const char *, const char *, float (*)(float, float, float, float), float, float, int, const char *[], const void *[]) {
}

void CRiInterface::RiMakeLatLongEnvironmentV(const char *, const char *, float (*)(float, float, float, float), float, float, int, const char *[], const void *[]) {
}

void CRiInterface::RiMakeCubeFaceEnvironmentV(const char *, const char *, const char *, const char *, const char *, const char *, const char *, float, float (*)(float, float, float, float), float, float, int, const char *[], const void *[]) {
}

void CRiInterface::RiMakeShadowV(const char *, const char *, int, const char *[], const void *[]) {
}

void CRiInterface::RiMakeBrickMapV(int, const char **, const char *, int, const char *[], const void *[]) {
}

void CRiInterface::RiErrorHandler(void (*handler)(int, int, const char *)) {
    errorHandler = handler;
}

void CRiInterface::RiArchiveRecord(const char *, const char *, va_list) {
}

void CRiInterface::RiReadArchiveV(const char *, void (*)(const char *, ...), int, const char *[], const void *[]) {
}

void *CRiInterface::RiArchiveBeginV(const char *, int, const char *[], const void *[]) {
    return NULL;
}

void CRiInterface::RiArchiveEnd(void) {
}

void CRiInterface::RiResourceV(const char *, const char *, int, const char *[], const void *[]) {
}

void CRiInterface::RiResourceBegin(void) {
}

void CRiInterface::RiResourceEnd(void) {
}

void CRiInterface::RiIfBeginV(const char *, int, const char *[], const void *[]) {
}

void CRiInterface::RiElseIfV(const char *, int, const char *[], const void *[]) {
}

void CRiInterface::RiElse(void) {
}

void CRiInterface::RiIfEnd(void) {
}

void CRiInterface::RiError(int c, int s, const char *m) {
    if (errorHandler != NULL) {
        errorHandler(c, s, m);
    }
}
