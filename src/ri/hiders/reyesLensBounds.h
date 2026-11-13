/**
 * Project: openRender
 *
 * File: reyesLensBounds.h
 *
 * Description:
 *   Spec 031 (geometry-hider-agnostic), lens phase, FR-031: how far the depth-of-field scatter of the
 *   Reyes hiders can move a point, in sample units. The stochastic raster scatters a vertex v by
 *   v + lens * dSampled * c, with c the signed screen-space circle of confusion of the vertex's depth
 *   (openrender::LensModel::circleOfConfusion) and lens a point on the unit disk, so the scatter reaches
 *   |c| * |dSampledx| on x and |c| * |dSampledy| on y. The bucket bound of an object (reyes.cpp's
 *   drawObject), the grid bounds and the per-time-stratum bounds expand by exactly that, through the
 *   functions below, and Hiders_ReyesBucketBoundCoversCoC calls the same functions.
 *
 *   Only the scatter reads a vertex's c signed; every bound, comparison and maximum takes its magnitude.
 *   Header-only: it names no renderer symbol, only the LensModel.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef REYESLENSBOUNDS_H
#define REYESLENSBOUNDS_H

#include <cmath>

#include "camera.h"

namespace reyes_lens {

    // A depth at or behind the eye plane has no finite 1/z: a bound reaching it is taken at this depth
    // (the value of C_EPSILON, common/algebra.h), which makes the bound large but finite
    constexpr float kMinDepth = 1e-6f;

    // How far the scatter can move a point, per axis, in sample units (both >= 0)
    struct SampleExpansion {
            float x, y;
    };

    // The largest |circle of confusion| over the depths [zMin, zMax] (zMax may be +infinity), in screen
    // units; 0 for a pinhole and for an orthographic lens. c is monotonic in 1/z, so the largest
    // magnitude is at one of the two ends. A zMin below kMinDepth (the eye plane, or behind it) is
    // clamped to kMinDepth, and a zMax below the clamped zMin to it.
    inline float maxAbsCoc(const openrender::LensModel &lens, float zMin, float zMax) {
        if (zMin < kMinDepth)
            zMin = kMinDepth;
        if (zMax < zMin)
            zMax = zMin;

        const float c1 = std::fabs(lens.circleOfConfusion(zMin));
        const float c2 = std::fabs(lens.circleOfConfusion(zMax));
        return (c2 > c1) ? c2 : c1;
    }

    // The scatter of a circle of confusion c (either sign): |c| * |dSampledx| on x, |c| * |dSampledy| on y
    inline SampleExpansion expansion(float c, float dSampledx, float dSampledy) {
        const float m = std::fabs(c);
        SampleExpansion e;
        e.x = m * std::fabs(dSampledx);
        e.y = m * std::fabs(dSampledy);
        return e;
    }

    // The sample-unit bound of an object whose screen-unit bound is bounds = {xmin, xmax, ymin, ymax}
    // (relative to the window's origin) over the depths [zMin, zMax]: converted by dSampled (an axis whose
    // dSampled is negative swaps its ends), then widened by the largest scatter. bounds is replaced by the
    // result, ordered. With no lens the widening is 0 and the result is the plain conversion.
    inline void expandBucketBounds(const openrender::LensModel &lens, float zMin, float zMax, float dSampledx, float dSampledy, float bounds[4]) {
        float xmin = bounds[0] * dSampledx;
        float xmax = bounds[1] * dSampledx;
        float ymin = bounds[2] * dSampledy;
        float ymax = bounds[3] * dSampledy;

        if (xmax < xmin) {
            const float t = xmax;
            xmax = xmin;
            xmin = t;
        }

        if (ymax < ymin) {
            const float t = ymax;
            ymax = ymin;
            ymin = t;
        }

        if (lens.aperture != 0) {
            const SampleExpansion e = expansion(maxAbsCoc(lens, zMin, zMax), dSampledx, dSampledy);

            xmin -= e.x;
            xmax += e.x;
            ymin -= e.y;
            ymax += e.y;
        }

        bounds[0] = xmin;
        bounds[1] = xmax;
        bounds[2] = ymin;
        bounds[3] = ymax;
    }

} // namespace reyes_lens

#endif
