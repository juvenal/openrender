/**
 * Project: openRender
 *
 * File: csgCaps.cpp
 *
 * Description:
 *   Implied-closure caps for single open quadric operands (spec 021,
 *   FR-021).
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

#include <algorithm>
#include <cmath>

#include "attributes.h"
#include "quadrics.h"
#include "xform.h"

// A flat cap disk is this much larger than the opening it closes, so it meets
// the lateral surface without a hairline; the cross-section test trims it
static const float kCapOversize = 1.02f;

// Inward step of the flat-cap cross-section test, relative to the quadric's size
static const float kCapProbeFraction = 1e-4f;

// Object-space vector to camera space (a direction: no translation)
static void toCameraDirection(const CXform *x, const float *v, float *out) {
    mulmv(out, x->from, v);
}

static void normalize3(float *v) {
    const float l = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (l > 0)
        for (int k = 0; k < 3; k++)
            v[k] /= l;
}

// The rendered normal of surface at its parametric centre (geometric normal,
// reversed as the sampler and intersectors reverse it)
static void renderedNormal(const CSurface *surface, float *N) {
    vector P;
    surface->samplePoint(0.5f, 0.5f, P, N);
    if ((surface->attributes->flags & ATTRIBUTES_FLAGS_INSIDE) ^ surface->xform->flip)
        mulvf(N, -1);
}

///////////////////////////////////////////////////////////////////////
// Function				:	addDiskCap
// Description			:	A flat cap: a disk of radius `radius` centred on
//							`centre` in the plane with outward normal `normal`
//							(both in the quadric's object space)
// Return Value			:	-
// Comments				:	The disk faces outward: if its own orientation
//							renders inward, it gets a clone of the attributes
//							with ATTRIBUTES_FLAGS_INSIDE toggled
static void addDiskCap(const CSurface *quadric, const float *centre, const float *normal, float radius, const float *side,
                       std::vector<CCSGCap> &caps) {
    CXform *x = new CXform(quadric->xform);
    x->translate(centre[0], centre[1], centre[2]);
    // Rotate the disk's +z onto `normal`
    const float cz = std::max(-1.0f, std::min(1.0f, normal[2]));
    const float axis[3] = {-normal[1], normal[0], 0};
    const float axisLength = sqrtf(axis[0] * axis[0] + axis[1] * axis[1]);
    if (axisLength > 1e-6f)
        x->rotate((float)(acos(cz) * 180.0 / C_PI), axis[0], axis[1], axis[2]);
    else if (cz < 0)
        x->rotate(180.0f, 1, 0, 0);

    CDisk *disk = new CDisk(quadric->attributes, x, NULL, 0, radius, 0.0f, (float)(2 * C_PI));

    CCSGCap cap;
    cap.surface = disk;
    cap.planar = TRUE;
    mulmp(cap.point, quadric->xform->from, centre);
    toCameraDirection(quadric->xform, normal, cap.normal);
    normalize3(cap.normal);
    cap.hasSide = side != NULL;
    if (side != NULL) {
        toCameraDirection(quadric->xform, side, cap.side);
        normalize3(cap.side);
    }
    else
        initv(cap.side, 0);
    cap.circles[0][0] = cap.circles[0][1] = cap.circles[1][0] = cap.circles[1][1] = 0;

    vector N;
    renderedNormal(disk, N);
    if (dotvv(N, cap.normal) < 0) {
        CAttributes *flipped = new CAttributes(disk->attributes);
        flipped->flags ^= ATTRIBUTES_FLAGS_INSIDE;
        flipped->attach();
        disk->attributes->detach();
        disk->attributes = flipped;
    }
    caps.push_back(cap);
}

// The two flat faces closing a partial sweep: the half-planes at azimuth 0
// and umax, each covered by a disk centred at (rho, z) on it
static void addSweepCaps(const CSurface *quadric, double umax, float rho, float z, float radius, std::vector<CCSGCap> &caps) {
    if (fabs(umax) >= 2 * C_PI)
        return;
    const double ends[2] = {0.0, umax};
    for (int e = 0; e < 2; e++) {
        const float c = (float)cos(ends[e]), s = (float)sin(ends[e]);
        const float centre[3] = {rho * c, rho * s, z};
        const float side[3] = {c, s, 0};
        // Outward: away from the swept interior
        const float sign = ((e == 0) == (umax > 0)) ? -1.0f : 1.0f;
        const float normal[3] = {-s * sign, c * sign, 0};
        addDiskCap(quadric, centre, normal, radius, side, caps);
    }
}

///////////////////////////////////////////////////////////////////////
// Function				:	buildImpliedCaps
// Description			:	See csgCaps.h
// Return Value			:	-
// Comments				:	Each cap is created attached once; the caller owns
//							that reference
void buildImpliedCaps(const CSurface *quadric, std::vector<CCSGCap> &caps) {
    caps.clear();
    const float up[3] = {0, 0, 1}, down[3] = {0, 0, -1};

    if (const CSphere *sphere = dynamic_cast<const CSphere *>(quadric)) {
        float r, umax, vmin, vmax;
        sphere->wireData(r, umax, vmin, vmax);
        r = fabsf(r);
        const float z0 = r * sinf(vmin), z1 = r * sinf(vmax);
        const float zlo = std::min(z0, z1), zhi = std::max(z0, z1);
        if (zlo > -r * (1 - 1e-6f)) {
            const float c[3] = {0, 0, zlo};
            addDiskCap(quadric, c, down, kCapOversize * sqrtf(std::max(0.0f, r * r - zlo * zlo)), NULL, caps);
        }
        if (zhi < r * (1 - 1e-6f)) {
            const float c[3] = {0, 0, zhi};
            addDiskCap(quadric, c, up, kCapOversize * sqrtf(std::max(0.0f, r * r - zhi * zhi)), NULL, caps);
        }
        addSweepCaps(quadric, umax, 0.5f * r, 0.5f * (zlo + zhi), kCapOversize * sqrtf(0.25f * r * r + 0.25f * (zhi - zlo) * (zhi - zlo)), caps);
    }
    else if (const CCylinder *cylinder = dynamic_cast<const CCylinder *>(quadric)) {
        float r, zmin, zmax, umax;
        cylinder->wireData(r, zmin, zmax, umax);
        r = fabsf(r);
        const float zlo = std::min(zmin, zmax), zhi = std::max(zmin, zmax);
        const float clo[3] = {0, 0, zlo}, chi[3] = {0, 0, zhi};
        addDiskCap(quadric, clo, down, kCapOversize * r, NULL, caps);
        addDiskCap(quadric, chi, up, kCapOversize * r, NULL, caps);
        addSweepCaps(quadric, umax, 0.5f * r, 0.5f * (zlo + zhi), kCapOversize * sqrtf(0.25f * r * r + 0.25f * (zhi - zlo) * (zhi - zlo)), caps);
    }
    else if (const CCone *cone = dynamic_cast<const CCone *>(quadric)) {
        float r, height, umax;
        cone->wireData(r, height, umax);
        r = fabsf(r);
        const float base[3] = {0, 0, 0};
        addDiskCap(quadric, base, height > 0 ? down : up, kCapOversize * r, NULL, caps);
        addSweepCaps(quadric, umax, 0.5f * r, 0.5f * height, kCapOversize * sqrtf(0.25f * r * r + 0.25f * height * height), caps);
    }
    else if (const CParaboloid *para = dynamic_cast<const CParaboloid *>(quadric)) {
        float r, zmin, zmax, umax;
        para->wireData(r, zmin, zmax, umax);
        r = fabsf(r);
        const float zlo = std::min(zmin, zmax), zhi = std::max(zmin, zmax);
        auto radiusAt = [&](float z) { return r * sqrtf(std::max(0.0f, z / zmax)); };
        const float ctop[3] = {0, 0, zhi};
        addDiskCap(quadric, ctop, up, kCapOversize * radiusAt(zhi), NULL, caps);
        if (zlo > 0) {
            const float cbot[3] = {0, 0, zlo};
            addDiskCap(quadric, cbot, down, kCapOversize * radiusAt(zlo), NULL, caps);
        }
        addSweepCaps(quadric, umax, 0.5f * r, 0.5f * (zlo + zhi), kCapOversize * sqrtf(0.25f * r * r + 0.25f * (zhi - zlo) * (zhi - zlo)), caps);
    }
    else if (const CHyperboloid *hyp = dynamic_cast<const CHyperboloid *>(quadric)) {
        const float *p1, *p2;
        float umax;
        hyp->wireData(p1, p2, umax);
        if (p1[2] == p2[2])
            return; // A flat ring encloses nothing
        const bool p1Low = p1[2] < p2[2];
        const float *lo = p1Low ? p1 : p2, *hi = p1Low ? p2 : p1;
        const float clo[3] = {0, 0, lo[2]}, chi[3] = {0, 0, hi[2]};
        addDiskCap(quadric, clo, down, kCapOversize * sqrtf(lo[0] * lo[0] + lo[1] * lo[1]), NULL, caps);
        addDiskCap(quadric, chi, up, kCapOversize * sqrtf(hi[0] * hi[0] + hi[1] * hi[1]), NULL, caps);
        // A partial sweep of a twisted hyperboloid has no flat gap faces; only
        // an untwisted one (p1, p2 at one azimuth) gets sweep caps
        const float cross = p1[0] * p2[1] - p1[1] * p2[0];
        if (fabsf(cross) < 1e-6f && fabs(umax) < 2 * C_PI) {
            const float rho = 0.5f * std::max(sqrtf(p1[0] * p1[0] + p1[1] * p1[1]), sqrtf(p2[0] * p2[0] + p2[1] * p2[1]));
            const float h = hi[2] - lo[2];
            addSweepCaps(quadric, umax, rho, 0.5f * (lo[2] + hi[2]), kCapOversize * sqrtf(rho * rho + 0.25f * h * h), caps);
        }
    }
    else if (const CToroid *torus = dynamic_cast<const CToroid *>(quadric)) {
        float R, r, vmin, vmax, umax;
        torus->wireData(R, r, vmin, vmax, umax);
        R = fabsf(R);
        r = fabsf(r);
        // Sweep gaps: the tube's circular cross-sections at azimuth 0 and umax
        if (fabs(umax) < 2 * C_PI) {
            const double ends[2] = {0.0, umax};
            for (int e = 0; e < 2; e++) {
                const float c = (float)cos(ends[e]), s = (float)sin(ends[e]);
                const float centre[3] = {R * c, R * s, 0};
                const float sign = ((e == 0) == (umax > 0)) ? -1.0f : 1.0f;
                const float normal[3] = {-s * sign, c * sign, 0};
                // Both ends can share one plane (a half sweep): each closes
                // only its own side of the axis
                const float side[3] = {c, s, 0};
                addDiskCap(quadric, centre, normal, kCapOversize * r, side, caps);
            }
        }
        // A tube cut across: the cone-shaped surface joining the cut circles
        if (fabsf(vmax - vmin) < 2 * C_PI) {
            const float a[3] = {R + r * cosf(vmin), 0, r * sinf(vmin)};
            const float b[3] = {R + r * cosf(vmax), 0, r * sinf(vmax)};
            CXform *x = new CXform(quadric->xform);
            CHyperboloid *cone = new CHyperboloid(quadric->attributes, x, NULL, 0, a, b, umax);
            CCSGCap cap;
            cap.surface = cone;
            cap.planar = FALSE;
            initv(cap.point, 0);
            initv(cap.normal, 0);
            cap.hasSide = FALSE;
            initv(cap.side, 0);
            cap.circles[0][0] = a[0];
            cap.circles[0][1] = a[2];
            cap.circles[1][0] = b[0];
            cap.circles[1][1] = b[2];
            caps.push_back(cap);
        }
    }

    for (CCSGCap &cap : caps)
        cap.surface->attach();
}

///////////////////////////////////////////////////////////////////////
// Function				:	capCrossSectionAccepts
// Description			:	See csgCaps.h
// Return Value			:	TRUE if P closes the opening
// Comments				:	A flat cap point closes the solid iff a point just
//							inside it (against its outward normal) is inside
//							the implied solid, and, for a sweep gap face, it
//							lies on that face's own half-plane. A tube-cut cap
//							point closes it iff it lies between the cut circles.
int capCrossSectionAccepts(const CSurface *quadric, const CCSGCap &cap, const float *P) {
    if (cap.planar) {
        const float probe = kCapProbeFraction * sqrtf((quadric->bmax[0] - quadric->bmin[0]) * (quadric->bmax[0] - quadric->bmin[0]) +
                                                      (quadric->bmax[1] - quadric->bmin[1]) * (quadric->bmax[1] - quadric->bmin[1]) +
                                                      (quadric->bmax[2] - quadric->bmin[2]) * (quadric->bmax[2] - quadric->bmin[2]));
        if (cap.hasSide) {
            // On the plane, the side direction measured from the quadric's axis
            vector axisPoint, d;
            const float origin[3] = {0, 0, 0};
            mulmp(axisPoint, quadric->xform->from, origin);
            subvv(d, P, axisPoint);
            if (dotvv(d, cap.side) < 0)
                return FALSE;
        }
        float inner[3];
        for (int k = 0; k < 3; k++)
            inner[k] = P[k] - probe * cap.normal[k];
        return quadric->impliedSolidContains(inner);
    }

    // Tube cut: the point's (rho, z) on the chord between the cut circles
    vector o;
    mulmp(o, quadric->xform->to, P);
    const float rho = sqrtf(o[0] * o[0] + o[1] * o[1]);
    const float ax = cap.circles[0][0], az = cap.circles[0][1], bx = cap.circles[1][0], bz = cap.circles[1][1];
    const float ex = bx - ax, ez = bz - az, len2 = ex * ex + ez * ez;
    if (!(len2 > 0))
        return FALSE;
    const float s = ((rho - ax) * ex + (o[2] - az) * ez) / len2;
    return s >= 0 && s <= 1;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CCSGCapRegion
// Method				:	accepts / classifyBound
// Description			:	The cross-section trim, then the boundary rule
// Return Value			:	-
// Comments				:	A cap patch is never ALL_IN: the cross-section trims
//							its rim
int CCSGCapRegion::accepts(CShadingContext *ctx, const float *P, const float *Ng) const {
    return capCrossSectionAccepts(quadric, cap, P) && evaluator->accepts(ctx, operand, P, Ng, TRUE);
}

int CCSGCapRegion::classifyBound(CShadingContext *ctx, const float *bmin, const float *bmax) const {
    return evaluator->classifyBound(ctx, operand, bmin, bmax) == REGION_ALL_OUT ? REGION_ALL_OUT : REGION_MIXED;
}
