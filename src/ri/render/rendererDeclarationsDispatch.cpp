/**
 * Project: openRender
 *
 * File: rendererDeclarationsDispatch.cpp
 *
 * Description:
 *   Real (rendering-time) bodies of the two parts of rendererDeclarations.cpp
 *   that reach into things a no-pipeline build can't have:
 *
 *   - CRenderer::makeGlobalVariable() touches the shading engine
 *     (CShadingContext::updateState(), notifying every live per-thread
 *     shading context that a new global-storage variable now exists).
 *     Unlike the geometry classes' intersect()/dice()/shade() (see
 *     src/ri/render/geometryDispatch.cpp), this one is not vtable-anchored --
 *     it is reached for real, on every call to CRenderer::initDeclarations()
 *     (including from ribpreview_load(), which needs initDeclarations() so
 *     the RIB parser doesn't reject standard parameters), since several of
 *     the "global ..." typed variables it declares (P, N, Cs, Ci, etc.)
 *     route through declareVariable() -> here. It is safe for a build with
 *     no rendering pipeline (ribVector) because `contexts` -- the per-thread
 *     CShadingContext array this loop notifies -- is only ever allocated
 *     once CRenderer::render()/beginFrame() actually starts rendering
 *     threads, which ribVector never does; `contexts` stays NULL for the
 *     entire lifetime of a ribpreview_load() call, so the notification loop
 *     is always a no-op there in practice.
 *
 *   - CRenderer::findCoordinateSystem() calls CRenderer::context->getXform()
 *     for the COORDINATE_SHADER/COORDINATE_CURRENT cases, where
 *     CRenderer::context is the static CRendererContext* that's NULL outside
 *     the full pipeline (see rib.y's getBasisSteps() and its own commit for
 *     the general version of this problem). Its one reachable caller,
 *     CObject::makeBound() in object.cpp, guards the whole call behind
 *     `attributes->maxDisplacementSpace != NULL` -- and that field is set
 *     ONLY by CRendererContext::RiAttributeV() (rendererContext.cpp, already
 *     excluded) parsing the "displacementbound"/"coordinatesystem"
 *     attribute; CRibGeometryContext doesn't override RiAttributeV at all
 *     (it inherits CRiInterface's no-op default), so nothing on the preview
 *     path can ever set maxDisplacementSpace non-NULL. Confirmed by grepping
 *     every site that reads or writes maxDisplacementSpace repo-wide. Unlike
 *     getAttributes(), this one doesn't need a real, generic fix -- there is
 *     no legitimate preview feature being silently broken, since
 *     displacementbound-coordinatesystem was never supported outside the
 *     full pipeline to begin with.
 *
 *   See rendererDeclarationsDispatchStub.cpp for that build's counterpart to
 *   both -- ribVector should never need CShadingContext or CRendererContext
 *   resolvable at all.
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
#include "rendererContext.h"
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

///////////////////////////////////////////////////////////////////////
// Class				:	CRendererContext
// Method				:	findCoordinateSystem
// Description			:	Find a coordinate system
// Return Value			:	TRUE on success
// Comments				:
int CRenderer::findCoordinateSystem(const char *name, const float *&from, const float *&to, ECoordinateSystem &cSystem) {
    CNamedCoordinateSystem *currentSystem;

    assert(CRenderer::definedCoordinateSystems != NULL);

    if (CRenderer::definedCoordinateSystems->find(name, currentSystem)) {
        from = currentSystem->from;
        to = currentSystem->to;
        cSystem = currentSystem->systemType;

        switch (cSystem) {
            case COORDINATE_OBJECT:
                break;
            case COORDINATE_CAMERA:
                from = identityMatrix;
                to = identityMatrix;
                break;
            case COORDINATE_WORLD:
                from = CRenderer::fromWorld;
                to = CRenderer::toWorld;
                break;
            case COORDINATE_SHADER:
            {
                CXform *currentXform = context->getXform(FALSE);
                from = currentXform->from;
                to = currentXform->to;
                break;
            }
            case COORDINATE_LIGHT:
            case COORDINATE_NDC:
            case COORDINATE_RASTER:
            case COORDINATE_SCREEN:
                break;
            case COORDINATE_CURRENT:
            {
                CXform *currentXform = context->getXform(FALSE);
                from = currentXform->from;
                to = currentXform->to;
                break;
            }
            case COLOR_RGB:
            case COLOR_HSL:
            case COLOR_HSV:
            case COLOR_XYZ:
            case COLOR_CIE:
            case COLOR_YIQ:
            case COLOR_XYY:
            case COORDINATE_CUSTOM:
                break;
        }

        return TRUE;
    }

    return FALSE;
}
