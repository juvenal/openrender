/**
 * Project: openRender
 *
 * File: csgTreeHelpers.h
 *
 * Description:
 *   Compact tree construction and composite-membership helpers shared by the
 *   rewritten CSG suites (spec 021, T022/T023): build a CCSGDefinition from a
 *   nested L()/N() description, evaluate the solid's membership at a point
 *   from per-operand classification, and estimate a solid's volume by
 *   classifying the centres of a regular grid.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef CSG_TREE_HELPERS_H
#define CSG_TREE_HELPERS_H

#include "csgTestUtils.h"

namespace csgtest {

    struct TreeSpec {
            int operand = -1;
            ECSGOperation operation = CSG_UNION;
            std::vector<TreeSpec> children;
    };

    inline TreeSpec L(int operand) {
        TreeSpec t;
        t.operand = operand;
        return t;
    }

    inline TreeSpec N(ECSGOperation op, std::vector<TreeSpec> children) {
        TreeSpec t;
        t.operation = op;
        t.children = children;
        return t;
    }

    inline int buildTree(CCSGDefinition *def, const TreeSpec &t) {
        if (t.operand >= 0)
            return def->addLeaf(t.operand);
        int node = def->addNode(t.operation);
        for (const TreeSpec &c : t.children)
            def->addChild(node, buildTree(def, c));
        return node;
    }

    inline CCSGDefinition *definition(int numOperands, const TreeSpec &tree) {
        CCSGDefinition *def = new CCSGDefinition();
        for (int i = 0; i < numOperands; i++)
            def->addOperand();
        def->setRoot(buildTree(def, tree));
        def->finalize(NULL);
        return def;
    }

    // The solid's membership at P
    inline int inside(const CCSGDefinition *def, const std::vector<CCSGResolvedOperand *> &ops, CShadingContext *ctx,
                      const float *P) {
        return def->evaluate([&](int op) { return classifyOperand(ctx, *ops[op], P) == CSG_IN; });
    }

    // Volume estimated from the centres of an n x n x n grid over [lo, hi]
    inline double volume(const CCSGDefinition *def, const std::vector<CCSGResolvedOperand *> &ops, CShadingContext *ctx,
                         const float *lo, const float *hi, int n) {
        int count = 0;
        for (const std::vector<float> &p : gridPoints(lo, hi, n))
            count += inside(def, ops, ctx, p.data());
        const double cell = (double)(hi[0] - lo[0]) * (hi[1] - lo[1]) * (hi[2] - lo[2]) / ((double)n * n * n);
        return count * cell;
    }

    // The volume the same grid gives an analytic membership test: the exact
    // answer volume() must reproduce, free of the grid's own sampling error
    inline double truthVolume(const float *lo, const float *hi, int n, const std::function<bool(const float *)> &in) {
        int count = 0;
        for (const std::vector<float> &p : gridPoints(lo, hi, n))
            count += in(p.data()) ? 1 : 0;
        const double cell = (double)(hi[0] - lo[0]) * (hi[1] - lo[1]) * (hi[2] - lo[2]) / ((double)n * n * n);
        return count * cell;
    }

    // accepts() for a point on a sphere leaf, with the sphere's normal
    inline int onSphere(const CCSGEvaluator &ev, int operand, float cx, float cy, float cz, float x, float y, float z) {
        const float P[3] = {x, y, z}, Ng[3] = {x - cx, y - cy, z - cz};
        return ev.accepts(NULL, operand, P, Ng);
    }

    // accepts() for a point on a planar leaf with normal (nx, ny, nz)
    inline int onPlane(const CCSGEvaluator &ev, CShadingContext *ctx, int operand, float x, float y, float z, float nx,
                       float ny, float nz) {
        const float P[3] = {x, y, z}, Ng[3] = {nx, ny, nz};
        return ev.accepts(ctx, operand, P, Ng);
    }

} // namespace csgtest

#endif
