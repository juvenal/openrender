/**
 * Project: Pixie
 *
 * File: depository.h
 *
 * Description:
 *   This file defines the interface for depository.
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
//  File				:	depository.h
//  Classes				:	CDepository
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#ifndef DEPOSITORY_H
#define DEPOSITORY_H

#include "common/global.h"
#include "map.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CDepositorySample
// Description			:	This class holds a depository sample
// Comments				:
class CDepositorySample : public CMapItem {
    public:
        float C[7];
};

///////////////////////////////////////////////////////////////////////
// Class				:	CLocalHash
// Description			:	A hash that holds the contribution that comes from nearby geometry
// Comments				:
class CDepository : public CMap<CDepositorySample> {
    public:
        CDepository();
        ~CDepository();

        void lookup(float *, const float *, const float *);
};

#endif
