/*
 * Project: Pixie
 *
 * File: align.h
 *
 * Description:
 *   This file defines various macros for memory alignment.
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

#ifndef ALIGN_H
#define ALIGN_H
#include "global.h"

// Include the pointer types if necessary
#ifdef __APPLE_CC__   // macOS
#include "inttypes.h"
#else
#ifndef _WINDOWS      // Not Windoze
#ifdef HAVE_STDINT_H
#include <stdint.h>
#endif
#else                 // Windoze
#ifndef uint32_t
#define uint32_t unsigned int
#endif
#ifndef uint64_t
#define uint64_t unsigned long long
#endif
#endif
#endif

// Check if a pointer is 8 byte aligned
#define isAligned64(__data) (((size_t)(__data) & 0x7) == 0)

// Alignment macro (returns a 64 bit aligned data)
#define align64(__data) (((size_t)(__data) + 7) & (~7))

#endif
