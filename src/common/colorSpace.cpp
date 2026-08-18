/**
 * Project: openRender
 *
 * File: colorSpace.cpp
 *
 * Description:
 *   Color-space conversion functions. Relocated verbatim from
 *   src/ri/init.cpp (spec 011-jit-opcode-parity, FR-009/D2) so both the
 *   interpreter and the LLVM JIT can call them without a reverse dependency
 *   on `ri`.
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

#include "colorSpace.h"
#include "algebra.h" // vector, COMP_R/G/B, movvv, initv

#include <math.h>

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	convertColorFrom
// Description			:	Do color conversion
// Return Value			:	-
// Comments				:
void convertColorFrom(float *out, const float *in, ECoordinateSystem s) {
    switch (s) {
    case COLOR_RGB:
        movvv(out, in);
        break;
    case COLOR_HSL: {

#define HueToRGB(r, m1, m2, h)                          \
    if (h < 0)                                          \
        h += 1;                                         \
    if (h > 1)                                          \
        h -= 1;                                         \
    if (6.0 * h < 1)                                    \
        r = (m1 + (m2 - m1) * h * 6);                   \
    else if (2.0 * h < 1)                               \
        r = m2;                                         \
    else if (3.0 * h < 2.0)                             \
        r = (m1 + (m2 - m1) * ((2.0f / 3.0f) - h) * 6); \
    else                                                \
        r = m1;

        float m1, m2, h;

        if (in[1] == 0)
            initv(out, in[2], in[2], in[2]);
        else {
            if (in[2] <= 0.5)
                m2 = in[2] * (1 + in[1]);
            else
                m2 = in[2] + in[1] - in[2] * in[1];
            m1 = 2 * in[2] - m2;

            h = in[0] + (1.0f / 3.0f);
            HueToRGB(out[0], m1, m2, h);
            h = in[0];
            HueToRGB(out[1], m1, m2, h);
            h = in[0] - (1.0f / 3.0f);
            HueToRGB(out[2], m1, m2, h);
        }
#undef HueToRGB

    } break;
    case COLOR_HSV: {
        if (in[1] < -1 || in[1] > 1) {
            if (in[0] == 0) {
                out[0] = in[2];
                out[1] = in[2];
                out[2] = in[2];
            } else {
                out[0] = in[0];
                out[1] = in[1];
                out[2] = in[2];
            }
        } else {
            float f, p, q, t, h;
            int i;

            h = (float)fmod(in[0], 1);
            if (h < 0)
                h += 1;

            h *= 6;

            i = (int)floor(h);
            f = h - i;
            p = in[2] * (1 - in[1]);
            q = in[2] * (1 - (in[1] * f));
            t = in[2] * (1 - (in[1] * (1 - f)));

            switch (i) {
            case 0:
                out[COMP_R] = in[2];
                out[COMP_G] = t;
                out[COMP_B] = p;
                break;
            case 1:
                out[COMP_R] = q;
                out[COMP_G] = in[2];
                out[COMP_B] = p;
                break;
            case 2:
                out[COMP_R] = p;
                out[COMP_G] = in[2];
                out[COMP_B] = t;
                break;
            case 3:
                out[COMP_R] = p;
                out[COMP_G] = q;
                out[COMP_B] = in[2];
                break;
            case 4:
                out[COMP_R] = t;
                out[COMP_G] = p;
                out[COMP_B] = in[2];
                break;
            case 5:
                out[COMP_R] = in[2];
                out[COMP_G] = p;
                out[COMP_B] = q;
                break;
            }
        }
    } break;
    case COLOR_XYZ:
        out[COMP_R] = (float)(3.24079 * in[0] - 1.537150 * in[1] - 0.498535 * in[2]);
        out[COMP_G] = (float)(-0.969256 * in[0] + 1.875992 * in[1] + 0.041556 * in[2]);
        out[COMP_B] = (float)(0.055648 * in[0] - 0.204043 * in[1] + 1.057311 * in[2]);
        break;
    case COLOR_CIE:
        out[COMP_R] = (float)(3.24079 * in[0] - 1.537150 * in[1] - 0.498535 * in[2]);
        out[COMP_G] = (float)(-0.969256 * in[0] + 1.875992 * in[1] + 0.041556 * in[2]);
        out[COMP_B] = (float)(0.055648 * in[0] - 0.204043 * in[1] + 1.057311 * in[2]);
        break;
    case COLOR_YIQ:
        out[COMP_R] = (float)(in[0] + 0.956 * in[1] + 0.620 * in[2]);
        out[COMP_G] = (float)(in[0] - 0.272 * in[1] - 0.647 * in[2]);
        out[COMP_B] = (float)(in[0] - 1.108 * in[1] + 1.705 * in[2]);
        break;
    case COLOR_XYY:
        vector tin;

        if (in[2] == 0) {
            tin[0] = 0;
            tin[1] = 0;
            tin[2] = 0;
        } else {
            float maxValue;
            float multipliedValue = in[0] * in[2] / in[1];
            if (0 > multipliedValue) {
                maxValue = 0;
            } else {
                maxValue = multipliedValue;
            }
            tin[0] = maxValue;
            tin[1] = in[2];
            float maxValue2;
            float multipliedValue2 = (1 - in[0] - in[1]) * in[2] / in[1];
            if (0 > multipliedValue2) {
                maxValue2 = 0;
            } else {
                maxValue2 = multipliedValue2;
            }
            tin[2] = maxValue2;
        }

        out[COMP_R] = (float)(3.24079 * tin[0] - 1.537150 * tin[1] - 0.498535 * tin[2]);
        out[COMP_G] = (float)(-0.969256 * tin[0] + 1.875992 * tin[1] + 0.041556 * tin[2]);
        out[COMP_B] = (float)(0.055648 * tin[0] - 0.204043 * tin[1] + 1.057311 * tin[2]);

        break;
    default:
        break;
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CShadingContext
// Method				:	convertColorTo
// Description			:	Do color conversion
// Return Value			:	-
// Comments				:
void convertColorTo(float *out, const float *in, ECoordinateSystem s) {
    switch (s) {
    case COLOR_RGB:
        movvv(out, in);
        break;
    case COLOR_HSL: {
        float mi;
        if (in[0] < in[1]) {
            mi = in[0];
        } else {
            mi = in[1];
        }
        if (in[2] < mi) {
            mi = in[2];
        }
        float ma;
        if (in[0] > in[1]) {
            ma = in[0];
        } else {
            ma = in[1];
        }
        if (in[2] > ma) {
            ma = in[2];
        }

        out[2] = (mi + ma) / 2;
        if (ma == mi) {
            out[0] = 100; // install value out of -1 to 1 range
            out[1] = 0;
        } else {
            float d = ma - mi;

            if (out[2] < 0.5) {
                out[1] = d / (ma + mi);
            } else {
                out[1] = d / (2 - (ma + mi));
            }

            if (in[COMP_R] == ma) {
                out[0] = (in[COMP_G] - in[COMP_B]) / d;
            } else if (in[COMP_G] == ma) {
                out[0] = 2 + (in[COMP_B] - in[COMP_R]) / d;
            } else {
                out[0] = 4 + (in[COMP_R] - in[COMP_G]) / d;
            }

            out[0] /= (float)6;

            if (out[0] < 0)
                out[0] += 1;
        }
    } break;
    case COLOR_HSV: {
        float ma;
        if (in[0] > in[1]) {
            ma = in[0];
        } else {
            ma = in[1];
        }
        if (in[2] > ma) {
            ma = in[2];
        }
        float mi;
        if (in[0] < in[1]) {
            mi = in[0];
        } else {
            mi = in[1];
        }
        if (in[2] < mi) {
            mi = in[2];
        }

        out[2] = ma;
        out[1] = (ma - mi) / ma;
        if (ma == 0) {
            out[0] = 100; // install value out of -1 to 1 range
        } else {
            float d = ma - mi;

            if (in[COMP_R] == ma) {
                out[0] = (in[COMP_G] - in[COMP_B]) / d;
            } else if (in[COMP_G] == ma) {
                out[0] = 2 + (in[COMP_B] - in[COMP_R]) / d;
            } else {
                out[0] = 4 + (in[COMP_R] - in[COMP_G]) / d;
            }

            out[0] /= (float)6;
            if (out[0] < 0)
                out[0] += 1;
        }
    } break;
    case COLOR_XYZ:
        out[0] = (float)(0.412453 * in[COMP_R] + 0.357580 * in[COMP_G] + 0.180423 * in[COMP_B]);
        out[1] = (float)(0.212671 * in[COMP_R] + 0.715160 * in[COMP_G] + 0.072169 * in[COMP_B]);
        out[2] = (float)(0.019334 * in[COMP_R] + 0.119193 * in[COMP_G] + 0.950227 * in[COMP_B]);
        break;
    case COLOR_CIE:
        out[0] = (float)(0.412453 * in[COMP_R] + 0.357580 * in[COMP_G] + 0.180423 * in[COMP_B]);
        out[1] = (float)(0.212671 * in[COMP_R] + 0.715160 * in[COMP_G] + 0.072169 * in[COMP_B]);
        out[2] = (float)(0.019334 * in[COMP_R] + 0.119193 * in[COMP_G] + 0.950227 * in[COMP_B]);
        break;
    case COLOR_YIQ:
        out[0] = (float)(0.299 * in[COMP_R] + 0.587 * in[COMP_G] + 0.114 * in[COMP_B]);
        out[1] = (float)(0.596 * in[COMP_R] - 0.275 * in[COMP_G] - 0.321 * in[COMP_B]);
        out[2] = (float)(0.212 * in[COMP_R] - 0.523 * in[COMP_G] + 0.311 * in[COMP_B]);
        break;
    case COLOR_XYY:
        vector tin;
        float sum;

        tin[0] = (float)(0.412453 * in[COMP_R] + 0.357580 * in[COMP_G] + 0.180423 * in[COMP_B]);
        tin[1] = (float)(0.212671 * in[COMP_R] + 0.715160 * in[COMP_G] + 0.072169 * in[COMP_B]);
        tin[2] = (float)(0.019334 * in[COMP_R] + 0.119193 * in[COMP_G] + 0.950227 * in[COMP_B]);

        sum = tin[0] + tin[1] + tin[2];
        if (sum == 0) {
            initv(out, 0, 0, 0);
        } else {
            out[0] = tin[0] / sum;
            out[1] = tin[1] / sum;
            out[2] = tin[2];
        }
        break;
    default:
        break;
    }
}
