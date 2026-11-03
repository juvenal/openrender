/**
 * Project: openRender
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

class CRiInterface;

// Begin a session with the back-end registered under key (riHooks.h), without RiBegin's full start (no
// .orenderrc, no network, no frame range): a CRiStandardSession, RiInit, the factory's context, riInstallContext.
// Returns the context, or nullptr after an error report when no back-end is registered under key or a context
// is already active ("Already started"). One light session at a time.
CRiInterface *RiBeginBackEnd(const char *key, void *argument);

// End it: delete the context (which uninstalls it) and end the standard session. Like RiBegin's light
// predecessor it does not run RiTini.
void RiEndBackEnd(CRiInterface *context);

#endif
