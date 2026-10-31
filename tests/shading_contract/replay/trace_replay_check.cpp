/**
 * Project: openRender
 *
 * File: trace_replay_check.cpp
 *
 * Description:
 *   Spec 026, Phase 1 (T013): the trace replay check. `trace_replay_check <builtin>` re-renders
 *   that builtin's scene on every backend this build has and compares the requests with the
 *   recorded replay (tests/shading_contract/replay/macos.txt, or linux.txt off macOS)
 *   exactly. The message of a failure names the builtin, the backend, the request, the ray
 *   and the first differing field. A missing replay file fails with "no replay file" (spec
 *   028 met this on Linux), and so does an empty or absent section.
 *
 *   Tests ShadingContract_TraceReplay_<builtin> (label shading_contract).
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
    if (argc != 2) {
        fprintf(stderr, "usage: %s <builtin>\n", argv[0]);
        return 2;
    }
    const std::string builtin = argv[1];

    const std::string file = replay::replayFilePath();
    bool haveFile = false;
    const std::vector<std::string> recorded = replay::readLines(file, &haveFile);
    if (!haveFile || recorded.empty()) {
        fprintf(stderr, "FAILED: no replay file: %s\n", file.c_str());
        return 1;
    }

    int failures = 0;
    for (const std::string &backend : replay::backends()) {
        std::vector<std::string> want;
        if (!replay::findSection(recorded, builtin, backend, want) || want.size() < 3 || want.back().find("requests=0 ") != std::string::npos) {
            fprintf(stderr, "FAILED: TraceReplay %s (%s): the replay file holds no (or an empty) section for it: %s\n", builtin.c_str(), backend.c_str(), file.c_str());
            failures++;
            continue;
        }
        const std::string tmp = replay::scratchFile(builtin, backend);
        remove(tmp.c_str());
        const int status = replay::runSectionChild(argv[0], builtin, backend, tmp);
        bool ok = false;
        const std::vector<std::string> got = replay::readLines(tmp, &ok);
        remove(tmp.c_str());
        if (status != 0 || !ok || got.size() < 3) {
            fprintf(stderr, "FAILED: TraceReplay %s (%s): the render exited %d\n", builtin.c_str(), backend.c_str(), status);
            failures++;
            continue;
        }
        const std::string diff = replay::compareSections(builtin, backend, want, got);
        if (!diff.empty()) {
            fprintf(stderr, "FAILED: %s\n", diff.c_str());
            failures++;
        }
        else
            printf("%s (%s): %s\n", builtin.c_str(), backend.c_str(), got.back().c_str());
    }
    fflush(stdout);
    return failures == 0 ? 0 : 1;
}
