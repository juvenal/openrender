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

#include "shadeop/shadeopModule.h" // CShadeopOperand, shadeopOperandData(), shadeopOperandExec()

///////////////////////////////////////////////////////////////////////
// Class				:	CDSO
// Description			:	Holds a DSO shader info: the operand the shadeop module selected, with the
//							name and prototype the shader asked for
// Comments				:	A real class (shader.h, serviceLookup.h, rendererServices.h and renderer.h
//							forward-declare it), kept an aggregate (the shading tests make stub ones with = {}).
//							The renderer creates one per operand and hands it to the shadeop registry as the
//							operand's host object, which destroys it when the operand is released.
class CDSO {
    public:
        CShadeopOperand *operand; // The operand: its module, init data, per-thread data and exec
        char *name;               // Name of the DSO shader (malloc'd)
        char *prototype;          // Prototype of the DSO shader (malloc'd)
};

#endif
