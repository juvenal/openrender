/**
 * tests/shading_contract/volume_segments_main.cpp
 *
 * The entry point of test_shading_contract_volume_segments (spec 056): the segment step against a level-entering tracer
 * double. argv[1] is "Interp" or "Jit" (the JIT only when the build has the LLVM JIT); each CTest entry runs one in a
 * process of its own (GitHub #2: one RiBegin/RiEnd session per process).
 */
#include <cstdio>
#include <cstring>

#include "cases_056.h"

int main(int argc, char **argv) {
    const char *which = argc > 1 ? argv[1] : "";
    if (strcmp(which, "Interp") == 0)
        return caseVolumeSegments(false);
#ifdef SHADING_CONTRACT_HAVE_JIT
    if (strcmp(which, "Jit") == 0)
        return caseVolumeSegments(true);
#endif
    fprintf(stderr, "usage: %s Interp|Jit\n", argv[0]);
    return 2;
}
