/**
 * Project: openRender
 *
 * File: rsloEmitter.cpp
 *
 * Description:
 *   CRSLObjectEmitter implementation: converts IRModule to .rslo text.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "rsloEmitter.h"
#include "rslo.h"      // for SLC_xxx constants

#include <cstdio>
#include <cassert>

// Version constants are declared in common/global.h via the build system.
// Emit the version line using values from the module's version string.
// The module stores version as "MAJOR.MINOR.PATCH" (already formatted).

// -------------------------------------------------------------------------
// CRSLObjectEmitter::emit
// -------------------------------------------------------------------------

void CRSLObjectEmitter::emit(const IRModule &mod, FILE *out, const char *shaderName) {
    assert(out != nullptr);

    // Version line.
    fprintf(out, "#!version %s\n", mod.version.c_str());

    // Shader name.
    if (shaderName != nullptr)
        fprintf(out, "#!name \"%s\"\n", shaderName);

    // Shader type.
    fprintf(out, "%s\n", mod.shaderType.c_str());

    // Parameter declarations.
    emitParameters(mod, out);

    // Variable declarations.
    emitVariables(mod, out);

    // Init and Code sections.
    emitFunctions(mod, out);
}

void CRSLObjectEmitter::emitFunctions(const IRModule &mod, FILE *out) {
    assert(out != nullptr);

    // Init section.
    fprintf(out, "#!Init:\n");
    emitFunction(mod.initFn, out);
    fprintf(out, "\treturn            \n");

    // Code section.
    fprintf(out, "#!Code:\n");
    emitFunction(mod.codeFn, out);
    fprintf(out, "\treturn\n");
}

// -------------------------------------------------------------------------
// CRSLObjectEmitter::emitParameters
// -------------------------------------------------------------------------

void CRSLObjectEmitter::emitParameters(const IRModule &mod, FILE *out) {
    fprintf(out, "#!parameters:\n");
    for (const IRVarInfo &v : mod.vars) {
        if (!v.isParameter()) continue;

        if (v.isOutput()) fprintf(out, "output\t");
        emitTypeTokens(v, out);

        fprintf(out, "%s", v.symbolName.c_str());

        if (v.isArray())
            fprintf(out, "[%d]", v.numItems);

        if (!v.defaultValue.empty())
            fprintf(out, "\t=\t%s\n", v.defaultValue.c_str());
        else
            fprintf(out, "\n");
    }
}

// -------------------------------------------------------------------------
// CRSLObjectEmitter::emitVariables
// -------------------------------------------------------------------------

void CRSLObjectEmitter::emitVariables(const IRModule &mod, FILE *out) {
    fprintf(out, "#!variables:\n");
    for (const IRVarInfo &v : mod.vars) {
        // Skip parameters and globals; skip SLC_NONE typed entries.
        if (v.isParameter()) continue;
        if (v.isGlobal())    continue;
        if (v.slcType & SLC_NONE) continue;

        emitTypeTokens(v, out);
        fprintf(out, "%s", v.cName.c_str());

        if (v.isArray())
            fprintf(out, "[%d]\n", v.numItems);
        else
            fprintf(out, "\n");
    }
}

// -------------------------------------------------------------------------
// CRSLObjectEmitter::emitTypeTokens
// -------------------------------------------------------------------------

// static
void CRSLObjectEmitter::emitTypeTokens(const IRVarInfo &v, FILE *out) {
    // Container.
    if (v.isUniform())
        fprintf(out, "uniform\t");
    else
        fprintf(out, "varying\t");

    // Base type (with vector sub-type for parameters).
    if (v.isFloat()) {
        fprintf(out, "float\t");
    } else if (v.isVector()) {
        if (v.isParameter()) {
            if (v.slcType & SLC_VPOINT)       fprintf(out, "point\t");
            else if (v.slcType & SLC_VVECTOR)  fprintf(out, "vector\t");
            else if (v.slcType & SLC_VNORMAL)  fprintf(out, "normal\t");
            else if (v.slcType & SLC_VCOLOR)   fprintf(out, "color\t");
            else                               fprintf(out, "vector\t");
        } else {
            // Variables section always writes "vector" regardless of sub-type.
            fprintf(out, "vector\t");
        }
    } else if (v.isString()) {
        fprintf(out, "string\t");
    } else if (v.isMatrix()) {
        fprintf(out, "matrix\t");
    }
}

// -------------------------------------------------------------------------
// CRSLObjectEmitter::emitFunction
// -------------------------------------------------------------------------

void CRSLObjectEmitter::emitFunction(const IRFunction &fn, FILE *out) {
    for (const IRBlock &blk : fn.blocks) {
        // Emit block label if this is not the anonymous entry block.
        if (!blk.label.empty()) {
            fprintf(out, "%s:\n", blk.label.c_str());
        }
        for (const IRInstr &instr : blk.instrs) {
            emitInstr(instr, out);
        }
    }
}

// -------------------------------------------------------------------------
// CRSLObjectEmitter::emitInstr
// -------------------------------------------------------------------------

void CRSLObjectEmitter::emitInstr(const IRInstr &instr, FILE *out) {
    // Leading tab matches the original getCode() formatting.
    fprintf(out, "\t%s", instr.opcode.c_str());

    // Optional type prototype.
    if (!instr.proto.empty())
        fprintf(out, "\t(\"%s\")", instr.proto.c_str());
    // Align: original code pads with spaces to column ~28 for alignment.
    // We use a simpler tab-based alignment here.

    // Result variable.
    if (!instr.result.empty())
        fprintf(out, "  %s", instr.result.c_str());

    // Source operands.
    for (const IROperand &op : instr.operands)
        fprintf(out, " %s", op.token.c_str());

    fprintf(out, "\n");
}
