/**
 * Project: openRender
 *
 * File: sloLoader.h
 *
 * Description:
 *   The loader of a standalone .slo LLVM bitcode file (spec 026, Q5): moved from the renderer core
 *   into Shading, beside parseShader() (the .rslo loader, rslo.y). It references no renderer. The
 *   declaration, like the definition (sloLoader.cpp), exists only when the JIT is built
 *   (OPENRENDER_HAVE_LLVM).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#ifndef SLOLOADER_H
#define SLOLOADER_H

#include "config.h" // OPENRENDER_HAVE_LLVM

class CShader;

#ifdef OPENRENDER_HAVE_LLVM
// Load a CShader from a .slo file. shaderName is the logical name (what shadername() returns), sloPath the
// file. Returns nullptr when the file carries no embedded metadata (a .slo older than format C3), so the
// caller falls back to the .rslo.
CShader *parseSloShader(const char *shaderName, const char *sloPath);
#endif

#endif
