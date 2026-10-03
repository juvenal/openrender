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

#include <algorithm>
#include <cmath>

#include "atomic.h"
#include "stats.h"

// Fixed, deterministic, axis-irrational classification ray directions: the
// first two always vote, the third breaks a tie (research.md Decision 3)
static const float kRayDirections[3][3] = {
    {0.5773502691f, 0.6324555320f, 0.5163977795f},
    {-0.4472135955f, 0.3162277660f, 0.8366600265f},
    {0.2672612419f, -0.8017837257f, 0.5345224838f},
};

// When the leaf's own operand does not change across P +- epsilon.Ng (the
// point is off its surface by more than epsilon: a tessellated hit, float
// rounding), the own-side probe is widened by this factor, up to this many times
static const float kOwnProbeGrowth = 10.0f;
static const int kOwnProbeRetries = 4;

static int insideBox(const float *P, const float *bmin, const float *bmax) {
    return P[0] >= bmin[0] && P[0] <= bmax[0] && P[1] >= bmin[1] && P[1] <= bmax[1] && P[2] >= bmin[2] && P[2] <= bmax[2];
}

static int boxesOverlap(const float *amin, const float *amax, const float *bmin, const float *bmax) {
    return amin[0] <= bmax[0] && amax[0] >= bmin[0] && amin[1] <= bmax[1] && amax[1] >= bmin[1] && amin[2] <= bmax[2] && amax[2] >= bmin[2];
}

// Parity of the crossings of the ray from P along dir with the operand
static int crossingParity(CShadingContext *ctx, const CCSGResolvedOperand &operand, const float *P, const float *dir) {
    int count = 0;
    for (const CSurface *surface : operand.surfaces)
        count += surface->countCrossings(ctx, P, dir);
    return count & 1;
}

// IN/OUT with the first vote cast along `first`
static int classifyWith(CShadingContext *ctx, const CCSGResolvedOperand &operand, const float *P, const float *first) {
    atomicIncrement(stats.numCsgClassifications);

    if (operand.kind == CSG_OPERAND_EMPTY || operand.kind == CSG_OPERAND_OPEN)
        return CSG_OUT; // Encloses nothing (FR-021, FR-025a)
    if (!insideBox(P, operand.bmin, operand.bmax))
        return CSG_OUT;
    if (operand.kind == CSG_OPERAND_SINGLE_QUADRIC)
        return operand.surfaces[0]->impliedSolidContains(P) ? CSG_IN : CSG_OUT;

    // Crossing parity: two votes, a third to break a tie (a vote spoilt by a
    // ray through an edge, a vertex or along a face)
    const int a = crossingParity(ctx, operand, P, first);
    const int b = crossingParity(ctx, operand, P, kRayDirections[1]);
    if (a == b)
        return a ? CSG_IN : CSG_OUT;
    return crossingParity(ctx, operand, P, kRayDirections[2]) ? CSG_IN : CSG_OUT;
}

int classifyOperand(CShadingContext *ctx, const CCSGResolvedOperand &operand, const float *P) {
    return classifyWith(ctx, operand, P, kRayDirections[0]);
}

