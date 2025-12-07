/**
 * Project: Pixie
 *
 * File: refCounter.cpp
 *
 * Description:
 *   This file implements the functionality for refCounter.
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
//  File				:	refCounter.cpp
//  Classes				:	CRefCounter
//  Description			:
//
////////////////////////////////////////////////////////////////////////
#include "refCounter.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CRefCounter
// Method				:	CRefCounter
// Description			:	Ctor
// Return Value			:	-
// Comments				:
CRefCounter::CRefCounter() {
    refCount = 0;
}

///////////////////////////////////////////////////////////////////////
// Class				:	CRefCounter
// Method				:	~CRefCounter
// Description			:	Dtor
// Return Value			:	-
// Comments				:
CRefCounter::~CRefCounter() {
}
