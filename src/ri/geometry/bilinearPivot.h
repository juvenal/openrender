/**
 * Project: openRender
 *
 * File: bilinearPivot.h
 *
 * Description:
 *   The pivot axis of the bilinear ray intersectors (GitHub #156). CPolygonQuad, CBilinearPatch and the
 *   tessellation's intersectQuadsBilin eliminate the ray parameter through one axis of the ray direction,
 *   the pivot k, and solve the two projected equations over the other two axes i and j. The pivot has
 *   always been z. A ray with q[2] == 0 makes both equations the same one (z(u, v) = r[2]), no root comes
 *   out and the ray misses every quad it crosses, so for that ray alone the pivot is the larger of x and y.
 *   Every other ray keeps the z pivot, and its results are bit for bit what they were.
 *
 *   Header-only and stateless; includes nothing.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef BILINEARPIVOT_H
#define BILINEARPIVOT_H

struct BilinearPivot {
    int i, j, k;
};

inline BilinearPivot bilinearPivot(const float *q) {
    if (q[2] != 0)
        return {0, 1, 2};
    if (q[0] * q[0] >= q[1] * q[1])
        return {1, 2, 0};
    return {0, 2, 1};
}

#endif
