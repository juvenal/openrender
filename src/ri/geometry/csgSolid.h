/**
 * Project: openRender
 *
 * File: csgSolid.h
 *
 * Description:
 *   The renderable CSG solid: one placement or instance of a shared
 *   definition, resolved lazily on first use (spec 021).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	csgSolid.h
//  Classes				:	CCSGSolid, CCSGInstanceState
//  Description			:	A solid handle in the scene and the BVH; its
//							per-instance state is built on the first
//							dice()/intersect() (spec 021, data-model.md)
//
////////////////////////////////////////////////////////////////////////
#ifndef CSGSOLID_H
#define CSGSOLID_H

#include <atomic>
#include <mutex>
#include <vector>

#include "csgClassify.h"
#include "csgDefinition.h"
#include "geometryHost.h"
#include "object.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGInstanceState
// Description			:	What one solid instance resolved to: its operand
//							members, their classification surfaces and kinds,
//							the evaluator and the per-operand regions
// Comments				:	Built once, then read concurrently by every thread
class CCSGInstanceState : public CFrameResource {
    public:
        CCSGInstanceState();
        ~CCSGInstanceState();

        // Keep o alive until the frame ends (or this state goes)
        void hold(CObject *o);

        // CFrameResource: rendering is over, drop every held object
        void frameEnded();

        std::vector<CCSGResolvedOperand *> operands; // Per operand: members, surfaces, kind, bound
        std::vector<CCSGRegion *> regions;           // Per operand: the region its leaves carry
        CCSGEvaluator *evaluator;                    // The boundary rule over operands
        int passthrough;                             // Operand rendered as authored (FR-019), or -1

    private:
        std::vector<CObject *> held; // One reference each: members and their surfaces
};

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGSolid
// Description			:	One placement (or ObjectInstance) of a CCSGDefinition
// Comments				:	Not OBJECT_DUMMY, so traversal calls intersect()
class CCSGSolid : public CObject {
    public:
        CCSGSolid(CAttributes *, CXform *, CCSGDefinition *);
        ~CCSGSolid();

        // CObject interface
        void intersect(CShadingContext *, CRay *);
        void dice(CReyes *);
        void instantiate(CAttributes *, CXform *, CRiInterface *) const;

        // Build the per-instance state and the children, exactly once; the
        // first dice() or intersect() calls it. Later calls return at once.
        void resolve(CShadingContext *context);

        // The resolved state, or NULL before the first resolve()
        const CCSGInstanceState *instanceState() const { return state.load(std::memory_order_acquire); }

        CCSGDefinition *definition; // Shared, attached

    private:
        CCSGInstanceState *build(CShadingContext *context);

        std::atomic<CCSGInstanceState *> state; // Attached by the solid
        std::mutex resolveMutex;
};

#endif // CSGSOLID_H
