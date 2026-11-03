/**
 * Project: openRender
 *
 * File: rendererDeclarations.cpp
 *
 * Description:
 *   This file implements the functionality for rendererDeclarations.
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

///////////////////////////////////////////////////////////////////////
//
//  File				:	rendererDeclarations.cpp
//  Classes				:	CRenderer
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#include <math.h>
#include <string.h>

#include "channelRegistry.h"
#include "declarations.h"
#include "displayChannel.h"
#include "common/diagnostics.h"
#include "renderer.h"
#include "rendererContext.h"
#include "riServices.h"
#include "shading.h"
#include "variableLookup.h"

// The services the renderer core installs for the Scene Interface (parse/riServices.h) while its declarations
// exist: the declared-variable table of Graphics State, and CRenderer::locateFile (which adds the network
// mapping and the temporary directory the standard search has no use for).
namespace {
    class CCoreDeclarationService : public CRiDeclarationService {
        public:
            CVariable *retrieveVariable(const char *name) override { return CRenderer::retrieveVariable(name); }
            CVariable *declareVariable(const char *name, const char *type) override { return CRenderer::declareVariable(name, type, 0); }
    };

    class CCoreFileSearchService : public CRiFileSearchService {
        public:
            int locateFile(char *result, const char *name, TSearchpath *searchpath, int tryNormalize) override {
                return CRenderer::locateFile(result, name, searchpath, tryNormalize);
            }
    };

    CCoreDeclarationService coreDeclarations;
    CCoreFileSearchService coreFileSearch;
    CRiDeclarationService *previousDeclarations = nullptr; // what was installed before initDeclarations()
    CRiFileSearchService *previousFileSearch = nullptr;
} // namespace

///////////////////////////////////////////////////////////////////////
// Class				:	CGlobalIdentifier
// Description			:	Holds a global identifier
// Comments				:
class CGlobalIdentifier {
    public:
        char name[64]; // Name of the identifier
        int id;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	initDeclarations
// Description			:	Init the declarations
// Return Value			:
// Comments				:
void CRenderer::initDeclarations() {

    // The declared-variable table, the variable list, the global variable array and the standard declared set
    // are Graphics State's (state/declarations.cpp); a global variable added while shading contexts are live
    // is announced to them by notifyGlobalVariable
    CDeclarations::acquire();
    CDeclarations::setGlobalObserver(&CRenderer::notifyGlobalVariable);

    // Init the coordinate systems
    definedCoordinateSystems = new CTrie<CNamedCoordinateSystem *>;

    // Zero is reserved as an invalid value
    numKnownGlobalIds = 1;
    // Global Ids
    globalIdHash = new CTrie<CGlobalIdentifier *>;

    ////////////////////////////////////////////////////////////////////////////////////////////
    //
    //
    //			R E G I S T E R    D E F A U L T   C O O R D I N A T E    S Y S T E M S
    //
    //
    ////////////////////////////////////////////////////////////////////////////////////////////

    defineCoordinateSystem(coordinateCameraSystem, identityMatrix, identityMatrix, COORDINATE_CAMERA);
    defineCoordinateSystem(coordinateWorldSystem, identityMatrix, identityMatrix, COORDINATE_WORLD);
    defineCoordinateSystem(coordinateObjectSystem, identityMatrix, identityMatrix, COORDINATE_OBJECT);
    defineCoordinateSystem(coordinateShaderSystem, identityMatrix, identityMatrix, COORDINATE_SHADER);
    defineCoordinateSystem(coordinateLightSystem, identityMatrix, identityMatrix, COORDINATE_LIGHT);
    defineCoordinateSystem(coordinateNDCSystem, identityMatrix, identityMatrix, COORDINATE_NDC);
    defineCoordinateSystem(coordinateRasterSystem, identityMatrix, identityMatrix, COORDINATE_RASTER);
    defineCoordinateSystem(coordinateScreenSystem, identityMatrix, identityMatrix, COORDINATE_SCREEN);
    defineCoordinateSystem(coordinateCurrentSystem, identityMatrix, identityMatrix, COORDINATE_CURRENT);

    // Define the default color systems
    defineCoordinateSystem(colorRgbSystem, identityMatrix, identityMatrix, COLOR_RGB);
    defineCoordinateSystem(colorHslSystem, identityMatrix, identityMatrix, COLOR_HSL);
    defineCoordinateSystem(colorHsvSystem, identityMatrix, identityMatrix, COLOR_HSV);
    defineCoordinateSystem(colorXyzSystem, identityMatrix, identityMatrix, COLOR_XYZ);
    defineCoordinateSystem(colorCieSystem, identityMatrix, identityMatrix, COLOR_CIE);
    defineCoordinateSystem(colorYiqSystem, identityMatrix, identityMatrix, COLOR_YIQ);
    defineCoordinateSystem(colorXyySystem, identityMatrix, identityMatrix, COLOR_XYY);

    // The Scene Interface resolves declared names and files through these while a renderer session lasts;
    // the services installed before are put back by shutdownDeclarations()
    previousDeclarations = riInstallDeclarationService(&coreDeclarations);
    previousFileSearch = riInstallFileSearchService(&coreFileSearch);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	shutdownDeclarations
// Description			:	Clear the declarations
// Return Value			:
// Comments				:
void CRenderer::shutdownDeclarations() {
    // Put back the services that were installed before initDeclarations()
    riInstallFileSearchService(previousFileSearch);
    riInstallDeclarationService(previousDeclarations);
    previousFileSearch = nullptr;
    previousDeclarations = nullptr;

    // Nobody to notify any more
    CDeclarations::setGlobalObserver(nullptr);

    // Ditch the global IDs
    assert(globalIdHash != NULL);
    globalIdHash->destroy();

    // The variables, the display channels and the lookup hooks are Graphics State's
    CDeclarations::release();

    // Ditch the coordinate stsrems
    assert(definedCoordinateSystems != NULL);
    definedCoordinateSystems->destroy();
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	defineCoordinateSystem
// Description			:	Define a coordinate system
// Return Value			:
// Comments				:
void CRenderer::defineCoordinateSystem(const char *name, const float *from, const float *to, ECoordinateSystem type) {
    CNamedCoordinateSystem *newEntry;

    assert(definedCoordinateSystems != NULL);

    // Check if this system has been defined already
    if (definedCoordinateSystems->find(name, newEntry)) {
        // If the coordinate system existed before, just override
        movmm(newEntry->from, from);
        movmm(newEntry->to, to);
        newEntry->systemType = type;
    }
    else {
        newEntry = new CNamedCoordinateSystem;
        strcpy(newEntry->name, name);
        movmm(newEntry->from, from);
        movmm(newEntry->to, to);
        newEntry->systemType = type;
        definedCoordinateSystems->insert(newEntry->name, newEntry);
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRendererContext
// Method				:	findCoordinateSystem
// Description			:	Find a coordinate system
// Return Value			:	TRUE on success
// Comments				:	Real body lives in rendererDeclarationsDispatch.cpp
//							(needs CRenderer::context->getXform(), NULL
//							outside the full pipeline); a loud stub lives in
//							vector/rendererDeclarationsDispatchStub.cpp for
//							consumers that must not need it resolvable.

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	declareVariable
// Description			:	Define a new variable
// Return Value			:	The new variable if successful, NULL otherwise
// Comments				:
CVariable *CRenderer::declareVariable(const char *name, const char *type, int marker) {
    return CDeclarations::declare(name, type, marker);
}

// CDeclarations::declare() numbers a global variable and calls notifyGlobalVariable(), which lives in
// rendererDeclarationsDispatch.cpp (real body) / rendererDeclarationsDispatchStub.cpp (ribVector stub)
// because it reaches the shading engine.

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	retrieveVariable
// Description			:	Lookup a variable
// Return Value			:	The variable if found, NULL otherwise
// Comments				:
CVariable *CRenderer::retrieveVariable(const char *name) {
    return CDeclarations::find(name);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	getGlobalID
// Description			:	Map a string to a number globally
// Return Value			:	The ID
// Comments				:
int CRenderer::getGlobalID(const char *name) {
    CGlobalIdentifier *cId;

    assert(globalIdHash != NULL);

    if (!globalIdHash->find(name, cId)) {
        cId = new CGlobalIdentifier;
        cId->id = numKnownGlobalIds++;
        strcpy(cId->name, name);
        globalIdHash->insert(cId->name, cId);
    }

    return cId->id;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRenderer
// Method				:	registerFrameTemporary
// Description			:	Register a file as a temporary
//						:	 Optionally register for deletion at frame-end
//						:	 certain files (namely those which are netrender temporaries)
//						:	 would otherwise be found in preference to the merged
//						:	 client-side file, and must be removed before next frame.
// Return Value			:
// Comments				:	Also removes any netFileMappings for the file
void CRenderer::registerFrameTemporary(const char *name, int deleteFile) {
    char *tmp = new char[strlen(name) + 2];
    tmp[0] = (deleteFile == TRUE) ? 1 : 0;
    strcpy(tmp + 1, name);
    if (frameTemporaryFiles == NULL) {
        frameTemporaryFiles = new CArray<const char *>;
    }

    frameTemporaryFiles->push(tmp);
}
