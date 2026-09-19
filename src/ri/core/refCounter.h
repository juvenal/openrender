/**
 * Project: openRender
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

        // A copy never inherits the source's reference count -- refCount is
        // per-instance state, not part of an object's logical value. The
        // copy ctor starts fresh at 0 (matching the default ctor); the copy
        // assignment leaves the target's own refCount untouched (it may
        // already have live references). Needed explicitly now that
        // atomic_int32's implicit copy operations are deleted, which would
        // otherwise make every CRefCounter-derived class non-copyable.
        CRefCounter(const CRefCounter &) : refCount(0) {}
        CRefCounter &operator=(const CRefCounter &) { return *this; }

        void attach() { atomicIncrement(refCount); }
        void detach() {
            if (atomicDecrement(refCount) == 0)
                delete this;
        }

        atomic_int32 refCount;
};

#endif
