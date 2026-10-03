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

// Spec 021 Phase 3 skeleton: inert answers so the tests compile and fail.
// Cap synthesis arrives with T056.

void buildImpliedCaps(const CSurface *, std::vector<CCSGCap> &caps) {
    caps.clear();
}

int capCrossSectionAccepts(const CSurface *, const CCSGCap &, const float *) {
    return FALSE;
}
