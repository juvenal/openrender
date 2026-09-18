/**
 * Project: openRender
 *
 * File: rendererDeclarationsDispatchStub.cpp
 *
 * Description:
 *   ribVector's counterpart to
 *   src/ri/render/rendererDeclarationsDispatch.cpp. CRenderer::render()/
 *   beginFrame() never run in this build, so `contexts` (the per-thread
 *   CShadingContext array) is always NULL and the notification loop in the
 *   real body is always a no-op in practice -- but omitting it here (rather
 *   than keeping the `if (contexts != NULL)` guard and relying on it always
 *   being false) means this build never needs CShadingContext resolvable at
 *   all, matching every other file in this stub/real split.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "displayChannel.h"
#include "renderer.h"

void CRenderer::makeGlobalVariable(CVariable *var) {
    var->entry = globalVariables->numItems;
    var->storage = STORAGE_GLOBAL;
    globalVariables->push(var);
    // No rendering pipeline in this build: there are no per-thread
    // CShadingContext instances to notify.
}
