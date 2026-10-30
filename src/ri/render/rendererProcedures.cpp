/**
 * Project: openRender
 *
 * File: rendererProcedures.cpp
 *
 * Description:
 *   The DynamicLoad hook implementation of the renderer core (see rendererProcedures.h).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "rendererProcedures.h"

#include "delayed.h"
#include "procDynamicLoadHook.h"
#include "proceduralModules.h"

// Expands one DynamicLoad procedural: the generator's ConvertParameters, Subdivide and Free, as RiProcDynamicLoad
// always ran them. The module is held by a frame reference (released after root->destroy() in endFrame), so the
// procedurals Subdivide creates may call back into it until the frame's geometry is gone.
static void expandDynamicLoad(void *data, float detail) {
    CDelayedData *delayed = (CDelayedData *)data;
    CDynamicLoadModule *module = acquireDynamicLoadModule(delayed->generator); // reports its own failure

    if (module == nullptr)
        return;

    const void *blindData = nullptr;
    if (module->convertParameters != nullptr)
        blindData = module->convertParameters(delayed->helper);
    if (module->subdivide != nullptr)
        module->subdivide(blindData, detail);
    if (module->free != nullptr)
        module->free(blindData);
}

void rendererProceduresInit() {
    setProcDynamicLoadHook(expandDynamicLoad);
}

void rendererProceduresShutdown() {
    setProcDynamicLoadHook(nullptr);
    releaseFrameProceduralModules();
}
