/**
 * Project: openRender
 *
 * File: channelRegistry.cpp
 *
 * Description:
 *   Implements the AOV/channel registry (spec 025). declare() is
 *   CRenderer::declareDisplayChannel() moved unchanged: error strings, codes,
 *   order and ownership are the same.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "channelRegistry.h"

#include "common/diagnostics.h"
#include "variableLookup.h"

void CChannelRegistry::add(CDisplayChannel *channel) {
    inOrder.push(channel);
    byName.insert(channel->name, channel);
}

CChannelRegistry::CChannelRegistry() {
    add(new CDisplayChannel("rgb", NULL, 3, 0));
    add(new CDisplayChannel("rgba", NULL, 4, 0));
    add(new CDisplayChannel("a", NULL, 1, 3));
    add(new CDisplayChannel("z", NULL, 1, 4));
    add(new CDisplayChannel("rgbaz", NULL, 5, 0));
}

// Each channel is deleted exactly once, through the ordered list. The trie only indexes them (its
// destructor frees nodes, never the channels), and the array frees only its own storage.
CChannelRegistry::~CChannelRegistry() {
    for (int i = 0; i < inOrder.numItems; i++)
        delete inOrder.array[i];
}

///////////////////////////////////////////////////////////////////////
// Class				:	CChannelRegistry
// Method				:	declare
// Description			:	Define a new displayChannel from a declaration string
// Return Value			:	The new display channel, NULL otherwise
// Comments				:
CDisplayChannel *CChannelRegistry::declare(const char *type) {
    CDisplayChannel *oChannel;
    CVariable cVariable, *oVariable;

    if (parseVariable(&cVariable, NULL, type) == FALSE) {
        return NULL;
    }

    if (byName.find(cVariable.name, oChannel) == TRUE) {
        if ((oChannel->numSamples == cVariable.numFloats) &&
            ((cVariable.storage != STORAGE_GLOBAL) || oChannel->outType == cVariable.entry)) {
            return oChannel;
        }
        else {
            error(CODE_SYSTEM, "Channel \"%s\" was previously defined differently\n", cVariable.name);
            return NULL;
        }
    }
    else {
        oVariable = declareVariableThroughHook(NULL, type);

        if (oVariable == NULL) {
            error(CODE_SYSTEM, "Channel definition \"%s\" is ill formed\n", type);
        }
        else {
            const int samples = oVariable->numFloats;
            const int outType = (oVariable->storage == STORAGE_GLOBAL) ? oVariable->entry : -1;
            oChannel = new CDisplayChannel(oVariable->name, oVariable, samples, -1, outType);
            add(oChannel);

            return oChannel;
        }
    }
    return NULL;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CChannelRegistry
// Method				:	declare
// Description			:	Define a new displayChannel for a declared variable
// Return Value			:	The new display channel, NULL otherwise
// Comments				:
CDisplayChannel *CChannelRegistry::declare(CVariable *var) {
    CDisplayChannel *oChannel;

    if (byName.find(var->name, oChannel) == TRUE) {
        if (oChannel->variable == var) {
            return oChannel;
        }
        else {
            return NULL;
        }
    }
    else {
        oChannel = new CDisplayChannel(var->name, var, var->numFloats, -1, var->entry);
        add(oChannel);

        return oChannel;
    }

    return NULL;
}

CDisplayChannel *CChannelRegistry::retrieve(const char *name) const {
    CDisplayChannel *oChannel;
    if (byName.find(name, oChannel) == TRUE) {
        return oChannel;
    }
    else {
        return NULL;
    }
}

void CChannelRegistry::resetUsage() {
    for (int i = 0; i < inOrder.numItems; i++) {
        if (inOrder.array[i]->variable != NULL) {
            inOrder.array[i]->sampleStart = -1;
        }
    }
}

// The session instance: created and destroyed on the renderer's thread with the declarations.
static CChannelRegistry *sessionRegistry = nullptr;

void beginChannelRegistry() {
    delete sessionRegistry; // a session that was never ended starts fresh
    sessionRegistry = new CChannelRegistry;
}

void endChannelRegistry() {
    delete sessionRegistry;
    sessionRegistry = nullptr;
}

CChannelRegistry *channelRegistry() {
    return sessionRegistry;
}
