/**
 * Project: openRender
 *
 * File: variableLookup.cpp
 *
 * Description:
 *   Implements the Graphics State variable-lookup hook.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "variableLookup.h"

// Installed and removed on one thread, with the declaration lifecycle
// (CRenderer::initDeclarations / shutdownDeclarations).
static TVariableLookup installedLookup = nullptr;

void installVariableLookup(TVariableLookup lookup) {
    installedLookup = lookup;
}

CVariable *lookupVariable(const char *name) {
    return installedLookup != nullptr ? installedLookup(name) : nullptr;
}
