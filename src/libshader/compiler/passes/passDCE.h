/**
 * Project: openRender
 *
 * File: passes/passDCE.h
 *
 * Description:
 *   CDCEPass: Dead Code Elimination pass.
 *
 *   Removes instructions whose result variable is never subsequently read.
 *   Only safe for "pure" opcodes (no side effects — no texture lookups,
 *   no illuminance/illuminate/gather loops, no printf/error calls).
 *
 *   Algorithm:
 *   1. Collect all variables that appear as operands in any instruction
 *      (the "live" set).
 *   2. Any instruction whose result is not in the live set and whose opcode
 *      is on the safe-to-remove list is dead and gets removed.
 *   3. Repeat until no changes (eliminates chains of dead code).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef OSHADER_PASSES_PASSDCE_H
#define OSHADER_PASSES_PASSDCE_H

#include "passManager.h"
#include <unordered_set>
#include <string>

class CDCEPass : public CIRPass {
public:
    bool        run(IRModule &mod) override;
    const char *name() const override { return "DCE"; }

private:
    // Returns true if an instruction with this opcode can be safely removed
    // when its result is dead (i.e. has no side effects).
    static bool isSafeToRemove(const std::string &opcode);

    // Run one elimination sweep on fn using the live-use set from the
    // whole module.  Returns true if any instructions were removed.
    static bool eliminateDeadInFn(IRFunction &fn,
                                  const std::unordered_set<std::string> &live);
};

#endif // OSHADER_PASSES_PASSDCE_H
