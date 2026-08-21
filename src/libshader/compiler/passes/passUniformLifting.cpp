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
#include <unordered_map>

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
// collectGatherOutputs
// -------------------------------------------------------------------------

// static
void CUniformLiftingPass::collectGatherOutputs(const IRFunction &fn,
                                               std::unordered_set<std::string> &out) {
    for (const IRBlock &blk : fn.blocks) {
        for (const IRInstr &instr : blk.instrs) {
            if (instr.opcode != "gatherHeader") continue;
            // operands = [category, P, N, sampleCone, samples,
            //             name0, value0, name1, value1, ...]
            for (size_t i = 6; i < instr.operands.size(); i += 2) {
                out.insert(instr.operands[i].token);
            }
        }
    }
}

// -------------------------------------------------------------------------
// liftFn: one forward sweep
// -------------------------------------------------------------------------

// static
bool CUniformLiftingPass::liftFn(IRFunction &fn, IRModule &mod,
                                 std::unordered_set<std::string> &uniformSet,
                                 const std::unordered_map<std::string, int> &writeCounts,
                                 const std::unordered_set<std::string> &gatherOutputs) {
    bool changed = false;
    for (IRBlock &blk : fn.blocks) {
        for (const IRInstr &instr : blk.instrs) {
            if (!instr.hasResult()) continue;
            if (!isUniformSafeOpcode(instr.opcode)) continue;

            // Already uniform?
            IRVarInfo *vi = mod.findVar(instr.result);
            if (!vi) continue;
            // Skip globals: their stride is fixed by the renderer (s_rslGlobalStrides).
            // Promoting them would incorrectly add them to uniformSet, causing local
            // variables that read from varying globals (e.g. alpha) to be wrongly promoted.
            if (vi->isGlobal()) continue;
            if (vi->isUniform()) {
                uniformSet.insert(instr.result);
                continue;
            }

            // gather() writes into its output-bound variables per loop
            // iteration without that write appearing as an IRInstr result,
            // so writeCounts below can't see it. Never promote these.
            if (gatherOutputs.find(instr.result) != gatherOutputs.end()) continue;

            // Variables written more than once in the function cannot be safely
            // promoted: a later write might assign a varying value, making the
            // "uniform" tag from an earlier write incorrect.
            {
                auto wc = writeCounts.find(instr.result);
                if (wc != writeCounts.end() && wc->second > 1) continue;
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

    // Count write sites per variable in each function section.
    // A variable written more than once cannot be safely promoted to UNIFORM:
    // different writes may produce values of different "uniformity" (one pass
    // assigns a uniform Ka-vector, the next assigns a varying ambient() result).
    // Single-write temporaries are safe to promote.
    auto countWrites = [](const IRFunction &fn,
                          std::unordered_map<std::string, int> &counts) {
        for (const IRBlock &blk : fn.blocks)
            for (const IRInstr &instr : blk.instrs)
                if (instr.hasResult())
                    counts[instr.result]++;
    };
    std::unordered_map<std::string, int> writeCounts;
    countWrites(mod.initFn, writeCounts);
    countWrites(mod.codeFn, writeCounts);

    // Variables bound as gather() "name:X", value outputs: exempt from
    // promotion regardless of writeCounts (see collectGatherOutputs).
    std::unordered_set<std::string> gatherOutputs;
    collectGatherOutputs(mod.initFn, gatherOutputs);
    collectGatherOutputs(mod.codeFn, gatherOutputs);

    // Seed the uniform set with all known-UNIFORM variables.
    std::unordered_set<std::string> uniformSet;
    for (const IRVarInfo &v : mod.vars) {
        if (v.isUniform())
            uniformSet.insert(v.cName);
    }

    // Fixpoint iteration; skip multi-write variables.
    bool changed;
    do {
        changed = false;
        changed |= liftFn(mod.initFn, mod, uniformSet, writeCounts, gatherOutputs);
        changed |= liftFn(mod.codeFn, mod, uniformSet, writeCounts, gatherOutputs);
        anyChanged |= changed;
    } while (changed);

    return anyChanged;
}
