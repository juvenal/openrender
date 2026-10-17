/**
 * Project: openRender
 *
 * File: dsoHandle.h
 *
 * Description:
 *   The loaded-DSO-shadeop record the shading library calls through. It lives in
 *   the shading library because the library's executor and the .rslo loader
 *   dereference it; the renderer core creates, caches and frees the records
 *   (CRenderer::getDSO) and hands them to the library through the renderer
 *   services (CRendererServices::getDSO).
 *
 * Authors:
 *   Okan Arikan <okan@cs.utexas.edu>
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 1999 - 2003, Okan Arikan <okan@cs.utexas.edu>
 *               2022 - 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef DSOHANDLE_H
#define DSOHANDLE_H

#include "ri/render/shadeop.h" // dsoInitFunction, dsoExecFunction, dsoCleanupFunction

///////////////////////////////////////////////////////////////////////
// Class				:	CDSO
// Description			:	Holds a DSO shader info
// Comments				:
class CDSO {
    public:
        void *handle;               // The handle to the module that implements the DSO shader
        dsoInitFunction init;       // Init function
        dsoExecFunction exec;       // Execute function
        dsoCleanupFunction cleanup; // Cleanup function
        char *name;                 // Name of the DSO shader
        char *prototype;            // Prototype of the DSO shader
        CDSO *next;
};

#endif
