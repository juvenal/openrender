/**
 * Project: openRender
 *
 * File: test_classify_determinism.cpp
 *
 * Description:
 *   Unit test (T019, spec 021) for FR-008: classification is a pure function
 *   of the point. The same point set, classified against analytic (quadric),
 *   polygon-mesh and bilinear-patch operands in forward order, in shuffled
 *   order and from 8 threads at once, must give bit-identical answers, and
 *   those answers must match the analytic shapes (so a classifier that
 *   ignores its input cannot pass).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <random>
#include <thread>

#include "csgTestUtils.h"

using namespace csgtest;

static const float LO[3] = {-0.5f, -0.5f, -0.5f}, HI[3] = {0.5f, 0.5f, 0.5f};

static std::vector<int> classifyAll(CShadingContext *ctx, const CCSGResolvedOperand &op,
                                    const std::vector<std::vector<float>> &pts, const std::vector<size_t> &order) {
    std::vector<int> out(pts.size(), -1);
    for (size_t i : order)
        out[i] = classifyOperand(ctx, op, pts[i].data());
    return out;
}

static bool deterministic(CShadingContext *ctx, const CCSGResolvedOperand &op, const std::vector<std::vector<float>> &pts,
                          const std::function<bool(const float *)> &truth) {
    std::vector<size_t> forward(pts.size());
    for (size_t i = 0; i < pts.size(); i++)
        forward[i] = i;
    std::vector<size_t> shuffled = forward;
    std::mt19937 rng(21);
    std::shuffle(shuffled.begin(), shuffled.end(), rng);

    const std::vector<int> a = classifyAll(ctx, op, pts, forward);
    const std::vector<int> b = classifyAll(ctx, op, pts, shuffled);

    // 8 threads, each classifying an interleaved share of the points. Every
    // operand here is analytic, polygonal or bilinear, which classify with a
    // NULL context, so no thread shares a shading context.
    std::vector<int> c(pts.size(), -1);
    std::vector<std::thread> threads;
    for (int t = 0; t < 8; t++)
        threads.emplace_back([&, t] {
            for (size_t i = t; i < pts.size(); i += 8)
                c[i] = classifyOperand(NULL, op, pts[i].data());
        });
    for (std::thread &th : threads)
        th.join();

    if (a != b || a != c)
        return false;
    for (size_t i = 0; i < pts.size(); i++)
        if (a[i] != (truth(pts[i].data()) ? CSG_IN : CSG_OUT))
            return false;
    return true;
}

TEST(quadric_operand) {
    CCsgSession s;
    CCSGResolvedOperand *op = makeOperand(s.ctx(), {sphere(s, 0, 0, 0, 0.7f)});
    const float lo[3] = {-1, -1, -1}, hi[3] = {1, 1, 1};
    ASSERT(deterministic(NULL, *op, gridPoints(lo, hi, 11),
                         [](const float *P) { return insideSphere(P, 0, 0, 0, 0.7f); }));
}

TEST(polygon_mesh_operand) {
    CCsgSession s;
    RtInt nverts[6] = {4, 4, 4, 4, 4, 4};
    RtInt verts[24] = {0, 3, 2, 1, 4, 5, 6, 7, 0, 1, 5, 4, 2, 3, 7, 6, 1, 2, 6, 5, 0, 4, 7, 3};
    RtPoint P[8] = {{-0.5f, -0.5f, -0.5f}, {0.5f, -0.5f, -0.5f}, {0.5f, 0.5f, -0.5f}, {-0.5f, 0.5f, -0.5f},
                    {-0.5f, -0.5f, 0.5f},  {0.5f, -0.5f, 0.5f},  {0.5f, 0.5f, 0.5f},  {-0.5f, 0.5f, 0.5f}};
    CObject *mesh = s.one([&] { RiPointsPolygons(6, nverts, verts, RI_P, (RtPointer)P, RI_NULL); });
    ASSERT(mesh != NULL);
    CCSGResolvedOperand *op = makeOperand(s.ctx(), {mesh});
    ASSERT(op->surfaces.size() >= 6);
    const float lo[3] = {-0.9f, -0.9f, -0.9f}, hi[3] = {0.9f, 0.9f, 0.9f};
    ASSERT(deterministic(NULL, *op, gridPoints(lo, hi, 9), [](const float *Q) { return insideBox(Q, LO, HI); }));
}

TEST(bilinear_patch_operand) {
    CCsgSession s;
    CCSGResolvedOperand *op = makeOperand(s.ctx(), boxPatches(s, LO, HI));
    ASSERT(op->surfaces.size() == 6);
    const float lo[3] = {-0.9f, -0.9f, -0.9f}, hi[3] = {0.9f, 0.9f, 0.9f};
    ASSERT(deterministic(NULL, *op, gridPoints(lo, hi, 9), [](const float *Q) { return insideBox(Q, LO, HI); }));
}

int main() {
    RUN(quadric_operand);
    RUN(polygon_mesh_operand);
    RUN(bilinear_patch_operand);
    SUMMARY();
}
