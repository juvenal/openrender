/**
 * Project: openRender
 *
 * File: passes/passCSE.h
 *
 * Description:
 *   CCSEPass: Common Subexpression Elimination + Loop-Invariant Code Motion.
 *
 *   === CSE ===
 *   Within each IRFunction, eliminates instructions that compute a value
 *   already computed by an earlier instruction.  The second (redundant)
 *   instruction is replaced by a move from the first result variable:
 *     moveff / movevv / movemm / movess  depending on the result variable's type.
 *
 *   Expression identity key: opcode + proto + ordered operand tokens.
 *   Only pure opcodes (same whitelist as DCE) are candidates.
 *
 *   === LICM ===
 *   Hoists loop-invariant pure instructions out of "illuminance",
 *   "illuminate", and "solar" loops.  An instruction is loop-invariant when:
 *     a) Its opcode is pure (same whitelist).
 *     b) None of its source operands are defined within the loop body.
 *     c) None of its source operands carry one of the known loop-bound RSL
 *        symbol names (L, Cl, Ol) — these are the variables whose values
 *        change on each light-loop iteration.
 *
 *   Hoisted instructions are inserted immediately before the loop-begin
 *   instruction so they execute once per shading-point instead of once per
 *   light source.
 *
 *   Nested loops are handled by depth tracking.  Gather loops are not
 *   currently transformed (they require different invariance reasoning).
 *
 *   The pass iterates both CSE and LICM to a fixpoint per function.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef OSHADER_PASSES_PASSCSE_H
#define OSHADER_PASSES_PASSCSE_H

#include "passManager.h"
#include <unordered_map>
#include <unordered_set>
#include <string>

class CCSEPass : public CIRPass {
public:
    bool        run(IRModule &mod) override;
    const char *name() const override { return "CSE"; }

private:
    // ---- CSE ----------------------------------------------------------------

    // One CSE sweep over fn.  Returns true if any instruction was replaced.
    static bool cseFn(IRFunction &fn, const IRModule &mod);

    // Build the expression key for an instruction.
    // Returns "" for instructions that must not be CSE-d.
    static std::string exprKey(const IRInstr &instr);

    // Return the appropriate scalar-move opcode for a result variable.
    // Falls back to "moveff" if the type cannot be determined.
    static std::string moveOpcode(const std::string &result,
                                  const IRModule &mod);

    // ---- LICM ---------------------------------------------------------------

    // One LICM sweep over fn.  Returns true if any instruction was hoisted.
    static bool licmFn(IRFunction &fn, const IRModule &mod);

    // Returns true if the opcode qualifies as pure for both CSE and LICM.
    static bool isPure(const std::string &opcode);

    // Returns true if opcode begins an illuminance/illuminate/solar loop.
    static bool isLoopBegin(const std::string &opcode);

    // Returns the matching loop-end opcode for a given loop-begin opcode.
    static std::string loopEndOpcode(const std::string &beginOpcode);

    // Returns true if the variable named cName is one of the light-loop-bound
    // RSL built-ins (L, Cl, Ol) whose value changes on every loop iteration.
    static bool isLoopBoundVar(const std::string &cName,
                               const IRModule &mod);
};

#endif // OSHADER_PASSES_PASSCSE_H
