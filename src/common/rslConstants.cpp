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

////////////////////////////////////////////////////////////////////////
// Step-filter functions
////////////////////////////////////////////////////////////////////////

#ifdef _WINDOWS
//
// Computation of the complementary error function erfc(x).
//
// The algorithm is based on a Chebyshev fit as denoted in
// Numerical Recipes 2nd ed. on p. 214 (W.H.Press et al.).
//
// The fractional error is always less than 1.2e-7.
//
//
// The parameters of the Chebyshev fit
//
inline double erfc(double x) {
    const double a1 = -1.26551223, a2 = 1.00002368,
                 a3 = 0.37409196, a4 = 0.09678418,
                 a5 = -0.18628806, a6 = 0.27886807,
                 a7 = -1.13520398, a8 = 1.48851587,
                 a9 = -0.82215223, a10 = 0.17087277;
    //
    double v = 1; // The return value
    double z = fabs(x);
    //
    if (z <= 0)
        return v; // erfc(0)=1
    //
    double t = 1 / (1 + 0.5 * z);
    //
    v = t * exp((-z * z) + a1 + t * (a2 + t * (a3 + t * (a4 + t * (a5 + t * (a6 + t * (a7 + t * (a8 + t * (a9 + t * a10)))))))));
    //
    if (x < 0)
        v = 2 - v; // erfc(-x)=2-erfc(x)
    //
    return v;
}
#endif

EXTERN(RtFloat)
RiGaussianStepFilter(RtFloat _t, RtFloat _edge, RtFloat _w) {
    const double t = _t, edge = _edge, w = _w;
    double res = 0.0;

    res = (1.0 / 2.0) * erfc((2.0 * sqrt(2.0) * (edge - t)) / w);

    return (RtFloat)res;
}

EXTERN(RtFloat)
RiCatmullRomStepFilter(RtFloat _t, RtFloat _edge, RtFloat _w) {
    const double t = _t, edge = _edge, w = _w;
    double res = 0.0;

    if (edge == t && edge >= (t + w) && edge < (t + 2.0 * w)) {
        res = -1.0 / 24.0;
    }
    else if (edge < t && (edge + w) <= t && (edge + 2.0 * w) <= t) {
        res = 1.0;
    }
    else if ((edge + w) == t && (edge + 2.0 * w) > t && edge < t) {
        res = 25.0 / 24.0;
    }
    else if (edge > t && edge > (t + w) && edge < (t + 2.0 * w)) {
        res = ((3.0 * edge - 3.0 * t - 2.0 * w) * pow(edge - t - 2.0 * w, 3.0)) / (24.0 * pow(w, 4.0));
    }
    else if ((edge + 2.0 * w) > t && edge < t && (edge + w) < t) {
        res = (-3.0 * pow(edge - t, 4.0) - 20.0 * pow(edge - t, 3.0) * w - 48.0 * pow(edge - t, 2.0) * w * w +
               48.0 * (-edge + t) * pow(w, 3.0) + 8.0 * pow(w, 4.0)) /
              (24.0 * pow(w, 4.0));
    }
    else if ((edge + w) > t && edge < t && (edge + 2.0 * w) <= t) {
        res = (-edge + t) / w + (3.0 * pow(edge - t, 4)) / (8.0 * pow(w, 4.0)) + (5.0 * pow(edge - t, 3)) / (6.0 * pow(w, 3.0)) + (11.0) / 24.0;
    }
    else if (edge < (t + w) && (edge > t || (edge >= t && edge < (t + 2.0 * w)))) {
        res = (-edge + t) / w - (3.0 * pow(edge - t, 4)) / (8.0 * pow(w, 4.0)) + (5.0 * pow(edge - t, 3)) / (6.0 * pow(w, 3.0)) + 1.0 / 2.0;
    }
    else if ((edge + w) > t && (edge + 2.0 * w) > t && edge < t) {
        res = (-edge + t) / w + (3.0 * pow(edge - t, 4)) / (8.0 * pow(w, 4.0)) + (5.0 * pow(edge - t, 3)) / (6.0 * pow(w, 3.0)) + 1.0 / 2.0;
    }
    else if (edge == t && edge >= (t + 2.0 * w) && edge < (t + w)) {
        res = 13.0 / 24.0;
    }

    return (RtFloat)res;
}

