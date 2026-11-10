/**
 * Project: openRender
 *
 * File: rayContext.h
 *
 * Description:
 *   IRayContext: what intersection, ray-hit shading, clustering, lazy child creation, the region and crossing
 *   hooks and the raytrace tessellation need from the render thread that calls them (spec 031, FR-003).
 *   Geometry owns the interface and never implements it; every hider implements it (CHider). Header only; not
 *   installed.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef RAYCONTEXT_H
#define RAYCONTEXT_H

#include "common/shadingDim.h"
#include "geometryFrame.h"

class CMemPage;
class CSurface;

///////////////////////////////////////////////////////////////////////
// Class				:	IRayContext
// Description			:	The per-thread services Geometry's ray side asks of the hider that owns the thread
// Comments				:	One per render thread. The caller writes the points' inputs into varyings(), calls
//						shade() or displace() and reads the outputs back from the same arrays.
class IRayContext {
    public:
        virtual ~IRayContext() = default;

        virtual int threadIndex() const = 0; // this render thread's index, 0 <= threadIndex() < frame().numThreads
        virtual CMemPage *&scratch() = 0;    // the thread's scratch arena: memBegin/memEnd/ralloc/memSave/memRestore
        virtual float **varyings() = 0;      // the current shading state's varying arrays (VARIABLE_* indices)

        // Shade the points written into varyings(): u x v points of `surface`, `dim` as today, `up` the PARAMETER_*
        // flags. shade() runs the full shading of ray hits; displace() the displacement only.
        virtual void shade(CSurface *surface, int u, int v, EShadingDim dim, unsigned int up) = 0;
        virtual void displace(CSurface *surface, int u, int v, EShadingDim dim, unsigned int up) = 0;

        virtual void *saveShadingState() = 0;              // opaque token, as the shading engine's state save
        virtual void restoreShadingState(void *state) = 0; // the token saveShadingState() returned

        const CGeometryFrame &frame() const { return geometryFrame(); }
};

#endif
