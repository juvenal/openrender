/**
 * Project: Pixie
 *
 * File: linsys.h
 *
 * Description:
 *   This file defines the interface for linsys.
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
//  File				:	linsys.h
//  Classes				:	-
//  Description			:	Functions for solving linear system of equations
//
////////////////////////////////////////////////////////////////////////
#ifndef LINSYS_H
#define LINSYS_H

#include "common/global.h"

int linSolve(float *A, float *b, int n, int nrhs);

#endif
