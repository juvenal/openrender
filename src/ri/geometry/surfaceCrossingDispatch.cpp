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

// Spec 021 Phase 2 skeleton: the Newton core and the dispatch arrive with T029.

CCrossingSeedGrid::CCrossingSeedGrid(const CCrossingSurface &, int nu, int nv) : nu(nu), nv(nv) {}

int findSurfaceCrossings(const CCrossingSurface &, const CCrossingSeedGrid &, const double *, const double *,
                         std::vector<CCrossingHit> &hits) {
    hits.clear();
    return 0;
}

int countFreeFormCrossings(CShadingContext *, const CSurface *, const float *, const float *) {
    return 0;
}
