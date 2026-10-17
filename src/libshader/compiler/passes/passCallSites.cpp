/**
 * Project: openRender
 *
 * File: passes/passCallSites.cpp
 *
 * Description:
 *   CCallSitePass implementation (spec 039, C1).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "passCallSites.h"

#include <unordered_set>

// The .rslo mnemonics of the stochastic builtins. gather is one call site,
// at its gatherHeader; illuminance is one at the loop's head.
bool CCallSitePass::isStochastic(const std::string &opcode) {
    static const std::unordered_set<std::string> kStochastic = {
        "random",
        "urandom",
        "trace",
        "transmission",
        "visibility",
        "gatherHeader",
        "occlusion",
        "indirectdiffuse",
        "illuminance",
        "texture",
        "environment",
        "shadow",
    };
    return kStochastic.count(opcode) != 0;
}

bool CCallSitePass::run(IRModule &mod) {
    int next = 0;
    bool changed = false;
    for (IRFunction *fn : {&mod.initFn, &mod.codeFn})
        for (IRBlock &block : fn->blocks)
            for (IRInstr &instr : block.instrs)
                if (isStochastic(instr.opcode)) {
                    if (instr.callSite != next) {
                        instr.callSite = next;
                        changed = true;
                    }
                    ++next;
                }
    return changed;
}
