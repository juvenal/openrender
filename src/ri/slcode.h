/**
 * Project: Pixie
 *
 * File: slcode.h
 *
 * Description:
 *   This file defines the interface for slcode.
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
//  File				:	slcode.h
//  Classes				:	-
//  Description			:	This file defines the entry point for all the
//							shading language functions/opcodes
//
////////////////////////////////////////////////////////////////////////
#ifndef SLCODE_H
#define SLCODE_H

#define DEFOPCODE(name, text, nargs, expr_pre, expr, expr_update, expr_post, params) OPCODE_##name,

#define DEFSHORTOPCODE(name, text, nargs, expr_pre, expr, expr_update, expr_post, params) OPCODE_##name,

#define DEFLINKOPCODE(name, text, nargs) OPCODE_##name,

#define DEFLINKFUNC(name, text, prototype, par) FUNCTION_##name,

#define DEFFUNC(name, text, prototype, expre_pre, expr, expr_update, expr_post, par) FUNCTION_##name,

#define DEFLIGHTFUNC(name, text, prototype, expre_pre, expr, expr_update, expr_post, par) FUNCTION_##name,

#define DEFSHORTFUNC(name, text, prototype, expre_pre, expr, expr_update, expr_post, par) FUNCTION_##name,

// Create an enumerated type of all the opcodes and functions
typedef enum {
#include "scriptFunctions.h"
#include "scriptOpcodes.h"
    OPCODE_NOP
} ESlCode;

#undef DEFOPCODE
#undef DEFSHORTOPCODE
#undef DEFFUNC
#undef DEFLIGHTFUNC
#undef DEFSHORTFUNC
#undef DEFLINKOPCODE
#undef DEFLINKFUNC

#endif
