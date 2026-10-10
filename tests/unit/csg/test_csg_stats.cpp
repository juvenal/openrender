/**
 * Project: openRender
 *
 * File: test_csg_stats.cpp
 *
 * Description:
 *   Unit test (T020, spec 021) for the CSG statistics counters (SC-006,
 *   FR-003, FR-004): numCsgClassifications, numCsgSeamSplits,
 *   numCsgInstancesResolved and numCsgProceduralExpansions are zeroed at
 *   every RiWorldBegin; the end-of-frame statistics at level >= 2 print them,
 *   zeros included, whenever the frame contains a solid, with the exact
 *   labels the tests and documentation rely on; and one classification
 *   increments numCsgClassifications by exactly 1.
 *
 * Authors:
 *   Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * Copyright (c) 2026, Juvenal A. Silva Jr. <juvenal.silva.jr@gmail.com>
 *
 * License: GNU Lesser General Public License (LGPL) 2.1
 *
 */

#include <string>
#include <unistd.h>

#include "csgTestUtils.h"
#include "common/stats.h"

using namespace csgtest;

static const char *LABELS[4] = {"CSG classifications:", "CSG seam splits:", "CSG instances resolved:",
                                "CSG procedural expansions:"};

// Everything printStats(level) writes to stdout
static std::string printed(int level) {
    fflush(stdout);
    char path[] = "/tmp/csg_stats_XXXXXX";
    int fd = mkstemp(path);
    int saved = dup(1);
    dup2(fd, 1);
    stats.printStats(level);
    fflush(stdout);
    dup2(saved, 1);
    close(saved);
    lseek(fd, 0, SEEK_SET);
    std::string out;
    char buf[4096];
    ssize_t n;
    while ((n = read(fd, buf, sizeof(buf))) > 0)
        out.append(buf, (size_t)n);
    close(fd);
    unlink(path);
    return out;
}

TEST(counters_reset_every_frame) {
    CCsgSession s;
    stats.numCsgClassifications = 5;
    stats.numCsgSeamSplits = 6;
    stats.numCsgInstancesResolved = 7;
    stats.numCsgProceduralExpansions = 8;
    stats.csgFrameHasSolid = TRUE;
    RiWorldEnd();
    RiWorldBegin(); // a new frame
    ASSERT(stats.numCsgClassifications == 0);
    ASSERT(stats.numCsgSeamSplits == 0);
    ASSERT(stats.numCsgInstancesResolved == 0);
    ASSERT(stats.numCsgProceduralExpansions == 0);
    ASSERT(stats.csgFrameHasSolid == FALSE);
}

TEST(printed_with_zeros_when_the_frame_has_a_solid) {
    CCsgSession s;
    stats.numCsgClassifications = 0;
    stats.numCsgSeamSplits = 0;
    stats.numCsgInstancesResolved = 0;
    stats.numCsgProceduralExpansions = 0;
    stats.csgFrameHasSolid = TRUE;
    const std::string out = printed(2);
    for (const char *label : LABELS) {
        const size_t at = out.find(label);
        ASSERT(at != std::string::npos);
        // "<label> 0" (any spacing)
        size_t v = at + strlen(label);
        while (v < out.size() && (out[v] == ' ' || out[v] == '\t'))
            v++;
        ASSERT(v < out.size() && out[v] == '0');
    }
}

TEST(not_printed_without_a_solid_or_below_level_two) {
    CCsgSession s;
    stats.csgFrameHasSolid = FALSE;
    std::string out = printed(2);
    for (const char *label : LABELS)
        ASSERT(out.find(label) == std::string::npos);

    stats.csgFrameHasSolid = TRUE;
    out = printed(1);
    for (const char *label : LABELS)
        ASSERT(out.find(label) == std::string::npos);
}

TEST(one_classification_counts_once) {
    CCsgSession s;
    CCSGResolvedOperand *op = makeOperand(s.ctx(), {sphere(s, 0, 0, 0, 1)});
    const int before = stats.numCsgClassifications;
    const float P[3] = {0, 0, 0};
    ASSERT(classifyOperand(NULL, *op, P) == CSG_IN);
    ASSERT(stats.numCsgClassifications == before + 1);
}

int main() {
    RUN(counters_reset_every_frame);
    RUN(printed_with_zeros_when_the_frame_has_a_solid);
    RUN(not_printed_without_a_solid_or_below_level_two);
    RUN(one_classification_counts_once);
    SUMMARY();
}
