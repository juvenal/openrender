/**
 * Project: openRender
 *
 * File: csgClassify.cpp
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
#include "csgClassify.h"

// Spec 021 Phase 2 skeleton: inert answers so the tests compile and fail.
// Classification, the boundary rule and bound classification arrive with T031.

int classifyOperand(CShadingContext *, const CCSGResolvedOperand &, const float *) {
    return CSG_OUT;
}

int classifyOperandAlong(CShadingContext *, const CCSGResolvedOperand &, const float *, const float *) {
    return CSG_OUT;
}

CCSGEvaluator::CCSGEvaluator(const CCSGDefinition *definition, const std::vector<CCSGResolvedOperand *> &operands, float epsilon)
    : definition(definition), operands(operands), epsilon(epsilon) {}

int CCSGEvaluator::classify(CShadingContext *, int, const float *) const {
    return CSG_OUT;
}

int CCSGEvaluator::accepts(CShadingContext *, int, const float *, const float *) const {
    return FALSE;
}

int CCSGEvaluator::classifyBound(CShadingContext *, int, const float *, const float *) const {
    return REGION_MIXED;
}

int CCSGRegion::accepts(CShadingContext *ctx, const float *P, const float *Ng) const {
    return evaluator->accepts(ctx, operand, P, Ng);
}

int CCSGRegion::classifyBound(CShadingContext *ctx, const float *bmin, const float *bmax) const {
    return evaluator->classifyBound(ctx, operand, bmin, bmax);
}
