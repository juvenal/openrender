/**
 * Project: openRender
 *
 * File: passes/passCallSites.h
 *
 * Description:
 *   CCallSitePass: assigns a call-site ordinal to every stochastic builtin
 *   call (spec 039, contracts/shading-runtime.md C1).
 *
 *   The ordinal (0, 1, 2, ...) is the position of the call among the
 *   stochastic calls of the final IR, the Init section first, then the Code
 *   section, in instruction order. It runs after the optimizing passes, so
 *   the numbering describes the code that is emitted. Both backends carry the
 *   ordinal: the interpreter as a trailing operand of the .rslo instruction,
 *   the JIT as a constant argument of the versioned runtime entry (C3).
 *
 *   The stochastic builtins: random, urandom, trace, transmission,
 *   visibility, gather, occlusion, indirectdiffuse, illuminance, and the
 *   texture, environment and shadow lookups.
 *
 *   The pass never removes or merges an instruction, so it cannot make
 *   random() pure (C2).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef OSHADER_PASSES_PASSCALLSITES_H
#define OSHADER_PASSES_PASSCALLSITES_H

#include "passManager.h"

#include <string>

class CCallSitePass : public CIRPass {
    public:
        bool run(IRModule &mod) override;
        const char *name() const override { return "CallSites"; }

        // TRUE if an instruction with this opcode is a stochastic call that
        // carries a call-site ordinal.
        static bool isStochastic(const std::string &opcode);
};

#endif // OSHADER_PASSES_PASSCALLSITES_H
