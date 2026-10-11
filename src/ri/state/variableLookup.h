/**
 * Project: openRender
 *
 * File: variableLookup.h
 *
 * Description:
 *   The Graphics State hook that resolves declared variable names. The
 *   renderer core installs its lookup while declarations exist, so the
 *   parameter-list code never names the renderer core (spec 023, R8).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef VARIABLE_LOOKUP_H
#define VARIABLE_LOOKUP_H

class CVariable;

using TVariableLookup = CVariable *(*)(const char *name);

// Installs the lookup used to resolve declared variable names (nullptr uninstalls).
void installVariableLookup(TVariableLookup lookup);

// Returns lookup(name), or nullptr when no lookup is installed.
CVariable *lookupVariable(const char *name);

#endif
