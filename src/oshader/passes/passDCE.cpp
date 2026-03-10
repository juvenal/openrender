/**
 * Project: openRender
 *
 * File: passes/passDCE.cpp
 *
 * Description:
 *   CDCEPass: Dead Code Elimination.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "passDCE.h"
#include <unordered_set>
#include <algorithm>

// -------------------------------------------------------------------------
// Helper: collect all variable names that appear as operands (sources)
// across all instructions in both init and code functions.
// -------------------------------------------------------------------------
static void collectLive(const IRFunction &fn,
                        std::unordered_set<std::string> &live) {
    for (const IRBlock &blk : fn.blocks) {
        for (const IRInstr &instr : blk.instrs) {
            for (const IROperand &op : instr.operands) {
                // Only add names (skip literals, labels, quoted strings).
                if (!op.isLiteral() && !op.isLabel() && !op.isQuoted())
                    live.insert(op.token);
            }
        }
    }
}

// -------------------------------------------------------------------------
// isSafeToRemove: only pure arithmetic / assignment opcodes.
// Side-effecting opcodes (texture, trace, printf, shadow, etc.) are NOT
// safe to remove even if their result variable is dead.
// -------------------------------------------------------------------------

// static
bool CDCEPass::isSafeToRemove(const std::string &opcode) {
    // Opcodes that are pure and have no observable side effects.
    // Keep this conservative — when in doubt, leave the instruction.
    static const char *pureOpcodes[] = {
        // Move / assign
        "movff", "movvv", "movmm", "movss",
        "vufloat", "vuvector", "vumatrix", "vustring",
        // Arithmetic — float
        "addf", "subf", "mulf", "divf", "negf",
        "addff", "subff", "mulff", "divff",
        // Arithmetic — vector
        "addvv", "subvv", "mulvv", "divvv",
        "addvf", "subvf", "mulvf", "divvf",
        "negv",
        // Comparisons (result is float condition)
        "flt", "fle", "fgt", "fge", "feq", "fne",
        "vlt", "vle", "vgt", "vge", "veq", "vne",
        // Math functions (result only depends on operands)
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
    for (int i = 0; pureOpcodes[i] != nullptr; ++i) {
        if (opcode == pureOpcodes[i]) return true;
    }
    return false;
}

// -------------------------------------------------------------------------
// eliminateDeadInFn: one sweep over a function.
// -------------------------------------------------------------------------

// static
bool CDCEPass::eliminateDeadInFn(IRFunction &fn,
                                 const std::unordered_set<std::string> &live) {
    bool changed = false;
    for (IRBlock &blk : fn.blocks) {
        auto it = blk.instrs.begin();
        while (it != blk.instrs.end()) {
            const IRInstr &instr = *it;
            // Only remove instructions that:
            // 1. Have a result variable.
            // 2. That result is not in the live (used) set.
            // 3. The opcode is safe to remove.
            if (instr.hasResult()
                && live.find(instr.result) == live.end()
                && isSafeToRemove(instr.opcode)) {
                it = blk.instrs.erase(it);
                changed = true;
            } else {
                ++it;
            }
        }
    }
    return changed;
}

// -------------------------------------------------------------------------
// CDCEPass::run
// -------------------------------------------------------------------------

bool CDCEPass::run(IRModule &mod) {
    bool anyChanged = false;

    // Iterate to fixpoint: removing one dead instruction may expose another.
    bool changed;
    do {
        changed = false;

        // Collect the live (used) set from both functions combined.
        // Note: we must include the module-level live variables (shader
        // outputs, globals) which are implicitly live.
        std::unordered_set<std::string> live;
        collectLive(mod.initFn, live);
        collectLive(mod.codeFn, live);

        // All shader output variables are live by definition.
        for (const IRVarInfo &v : mod.vars) {
            if (v.isGlobal() || v.isOutput()) {
                live.insert(v.cName);
            }
        }

        changed |= eliminateDeadInFn(mod.initFn, live);
        changed |= eliminateDeadInFn(mod.codeFn, live);
        anyChanged |= changed;
    } while (changed);

    return anyChanged;
}
