/**
 * Project: openRender
 *
 * File: rslConstants.cpp
 *
 * Description:
 *   RSL basis matrices and filter functions.
 *   Moved from src/ri/ri.cpp to break circular dependency with libshader.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "../ri/ri.h"  // RtBasis, RtFloat, EXTERN macro
#include "algebra.h"   // C_PI
#include <cmath>

////////////////////////////////////////////////////////////////////////
// The cubic spline basis matrices
////////////////////////////////////////////////////////////////////////

RtBasis RiCatmullRomBasis = {
    {(float)(-1.0 / 2.0), (float)(3.0 / 2.0), (float)(-3.0 / 2.0), (float)(1.0 / 2.0)},
    {(float)(2.0 / 2.0), (float)(-5.0 / 2.0), (float)(4.0 / 2.0), (float)(-1.0 / 2.0)},
    {(float)(-1.0 / 2.0), (float)(0.0 / 2.0), (float)(1.0 / 2.0), (float)(0.0 / 2.0)},
    {(float)(0.0 / 2.0), (float)(2.0 / 2.0), (float)(0.0 / 2.0), (float)(0.0 / 2.0)}};

RtBasis RiBezierBasis = {
    {(float)-1, (float)3, (float)-3, (float)1},
    {(float)3, (float)-6, (float)3, (float)0},
    {(float)-3, (float)3, (float)0, (float)0},
    {(float)1, (float)0, (float)0, (float)0}};

RtBasis RiBSplineBasis = {
    {(float)(-1.0 / 6.0), (float)(3.0 / 6.0), (float)(-3.0 / 6.0), (float)(1.0 / 6.0)},
    {(float)(3.0 / 6.0), (float)-(6.0 / 6.0), (float)(3.0 / 6.0), (float)(0.0 / 6.0)},
    {(float)(-3.0 / 6.0), (float)(0.0 / 6.0), (float)(3.0 / 6.0), (float)(0.0 / 6.0)},
    {(float)(1.0 / 6.0), (float)(4.0 / 6.0), (float)(1.0 / 6.0), (float)(0.0 / 6.0)}};

RtBasis RiHermiteBasis = {
    {(float)1, (float)1, (float)-3, (float)1},
    {(float)-1, (float)-2, (float)4, (float)-1},
    {(float)-1, (float)1, (float)0, (float)0},
    {(float)1, (float)0, (float)0, (float)0}};

RtBasis RiPowerBasis = {
    {(float)1, (float)0, (float)0, (float)0},
    {(float)0, (float)1, (float)0, (float)0},
    {(float)0, (float)0, (float)1, (float)0},
    {(float)0, (float)0, (float)0, (float)1}};

RtBasis RiLinearBasis = {
    {(float)0, (float)0, (float)0, (float)0},
    {(float)0, (float)0, (float)0, (float)0},
    {(float)0, (float)0, (float)1, (float)0},
    {(float)0, (float)0, (float)0, (float)1}};

////////////////////////////////////////////////////////////////////////
// Filter functions
////////////////////////////////////////////////////////////////////////

EXTERN(RtFloat)
RiGaussianFilter(RtFloat x, RtFloat y, RtFloat xwidth, RtFloat ywidth) {
    x = 2 * x / xwidth;
    y = 2 * y / ywidth;

    return expf(-2 * (x * x + y * y));
}

EXTERN(RtFloat)
RiBoxFilter(RtFloat, RtFloat, RtFloat, RtFloat) {
    return 1;
}

EXTERN(RtFloat)
RiTriangleFilter(RtFloat x, RtFloat y, RtFloat xwidth, RtFloat ywidth) {
    if (x < 0.0)
        x = -x;
    if (y < 0.0)
        y = -y;

    xwidth *= 0.5f;
    ywidth *= 0.5f;

    if (x > y) {
        return (RtFloat)(xwidth - x) / xwidth;
    } else {
        return (RtFloat)(ywidth - y) / ywidth;
    }
}

EXTERN(RtFloat)
RiCatmullRomFilter(RtFloat x, RtFloat y, RtFloat, RtFloat) {
    float r2 = (x * x + y * y);
    float r = sqrtf(r2);

    if (r < 1.0f) {
        return 1.5f * r * r2 - 2.5f * r2 + 1.0f;
    } else if (r < 2.0f) {
        return -0.5f * r * r2 + 2.5f * r2 - 4.0f * r + 2.0f;
    } else {
        return 0.0f;
    }
}

EXTERN(RtFloat)
RiMitchellFilter(RtFloat x, RtFloat y, RtFloat xwidth, RtFloat ywidth) {
    x /= xwidth;
    y /= ywidth;

#define B 1 / 3.0f
#define C 1 / 3.0f

    x = fabsf(2.f * x);
    if (x > 1.f)
        x = ((-B - 6 * C) * x * x * x + (6 * B + 30 * C) * x * x + (-12 * B - 48 * C) * x + (8 * B + 24 * C)) * (1.f / 6.f);
    else
        x = ((12 - 9 * B - 6 * C) * x * x * x + (-18 + 12 * B + 6 * C) * x * x + (6 - 2 * B)) * (1.f / 6.f);

    y = fabsf(2.f * y);
    if (y > 1.f)
        y = ((-B - 6 * C) * y * y * y + (6 * B + 30 * C) * y * y + (-12 * B - 48 * C) * y + (8 * B + 24 * C)) * (1.f / 6.f);
    else
        y = ((12 - 9 * B - 6 * C) * y * y * y + (-18 + 12 * B + 6 * C) * y * y + (6 - 2 * B)) * (1.f / 6.f);

#undef B
#undef C

    return x * y;
}

EXTERN(RtFloat)
RiBesselFilter(RtFloat x, RtFloat y, RtFloat, RtFloat) {
    const float x2 = x * x;
    const float y2 = y * y;

    if (x2 + y2 < 0.0001f)
        return 1.0f;

    const float d = sqrtf(x2 + y2);
    return (float)(j1(d * 2) / d);
}

EXTERN(RtFloat)
RiSincFilter(RtFloat x, RtFloat y, RtFloat xwidth, RtFloat ywidth) {
    if (x != 0.0) {
        x *= (float)C_PI;
        x = cosf(0.5f * x / xwidth) * sinf(x) / x;
    } else {
        x = 1.0;
    }

    if (y != 0.0) {
        y *= (float)C_PI;
        y = cosf(0.5f * y / ywidth) * sinf(y) / y;
    } else {
        y = 1.0;
    }

    return x * y;
}

EXTERN(RtFloat)
RiBlackmanHarrisFilter(RtFloat x, RtFloat y, RtFloat xwidth, RtFloat ywidth) {
    float xc = x / xwidth;
    float yc = y / ywidth;
    float r2 = (xc * xc + yc * yc);
    float r = 0.5f - sqrtf(r2);

    const float N = 1;
    const float a0 = 0.35875f;
    const float a1 = 0.48829f;
    const float a2 = 0.14128f;
    const float a3 = 0.01168f;

    if (r <= N * 0.5f) {
        return (float)(a0 - a1 * cosf(2 * ((float)C_PI) * r / N) + a2 * cosf(4 * ((float)C_PI) * r / N) - a3 * cosf(6 * ((float)C_PI) * r / N));
    } else {
        return 0;
    }
}

// RiCatmullRomStepFilter, RiMitchellStepFilter, RiGaussianStepFilter
// remain in src/ri/ri.cpp (used by rendererFiles.cpp, not by shading engine)
