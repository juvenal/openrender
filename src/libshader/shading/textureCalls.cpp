/**
 * Project: openRender
 *
 * File: textureCalls.cpp
 *
 * Description:
 *   See textureCalls.h.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva@v2-labs.press>
 *
 * Copyright (c) 2025 - 2026, Juvenal A. Silva Jr.
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "textureCalls.h"

#include <atomic>
#include <string.h>

#include "common/diagnostics.h"
#include "rendererServices.h"
#include "shading.h"

namespace libshader {

    // One warning per builtin per process, the first call made without a texture system reports it
    static std::atomic<bool> s_warnedTexture{false};
    static std::atomic<bool> s_warnedEnvironment{false};
    static std::atomic<bool> s_warnedShadow{false};
    static std::atomic<bool> s_warnedInfo{false};
    static std::atomic<bool> s_warned3d{false};
    static std::atomic<bool> s_warnedBake3d{false};
    static std::atomic<bool> s_warnedOther{false};

    static std::atomic<bool> &warnedFlag(const char *builtin) {
        if (!strcmp(builtin, "texture"))
            return s_warnedTexture;
        if (!strcmp(builtin, "environment"))
            return s_warnedEnvironment;
        if (!strcmp(builtin, "shadow"))
            return s_warnedShadow;
        if (!strcmp(builtin, "textureinfo"))
            return s_warnedInfo;
        if (!strcmp(builtin, "texture3d"))
            return s_warned3d;
        if (!strcmp(builtin, "bake3d"))
            return s_warnedBake3d;
        return s_warnedOther;
    }

    static void warnNoSystem(const char *builtin, const char *name) {
        if (!warnedFlag(builtin).exchange(true))
            warning(CODE_NOTSTARTED, "no texture system is installed: %s(\"%s\") returns the fill value\n", builtin, name ? name : "");
    }

    CTextureSystem *textureSystemOf(CShadingContext *context) {
        CRendererServices *svc = context ? context->getServices() : CShadingContext::defaultServices();
        return svc ? svc->textureSystem() : nullptr;
    }

    TextureHandle *textureHandle(CShadingContext *context, const char *name, ETextureKind kind, const char *builtin) {
        CTextureSystem *system = textureSystemOf(context);
        if (!system) {
            warnNoSystem(builtin, name);
            return nullptr;
        }
        return system->handle(name, kind);
    }

    bool texture3dAvailable(CShadingContext *context, const char *builtin) {
        CRendererServices *svc = context ? context->getServices() : CShadingContext::defaultServices();
        if (svc)
            return true;
        if (!warnedFlag(builtin).exchange(true))
            warning(CODE_NOTSTARTED, "renderer services are not installed: %s() does nothing\n", builtin);
        return false;
    }

} // namespace libshader
