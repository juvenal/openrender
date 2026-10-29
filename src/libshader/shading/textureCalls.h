/**
 * Project: openRender
 *
 * File: textureCalls.h
 *
 * Description:
 *   How the shading library asks the texture system for things (spec 027). Every texture(),
 *   environment(), shadow() and textureinfo() call, in the interpreter, the JIT runtime and the
 *   batch builtins, gets its handle here: the context's renderer services supply the texture
 *   system (CRendererServices::textureSystem()) and the system supplies the handle.
 *
 *   With no services, or services that carry no texture system, each lookup returns the
 *   not-found value (nullptr) and the first one of each kind reports one CODE_NOTSTARTED
 *   warning that names the kind; later ones are silent. The callers then write the "fill" value.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva@v2-labs.press>
 *
 * Copyright (c) 2025 - 2026, Juvenal A. Silva Jr.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef TEXTURECALLS_H
#define TEXTURECALLS_H

#include "common/algebra.h"
#include "ri/texture/textureSystem.h"

class CShadingContext;

namespace libshader {

    // The texture system of the context's services, or nullptr (silent)
    CTextureSystem *textureSystemOf(CShadingContext *context);

    // The handle of `name` for a `builtin` ("texture", "environment", "shadow", "textureinfo"), or
    // nullptr when there is no texture system (one warning per builtin) or, for Info, no file
    TextureHandle *textureHandle(CShadingContext *context, const char *name, ETextureKind kind, const char *builtin);

    // Whether the 3D channel operations have services to run against ("texture3d", "bake3d");
    // false with one warning per builtin when there are none
    bool texture3dAvailable(CShadingContext *context, const char *builtin);

    // The lookup footprints, in one place for the interpreter's macros and the JIT's batch functions (spec 027, T099,
    // FR-022): the arithmetic is the interpreter's, written once so the two backends cannot drift.

    // texture(): the four corners around (s, t), from the grid derivatives of the coordinates (dsdu = Du(s) ...),
    // the shading point's du and dv and the swidth/twidth multipliers
    inline void textureCorners(float *cs, float *ct, float s, float t, float dsdu, float dsdv, float dtdu, float dtdv, float du, float dv, float swidth, float twidth) {
        cs[0] = s;
        cs[1] = s + dsdu * du * swidth;
        cs[2] = s + dsdv * dv * swidth;
        cs[3] = s + (dsdu * du + dsdv * dv) * swidth;
        ct[0] = t;
        ct[1] = t + dtdu * du * twidth;
        ct[2] = t + dtdv * dv * twidth;
        ct[3] = t + (dtdu * du + dtdv * dv) * twidth;
    }

    // environment() and shadow() (not traced): the four directions (or points) around D. dDdu and dDdv are
    // scaled in place by the footprint, as the interpreter always did.
    inline void environmentCorners(float *D0, float *D1, float *D2, float *D3, const float *D, float *dDdu, float *dDdv, float du, float dv, float swidth, float twidth) {
        mulvf(dDdu, du * swidth * 0.5f);
        mulvf(dDdv, dv * twidth * 0.5f);
        subvv(D0, D, dDdu);
        subvv(D0, dDdv);
        addvv(D1, D, dDdu);
        subvv(D1, dDdv);
        subvv(D2, D, dDdu);
        addvv(D2, dDdv);
        addvv(D3, D, dDdu);
        addvv(D3, dDdv);
    }

} // namespace libshader

#endif
