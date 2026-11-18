/**
 * Spec 057 (area-light geometry), Phase 4 (tasks.md T063): main of the area-light contract program.
 *   test_shading_contract_area_light Interp|Jit
 */
#include <cstdio>
#include <cstring>

#include "cases_057.h"

int main(int argc, char **argv) {
    const char *which = argc > 1 ? argv[1] : "";
    if (strcmp(which, "Interp") == 0)
        return caseAreaLight(false);
#ifdef SHADING_CONTRACT_HAVE_JIT
    if (strcmp(which, "Jit") == 0)
        return caseAreaLight(true);
#endif
    fprintf(stderr, "usage: %s Interp|Jit\n", argv[0]);
    return 2;
}
