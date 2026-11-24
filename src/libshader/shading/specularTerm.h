/**
 * Project: openRender
 *
 * File: specularTerm.h
 *
 * Description:
 *   The per-point terms of the RSL specular() and phong() builtins, shared by
 *   the interpreter (SPECULAREXPR and PHONGEXPR in shaderFunctions.h) and the
 *   LLVM JIT (CShadingContext::callSpecular and callPhong in shading.cpp), so
 *   that the two backends compute them in the same precision, in the same
 *   order and with the same guards (GitHub #111, #113).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef LIBSHADER_SPECULAR_TERM_H
#define LIBSHADER_SPECULAR_TERM_H

#include <math.h>

#include "common/algebra.h"

namespace libshader {

    // The exponent specular() raises N.H to, from the point's roughness.
    // A roughness of 0 gives an infinite exponent, as it always has in the
    // interpreter.
    inline float specularPower(float roughness) {
        return 10.0f / roughness;
    }

    // Adds one light's specular contribution at one point to R.
    //   N      the (front-facing) normal
    //   V      the direction towards the viewer
    //   L, Cl  the light's L and Cl at the point
    //   ns     the light's __nonspecular at the point (0 for a light without one)
    //   power  specularPower(roughness)
    // The halfway vector is tested before it is normalized: V + L can be
    // (0, 0, 0), and a zero L normalizes to NaN, which the same test rejects
    // (CLAUDE.md gotcha 2). __nonspecular scales the highlight linearly, as it
    // does in phong(): (1 - ns) * pow(N.H, power).
    inline void specularAccumulate(float *R, const float *N, const float *V, const float *L, const float *Cl, float ns, float power) {
        vector Ltmp, halfway;
        normalizev(Ltmp, L);
        addvv(halfway, V, Ltmp);
        if (dotvv(halfway, halfway) > 0) {
            normalizev(halfway);
            const float tmp = dotvv(N, halfway);
            if (tmp > 0) {
                const float coefficient = (1.0f - ns) * (float)pow(tmp, power);
                if (coefficient > 0) {
                    R[COMP_R] += coefficient * Cl[COMP_R];
                    R[COMP_G] += coefficient * Cl[COMP_G];
                    R[COMP_B] += coefficient * Cl[COMP_B];
                }
            }
        }
    }

    // phong(): the reflection of V about N, R = 2 N (N.V) - V, computed once per point
    // before the light loop.
    inline void phongReflect(float *refDir, const float *N, const float *V) {
        mulvf(refDir, N, 2 * dotvv(N, V));
        subvv(refDir, V);
    }

    // Adds one light's phong contribution at one point to R.
    //   refDir  phongReflect() of the point
    //   L, Cl   the light's L and Cl at the point
    //   ns      the light's __nonspecular at the point (0 for a light without one)
    //   size    the point's exponent (varying: read per point by both backends)
    // __nonspecular scales the highlight linearly, (1 - ns) * pow(R.L, size).
    inline void phongAccumulate(float *R, const float *refDir, const float *L, const float *Cl, float ns, float size) {
        vector Ltmp;
        normalizev(Ltmp, L);
        const float dotProduct = dotvv(refDir, Ltmp);
        const float clampedDot = (0 > dotProduct) ? 0.0f : dotProduct;
        const float coefficient = (1.0f - ns) * (float)pow(clampedDot, size);
        if (coefficient > 0) {
            R[COMP_R] += coefficient * Cl[COMP_R];
            R[COMP_G] += coefficient * Cl[COMP_G];
            R[COMP_B] += coefficient * Cl[COMP_B];
        }
    }

} // namespace libshader

#endif
