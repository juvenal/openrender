/**
 * Project: Pixie
 *
 * File: refCounter.h
 *
 * Description:
 *   This file defines the interface for refCounter.
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
//  File				:	refCounter.h
//  Classes				:	CRefCounter
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef REFCOUNTER_H
#define REFCOUNTER_H

#include "atomic.h"
#include "common/global.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CRefCounter
// Description			:	Reference counter
// Comments				:
class CRefCounter {
    public:
        CRefCounter();
        virtual ~CRefCounter();

        void attach() { atomicIncrement(&refCount); }
        void detach() {
            if (atomicDecrement(&refCount) == 0)
                delete this;
        }

        int refCount;
};

#endif
