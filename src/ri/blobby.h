/**
 * Project: openRender
 *
 * File: blobby.h
 *
 * Description:
 *   This file defines the interface for blobby.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

///////////////////////////////////////////////////////////////////////
//
//  File				:	blobby.h
//  Classes				:	-
//  Description			:	Renderer integration for RiBlobby: turns a
//							validated code array into the CPolygonMesh that
//							addObject() hands to every hider
//							(spec 015, research Decision 1).
//
//							The surface is derived once, here, in the
//							geometry domain, before any hider runs. No
//							hider contains blobby-specific code (FR-022).
//
////////////////////////////////////////////////////////////////////////
#ifndef BLOBBY_H
#define BLOBBY_H

#include "common/global.h"

class CAttributes;
class CBlobbyProgram;
class CObject;
class CXform;

///////////////////////////////////////////////////////////////////////
// Function				:	blobbyCreate
// Description			:	Extract `program`'s surface and wrap it in a
//							CPolygonMesh carrying P, N and the blended
//							per-blob primvars.
// Return Value			:	The mesh, or NULL when the field yields no
//							geometry (FR-030).
// Comments				:	`xform` is the blobby's own transform, not
//							identity: vertices stay in object space and
//							CSurface::sample() applies xform->from, which
//							is what preserves instancing.
///////////////////////////////////////////////////////////////////////
CObject *blobbyCreate(CAttributes *attributes, CXform *xform, const CBlobbyProgram *program, const CBlobbyProgram *programClose, int numParameters, const char **tokens, const void **parameters);

///////////////////////////////////////////////////////////////////////
// Function				:	blobbyDefaultCellSize
// Description			:	Cell size to use when the scene sets no
//							tolerance, derived from the program's own field
//							extent (FR-025).
///////////////////////////////////////////////////////////////////////
float blobbyDefaultCellSize(const CBlobbyProgram *program);

///////////////////////////////////////////////////////////////////////
// Function				:	blobbyCellSizeFromTolerance
// Description			:	Validate an author-supplied tolerance and turn
//							it into a cell size. Zero, negative, and absurd
//							values produce a diagnostic and fall back to
//							the default (US6 scenario 4).
///////////////////////////////////////////////////////////////////////
float blobbyCellSizeFromTolerance(const CBlobbyProgram *program, float tolerance);

#endif
