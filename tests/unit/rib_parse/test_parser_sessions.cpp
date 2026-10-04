/**
 * Project: openRender
 *
 * File: test_parser_sessions.cpp
 *
 * Description:
 *   Regression test: every RiBegin/RiEnd session in one process can read RIB.
 *   RiEnd's parserCleanup() used to set the flex lexer's yy_init to 1, which
 *   flex 2.6 reads as "already initialized", so the first parse of the next
 *   session skipped lexer setup and wrote through a NULL buffer (a crash in
 *   riblex). Found by spec 021's procedural-operand unit test, which reads an
 *   archive in each of several sessions.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <unistd.h>

#include "ri.h"

static std::string received; // The structure comments the parser reported

static void onComment(RtToken format, ...) {
    va_list args;
    va_start(args, format);
    char text[256];
    vsnprintf(text, sizeof(text), format, args);
    va_end(args);
    received += text;
}

// One session that reads an archive holding a structure comment and a
// primitive; returns what the parser reported
static std::string session(const std::string &dir, int n) {
    const std::string path = dir + "/session" + std::to_string(n) + ".rib";
    FILE *f = fopen(path.c_str(), "w");
    fprintf(f, "#!marker-%d\nSphere 1 -1 1 360\n", n);
    fclose(f);

    received.clear();
    RiBegin(RI_NULL);
    RiWorldBegin();
    RiReadArchive((RtToken)path.c_str(), onComment, RI_NULL);
    RiWorldEnd();
    RiEnd();
    unlink(path.c_str());
    return received;
}

int main() {
    char tmpl[] = "/tmp/rib_sessions_XXXXXX";
    const std::string dir = mkdtemp(tmpl);
    int failed = 0;
    for (int n = 1; n <= 3; n++) {
        printf("session %d ... ", n);
        fflush(stdout);
        const std::string got = session(dir, n);
        const std::string want = "marker-" + std::to_string(n);
        if (got.find(want) == std::string::npos) {
            printf("FAILED (parser reported \"%s\", expected \"%s\")\n", got.c_str(), want.c_str());
            failed++;
        }
        else
            printf("PASSED\n");
    }
    rmdir(dir.c_str());
    printf("\n%d passed, %d failed\n", 3 - failed, failed);
    return failed == 0 ? 0 : 1;
}
