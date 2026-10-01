/**
 * Project: openRender
 *
 * File: csgClassify.h
 *
 * Description:
 *   Operand point classification, the boundary rule and the CSG
 *   implementation of the generic region predicate (spec 021).
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
//  File				:	csgClassify.h
//  Classes				:	CCSGEvaluator, CCSGRegion
//  Description			:	Operand point classification, the boundary rule,
//							and the CSG implementation of the generic region
//							predicate (spec 021, research.md Decisions 3 and 4)
//
////////////////////////////////////////////////////////////////////////
#ifndef CSGCLASSIFY_H
#define CSGCLASSIFY_H

#include <vector>

#include "csgDefinition.h"
#include "object.h"

const int CSG_OUT = 0;
const int CSG_IN = 1;

// IN/OUT for a camera-space point against one resolved operand. Empty and
// open operands are always OUT; a single quadric uses its implied closed
// solid; anything else uses crossing parity along a fixed direction, with a
// fallback direction and a majority of three near grazing or edge crossings.
int classifyOperand(CShadingContext *ctx, const CCSGResolvedOperand &operand, const float *P);

// The same, along an explicit first ray direction (for tests that need to
// force a grazing or edge crossing); the fallback directions still apply.
int classifyOperandAlong(CShadingContext *ctx, const CCSGResolvedOperand &operand, const float *P, const float *dir);

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGEvaluator
// Description			:	One instance's solid: its definition plus its
//							resolved operands. Answers the boundary rule for a
//							point on a leaf of a given operand.
// Comments				:	Immutable after construction; safe to query from
//							any number of threads.
class CCSGEvaluator {
    public:
        CCSGEvaluator(const CCSGDefinition *definition, const std::vector<CCSGResolvedOperand *> &operands, float epsilon);

        // IN/OUT for operand at P
        int classify(CShadingContext *ctx, int operand, const float *P) const;

        // TRUE if the point P (normal Ng) of a leaf of `operand` lies on the
        // composite boundary (the boundary rule, two-sided, with the lowest
        // operand index winning between coincident faces)
        int accepts(CShadingContext *ctx, int operand, const float *P, const float *Ng) const;

        // REGION_ALL_IN / REGION_ALL_OUT / REGION_MIXED for a patch of a leaf
        // of `operand` inside [bmin, bmax]
        int classifyBound(CShadingContext *ctx, int operand, const float *bmin, const float *bmax) const;

        const CCSGDefinition *definition;
        std::vector<CCSGResolvedOperand *> operands;
        float epsilon; // The two-sided offset (C_EPSILON x solid bound size)
};

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGRegion
// Description			:	The region predicate attached to every leaf of one
//							operand of one instance
// Comments				:
class CCSGRegion : public CRegionPredicate {
    public:
        CCSGRegion(const CCSGEvaluator *evaluator, int operand) : evaluator(evaluator), operand(operand) {}

        int accepts(CShadingContext *ctx, const float *P, const float *Ng) const override;
        int classifyBound(CShadingContext *ctx, const float *bmin, const float *bmax) const override;

        const CCSGEvaluator *evaluator;
        int operand;
};

#endif // CSGCLASSIFY_H
