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

} // namespace libshader

#endif
