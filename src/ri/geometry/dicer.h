/**
 * Project: openRender
 *
 * File: dicer.h
 *
 * Description:
 *   IDicer: what a surface's dice() and splitToChildren() need from the hider that dices (spec 031, FR-002).
 *   Geometry owns the interface and never implements it; the hiders that rasterize implement it (the Reyes
 *   hider and its two rasterizers). Header only; not installed.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef DICER_H
#define DICER_H

#include "rayContext.h"

class CObject;
class CSurface;
struct CGridSnap; // patchLayout.h

///////////////////////////////////////////////////////////////////////
// Class				:	IDicer
// Description			:	The services Geometry's dicing side asks of the hider that dices
// Comments				:	It does not derive from IRayContext: the dicing thread's context is rayContext().
class IDicer {
    public:
        virtual ~IDicer() = default;

        virtual IRayContext &rayContext() = 0; // the dicing thread's context: scratch, varyings, lazy resolution and child creation

        virtual void drawObject(CObject *object) = 0; // queue an object (children, split pieces)
        virtual void drawGrid(CSurface *surface, int udiv, int vdiv, float umin, float umax, float vmin, float vmax,
                              const CGridSnap *snap) = 0;             // dice and rasterize a grid (snap may be null)
        virtual void drawPoints(CSurface *points, int numPoints) = 0; // RiPoints

        // The probe: the displacement estimate of the u x v points written into rayContext().varyings(); the results are
        // read back from the same arrays.
        virtual void probe(CSurface *surface, int u, int v, EShadingDim dim, unsigned int up) = 0;

        virtual int inFrustum(const float *bmin, const float *bmax) = 0; // TRUE if the camera-space box may be visible

        const CGeometryFrame &frame() const { return geometryFrame(); }
};

#endif
