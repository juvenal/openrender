/**
 * Project: openRender
 *
 * File: delayed.h
 *
 * Description:
 *   This file defines the interface for delayed.
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
//  File				:	delayed.h
//  Classes				:	CDelayed
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef DELAYED_H
#define DELAYED_H

#include "common/global.h"
#include "object.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CDelayedObject
// Description			:	Contains a delayed object
// Comments				:
class CDelayedObject : public CObject {
    public:
        CDelayedObject(CAttributes *, CXform *, const float *, const float *, void (*subdivisionFunction)(void *, float), void (*freeFunction)(void *), void *, int *drc = NULL);
        ~CDelayedObject();

        // Object interface
        void intersect(IRayContext &, CRay *);
        void dice(IDicer &);
        void instantiate(CAttributes *, CXform *, IObjectSink *) const;

        void (*subdivisionFunction)(void *, float);
        void (*freeFunction)(void *);
        void *data;
        int *dataRefCount;

        vector objectBmin, objectBmax;

        int processed;
};

///////////////////////////////////////////////////////////////////////
// Class				:	CDelayedInstance
// Description			:	Contains an instance object
// Comments				:
class CDelayedInstance : public CObject {
    public:
        CDelayedInstance(CAttributes *, CXform *, CObject *);
        ~CDelayedInstance();
        int honorsRegion() const { return FALSE; } // An ObjectInstance cannot be an operand (spec 021)

        // Object interface
        void intersect(IRayContext &, CRay *);
        void dice(IDicer &);
        void instantiate(CAttributes *, CXform *, IObjectSink *) const;

        CObject *instance;
        int processed;
};

#endif
