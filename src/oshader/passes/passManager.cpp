/**
 * Project: openRender
 *
 * File: passes/passManager.cpp
 *
 * Description:
 *   CPassManager implementation.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2025, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "passManager.h"
#include <cstdio>

void CPassManager::addPass(std::unique_ptr<CIRPass> pass) {
    passes_.push_back(std::move(pass));
}

void CPassManager::run(IRModule &mod, bool dumpIR) {
    for (auto &pass : passes_) {
        if (dumpIR) {
            fprintf(stderr, "[IR] Running pass: %s\n", pass->name());
        }
        const bool changed = pass->run(mod);
        if (dumpIR && changed) {
            fprintf(stderr, "[IR] Pass %s modified the module.\n", pass->name());
        }
    }
}
