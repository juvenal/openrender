/**
 * Project: openRender
 *
 * File: rendererDeclarationsDispatch.cpp
 *
 * Description:
 *   Real (rendering-time) body of CRenderer::makeGlobalVariable(), the one
 *   part of rendererDeclarations.cpp that reaches into the shading engine
 *   (CShadingContext::updateState(), notifying every live per-thread
 *   shading context that a new global-storage variable now exists).
 *
 *   Unlike the geometry classes' intersect()/dice()/shade() (see
 *   src/ri/render/geometryDispatch.cpp), this one is not vtable-anchored --
 *   it is reached for real, on every call to CRenderer::initDeclarations()
 *   (including from ribpreview_load(), which needs initDeclarations() so the
 *   RIB parser doesn't reject standard parameters), since several of the
 *   "global ..." typed variables it declares (P, N, Cs, Ci, etc.) route
 *   through declareVariable() -> here. It is safe for a build with no
 *   rendering pipeline (ribVector) because `contexts` -- the per-thread
 *   CShadingContext array this loop notifies -- is only ever allocated once
 *   CRenderer::render()/beginFrame() actually starts rendering threads,
 *   which ribVector never does; `contexts` stays NULL for the entire
 *   lifetime of a ribpreview_load() call, so the notification loop is
 *   always a no-op there in practice. See
 *   rendererDeclarationsDispatchStub.cpp for that build's counterpart,
 *   which omits the loop entirely rather than relying on the NULL guard
 *   alone -- ribVector should never need CShadingContext resolvable at all.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "displayChannel.h"
#include "renderer.h"
#include "shading.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	makeGlobalVariable
// Description			:	Forcefully make a variable global
// Return Value			:
// Comments				:
void CRenderer::makeGlobalVariable(CVariable *var) {

    // Did we already start rendering ?
    var->entry = globalVariables->numItems;
    var->storage = STORAGE_GLOBAL;
    globalVariables->push(var);

    // Flush the states for the shading contexts
    if (contexts != NULL) {
        int i;

        for (i = 0; i < numThreads; i++) {
            contexts[i]->updateState();
        }
    }
}
