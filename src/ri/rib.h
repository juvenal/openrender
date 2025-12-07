/**
 * Project: Pixie
 *
 * File: rib.h
 *
 * Description:
 *   This file defines the interface for rib.
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
//  File				:	rib.h
//  Classes				:	-
//  Description			:	RIB Parser headers
//
////////////////////////////////////////////////////////////////////////
#ifndef RIB_H
#define RIB_H

void ribParse(const char *, void (*callback)(const char *, ...));
void parserCleanup();

#endif
