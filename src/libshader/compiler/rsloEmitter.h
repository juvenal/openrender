/**
 * Project: openRender
 *
 * File: rsloEmitter.h
 *
 * Description:
 *   CRSLObjectEmitter: converts an IRModule to .sdr/.rslo text output.
 *
 *   The emitter takes the (possibly pass-transformed) IRModule and writes
 *   it to a FILE* in the same format that the existing expression.cpp
 *   getCode() methods produce.  This allows passes to transform the IR
 *   and still produce valid .rslo output.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef OSHADER_RSLOEMITTER_H
#define OSHADER_RSLOEMITTER_H

#include "ir.h"
#include <cstdio>

// -------------------------------------------------------------------------
// CRSLObjectEmitter
//
// Usage:
//   CRSLObjectEmitter emitter;
//   emitter.emit(mod, out);  // writes .sdr text to FILE* out
// -------------------------------------------------------------------------
class CRSLObjectEmitter {
public:
    CRSLObjectEmitter() = default;

    // Emit the full .sdr file including headers and code sections.
    // The version line and shader type line are written first, then
    // #!parameters:, #!variables:, #!Init: and #!Code: sections.
    // shaderName, when non-null, is written as a "#!name" line right after
    // the version line (mirrors CScriptContext::generateCode()).
    void emit(const IRModule &mod, FILE *out, const char *shaderName = nullptr);

    // Emit only the #!Init: and #!Code: function sections to an already-open
    // file.  Use this when the caller has already written the header sections
    // (version, shader type, #!parameters:, #!variables:) to out.
    void emitFunctions(const IRModule &mod, FILE *out);

private:
    void emitParameters(const IRModule &mod, FILE *out);
    void emitVariables(const IRModule &mod, FILE *out);
    void emitFunction(const IRFunction &fn, FILE *out);
    void emitInstr(const IRInstr &instr, FILE *out);

    // Write the type tokens ("uniform float", "varying vector", etc.)
    // for one variable entry in the parameters or variables section.
    static void emitTypeTokens(const IRVarInfo &v, FILE *out);
};

#endif // OSHADER_RSLOEMITTER_H
