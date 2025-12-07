/**
 * Project: Pixie
 *
 * File: error.cpp
 *
 * Description:
 *   This file implements the functionality for error.
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
//  File				:	error.cpp
//  Classes				:	-
//  Description			:	The error display functions
//
////////////////////////////////////////////////////////////////////////
#include <stdarg.h>

#include "common/os.h"
#include "error.h"
#include "ri.h"
#include "riInterface.h"
#include "ri_config.h"

///////////////////////////////////////////////////////////////////////
// Function				:	translate
// Description			:	Translate an error code
// Return Value			:
// Comments				:
static int translate(EErrorCode c) {
    int code;

    switch (c) {
    case CODE_NOERROR:
        code = RIE_NOERROR;
        break;
    case CODE_NOMEM:
        code = RIE_NOMEM;
        break;
    case CODE_SYSTEM:
        code = RIE_SYSTEM;
        break;
    case CODE_NOFILE:
        code = RIE_NOFILE;
        break;
    case CODE_BADFILE:
        code = RIE_BADFILE;
        break;
    case CODE_VERSION:
        code = RIE_VERSION;
        break;
    case CODE_INCAPABLE:
        code = RIE_INCAPABLE;
        break;
    case CODE_OPTIONAL:
        code = RIE_OPTIONAL;
        break;
    case CODE_UNIMPLEMENT:
        code = RIE_UNIMPLEMENT;
        break;
    case CODE_LIMIT:
        code = RIE_LIMIT;
        break;
    case CODE_BUG:
        code = RIE_BUG;
        break;
    case CODE_NOTSTARTED:
        code = RIE_NOTSTARTED;
        break;
    case CODE_NESTING:
        code = RIE_NESTING;
        break;
    case CODE_NOTOPTIONS:
        code = RIE_NOTOPTIONS;
        break;
    case CODE_NOTATTRIBS:
        code = RIE_NOTATTRIBS;
        break;
    case CODE_NOTPRIMS:
        code = RIE_NOTPRIMS;
        break;
    case CODE_ILLSTATE:
        code = RIE_ILLSTATE;
        break;
    case CODE_BADMOTION:
        code = RIE_BADMOTION;
        break;
    case CODE_BADSOLID:
        code = RIE_BADSOLID;
        break;
    case CODE_BADTOKEN:
        code = RIE_BADTOKEN;
        break;
    case CODE_RANGE:
        code = RIE_RANGE;
        break;
    case CODE_CONSISTENCY:
        code = RIE_CONSISTENCY;
        break;
    case CODE_BADHANDLE:
        code = RIE_BADHANDLE;
        break;
    case CODE_NOSHADER:
        code = RIE_NOSHADER;
        break;
    case CODE_MISSINGDATA:
        code = RIE_MISSINGDATA;
        break;
    case CODE_SYNTAX:
        code = RIE_SYNTAX;
        break;
    case CODE_MATH:
        code = RIE_MATH;
        break;
    case CODE_LOG:
        code = RIE_LOG;
        break;
    case CODE_SCRIPT:
        code = RIE_SCRIPT;
        break;
    case CODE_PRINTF:
        code = RIE_PRINTF;
        break;
    case CODE_RESOLUTION:
        code = RIE_LOG;
        break;
    case CODE_STATS:
        code = RIE_STATS;
        break;
    case CODE_PROGRESS:
        code = RIE_PROGRESS;
        break;
    default:
        error(CODE_BUG, "Unknown error code used\n");
        code = RIE_NOERROR;
        break;
    }

    return code;
}

///////////////////////////////////////////////////////////////////////
// Function				:	error
// Description			:	Generate an error message
// Return Value			:
// Comments				:
void error(EErrorCode code, const char *mes, ...) {
    char tmp[OS_MAX_PATH_LENGTH];
    va_list args;

    va_start(args, mes);
    vsprintf(tmp, mes, args);

    renderMan->RiError(translate(code), RIE_ERROR, tmp);
    va_end(args);
}

///////////////////////////////////////////////////////////////////////
// Function				:	warning
// Description			:	Generate a warning message
// Return Value			:
// Comments				:
void warning(EErrorCode code, const char *mes, ...) {
    char tmp[OS_MAX_PATH_LENGTH];
    va_list args;

    va_start(args, mes);
    vsprintf(tmp, mes, args);

    renderMan->RiError(translate(code), RIE_WARNING, tmp);
    va_end(args);
}

///////////////////////////////////////////////////////////////////////
// Function				:	fatal
// Description			:	Generate a fatal message
// Return Value			:
// Comments				:
void fatal(EErrorCode code, const char *mes, ...) {
    char tmp[OS_MAX_PATH_LENGTH];
    va_list args;

    va_start(args, mes);
    vsprintf(tmp, mes, args);

    renderMan->RiError(translate(code), RIE_SEVERE, tmp);
    va_end(args);
}

///////////////////////////////////////////////////////////////////////
// Function				:	info
// Description			:	Generate an info message
// Return Value			:
// Comments				:
void info(EErrorCode code, const char *mes, ...) {
    char tmp[OS_MAX_PATH_LENGTH];
    va_list args;

    va_start(args, mes);
    vsprintf(tmp, mes, args);

    renderMan->RiError(translate(code), RIE_INFO, tmp);
    va_end(args);
}
