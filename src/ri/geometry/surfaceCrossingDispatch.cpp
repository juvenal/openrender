/**
 * Project: openRender
 *
 * File: surfaceCrossingDispatch.cpp
 *
 * Description:
 *   Renderer-only crossing counts for free-form surfaces: a pure Newton
 *   core seeded from an immutable per-surface evaluation grid (spec 021).
 *   ribVector links vector/surfaceCrossingDispatchStub.cpp instead.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "surfaceCrossingDispatch.h"

#include <algorithm>
#include <cmath>
#include <mutex>

#include "object.h"
#include "rendererc.h"

// Newton refinement limits. Surfaces evaluate in float (sample()), so the
// residual bottoms out near float resolution: iteration stops once the
// parametric step is negligible, and a root is accepted at float-level residual.
static const int kNewtonIterations = 32;
static const double kNewtonTolerance = 1e-10; // Residual for early exit, relative to the grid's size
static const double kNewtonStep = 1e-12;      // Parametric step below which iteration stops
static const double kAcceptTolerance = 1e-6;  // Residual a converged root must reach, relative to the grid's size
static const double kDuplicateDistance = 1e-5; // Roots closer than this along the ray (relative to the grid's size) are one

// Free-form seed grid resolution per surface (spec 021, research.md Decision 3)
static const int kSeedGridU = 8;
static const int kSeedGridV = 8;

///////////////////////////////////////////////////////////////////////
// Class				:	CCrossingSeedGrid
// Method				:	CCrossingSeedGrid
// Description			:	Sample the surface on a regular grid and bound each
//							cell, padded by how far the surface bulges away
//							from the cell's corners at its centre and edge
//							midpoints
// Return Value			:	-
// Comments				:
CCrossingSeedGrid::CCrossingSeedGrid(const CCrossingSurface &surface, int nu, int nv) : nu(nu), nv(nv) {
    double du[3], dv[3];
    P.resize((size_t)(nu + 1) * (nv + 1) * 3);
    for (int j = 0; j <= nv; j++)
        for (int i = 0; i <= nu; i++) {
            const double u = surface.umin + (surface.umax - surface.umin) * i / nu;
            const double v = surface.vmin + (surface.vmax - surface.vmin) * j / nv;
            surface.evaluate(u, v, &P[((size_t)j * (nu + 1) + i) * 3], du, dv);
        }

    double gmin[3] = {HUGE_VAL, HUGE_VAL, HUGE_VAL}, gmax[3] = {-HUGE_VAL, -HUGE_VAL, -HUGE_VAL};
    cellMin.resize((size_t)nu * nv * 3);
    cellMax.resize((size_t)nu * nv * 3);
    for (int j = 0; j < nv; j++)
        for (int i = 0; i < nu; i++) {
            const double *c[4] = {&P[((size_t)j * (nu + 1) + i) * 3], &P[((size_t)j * (nu + 1) + i + 1) * 3],
                                  &P[((size_t)(j + 1) * (nu + 1) + i) * 3], &P[((size_t)(j + 1) * (nu + 1) + i + 1) * 3]};
            double *lo = &cellMin[((size_t)j * nu + i) * 3], *hi = &cellMax[((size_t)j * nu + i) * 3];
            for (int k = 0; k < 3; k++) {
                lo[k] = std::min(std::min(c[0][k], c[1][k]), std::min(c[2][k], c[3][k]));
                hi[k] = std::max(std::max(c[0][k], c[1][k]), std::max(c[2][k], c[3][k]));
            }

            // Bulge: the surface at (s, t) in the cell versus the bilinear
            // interpolation of its corners
            static const double probes[5][2] = {{0.5, 0.5}, {0.5, 0}, {0.5, 1}, {0, 0.5}, {1, 0.5}};
            double bulge = 0;
            for (const double *pr : probes) {
                const double s = pr[0], t = pr[1];
                const double u = surface.umin + (surface.umax - surface.umin) * (i + s) / nu;
                const double v = surface.vmin + (surface.vmax - surface.vmin) * (j + t) / nv;
                double Q[3];
                surface.evaluate(u, v, Q, du, dv);
                double dev = 0;
                for (int k = 0; k < 3; k++) {
                    const double b = (1 - s) * (1 - t) * c[0][k] + s * (1 - t) * c[1][k] + (1 - s) * t * c[2][k] + s * t * c[3][k];
                    dev = std::max(dev, std::fabs(Q[k] - b));
                    lo[k] = std::min(lo[k], Q[k]);
                    hi[k] = std::max(hi[k], Q[k]);
                }
                bulge = std::max(bulge, dev);
            }
            for (int k = 0; k < 3; k++) {
                lo[k] -= 2 * bulge;
                hi[k] += 2 * bulge;
                gmin[k] = std::min(gmin[k], lo[k]);
                gmax[k] = std::max(gmax[k], hi[k]);
            }
        }
    scale = std::sqrt((gmax[0] - gmin[0]) * (gmax[0] - gmin[0]) + (gmax[1] - gmin[1]) * (gmax[1] - gmin[1]) +
                      (gmax[2] - gmin[2]) * (gmax[2] - gmin[2]));
    if (!(scale > 0))
        scale = 1;

    // Pad every cell by a small fraction of the whole, so rays through a cell
    // boundary or a flat cell still reach it
    for (size_t k = 0; k < cellMin.size(); k++) {
        cellMin[k] -= 1e-6 * scale;
        cellMax[k] += 1e-6 * scale;
    }
}

// Does the ray from + t dir, t > 0, meet the box?
static bool rayHitsBox(const double *from, const double *dir, const double *lo, const double *hi) {
    double t0 = 0, t1 = HUGE_VAL;
    for (int k = 0; k < 3; k++) {
        if (dir[k] == 0) {
            if (from[k] < lo[k] || from[k] > hi[k])
                return false;
            continue;
        }
        double a = (lo[k] - from[k]) / dir[k], b = (hi[k] - from[k]) / dir[k];
        if (a > b)
            std::swap(a, b);
        t0 = std::max(t0, a);
        t1 = std::min(t1, b);
        if (t0 > t1)
            return false;
    }
    return true;
}

// Newton iteration on S(u, v) - (from + t dir) = 0, kept within [ulo, uhi] x [vlo, vhi]
static bool newton(const CCrossingSurface &surface, const double *from, const double *dir, double scale, double ulo, double uhi,
                   double vlo, double vhi, double &u, double &v, double &t) {
    const double dd = dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2];
    double P[3], Su[3], Sv[3];
    surface.evaluate(u, v, P, Su, Sv);
    t = ((P[0] - from[0]) * dir[0] + (P[1] - from[1]) * dir[1] + (P[2] - from[2]) * dir[2]) / dd;
    const double tolerance = kNewtonTolerance * scale;
    for (int it = 0; it < kNewtonIterations; it++) {
        const double F[3] = {P[0] - from[0] - t * dir[0], P[1] - from[1] - t * dir[1], P[2] - from[2] - t * dir[2]};
        const double residual = std::sqrt(F[0] * F[0] + F[1] * F[1] + F[2] * F[2]);
        if (residual <= tolerance)
            return true;

        // J [du dv dt]^T = -F, J = [Su | Sv | -dir], by Cramer's rule
        const double m[3] = {-dir[0], -dir[1], -dir[2]};
        const double det = Su[0] * (Sv[1] * m[2] - Sv[2] * m[1]) - Sv[0] * (Su[1] * m[2] - Su[2] * m[1]) + m[0] * (Su[1] * Sv[2] - Su[2] * Sv[1]);
        if (!std::isfinite(det) || det == 0)
            return false;
        const double r[3] = {-F[0], -F[1], -F[2]};
        const double ddu = (r[0] * (Sv[1] * m[2] - Sv[2] * m[1]) - Sv[0] * (r[1] * m[2] - r[2] * m[1]) + m[0] * (r[1] * Sv[2] - r[2] * Sv[1])) / det;
        const double ddv = (Su[0] * (r[1] * m[2] - r[2] * m[1]) - r[0] * (Su[1] * m[2] - Su[2] * m[1]) + m[0] * (Su[1] * r[2] - Su[2] * r[1])) / det;
        const double ddt = (Su[0] * (Sv[1] * r[2] - Sv[2] * r[1]) - Sv[0] * (Su[1] * r[2] - Su[2] * r[1]) + r[0] * (Su[1] * Sv[2] - Su[2] * Sv[1])) / det;
        if (!std::isfinite(ddu) || !std::isfinite(ddv) || !std::isfinite(ddt))
            return false;
        const double un = std::min(std::max(u + ddu, ulo), uhi), vn = std::min(std::max(v + ddv, vlo), vhi);
        const bool settled = std::fabs(un - u) <= kNewtonStep && std::fabs(vn - v) <= kNewtonStep;
        u = un;
        v = vn;
        t += ddt;
        surface.evaluate(u, v, P, Su, Sv);
        if (settled)
            break;
    }
    // Re-derive t for the final (u, v) so a float-noisy last step cannot skew
    // it; the residual is then P's distance from the ray
    t = ((P[0] - from[0]) * dir[0] + (P[1] - from[1]) * dir[1] + (P[2] - from[2]) * dir[2]) / dd;
    const double F[3] = {P[0] - from[0] - t * dir[0], P[1] - from[1] - t * dir[1], P[2] - from[2] - t * dir[2]};
    return std::sqrt(F[0] * F[0] + F[1] * F[1] + F[2] * F[2]) <= kAcceptTolerance * scale;
}

///////////////////////////////////////////////////////////////////////
// Function				:	findSurfaceCrossings
// Description			:	Every crossing of the ray with the surface: each
//							seed-grid cell the ray reaches is refined by Newton
//							iteration from its centre, then (if that fails or
//							leaves the cell) from its four quarter points;
//							roots found twice are merged
// Return Value			:	The number of crossings
// Comments				:	Pure: reads only the surface and the immutable grid
int findSurfaceCrossings(const CCrossingSurface &surface, const CCrossingSeedGrid &grid, const double *from, const double *dir,
                         std::vector<CCrossingHit> &hits) {
    hits.clear();
    const double dirLength = std::sqrt(dir[0] * dir[0] + dir[1] * dir[1] + dir[2] * dir[2]);
    const double su = (surface.umax - surface.umin) / grid.nu, sv = (surface.vmax - surface.vmin) / grid.nv;
    static const double seeds[5][2] = {{0.5, 0.5}, {0.25, 0.25}, {0.75, 0.25}, {0.25, 0.75}, {0.75, 0.75}};

    for (int j = 0; j < grid.nv; j++)
        for (int i = 0; i < grid.nu; i++) {
            const size_t cell = ((size_t)j * grid.nu + i) * 3;
            if (!rayHitsBox(from, dir, &grid.cellMin[cell], &grid.cellMax[cell]))
                continue;
            const double u0 = surface.umin + su * i, v0 = surface.vmin + sv * j;
            for (const double *seed : seeds) {
                double u = u0 + seed[0] * su, v = v0 + seed[1] * sv, t;
                // Allow wandering half a cell outside, never outside the surface
                const double ulo = std::max(surface.umin, u0 - 0.5 * su), uhi = std::min(surface.umax, u0 + 1.5 * su);
                const double vlo = std::max(surface.vmin, v0 - 0.5 * sv), vhi = std::min(surface.vmax, v0 + 1.5 * sv);
                if (!newton(surface, from, dir, grid.scale, ulo, uhi, vlo, vhi, u, v, t))
                    continue;
                if (t <= 0)
                    break; // Converged behind the origin: this cell's root is not on the ray
                // Seeds converging on one root stop apart by float noise:
                // the same point along the ray is the same crossing
                bool duplicate = false;
                for (const CCrossingHit &h : hits)
                    if (std::fabs(h.t - t) * dirLength <= kDuplicateDistance * grid.scale)
                        duplicate = true;
                if (!duplicate)
                    hits.push_back({u, v, t});
                const bool inCell = u >= u0 - 1e-9 * su && u <= u0 + su * (1 + 1e-9) && v >= v0 - 1e-9 * sv && v <= v0 + sv * (1 + 1e-9);
                if (inCell)
                    break;
            }
        }
    std::sort(hits.begin(), hits.end(), [](const CCrossingHit &a, const CCrossingHit &b) { return a.t < b.t; });
    return (int)hits.size();
}

///////////////////////////////////////////////////////////////////////
// Class				:	CSampledCrossingSurface
// Description			:	A CSurface evaluated through its own sample(), at
//							shutter open and undisplaced
// Comments				:	sample() needs no shading context when given local
//							buffers for every variable any free-form sampler
//							writes unconditionally (see tesselateSurfaceGrid).
//							The partials sample() reports are not all with
//							respect to the [0, 1] patch parameters (a NURBS
//							patch reports them per knot-space unit), so they
//							are taken by central differences of P instead, all
//							five points in one sample() call.
class CSampledCrossingSurface : public CCrossingSurface {
    public:
        explicit CSampledCrossingSurface(const CSurface *surface) : surface(surface) {}

        void evaluate(double u, double v, double *P, double *dPdu, double *dPdv) const override {
            static const double h = 1e-3;
            const double u0 = std::max(u - h, umin), u1 = std::min(u + h, umax);
            const double v0 = std::max(v - h, vmin), v1 = std::min(v + h, vmax);
            float uu[5] = {(float)u, (float)u0, (float)u1, (float)u, (float)u};
            float vv[5] = {(float)v, (float)v, (float)v, (float)v0, (float)v1};
            float time[5] = {0, 0, 0, 0, 0};
            float p[15], du[15], dv[15], ng[15], dt[15], pw[20];
            float *varying[VARIABLE_CONSTANTWIDTH + 1];
            for (unsigned int k = 0; k <= VARIABLE_CONSTANTWIDTH; k++)
                varying[k] = NULL;
            varying[VARIABLE_U] = uu;
            varying[VARIABLE_V] = vv;
            varying[VARIABLE_TIME] = time;
            varying[VARIABLE_P] = p;
            varying[VARIABLE_DPDU] = du;
            varying[VARIABLE_DPDV] = dv;
            varying[VARIABLE_NG] = ng;
            varying[VARIABLE_DPDTIME] = dt;
            varying[VARIABLE_PW] = pw;
            unsigned int up = PARAMETER_P | PARAMETER_DPDU | PARAMETER_DPDV | PARAMETER_NG | PARAMETER_BEGIN_SAMPLE;
            surface->sample(0, 5, varying, NULL, up);
            for (int k = 0; k < 3; k++) {
                P[k] = p[k];
                dPdu[k] = ((double)p[6 + k] - p[3 + k]) / ((double)uu[2] - uu[1]);
                dPdv[k] = ((double)p[12 + k] - p[9 + k]) / ((double)vv[4] - vv[3]);
            }
        }

    private:
        const CSurface *surface;
};

// What a free-form surface caches for its crossing queries
class CFreeFormCrossingCache : public CCrossingCache {
    public:
        explicit CFreeFormCrossingCache(const CSurface *s) : surface(s), grid(surface, kSeedGridU, kSeedGridV) {}

        CSampledCrossingSurface surface;
        CCrossingSeedGrid grid;
};

static std::mutex crossingCacheMutex;

///////////////////////////////////////////////////////////////////////
// Function				:	countFreeFormCrossings
// Description			:	Crossings of a free-form CSurface, through its seed
//							grid (built once, under a mutex, on first use)
// Return Value			:	The unsigned number of crossings
// Comments				:
int countFreeFormCrossings(CShadingContext *, const CSurface *surface, const float *from, const float *dir) {
    CCrossingCache *cache = surface->crossingCache.load(std::memory_order_acquire);
    if (cache == NULL) {
        std::lock_guard<std::mutex> lock(crossingCacheMutex);
        cache = surface->crossingCache.load(std::memory_order_relaxed);
        if (cache == NULL) {
            cache = new CFreeFormCrossingCache(surface);
            surface->crossingCache.store(cache, std::memory_order_release);
        }
    }
    const CFreeFormCrossingCache *ff = static_cast<const CFreeFormCrossingCache *>(cache);
    const double f[3] = {from[0], from[1], from[2]}, d[3] = {dir[0], dir[1], dir[2]};
    std::vector<CCrossingHit> hits;
    return findSurfaceCrossings(ff->surface, ff->grid, f, d, hits);
}
