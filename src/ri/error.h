/**
 * Project: Pixie
 *
 * File: error.h
 *
 * Description:
 *   This file defines the interface for error.
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
//  File				:	error.h
//  Classes				:	-
//  Description			:	The error display functions
//
////////////////////////////////////////////////////////////////////////
#ifndef ERROR_H
#define ERROR_H

#include "common/global.h" // The global header file

// Error codes for the error function (Used by sfError)
typedef enum {
    CODE_NOERROR,
    CODE_NOMEM,
    CODE_SYSTEM,
    CODE_NOFILE,
    CODE_BADFILE,
    CODE_VERSION,
    CODE_INCAPABLE,
    CODE_OPTIONAL,
    CODE_UNIMPLEMENT,
    CODE_LIMIT,
    CODE_BUG,
    CODE_NOTSTARTED,
    CODE_NESTING,
    CODE_NOTOPTIONS,
    CODE_NOTATTRIBS,
    CODE_NOTPRIMS,
    CODE_ILLSTATE,
    CODE_BADMOTION,
    CODE_BADSOLID,
    CODE_BADTOKEN,
    CODE_RANGE,
    CODE_CONSISTENCY,
    CODE_BADHANDLE,
    CODE_NOSHADER,
    CODE_MISSINGDATA,
    CODE_SYNTAX,
    CODE_MATH,
    CODE_LOG,
    CODE_SCRIPT,
    CODE_PRINTF,
    CODE_RESOLUTION,
    CODE_STATS,
    CODE_PROGRESS
} EErrorCode;

void error(EErrorCode, const char *, ...);
void warning(EErrorCode, const char *, ...);
void fatal(EErrorCode, const char *, ...);
void info(EErrorCode, const char *, ...);

#endif
