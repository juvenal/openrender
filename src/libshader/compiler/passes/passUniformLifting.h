/**
 * Project: openRender
 *
 * File: passes/passUniformLifting.h
 *
 * Description:
 *   CUniformLiftingPass: promote inferred-varying temporary variables to
 *   UNIFORM when all their operand sources are UNIFORM.
 *
 *   In the current compiler, temporaries are created VARYING by default.
 *   When a temporary is computed solely from UNIFORM parameters or other
 *   UNIFORM temporaries, it can be promoted to UNIFORM.  This saves the
 *   interpreter from looping over all vertices for that variable.
 *
 *   Algorithm (forward dataflow):
 *   1. Seed: all SLC_UNIFORM parameters and SLC_UNIFORM declared variables
 *      are in the uniform set.
 *   2. For each instruction in program order: if all named source operands
 *      are in the uniform set, and the opcode is "uniform-safe" (no
 *      per-vertex built-ins like P, N, I, Cs, etc.), promote the result
 *      to UNIFORM (set SLC_UNIFORM in vars[result].slcType).
 *   3. Repeat until no further promotions (fixpoint).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef OSHADER_PASSES_PASSUNIFORMLIFTING_H
#define OSHADER_PASSES_PASSUNIFORMLIFTING_H

#include "passManager.h"
#include <unordered_map>
#include <unordered_set>
#include <string>

class CUniformLiftingPass : public CIRPass {
public:
    bool        run(IRModule &mod) override;
    const char *name() const override { return "UniformLifting"; }

private:
    // Returns true if an opcode never touches per-vertex built-in variables
    // (so its result can be UNIFORM if all inputs are UNIFORM).
    static bool isUniformSafeOpcode(const std::string &opcode);

    // One forward sweep over fn using the current uniform set.
    // writeCounts: number of write sites per variable in the whole function.
    // Variables with >1 write site are never promoted.
    // gatherOutputs: variables bound as a "name:X", value output of a
    // gather() statement. gather() writes a new value into these per loop
    // iteration without appearing as an IRInstr result, so writeCounts can't
    // see that write; these are never promoted regardless of write count.
    // Returns true if any variable was promoted.
    static bool liftFn(IRFunction &fn, IRModule &mod,
                       std::unordered_set<std::string> &uniformSet,
                       const std::unordered_map<std::string, int> &writeCounts,
                       const std::unordered_set<std::string> &gatherOutputs);

    // Collects the value-half variable names of every gather() "name:X",
    // value output binding in fn (see gatherHeader's operand layout:
    // [category, P, N, sampleCone, samples, name0, value0, name1, value1, ...]).
    static void collectGatherOutputs(const IRFunction &fn,
                                     std::unordered_set<std::string> &out);
};

#endif // OSHADER_PASSES_PASSUNIFORMLIFTING_H
