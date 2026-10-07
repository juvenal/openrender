/**
 * Project: openRender
 *
 * File: process.h
 *
 * Description:
 *   Runs an external tool (nm, ninja) and captures its standard output.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#ifndef ARCHCHECK_PROCESS_H
#define ARCHCHECK_PROCESS_H

#include <string>
#include <vector>

namespace archcheck {

    struct ProcessResult {
            int status = -1;    // exit status; 127 = command not found
            std::string output; // stdout only; stderr goes to archcheck's stderr
    };

    // Runs argv[0] with the given arguments through /bin/sh, every argument
    // single-quoted, so paths with spaces are safe.
    ProcessResult runProcess(const std::vector<std::string> &argv);

} // namespace archcheck

#endif // ARCHCHECK_PROCESS_H
