/**
 * Project: openRender
 *
 * File: passes/passUniformLifting.cpp
 *
 * Description:
 *   CUniformLiftingPass implementation.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "passUniformLifting.h"
#include "../rslo.h"  // for SLC_xxx constants

// -------------------------------------------------------------------------
// isUniformSafeOpcode
// -------------------------------------------------------------------------

// static
bool CUniformLiftingPass::isUniformSafeOpcode(const std::string &opcode) {
    // Opcodes that only access their explicit operands (no hidden per-vertex
    // state).  Their result can be UNIFORM if all operands are UNIFORM.
    static const char *uniformSafe[] = {
        // Move / constant load
        "moveff", "movevv", "movemm", "movess",
        "vufloat", "vuvector", "vumatrix", "vustring",
        // Arithmetic — float
        "addf", "subf", "mulf", "divf", "negf",
        "addff", "subff", "mulff", "divff",
        // Arithmetic — vector
        "addvv", "subvv", "mulvv", "divvv",
        "addvf", "subvf", "mulvf", "divvf",
        "negv",
        // Comparisons
        "flt", "fle", "fgt", "fge", "feq", "fne",
        "vlt", "vle", "vgt", "vge", "veq", "vne",
        // Math
        "sin", "cos", "tan", "asin", "acos", "atan", "atan2",
        "exp", "log", "sqrt", "abs", "sign", "floor", "ceil",
        "pow", "mod", "clampf", "clampv", "mixf", "mixv",
        "normalize", "length", "dot", "cross",
        "vfromf", "vfromvff", "vfromfff",
        "xcomp", "ycomp", "zcomp",
        "setxcomp", "setycomp", "setzcomp",
        "matfromf", "matfromv",
        "inversesqrt",
        nullptr
    };
    for (int i = 0; uniformSafe[i] != nullptr; ++i) {
        if (opcode == uniformSafe[i]) return true;
    }
    return false;
}

// -------------------------------------------------------------------------
// liftFn: one forward sweep
// -------------------------------------------------------------------------

// static
bool CUniformLiftingPass::liftFn(IRFunction &fn, IRModule &mod,
                                 std::unordered_set<std::string> &uniformSet) {
    bool changed = false;
    for (IRBlock &blk : fn.blocks) {
        for (const IRInstr &instr : blk.instrs) {
            if (!instr.hasResult()) continue;
            if (!isUniformSafeOpcode(instr.opcode)) continue;

            // Already uniform?
            IRVarInfo *vi = mod.findVar(instr.result);
            if (!vi) continue;
            if (vi->isUniform()) {
                uniformSet.insert(instr.result);
                continue;
            }

            // Check if all named operands are in the uniform set.
            bool allUniform = true;
            for (const IROperand &op : instr.operands) {
                if (op.isLiteral() || op.isQuoted() || op.isLabel()) continue;
                if (uniformSet.find(op.token) == uniformSet.end()) {
                    allUniform = false;
                    break;
                }
            }

            if (allUniform) {
                // Promote: set SLC_UNIFORM, clear SLC_VARYING.
                vi->slcType |=  SLC_UNIFORM;
                vi->slcType &= ~SLC_VARYING;
                uniformSet.insert(instr.result);
                changed = true;
            }
        }
    }
    return changed;
}

// -------------------------------------------------------------------------
// CUniformLiftingPass::run
// -------------------------------------------------------------------------

bool CUniformLiftingPass::run(IRModule &mod) {
    bool anyChanged = false;

    // Seed the uniform set with all known-UNIFORM variables.
    std::unordered_set<std::string> uniformSet;
    for (const IRVarInfo &v : mod.vars) {
        if (v.isUniform())
            uniformSet.insert(v.cName);
    }

    // Fixpoint iteration.
    bool changed;
    do {
        changed = false;
        changed |= liftFn(mod.initFn, mod, uniformSet);
        changed |= liftFn(mod.codeFn, mod, uniformSet);
        anyChanged |= changed;
    } while (changed);

    return anyChanged;
}
