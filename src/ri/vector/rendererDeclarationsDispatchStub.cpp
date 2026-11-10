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
 *     the notification loop in notifyGlobalVariable()'s real body is always a
 *     no-op in practice -- but omitting it here (rather than keeping the
 *     `if (contexts != NULL)` guard and relying on it always being false)
 *     means this build never needs CShadingContext resolvable at all.
 *
 *   - CRenderer::findCoordinateSystem() is no longer defined here (spec 031): its one reachable caller,
 *     CObject::makeBound(), asks the Geometry host (CGeometryHost::findCoordinateSystem), and a build with no
 *     host resolves to nothing, as for an unknown system.
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
#include "common/diagnostics.h"
#include "renderer.h"

void CRenderer::notifyGlobalVariable(CVariable *) {
    // No rendering pipeline in this build: there are no per-thread
    // CShadingContext instances to notify (the numbering is Graphics State's).
}

