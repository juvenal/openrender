/**
 * Project: openRender
 *
 * File: rendererDeclarationsDispatchStub.cpp
 *
 * Description:
 *   ribVector's counterpart to src/ri/render/rendererDeclarationsDispatch.cpp,
 *   for both methods there:
 *
 *   - CRenderer::render()/beginFrame() never run in this build, so
 *     `contexts` (the per-thread CShadingContext array) is always NULL and
 *     the notification loop in makeGlobalVariable()'s real body is always a
 *     no-op in practice -- but omitting it here (rather than keeping the
 *     `if (contexts != NULL)` guard and relying on it always being false)
 *     means this build never needs CShadingContext resolvable at all.
 *
 *   - CRenderer::findCoordinateSystem()'s one reachable caller
 *     (CObject::makeBound()) guards it behind
 *     `attributes->maxDisplacementSpace != NULL`, which nothing on the
 *     preview path can ever set true (see rendererDeclarationsDispatch.cpp's
 *     header comment for the full argument) -- so this is a loud,
 *     unreachable stub, matching CSurface::intersect()'s pattern rather
 *     than makeGlobalVariable()'s.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include <cassert>

#include "displayChannel.h"
#include "error.h"
#include "renderer.h"

void CRenderer::makeGlobalVariable(CVariable *var) {
    var->entry = globalVariables->numItems;
    var->storage = STORAGE_GLOBAL;
    globalVariables->push(var);
    // No rendering pipeline in this build: there are no per-thread
    // CShadingContext instances to notify.
}

int CRenderer::findCoordinateSystem(const char *, const float *&, const float *&, ECoordinateSystem &) {
    error(CODE_BUG, "CRenderer::findCoordinateSystem() reached in a build with no rendering pipeline\n");
    assert(FALSE);
    return FALSE;
}
