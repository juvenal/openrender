/**
 * Project: openRender
 *
 * File: geometryFrame.h
 *
 * Description:
 *   The frame limits and constants that dicing and ray tessellation share (spec 031, FR-002). The renderer core
 *   builds the value once per frame from its own values (stored copies, never recomputed) and installs it until
 *   the end of the frame; Geometry reads it lock-free through geometryFrame(). With nothing installed every
 *   field is 0.
 *
 *   Validation: numThreads >= 1 and maxGridSize >= 1 whenever a frame is installed (asserted in debug at
 *   install). With nothing installed only raytracingFlags may be read (by CObject::raytraced(), which then
 *   answers false, as the renderer's own flags do outside a frame today); a debug build of the renderer
 *   reports once any other read with no frame installed.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef GEOMETRYFRAME_H
#define GEOMETRYFRAME_H

#include "common/algebra.h"
#include "options.h" // OPTIONS_PROJECTION_*, OPTIONS_FLAGS_* (Graphics State)

///////////////////////////////////////////////////////////////////////
// Class				:	CGeometryFrame
// Description			:	The frame values Geometry reads
// Comments				:	A plain aggregate: value-initialise it to get the all-zero default
struct CGeometryFrame {
        int numThreads, maxGridSize, maxEyeSplits;
        unsigned int flags;           // OPTIONS_FLAGS_MOTIONBLUR, OPTIONS_FLAGS_FOCALBLUR, ...
        unsigned int raytracingFlags; // the attribute flags that make an object raytraced
        int projection;               // an EProjectionType value
        float imagePlane, dPixeldx, dPixeldy, pixelLeft, pixelTop, clipMin;
        float cocFactorPixels, invFocaldistance; // until the lens phase, which replaces them by `openrender::LensModel lens`
};

void installGeometryFrame(const CGeometryFrame *frame); // the renderer core; nullptr uninstalls
const CGeometryFrame &geometryFrame();                  // never null: the installed value or the zero default

// The attribute flags that make an object raytraced: the installed frame's, or 0 with none installed. The one
// read that is allowed outside a frame (CObject::raytraced()).
unsigned int geometryFrameRaytracingFlags();

// camera2pixels() of renderer.h over the frame's copies: the same expression, character for character
inline void camera2pixels(const CGeometryFrame &f, int n, float *P) {
    if (f.projection == OPTIONS_PROJECTION_PERSPECTIVE) {
        for (; n > 0; n--, P += 3) {
            P[COMP_X] = (f.imagePlane * P[COMP_X] / P[COMP_Z] - f.pixelLeft) * f.dPixeldx;
            P[COMP_Y] = (f.imagePlane * P[COMP_Y] / P[COMP_Z] - f.pixelTop) * f.dPixeldy;
        }
    }
    else {
        for (; n > 0; n--, P += 3) {
            P[COMP_X] = (P[COMP_X] - f.pixelLeft) * f.dPixeldx;
            P[COMP_Y] = (P[COMP_Y] - f.pixelTop) * f.dPixeldy;
        }
    }
}

#endif
