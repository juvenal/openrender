/**
 * Project: openRender
 *
 * File: trace_replay_record.cpp
 *
 * Description:
 *   Spec 026, Phase 1 (T012): the trace replay recorder. Renders the eight replay scenes,
 *   once per backend, and writes every trace request Shading makes to `replay-fresh.txt` in
 *   the working directory (the build tree, never the source tree). The maintainer copies one
 *   such file to tests/shading_contract/replay/{macos,linux}.txt once, on the starting tree;
 *   a recorded file is never re-recorded from a changed tree (tasks.md, "Thresholds and
 *   references").
 *
 *   Test ShadingContract_TraceReplayRecord (label shading_contract-record).
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */
#include "trace_replay.h"

int main(int argc, char **argv) {
    if (argc >= 2 && strcmp(argv[1], "section") == 0)
        return replay::sectionMain(argc, argv);

    std::ofstream fresh("replay-fresh.txt");
    if (!fresh.good()) {
        fprintf(stderr, "cannot write replay-fresh.txt\n");
        return 1;
    }
    fresh << "# openRender spec 026 trace replay, format 1. Recorded on the starting tree (d01da47 plus the observer hook).\n";
    fresh << "# Never re-recorded from a changed tree: a difference means a call site's rays changed.\n";
    int failures = 0;
    for (const std::string &builtin : replay::builtins())
        for (const std::string &backend : replay::backends()) {
            const std::string tmp = replay::scratchFile(builtin, backend);
            remove(tmp.c_str());
            const int status = replay::runSectionChild(argv[0], builtin, backend, tmp);
            bool ok = false;
            const std::vector<std::string> section = replay::readLines(tmp, &ok);
            remove(tmp.c_str());
            if (status != 0 || !ok || section.size() < 3) {
                fprintf(stderr, "FAILED: %s (%s): the render exited %d\n", builtin.c_str(), backend.c_str(), status);
                failures++;
                continue;
            }
            for (const std::string &l : section)
                fresh << l << "\n";
            printf("%s\n%s\n", section.front().c_str(), section.back().c_str());
            if (section.back().find("requests=0 ") != std::string::npos) {
                fprintf(stderr, "FAILED: %s (%s): the scene made no request\n", builtin.c_str(), backend.c_str());
                failures++;
            }
        }
    fflush(stdout);
    return failures == 0 ? 0 : 1;
}
