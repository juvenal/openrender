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

// The declaration hook (spec 025, FR-016): declares a variable from a declaration string. The channel
// registry reaches the renderer core's declared-variable table through it. The renderer core installs a
// captureless adapter while declarations exist. name may be nullptr (the name is then parsed from the
// declaration).
using TVariableDeclaration = CVariable *(*)(const char *name, const char *declaration);

// Installs the declaration hook (nullptr uninstalls).
void installVariableDeclaration(TVariableDeclaration declaration);

// Returns declaration(name, decl), or nullptr when no hook is installed.
CVariable *declareVariableThroughHook(const char *name, const char *declaration);

#endif
