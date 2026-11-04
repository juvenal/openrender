/**
 * Project: openRender
 *
 * File: declarations.cpp
 *
 * Description:
 *   The declared-variable table of Graphics State; see declarations.h. The body of the standard declared set
 *   and of declare() are CRenderer::initDeclarations() and CRenderer::declareVariable() as they were in the
 *   renderer core (rendererDeclarations.cpp), moved without a change of order or rule.
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include <assert.h>
#include <string.h>

#include "channelRegistry.h"
#include "common/containers.h"
#include "common/diagnostics.h"
#include "common/rendererc.h"
#include "common/riVocabulary.h"
#include "common/variable.h"
#include "declarations.h"
#include "variableLookup.h"

namespace {
    int g_acquired = 0;                                 // the reference count
    CTrie<CVariable *> *g_declared = nullptr;           // declared variables by name
    CVariable *g_variables = nullptr;                   // list of all defined variables
    CArray<CVariable *> *g_globals = nullptr;           // array of global variables only
    CDeclarations::TGlobalObserver g_observer = nullptr; // the core's notification of its shading contexts

    // The hooks Graphics State's parameter lists and channel registry resolve names through
    CVariable *lookupHook(const char *name) {
        return CDeclarations::find(name);
    }
    // &CDeclarations::declare does not convert to a TVariableDeclaration (it takes a third, defaulted parameter)
    CVariable *declareHook(const char *name, const char *declaration) {
        return CDeclarations::declare(name, declaration, 0);
    }
} // namespace

///////////////////////////////////////////////////////////////////////
// Class				:	CDeclarations
// Method				:	acquire
// Description			:	Create the table on the first call
// Return Value			:
// Comments				:
void CDeclarations::acquire() {
    if (g_acquired++ != 0)
        return;

    // Init the declared variables
    g_declared = new CTrie<CVariable *>;
    // Allocate the global variables array
    g_globals = new CArray<CVariable *>;
    // This is the list of all variables
    g_variables = nullptr;

    // The display channels live in Graphics State's channel registry, which registers the five
    // built-ins (rgb, rgba, a, z, rgbaz) itself
    beginChannelRegistry();

    ////////////////////////////////////////////////////////////////////////////////////////////
    //
    //
    //			R E G I S T E R    O P T I O N S / A T T R I B U T E S
    //
    //
    ////////////////////////////////////////////////////////////////////////////////////////////
    // Define the options
    declare(RI_ARCHIVE, "string");
    declare(RI_PROCEDURAL, "string");
    declare(RI_TEXTURE, "string");
    declare(RI_SHADER, "string");
    declare(RI_DISPLAY, "string");
    declare(RI_RESOURCE, "string");
    declare(RI_GEOMETRY, "string");

    declare(RI_BUCKETSIZE, "int[2]");
    declare(RI_METABUCKETS, "int[2]");
    declare(RI_INHERITATTRIBUTES, "int");
    declare(RI_GRIDSIZE, "int");
    declare(RI_EYESPLITS, "int");
    declare(RI_TEXTUREMEMORY, "int");
    declare(RI_BRICKMEMORY, "int");
    declare(RI_NUMTHREADS, "int");
    declare(RI_THREADSTRIDE, "int");
    declare(RI_GEOCACHEMEMORY, "int");
    declare(RI_OTHRESHOLD, "color");
    declare(RI_ZTHRESHOLD, "color");

    declare(RI_RADIANCECACHE, "int");
    declare(RI_JITTER, "float");
    declare(RI_FALSECOLOR, "int");
    declare(RI_EMIT, "int");
    declare(RI_SAMPLESPECTRUM, "int");
    declare(RI_DEPTHFILTER, "string");
    declare(RI_FILTER, "string");
    declare(RI_MODE, "string");

    declare(RI_MAXDEPTH, "int");

    declare(RI_ENDOFFRAME, "int");
    declare(RI_FILELOG, "string");
    declare(RI_PROGRESS, "int");

    declare(RI_TEXTURECACHEENABLE, "int");
    declare(RI_TEXTURECACHEDIRECTORY, "string");

    // Option "sampling" (spec 039)
    declare(RI_SAMPLER, "string");
    declare(RI_SEED, "int");

    // File display variables

    declare("quantize", "float[4]");
    declare("dither", "float");
    declare("gamma", "float");
    declare("gain", "float");
    declare("near", "float");
    declare("far", "float");
    declare("Software", "string");
    declare("compression", "string");
    declare("NP", "float[16]");
    declare("Nl", "float[16]");

    // Define the attributes
    declare(RI_NUMPROBES, "int[2]");
    declare(RI_MINSUBDIVISION, "int");
    declare(RI_MAXSUBDIVISION, "int");
    declare(RI_MINSPLITS, "int");
    declare(RI_BOUNDEXPAND, "float");
    declare(RI_BINARY, "int");
    declare(RI_RASTERORIENT, "int");

    declare(RI_SPHERE, "float");
    declare(RI_DISPLACEMENT, "float");
    declare(RI_COORDINATESYSYTEM, "string");

    declare(RI_TESSELLATIONTOLERANCE, "float");

    declare(RI_DISPLACEMENTS, "int");
    declare(RI_BIAS, "float");
    declare(RI_MAXDIFFUSEDEPTH, "int");
    declare(RI_MAXSPECULARDEPTH, "int");
    declare(RI_SAMPLEMOTION, "int");

    declare(RI_HANDLE, "string");
    declare(RI_FILEMODE, "string");
    declare(RI_MAXERROR, "float");
    declare(RI_MAXPIXELDIST, "float");

    declare(RI_GLOBALMAP, "string");
    declare(RI_CAUSTICMAP, "string");
    declare(RI_SHADINGMODEL, "string");
    declare(RI_ESTIMATOR, "int");
    declare(RI_ILLUMINATEFRONT, "int");
    declare(RI_IOR, "float");
    declare(RI_IORRANGE, "float[2]");

    declare(RI_TRANSMISSION, "int");
    declare(RI_CAMERA, "int");
    declare(RI_SPECULAR, "int");
    declare(RI_DIFFUSE, "int");
    declare(RI_PHOTON, "int");

    declare(RI_DIFFUSEHITMODE, "string");
    declare(RI_SPECULARHITMODE, "string");
    declare(RI_TRANSMISSIONHITMODE, "string");
    declare(RI_CAMERAHITMODE, "string");
    declare(RI_SHADERFORMAT, "string");

    // Blobby implicit surfaces (spec 015). Both must be pre-declared here
    // or the RIB parser rejects them with "Parameter not declared" before
    // RiAttributeV/RiOptionV ever sees them.
    declare(RI_BLOBBYTOLERANCE, "float");
    declare(RI_BLOBBYOPCODEORDER, "string");
    declare(RI_DEFAULT, "string"); // Option "shaderformat" "default" token

    declare(RI_NAME, "string");

    declare(RI_HIDDEN, "int");
    declare(RI_BACKFACING, "int");

    declare(RI_SENSE, "string"); // Attribute "trimcurve" "sense" ("inside"/"outside")

    ////////////////////////////////////////////////////////////////////////////////////////////
    //
    //
    //			R E G I S T E R    D E F A U L T    V A R I A B L E S
    //
    //
    ////////////////////////////////////////////////////////////////////////////////////////////
    CVariable *tmp;
    tmp = declare("P", "global vertex point", PARAMETER_P);
    assert(tmp->entry == VARIABLE_P);
    tmp = declare("Ps", "global vertex point", PARAMETER_PS);
    assert(tmp->entry == VARIABLE_PS);
    tmp = declare("N", "global varying normal", PARAMETER_N);
    assert(tmp->entry == VARIABLE_N);
    tmp = declare("Ng", "global varying normal", PARAMETER_NG);
    assert(tmp->entry == VARIABLE_NG);
    tmp = declare("dPdu", "global vertex vector", PARAMETER_DPDU);
    assert(tmp->entry == VARIABLE_DPDU);
    tmp = declare("dPdv", "global vertex vector", PARAMETER_DPDV);
    assert(tmp->entry == VARIABLE_DPDV);
    tmp = declare("L", "global varying vector", PARAMETER_L);
    assert(tmp->entry == VARIABLE_L);
    tmp = declare("Cs", "global varying color", PARAMETER_CS);
    assert(tmp->entry == VARIABLE_CS);
    tmp = declare("Os", "global varying color", PARAMETER_OS);
    assert(tmp->entry == VARIABLE_OS);
    tmp = declare("Cl", "global varying color", PARAMETER_CL);
    assert(tmp->entry == VARIABLE_CL);
    tmp = declare("Ol", "global varying color", PARAMETER_OL);
    assert(tmp->entry == VARIABLE_OL);
    tmp = declare("Ci", "global varying color", PARAMETER_CI);
    assert(tmp->entry == VARIABLE_CI);
    tmp = declare("Oi", "global varying color", PARAMETER_OI);
    assert(tmp->entry == VARIABLE_OI);
    tmp = declare("s", "global varying float", PARAMETER_S);
    assert(tmp->entry == VARIABLE_S);
    tmp = declare("t", "global varying float", PARAMETER_T);
    assert(tmp->entry == VARIABLE_T);
    tmp = declare("st", "varying float[2]", PARAMETER_S | PARAMETER_T);
    tmp = declare("du", "global varying float", PARAMETER_DU | PARAMETER_DERIVATIVE);
    assert(tmp->entry == VARIABLE_DU);
    tmp = declare("dv", "global varying float", PARAMETER_DV | PARAMETER_DERIVATIVE);
    assert(tmp->entry == VARIABLE_DV);
    tmp = declare("u", "global varying float", PARAMETER_U);
    assert(tmp->entry == VARIABLE_U);
    tmp = declare("v", "global varying float", PARAMETER_V);
    assert(tmp->entry == VARIABLE_V);
    tmp = declare("I", "global varying vector", PARAMETER_I);
    assert(tmp->entry == VARIABLE_I);
    tmp = declare("E", "global varying point", PARAMETER_E);
    assert(tmp->entry == VARIABLE_E);
    tmp = declare("alpha", "global varying float", PARAMETER_ALPHA);
    assert(tmp->entry == VARIABLE_ALPHA);
    tmp = declare("time", "global varying float", PARAMETER_TIME);
    assert(tmp->entry == VARIABLE_TIME);
    tmp = declare("Pw", "global vertex htpoint", PARAMETER_P);
    tmp->entry = VARIABLE_PW;
    tmp = declare("Pz", "vertex float", PARAMETER_P);
    tmp->entry = VARIABLE_P;
    tmp = declare("ncomps", "global uniform float", PARAMETER_NCOMPS);
    assert(tmp->entry == VARIABLE_NCOMPS);
    tmp = declare("dtime", "global uniform float", PARAMETER_DTIME);
    assert(tmp->entry == VARIABLE_DTIME);
    tmp = declare("dPdtime", "global varying vector", PARAMETER_DPDTIME);
    assert(tmp->entry == VARIABLE_DPDTIME);

    tmp = declare("width", "global varying float", 0);
    assert(tmp->entry == VARIABLE_WIDTH);
    tmp = declare("constantwidth", "global constant float", 0);
    assert(tmp->entry == VARIABLE_CONSTANTWIDTH);
    tmp = declare("Np", "uniform normal", PARAMETER_NG);

    // Parameter lists (state/pl.cpp) resolve declared names through this hook
    installVariableLookup(&lookupHook);

    // The channel registry declares new channels' variables through the declared-variable table
    installVariableDeclaration(&declareHook);
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDeclarations
// Method				:	release
// Description			:	Destroy the table on the last call
// Return Value			:
// Comments				:
void CDeclarations::release() {
    if (g_acquired == 0)
        return;
    if (--g_acquired != 0)
        return;

    // The declarations are going away: stop resolving names through them
    installVariableLookup(nullptr);
    installVariableDeclaration(nullptr);

    // Ditch the display channels
    endChannelRegistry();

    // The variables
    assert(g_declared != nullptr);
    g_declared->destroy();
    g_declared = nullptr;
    g_variables = nullptr;
    delete g_globals;
    g_globals = nullptr;
}

bool CDeclarations::active() {
    return g_acquired != 0;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDeclarations
// Method				:	declare
// Description			:	Define a new variable
// Return Value			:	The new variable if successful, NULL otherwise
// Comments				:
CVariable *CDeclarations::declare(const char *name, const char *type, int marker) {
    CVariable cVariable, *nVariable;

    if (g_declared == nullptr)
        return nullptr;

    if (parseVariable(&cVariable, name, type)) {
        // Parse successful, insert the variable into the dictionary
        CVariable *oVariable;

        if (g_declared->find(cVariable.name, oVariable)) {
            if ((cVariable.numFloats != oVariable->numFloats) ||
                (cVariable.numItems != oVariable->numItems)) {
                error(CODE_SYSTEM, "Variable \"%s\" was previously defined differently\n", cVariable.name);
            }
            else {
                if (cVariable.type != oVariable->type) {
                    if ((cVariable.type == TYPE_STRING) || (oVariable->type == TYPE_STRING)) {
                        error(CODE_SYSTEM, "Variable \"%s\" was previously defined differently\n", cVariable.name);
                    }
                }
            }

            // Overwrite the container
            oVariable->container = cVariable.container;

            // If we're overwriting "st" make sure to overwrite the s and t too
            if (oVariable->usageMarker == (PARAMETER_S | PARAMETER_T)) {
                if (g_declared->find("s", nVariable)) {
                    nVariable->container = cVariable.container;
                }
                else {
                    assert(FALSE);
                }

                if (g_declared->find("t", nVariable)) {
                    nVariable->container = cVariable.container;
                }
                else {
                    assert(FALSE);
                }
            }

            return oVariable;
        }

        // Add the new variable into the variables list
        nVariable = new CVariable;
        nVariable[0] = cVariable;
        nVariable->next = g_variables;
        nVariable->usageMarker = marker;
        nVariable->defaultValue = NULL;
        nVariable->accessor = -1;
        g_variables = nVariable;

        // Insert the variable into the variables trie
        g_declared->insert(nVariable->name, nVariable);

        // Make the variable global if it needs to be global
        if (nVariable->storage == STORAGE_GLOBAL) {
            makeGlobal(nVariable);
        }

        return nVariable;
    }
    else {
        return nullptr;
    }
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDeclarations
// Method				:	find
// Description			:	Lookup a variable
// Return Value			:	The variable if found, NULL otherwise
// Comments				:
CVariable *CDeclarations::find(const char *name) {
    CVariable *cVariable;

    if (g_declared != nullptr && g_declared->find(name, cVariable))
        return cVariable;

    return nullptr;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CDeclarations
// Method				:	makeGlobal
// Description			:	Forcefully make a variable global
// Return Value			:
// Comments				:	The observer flushes the states of the live shading contexts
void CDeclarations::makeGlobal(CVariable *var) {
    assert(g_globals != nullptr);

    var->entry = g_globals->numItems;
    var->storage = STORAGE_GLOBAL;
    g_globals->push(var);

    if (g_observer != nullptr)
        g_observer(var);
}

int CDeclarations::numGlobals() {
    return g_globals != nullptr ? g_globals->numItems : 0;
}

CVariable *CDeclarations::global(int index) {
    return g_globals->array[index];
}

CVariable *CDeclarations::variableList() {
    return g_variables;
}

void CDeclarations::setGlobalObserver(TGlobalObserver observer) {
    g_observer = observer;
}