int classifyOperandAlong(CShadingContext *ctx, const CCSGResolvedOperand &operand, const float *P, const float *dir) {
    return classifyWith(ctx, operand, P, dir);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGEvaluator
// Method				:	CCSGEvaluator
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CCSGEvaluator::CCSGEvaluator(const CCSGDefinition *definition, const std::vector<CCSGResolvedOperand *> &operands, float epsilon)
    : definition(definition), operands(operands), epsilon(epsilon) {}

int CCSGEvaluator::classify(CShadingContext *ctx, int operand, const float *P) const {
    return classifyOperand(ctx, *operands[operand], P);
}

// How many of the operand's surfaces the segment (a, b] crosses
static int segmentCrossings(CShadingContext *ctx, const CCSGResolvedOperand &operand, const float *a, const float *b) {
    const float dir[3] = {b[0] - a[0], b[1] - a[1], b[2] - a[2]};
    float lo[3], hi[3];
    for (int k = 0; k < 3; k++) {
        lo[k] = std::min(a[k], b[k]);
        hi[k] = std::max(a[k], b[k]);
    }
    int count = 0;
    for (const CSurface *surface : operand.surfaces)
        if (boxesOverlap(lo, hi, surface->bmin, surface->bmax))
            count += surface->countCrossings(ctx, a, dir) - surface->countCrossings(ctx, b, dir);
    return count;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGEvaluator
// Method				:	accepts
// Description			:	The boundary rule, two-sided: P (normal Ng) on a
//							leaf of `operand` is on the composite boundary iff
//							the solid's membership differs at P + e.Ng and
//							P - e.Ng, the leaf's own operand changes across P,
//							and no lower-indexed operand changes across P too
//							(between coincident faces the lowest index wins)
// Return Value			:	TRUE if the point renders
// Comments				:	Every other operand is classified lazily, only when
//							the tree evaluation (short-circuit) asks for it.
//							Ng's sign is irrelevant: it is only the axis.
int CCSGEvaluator::accepts(CShadingContext *ctx, int operand, const float *P, const float *Ng) const {
    float n[3] = {Ng[0], Ng[1], Ng[2]};
    const float len = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    if (!(len > 0))
        return FALSE;
    for (int k = 0; k < 3; k++)
        n[k] /= len;

    float side[2][3];
    for (int k = 0; k < 3; k++) {
        side[0][k] = P[k] + epsilon * n[k];
        side[1][k] = P[k] - epsilon * n[k];
    }

    // The leaf's own operand on either side
    const CCSGResolvedOperand &own = *operands[operand];
    int ownSide[2] = {classify(ctx, operand, side[0]), classify(ctx, operand, side[1])};
    if (ownSide[0] == ownSide[1]) {
        // Another surface of the same operand also crosses here (identical
        // shapes cancelling by parity, an internal face): nothing renders
        if (segmentCrossings(ctx, own, side[1], side[0]) != 0)
            return FALSE;

        // Otherwise P is merely off its own surface by more than epsilon:
        // probe the own operand further out
        float reach = epsilon;
        for (int retry = 0; retry < kOwnProbeRetries && ownSide[0] == ownSide[1]; retry++) {
            reach *= kOwnProbeGrowth;
            float far[2][3];
            for (int k = 0; k < 3; k++) {
                far[0][k] = P[k] + reach * n[k];
                far[1][k] = P[k] - reach * n[k];
            }
            ownSide[0] = classify(ctx, operand, far[0]);
            ownSide[1] = classify(ctx, operand, far[1]);
        }
        if (ownSide[0] == ownSide[1])
            return FALSE;
    }

    // Lazily classified memberships of every other operand, per side
    const int numOperands = (int)operands.size();
    std::vector<signed char> memo(2 * numOperands, -1);
    auto membership = [&](int s, int op) -> int {
        if (op == operand)
            return ownSide[s] == CSG_IN;
        signed char &m = memo[s * numOperands + op];
        if (m < 0)
            m = (signed char)(classify(ctx, op, side[s]) == CSG_IN);
        return m;
    };

    const int plus = definition->evaluate([&](int op) { return membership(0, op); });
    const int minus = definition->evaluate([&](int op) { return membership(1, op); });
    if (plus == minus)
        return FALSE;

    // A coincident face of a lower-indexed operand renders instead
    for (int op = 0; op < operand; op++)
        if (membership(0, op) != membership(1, op))
            return FALSE;

    return TRUE;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGEvaluator
// Method				:	classifyBound
// Description			:	ALL_IN / ALL_OUT when every other operand's
//							membership is provably constant over the box (the
//							box misses its bound, lies inside a convex implied
//							solid, or misses all its surfaces), and the leaf's
//							own operand has no other surface there; MIXED
//							otherwise
// Return Value			:	-
// Comments				:	Conservative: MIXED is always a safe answer
int CCSGEvaluator::classifyBound(CShadingContext *ctx, int operand, const float *bmin, const float *bmax) const {
    float lo[3], hi[3];
    for (int k = 0; k < 3; k++) {
        lo[k] = bmin[k] - epsilon;
        hi[k] = bmax[k] + epsilon;
    }

    // Another surface of the leaf's own operand in the box may coincide with it
    const CCSGResolvedOperand &own = *operands[operand];
    int ownSurfaces = 0;
    for (const CSurface *surface : own.surfaces)
        if (boxesOverlap(lo, hi, surface->bmin, surface->bmax))
            ownSurfaces++;
    if (ownSurfaces > 1)
        return REGION_MIXED;

    const int numOperands = (int)operands.size();
    std::vector<int> value(numOperands, CSG_OUT);
    for (int op = 0; op < numOperands; op++) {
        if (op == operand)
            continue;
        const CCSGResolvedOperand &other = *operands[op];
        if (other.kind == CSG_OPERAND_EMPTY || other.kind == CSG_OPERAND_OPEN || !boxesOverlap(lo, hi, other.bmin, other.bmax)) {
            value[op] = CSG_OUT;
            continue;
        }

        if (other.kind == CSG_OPERAND_SINGLE_QUADRIC && other.surfaces[0]->impliedSolidIsConvex()) {
            int corners = 0;
            for (int c = 0; c < 8; c++) {
                const float Q[3] = {(c & 1) ? hi[0] : lo[0], (c & 2) ? hi[1] : lo[1], (c & 4) ? hi[2] : lo[2]};
                corners += other.surfaces[0]->impliedSolidContains(Q) ? 1 : 0;
            }
            if (corners == 8) {
                value[op] = CSG_IN;
                continue;
            }
            if (corners > 0)
                return REGION_MIXED;
        }

        // The operand's boundary (its surfaces, and any implied cap, which
        // lies within its quadric's bound) misses the box: one value throughout
        bool touches = false;
        for (const CSurface *surface : other.surfaces)
            if (boxesOverlap(lo, hi, surface->bmin, surface->bmax)) {
                touches = true;
                break;
            }
        if (touches)
            return REGION_MIXED;
        const float centre[3] = {0.5f * (lo[0] + hi[0]), 0.5f * (lo[1] + hi[1]), 0.5f * (lo[2] + hi[2])};
        value[op] = classify(ctx, op, centre);
    }

    const int in = definition->evaluate([&](int op) { return op == operand ? 1 : value[op] == CSG_IN; });
    const int out = definition->evaluate([&](int op) { return op == operand ? 0 : value[op] == CSG_IN; });
    return in != out ? REGION_ALL_IN : REGION_ALL_OUT;
}

int CCSGRegion::accepts(CShadingContext *ctx, const float *P, const float *Ng) const {
    return evaluator->accepts(ctx, operand, P, Ng);
}

int CCSGRegion::classifyBound(CShadingContext *ctx, const float *bmin, const float *bmax) const {
    return evaluator->classifyBound(ctx, operand, bmin, bmax);
}
