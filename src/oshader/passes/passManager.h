/**
 * Project: openRender
 *
 * File: passes/passManager.h
 *
 * Description:
 *   CPassManager: manages and runs IR optimization passes.
 *
 *   Each pass implements the CIRPass interface and receives an IRModule
 *   to transform in place.  The pass manager owns the pass objects and
 *   runs them in the order they were added.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef OSHADER_PASSES_PASSMANAGER_H
#define OSHADER_PASSES_PASSMANAGER_H

#include "../ir.h"
#include <vector>
#include <memory>
#include <string>

// -------------------------------------------------------------------------
// CIRPass — base interface for all IR passes.
// -------------------------------------------------------------------------
class CIRPass {
public:
    virtual ~CIRPass() = default;

    // Transform mod in place.  Returns true if the module was modified.
    virtual bool run(IRModule &mod) = 0;

    // Human-readable name for logging and -dump-ir output.
    virtual const char *name() const = 0;
};

// -------------------------------------------------------------------------
// CPassManager — runs a sequence of CIRPass on an IRModule.
// -------------------------------------------------------------------------
class CPassManager {
public:
    CPassManager()  = default;
    ~CPassManager() = default;

    // Add a pass; the manager takes ownership.
    void addPass(std::unique_ptr<CIRPass> pass);

    // Run all passes on mod.
    // If dumpIR is true, print the IR to stderr before/after each pass.
    void run(IRModule &mod, bool dumpIR = false);

private:
    std::vector<std::unique_ptr<CIRPass>> passes_;
};

#endif // OSHADER_PASSES_PASSMANAGER_H
