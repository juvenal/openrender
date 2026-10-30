/**
 * Project: openRender
 *
 * File: specularTerm.h
 *
 * Description:
 *   The per-point term of the RSL specular() builtin, shared by the
 *   interpreter (SPECULAREXPR in shaderFunctions.h) and the LLVM JIT
 *   (CShadingContext::callSpecular in shading.cpp), so that the two backends
 *   compute it in the same precision, in the same order and with the same
 *   guards (GitHub #111).
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

} // namespace libshader

#endif
