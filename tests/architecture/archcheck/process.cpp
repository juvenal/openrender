/**
 * Project: openRender
 *
 * File: process.cpp
 *
 * Description:
 *   External tool runner (see process.h).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include "process.h"

#include <array>
#include <cstdio>
#include <sys/wait.h>

namespace archcheck {

    namespace {

        std::string shellQuote(const std::string &s) {
            std::string q = "'";
            for (const char c : s) {
                if (c == '\'')
                    q += "'\\''";
                else
                    q += c;
            }
            return q + "'";
        }

    } // namespace

    ProcessResult runProcess(const std::vector<std::string> &argv) {
        std::string cmd;
        for (const std::string &a : argv) {
            if (!cmd.empty())
                cmd += ' ';
            cmd += shellQuote(a);
        }

        ProcessResult r;
        FILE *pipe = popen(cmd.c_str(), "r");
        if (pipe == nullptr)
            return r;
        std::array<char, 65536> buf;
        std::size_t n;
        while ((n = std::fread(buf.data(), 1, buf.size(), pipe)) > 0)
            r.output.append(buf.data(), n);
        const int status = pclose(pipe);
        r.status = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
        return r;
    }

} // namespace archcheck
