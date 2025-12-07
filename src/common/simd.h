/**
 * Project: Pixie
 *
 * File: simd.h
 *
 * Description:
 *   This file defines the interface for simd.
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
//  File				:	simd.h
//  Classes				:	-
//  Description			:	The simd operations
//
////////////////////////////////////////////////////////////////////////
#ifndef SIMD_H
#define SIMD_H

#include "global.h"

#define perform2(__count, __dest, __src1, __src2, __OP) \
    for (int i = __count >> 2; i > 0; i--)              \
        *__dest++ = (*__src1++)__OP(*__src2++);         \
    switch (__count & 3) {                              \
    case 4:                                             \
        *__dest++ = (*__src1++)__OP(*__src2++);         \
    case 3:                                             \
        *__dest++ = (*__src1++)__OP(*__src2++);         \
    case 2:                                             \
        *__dest++ = (*__src1++)__OP(*__src2++);         \
    case 1:                                             \
        *__dest++ = (*__src1++)__OP(*__src2++);         \
    default:                                            \
        break;                                          \
    }

#define perform1(__count, __dest, __src1, __OP) \
    for (int i = __count >> 2; i > 0; i--)      \
        *__dest++ __OP(*__src1++);              \
    switch (__count & 3) {                      \
    case 3:                                     \
        *__dest++ __OP(*__src1++);              \
    case 2:                                     \
        *__dest++ __OP(*__src1++);              \
    case 1:                                     \
        *__dest++ __OP(*__src1++);              \
    default:                                    \
        break;                                  \
    }

void simdAdd(int count, float *dest, const float *src1, const float *src2) { perform2(count, dest, src1, src2, +); }
void simdSub(int count, float *dest, const float *src1, const float *src2) { perform2(count, dest, src1, src2, -); }
void simdMult(int count, float *dest, const float *src1, const float *src2) { perform2(count, dest, src1, src2, *); }
void simdDiv(int count, float *dest, const float *src1, const float *src2) { perform2(count, dest, src1, src2, /); }
void simdAdd(int count, float *dest, const float *src1) { perform1(count, dest, src1, +=); }
void simdSub(int count, float *dest, const float *src1) { perform1(count, dest, src1, -=); }
void simdMult(int count, float *dest, const float *src1) { perform1(count, dest, src1, *=); }
void simdDiv(int count, float *dest, const float *src1) { perform1(count, dest, src1, /=); }

#undef perform2
#undef perform1

#endif
