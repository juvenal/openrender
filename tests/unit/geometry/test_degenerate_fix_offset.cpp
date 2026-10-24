/**
 * Project: openRender
 *
 * File: test_degenerate_fix_offset.cpp
 *
 * Description:
 *   Unit test for the degenerate-vector repair of a surface's sample() when
 *   it is called with start > 0, as the raytracer does for the derivative
 *   points (shading.cpp: sample(numRealVertices, 2 * numRealVertices, ...)).
 *   normalFix() and tangentFix() (geometry/patchUtils.h) must repair Ng,
 *   dPdu and dPdv over the points of the call, [start, start + numVertices),
 *   with neighbours taken from that range only, and must leave the points
 *   before start alone.
 *
 *   A bilinear patch with two coincident corners has a collapsed edge: with
 *   P2 == P3, dPdu and Ng are 0 along v = 1; with P1 == P3, dPdv and Ng are 0
 *   along u = 1. Slot 0 holds a sentinel point (as the main intersection
 *   points of a raytraced batch do), close in (u, v) to the degenerate point
 *   but with recognisable vectors; slots 1 and 2 are sampled with
 *   sample(1, 2, ...), one regular and one on the collapsed edge, in both
 *   orders. The degenerate point must take exactly the regular point's
 *   vector.
 *
 *   With no camera transform before RiWorldBegin, camera space equals the
 *   patch's object space.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "common/rendererc.h"
#include "csgTestUtils.h"

using namespace csgtest;

namespace {

    const float kSentinel[3] = {7.0f, -5.0f, 3.0f};
    const int kSlots = 3;
    const int kStart = 1;
    const int kCount = 2;

    // The varying arrays of one three-point batch
    struct CBatch {
            float data[VARIABLE_CONSTANTWIDTH + 1][kSlots * 16];
            float *varying[VARIABLE_CONSTANTWIDTH + 1];

            CBatch() {
                for (unsigned int k = 0; k <= VARIABLE_CONSTANTWIDTH; k++) {
                    for (int j = 0; j < kSlots * 16; j++)
                        data[k][j] = 0;
                    varying[k] = data[k];
                }
            }

            float *vec(unsigned int variable, int slot) { return varying[variable] + 3 * slot; }
    };

    // The single bilinear patch surface of RiPatch(bilinear, P)
    CSurface *bilinearPatch(CCsgSession &s, float *P) {
        CObject *o = s.one([&] { RiPatch(RI_BILINEAR, RI_P, P, RI_NULL); });
        if (o == NULL)
            return NULL;
        std::vector<CSurface *> surfaces;
        collectSurfaces(s.ctx(), o, surfaces);
        return surfaces.size() == 1 ? surfaces[0] : NULL;
    }

    // Samples slots 1 and 2 at (u1, v1) and (u2, v2) with start = 1. Slot 0
    // is at (u0, v0) with every vector set to the sentinel.
    void sampleBatch(const CSurface *surface, CBatch &b, float u0, float v0, float u1, float v1, float u2, float v2) {
        const float us[kSlots] = {u0, u1, u2};
        const float vs[kSlots] = {v0, v1, v2};
        for (int i = 0; i < kSlots; i++) {
            b.varying[VARIABLE_U][i] = us[i];
            b.varying[VARIABLE_V][i] = vs[i];
            b.varying[VARIABLE_TIME][i] = 0;
        }
        for (unsigned int variable : {VARIABLE_NG, VARIABLE_DPDU, VARIABLE_DPDV})
            movvv(b.vec(variable, 0), kSentinel);
        unsigned int up = PARAMETER_P | PARAMETER_DPDU | PARAMETER_DPDV | PARAMETER_NG | PARAMETER_BEGIN_SAMPLE;
        surface->sample(kStart, kCount, b.varying, NULL, up);
    }

    bool finiteNonZero(const float *x) {
        return std::isfinite(x[0]) && std::isfinite(x[1]) && std::isfinite(x[2]) && dotvv(x, x) > 0;
    }

    bool same(const float *a, const float *b) { return a[0] == b[0] && a[1] == b[1] && a[2] == b[2]; }

    // The degenerate slot took the regular slot's vector, and slot 0 kept the sentinel
    bool repaired(CBatch &b, unsigned int variable, int degenerate, int regular) {
        const float *d = b.vec(variable, degenerate);
        const float *r = b.vec(variable, regular);
        if (!finiteNonZero(r)) {
            printf("\n  regular slot %d of variable %u is degenerate itself", regular, variable);
            return false;
        }
        if (!finiteNonZero(d) || !same(d, r)) {
            printf("\n  slot %d of variable %u is (%g %g %g), expected slot %d's (%g %g %g)", degenerate, variable,
                   d[0], d[1], d[2], regular, r[0], r[1], r[2]);
            return false;
        }
        if (!same(b.vec(variable, 0), kSentinel)) {
            printf("\n  slot 0 of variable %u changed", variable);
            return false;
        }
        return true;
    }

    // P2 == P3: dPdu and Ng vanish along v = 1
    float collapsedV1[12] = {0, 0, 0, 1, 0, 0, 0.5f, 1, 0, 0.5f, 1, 0};
    // P1 == P3: dPdv and Ng vanish along u = 1
    float collapsedU1[12] = {0, 0, 0, 1, 0.5f, 0, 0, 1, 0, 1, 0.5f, 0};

} // namespace

// The degenerate point is the last of the call: a repair that starts at slot 0
// never reaches it
TEST(ng_and_dpdu_repaired_when_degenerate_point_is_last) {
    CCsgSession s;
    CSurface *patch = bilinearPatch(s, collapsedV1);
    ASSERT(patch != NULL);
    CBatch b;
    sampleBatch(patch, b, 0.5f, 0.99f, 0.5f, 0.5f, 0.5f, 1.0f);
    // Both checked, so a failure reports every vector left unrepaired
    const bool normal = repaired(b, VARIABLE_NG, 2, 1);
    const bool tangent = repaired(b, VARIABLE_DPDU, 2, 1);
    ASSERT(normal && tangent);
}

// The degenerate point is the first of the call: a repair that starts at slot 0
// takes the neighbour from slot 0, outside the call
TEST(ng_and_dpdu_repaired_when_degenerate_point_is_first) {
    CCsgSession s;
    CSurface *patch = bilinearPatch(s, collapsedV1);
    ASSERT(patch != NULL);
    CBatch b;
    sampleBatch(patch, b, 0.5f, 0.99f, 0.5f, 1.0f, 0.5f, 0.5f);
    // Both checked, so a failure reports every vector left unrepaired
    const bool normal = repaired(b, VARIABLE_NG, 1, 2);
    const bool tangent = repaired(b, VARIABLE_DPDU, 1, 2);
    ASSERT(normal && tangent);
}

TEST(dpdv_repaired_when_degenerate_point_is_last) {
    CCsgSession s;
    CSurface *patch = bilinearPatch(s, collapsedU1);
    ASSERT(patch != NULL);
    CBatch b;
    sampleBatch(patch, b, 0.99f, 0.5f, 0.5f, 0.5f, 1.0f, 0.5f);
    // Both checked, so a failure reports every vector left unrepaired
    const bool normal = repaired(b, VARIABLE_NG, 2, 1);
    const bool tangent = repaired(b, VARIABLE_DPDV, 2, 1);
    ASSERT(normal && tangent);
}

TEST(dpdv_repaired_when_degenerate_point_is_first) {
    CCsgSession s;
    CSurface *patch = bilinearPatch(s, collapsedU1);
    ASSERT(patch != NULL);
    CBatch b;
    sampleBatch(patch, b, 0.99f, 0.5f, 1.0f, 0.5f, 0.5f, 0.5f);
    // Both checked, so a failure reports every vector left unrepaired
    const bool normal = repaired(b, VARIABLE_NG, 1, 2);
    const bool tangent = repaired(b, VARIABLE_DPDV, 1, 2);
    ASSERT(normal && tangent);
}

int main() {
    RUN(ng_and_dpdu_repaired_when_degenerate_point_is_last);
    RUN(ng_and_dpdu_repaired_when_degenerate_point_is_first);
    RUN(dpdv_repaired_when_degenerate_point_is_last);
    RUN(dpdv_repaired_when_degenerate_point_is_first);
    SUMMARY();
}
