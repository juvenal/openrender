/**
 * Project: openRender
 *
 * File: hider.cpp
 *
 * Description:
 *   CHider (spec 029): builds the shading context a hider owns. References no CRenderer symbol.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "hider.h"

///////////////////////////////////////////////////////////////////////
// Class				:	CHider
// Method				:	CHider
// Description			:	Construct the hider and the shading context of its thread
// Return Value			:	-
// Comments				:
CHider::CHider(int thread) : context(thread) {
}

///////////////////////////////////////////////////////////////////////
// Class				:	CHider
// Method				:	~CHider
// Description			:	Destroy the hider; the context goes after this body
// Return Value			:	-
// Comments				:
CHider::~CHider() {
}
