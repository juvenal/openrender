/**
 * Project: openRender
 *
 * File: declarations.h
 *
 * Description:
 *   The declared-variable table of Graphics State (spec 038): the trie of declared names, the list of
 *   variables, the array of global variables and its numbering, and the standard declared set. It used to be
 *   statics of the renderer core (CRenderer::declaredVariables, variables, globalVariables), which made the
 *   RIB grammar, the RIB writer, the preview and the data viewer link the core to parse a parameter list.
 *
 *   One table per process at a time, reference counted: the first acquire() builds it and declares the
 *   standard set in exactly the order the core always did (every VARIABLE_* entry index is unchanged), and the
 *   last release() destroys it; a release with nothing acquired does nothing. Single-threaded: acquired and
 *   released by the session's thread, like the hooks of variableLookup.h it installs.
 *
 *   Depends on Infrastructure and on Graphics State's own channel registry and variable-lookup hook only.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef DECLARATIONS_H
#define DECLARATIONS_H

class CVariable;

class CDeclarations {
    public:
        // First acquire: create the table; begin the channel registry; declare the standard set in exactly
        // today's order; install installVariableLookup / installVariableDeclaration (variableLookup.h) on this
        // table. Later acquires only count.
        static void acquire();

        // Last release: uninstall the two hooks, end the channel registry, destroy the table. A release with
        // nothing acquired does nothing.
        static void release();

        static bool active();

        // CRenderer::declareVariable's semantics, unchanged: parse; the redeclaration diagnostic
        // ("Variable \"%s\" was previously defined differently\n", CODE_SYSTEM); the "st" rule; and for a
        // global storage class: number the variable (entry = count, storage = STORAGE_GLOBAL), append it to
        // the global array, then call the observer. nullptr if the declaration does not parse or there is no
        // table.
        static CVariable *declare(const char *name, const char *type, int marker = 0);

        // nullptr if absent or no table
        static CVariable *find(const char *name);

        // Forcefully make an already declared variable global (the display code does this for an output
        // variable): number it, append it to the global array, call the observer.
        static void makeGlobal(CVariable *variable);

        static int numGlobals();
        static CVariable *global(int index);

        // The head of the list of every variable, the last one declared first (CVariable::next walks it);
        // nullptr if there is no table.
        static CVariable *variableList();

        // Called after a global variable is added; the renderer core installs the notification of its live
        // shading contexts. nullptr uninstalls.
        using TGlobalObserver = void (*)(CVariable *);
        static void setGlobalObserver(TGlobalObserver observer);
};

#endif
