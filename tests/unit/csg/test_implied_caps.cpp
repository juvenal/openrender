/**
 * Project: openRender
 *
 * File: test_implied_caps.cpp
 *
 * Description:
 *   Unit test (T042, spec 021) for implied-closure caps (FR-021, research.md
 *   Decision 7). For each single-quadric kind: the number of caps, the plane
 *   of each flat cap (or, for a torus cut across its tube, its two cut
 *   circles), its orientation (out of the implied solid, as the cap surface
 *   itself renders), and that each cap's trim accepts exactly the open
 *   cross-section inside the lateral surface. A full sphere or torus, and a
 *   disk, get no caps.
 *
 *   With no camera transform before RiWorldBegin, camera space equals the
 *   quadrics' object space.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "csgCaps.h"
#include "csgTestUtils.h"
#include "common/rendererc.h"

using namespace csgtest;

static std::vector<CCSGCap> capsOf(CCsgSession &s, const std::function<void()> &emit, CSurface **quadric) {
    *quadric = dynamic_cast<CSurface *>(s.one(emit));
    std::vector<CCSGCap> caps;
    if (*quadric != NULL)
        buildImpliedCaps(*quadric, caps);
    return caps;
}

// The flat cap whose plane passes through p with normal n (either sign of n
// is matched against the cap's own outward normal, which must equal n)
static const CCSGCap *flatCap(const std::vector<CCSGCap> &caps, float px, float py, float pz, float nx, float ny,
                              float nz) {
    for (const CCSGCap &c : caps) {
        if (!c.planar)
            continue;
        const float d = (px - c.point[0]) * c.normal[0] + (py - c.point[1]) * c.normal[1] + (pz - c.point[2]) * c.normal[2];
        if (std::fabs(d) < 1e-5f && std::fabs(c.normal[0] - nx) < 1e-5f && std::fabs(c.normal[1] - ny) < 1e-5f &&
            std::fabs(c.normal[2] - nz) < 1e-5f)
            return &c;
    }
    return NULL;
}

// The cap surface's rendered normal at the middle of its parameter range:
// its geometric normal, reversed when the sampler and intersectors reverse it
static void renderedNormal(const CSurface *surface, float *N) {
    float u = 0.5f, v = 0.5f, time = 0, P[3], du[3], dv[3], ng[3], dt[3], pw[4];
    float *varying[VARIABLE_CONSTANTWIDTH + 1];
    for (unsigned int k = 0; k <= VARIABLE_CONSTANTWIDTH; k++)
        varying[k] = NULL;
    varying[VARIABLE_U] = &u;
    varying[VARIABLE_V] = &v;
    varying[VARIABLE_TIME] = &time;
    varying[VARIABLE_P] = P;
    varying[VARIABLE_DPDU] = du;
    varying[VARIABLE_DPDV] = dv;
    varying[VARIABLE_NG] = ng;
    varying[VARIABLE_DPDTIME] = dt;
    varying[VARIABLE_PW] = pw;
    unsigned int up = PARAMETER_P | PARAMETER_DPDU | PARAMETER_DPDV | PARAMETER_NG | PARAMETER_BEGIN_SAMPLE;
    surface->sample(0, 1, varying, NULL, up);
    const float sign = ((surface->attributes->flags & ATTRIBUTES_FLAGS_INSIDE) ^ surface->xform->flip) ? -1.0f : 1.0f;
    for (int k = 0; k < 3; k++)
        N[k] = sign * ng[k];
}

static bool facesAlong(const CCSGCap &cap) {
    float N[3];
    renderedNormal(cap.surface, N);
    return N[0] * cap.normal[0] + N[1] * cap.normal[1] + N[2] * cap.normal[2] > 0;
}

static int accepts(const CSurface *q, const CCSGCap *cap, float x, float y, float z) {
    const float P[3] = {x, y, z};
    return capCrossSectionAccepts(q, *cap, P);
}

TEST(closed_shapes_and_disks_get_no_caps) {
    CCsgSession s;
    CSurface *q;
    ASSERT(capsOf(s, [] { RiSphere(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL); }, &q).empty() && q != NULL);
    ASSERT(capsOf(s, [] { RiTorus(1.0f, 0.25f, 0.0f, 360.0f, 360.0f, RI_NULL); }, &q).empty() && q != NULL);
    ASSERT(capsOf(s, [] { RiDisk(0.0f, 1.0f, 360.0f, RI_NULL); }, &q).empty() && q != NULL);
}

TEST(cylinder_end_caps) {
    CCsgSession s;
    CSurface *q;
    std::vector<CCSGCap> caps = capsOf(s, [] { RiCylinder(1.0f, -1.0f, 1.0f, 360.0f, RI_NULL); }, &q);
    ASSERT(caps.size() == 2);
    const CCSGCap *top = flatCap(caps, 0, 0, 1, 0, 0, 1), *bottom = flatCap(caps, 0, 0, -1, 0, 0, -1);
    ASSERT(top != NULL && bottom != NULL);
    ASSERT(facesAlong(*top) && facesAlong(*bottom));
    ASSERT(accepts(q, top, 0.5f, 0, 1) == TRUE);
    ASSERT(accepts(q, top, 1.5f, 0, 1) == FALSE);
    ASSERT(accepts(q, bottom, 0, -0.9f, -1) == TRUE);
    ASSERT(accepts(q, bottom, 0, -1.1f, -1) == FALSE);
}

TEST(z_cut_sphere_caps) {
    CCsgSession s;
    CSurface *q;
    std::vector<CCSGCap> caps = capsOf(s, [] { RiSphere(1.0f, -0.5f, 0.5f, 360.0f, RI_NULL); }, &q);
    ASSERT(caps.size() == 2);
    const CCSGCap *top = flatCap(caps, 0, 0, 0.5f, 0, 0, 1), *bottom = flatCap(caps, 0, 0, -0.5f, 0, 0, -1);
    ASSERT(top != NULL && bottom != NULL);
    ASSERT(facesAlong(*top) && facesAlong(*bottom));
    ASSERT(accepts(q, top, 0.8f, 0, 0.5f) == TRUE);   // inside r = sqrt(0.75) = 0.866
    ASSERT(accepts(q, top, 0.9f, 0, 0.5f) == FALSE);
}

TEST(half_sweep_cylinder_end_and_sweep_gap_caps) {
    CCsgSession s;
    CSurface *q;
    std::vector<CCSGCap> caps = capsOf(s, [] { RiCylinder(1.0f, -1.0f, 1.0f, 180.0f, RI_NULL); }, &q);
    ASSERT(caps.size() == 4); // two half-disc ends, two half-planes closing the sweep gap
    const CCSGCap *top = flatCap(caps, 0, 0, 1, 0, 0, 1);
    ASSERT(top != NULL && facesAlong(*top));
    ASSERT(accepts(q, top, 0, 0.5f, 1) == TRUE);   // the y >= 0 half
    ASSERT(accepts(q, top, 0, -0.5f, 1) == FALSE);
    // Both sweep-gap faces lie in y = 0 and face -y, one at x > 0, one at x < 0
    int gaps = 0, positive = 0, negative = 0;
    for (const CCSGCap &c : caps) {
        if (!c.planar || std::fabs(c.normal[1] + 1) > 1e-5f || std::fabs(c.point[1]) > 1e-5f)
            continue;
        gaps++;
        ASSERT(facesAlong(c));
        positive += accepts(q, &c, 0.5f, 0, 0);
        negative += accepts(q, &c, -0.5f, 0, 0);
        ASSERT(accepts(q, &c, 1.5f, 0, 0) == FALSE);
        ASSERT(accepts(q, &c, 0.5f, 0, 1.5f) == FALSE);
    }
    ASSERT(gaps == 2);
    ASSERT(positive == 1 && negative == 1);
}

TEST(cone_base_cap) {
    CCsgSession s;
    CSurface *q;
    std::vector<CCSGCap> caps = capsOf(s, [] { RiCone(1.0f, 1.0f, 360.0f, RI_NULL); }, &q);
    ASSERT(caps.size() == 1);
    const CCSGCap *base = flatCap(caps, 0, 0, 0, 0, 0, -1);
    ASSERT(base != NULL && facesAlong(*base));
    ASSERT(accepts(q, base, 0.5f, 0, 0) == TRUE);
    ASSERT(accepts(q, base, 1.2f, 0, 0) == FALSE);
}

TEST(paraboloid_top_and_bottom_caps) {
    CCsgSession s;
    CSurface *q;
    std::vector<CCSGCap> caps = capsOf(s, [] { RiParaboloid(1.0f, 0.0f, 1.0f, 360.0f, RI_NULL); }, &q);
    ASSERT(caps.size() == 1); // zmin = 0 is the apex: only the top is open
    const CCSGCap *top = flatCap(caps, 0, 0, 1, 0, 0, 1);
    ASSERT(top != NULL && facesAlong(*top));
    ASSERT(accepts(q, top, 0.9f, 0, 1) == TRUE);
    ASSERT(accepts(q, top, 1.1f, 0, 1) == FALSE);

    caps = capsOf(s, [] { RiParaboloid(1.0f, 0.25f, 1.0f, 360.0f, RI_NULL); }, &q);
    ASSERT(caps.size() == 2);
    const CCSGCap *bottom = flatCap(caps, 0, 0, 0.25f, 0, 0, -1);
    ASSERT(bottom != NULL && facesAlong(*bottom));
    ASSERT(accepts(q, bottom, 0.4f, 0, 0.25f) == TRUE);  // r = sqrt(0.25) = 0.5 there
    ASSERT(accepts(q, bottom, 0.6f, 0, 0.25f) == FALSE);
}

TEST(hyperboloid_end_caps) {
    CCsgSession s;
    CSurface *q;
    RtPoint p1 = {1, 0, -1}, p2 = {1, 0, 1};
    std::vector<CCSGCap> caps = capsOf(s, [&] { RiHyperboloid(p1, p2, 360.0f, RI_NULL); }, &q);
    ASSERT(caps.size() == 2);
    const CCSGCap *top = flatCap(caps, 0, 0, 1, 0, 0, 1), *bottom = flatCap(caps, 0, 0, -1, 0, 0, -1);
    ASSERT(top != NULL && bottom != NULL);
    ASSERT(facesAlong(*top) && facesAlong(*bottom));
    ASSERT(accepts(q, top, 0.9f, 0, 1) == TRUE);
    ASSERT(accepts(q, top, 1.1f, 0, 1) == FALSE);
}

TEST(partial_sweep_torus_caps) {
    CCsgSession s;
    CSurface *q;
    std::vector<CCSGCap> caps = capsOf(s, [] { RiTorus(1.0f, 0.25f, 0.0f, 360.0f, 180.0f, RI_NULL); }, &q);
    ASSERT(caps.size() == 2); // the tube's circular cross-sections at azimuth 0 and 180
    int positive = 0, negative = 0;
    for (const CCSGCap &c : caps) {
        ASSERT(c.planar);
        ASSERT(std::fabs(c.point[1]) < 1e-5f && std::fabs(c.normal[1] + 1) < 1e-5f);
        ASSERT(facesAlong(c));
        positive += accepts(q, &c, 1.1f, 0, 0.1f);
        negative += accepts(q, &c, -1.1f, 0, 0.1f);
        ASSERT(accepts(q, &c, 1.4f, 0, 0) == FALSE);   // outside the tube
        ASSERT(accepts(q, &c, 0.5f, 0, 0) == FALSE);   // the hole
    }
    ASSERT(positive == 1 && negative == 1);
}

TEST(tube_cut_torus_cone_shaped_cap) {
    CCsgSession s;
    CSurface *q;
    std::vector<CCSGCap> caps = capsOf(s, [] { RiTorus(1.0f, 0.25f, -45.0f, 90.0f, 360.0f, RI_NULL); }, &q);
    ASSERT(caps.size() == 1);
    const CCSGCap &cap = caps[0];
    ASSERT(!cap.planar);
    // The two cut circles, (rho, z) = (R + r cos phi, r sin phi)
    const float a[2] = {1.0f + 0.25f * (float)std::cos(-M_PI / 4), 0.25f * (float)std::sin(-M_PI / 4)};
    const float b[2] = {1.0f, 0.25f};
    const bool ab = std::fabs(cap.circles[0][0] - a[0]) < 1e-4f && std::fabs(cap.circles[0][1] - a[1]) < 1e-4f &&
                    std::fabs(cap.circles[1][0] - b[0]) < 1e-4f && std::fabs(cap.circles[1][1] - b[1]) < 1e-4f;
    const bool ba = std::fabs(cap.circles[1][0] - a[0]) < 1e-4f && std::fabs(cap.circles[1][1] - a[1]) < 1e-4f &&
                    std::fabs(cap.circles[0][0] - b[0]) < 1e-4f && std::fabs(cap.circles[0][1] - b[1]) < 1e-4f;
    ASSERT(ab || ba);
    // The chord's midpoint lies in the cross-section; a point on the chord's
    // line beyond its end, outside the tube, does not
    ASSERT(accepts(q, &cap, 0.5f * (a[0] + b[0]), 0, 0.5f * (a[1] + b[1])) == TRUE);
    ASSERT(accepts(q, &cap, b[0] + 0.5f * (b[0] - a[0]), 0, b[1] + 0.5f * (b[1] - a[1])) == FALSE);
}

int main() {
    RUN(closed_shapes_and_disks_get_no_caps);
    RUN(cylinder_end_caps);
    RUN(z_cut_sphere_caps);
    RUN(half_sweep_cylinder_end_and_sweep_gap_caps);
    RUN(cone_base_cap);
    RUN(paraboloid_top_and_bottom_caps);
    RUN(hyperboloid_end_caps);
    RUN(partial_sweep_torus_caps);
    RUN(tube_cut_torus_cone_shaped_cap);
    SUMMARY();
}