EXTERN(RtFloat)
RiMitchellStepFilter(RtFloat _t, RtFloat _edge, RtFloat _w) {
    const double t = _t, edge = _edge, w = _w;
    double res = 0.0;

    if (edge == t && edge >= (t + w) && edge < (t + 2.0 * w)) {
        res = -1.0 / 72.0;
    }
    else if (edge < t && (edge + w) <= t && (edge + 2.0 * w) <= t) {
        res = 1.0;
    }
    else if ((edge + w) == t && (edge + 2.0 * w) > t && edge < t) {
        res = 73.0 / 72.0;
    }
    else if (edge > t && edge > (t + w) && edge < (t + 2.0 * w)) {
        res = ((7.0 * edge - 7.0 * t - 6.0 * w) * pow(edge - t - 2.0 * w, 3.0)) / (72.0 * pow(w, 4.0));
    }
    else if ((edge + 2.0 * w) > t && edge < t && (edge + w) < t) {
        res = (-7.0 * pow(edge - t, 4.0) - 48.0 * pow(edge - t, 3.0) * w - 120.0 * pow(edge - t, 2.0) * w * w +
               128.0 * (-edge + t) * pow(w, 3.0) + 24.0 * pow(w, 4.0)) /
              (72.0 * pow(w, 4.0));
    }
    else if ((edge + w) > t && edge < t && (edge + 2.0 * w) <= t) {
        res = (64.0 * (-edge + t) / (w * 72.0)) + (35.0 / 72.0) + ((21.0 * pow(edge - t, 4)) + (48.0 * w * pow(edge - t, 3))) / (72.0 * pow(w, 4.0));
    }
    else if (edge < (t + w) && (edge > t || (edge >= t && edge < (t + 2.0 * w)))) {
        res = (64.0 * (-edge + t) / (w * 72.0)) + (36.0 / 72.0) + ((-21.0 * pow(edge - t, 4)) + (48.0 * w * pow(edge - t, 3))) / (72.0 * pow(w, 4.0));
    }
    else if ((edge + w) > t && (edge + 2.0 * w) > t && edge < t) {
        res = (64.0 * (-edge + t) / (w * 72.0)) + (36.0 / 72.0) + ((21.0 * pow(edge - t, 4)) + (48.0 * w * pow(edge - t, 3))) / (72.0 * pow(w, 4.0));
    }
    else if (edge == t && edge >= (t + 2.0 * w) && edge < (t + w)) {
        res = 37.0 / 72.0;
    }

    return (RtFloat)res;
}

EXTERN(RtFloat)
RiTriangleStepFilter(RtFloat _t, RtFloat _edge, RtFloat _w) {
    const double t = _t, edge = _edge, w = _w;
    double res = 0.0;

    if ((edge - t + w) <= 0 && (edge - t) < 0) {
        res = 1.0;
    }
    else if ((edge - t) < 0 && (edge - t + w) > 0) {
        res = (-edge * edge + 2.0 * edge * t - t * t - 2.0 * edge * w + 2.0 * t * w + w * w) / (2.0 * w * w);
    }
    else if ((edge - t) >= 0 && (edge - t - w) < 0) {
        res = (edge * edge - 2.0 * edge * t + t * t - 2.0 * edge * w + 2.0 * t * w + w * w) / (2.0 * w * w);
    }

    return (RtFloat)res;
}

EXTERN(RtFloat)
RiBoxStepFilter(RtFloat _t, RtFloat _edge, RtFloat _w) {
    const double t = _t, edge = _edge, w = _w;
    double res = 0.0;

    if ((edge - t) < 0 && (2.0 * edge - 2 * t + w) <= 0) {
        res = 1.0;
    }
    else if (((2.0 * edge - 2.0 * t + w) > 0 && (edge - t) < 0) || ((edge - t) >= 0 && (2.0 * edge - 2.0 * t - w) < 0)) {
        res = (-2.0 * edge + 2.0 * t + w) / (2.0 * w);
    }

    return (RtFloat)res;
}
