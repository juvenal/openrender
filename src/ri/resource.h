/**
 * Project: Pixie
 *
 * File: resource.h
 *
 * Description:
 *   This file defines the interface for resource.
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
//  File				:	resource.h
//  Classes				:	CResource
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef RESOURCE_H
#define RESOURCE_H

#include "attributes.h"
#include "common/global.h"
#include "xform.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CResource
// Description			:	Holds resources
// Comments				:
class CResource {
    public:
        CResource(const char *name, CAttributes *attributes, CXform *xform);
        ~CResource();

        void restore(CAttributes *attributes, CXform *xform);

        char *name;
        CAttributes *attributes;
        CXform *xform;
        CResource *next;
};

#endif
