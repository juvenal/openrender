/**
 * tests/shading_contract/message_side_main.cpp
 *
 * The entry point of test_shading_contract_message_side (spec 056): incident() and opposite() read the side the ray
 * comes from. argv[1] is "Interp" or "Jit" (the JIT only when the build has the LLVM JIT).
 */
#include <cstdio>
#include <cstring>

#include "cases_056.h"

int main(int argc, char **argv) {
    const char *which = argc > 1 ? argv[1] : "";
    if (strcmp(which, "Interp") == 0)
        return caseMessageSide(false);
#ifdef SHADING_CONTRACT_HAVE_JIT
    if (strcmp(which, "Jit") == 0)
        return caseMessageSide(true);
#endif
    fprintf(stderr, "usage: %s Interp|Jit\n", argv[0]);
    return 2;
}
