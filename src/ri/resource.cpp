/**
 * Project: Pixie
 *
 * File: resource.cpp
 *
 * Description:
 *   This file implements the functionality for resource.
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
//  File				:	resource.cpp
//  Classes				:	CResource
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#include "resource.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CResource
// Method				:	CResource
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CResource::CResource(const char *name, CAttributes *attributes, CXform *xform) {
    this->name = strdup(name);
    this->attributes = new CAttributes(attributes);
    this->xform = new CXform(xform);
    next = NULL;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CResource
// Method				:	CResource
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CResource::~CResource() {
    free(name);
    delete attributes;
    delete xform;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CResource
// Method				:	restore
// Description			:	Restore whatever that was saved in this resource
// Return Value			:	-
// Comments				:
void CResource::restore(CAttributes *attributes, CXform *xform) {
}
