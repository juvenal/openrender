/**
 * Project: openRender
 *
 * File: passes/passConstFold.h
 *
 * Description:
 *   CConstFoldPass: Constant Folding pass.
 *
 *   Evaluates pure float arithmetic instructions at compile time when all
 *   operand values are known constants.  Replaces the instruction with a
 *   single "vufloat result <value>" load.
 *
 *   Algorithm (single forward pass to fixpoint):
 *   1. Seed the constant map with every "vufloat result literal" instruction
 *      (uniform float load from an inline literal).
 *   2. For each subsequent instruction whose opcode is foldable and whose
 *      every operand token is either a numeric literal or already in the
 *      constant map: evaluate the expression in double precision, verify
 *      the result is finite, replace the instruction in place with
 *      "vufloat result <computed>", and add the result to the constant map.
 *   3. Repeat until no changes (a single forward pass suffices for acyclic
 *      RSL code; the outer fixpoint loop handles the rare case of blocks
 *      ordered non-dominance-first).
 *
 *   Only float scalar opcodes are folded.  Vector/matrix/string folds are
 *   not supported — their operands are rarely all-constant in practice and
 *   the three-float representation used in the .rslo format makes inline
 *   vector constants ambiguous to parse.
 *
 *   Non-finite results (NaN, ±Inf) are never emitted — the instruction is
 *   left as-is so the interpreter can handle the edge case at runtime.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef OSHADER_PASSES_PASSCONSTFOLD_H
#define OSHADER_PASSES_PASSCONSTFOLD_H

#include "passManager.h"
#include <unordered_map>
#include <string>

class CConstFoldPass : public CIRPass {
public:
    bool        run(IRModule &mod) override;
    const char *name() const override { return "ConstFold"; }

private:
    // One forward sweep over fn.
    // constMap: cName → constant double value (mutated in place).
    // Returns true if any instruction was replaced.
    static bool foldFn(IRFunction &fn,
                       std::unordered_map<std::string, double> &constMap);

    // Attempt to resolve an operand token to a double value.
    // Returns true and sets `out` if the token is a literal or is in constMap.
    static bool resolveOperand(const std::string &token,
                               const std::unordered_map<std::string, double> &constMap,
                               double &out);

    // Evaluate a foldable scalar-float opcode given its argument values.
    // Returns true and sets `result` if the opcode is supported and the
    // result is finite.  Returns false otherwise (instruction not folded).
    static bool evalFloatOp(const std::string &opcode,
                            const std::vector<double> &args,
                            double &result);

    // Returns true if this opcode can be constant-folded (scalar float only).
    static bool isFoldable(const std::string &opcode);

    // Format a double as a minimal precise string suitable for the .rslo stream.
    static std::string fmtDouble(double v);
};

#endif // OSHADER_PASSES_PASSCONSTFOLD_H
