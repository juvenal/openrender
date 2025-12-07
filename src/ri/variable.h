/**
 * Project: Pixie
 *
 * File: variable.h
 *
 * Description:
 *   This file defines the interface for variable.
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
//  File				:	variable.h
//  Classes				:	CVariable
//  Description			:	This class holds variable information
//
////////////////////////////////////////////////////////////////////////
#ifndef VARIABLE_H
#define VARIABLE_H

#include "common/global.h" // The global header file
#include "rendererc.h"
#include "ri_config.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CVariable
// Description			:	This class holds information about a variable
// Comments				:	FIXME: Maximum length of a variable's name is 63 characters
class CVariable {
    public:
        char name[VARIABLE_NAME_LENGTH]; // Name as it is referenced

        int numItems;    // Number of items if this is an array
        int numFloats;   // Number of floats per variable (1 for float, 3 for color, 16 for matrix)
        int entry;       // The global variable number as it's referenced from a grid (-1 if not global)
        int usageMarker; // The usage or flag

        void *defaultValue; // Points to the memory area that holds the default value for the variable
        CVariable *next;    // Linked list next (used to maintain shader parameter lists)

        int accessor; // Which entry in the locals array are we?

        EVariableType type;       // Type
        EVariableClass container; // Container type
        EVariableStorage storage; // If the variable is global, parameter or mutable parameter
};

int parseVariable(CVariable *, const char *, const char *);

#endif
